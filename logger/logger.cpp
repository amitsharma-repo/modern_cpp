
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <cstring>

#include <thread>
#include <atomic>
#include <mutex>

#include <memory>

#include <chrono>

#include <iomanip>
#include <iostream>

#include "logger.h"

char const * const getStringLogLevel(Logger::Level const lvl)
{
	switch(lvl)
	{
		case Logger::INFO:		return "INFO ";
		case Logger::DEBUG:		return "DEBUG";
		case Logger::WARNING:	return "WARN ";
		case Logger::ERROR:		return "ERROR";
		case Logger::FATAL:		return "FATAL";
	}

	return "";
}

void getLocalTimestamp(std::ostringstream &oss)
{
	uint64_t timestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
	std::time_t time = timestamp / 1000000;

	auto gmtime = std::gmtime(&time);

	char buffer[32];
	strftime(buffer, sizeof(buffer), "%Y-%m-%d %T.", gmtime);

	char microseconds[7];
	sprintf(microseconds, "%06lu", timestamp % 1000000);

	oss << buffer << microseconds;
}

std::string const fileTimeStamp()
{
	const auto now = std::chrono::system_clock::now();
	const auto nowAsTimeT = std::chrono::system_clock::to_time_t(now);
	const auto nowMs = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()) % 1000000;

	std::ostringstream oss;
	oss << std::put_time(std::localtime(&nowAsTimeT), "%Y%m%d_%H%M%S");
	return oss.str();
}

class SpinLock
{
public:
	void lock()
	{
		while (locked_.test_and_set(std::memory_order_acquire));
	}

	void unlock()
	{
		locked_.clear(std::memory_order_release);
	}

private:
	std::atomic_flag locked_{ATOMIC_FLAG_INIT};
};

class MemoryMappedFile
{
public:
	MemoryMappedFile(std::string const &fileName, uint64_t const fileSize): fileSize_{fileSize}, currFileSize_{fileSize}
	{
		newFile(fileName);
	}

	MemoryMappedFile(MemoryMappedFile const &) = delete;
	MemoryMappedFile & operator=(MemoryMappedFile const &) = delete;

	~MemoryMappedFile() noexcept
	{
		commit();
	}

	bool write(const char *str, uint64_t const length)
	{
		if ((currAddress_ + length) > endAddress_)
			return false;

		currAddress_ = static_cast<char *>(::mempcpy(currAddress_, str, length));
		writtenBytes_ += length;

		return true;
	}

	void flushToDisk()
	{
		if (startAddress_ != nullptr && startAddress_ != MAP_FAILED)
			::msync(startAddress_, writtenBytes_ , MAP_SYNC);
	}

	bool extendFile()
	{
		flushToDisk();

		void *temp = ::mremap(startAddress_, currFileSize_, currFileSize_ + fileSize_, MREMAP_MAYMOVE);
		if (temp == MAP_FAILED)
			return false;

		currFileSize_ += fileSize_;

		::ftruncate(fileDesc_, currFileSize_);

		startAddress_ = static_cast<char *>(temp);
		endAddress_ = startAddress_ + currFileSize_;
		currAddress_ = startAddress_ + writtenBytes_;

		return true;
	}

	bool newFile(std::string const &fileName)
	{
		if (writtenBytes_ > 0)
			commit();

		fileDesc_ = ::open(fileName.c_str(), O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
		if (fileDesc_ < 0)
			return false;

		if (::ftruncate(fileDesc_, currFileSize_) < 0)
			return false;

		void *temp = ::mmap(nullptr, currFileSize_, PROT_WRITE, MAP_SHARED, fileDesc_, 0);
		if (temp == MAP_FAILED)
			return false;

		startAddress_ = static_cast<char *>(temp);
		endAddress_ = startAddress_ + currFileSize_;
		currAddress_ = startAddress_;

		writtenBytes_ = 0;

		return true;
	}

private:
	void commit()
	{
		if (startAddress_ != nullptr && startAddress_ != MAP_FAILED)
		{
			::msync(startAddress_, currFileSize_ , MAP_SYNC);
			::munmap(startAddress_, currFileSize_);
		}

		if (fileDesc_ > 0)
		{
			::ftruncate(fileDesc_, writtenBytes_); //Shrink the file to exact size
			::close(fileDesc_);
		}
	}

	int32_t fileDesc_{0};

	char *startAddress_{nullptr};
	char *endAddress_{nullptr};
	char *currAddress_{nullptr};

	uint64_t fileSize_{0};
	uint64_t currFileSize_{0};

	uint64_t writtenBytes_{0};
};

namespace 
{

std::string const timeStamp = fileTimeStamp();
uint32_t fileCounter{1};
std::unique_ptr<MemoryMappedFile> filePtr;
SpinLock spinLock;

}

std::string const getLogFileName(std::string const &fileName)
{
	std::string modifiedFileName;

	std::size_t const pos = fileName.find(".");
	if (pos != std::string::npos)
		modifiedFileName = fileName.substr(0, pos) + "_" + timeStamp + fileName.substr(pos);
	else
		modifiedFileName = fileName + "_" + timeStamp + ".log";

	return modifiedFileName;
}

std::string getNextLogFileName(std::string const &fileName)
{
	std::string modifiedFileName;

	std::size_t pos = fileName.find(".");
	if (pos != std::string::npos)
		modifiedFileName = fileName.substr(0, pos) + "_" + timeStamp + "_Part_" + std::to_string(fileCounter) + fileName.substr(pos);
	else
		modifiedFileName = fileName + "_" + timeStamp + "_Part_" + std::to_string(fileCounter) + ".log";

	++fileCounter;

	return modifiedFileName;
}

void Logger::setFile(std::string file, uint64_t const size, Logger::FilePolicy const pol)
{
	fileName_ = std::move(file);
	fileSize_ = size;
	policy_ = pol;

	filePtr.reset(new MemoryMappedFile(getLogFileName(fileName_), fileSize_));
}

void Logger::log(Logger::Level const lvl, std::ostringstream &os)
{
	if (lvl < level_)
		return;

	std::string const msg = os.str();
	os.str("");

	getLocalTimestamp(os);
	os << "|" << getStringLogLevel(lvl) << "|" << getpid() << "|" << std::this_thread::get_id() << "|" << msg << std::endl;

	if (consoleFlag_)
		::write(1, os.str().c_str(), os.str().size());

	if (! filePtr)
		return;

	std::unique_lock<SpinLock> lock{spinLock};

	bool status = filePtr->write(os.str().c_str(), os.str().size());
	if (status)
		return;

	status = (policy_ == Logger::NEW_FILE) ? filePtr->newFile(getNextLogFileName(fileName_)) : filePtr->extendFile();
	if (status)
		filePtr->write(os.str().c_str(), os.str().size());
}



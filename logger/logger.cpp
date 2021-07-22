
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <string>

#include <thread>
#include <atomic>
#include <mutex>

#include <memory>

#include <chrono>

#include <iomanip>
#include <iostream>

#include "logger.h"

using namespace std;

bool renameFile(string const &existingFile)
{
	static uint32_t counter = 0;

	string const newFile = existingFile + "." + to_string(counter++);

	return rename(existingFile.c_str(), newFile.c_str()) == 0;
}

void *memcpy(void *dst, const void *src, size_t n)
{
	void *ret = dst;
	asm volatile("rep movsb" : "+D" (dst) : "c"(n), "S"(src) : "cc", "memory");
	return ret;
}

void getCurrentGMTime(char *buff, uint32_t const size)
{
	uint64_t const timestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
	std::time_t time = timestamp / 1000000;
	uint64_t const microseconds = timestamp % 1000000;

	auto gmtime = std::gmtime(&time);

	char localBuffer[32] = {0};
	char *const ptr = localBuffer;

	size_t len = strftime(ptr, sizeof(localBuffer), "%Y%m%d-%T.", gmtime);
	len += sprintf(ptr + len, "%06lu", microseconds);

	localBuffer[(len < size ? len : size) - 1] = 0;

	memcpy(buff, localBuffer, size);
}

const char * to_string(Logger::Level const level)
{
	switch(level)
	{
		case Logger::Level::TRACE:		return "TRACE";
		case Logger::Level::DEBUG:		return "DEBUG";
		case Logger::Level::INFO:		return "INFO";
		case Logger::Level::WARNING:	return "WARN";
		case Logger::Level::ERROR:		return "ERROR";
		case Logger::Level::FATAL:		return "FATAL";
	}
	return "";
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
		while (locked_.test_and_set(std::memory_order_acquire))
			std::this_thread::yield();
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

		currAddress_ = static_cast<char *>(memcpy(currAddress_, str, length));
		currAddress_ += length;
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

std::thread::id getThreadID()
{
	thread_local static std::thread::id id = std::this_thread::get_id();
	return id;
}

void Logger::log(Level const level, const char * const buff, const char * const fileName, uint32_t const lineNo, const char * const functionName)
{
	if (level < level_)
		return;

	std::unique_lock<SpinLock> lock{spinLock};

	char timeStamp[32];
	getCurrentGMTime(timeStamp, size(timeStamp));

	char formattedLogBuffer[1024] = {0}; //TODO: optimize this size
	char * const ptr = formattedLogBuffer;

	size_t len = sprintf(ptr, "%s|%d|%020llu|[%5s]|%s", timeStamp, getpid(), getThreadID(), to_string(level), buff);

	if (fileName != nullptr)
		len += sprintf(ptr + len, " [%s: %d", fileName, lineNo);

	if (functionName != nullptr)
		len += sprintf(ptr + len, ", %s", functionName);

	if (fileName != nullptr)
		len += sprintf(ptr + len, "]");

	len += sprintf(ptr + len, "\n");

	formattedLogBuffer[len] = 0; //Add this buffer to lockfree queue

	if (consoleFlag_)
		::write(1, formattedLogBuffer, len);

	if (! filePtr)
		return;

	bool status = filePtr->write(formattedLogBuffer, len);
	if (status)
		return;

	status = (policy_ == Logger::NEW_FILE) ? filePtr->newFile(getNextLogFileName(fileName_)) : filePtr->extendFile();
	if (status)
		filePtr->write(formattedLogBuffer, len);
}

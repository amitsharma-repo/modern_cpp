/**
  * C++17 compliant code
  */

#include <sstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <string.h>

#define THROW_EXCEPTION(Class, msg) \
do { \
	std::ostringstream oss; \
	auto filePtr = util::getStartEnd(__FILE__); \
	oss << msg << " [" << util::getFileName(filePtr.first, filePtr.second) << ": " << __LINE__ << ", " << __PRETTY_FUNCTION__ << "]" << std::endl; \
	throw Class(oss.str()); \
} while (false)

std::mutex __displayMutex__;
#define LOG(msg) \
do { \
	char buffer[64]; \
	util::getCurrentLocalTime(buffer, sizeof(buffer)); \
	auto filePtr = util::getStartEnd(__FILE__); \
	std::unique_lock<std::mutex> lock{__displayMutex__}; \
	std::cout << buffer << "|" << util::getThreadID() << "|" << msg << " [" << __PRETTY_FUNCTION__ << ", " << util::getFileName(filePtr.first, filePtr.second) << ": " << __LINE__ << "]" << std::endl; \
} while(false)

namespace util
{

std::thread::id getThreadID()
{
	thread_local static std::thread::id id = std::this_thread::get_id();
	return id;
}

template<uint32_t N>
constexpr std::pair<const char *const, const char *const> getStartEnd(char const (&file)[N])
{
	return {file, file+N};
}

constexpr const char * getFileName(const char *const start, const char *const end)
{
	for (int i = 0; start < end-i; ++i)
	{
		if (end[-i] == '/')
			return end - i + 1;
	}
	return start;
}

void getCurrentLocalTime(char *buff, uint32_t const size)
{
	uint64_t const timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
	std::time_t time = timestamp / 1000000000;
	uint64_t const nanoseconds = timestamp % 1000000000;

	auto gmtime = std::localtime(&time);

	char localBuffer[32] = {0};
	char *const ptr = localBuffer;

	size_t len = strftime(ptr, sizeof(localBuffer), "%Y%m%d-%T.", gmtime);
	len += sprintf(ptr + len, "%09lu", nanoseconds);

	localBuffer[(len < size ? len : size)] = 0;

	memcpy(buff, localBuffer, size);
}

}//end of namespace util

template <typename Ch, typename Tr, typename T, typename U>
std::basic_ostream<Ch, Tr> & operator << (std::basic_ostream<Ch, Tr> &out, std::pair <T, U> const& p)
{
	return out << "(" << p.first << ", " << p.second << ")";
}

template<typename Ch, typename Tr, template<typename, typename...> typename ContainerType, typename Type1, typename... TypeN>
std::basic_ostream<Ch, Tr> & operator << (std::basic_ostream<Ch, Tr> &out, ContainerType<Type1, TypeN...> const &obj)
{
	auto curPos = begin(obj), endPos = end(obj);

	if (curPos == endPos)
		return out;

	out << *curPos;

	for(++curPos; curPos != endPos; ++ curPos)
		out << ", " << *curPos;

	return out;
}

template<typename... TypeN>
std::ostream& operator<<(std::ostream &out, std::tuple<TypeN...> const& theTuple)
{
	std::apply(
		[&out](TypeN const&... tupleArgs)
		{
			out << '[';
			std::size_t n{0};
			((out << tupleArgs << (++n != sizeof...(TypeN) ? ", " : "")), ...);
			out << ']';
		},
		theTuple
	);
	return out;
}

#include <atomic>
#include <memory>
#include <deque>
#include <condition_variable>
#include <functional>

template<typename MessageType>
class MessageProcessor
{
public:
	~MessageProcessor()
	{
		stop();

		if (workerThread_.joinable())
			workerThread_.join();
	}

	/**
	 * Post message to sleepy worker thread to process
	 */
	void postMessage(MessageType const &msg)
	{
		std::unique_lock<std::mutex> lock{m_};
		queue_.push_back(msg);
		cv_.notify_one();
	}

	/**
	 * Register your function to process posted message.
	 * Even if you call this function more than one time,
	 * only first call will be effective.
	 */
	void registerHandler(std::function<void(MessageType)> handler)
	{
		std::call_once(init_, [&](){
			handler_ = std::move(handler);
			workerThread_ = std::thread{&MessageProcessor::process, this};
		});
	}

	/**
	 * Stop the worker thread
	 */
	void stop()
	{
		stop_ = true;
		cv_.notify_one();
	}

private:
	/**
	 * This implments a sleepy thread which wakesup
	 * when there is a data posted by user using postMessage()
	 */
	void process()
	{
		while(!stop_.load(std::memory_order_acquire))
		{
			std::unique_lock<std::mutex> lock{m_};
			cv_.wait(lock, [&]{
				if (stop_.load(std::memory_order_acquire))
					return true;

				return !(queue_.empty());
			});

			if (queue_.empty())
				continue;

			MessageType msg = queue_.front();
			queue_.pop_front();

			try
			{
				handler_(msg);
			}
			catch(std::exception const &ex)
			{
				LOG("Error while processing data: " << msg << ", Exception: " << ex.what());
			}
			catch(...)
			{
				LOG("UNKOWN Error while processing data: " << msg);
			}
		}
	}

	std::thread workerThread_;
	std::atomic<bool> stop_{false};
	std::deque<MessageType> queue_;
	std::mutex m_;
	std::condition_variable cv_;
	std::function<void(MessageType)> handler_{};
	std::once_flag init_;
};

int main()
{
	MessageProcessor<int> processor;
	processor.registerHandler([](int i){
		LOG("Received value: " << i);
	});

	std::thread t([&]() {
		for (int i = 1; i <= 100; ++i)
		{
			LOG("Submitting value: " << i);
			processor.postMessage(i);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	});

	for (int i = 1; i <= 100; ++i)
	{
		LOG("Runing main thread: " << i);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	t.join();

	processor.stop();

	return 0;
}

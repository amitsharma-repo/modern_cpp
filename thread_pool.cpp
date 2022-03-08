#include <cstring>
#include <sstream>
#include <iostream>
#include <stdexcept>

#include <unordered_map>
#include <functional>
#include <deque>
#include <utility>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

using namespace std;

#define THROW_EXCEPTION(Class, msg) \
do { \
	std::ostringstream oss; \
	auto filePtr = util::getStartEnd(__FILE__); \
	oss << msg << " [" << util::getFileName(filePtr.first, filePtr.second) << ": " << __LINE__ << ", " << __PRETTY_FUNCTION__ << "]" << std::endl; \
	throw Class(oss.str()); \
} while (false)

std::mutex displayMutex;
#define LOG(msg) \
do { \
	char buffer[64]; \
	util::getCurrentLocalTime(buffer, sizeof(buffer)); \
	auto filePtr = util::getStartEnd(__FILE__); \
	std::unique_lock<std::mutex> lock{displayMutex}; \
	std::cout << buffer << "|" << util::getThreadID() << "|" << msg << " [" << __PRETTY_FUNCTION__ << ", " << util::getFileName(filePtr.first, filePtr.second) << ": " << __LINE__ << "]" << std::endl; \
} while(false)

#define CONCATE(name, line) name##line
#define SCOPE_EXIT(f) const auto& CONCATE(__scopeExit, __LINE__) = util::createScopeExit(f)

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

constexpr const char * const getFileName(const char *const start, const char *const end)
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

template<typename T>
class ScopeExit
{
public:
	explicit ScopeExit(T&& func): func_(std::forward<T>(func))
	{
	}

	~ScopeExit() noexcept
	{
		try
		{
			func_();
		}
		catch(...)
		{
		}
	}

private:
	T func_;
};

template <typename T>
ScopeExit<T> createScopeExit(T&& func)
{
	return ScopeExit<T>(std::forward<T>(func));
}

}//end of namespace util

class ThreadPool
{
public:
	ThreadPool(uint32_t count=std::thread::hardware_concurrency()): count_{count}, threads_{count}
	{
		LOG("Default thread count: " << count_);
		for(uint32_t i = 0; i < count_; ++i)
			threads_[i] = std::thread(&ThreadPool::run, this);
	}

	~ThreadPool() noexcept
	{
		LOG("Terminating Threadpool. Wating for all tasks to finish");

		shutdown();

		for (uint32_t i = 0; i < count_; ++i)
			if (threads_[i].joinable())
				threads_[i].join();

		LOG("Threadpool terminated");
	}

	void submitTask(std::function<void()> function)
	{
		std::unique_lock<std::mutex> lock{mt_};
		queue_.push_back(std::move(function));
		cv_.notify_one();
	}

private:
	void run()
	{
		while(startStopFlag_.load(std::memory_order_acquire) || !queue_.empty()) //!queue_.empty() => helps to execute all pending tasks
		{
			std::unique_lock<std::mutex> lock{mt_};
			cv_.wait(lock, [&](){
				return (! queue_.empty());
			});

			auto task = std::move(queue_.front());
			queue_.pop_front();

			lock.unlock();

			task();
		}
	}

	void shutdown()
	{
		startStopFlag_.store(false, std::memory_order_release);
		cv_.notify_all();
	}

	uint32_t count_;
	std::deque<std::function<void()>> queue_;
	std::vector<std::thread> threads_;
	volatile std::atomic<bool> startStopFlag_{true};
	std::mutex mt_;
	std::condition_variable cv_;
};

int main()
{
	SCOPE_EXIT([](){
		LOG("Out of scope main()... Terminating main()");
	});

	ThreadPool pool{2};

	pool.submitTask([](){
		for (int32_t i = 0; i < 10; i += 1)
		{
			LOG("Task1 => " << i);
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	});
	pool.submitTask([](){
		for (int32_t i = 0; i < 10; i += 2)
		{
			LOG("Task2 => " << i);
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	});
	pool.submitTask([](){
		for (int32_t i = 0; i < 10; i += 3)
		{
			LOG("Task3 => " << i);
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	});
	return 0;
}

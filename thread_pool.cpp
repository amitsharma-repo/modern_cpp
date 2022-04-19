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

#include <deque>
#include <vector>
#include <condition_variable>
#include <functional>
#include <atomic>


/**
 * Flexible threadpool implementation
 * It can accept tasks and run parallel to existing tasks
 * even if total tasks are greater than threadpool size.
 *
 * Once threadpool is shutdown, submitted tasks will not
 * be scheduled.
 */
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

	/**
	 * function: Task to be schduled
	 * createNewIfReq: Add new thread when total active tasks are greater than threadpool size
	 */
	void submitTask(std::function<void()> function, const bool createNewIfReq=false)
	{
		if (startFlag_.load(std::memory_order_acquire))
		{
			std::unique_lock<std::mutex> lock{mt_};

			queue_.push_back(std::move(function));

			if (createNewIfReq && queue_.size() > count_)
			{
				threads_.emplace_back(&ThreadPool::run, this);
				++count_;
			}

			cv_.notify_one();
		}
	}

	/**
	 * Shutdown threadpool. Task will not be schduled once
	 * this function is called
	 */
	void shutdown()
	{
		startFlag_.store(false, std::memory_order_release);
		cv_.notify_all();
	}

private:
	void run()
	{
		while(startFlag_.load(std::memory_order_acquire) || !queue_.empty()) //!queue_.empty() => helps to execute all pending tasks
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

	uint32_t count_;
	std::deque<std::function<void()>> queue_;
	std::vector<std::thread> threads_;
	volatile std::atomic<bool> startFlag_{true};
	std::mutex mt_;
	std::condition_variable cv_;
};

int main()
{
	SCOPE_EXIT([]{
		LOG("Out of scope main()... Terminating main()");
	});

	ThreadPool pool{2};

	pool.submitTask([](){
		for (int32_t i = 0; i < 10; i += 1)
		{
			LOG("Task1 => " << i);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	});
	pool.submitTask([](){
		for (int32_t i = 0; i < 10; i += 2)
		{
			LOG("Task2 => " << i);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	});

	//This task is scheduled along with Task1 and Task2 even if threadpool is 2
	pool.submitTask([](){
		for (int32_t i = 0; i < 10; i += 3)
		{
			LOG("Task3 => " << i);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}, true);

	//This task is scheduled when Task3 will be completed
	pool.submitTask([](){
		for (int32_t i = 0; i < 10; i += 4)
		{
			LOG("Task4 => " << i);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	});

	pool.shutdown(); //Stop the threadpool. Can't accept task now onwards

	//This task is not going to be scheduled
	pool.submitTask([](){
		for (int32_t i = 0; i < 10; i += 5)
		{
			LOG("Task5 => " << i);
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	});

	return 0;
}

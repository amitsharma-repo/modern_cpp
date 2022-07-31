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
	std::cout << buffer << "|" << util::getThreadID() << "|" << util::getThreadName() << "|" << msg << " [" << __PRETTY_FUNCTION__ << ", " << util::getFileName(filePtr.first, filePtr.second) << ": " << __LINE__ << "]" << std::endl; \
} while(false)

namespace util
{

class ThreadName
{
public:
	ThreadName()
	{
		pthread_getname_np(pthread_self(), buff_, sizeof(buff_));
	}

	std::string const name() const
	{
		return buff_;
	}

private:
	char buff_[64];
};

inline std::thread::id getThreadID()
{
	thread_local static std::thread::id id = std::this_thread::get_id();
	return id;
}

inline std::string getThreadName()
{
	thread_local static ThreadName name;
	return name.name();
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

#include <functional>
#include <chrono>
#include <condition_variable>
#include <atomic>

class TimerThread
{
	using ClockType = std::chrono::high_resolution_clock;

public:
	TimerThread() = default;
	TimerThread(TimerThread const &) = delete;
	TimerThread & operator=(TimerThread const &) = delete;

	~TimerThread() noexcept
	{
		try
		{
			stop();
		}
		catch(std::exception const &ex)
		{
			std::cout << "Exception: " << ex.what() << std::endl;
		}
		catch(...)
		{
		}
	}

	void registerCallback(std::function<void()> func, uint32_t const interval=0)
	{
		std::unique_lock<std::mutex> lock{mt_};
		timers_.emplace_back(std::move(func), ClockType::now(), interval);
		cv_.notify_all();
	}

	void start(uint32_t const delay=0)
	{
		if (! start_)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(delay));
			workerThread_ = std::thread{&TimerThread::run, this};
			start_ = true;
		}
	}

private:
	void run()
	{
		for (auto &t: timers_)
			t.prevFireTime_ = ClockType::now();

		while (startTimerFlag_.load(std::memory_order_acquire))
		{
			std::unique_lock<std::mutex> lock{mt_};
			cv_.wait(lock, [this]() -> bool {
				return ! timers_.empty();
			});

			for (auto &t: timers_)
				if (t.isReady())
					t.func_();
		}
	}

	void stop()
	{
		startTimerFlag_.store(false, std::memory_order_release);
		cv_.notify_all();
		if (workerThread_.joinable())
			workerThread_.join();
	}

	struct TimerInfo
	{
		TimerInfo() = default;

		TimerInfo(std::function<void()> func, ClockType::time_point prevFireTime, uint32_t const interval):
			func_{std::move(func)},
			prevFireTime_{prevFireTime},
			intervalMilliSec_{interval}
		{
		}

		bool isReady()
		{
			if (!isFiredFirstTime)
			{
				isFiredFirstTime = true;
				return true;
			}
			else if (intervalMilliSec_ != 0)
			{
				auto current = ClockType::now();
				uint32_t const duration = std::chrono::duration_cast<std::chrono::milliseconds>(current - prevFireTime_).count();

				if (duration >= intervalMilliSec_)
				{
					prevFireTime_ = current;
					return true;
				}
			}

			return false;
		}

		std::function<void()> func_;
		ClockType::time_point prevFireTime_;
		uint32_t intervalMilliSec_;
		bool isFiredFirstTime{false};
	};

	std::vector<TimerInfo> timers_;
	std::thread workerThread_;
	std::mutex mt_;
	std::condition_variable cv_;
	std::atomic<bool> startTimerFlag_{true};
	bool start_{false};
};

int main()
{
	TimerThread timer;

	timer.registerCallback([](){
		std::cout << "Timer 1 - " << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() << std::endl;
	}, 1000);

	timer.registerCallback([](){
		std::cout << "Timer 2 - " << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() << std::endl;
	}, 2000);

	timer.registerCallback([](){
		std::cout << "Timer 3 - " << std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() << std::endl;
	});

	timer.start();

	std::this_thread::sleep_for(std::chrono::seconds(5));

	LOG("Terminating main()...");

	return 0;
}

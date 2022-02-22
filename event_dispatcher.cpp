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

/**
 * @brief: Add any event here
 */
enum class Event
{
	EVENT1,
	EVENT2,
	EVENT3,
	EVENT4,
	EVENT5
};

/**
 * @brief: All the custom handler data class to recevie data, must inherit this class
 */
class EventData{};
using EventDataPtr = std::shared_ptr<EventData>;

/**
 * @brief: All the custom event hanlder classes must inherit this class
 */
class EventHandler
{
public:
	~EventHandler() noexcept
	{
	}

	virtual void handleEvent(EventDataPtr const &eventDataPtr) = 0;

protected:
	template<typename DestinationType>
	std::shared_ptr<DestinationType> getExactEventData(EventDataPtr const &eventDataPtr)
	{
		std::shared_ptr<DestinationType> ptr = std::static_pointer_cast<DestinationType>(eventDataPtr);
		if (ptr == nullptr)
			THROW_EXCEPTION(std::runtime_error, "Invalid data");
		return ptr;
	}
};
using EventHandlerPtr = std::shared_ptr<EventHandler>;

/**
 * @brief: Actual event dispatcher containing all the complexities. Once this class is instantiated, event dispatcher will start automatically
 */
class EventDispatcher
{
public:
	EventDispatcher(int threadCount=16): threadCount_{threadCount}
	{
	}

	~EventDispatcher() noexcept
	{
		LOG("Terminating EventDispatcher");

		stop();

		if (runnerThread_.joinable())
		runnerThread_.join();

		for(auto &thread: workerThreads_)
			if (thread.joinable())
				thread.join();

		LOG("EventDispatcher terminated");
	}

	EventDispatcher(EventDispatcher const &) = delete;
	EventDispatcher & operator=(EventDispatcher const &) = delete;

	void registerEventHandler(Event event, EventHandlerPtr const &handler)
	{
		queueHandlers_.insert_or_assign(event, handler);
	}

	void dispatchEvent(Event ev, EventDataPtr const &dataPtr);

private:
	void run();
	void stop()
	{
		runnerStartStopFlag_.store(false, std::memory_order_release);
	}

	std::unordered_map<Event, EventHandlerPtr> queueHandlers_; //Stores events and their handlers
	int32_t threadCount_; //This should be used in thread pool if it is being used
	std::deque<std::pair<Event, EventDataPtr>> dispatchedEvents_;
	std::condition_variable cv_;
	std::mutex mt_;
	std::vector<std::thread> workerThreads_;
	std::thread runnerThread_{&EventDispatcher::run, this};
	volatile std::atomic<bool> runnerStartStopFlag_{true};
};

void EventDispatcher::run()
{
	while(runnerStartStopFlag_.load(std::memory_order_acquire))
	{
		std::unique_lock<std::mutex> lock(mt_);
		cv_.wait(lock, [&](){
			return ! dispatchedEvents_.empty(); //Wait if no events are dispatched
		});

		std::pair<Event, EventDataPtr> event = std::move(dispatchedEvents_.front());
		dispatchedEvents_.pop_front();

		lock.unlock();

		auto curItr = queueHandlers_.find(event.first); //TODO: Synchronization required if registerEventHandler() is called after dispatchEvent()
		if (curItr == queueHandlers_.end())
			continue;

		auto &handler = curItr->second;

		std::thread t([&](){
			handler->handleEvent(event.second);
		});

		workerThreads_.push_back(std::move(t));
	}
}

void EventDispatcher::dispatchEvent(Event ev, EventDataPtr const &dataPtr)
{
	std::unique_lock<std::mutex> lock(mt_);
	dispatchedEvents_.push_back(std::make_pair(ev, dataPtr));
	cv_.notify_one();
}

class EventHandler_Event1: public EventHandler
{
public:
	class EventData_Event1: public EventData
	{

	};

	void handleEvent(EventDataPtr const &eventDataPtr) override
	{
		std::shared_ptr<EventHandler_Event1::EventData_Event1> ptr = getExactEventData<EventHandler_Event1::EventData_Event1>(eventDataPtr);

		for (int i = 0; i < 10; i += 1)
		{
			LOG("Processing EVENT1 " << i);
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}
};

class EventHandler_Event2: public EventHandler
{
public:
	class EventData_Event2: public EventData
	{

	};

	void handleEvent(EventDataPtr const &eventDataPtr) override
	{
		std::shared_ptr<EventHandler_Event2::EventData_Event2> ptr = getExactEventData<EventHandler_Event2::EventData_Event2>(eventDataPtr);

		for (int i = 0; i < 10; i += 2)
		{
			LOG("Processing EVENT2 " << i);
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}
};

class EventHandler_Event3: public EventHandler
{
public:
	class EventData_Event3: public EventData
	{

	};

	void handleEvent(EventDataPtr const &eventDataPtr) override
	{
		std::shared_ptr<EventHandler_Event3::EventData_Event3> ptr = getExactEventData<EventHandler_Event3::EventData_Event3>(eventDataPtr);

		for (int i = 0; i < 10; i += 3)
		{
			LOG("Processing EVENT3 " << i);
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}
};

class EventHandler_Event4: public EventHandler
{
public:
	class EventData_Event4: public EventData
	{

	};

	void handleEvent(EventDataPtr const &eventDataPtr) override
	{
		std::shared_ptr<EventHandler_Event4::EventData_Event4> ptr = getExactEventData<EventHandler_Event4::EventData_Event4>(eventDataPtr);

		for (int i = 0; i < 10; i += 4)
		{
			LOG("Processing EVENT4 " << i);
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}
};

int main()
{
	SCOPE_EXIT([](){
		LOG("Terminating main thread...");
	});

	EventDispatcher dispatcher; //event dispatcher started automatically

	dispatcher.registerEventHandler(Event::EVENT1, std::make_shared<EventHandler_Event1>());
	dispatcher.registerEventHandler(Event::EVENT2, std::make_shared<EventHandler_Event2>());
	dispatcher.registerEventHandler(Event::EVENT3, std::make_shared<EventHandler_Event3>());

	dispatcher.dispatchEvent(Event::EVENT1, std::make_shared<EventHandler_Event1::EventData_Event1>());
	dispatcher.dispatchEvent(Event::EVENT2, std::make_shared<EventHandler_Event2::EventData_Event2>());
	dispatcher.dispatchEvent(Event::EVENT3, std::make_shared<EventHandler_Event3::EventData_Event3>());

	std::this_thread::sleep_for(std::chrono::seconds(10));

	dispatcher.dispatchEvent(Event::EVENT3, std::make_shared<EventHandler_Event3::EventData_Event3>());
	dispatcher.dispatchEvent(Event::EVENT4, std::make_shared<EventHandler_Event4::EventData_Event4>()); //This event won't processed since there is no handler registered

	return 0;
}



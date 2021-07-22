#include <thread>
#include <vector>

#include "logger.h"

void func()
{
	for (uint64_t i = 1; i <= 9999999; ++i)
	{
		LOG_TRACE("This is a trace log, i = " << i);
		LOG_DEBUG("This is a debug log, i = " << i);
		LOG_INFO("This is an info log, i = " << i);
		LOG_WARN("This is a warning log, i = " << i);
		LOG_ERROR("This is an error log, i = " << i);
		LOG_FATAL("This is a fatal log, i = " << i);
	}
}

int32_t main()
{
	Logger::instance().setFile("TestFile", 4*GB, Logger::EXTEND_FILE);
	Logger::instance().setConsoleFlag(false);

	constexpr uint32_t threadCount = 4;

	std::vector<std::thread> vec;

	for (uint32_t i = 0; i < threadCount; ++i)
		vec.emplace_back(func);

	for (uint32_t i = 0; i < threadCount; ++i)
		vec[i].join();

	return 0;
}



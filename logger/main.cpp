#include <thread>
#include <vector>

#include "logger.h"

void func()
{
	for (uint64_t i = 1; i <= 9999999; ++i)
	{
		LOG_DBG("This is a debug log, i = " << i);
		LOG_INF("This is an info log, i = " << i);
		LOG_WRN("This is a warning log, i = " << i);
		LOG_ERR("This is an error log, i = " << i);
		LOG_FAT("This is a fatal log, i = " << i);
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



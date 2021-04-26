
#pragma once

#include <string>
#include <iosfwd>
#include <sstream>

#define	LOG(level, msg) \
do { \
	std::ostringstream oss; \
	oss << msg << " [" << __FILE__ << ": " << __LINE__ << " " << __PRETTY_FUNCTION__ << "]"; \
	Logger::instance().log(level, oss); \
} while (false) \

#define	LOG_DBG(msg)	LOG(Logger::DEBUG, msg)

#define	LOG_INF(msg)	LOG(Logger::INFO, msg)

#define	LOG_WRN(msg)	LOG(Logger::WARNING, msg)

#define	LOG_ERR(msg)	LOG(Logger::ERROR, msg)

#define	LOG_FAT(msg)	LOG(Logger::FATAL, msg)

constexpr uint64_t KB = 1024;
constexpr uint64_t MB = 1024 * KB;
constexpr uint64_t GB = 1024 * MB;
constexpr uint64_t TB = 1024 * GB;

class Logger
{
public:
	enum Level
	{
		DEBUG = 1,
		INFO,
		WARNING,
		ERROR,
		FATAL
	};

	enum FilePolicy
	{
		NEW_FILE = 1,
		EXTEND_FILE
	};

	static Logger & instance()
	{
		static Logger object;
		return object;
	}

	Logger(Logger const &) = delete;
	Logger & operator=(Logger const &) = delete;

	void setFile(std::string file, uint64_t const size, Logger::FilePolicy const pol=Logger::NEW_FILE);

	void setConsoleFlag(bool const flag) noexcept
	{
		consoleFlag_ = flag;
	}

	void setLevel(Level lvl) noexcept
	{
		level_ = lvl;
	}

	void log(Logger::Level const level, std::ostringstream &os);

private:
	Logger() = default;

	bool consoleFlag_{true};
	Level level_{DEBUG};
	std::string fileName_;
	uint64_t fileSize_{0};
	FilePolicy policy_{NEW_FILE};
};


template <typename Ch, typename Tr, typename T, typename U>
std::basic_ostream<Ch, Tr> & operator << (std::basic_ostream<Ch, Tr> &out, std::pair <T, U> const& p)
{
	return out << p.first << ", " << p.second;
}

template<typename Ch, typename Tr, template<typename, typename...> typename ContainerType, typename Type1, typename... TypeN>
std::basic_ostream<Ch, Tr> & operator << (std::basic_ostream<Ch, Tr> &out, ContainerType<Type1, TypeN...> const &obj)
{
	if (obj.empty())
		return out;

	auto curPos = begin(obj), endPos = end(obj);

	out << *curPos;

	for(++curPos; curPos != endPos; ++ curPos)
		out << ", " << *curPos;
	
	return out;
}



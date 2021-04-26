//g++ -std=c++17 compile_time_int_to_string.cpp

#include <iostream>

namespace approach1
{

template<uint8_t... digits> 
struct positive_to_chars
{
	static inline constexpr char value[] = {('0' + digits)..., 0};
};

template<uint8_t... digits> 
struct negative_to_chars 
{
	static inline constexpr char value[] = {'-', ('0' + digits)..., 0};
};

template<bool neg, uint8_t... digits>
struct to_chars: positive_to_chars<digits...>
{
};

template<uint8_t... digits>
struct to_chars<true, digits...>: negative_to_chars<digits...>
{
};

template<bool neg, uintmax_t rem, uint8_t... digits>
struct explode : explode<neg, rem / 10, rem % 10, digits...>
{
};

template<bool neg, uint8_t... digits>
struct explode<neg, 0, digits...>: to_chars<neg, digits...>
{
};

template<typename T>
constexpr uintmax_t cabs(T num)
{
	return (num < 0) ? -num : num;
}

template<int num>
struct IntToStr: explode<(num < 0), cabs(num)>
{
};

}

namespace approach2
{

template <int i, bool gTen>
struct UintToStrImpl
{
	UintToStrImpl<i / 10, (i > 99)> c;
	const char c0 = '0' + i % 10;
};

template <int i>
struct UintToStrImpl <i, false>
{
	const char c0 = '0' + i;
};

template <int i, bool sign>
struct IntToStrImpl
{
	UintToStrImpl<i, (i > 9)> num_;
};

template <int i>
struct IntToStrImpl <i, false>
{
	const char sign = '-';
	UintToStrImpl<-i, (-i > 9)> num_;
};

template <int i>
struct IntToStr
{
	IntToStrImpl<i, (i >= 0)> num_;
	const char end = '\0';
	const char* str = (char*)this;
};

}

namespace approach3
{

template<int N>
struct IntToStr
{
	constexpr int length(int val)
	{
		int len = 0;
		for (; val != 0; ++len, val /= 10);
		return len;
	}

	constexpr IntToStr()
	{
		int val = N;
		uint16_t i = length(N);

		if constexpr (N < 0)
		{
			val *= -1;
			str[0] = '-';
		}
		else
			--i;

		str[i] = 0;

		while (val != 0)
		{
			str[i--] = ('0' + (val % 10));
			val /= 10;
		}
	}

	char str[1024] = {};
};

}

int main()
{
	std::cout << approach1::IntToStr<1234>::value << std::endl;
	std::cout << approach1::IntToStr<-1234>::value << std::endl;

	std::cout << approach2::IntToStr<1234>().str << std::endl;
	std::cout << approach2::IntToStr<-1234>().str << std::endl;

	std::cout << approach3::IntToStr<1234>().str << std::endl;
	std::cout << approach3::IntToStr<-1234>().str << std::endl;
	return 0;
}

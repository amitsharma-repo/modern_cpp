#pragma once

#include <type_traits>

/**
 * Find maximum values at compile time
 */
template <size_t arg1, size_t ... others>
struct static_max;

template <size_t arg>
struct static_max<arg>
{
	constexpr static size_t const value = arg;
};

template <size_t arg1, size_t arg2, size_t ... others>
struct static_max<arg1, arg2, others...>
{
	constexpr static size_t const value = ((arg1 >= arg2) ? static_max<arg1, others...>::value : static_max<arg2, others...>::value);
};

/**
 * Return hash code of type
 */
template<typename T=void>
constexpr static size_t value_type_code()
{
	return typeid(T).hash_code();
}

/**
 * Helper class to copy/move/destroy object at certain location
 */
template<typename... Ts>
struct variant_helper;

template<typename F, typename... Ts>
struct variant_helper<F, Ts...>
{
	static void destroy(size_t current_value_type_code, void *memory)
	{
		if (current_value_type_code == value_type_code<>())
			return;

		if (current_value_type_code == value_type_code<F>())
			reinterpret_cast<F*>(memory)->~F();
		else
			variant_helper<Ts...>::destroy(current_value_type_code, memory);
	}

	static void move(size_t old_value_type_code, void *old_memory, void *new_memory)
	{
		if (old_value_type_code == value_type_code<F>())
			new (new_memory) F(std::move(*reinterpret_cast<F*>(old_memory)));
		else
			variant_helper<Ts...>::move(old_value_type_code, old_memory, new_memory);
	}

	static void copy(size_t old_value_type_code, const void *old_memory, void *new_memory)
	{
		if (old_value_type_code == value_type_code<F>())
			new (new_memory) F(*reinterpret_cast<const F*>(old_memory));
		else
			variant_helper<Ts...>::copy(old_value_type_code, old_memory, new_memory);
	}
};

template<>
struct variant_helper<>
{
	static void destroy(size_t value_type_code, void *memory) { throw "invalid hash code"; }
	static void move(size_t old_value_type_code, void *old_memory, void *new_memory) { throw "invalid hash code"; }
	static void copy(size_t old_value_type_code, const void *old_memory, void *new_memory) { throw "invalid hash code"; }
};

/**
 * Matches KeyType with Type1, ...TypeN
 */
template<typename KeyType, typename Type1, typename ... TypeN>
struct match_type
{
	constexpr static bool value = std::is_same<KeyType, Type1>::value ? std::is_same<KeyType, Type1>::value : match_type<KeyType, TypeN...>::value;
};

template<typename KeyType, typename Type>
struct match_type<KeyType, Type>
{
	constexpr static bool value = std::is_same<KeyType, Type>::value;
};

template<typename... Typen>
class variant
{
	using variant_helper_type = variant_helper<Typen...>;

public:
	variant() = default;

	template<typename Type>
	variant(Type &&args)
	{
		static_assert(match_type<Type, Typen...>::value);

		new(&memory_) Type(std::forward<Type>(args));
		current_stored_value_type_code_ = value_type_code<Type>();
	}

	variant(variant const &obj)
	{
		static_assert(sizeof(obj.memory_) == sizeof(memory_), "Different memory size");

		variant_helper_type::copy(current_stored_value_type_code_, &obj.memory_, &memory_);
		current_stored_value_type_code_ = obj.current_stored_value_type_code_;
	}

	variant(variant &&obj)
	{
		static_assert(sizeof(obj.memory_) == sizeof(memory_), "Different memory size");

		variant_helper_type::move(current_stored_value_type_code_, &obj.memory_, &memory_);
		current_stored_value_type_code_ = obj.current_stored_value_type_code_;
	}

	~variant() noexcept
	{
		variant_helper_type::destroy(current_stored_value_type_code_, &memory_);
	}

	variant & operator=(variant const &rhs)
	{
		if (this == &rhs)
			return *this;

		static_assert(sizeof(rhs.memory_) == sizeof(memory_), "Different memory size");

		variant_helper_type::destroy(current_stored_value_type_code_, &memory_);
		variant_helper_type::copy(current_stored_value_type_code_, &rhs.memory_, &memory_);
		current_stored_value_type_code_ = rhs.current_stored_value_type_code_;
	}

	variant & operator=(variant &&rhs) noexcept
	{
		static_assert(sizeof(rhs.memory_) == sizeof(memory_), "Different memory size");
	
		variant_helper_type::destroy(current_stored_value_type_code_, &memory_);
		variant_helper_type::move(current_stored_value_type_code_, &rhs.memory_, &memory_);
		current_stored_value_type_code_ = rhs.current_stored_value_type_code_;
	}

	template<typename Type>
	variant & operator=(Type &&val) noexcept
	{
		static_assert(match_type<Type, Typen...>::value);

		variant_helper_type::destroy(current_stored_value_type_code_, &memory_);
		new(&memory_) Type(std::forward<Type>(val));
		current_stored_value_type_code_ = value_type_code<Type>();
	}

	template<typename Type, typename... Tn>
	void emplace(Tn && ... args)
	{
		static_assert(match_type<Type, Typen...>::value);

		variant_helper_type::destroy(current_stored_value_type_code_, &memory_);
		new(&memory_) Type(std::forward<Tn>(args)...);
		current_stored_value_type_code_ = value_type_code<Type>();
	}

	template<typename T>
	T & as()
	{
		if (value_type_code<T>() == current_stored_value_type_code_)
		{
			T *address = reinterpret_cast<T *>(&memory_);
			return *address;
		}
		throw bad_cast{};
	}

	template<typename T>
	T const & as() const
	{
		if (value_type_code<T>() == current_stored_value_type_code_)
		{
			T const *address = reinterpret_cast<T const *>(&memory_);
			return *address;
		}
		throw bad_cast{};
	}

	template<typename T>
	operator T () const
	{
		return as<T>();
	}

	bool valid() const
	{
		return current_stored_value_type_code_ != value_type_code<>();
	}

	constexpr static size_t const max_size = static_max<sizeof(Typen)...>::value;
	constexpr static size_t const max_alignment = static_max<alignof(Typen)...>::value;

private:
	using data_type = typename std::aligned_storage<max_size, max_alignment>::type;

	data_type memory_{0};
	size_t current_stored_value_type_code_{value_type_code<>()};
};



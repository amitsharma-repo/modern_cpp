/**
 * This is an alternative of std::any. Additional feature with this implementation is to avoid runtime cost.
 * std::any allocates memory at runtime and then stores value which is an expensive. That's why std::any size is 16 bytes only.
 * With this implmentation we can follow any approach: 1) runtime like std::any or 2) compile time memory

 * Example #1: [runtime memory allocation approach]

 * any obj;
 * obj = AnyType{}; //Store 
 * obj.get<AnyType>(); //Fetch

 * obj = string("abc");
 * obj.get<string>(); 
 
 * Example #2: [compile time memory approach]
 
 * any<256> obj; // 256 bytes is a guess, the maximum size of data going to be stored
 * obj = AnyType{}; //Store 
 * obj.get<AnyType>(); //Fetch

 * obj = string("abc");
 * obj.get<string>(); 
 */

#pragma once

#include <memory>
#include <array>
#include <cassert>
#include <type_traits>

template<typename T=void>
constexpr static size_t value_type_code()
{
	return typeid(T).hash_code();
}

template<size_t SIZE>
class any_impl
{
public:
	any_impl() = default;

	~any_impl() noexcept
	{
		reset();
	}

	template<typename Type>
	void store(Type val)
	{
		static_assert(SIZE >= sizeof(Type));

		new(storage_.data())Type(std::move(val));
		current_stored_value_type_code_ = value_type_code<Type>();

		if constexpr (! std::is_trivial<Type>::value)
			destroy_ = [](const void* x) { static_cast<const Type*>(x)->~Type(); };
	}

	template<typename ObjectType, typename...ParamType>
	void emplace(ParamType &&... val)
	{
		static_assert(SIZE >= sizeof(ObjectType));

		new(storage_.data())ObjectType(std::forward<ParamType>(val)...);
		current_stored_value_type_code_ = value_type_code<ObjectType>();

		if constexpr (! std::is_trivial<ObjectType>::value)
			destroy_ = [](const void* x) { static_cast<const ObjectType*>(x)->~ObjectType(); };
	}

	template<typename Type>
	Type & fetch()
	{
		assert(current_stored_value_type_code_ == value_type_code<Type>());

		Type *temp = reinterpret_cast<Type *>(storage_.data());
		return *temp;
	}

	void reset() noexcept
	{
		if (destroy_ != nullptr)
			(*destroy_)(storage_.data());

		current_stored_value_type_code_ = value_type_code();
		destroy_  = nullptr;
	}

	operator bool() const
	{
		return current_stored_value_type_code_ != value_type_code();
	}

	void copy(any_impl const &rhs)
	{
		std::copy(std::begin(rhs.storage_), std::end(rhs.storage_), std::begin(storage_));

		current_stored_value_type_code_ = rhs.current_stored_value_type_code_;
		destroy_ = rhs.destroy_;
	}

	void move(any_impl &rhs) noexcept
	{
		copy(rhs);

		rhs.current_stored_value_type_code_ = value_type_code();
		rhs.destroy_ = nullptr;
	}

	void swap(any_impl &obj) noexcept
	{
		std::swap(current_stored_value_type_code_, obj.current_stored_value_type_code_);
		std::swap(destroy_, obj.destroy_);

		for (uint32_t i = 0; i < SIZE; ++i)
			std::swap(obj.storage_[i], storage_[i]);
	}

private:
	std::array<char, SIZE> storage_{0};
	size_t current_stored_value_type_code_{value_type_code()};
	void(*destroy_)(const void*){nullptr};
};

template<>
class any_impl<0>
{
public:
	any_impl() = default;

	template<typename Type>
	void store(Type val)
	{
		ptr_.reset(new storage_holder<Type>(std::move(val)));
		current_stored_value_type_code_ = value_type_code<Type>();
	}

	template<typename ObjectType, typename...ParamType>
	void emplace(ParamType &&... val)
	{
		ptr_.reset(new storage_holder<ObjectType>(std::forward<ParamType>(val)...));
		current_stored_value_type_code_ = value_type_code<ObjectType>();
	}

	template<typename Type>
	Type & fetch()
	{
		assert(current_stored_value_type_code_ == value_type_code<Type>());
		storage_holder<Type> *temp = static_cast<storage_holder<Type> *>(ptr_.get());
		assert(temp != nullptr);
		return temp->data_;
	}

	void reset() noexcept
	{
		ptr_.reset();
		current_stored_value_type_code_ = value_type_code();
	}

	operator bool() const
	{
		return current_stored_value_type_code_ != value_type_code();
	}
	
	void copy(any_impl const &rhs)
	{
		current_stored_value_type_code_ = rhs.current_stored_value_type_code_;
		ptr_ = rhs.ptr_;
	}

	void move(any_impl &rhs) noexcept
	{
		current_stored_value_type_code_ = rhs.current_stored_value_type_code_;
		rhs.current_stored_value_type_code_ = value_type_code();

		ptr_ = std::move(rhs.ptr_);
	}

	void swap(any_impl &obj) noexcept
	{
		std::swap(current_stored_value_type_code_, obj.current_stored_value_type_code_);
		std::swap(ptr_, obj.ptr_);
	}

private:
	struct storage { virtual ~storage() noexcept{} };

	template<typename Type>
	struct storage_holder: public storage
	{
		storage_holder(Type val): data_{std::move(val)}
		{
		}

		template<typename... ParamType>
		storage_holder(ParamType && ...val): data_{std::forward<ParamType>(val)...}
		{
			
		}

		Type data_;
	};

	std::shared_ptr<storage> ptr_;
	size_t current_stored_value_type_code_{value_type_code()};
};

template<size_t SIZE=0>
class any
{
public:
	any() = default;

	template<typename Type>
	any(Type const &obj)
	{
		impl_.store(obj);
	}

	template<typename Type>
	any(Type &&obj)
	{
		impl_.store(std::move(obj));
	}

	any(any const &rhs)
	{
		impl_.copy(rhs.impl_);
	}

	any(any &&rhs)
	{
		impl_.move(rhs.impl_);
	}

	template<typename Type>
	any & operator=(Type const &obj)
	{
		impl_.reset();
		impl_.store(obj);
		return *this;
	}

/*
 * Enabling this will create many issues, 
 * since universal reference parameter is always the first choice to resolve the parameter.
 */
#ifdef __UNIVERSAL_REF__ 
	template<typename Type>
	any & operator=(Type &&obj)
	{
		impl_.reset();
		impl_.store(std::move(obj));
		return *this;
	}
#endif

	any & operator=(any const &rhs)
	{
		if (this != &rhs)
		{
			impl_.reset();
			impl_.copy(rhs.impl_);
		}
		return *this;
	}

	any & operator=(any &&rhs)
	{
		impl_.reset();
		impl_.move(rhs.impl_);
		return *this;
	}

	bool has_value() const noexcept
	{
		return impl_;
	}

	template<typename Type>
	Type & get()
	{
		return impl_.template fetch<Type>();
	}

	void reset() noexcept
	{
		impl_.reset();
	}

	void swap(any &rhs) noexcept
	{
		impl_.swap(rhs.impl_);
	}

	template<typename ObjectType, typename...ParamType>
	void emplace(ParamType &&... val)
	{
		impl_.reset();
		impl_.template emplace<ObjectType>(std::forward<ParamType>(val)...);
	}

private:
	any_impl<SIZE> impl_;
};


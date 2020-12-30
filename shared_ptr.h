#pragma once

#include <cassert>

/**
 * TODO:
 * 1. Custom deleter feature
 * 2. make_shared<T>() implementation
 * 3. Call of derived class destructor if shared_ptr is base type.
 * 
 * Implementation detail
 * ---------------------
 * 
 * The whole implementation is dependent upon 2 logics: 
 * 	1) releaseCurrent and 
 * 	2) shareOwnership/takeOwnerShip(if dealing with directly underlying object address)
 *
 * If there is any existing shared_ptr object and it requires to update with new underlying object address, it should
 * use both logics.
 * 
 * If new object of shared_ptr is going to be created, just use shareOwnership/takeOwnerShip
 */

struct default_deleter
{
	template<typename Type>
	void free (Type * &ptr)const
	{
		delete ptr;
	}
};

template<typename Type>
class shared_ptr
{
public:
	shared_ptr() = default;

	explicit shared_ptr(nullptr_t) noexcept {}

	explicit shared_ptr(Type *ptr)
	{
		takeOwnerShip(ptr);
	}

	~shared_ptr() noexcept
	{
		releaseCurrent();
	}

	shared_ptr(shared_ptr const &obj) noexcept
	{
		shareOwnership(obj);
	}

	/**
	 * Removing this function definition is not going to cause any issue.
	 * Copy constructor will handle this automatically.
	 * This is added only for optimization purpose.
	 */
	shared_ptr(shared_ptr &&obj) noexcept
	{
		shareOwnership(obj, false); //false shared onwership is only required incase of rvalue object
		invalidate(&obj); //rvalue object must be invalidated
	}

	shared_ptr & operator=(shared_ptr const &rhs) noexcept
	{
		if (this != &rhs)
		{
			releaseCurrent();
			shareOwnership(rhs);
		}
		return *this;
	}

	/**
	 * Removing this function definition is not going to cause any issue.
	 * Copy assignment will handle this automatically.
	 * This is added only for optimization purpose.
	 */
	shared_ptr & operator=(shared_ptr &&rhs) noexcept
	{
		releaseCurrent();
		shareOwnership(rhs, false); //false shared onwership is only required incase of rvalue object
		invalidate(&rhs); //rvalue object must be invalidated
		return *this;
	}

	Type & operator * () const noexcept
	{
		assert(ptr_ != nullptr);
		return *ptr_;
	}

	Type * operator -> () noexcept
	{
		return ptr_;
	}

	Type const * operator-> () const noexcept
	{
		return ptr_;
	}

	operator bool() const noexcept
	{
		return ptr_ != nullptr;
	}

	uint64_t use_count() const noexcept
	{
		return (refCount_ != nullptr) ? *refCount_ : 0;
	}

	Type * get ()
	{
		return ptr_;
	}

	const Type * get() const
	{
		return ptr_;
	}

	void reset(Type *newPtr) noexcept
	{
		releaseCurrent();
		takeOwnerShip(newPtr);
	}

	bool unique() const noexcept
	{
		return use_count() == 1;
	}

private:
	void takeOwnerShip(Type *ptr)
	{
		ptr_ = ptr;
		refCount_ = (ptr_ != nullptr) ? new uint64_t(1) : nullptr;
	}

	void shareOwnership(shared_ptr const &obj, bool shareRefCount = true) noexcept
	{
		ptr_ = obj.ptr_;
		refCount_ = obj.refCount_;
		if (shareRefCount && (refCount_ != nullptr))
			++(*refCount_);
	}

	void releaseCurrent() noexcept
	{
		if (refCount_ == nullptr)
			return;

		--(*refCount_);
		if (*refCount_ <= 0)
		{
			del_.free(ptr_);
			del_.free(refCount_);
			invalidate(this);
		}
	}

	void invalidate(shared_ptr * const ptrObj) noexcept
	{
		assert(ptrObj != nullptr);

		ptrObj->ptr_ = nullptr;
		ptrObj->refCount_ = nullptr;
	}

	Type *ptr_{nullptr};
	uint64_t *refCount_{nullptr};
	default_deleter del_;
};

template<typename Type>
bool operator ==(shared_ptr<Type> const &lhs, shared_ptr<Type> const &rhs)
{
	return lhs.get() == rhs.get();
}

template<typename Type>
bool operator !=(shared_ptr<Type> const &lhs, shared_ptr<Type> const &rhs)
{
	return !(lhs == rhs);
}

template<typename Type>
bool operator ==(shared_ptr<Type> const &lhs, Type const *ptr)
{
	return lhs.get() == ptr;
}

template<typename Type>
bool operator !=(shared_ptr<Type> const &lhs, Type const *ptr)
{
	return !(lhs == ptr);
}

#include <iostream>
#include <functional>

using namespace std;

template<typename Type>
class List;

namespace
{

template<typename Type>
struct Iterator;

template<typename Type>
struct Const_Iterator;

template<typename Type>
struct Node;

template<typename Type>
using NodePtr = Node<Type> *;

template<typename Type>
struct Node
{
	Node(Type &data): data_{move(data)}
	{
	}

	template<typename... Args>
	Node(Args&&... args): data_{args...}
	{
	}

private:
	friend class List<Type>;
	friend struct Iterator<Type>;
	friend struct Const_Iterator<Type>;

	Type data_{};
	NodePtr<Type> prev_{nullptr}, next_{nullptr};
};

template<typename Type>
struct Iterator
{
	using value_type = Type;
	using pointer = Type *;
	using const_pointer = Type const *;
	using reference = Type &;
	using const_referece = Type const &;
	using difference_type = ptrdiff_t;
	using iterator_category = std::bidirectional_iterator_tag;

	Iterator() = default;

	Iterator(NodePtr<Type> current): current_{current}
	{
	}

	Iterator & operator++()
	{
		current_ = current_->next_;
		return *this;
	}

	const Iterator operator++(int)
	{
		Iterator tmp(*this);
		current_ = current_->next_;
		return tmp;
	}

	Iterator & operator--()
	{
		current_ = current_->prev_;
		return *this;
	}

	const Iterator operator--(int)
	{
		Iterator tmp(*this);
		current_ = current_->prev_;
		return tmp;
	}

	NodePtr<Type> operator->() const
	{
		return current_;
	}

	Type & operator*()
	{
		return current_->data_;
	}

	template<typename T>
	friend bool operator==(Iterator<T> const &lhs, Iterator<T> const &rhs);

	template<typename T>
	friend bool operator!=(Iterator<T> const &lhs, Iterator<T> const &rhs);

	template<typename T>
	friend int32_t operator-(Iterator<T> const &lhs, Iterator<T> const &rhs);

	template<typename T>
	friend Iterator<T> & operator+(Iterator<T> &lhs, int32_t n);

	template<typename T>
	friend Iterator<T> & operator-(Iterator<T> &lhs, int32_t n);

	friend class List<Type>;

protected:
	NodePtr<Type> current_{nullptr};
};

template<typename Type>
struct Const_Iterator: public Iterator<Type>
{
	using base = Iterator<Type>;

	using base::current_;
	using typename base::value_type;
	using typename base::pointer;
	using typename base::reference;
	using typename base::const_pointer;
	using typename base::const_referece;
	using typename base::iterator_category;
	using typename base::difference_type;

	Const_Iterator() = default;

	Const_Iterator(NodePtr<Type> current): Iterator<Type>(current)
	{
	}

	Const_Iterator(Iterator<Type> const &obj): Iterator<Type>(obj)
	{
	}

	Const_Iterator & operator=(Iterator<Type> const &obj)
	{
		*(static_cast<Iterator<Type> *>(const_cast<Const_Iterator *>(this))) = obj;
		return *this;
	}

	const Node<const Type> * operator->()const
	{
		return current_;
	}

	const Type & operator*() const
	{
		return current_->data_;
	}

	friend class List<Type>;
};

template<typename Type>
bool operator==(Iterator<Type> const &lhs, Iterator<Type> const &rhs)
{
	return lhs.current_ == rhs.current_;
}

template<typename Type>
bool operator!=(Iterator<Type> const &lhs, Iterator<Type> const &rhs)
{
	return ! operator==(lhs, rhs);
}

template<typename Type>
int32_t operator-(Iterator<Type> const &lhs, Iterator<Type> const &rhs)
{
	return std::distance(lhs, rhs);
}

template<typename Type>
Iterator<Type> & operator+(Iterator<Type> &lhs, int32_t n)
{
	std::advance(lhs, n);
	return lhs;
}

template<typename Type>
Iterator<Type> & operator-(Iterator<Type> &lhs, int32_t n)
{
	std::advance(lhs, -n);
	return lhs;
}

}

template<typename Type>
class List
{
public:
	using iterator = Iterator<Type>;
	using const_iterator = Const_Iterator<Type>;

	List() = default;

	~List()
	{
		__removeAll();
	}

	List(uint32_t size, Type const &val=Type{})
	{
		for (uint32_t i = 0; i < size; ++i)
			push_back(val);
	}

	List(List<Type> const &list)
	{
		for (auto const &ele: list)
			push_back(ele);
	}

	List(List<Type> &&list)
	{
		swap(list);
	}

	List<Type> & operator=(List<Type> const &list)
	{
		if (this != &list)
		{
			List<Type> anotherList;

			for (auto const &ele: list)
				anotherList.push_back(ele);

			swap(anotherList);
		}

		return *this;
	}

	List<Type> & operator=(List<Type> &&list)
	{
		if (this != &list)
			swap(list);

		return *this;
	}

	const_iterator begin() const
	{
		return const_iterator(head_);
	}

	const_iterator end() const
	{
		return const_iterator(nullptr);
	}

	iterator begin()
	{
		return iterator(head_);
	}

	iterator end()
	{
		return iterator(nullptr);
	}

	void insert(iterator const &position, Type const &val, uint32_t const count=1);

	template<typename ... Args>
	void emplace(iterator const &position, Args&&... args);

	template<typename ... Args>
	void emplace_back(Args&&... args);

	template<typename ... Args>
	void emplace_front(Args&&... args);

	void push_back(Type const &data);
	void push_front(Type const &data);

	void pop_back();
	void pop_front();

	uint32_t remove(Type const &data);
	iterator find(Type const &data);
	const_iterator find(Type const &data) const;

	void sort(std::function<bool(Type const &lhs, Type const &rhs)> func) noexcept;
	void reverse() noexcept;

	Type const & front() const noexcept { return head_->data_; }
	Type const & back() const noexcept { return tail_->data_; }

	uint32_t size() const noexcept { return length_; }
	bool empty() const noexcept { return head_ == nullptr && tail_ == nullptr; }

	void clear() noexcept
	{
		__removeAll();
		head_ = tail_ = nullptr;
		length_ = 0;
	}

	void swap(List &list) noexcept
	{
		std::swap(list.head_, head_);
		std::swap(list.tail_, tail_);
		std::swap(list.length_, length_);
	}

private:
	NodePtr<Type> __createNode(Type data)
	{
		++length_;
		return new Node<Type>(data);
	}

	template<typename ... Args>
	NodePtr<Type> __createNode(Args&&... args)
	{
		++length_;
		return new Node<Type>(args...);
	}

	void __release(const NodePtr<Type> node)
	{
		--length_;
		delete node;
	}

	NodePtr<Type> __find(Type const &data) const
	{
		NodePtr<Type> foundNode{nullptr};

		for (NodePtr<Type> headTemp = head_, tailTemp = tail_; headTemp != tailTemp; headTemp = headTemp->next_, tailTemp = tailTemp->prev_)
		{
			if (headTemp->data_ == data)
			{
				foundNode = headTemp;
				break;
			}
			else if (tailTemp->data_ == data)
			{
				foundNode = tailTemp;
				break;
			}

			if (headTemp->next_ == tailTemp)
				break;
		}

		return foundNode;
	}

	void __removeAll()
	{
		for (NodePtr<Type> headTemp = head_; headTemp != nullptr; )
		{
			const NodePtr<Type> node = headTemp;
			headTemp = headTemp->next_;
			__release(node);
		}
	}

	void __remove(const NodePtr<Type> foundNode)
	{
		if (foundNode == head_ && foundNode == tail_)
		{
			head_ = tail_ = nullptr;
		}
		else if (foundNode == head_)
		{
			head_ = head_->next_;
			head_->prev_ = nullptr;
		}
		else if (foundNode == tail_)
		{
			auto parent = tail_->prev_;
			tail_ = parent;
			parent->next_ = nullptr;
		}
		else
		{
			auto parent = foundNode->prev_;
			auto nextDescendent = foundNode->next_;

			parent->next_ = nextDescendent;
			nextDescendent->prev_ = parent;
		}

		__release(foundNode);
	}

	void __addNodeAtFront(const NodePtr<Type> newNode)
	{
		if (empty())
			head_ = tail_ = newNode;
		else
		{
			newNode->next_ = head_;
			head_->prev_ = newNode;
			head_ = newNode;
		}
	}

	void __addNodeAtEnd(const NodePtr<Type> newNode)
	{
		if (empty())
			head_ = tail_ = newNode;
		else
		{
			newNode->prev_ = tail_;
			tail_->next_ = newNode;
			tail_ = newNode;
		}
	}

	void __insert(const NodePtr<Type> position, const NodePtr<Type> newNode)
	{
		if (position == head_)
			__addNodeAtFront(newNode);
		else if (position == tail_)
			__addNodeAtEnd(newNode);
		else
		{
			auto parentPostion = position->prev_;

			parentPostion->next_ = newNode;
			newNode->prev_ = parentPostion;

			newNode->next_ = position;
			position->prev_ = newNode;
		}
	}

	NodePtr<Type> head_{nullptr};
	NodePtr<Type> tail_{nullptr};
	uint32_t length_{0};
};

template<typename Type>
void List<Type>::insert(Iterator<Type> const &position, Type const &val, uint32_t const count)
{
	auto current = position.current_;

	for (uint32_t i = 0; i < count; ++i)
	{
		auto newNode = __createNode(val);
		__insert(current, newNode);
	}
}

template<typename Type>
template<typename ... Args>
void List<Type>::emplace(Iterator<Type> const &position, Args&&... args)
{
	auto newNode = __createNode(args...);
	__insert(position.current_, newNode);
}

template<typename Type>
template<typename ... Args>
void List<Type>::emplace_back(Args&&... args)
{
	auto newNode = __createNode(args...);
	__addNodeAtEnd(newNode);
}

template<typename Type>
template<typename ... Args>
void List<Type>::emplace_front(Args&&... args)
{
	auto newNode = __createNode(args...);
	__addNodeAtFront(newNode);
}

template<typename Type>
void List<Type>::push_back(Type const &data)
{
	auto newNode = __createNode(data);
	__addNodeAtEnd(newNode);
}

template<typename Type>
void List<Type>::push_front(Type const &data)
{
	auto newNode = __createNode(data);
	__addNodeAtFront(newNode);
}

template<typename Type>
void List<Type>::pop_back()
{
	NodePtr<Type> node{nullptr};

	if (head_ == tail_)
	{
		node = head_;
		head_ = tail_ = nullptr;
	}
	else
	{
		node = tail_;
		tail_ = tail_->prev_;
		tail_->next_ = nullptr;
	}

	release(node);
}

template<typename Type>
void List<Type>::pop_front()
{
	NodePtr<Type> node{nullptr};

	if (head_ == tail_)
	{
		node = head_;
		head_ = tail_ = nullptr;
	}
	else
	{
		node = head_;
		head_ = head_->next_;
		head_->prev = nullptr;
	}

	release(node);
}

template<typename Type>
uint32_t List<Type>::remove(Type const &data)
{
	uint32_t count = 0;
	for(auto node = __find(data); node != nullptr; __remove(node), ++count, node = __find(data));
	return count;
}

template<typename Type>
typename List<Type>::iterator List<Type>::find(Type const &data)
{
	auto foundNode = __find(data);
	return iterator(foundNode);
}

template<typename Type>
typename List<Type>::const_iterator List<Type>::find(Type const &data) const
{
	auto foundNode = __find(data);
	return const_iterator(foundNode);
}

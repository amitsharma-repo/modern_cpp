#include <iostream>

template<typename T1, typename T2>
std::ostream & operator<<(std::ostream &out, std::pair<T1, T2> const &obj)
{
	return out << obj.first << ":" << obj.second;
}

template<typename Type>
class LinkedList
{
	struct Node
	{
		Type _data{};
		Node *_next{nullptr};

		Node(Type &data): _data{std::move(data)}, _next{nullptr}
		{
		}
	};

	using NodePtr = Node *;

	NodePtr _head{nullptr}, _tail{nullptr};
	uint32_t _count{0};

	NodePtr _createNode (Type &data)
	{
		NodePtr tmp = new Node(data);
		++_count;
		return tmp;
	}

	void _releaseNode (NodePtr node)
	{
		delete node;
		--_count;
	}

	NodePtr _removeNode (NodePtr current, NodePtr parent)
	{
		if (_head == current && _tail == current)
			_head = _tail = nullptr;
		else if (_head == current)
			_head = _head->_next;
		else if (_tail  == current)
		{
			parent->_next = nullptr;
			_tail = parent;
		}
		else
			parent->_next = current->_next;

		_releaseNode (current);

		return parent == nullptr ? nullptr : parent->_next;
	}

	void _reset() noexcept
	{
		_head = _tail = nullptr;
		_count = 0;
	}

public:
	class Iterator
	{
		NodePtr _node{nullptr};

	public:
		Iterator() = default;

		Iterator(NodePtr node): _node{node}
		{
		}

		void set(NodePtr node)
		{
			_node = node;
		}

		NodePtr get()
		{
			return _node;
		}

		Node const * get() const
		{
			return _node;
		}

		operator bool() const noexcept
		{
			return _node != nullptr;
		}

		Iterator & operator++() noexcept
		{
			_node = _node->_next;
			return *this;
		}

		Iterator operator++(int) noexcept
		{
			Iterator tmp{*this};
			_node = _node->_next;
			return tmp;
		}

		Type & operator* () noexcept
		{
			return _node->_data;
		}

		const Type & operator* () const noexcept
		{
			return _node->_data;
		}

		Type * operator->() noexcept
		{
			return &(_node->_data);
		}

		const Type * operator->() const noexcept
		{
			return &(_node->_data);
		}

		bool operator==(Iterator const &itr) const noexcept
		{
			return _node == itr._node;
		}

		bool operator!=(Iterator const &itr) const noexcept
		{
			return ! (*this == itr);
		}
	};

	LinkedList() = default;

	~LinkedList()
	{
		clear();
	}

	LinkedList(std::initializer_list<Type> const &list)
	{
		for (auto const &ele: list)
			push_back(ele);
	}

	LinkedList(LinkedList &list)
	{
		for (NodePtr tmp = list._head; tmp != nullptr; tmp = tmp->_next)
			push_back(tmp->_data);
	}

	LinkedList(LinkedList &&list)
	{
		swap(list);
		list.reset();
	}

	LinkedList & operator=(LinkedList const &list)
	{
		if (this != &list)
		{
			LinkedList newList;
			for (NodePtr tmp = list._head; tmp != nullptr; tmp = tmp->_next)
				newList.push_back(tmp->_data);

			swap(newList);
			newList.clear();
		}

		return *this;
	}

	LinkedList & operator=(LinkedList &&list) noexcept
	{
		swap(list);
		list.clear();
		return *this;
	}

	LinkedList & operator=(std::initializer_list<Type> const &list)
	{
		LinkedList newList;

		for (auto const &ele: list)
			newList.push_back(ele);

		swap(newList);
		newList.clear();

		return *this;
	}

	void push_back(Type data)
	{
		NodePtr newNode = _createNode (data);
		if (empty())
			_head = _tail = newNode;
		else
		{
			_tail->_next = newNode;
			_tail = newNode;
		}
	}

	void push_front(Type data)
	{
		NodePtr newNode = _createNode (data);
		if (empty())
			_head = _tail = newNode;
		else
		{
			newNode->_next = _head;
			_head = newNode;
		}
	}

	template<typename Comp=std::equal_to<Type>>
	Iterator find(Type const &data, Comp cmp={}) const
	{
		for (NodePtr tmp = _head; tmp != nullptr; tmp = tmp->_next)
			if (cmp(tmp->_data, data))
				return Iterator(tmp);

		return Iterator();
	}

	template<typename Comp=std::equal_to<Type>>
	Iterator find(Type const &data, Iterator &parent, Comp cmp={}) const
	{
		for (NodePtr tmp = _head; tmp != nullptr; parent.set(tmp), tmp = tmp->_next)
			if (cmp(tmp->_data, data))
				return Iterator(tmp);

		parent.set(nullptr);
		return Iterator();
	}

	template<typename Comp=std::equal_to<Type>>
	bool remove(Type const &data, Comp cmp={})
	{
		NodePtr parent, current;
		for (current = _head; current != nullptr; parent = current, current = current->_next)
		{
			if (cmp(current->_data, data))
			{
				_removeNode (current, parent);
				return true;
			}
		}
		return false;
	}

	Iterator remove(Iterator currentItr, Iterator parentItr)
	{
		return Iterator(_removeNode(currentItr.get(), parentItr.get()));
	}

	bool empty() const noexcept
	{
		return (_head == nullptr) && (_tail == nullptr);
	}

	uint32_t size() const noexcept
	{
		return _count;
	}

	void clear()
	{
		while(_head != nullptr)
		{
			NodePtr tmp = _head;
			_head = _head->_next;
			_releaseNode (tmp);
		}

		_reset();
	}

	Iterator begin() const noexcept
	{
		return Iterator(_head);
	}

	Iterator end() const noexcept
	{
		return Iterator();
	}

	void swap(LinkedList &list) noexcept
	{
		std::swap(_head, list._head);
		std::swap(_tail, list._tail);
		std::swap(_count, list._count);
	}

	std::ostream & printDebug(std::ostream &out) const
	{
		out << "[" << size() << " - " << std::boolalpha << empty() << "] => ";

		for (NodePtr start = _head; start != nullptr; start = start->_next)
			out << start->_data << ", ";

		return out;
	}
};

template<typename Type>
std::ostream & operator<<(std::ostream &out, LinkedList<Type> const &list)
{
	if (list.empty())
		return out;

	auto start = list.begin(), end = list.end();

	out << *start;

	for (++start; start != end; ++start)
		out << ", " << *start;

	return out;
}

template<typename KeyType, typename MappedType, typename Hasher=std::hash<KeyType>, typename EqualTo=std::equal_to<KeyType>>
class UnorderedMap
{
public:
	using ValueType = std::pair<const KeyType, MappedType>;

	class UMIterator
	{
	public:
		UMIterator() = default;

		UMIterator & set(LinkedList<ValueType> *bucket, uint32_t bucketSize) noexcept
		{
			_bucket = bucket;
			_bucketSize = bucketSize;

			return *this;
		}

		UMIterator & set(typename LinkedList<ValueType>::Iterator itr) noexcept
		{
			_itr = itr;
			return *this;
		}

		UMIterator & set(uint32_t const index) noexcept
		{
			_currentIndex = index;
			return *this;
		}

		UMIterator operator++() noexcept
		{
			increment();
			return *this;
		}

		UMIterator operator++(int) noexcept
		{
			UMIterator itr{*this};
			increment();
			return itr;
		}

		ValueType & operator*() noexcept
		{
			return *_itr;
		}

		ValueType * operator->() noexcept
		{
			return _itr.operator->();
		}

		const ValueType & operator*() const noexcept
		{
			return *_itr;
		}

		const ValueType * operator->() const noexcept
		{
			return _itr.operator->();
		}

		bool operator==(UMIterator const &itr) const noexcept
		{
			return (_bucket == itr._bucket) && (_bucketSize == itr._bucketSize) && (_currentIndex == itr._currentIndex) && (_itr == itr._itr);
		}

		bool operator!=(UMIterator const &itr) const noexcept
		{
			return ! (*this == itr);
		}

		std::ostream & printDebug(std::ostream &out)
		{
			out << "Bucketsize: " << _bucketSize << ", Currentindex: " << _currentIndex << " => ";
			out << _bucket[_currentIndex] << std::endl;
			return out;
		}

	private:
		void increment()
		{
			if ((_itr == _bucket[_currentIndex].end()) || (++_itr == _bucket[_currentIndex].end()))
			{
				for (uint32_t index = _currentIndex + 1; index < _bucketSize; ++index)
				{
					if (! _bucket[index].empty())
					{
						_itr = _bucket[index].begin();
						_currentIndex = index;
						break;
					}
				}
			}
		}

		LinkedList<ValueType> *_bucket{nullptr};
		uint32_t _bucketSize{0};
		uint32_t _currentIndex{0};
		typename LinkedList<ValueType>::Iterator _itr;

		template<typename K, typename M, typename H, typename E>
		friend class UnorderedMap;
	};

	using Iterator = UMIterator;
	using Const_Iterator = const UMIterator;

	UnorderedMap()
	{
		reserve(16);
	}

	~UnorderedMap()
	{
		delete [] _bucket;
	}

	UnorderedMap(std::initializer_list<std::pair<KeyType, MappedType>> const &list)
	{
		reserve(list.size());

		for (auto const &ele: list)
			insert(ele.first, ele.second);
	}

	UMIterator begin() const
	{
		uint32_t index = firstNonEmptyBucketIndex();

		UMIterator itr;
		itr.set(_bucket, _bucketSize).set(index).set(_bucket[index].begin());

		return itr;
	}

	UMIterator end() const
	{
		uint32_t index = lastNonEmptyBucketIndex();

		UMIterator itr;
		itr.set(_bucket, _bucketSize).set(index).set(_bucket[index].end());

		return itr;
	}

	uint32_t size() const noexcept
	{
		return _count;
	}

	uint32_t empty() const noexcept
	{
		return _count == 0;
	}

	void reserve(uint32_t const size)
	{
		_bucketSize = size;
		_bucket = new LinkedList<ValueType>[_bucketSize];
	}
	void setBucketSizeMultiplier(uint8_t factor) noexcept
	{
		if (factor > 1)
			_bucketSizeMultiplierFactor = factor;
	}

	void setLoadFactor(uint32_t factor) noexcept
	{
		if (factor > 1)
			_maxLoadFactor = factor;
	}

	void rehash(uint32_t newBucketSize)
	{
		if ((_count/_bucketSize) < _maxLoadFactor)
			return;

		std::cout << "Rehasing needed from " << _bucketSize << " to " << newBucketSize << std::endl;

		LinkedList<ValueType> *newBucket = new LinkedList<ValueType>[newBucketSize];

		for (uint32_t i = 0; i < _bucketSize; ++i)
		{
			for (auto start = _bucket[i].begin(), end = _bucket[i].end(); start != end; ++start)
			{
				typename LinkedList<ValueType>::Iterator itr;
				insertOrUpdate(newBucket, newBucketSize, start->first, start->second, itr);
			}
		}

		std::swap(_bucket, newBucket);
		std::swap(_bucketSize, newBucketSize);

		delete []newBucket;
	}

	std::pair<bool, UMIterator> insert(KeyType const &key, MappedType const &mappedValue)
	{
		rehash(_bucketSize * _bucketSizeMultiplierFactor);

		typename LinkedList<ValueType>::Iterator tempItr;

		std::pair<bool, uint32_t> status = insertOrUpdate(_bucket, _bucketSize, key, mappedValue, tempItr);

		if (status.first)
			++_count;

		UMIterator itr;
		itr.set(_bucket, _bucketSize).set(status.second).set(tempItr);

		return {status.first, itr};
	}

	MappedType & operator[](KeyType const &key)
	{
		rehash(_bucketSize * _bucketSizeMultiplierFactor);

		typename LinkedList<ValueType>::Iterator tempItr;

		std::pair<bool, uint32_t> status = insertOrUpdate(_bucket, _bucketSize, key, MappedType{}, tempItr);

		if (status.first)
			++_count;

		return tempItr->second;
	}

	UMIterator find(KeyType const &key)
	{
		typename LinkedList<ValueType>::Iterator tempItr;
		std::pair<bool, uint32_t> status = find(key, tempItr);

		if (! status.first)
			return end();

		UMIterator itr;
		itr.set(_bucket, _bucketSize).set(status.second).set(tempItr);

		return itr;
	}

	bool erase(KeyType const &key)
	{
		uint32_t index = getBucketIndex(key);
		return _bucket[index].remove(ValueType(key, MappedType()), [](ValueType const &lhs, ValueType const &rhs){
			return lhs.first == rhs.first;
		});
	}

	UMIterator erase(UMIterator const &keyItr)
	{
		typename LinkedList<ValueType>::Iterator parentItr;

		auto start = _bucket[keyItr._currentIndex].begin(), end = _bucket[keyItr._currentIndex].end();
		for (; start != end; parentItr = start, ++start)
			if (start == keyItr._itr)
				break;

		typename LinkedList<ValueType>::Iterator nextItr = _bucket[keyItr._currentIndex].remove(keyItr._itr, parentItr);

		UMIterator itr;
		itr.set(_bucket, _bucketSize).set(keyItr._currentIndex).set(nextItr);

		if (nextItr == _bucket[keyItr._currentIndex].end())
			++itr;

		return itr;
	}

	std::ostream & printDebug(std::ostream &out) const
	{
		out << "Bucketsize: " << _bucketSize << ", Count: " << _count << ", Maxloadfactor: " << _maxLoadFactor << " => \n";
		for (uint32_t i = 0; i < _bucketSize; ++i)
			out << _bucket[i] << std::endl;
		return out;
	}

private:
	std::pair<bool, uint32_t> find(KeyType const &key, typename LinkedList<ValueType>::Iterator &itr) const
	{
		bool findStatus{true};

		uint32_t index = getBucketIndex(key);

		itr = _bucket[index].find(ValueType(key, MappedType()), [](ValueType const &lhs, ValueType const &rhs){
			return lhs.first == rhs.first;
		});

		if (itr == _bucket[index].end())
			findStatus = false;

		return {findStatus, index};
	}

	std::pair<bool, uint32_t> insertOrUpdate(
		LinkedList<ValueType> *bucket,
		uint32_t const bucketSize,
		KeyType const &key,
		MappedType const &mappedValue,
		typename LinkedList<ValueType>::Iterator &itr
	) const
	{
		bool insertStatus{false};
		uint32_t index = getBucketIndex(key, bucketSize);

		itr = bucket[index].find(ValueType(key, MappedType()), [](ValueType const &lhs, ValueType const &rhs){
			return lhs.first == rhs.first;
		});

		if (itr == bucket[index].end())
		{
			bucket[index].push_back(ValueType(key, mappedValue));
			itr = bucket[index].begin();
			insertStatus = true;
		}
		else
		{
			itr->second = mappedValue;
		}

		return {insertStatus, index};
	}

	uint32_t firstNonEmptyBucketIndex() const noexcept
	{
		for (uint32_t i = 0; i < _bucketSize; ++i)
			if (! _bucket[i].empty())
				return i;

		return 0;
	}

	uint32_t lastNonEmptyBucketIndex() const noexcept
	{
		for (uint32_t i = _bucketSize - 1; i > 0; --i)
			if (! _bucket[i].empty())
				return i;

		return (! _bucket[0].empty()) ? 0 : (_bucketSize - 1);
	}

	uint32_t getBucketIndex(KeyType const &key, uint32_t bucketSize=0) const noexcept
	{
		uint32_t const size = (bucketSize == 0 ? _bucketSize : bucketSize);
		return _hash(key) % size;
	}

	LinkedList<ValueType> *_bucket{nullptr};
	uint32_t _bucketSize{0};
	Hasher _hash{};
	uint32_t _maxLoadFactor{1};
	uint32_t _count{0};
	uint8_t _bucketSizeMultiplierFactor{2};
};

template<typename T1, typename T2>
std::ostream & operator<<(std::ostream &out, UnorderedMap<T1, T2> const &map)
{
	if (map.empty())
		return out;

	auto start = map.begin(), end = map.end();

	out << *start;

	for (++start; start != end; ++start)
		out << ", " << *start;

	return out;
}

int main()
{
	UnorderedMap<int, std::string> map{
		{1, "A"},
		{2, "B"},
		{3, "C"},
		{4, "D"},
		{5, "E"},
		{1, "A1"},
		{3, "C1"},
	};

	map[5] = "E1";
	map[6] = "F";

	auto itr = map.find(6);
	if (itr == map.end())
		std::cout << "Not found" << std::endl;
	else
		itr->second = "F1";

	std::cout << map << std::endl;

	bool status = map.erase(6);

	std::cout << std::boolalpha << status << ", " << map << std::endl;

	status = map.erase(7);

	std::cout << std::boolalpha << status << ", " << map << std::endl;

	itr = map.erase(map.find(3));

	std::cout << std::boolalpha << status << ", " << map << std::endl;

	for (; itr != map.end(); ++itr)
		std::cout << *itr << ", ";

	return 0;
}


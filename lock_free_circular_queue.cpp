#include <atomic>
#include <array>

using namespace std;

template<typename ElementType, size_t SIZE>
class CircularQueue
{
public:
	CircularQueue(): queue_tail{0}, queue_head{0}
	{
	}

	bool push(const ElementType& item);

	bool pop(ElementType& item);

	bool is_empty() const
	{
		return (queue_head.load(std::memory_order_acquire) == queue_tail.load(std::memory_order_acquire));
	}

	bool is_full() const
	{
		const auto next_tail = increment(queue_tail.load(std::memory_order_acquire));
		return (next_tail == queue_head.load(std::memory_order_acquire));
	}

private:
	size_t increment(size_t index) const
	{
		return (index + 1) % QUEUE_CAPACITY;
	}

	inline static constexpr size_t QUEUE_CAPACITY = SIZE + 1;

	std::atomic<size_t> queue_head, queue_tail;
	array<ElementType, QUEUE_CAPACITY> queue;
};

template<typename ElementType, size_t SIZE>
bool CircularQueue<ElementType, SIZE>::push(const ElementType& item)
{
	const auto current_tail = queue_tail.load(std::memory_order_relaxed);
	const auto next_tail = increment(current_tail);

	if (next_tail != queue_head.load(std::memory_order_acquire))
	{
		queue[current_tail] = item;
		queue_tail.store(next_tail, std::memory_order_release);
		return true;
	}

	return false;
}

template<typename ElementType, size_t SIZE>
bool CircularQueue<ElementType, SIZE>::pop(ElementType& item)
{
	const auto current_head = queue_head.load(std::memory_order_relaxed);

	if (current_head == queue_tail.load(std::memory_order_acquire))
		return false;

	item = queue[current_head];

	queue_head.store(increment(current_head), std::memory_order_release);

	return true;
}

#include <thread>
#include <mutex>
#include <string>
#include <iostream>

using namespace std;

std::mutex m;

template<typename T>
void display(std::string const &msg, T const &val)
{
	std::unique_lock<std::mutex> lock{m};
	cout << msg << " " << std::this_thread::get_id() << ", value = " << val << endl;
}

void push(CircularQueue<int, 1024*1024> &queue)
{
	for (int i = 1; i <= 1024*1024; ++i)
	{
		queue.push(i);
		display("Pushed", i);
	}
}

void pop(CircularQueue<int, 1024*1024> &queue)
{
	for (int i = 1; i <= 1024*1024; ++i)
	{
		if (queue.is_empty())
			--i;
		else
		{
			int data{0};
			queue.pop(data);
			display("Poped", data);
		}
	}
}

int main()
{
	CircularQueue<int, 1024*1024> queue;

	std::thread t1{push, std::ref(queue)};
	std::thread t2{pop, std::ref(queue)};

	t1.join();
	t2.join();

	return 0;
}


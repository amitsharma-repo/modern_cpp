#include <iostream>
#include <thread>
#include <atomic>

using namespace std;

template<typename T, size_t QUEUE_SIZE>
class MPMC_Circular_Queue
{
public:
	MPMC_Circular_Queue(MPMC_Circular_Queue const&) = delete;
	MPMC_Circular_Queue & operator = (MPMC_Circular_Queue const&) = delete;

	MPMC_Circular_Queue(): index_mask_(QUEUE_SIZE - 1)
	{
		static_assert(((QUEUE_SIZE >= 2) && ((QUEUE_SIZE & (QUEUE_SIZE - 1)) == 0)), "Queue size is not power of 2");

		for (size_t i = 0; i != QUEUE_SIZE; i += 1)
			buffer_[i].sequence_.store(i, std::memory_order_relaxed);

		enqueue_index_.store(0, std::memory_order_relaxed);
		dequeue_index_.store(0, std::memory_order_relaxed);
	}

	bool enqueue(T data)
	{
		Data_Cell* cell;
		size_t index = enqueue_index_.load(std::memory_order_relaxed);

		for (;;)
		{
			cell = &buffer_[index & index_mask_];
			size_t seq = cell->sequence_.load(std::memory_order_acquire);

			intptr_t diff = (intptr_t)seq - (intptr_t)index;

			if (diff == 0)
			{
				if (enqueue_index_.compare_exchange_weak(index, index + 1, std::memory_order_relaxed))
					break;
			}
			else if (diff < 0)
				return false;
			else
				index = enqueue_index_.load(std::memory_order_relaxed);
		}

		cell->data_ = std::move(data);
		cell->sequence_.store(index + 1, std::memory_order_release);

		return true;
	}

	bool dequeue(T& data)
	{
		Data_Cell* cell;
		size_t index = dequeue_index_.load(std::memory_order_relaxed);

		for (;;)
		{
			cell = &buffer_[index & index_mask_];
			size_t seq = cell->sequence_.load(std::memory_order_acquire);

			intptr_t diff = (intptr_t)seq - (intptr_t)(index + 1);

			if (diff == 0)
			{
				if (dequeue_index_.compare_exchange_weak(index, index + 1, std::memory_order_relaxed))
					break;
			}
			else if (diff < 0)
				return false;
			else
				index = dequeue_index_.load(std::memory_order_relaxed);
		}

		data = std::move(cell->data_);
		cell->sequence_.store(index + index_mask_ + 1, std::memory_order_release);

		return true;
	}

private:
	struct Data_Cell
	{
		std::atomic<size_t> sequence_;
		T data_;
	};

	constexpr static size_t CACHELINE_SIZE = 64;

	using Cache_Line_Padding = char[CACHELINE_SIZE];

	Cache_Line_Padding pad0_;
	Data_Cell buffer_[QUEUE_SIZE];

	size_t const index_mask_;
	Cache_Line_Padding pad1_;

	std::atomic<size_t> enqueue_index_;
	Cache_Line_Padding pad2_;

	std::atomic<size_t> dequeue_index_;
	Cache_Line_Padding pad3_;
};

#include <unistd.h>

bool setAffinity(int coreId, std::thread &thread)
{
	int num_cores = sysconf(_GLIBCXX_USE_SC_NPROCESSORS_ONLN);

	if (coreId < 0 || coreId >= num_cores)
		return false;

	cpu_set_t cpuset;

	CPU_ZERO(&cpuset);
	CPU_SET(coreId, &cpuset);

	pthread_t currentThread = thread.native_handle();

	return pthread_setaffinity_np(currentThread, sizeof(cpu_set_t), &cpuset) == 0;
}

int main()
{
	constexpr int size = 1024;
	MPMC_Circular_Queue<int, size> queue;

	std::thread oddThread([&](){
		for (int i = 1; i <= 2*size; i += 2)
			queue.enqueue(i);
	});

	std::thread evenThread([&](){
		for (int i = 2; i <= 2*size; i += 2)
			queue.enqueue(i);
	});

	std::thread popThread([&](){
		for (int i = 1; i <= 2*size; ++i)
		{
			int data{0};
			if (queue.dequeue(data))
				cout << data << " ";
		}
	});

	//setAffinity(0, oddThread);
	//setAffinity(0, evenThread);
	//setAffinity(0, popThread);

	oddThread.join();
	evenThread.join();
	popThread.join();

	return 0;
}

#include "threadpool.hpp"
#include <iostream>
#include <chrono>

std::mutex console_mutex;

// test threadpool with a long time work
int pool_test_0(int j)
{
	std::this_thread::sleep_for(std::chrono::seconds(1) * j); // emulate heavy work

	auto lock = std::unique_lock{ console_mutex };
	std::cout << "thread id: " << std::this_thread::get_id() << '\n';

	return j;
}

// test threadpool with a container task
void pool_test_1(pool::threadpool& pool, uint32_t container_size)
{
	std::vector<int> test_container(container_size);
	for (int j = 0; j < test_container.size(); j++)
	{
		test_container[j] = j;
	}

	auto future_ptr = pool.enqueue_container_task(test_container, [](auto&& num) { num *= 2; });
	future_ptr->wait();

	for (auto&& elem : test_container)
	{
		std::cout << elem << ' ';
	}

	std::cout << '\n';
}

int main()
{
	constexpr uint32_t threads_number = 4;
	pool::threadpool pool{ threads_number };
	std::vector<pool::generic_future<int>> futures;
	
	for (int j = 0; j < 10; j++)
	{
		futures.push_back(pool.enqueue_task(pool_test_0, j));
	}

	for (auto&& future : futures)
	{
		future.get();
	}

	pool_test_1(pool, 100);

	return 0;
}
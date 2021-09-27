#include "threadpool.hpp"

pool::task_base::task_base()
{
	promise = std::make_shared<std::promise<std::any>>();
}

pool::threadpool::threadpool(uint32_t number_of_workers_)
{
	queue = std::make_shared<std::tuple<std::queue<std::shared_ptr<task_base>>, std::mutex, std::condition_variable>>();

	increase_workers_number(number_of_workers_);
}

uint32_t pool::threadpool::get_number_of_workers()
{
	return workers_state.size();
}

pool::threadpool::~threadpool()
{
	auto lock = std::unique_lock{ std::get<1>(*queue) };

	decrease_workers_number(get_number_of_workers());

	std::get<2>(*queue).notify_all();
}

void pool::threadpool::set_workers_number(uint32_t num)
{
	if (get_number_of_workers() > num)
	{
		decrease_workers_number(get_number_of_workers() - num);
	}
	else if (get_number_of_workers() < num)
	{
		increase_workers_number(num - get_number_of_workers());
	}
}

void pool::threadpool::increase_workers_number(uint32_t num)
{
	for (int j = 0; j < num; j++)
	{
		workers_state.push_back(std::make_shared<bool>(true));
		std::thread{ thread_main, workers_state.back(), queue }.detach();
	}
}

void pool::threadpool::decrease_workers_number(uint32_t num)
{
	if (num > workers_state.size())
	{
		throw std::exception{ "decrease number is bigger than the number of workers" };
	}
	
	for (int j = 0; j < num; j++)
	{
		*workers_state.back() = false;
		workers_state.pop_back();
	}

	std::get<2>(*queue).notify_all();
}

void pool::container_future::refresh_task_state()
{
	auto lock = std::unique_lock{ bitset_lock };

	for (auto&& element : tasks_status)
	{
		if (element == false)
		{
			element = true;
			break;
		}
	}

	if (std::ranges::all_of(tasks_status, [](auto&& status) { return status == true; }))
	{
		ready_status.test_and_set();
		ready_status.notify_all();
	}
}

pool::container_future::container_future(uint32_t number_of_tasks)
{
	tasks_status = std::vector<bool>(number_of_tasks, false);
}

void pool::container_future::wait()
{
	while (!ready_status.test())
	{
		ready_status.wait(false);
	}
}
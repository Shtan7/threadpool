#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>
#include <future>
#include <any>
#include <concepts>
#include <ranges>
#include <functional>
#include <algorithm>

namespace pool
{
	// returned by threadpool::enqueue_task 
	template<class t>
	struct generic_future
	{
		std::future<std::any> future;

		generic_future(std::future<std::any> future_)
		{
			future = std::move(future_);
		}

		t get()
		{
			return std::any_cast<t>(future.get());
		}
	};

	// represents single task in threadpool queue
	struct task_base
	{
		std::shared_ptr<std::promise<std::any>> promise;

		auto return_future_object()
		{
			return promise->get_future();
		}

		virtual void operator()() = 0;
		task_base();
		virtual ~task_base() = default;
	};

	template<class fn>
	struct task : task_base
	{
		fn function;

		task(fn function_) : function{ function_ }
		{}

		void operator()() override
		{
			if constexpr (std::is_same<decltype(function()), void>())
			{
				function();
			}
			else
			{
				promise->set_value(std::any{ function() });
			}
		}

		virtual ~task() = default;
	};

	// concept that only accepts a random access stl container
	template<class container>
	concept random_access_container = requires(container a, const container b)
	{
		requires std::regular<container>;
		requires std::swappable<container>;
		requires std::destructible<typename container::value_type>;
		requires std::same_as<typename container::reference, typename container::value_type&>;
		requires std::same_as<typename container::const_reference, const typename container::value_type&>;
		requires std::forward_iterator<typename container::iterator>;
		requires std::forward_iterator<typename container::const_iterator>;
		requires std::signed_integral<typename container::difference_type>;
		requires std::same_as<typename container::difference_type, typename std::iterator_traits<typename container::iterator>::difference_type>;
		requires std::same_as<typename container::difference_type, typename std::iterator_traits<typename container::const_iterator>::difference_type>;
		requires std::random_access_iterator<typename container::iterator>;
		requires std::random_access_iterator<typename container::const_iterator>;

		{ a.begin() } -> std::convertible_to<typename container::iterator>;
		{ a.end() } -> std::convertible_to<typename container::iterator>;
		{ b.begin() } -> std::convertible_to<typename container::const_iterator>;
		{ b.end() } -> std::convertible_to<typename container::const_iterator>;
		{ a.cbegin() } -> std::convertible_to<typename container::const_iterator>;
		{ a.cend() } -> std::convertible_to<typename container::const_iterator>;
		{ a.size() } -> std::convertible_to<typename container::size_type>;
		{ a.max_size() } -> std::convertible_to<typename container::size_type>;
		{ a.empty() } -> std::convertible_to<bool>;
	};

	class threadpool;

	// Future for a container task. Signals if all container related tasks are finished.
	class container_future
	{
		friend class threadpool;

	private:
		std::mutex bitset_lock;
		std::vector<bool> tasks_status;
		std::atomic_flag ready_status = {};

		void refresh_task_state();

	public:
		container_future(uint32_t number_of_tasks);
		void wait();
	};

	class threadpool
	{
	private:
		// tuple has 3 elemets: queue, queue lock and cv that signals to sleeping workers
		std::shared_ptr<std::tuple<std::queue<std::shared_ptr<task_base>>, std::mutex, std::condition_variable>> queue;
		std::vector<std::shared_ptr<bool>> workers_state;

		std::function<void(std::shared_ptr<bool>, decltype(queue))> thread_main = [](std::shared_ptr<bool> worker_status, auto local_queue_ptr)
		{
			std::shared_ptr<task_base> task;

			while (true)
			{
				{
					auto lock = std::unique_lock{ std::get<1>(*local_queue_ptr) };

					while (std::get<0>(*local_queue_ptr).empty() || !(*worker_status))
					{
						if (*worker_status == false)
						{
							// destroy thread if a worker status is false
							return;
						}

						std::get<2>(*local_queue_ptr).wait(lock);
					}

					task = std::get<0>(*local_queue_ptr).front();
					std::get<0>(*local_queue_ptr).pop();
				}

				task->operator()();
				task.reset();
			}
		};

		threadpool(threadpool&&) = delete;
		threadpool(threadpool const&) = delete;
		threadpool& operator=(threadpool&&) = delete;
		threadpool& operator=(threadpool const&) = delete;

	public:
		threadpool(uint32_t number_of_workers_ = std::thread::hardware_concurrency());
		~threadpool();
		uint32_t get_number_of_workers();
		void increase_workers_number(uint32_t num);
		void decrease_workers_number(uint32_t num);
		void set_workers_number(uint32_t num);

		template<class fx, class ...types>
		auto enqueue_task(fx&& func, types&& ...args)
		{
			auto binded_functor = std::bind(func, std::forward<types>(args)...);
			auto task_ptr = std::shared_ptr<task_base>(new task{ binded_functor });
			auto future = generic_future<decltype(binded_functor())>{ task_ptr->return_future_object() };

			{
				auto lock = std::unique_lock{ std::get<1>(*queue) };
				std::get<0>(*queue).push(std::move(task_ptr));

				std::get<2>(*queue).notify_one();
			}

			if constexpr (std::is_same<generic_future<void>, decltype(future)>())
			{
				return;
			}
			else
			{
				return future;
			}
		}

		template<class cnt>
		requires random_access_container<std::remove_cvref_t<cnt>>
		std::shared_ptr<container_future> enqueue_container_task(cnt& container, auto&& functor)
		{
			uint32_t number_of_threads = get_number_of_workers();
			uint32_t elements_in_task = container.size() / number_of_threads;
			uint32_t additional_task = container.size() % number_of_threads;
			
			uint32_t number_of_bits = additional_task ? number_of_threads + 1 : number_of_threads;
			auto future = std::make_shared<container_future>(number_of_bits);

			for (int j = 0; j < number_of_threads; j++)
			{
				auto task = [=](auto& container_, auto&& functor_, std::shared_ptr<container_future> future_ptr)
				{
					auto view = std::ranges::subrange(container_.begin() + j * elements_in_task, container_.begin() + (j + 1) * elements_in_task);
					for (auto&& element : view)
					{
						functor_(element);
					}

					future_ptr->refresh_task_state();
				};

				enqueue_task(task, std::ref(container), functor, future);
			}

			if (additional_task)
			{
				auto add_task = [=](auto& container_, auto&& functor_, std::shared_ptr<container_future> future_ptr)
				{
					auto view = std::ranges::subrange(container_.begin() + elements_in_task * number_of_threads,
						container_.begin() + number_of_threads * elements_in_task + additional_task);

					for (auto&& element : view)
					{
						functor_(element);
					}

					future_ptr->refresh_task_state();
				};

				enqueue_task(add_task, std::ref(container), functor, future);
			}

			return future;
		}
	};
}
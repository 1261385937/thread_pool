#pragma once

#include <memory>
#include <vector>
#include <future>
#include <atomic>
#include <thread>
#include <algorithm>
#include <array>
#include "blockingconcurrentqueue.h"

namespace thp {
	class function_wrapper {
	private:
		struct impl_base {
			virtual void call() = 0;
			virtual ~impl_base() {}
		};

		template<typename F, typename... ArgTypes>
		struct impl_type : impl_base {
			F f_;
			std::tuple<ArgTypes...> tuple_;

			template<typename Function, typename ...Types>
			impl_type(Function&& f, Types&& ... args)
				: f_(std::forward<Function>(f))
				, tuple_(std::forward_as_tuple(std::forward<Types>(args)...))
				//, tuple_(std::tie(args...) 
			{}

			template<std::size_t... I>
			void callable(std::index_sequence<I...>) {
				f_(std::get<I>(tuple_)...);
			}

			void call() {
				this->callable(std::make_index_sequence<sizeof...(ArgTypes)>());
			}
		};

		std::unique_ptr<impl_base> impl_;

	public:
		template<typename F, typename... ArgTypes>
		function_wrapper(F&& f, ArgTypes&& ... args) :
			impl_(std::make_unique<impl_type<F, ArgTypes...>>(std::forward<F>(f), std::forward<ArgTypes>(args)...))
		{}

		function_wrapper(function_wrapper&& other) noexcept
			:impl_(std::move(other.impl_))
		{}

		function_wrapper& operator=(function_wrapper&& other) noexcept
		{
			impl_ = std::move(other.impl_);
			return *this;
		}

		void operator()() { impl_->call(); }

		function_wrapper() = default;
		function_wrapper(const function_wrapper&) = delete;
		function_wrapper& operator=(const function_wrapper&) = delete;
	};

	enum class work_mode {
		nosteal,
		steal
	};

	template<work_mode WorkMode = work_mode::nosteal>
	class thread_pool
	{
	private:
		size_t pool_size_;
		std::atomic_bool run_;
		std::atomic<uint64_t> task_id_;
		std::vector<std::atomic<uint32_t>> tasks_count_;
		std::vector<std::atomic<uint32_t>> tasks_left_;
		std::vector<std::thread> threads_;
		std::vector<moodycamel::BlockingConcurrentQueue<function_wrapper>> queue_groups_;
	private:
		void start_thread(size_t index) {
			function_wrapper task;
			while (run_) {
				queue_groups_[index].wait_dequeue(task);
				tasks_left_[index]--;
				deal_task(std::move(task));

				if constexpr (WorkMode == work_mode::steal) {
					if (tasks_left_[index].load() != 0) {
						continue;
					}
					steal(index);
				}
			}

			while (queue_groups_[index].try_dequeue(task)) {
				tasks_left_[index]--;
				deal_task(std::move(task));

				if constexpr (WorkMode == work_mode::steal) {
					if (tasks_left_[index].load() != 0) {
						continue;
					}
					steal(index);
				}
			}
		}

		void deal_task(function_wrapper&& task) {
			try {
				task();
			}
			catch (const std::exception&) {
				//std::printf("thread_pool deal_task error: %s\n", e.what());
			}
		}

	public:
		explicit thread_pool(size_t pool_size = std::thread::hardware_concurrency())
			:pool_size_(pool_size)
			, run_(true)
			, task_id_(0)
			, tasks_count_(pool_size_)
			, tasks_left_(pool_size_) {
			for (size_t i = 0; i < pool_size; ++i) {
				queue_groups_.emplace_back(moodycamel::BlockingConcurrentQueue<function_wrapper>());
			}

			//can not put together. multiple thread
			for (size_t i = 0; i < pool_size; ++i) {
				threads_.emplace_back(std::thread(&thread_pool::start_thread, this, i));
			}
		}

		~thread_pool() {
			stop();
		}

		template<typename Function, typename... ArgTypes>
		auto add_task_future(Function&& f, ArgTypes&& ...args)
			//->std::future<typename std::invoke_result_t<Function, ArgTypes...>>
		{
			using result_type = typename std::invoke_result_t<Function, ArgTypes...>;
			std::packaged_task<result_type(ArgTypes...)> task(std::forward<Function>(f));
			std::future<result_type> res(task.get_future());
			auto task_id = task_id_.load();
			++task_id_;
			std::size_t index = task_id % pool_size_;
			queue_groups_[index].enqueue(function_wrapper(std::move(task), std::forward<ArgTypes>(args)...));
			tasks_count_[index]++;
			tasks_left_[index]++;
			return res;
		}

		template<typename Function, typename... ArgTypes>
		void add_task(Function&& f, ArgTypes&& ...args) {
			auto task_id = task_id_.load();
			++task_id_;
			std::size_t index = task_id % pool_size_;
			queue_groups_[index].enqueue(function_wrapper(std::forward<Function>(f), std::forward<ArgTypes>(args)...));
			tasks_count_[index]++;
			tasks_left_[index]++;
		}

		void stop() {
			run_ = false;
			//static int count = 0;
			for (size_t i = 0; i < pool_size_; ++i) {
				//count += tasks_count_[i];
				queue_groups_[i].enqueue(function_wrapper(exit_signal));
				tasks_left_[i]++;
			}
			for (auto& thread : threads_) {
				if (thread.joinable())
					thread.join();
			}
			//printf("all work thread exit\n");
		}

	private:
		static void exit_signal() {}

		void steal(size_t index) {
			function_wrapper task;
			for (size_t i = 0; i < pool_size_; ++i) {
			again:
				if (tasks_left_[i].load() == 0) {
					continue;
				}
				auto sucess = queue_groups_[i].try_dequeue(task);
				if (!sucess) {
					continue; //steal the next 
				}

				tasks_left_[i]--;
				deal_task(std::move(task));

				if (tasks_left_[index].load() == 0) {//self still has no task
					goto again; //attempt to steal this thread task again 
				}
				else { //go back to self
					break;
				}
			}
		}
	};
}

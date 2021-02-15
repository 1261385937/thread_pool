#pragma once

#include <memory>
#include <vector>
#include <future>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <array>
#include "concurrentqueue.h"

namespace nc {
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

		function_wrapper(function_wrapper&& other)
			:impl_(std::move(other.impl_))
		{}

		function_wrapper& operator=(function_wrapper&& other)
		{
			impl_ = std::move(other.impl_);
			return *this;
		}

		void operator()() { impl_->call(); }

		function_wrapper() = default;
		function_wrapper(const function_wrapper&) = delete;
		function_wrapper& operator=(const function_wrapper&) = delete;
	};

	class thread_pool
	{
	private:
		size_t pool_size_;
		std::atomic_bool run_;
		std::vector<std::shared_ptr<std::mutex>> mtxs_;
		std::atomic<uint64_t> task_id_;
		std::vector<std::shared_ptr<std::condition_variable>> conditions_;
		std::vector<std::thread> threads_;
		std::vector<moodycamel::ConcurrentQueue<function_wrapper>> queue_groups_;
	private:
		void start_thread(size_t index) {
			function_wrapper task;
			while (run_) {
				{
					//just for notify
					std::unique_lock<std::mutex> lock(*mtxs_[index]);
					conditions_[index]->wait(lock,
						[this, index, &task]() {return queue_groups_[index].try_dequeue(task) || !run_; });
				}
				if (run_) {
					deal_task(task);
				}
			}
			while (queue_groups_[index].try_dequeue(task)) { // still has untreated task
				deal_task(task);
			}
		}

		void deal_task(function_wrapper& task) {
			try {
				task();
			}
			catch (const std::exception& e) {
				std::printf("thread_pool deal_task error: %s\n", e.what());
			}
		}

	public:
		explicit thread_pool(size_t pool_size = std::thread::hardware_concurrency())
			:pool_size_(pool_size)
			, run_(true)
			, task_id_(0) {
			for (size_t i = 0; i < pool_size; ++i) {
				mtxs_.emplace_back(std::make_shared<std::mutex>());
				conditions_.emplace_back(std::make_shared<std::condition_variable>());
				queue_groups_.emplace_back(moodycamel::ConcurrentQueue<function_wrapper>());
			}

			//can not put together. multiple thread
			for (size_t i = 0; i < pool_size; ++i) {
				threads_.emplace_back(std::thread(&thread_pool::start_thread, this, i));
			}
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
			conditions_[index]->notify_one();
			return res;
		}

		template<typename Function, typename... ArgTypes>
		void add_task(Function&& f, ArgTypes&& ...args) {
			auto task_id = task_id_.load();
			++task_id_;
			std::size_t index = task_id % pool_size_;
			queue_groups_[index].enqueue(function_wrapper(std::forward<Function>(f), std::forward<ArgTypes>(args)...));
			conditions_[index]->notify_one();
			
		}

		void stop() {
			run_ = false;
			for (auto& cond : conditions_) {
				cond->notify_one();
			}

			for (auto& thread : threads_) {
				if (thread.joinable())
					thread.join();
			}
			//printf("all work thread exit\n");
		}
	};
}

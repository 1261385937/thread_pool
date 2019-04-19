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
#include "../concurrentqueue/concurrentqueue.h"

namespace thp {
	class FunctionWrapper
	{
	private:
		struct ImplBase
		{
			virtual void call() = 0;
			virtual ~ImplBase() {}
		};

		template<typename F, typename... ArgTypes>
		struct ImplType : ImplBase
		{
			F f_;
			std::tuple<ArgTypes...> tuple_;

			template<typename Function, typename ...Types>
			ImplType(Function&& f, Types&& ... args)
				: f_(std::forward<Function>(f))
				, tuple_(std::forward_as_tuple(std::forward<Types>(args)...))
				//, tuple_(std::tie(args...) 
			{}

			template<std::size_t... I>
			void callable(std::index_sequence<I...>) { f_(std::get<I>(tuple_)...); }

			void call() { callable(std::make_index_sequence<sizeof...(ArgTypes)>()); }
		};

		std::unique_ptr<ImplBase> impl_;

	public:
		template<typename F, typename... ArgTypes>
		FunctionWrapper(F&& f, ArgTypes&& ... args) :
			impl_(std::make_unique<ImplType<F, ArgTypes...>>(std::forward<F>(f), std::forward<ArgTypes>(args)...))
		{}

		FunctionWrapper(FunctionWrapper&& other)
			:impl_(std::move(other.impl_))
		{}

		FunctionWrapper& operator=(FunctionWrapper&& other)
		{
			impl_ = std::move(other.impl_);
			return *this;
		}

		void operator()() { impl_->call(); }

		FunctionWrapper() = default;
		FunctionWrapper(const FunctionWrapper&) = delete;
		FunctionWrapper& operator=(const FunctionWrapper&) = delete;
	};

	class ThreadPool
	{
	private:
		int pool_size_;
		std::atomic_bool run_;
		std::vector<std::shared_ptr<std::mutex>> mtxs_;
		std::atomic<uint64_t> task_id_;
		std::vector<std::shared_ptr<std::condition_variable>> conditions_;
		std::vector<std::thread> threads_;
		std::vector<moodycamel::ConcurrentQueue<FunctionWrapper>> queue_groups_;
	private:
		void startThread(int index)
		{
			FunctionWrapper task;
			while (run_)
			{
				//just for notify
				{
					std::unique_lock<std::mutex> lock(*mtxs_[index]);
					conditions_[index]->wait(lock,
						[this, index, &task]() {return queue_groups_[index].try_dequeue(task) || !run_; });
				}
				if (run_)
					dealTask(index, task);
			}
			while (!run_ && queue_groups_[index].try_dequeue(task)) // still has untreated task
				dealTask(index, task);
		}

		void dealTask(int queue_index, FunctionWrapper& task)
		{
			try
			{
				task();
			}
			catch (const std::exception & e)
			{
				std::printf("%s\n", e.what());
			}
		}

	public:
		explicit ThreadPool(int pool_size = std::thread::hardware_concurrency())
			:pool_size_(pool_size)
			, run_(true)
			,task_id_(0)
		{
			for (int i = 0; i < pool_size; ++i)
			{
				mtxs_.emplace_back(std::make_shared<std::mutex>());
				conditions_.emplace_back(std::make_shared<std::condition_variable>());
				queue_groups_.emplace_back(moodycamel::ConcurrentQueue<FunctionWrapper>());
			}
			//can not put together
			for (int i = 0; i < pool_size; ++i)
				threads_.emplace_back(std::thread(&ThreadPool::startThread, this, i));
		}

		template<typename Function, typename... ArgTypes>
		auto addTask(Function && f, ArgTypes && ...args)
			->std::future<typename std::invoke_result_t<Function, ArgTypes...>>
		{
			typedef typename std::invoke_result_t<Function, ArgTypes...> result_type;
			std::packaged_task<result_type(ArgTypes...)> task(std::forward<Function>(f));
			std::future<result_type> res(task.get_future());
			auto task_id = task_id_.load();
			++task_id_;
			auto index = task_id % pool_size_;
			queue_groups_[index].enqueue(FunctionWrapper(std::move(task), std::forward<ArgTypes>(args)...));
			conditions_[index]->notify_one();
			return res;
		}

		void stop()
		{
			run_ = false;
			for (auto& cond : conditions_)
				cond->notify_one();
			for (auto& thread : threads_)
			{
				if (thread.joinable())
					thread.join();
			}
			//printf("all work thread exit\n");
		}
	};
}

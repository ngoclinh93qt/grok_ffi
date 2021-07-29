/*
 *    Copyright (C) 2016-2021 Grok Image Compression Inc.
 *
 *    This source code is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This source code is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <map>
#include <type_traits>

/*
	tf::Executor executor;
	tf::Taskflow taskflow("simple");

	tf::Task A = taskflow.emplace([](){ std::cout << "TaskA\n"; });
	tf::Task B = taskflow.emplace([](){ std::cout << "TaskB\n"; });
	tf::Task C = taskflow.emplace([](){ std::cout << "TaskC\n"; });
	tf::Task D = taskflow.emplace([](){ std::cout << "TaskD\n"; });

	A.precede(B, C);  // A runs before B and C
	D.succeed(B, C);  // D runs after  B and C

	executor.run(taskflow).wait();
*/

class ThreadPool
{
  public:
	ThreadPool(size_t);
	template<class F, class... Args>
	auto enqueue(F&& f, Args&&... args)
		-> std::future<typename std::invoke_result<F, Args...>::type>;
	~ThreadPool();
	int thread_number(std::thread::id id)
	{
		if(id_map.find(id) != id_map.end())
			return (int)id_map[id];
		return -1;
	}
	size_t num_threads()
	{
		return m_num_threads;
	}

	static ThreadPool* get()
	{
		return instance(0);
	}
	static ThreadPool* instance(uint32_t numthreads)
	{
		std::unique_lock<std::mutex> lock(singleton_mutex);
		if(!singleton)
			singleton = new ThreadPool(numthreads ? numthreads : hardware_concurrency());
		return singleton;
	}
	static void release()
	{
		std::unique_lock<std::mutex> lock(singleton_mutex);
		delete singleton;
		singleton = nullptr;
	}
	static uint32_t hardware_concurrency()
	{
		uint32_t ret = 0;

#if _MSC_VER >= 1200 && MSC_VER <= 1910
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		ret = sysinfo.dwNumberOfProcessors;

#else
		ret = std::thread::hardware_concurrency();
#endif
		return ret;
	}

  private:
	// need to keep track of threads so we can join them
	std::vector<std::thread> workers;
	// the task queue
	std::queue<std::function<void()>> tasks;

	// synchronization
	std::mutex queue_mutex;
	std::condition_variable condition;
	bool stop;

	std::map<std::thread::id, size_t> id_map;
	size_t m_num_threads;

	static ThreadPool* singleton;
	static std::mutex singleton_mutex;
};

// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads) : stop(false), m_num_threads(threads)
{
	if(threads == 1)
		return;

	for(size_t i = 0; i < threads; ++i)
		workers.emplace_back([this] {
			for(;;)
			{
				std::function<void()> task;

				{
					std::unique_lock<std::mutex> lock(this->queue_mutex);
					this->condition.wait(lock,
										 [this] { return this->stop || !this->tasks.empty(); });
					if(this->stop && this->tasks.empty())
						return;
					task = std::move(this->tasks.front());
					this->tasks.pop();
				}

				task();
			}
		});
	size_t thread_count = 0;
	for(std::thread& worker : workers)
	{
		id_map[worker.get_id()] = thread_count;
#ifdef __linux__
		// Create a cpu_set_t object representing a set of CPUs. Clear it and mark
		// only CPU i as set.
		// Note: we assume that the second half of the logical cores
		// are hyper-threaded siblings to the first half
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		CPU_SET(thread_count, &cpuset);
		int rc = pthread_setaffinity_np(worker.native_handle(), sizeof(cpu_set_t), &cpuset);
		if(rc != 0)
		{
			std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
		}
#endif
		thread_count++;
	}
}

// add new work item to the pool
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
	-> std::future<typename std::invoke_result<F, Args...>::type>
{
	assert(m_num_threads > 1);
	using return_type = typename std::invoke_result<F, Args...>::type;

	auto task = std::make_shared<std::packaged_task<return_type()>>(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...));

	std::future<return_type> res = task->get_future();
	{
		std::unique_lock<std::mutex> lock(queue_mutex);

		// don't allow enqueueing after stopping the pool
		if(stop)
			throw std::runtime_error("enqueue on stopped ThreadPool");

		tasks.emplace([task]() { (*task)(); });
	}
	condition.notify_one();
	return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
	{
		std::unique_lock<std::mutex> lock(queue_mutex);
		stop = true;
	}
	condition.notify_all();
	for(std::thread& worker : workers)
		worker.join();
}

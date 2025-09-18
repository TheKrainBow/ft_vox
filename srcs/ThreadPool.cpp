#include "ThreadPool.hpp"

ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
	std::cout << "Threads available: " << numThreads << std::endl;
	for (size_t i = 0; i < numThreads; ++i) {
		workers.emplace_back([this] {
			while (true) {
				std::function<void()> task;
				{
					std::unique_lock<std::mutex> lock(this->queueMutex);
					this->condition.wait(lock, [this] {
						return this->stop || !this->tasks.empty();
					});
					// On shutdown, exit immediately without draining queued tasks
					if (this->stop)
						return;
					task = std::move(this->tasks.front());
					this->tasks.pop();
				}
				task();
			}
		});
	}
}

ThreadPool::~ThreadPool() {
	joinThreads();
}

void ThreadPool::joinThreads()
{
	{
		std::unique_lock<std::mutex> lock(queueMutex);
		// Make join idempotent and safe to call multiple times
		stop = true;
	}
	condition.notify_all();
	for (std::thread &worker : workers)
		if (worker.joinable()) worker.join();
	workers.clear();
	// Drop any remaining queued tasks
	{
		std::unique_lock<std::mutex> lock(queueMutex);
		std::queue<std::function<void()>> empty;
		tasks.swap(empty);
	}
}

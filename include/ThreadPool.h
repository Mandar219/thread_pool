#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <future>
#include "LockFreeQueue.h"

// Task wrapper for Priority Queue
struct Task {
    int priority;
    std::function<void()> func;
    // Max Heap: Largest priority number is at the top
    bool operator<(const Task& other) const { return priority < other.priority; }
};

class ThreadPool {
    std::vector<std::thread> workers;
    
    // 1. Mutex-based Priority Queue
    std::priority_queue<Task> priority_tasks;
    std::mutex priority_mutex;

    // 2. Lock-Free Queue for standard tasks
    LockFreeQueue<std::function<void()>> fast_tasks;

    // Synchronization
    std::condition_variable condition;
    std::atomic<bool> stop;
    std::atomic<size_t> active_tasks;

public:
    explicit ThreadPool(size_t threads) : stop(false), active_tasks(0) {
        for(size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this] {
                while(true) {
                    std::function<void()> task;
                    bool task_found = false;

                    // 1. Check Priority Queue (Needs Lock)
                    {
                        std::unique_lock<std::mutex> lock(this->priority_mutex);
                        if (!this->priority_tasks.empty()) {
                            task = std::move(const_cast<Task&>(this->priority_tasks.top()).func);
                            this->priority_tasks.pop();
                            task_found = true;
                        }
                    }

                    // 2. Check Lock-Free Queue (No Lock)
                    if (!task_found) {
                        task_found = this->fast_tasks.pop(task);
                    }

                    // 3. If no task, WAIT
                    if (!task_found) {
                        std::unique_lock<std::mutex> lock(this->priority_mutex);
                        if (this->stop && this->priority_tasks.empty() && this->fast_tasks.empty())
                            return;
                        
                        this->condition.wait(lock, [this] { 
                            return this->stop || 
                                   !this->priority_tasks.empty() || 
                                   this->active_tasks.load() > 0; 
                        });
                        continue; 
                    }

                    // Execute
                    task();
                    active_tasks.fetch_sub(1);
                }
            });
        }
    }

    // Enqueue Fast (Lock-Free)
    template<class F, class... Args>
    auto enqueue_fast(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
        using return_type = typename std::invoke_result<F, Args...>::type;
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        std::future<return_type> res = task->get_future();

        active_tasks.fetch_add(1);
        
        // LOOP until we successfully push (Spin-Wait Strategy)
        // If the Lock-Free queue is full, we spin. 
        // In a real app, you might fallback to the mutex queue after N retries.
        while(!fast_tasks.push([task](){ (*task)(); })) {
            std::this_thread::yield(); // Be nice to the CPU while waiting
        }
        
        condition.notify_one();
        return res;
    }

    // Enqueue Priority (Mutex)
    template<class F, class... Args>
    auto enqueue_priority(int priority, F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
        using return_type = typename std::invoke_result<F, Args...>::type;
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        std::future<return_type> res = task->get_future();

        {
            std::lock_guard<std::mutex> lock(priority_mutex);
            priority_tasks.push({priority, [task](){ (*task)(); }});
        }
        
        condition.notify_one();
        return res;
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(priority_mutex);
            stop = true;
        }
        condition.notify_all();
        for(std::thread &worker : workers) {
            if(worker.joinable()) worker.join();
        }
    }
};
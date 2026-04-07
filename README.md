# High-Performance C++ Task Scheduler

A low-latency, custom C++ thread pool leveraging a lock-free MPMC (Multi-Producer Multi-Consumer) ring buffer and priority scheduling. Designed to maximize CPU cache locality and eliminate OS-level lock contention, this scheduler is optimized for parallel workloads requiring deterministic execution and high throughput.

## Features

* **Lock-Free Fast Path:** A zero-allocation ring buffer queue utilizing `std::atomic` operations for ultra-fast task dispatching.
* **Priority Scheduling:** A mutex-backed priority queue for dynamic workload balancing and handling VIP tasks.
* **Async Execution:** Seamless `std::future` and `std::packaged_task` integration, allowing the main thread to easily retrieve results and catch background exceptions.
* **Hardware Sympathy:** Dynamically detects and scales to the system's logical core count.
* **Graceful Shutdown:** Implements safe lifecycle management for clean, leak-free teardowns.

## Performance
On an 8-core CPU (Apple M3), the scheduler achieves:
* **Throughput:** >660,000 tasks/sec
* **Latency:** Processed 1,000,000 parallel CPU-intensive workloads in ~1.5 seconds.

## Architecture Overview

- `LockFreeQueue.h`: Implements the bounded ring buffer. Uses std::atomic sequence numbers and compare_exchange_weak to prevent the ABA problem and eliminate the need for locks.

- `ThreadPool.h`: Manages the worker threads, condition variables, and std::packaged_task wrapping. Uses a hybrid approach: workers aggressively poll the lock-free queue and fall back to the priority queue/sleep if starved.

- `main.cpp`: The driver code simulating mixed traffic and calculating throughput.

## Prerequisites

* A modern C++ compiler supporting **C++17** or higher (e.g., GCC, Clang).
* `make` (or your preferred build system).
* POSIX threads (`-pthread` flag).

## Build Instructions

This project uses **CMake** for its build system. 

```bash
# Clone the repository
git clone https://github.com/Mandar219/thread_pool.git
cd cpp-task-scheduler

# Create a build directory and navigate into it
mkdir build && cd build

# Configure the project 
cmake ..

# Build the executable
cmake --build .
```

## Running the Application

The executable accepts an optional command-line argument to define the number of tasks to run during the performance test.

```bash
# Run with the default task count (50,000 tasks)
./scheduler

# Run with a custom task count (e.g., 1,000,000 tasks)
./scheduler 1000000
```

## Usage Example

```cpp
#include "ThreadPool.h"
#include <iostream>

int multiply(int a, int b) { return a * b; }

int main() {
    // Initialize pool with 8 hardware threads
    ThreadPool pool(8); 

    // 1. Enqueue to Lock-Free Fast Queue
    auto fast_result = pool.enqueue_fast(multiply, 10, 5);
    
    // 2. Enqueue to Priority Queue (Higher number = higher priority)
    int priority_level = 10; 
    auto priority_result = pool.enqueue_priority(priority_level, multiply, 20, 2);

    // Get results (blocks until tasks are complete)
    std::cout << "Fast Queue Result: " << fast_result.get() << "\n";
    std::cout << "Priority Queue Result: " << priority_result.get() << "\n";

    return 0;
}
```
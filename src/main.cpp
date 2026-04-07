#include <iostream>
#include <chrono>
#include <vector>
#include <future>
#include <csignal> // For signal handling
#include <string>
#include "../include/ThreadPool.h"

// Global flag for signal handler
std::atomic<bool> exit_requested(false);

void signal_handler(int signum) {
    std::cout << "\n[!] Interrupt signal (" << signum << ") received. Stopping...\n";
    exit_requested = true;
}

// A dummy CPU intensive task
void heavy_computation(int iterations) {
    volatile int result = 0;
    for (int i = 0; i < iterations; ++i) {
        result += i * i;
    }
}

int main(int argc, char* argv[]) {
    // 1. Dynamic Thread Count
    // hardware_concurrency can return 0 if not computable, so we provide a fallback
    unsigned int hw_threads = std::thread::hardware_concurrency();
    const int THREADS = (hw_threads > 0) ? hw_threads : 8;

    // 2. Dynamic Task Count via CLI Arguments
    int NUM_TASKS = 50000; // Default
    if (argc > 1) {
        try {
            NUM_TASKS = std::stoi(argv[1]);
            if (NUM_TASKS < 0) throw std::invalid_argument("Negative tasks");
        } catch (const std::exception& e) {
            std::cerr << "Invalid argument for NUM_TASKS. Using default: " << NUM_TASKS << "\n";
        }
    }

    // Register signal handler
    std::signal(SIGINT, signal_handler);
    
    std::cout << "================================================\n";
    std::cout << "Detected Hardware Cores: " << hw_threads << "\n";
    std::cout << "Initializing ThreadPool with " << THREADS << " threads...\n";
    std::cout << "Target Task Count: " << NUM_TASKS << "\n";
    std::cout << "================================================\n";

    {
        ThreadPool pool(THREADS);
        std::cout << "Starting Performance Test...\n";

        auto start_time = std::chrono::high_resolution_clock::now();
        std::vector<std::future<void>> results;
        results.reserve(NUM_TASKS);

        for(int i = 0; i < NUM_TASKS; ++i) {
            if (exit_requested) break;

            // 80% Fast Queue (Lock-Free), 20% Priority Queue (Mutex)
            if (i % 5 != 0) {
                results.emplace_back(pool.enqueue_fast(heavy_computation, 1000));
            } else {
                results.emplace_back(pool.enqueue_priority(i % 10, heavy_computation, 1000));
            }
        }

        // Wait for results
        int completed = 0;
        for(auto && result : results) {
            if (exit_requested) break;
            result.get(); 
            completed++;
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        if (exit_requested) {
            std::cout << "\n[!] Operation cancelled by user.\n";
        } else {
            std::cout << "------------------------------------------------\n";
            std::cout << "Tasks Completed: " << completed << "\n";
            std::cout << "Total Time: " << duration << " ms\n";
            if (duration > 0)
                std::cout << "Throughput: " << (completed * 1000.0 / duration) << " tasks/sec\n";
            std::cout << "------------------------------------------------\n";
        }
    } 

    std::cout << "Exiting cleanly.\n";
    return 0;
}
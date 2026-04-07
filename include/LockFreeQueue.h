#pragma once
#include <atomic>
#include <vector>
#include <optional>
#include <cassert>

template<typename T>
class LockFreeQueue {
private:
    struct Cell {
        std::atomic<size_t> sequence;
        T data;
        
        Cell() : sequence(0) {}
    };

    static const size_t BUFFER_SIZE = 1024; // Must be power of 2
    static const size_t MASK = BUFFER_SIZE - 1;

    std::vector<Cell> buffer;
    std::atomic<size_t> enqueue_pos; // Tail
    std::atomic<size_t> dequeue_pos; // Head

public:
    LockFreeQueue() : buffer(BUFFER_SIZE), enqueue_pos(0), dequeue_pos(0) {
        // Initialize sequences
        for (size_t i = 0; i < BUFFER_SIZE; ++i) {
            buffer[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    bool push(T const& data) {
        Cell* cell;
        size_t pos = enqueue_pos.load(std::memory_order_relaxed);

        while (true) {
            cell = &buffer[pos & MASK];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)pos;

            if (dif == 0) {
                // Slot is empty and ready for us
                if (enqueue_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    break; // Successfully claimed the slot
                }
            } else if (dif < 0) {
                // Queue is full
                return false;
            } else {
                // Position moved by another thread, try updated position
                pos = enqueue_pos.load(std::memory_order_relaxed);
            }
        }

        cell->data = data;
        // Release: Indicate data is ready. 
        // We increment sequence to (pos + 1)
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& result) {
        Cell* cell;
        size_t pos = dequeue_pos.load(std::memory_order_relaxed);

        while (true) {
            cell = &buffer[pos & MASK];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);

            if (dif == 0) {
                // Data is ready
                if (dequeue_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    break; // Successfully claimed the item
                }
            } else if (dif < 0) {
                // Queue is empty
                return false;
            } else {
                // Position moved by another thread
                pos = dequeue_pos.load(std::memory_order_relaxed);
            }
        }

        result = std::move(cell->data);
        // Release: Indicate slot is free. 
        // We increment sequence to (pos + mask + 1) for the next wrap-around
        cell->sequence.store(pos + MASK + 1, std::memory_order_release);
        return true;
    }
    
    bool empty() const {
        // Approximate check
        return dequeue_pos.load() == enqueue_pos.load();
    }
};
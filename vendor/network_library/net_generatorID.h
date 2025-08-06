#pragma once
#include <atomic>

class GeneratorID {
public:
    static uint64_t getID() {
        return counter.fetch_add(1, std::memory_order_relaxed);
    }

private:
    static inline std::atomic<uint64_t> counter = 0;
};
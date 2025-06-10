
#include "perf-lib.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <linux/perf_event.h>
#include <numeric>
#include <random>
#include <unordered_map>
#include <vector>

template<uint64_t v>
uint64_t __attribute__ ((noinline)) sum_if_less_than(const uint64_t* arr, size_t num_elems) {
    uint64_t res = 0;
    for (size_t i = 0; i < num_elems; ++i) {
        if (arr[i] < v) {
            res += arr[i];
        }
    }
    return res;
}


uint64_t __attribute__ ((noinline)) sequential_sum(const uint64_t* arr, size_t num_elems) {
    uint64_t res = 0;
    for (size_t i = 0; i < num_elems; ++i) {
        res += arr[i];
    }
    return res;
}

uint64_t random_sum(const uint64_t* arr, size_t num_elems) {
    uint64_t res = 0;
    size_t seed = 12345;

    for (int i = 0; i < num_elems; ++i) {
        seed = seed * 1103515245 + 12345;
        res += arr[(seed + i) % num_elems];
    }
    return res;
}

constexpr uint64_t vector_size = 10000000;

int main() {


    std::cout << "sequential access pattern" << std::endl;
    {
        std::vector<uint64_t> vec(vector_size);
        std::iota(vec.begin(), vec.end(), 0);
        PerfEventGroup events(PERF_COUNT_HW_CPU_CYCLES, PERF_TYPE_HARDWARE, "cycles");
        events.AddEvent(PERF_COUNT_HW_INSTRUCTIONS, PERF_TYPE_HARDWARE, "ins");
        events.AddEvent(PERF_COUNT_HW_CACHE_L1D |
                (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                (PERF_COUNT_HW_CACHE_RESULT_MISS << 16), PERF_TYPE_HW_CACHE, "l1d-cache-misses");

        events.AddEvent(PERF_COUNT_HW_CACHE_L1D |
                (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16), PERF_TYPE_HW_CACHE, "l1d-cache-accesses");

        events.AddEvent(PERF_COUNT_HW_CACHE_MISSES, PERF_TYPE_HARDWARE, "llc-cache-misses");
        events.AddEvent(PERF_COUNT_HW_CACHE_REFERENCES, PERF_TYPE_HARDWARE, "llc-cache-accesses");
        events.AddEvent(PERF_COUNT_HW_CACHE_L1D |
                (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) |
                (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16), PERF_TYPE_HW_CACHE, "l1d-cache-prefetches");

        events.Enable();
        uint64_t res = sequential_sum(vec.data(), vec.size());
        events.Disable();
        auto results = events.ReadEvents();
        std::cout << res << std::endl;

        for (const auto [key, value] : results) {
            std::cout << key << ", " << value << std::endl;
        }
    }


    std::cout << "random access pattern" << std::endl;
    {
        std::vector<uint64_t> vec(vector_size);
        std::iota(vec.begin(), vec.end(), 0);

        PerfEventGroup events(PERF_COUNT_HW_CPU_CYCLES, PERF_TYPE_HARDWARE, "cycles");
        events.AddEvent(PERF_COUNT_HW_INSTRUCTIONS, PERF_TYPE_HARDWARE, "ins");
        events.AddEvent(PERF_COUNT_HW_CACHE_L1D |
                (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                (PERF_COUNT_HW_CACHE_RESULT_MISS << 16), PERF_TYPE_HW_CACHE, "l1d-cache-misses");

        events.AddEvent(PERF_COUNT_HW_CACHE_L1D |
                (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16), PERF_TYPE_HW_CACHE, "l1d-cache-accesses");

        events.AddEvent(PERF_COUNT_HW_CACHE_MISSES, PERF_TYPE_HARDWARE, "llc-cache-misses");
        events.AddEvent(PERF_COUNT_HW_CACHE_REFERENCES, PERF_TYPE_HARDWARE, "llc-cache-accesses");
        events.AddEvent(PERF_COUNT_HW_CACHE_L1D |
                (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) |
                (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16), PERF_TYPE_HW_CACHE, "l1d-cache-prefetches");

        events.Enable();
        uint64_t res = random_sum(vec.data(), vec.size());
        events.Disable();
        auto results = events.ReadEvents();
        std::cout << res << std::endl;

        for (const auto [key, value] : results) {
            std::cout << key << ", " << value << std::endl;
        }
    }

    {
        std::cout << "sorted array" << std::endl;
        PerfEventGroup events(PERF_COUNT_HW_BRANCH_INSTRUCTIONS, PERF_TYPE_HARDWARE, "branch-ins");
        events.AddEvent(PERF_COUNT_HW_BRANCH_MISSES, PERF_TYPE_HARDWARE, "branch-mispredict");
        std::vector<uint64_t> vec(vector_size);
        std::iota(vec.begin(), vec.end(), 0);

        events.Enable();
        uint64_t res = sum_if_less_than<vector_size / 2>(vec.data(), vec.size());
        events.Disable();
        auto results = events.ReadEvents();

        std::cout << res << std::endl;

        for (const auto [key, value] : results) {
            std::cout << key << ", " << value << std::endl;
        }
    }

    {
        std::cout << "shuffled array" << std::endl;
        PerfEventGroup events(PERF_COUNT_HW_BRANCH_INSTRUCTIONS, PERF_TYPE_HARDWARE, "branch-ins");
        events.AddEvent(PERF_COUNT_HW_BRANCH_MISSES, PERF_TYPE_HARDWARE, "branch-mispredict");
        std::vector<uint64_t> vec(vector_size);
        std::iota(vec.begin(), vec.end(), 0);

        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(vec.begin(), vec.end(), g);

        events.Enable();
        uint64_t res = sum_if_less_than<vector_size / 2>(vec.data(), vec.size());
        events.Disable();
        auto results = events.ReadEvents();

        std::cout << res << std::endl;

        for (const auto [key, value] : results) {
            std::cout << key << ", " << value << std::endl;
        }
    }



    return 0;
}
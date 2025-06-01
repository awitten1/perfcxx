
#include "perf-lib.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <linux/perf_event.h>
#include <numeric>
#include <unordered_map>
#include <vector>

int main() {

    int vector_size = 10000000;


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



        events.Enable();

        uint64_t res = 0;
        for (int i = 0; i < vec.size(); ++i) {
            res += vec[i];
        }

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
        uint32_t seed = 12345;

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

        events.Enable();

        uint64_t res = 0;
        for (int i = 0; i < vec.size(); ++i) {
            seed = seed * 1103515245 + 12345;
            res += vec[seed % vec.size()];
        }
        events.Disable();
        auto results = events.ReadEvents();
        std::cout << res << std::endl;

        for (const auto [key, value] : results) {
            std::cout << key << ", " << value << std::endl;
        }
    }




    return 0;
}
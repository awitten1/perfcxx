
#include "perf-lib.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <linux/perf_event.h>
#include <memory>
#include <numeric>
#include <string>
#include <unordered_map>
#include <vector>

// 00000000000032f0 <_Z14sequential_sumPKmm>:
//     32f0:	f3 0f 1e fa          	endbr64
//     32f4:	48 85 f6             	test   %rsi,%rsi
//     32f7:	74 17                	je     3310 <_Z14sequential_sumPKmm+0x20>
//     32f9:	48 8d 14 f7          	lea    (%rdi,%rsi,8),%rdx
//     32fd:	31 c0                	xor    %eax,%eax
//     32ff:	90                   	nop
//     3300:	48 03 07             	add    (%rdi),%rax
//     3303:	48 83 c7 08          	add    $0x8,%rdi
//     3307:	48 39 d7             	cmp    %rdx,%rdi
//     330a:	75 f4                	jne    3300 <_Z14sequential_sumPKmm+0x10>
//     330c:	c3                   	ret
//     330d:	0f 1f 00             	nopl   (%rax)
//     3310:	31 c0                	xor    %eax,%eax
//     3312:	c3                   	ret
//     3313:	66 66 2e 0f 1f 84 00 	data16 cs nopw 0x0(%rax,%rax,1)
//     331a:	00 00 00 00
//     331e:	66 90                	xchg   %ax,%ax
uint64_t __attribute__ ((noinline)) sequential_sum(const uint64_t* arr, size_t sz) {
    uint64_t res = 0;
    for (size_t i = 0; i < sz; ++i) {
        res += arr[i];
    }
    return res;
}


// 0000000000003320 <_Z10random_sumPKmm>:
//     3320:	f3 0f 1e fa          	endbr64
//     3324:	48 85 f6             	test   %rsi,%rsi
//     3327:	74 37                	je     3360 <_Z10random_sumPKmm+0x40>
//     3329:	45 31 c0             	xor    %r8d,%r8d          # zeroes r8
//     332c:	45 31 c9             	xor    %r9d,%r9d          # zeroes r9
//     332f:	b9 39 30 00 00       	mov    $0x3039,%ecx       # 12345 = 0x3039
//     3334:	0f 1f 40 00          	nopl   0x0(%rax)          # nop
//     3338:	48 69 c9 6d 4e c6 41 	imul   $0x41c64e6d,%rcx,%rcx  # 1103515245 = 0x41c64e6d
//     333f:	31 d2                	xor    %edx,%edx          # zero rdx
//     3341:	49 83 c0 01          	add    $0x1,%r8           # add 1 to r8
//     3345:	48 81 c1 39 30 00 00 	add    $0x3039,%rcx       # add 12345 to seed
//     334c:	48 89 c8             	mov    %rcx,%rax          # rcx -> rax
//     334f:	48 f7 f6             	div    %rsi
//     3352:	4c 03 0c d7          	add    (%rdi,%rdx,8),%r9  # one memory access
//     3356:	4c 39 c6             	cmp    %r8,%rsi
//     3359:	75 dd                	jne    3338 <_Z10random_sumPKmm+0x18>
//     335b:	4c 89 c8             	mov    %r9,%rax
//     335e:	c3                   	ret
//     335f:	90                   	nop
//     3360:	45 31 c9             	xor    %r9d,%r9d
//     3363:	4c 89 c8             	mov    %r9,%rax
//     3366:	c3                   	ret
//     3367:	90                   	nop
//     3368:	0f 1f 84 00 00 00 00 	nopl   0x0(%rax,%rax,1)
//     336f:	00
//
// This code results in lower l1d hit rate.  There is some strangeness in the measurements.
// When vector size = 1e6, the hit rate is close to 0%.  When vector size = 1e7,
// for some reason, the l1d access rate spikes by more than a factor of 10 (closer to a factor of 20)
// and the hit rate spikes to ~45%.  But the l1d cache misses continues to be around vector size number.
// The question is where are the extra l1d cache accesses coming from.
uint64_t __attribute__ ((noinline)) random_sum(const uint64_t* arr, size_t sz) {
    size_t seed = 12345;
    uint64_t result = 0;
    for (size_t i = 0; i < sz; ++i) {
        seed = seed * 1103515245 + 12345;
        result += arr[seed % sz];
    }
    return result;
}




int main(int argc, char** argv) {

    std::ofstream output_file("out.csv", std::ios::trunc | std::ios::out);

    bool written_header = false;

    for (int i = 0; i < 24; ++i) {
        int vector_size = 1 << i;

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
                    (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) |
                    (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16), PERF_TYPE_HW_CACHE, "l1d-cache-prefetch");

            events.AddEvent(PERF_COUNT_HW_CACHE_L1D |
                    (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                    (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16), PERF_TYPE_HW_CACHE, "l1d-cache-accesses");

            events.AddEvent(PERF_COUNT_HW_CACHE_MISSES, PERF_TYPE_HARDWARE, "llc-cache-misses");
            events.AddEvent(PERF_COUNT_HW_CACHE_REFERENCES, PERF_TYPE_HARDWARE, "llc-cache-accesses");



            events.Enable();

            uint64_t res = sequential_sum(vec.data(), vec.size());

            events.Disable();
            auto results = events.ReadEvents();
            std::cout << res << std::endl;

            if (!written_header) {
                output_file << "vector_size,access_pattern,";
                for (auto it = results.begin(); it != results.end(); ++it) {
                    output_file << it->first;
                    if (std::next(it) != results.end()){
                        output_file << ",";
                    }
                }
                output_file << '\n';
                written_header = true;
            }
            output_file << vector_size << "," << "sequential,";
            for (auto it = results.begin(); it != results.end(); ++it) {
                output_file << it->second;
                if (std::next(it) != results.end()){
                    output_file << ",";
                }
            }
            output_file << '\n';
            //std::cout << "l1d-cache hit rate = " << (float)(results.at("l1d-cache-accesses") - results.at("l1d-cache-misses")) / results.at("l1d-cache-accesses") << std::endl;

        }


        std::cout << "random access pattern" << std::endl;
        {
            //std::vector<uint64_t> vec(vector_size);
            std::unique_ptr<uint64_t[]> arr(new uint64_t[vector_size]);
            std::iota(arr.get(), arr.get() + vector_size, 0);
            uint32_t seed = 12345;

            PerfEventGroup events(PERF_COUNT_HW_CPU_CYCLES, PERF_TYPE_HARDWARE, "cycles");
            events.AddEvent(PERF_COUNT_HW_INSTRUCTIONS, PERF_TYPE_HARDWARE, "ins");
            events.AddEvent(PERF_COUNT_HW_CACHE_L1D |
                    (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                    (PERF_COUNT_HW_CACHE_RESULT_MISS << 16), PERF_TYPE_HW_CACHE, "l1d-cache-misses");

            events.AddEvent(PERF_COUNT_HW_CACHE_L1D |
                    (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                    (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16), PERF_TYPE_HW_CACHE, "l1d-cache-accesses");

            events.AddEvent(PERF_COUNT_HW_CACHE_L1D |
                    (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) |
                    (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16), PERF_TYPE_HW_CACHE, "l1d-cache-prefetch");


            events.AddEvent(PERF_COUNT_HW_CACHE_MISSES, PERF_TYPE_HARDWARE, "llc-cache-misses");
            events.AddEvent(PERF_COUNT_HW_CACHE_REFERENCES, PERF_TYPE_HARDWARE, "llc-cache-accesses");

            events.Enable();

            uint64_t res = random_sum(arr.get(), vector_size);

            events.Disable();
            auto results = events.ReadEvents();
            std::cout << res << std::endl;

            output_file << vector_size << "," << "random,";

            for (auto it = results.begin(); it != results.end(); ++it) {
                output_file << it->second;
                if (std::next(it) != results.end()){
                    output_file << ",";
                }
            }
            output_file << '\n';
            //std::cout << "l1d-cache hit rate = " << (float)(results.at("l1d-cache-accesses") - results.at("l1d-cache-misses")) / results.at("l1d-cache-accesses") << std::endl;
        }

    }




    return 0;
}
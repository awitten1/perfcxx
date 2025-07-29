
// This file has AMD model 1AH, family 44H specific counters.
// Reference guide for this is here:
//  "Processor Programming Reference (PPR) for AMD Family 1Ah Model 44h"

#include "perf-lib.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <linux/perf_event.h>
#include <numeric>
#include <string>

namespace amd {

namespace family_1ah {

namespace model_44ah {

inline bool check_processor_model() {
    std::ifstream ifile("/proc/cpuinfo");

    std::string line;
    while(std::getline(ifile, line)) {
        auto pos = line.find(':');
        auto key = line.substr(0, pos);
        if (key.find("cpu family") != std::string::npos) {
            auto val = line.substr(pos + 1);
            int cpu_family = std::stoi(val);
            if (cpu_family != 0x1a) {
                return false;
            }
        } else if (key.find("model name") != std::string::npos) {
            auto val = line.substr(pos + 1);
            if (val.find("AMD") == std::string::npos) {
                return false;
            }
        } else if (key.find("model") != std::string::npos) {
            auto val = line.substr(pos + 1);
            int model = std::stoi(val);
            if (model != 0x44) {
                return false;
            }
        }
    }

    return true;

}


// inline void cpuid() {
//     int eax = 0, ebx = 0, ecx = 0, edx = 0, leaf = 0;
//     asm volatile ("cpuid"
//               : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
//               : "a" (leaf));

//     int model = (eax >> 4) & ((1 << 4) - 1);
//     std::cout << "model = " << std::hex << model << std::endl;
// }

// These are taken from https://github.com/torvalds/linux/tree/403d1338a4a59cfebb4ded53fa35fbd5119f36b1/tools/perf/pmu-events/arch/x86/amdzen5
// That link contains events and recommended "metrics" which are functions of the events.
// The event codes are umask + eventCode.
inline void add_amd_specific_events(PerfEventGroup* event_group) {
    event_group->AddEvent(0xf760, PERF_TYPE_RAW, "amd_all_l2_cache_access");
    event_group->AddEvent(0xf160, PERF_TYPE_RAW, "amd_l2_cache_access_no_prefetch");
    event_group->AddEvent(0x0729, PERF_TYPE_RAW, "amd_memory_ops");
    event_group->AddEvent(0xe060, PERF_TYPE_RAW, "amd_all_data_l2_cache_access");
    //event_group->AddEvent(0x0260, PERF_TYPE_RAW, "amd_l2_request_g1_l2_hw_pf");

}

}

}
}


#include "perf-lib.hpp"
#include <memory>

static std::unique_ptr<PerfEventGroup> event_group(new PerfEventGroup{});

PerfEventGroup& GetGlobalPerfEventGroup() {
    return *event_group;
}
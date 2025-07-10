#include "perf-lib.hpp"
#include <memory>

static std::unique_ptr<PerfEventGroup> event_group;

PerfEventGroup& GetGlobalPerfEventGroup() {
    return *event_group;
}
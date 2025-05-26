#pragma once

#include <algorithm>
#include <cstring>
#include <iostream>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <thread>

// https://man7.org/linux/man-pages/man2/perf_event_open.2.html
class PerfEventGroup {
    int fd_;

    struct EventInfo {
        uint64_t config;
        uint64_t id;
        std::string event_name;
        int fd;
    };
    std::vector<EventInfo> event_info_map_;

    uint64_t GetEventId(int fd) {
        uint64_t event_id;
        int ret = ioctl(fd, PERF_EVENT_IOC_ID, &event_id);
        if (ret == -1) {
            std::cerr << "failed to get event id" << std::endl;
            return -1;
        }
        return event_id;
    }

    int NumEvents() {
        return event_info_map_.size();
    }

    void AddEvent(bool leader, uint64_t config, uint32_t type, const std::string& event_name) {
        struct perf_event_attr pe;
        memset(&pe, 0, sizeof(pe));
        pe.config = config;
        pe.type = type;
        pe.size = sizeof(pe);

        // disable leader initially.  when leader starts, everyone starts.
        pe.disabled = leader ? 1 : 0;
        pe.sample_period = 0;
        pe.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
        pe.exclude_hv = 1;
        pe.exclude_kernel = 0;
        unsigned long flags = 0;
        int cpu = -1; // any cpu
        pid_t pid = 0; // this process
        int group_fd = leader ? -1 : fd_;
        int fd = syscall(SYS_perf_event_open, &pe, pid, cpu, group_fd, flags);
        if (leader) {
            fd_ = fd;
        }
        if (fd == -1) {
            std::cerr << "failed opening perf event, reason = " << strerror(errno) << std::endl;
            return;
        }
        event_info_map_.push_back(EventInfo{
            .config = config, .id = GetEventId(fd), .event_name = event_name, .fd = fd});
    }


public:

    explicit PerfEventGroup(uint64_t config, uint32_t type, const std::string& name) {
        AddEvent(true, config, type, name);
    }

    ~PerfEventGroup() {
        for (auto it = event_info_map_.rbegin(); it != event_info_map_.rend(); ++it) {
            close(it->fd);
        }
    }

    int GetFd() {
        return fd_;
    }

    struct EventArg {
        uint64_t config;
        uint32_t type;
        std::string name;
    };

    static PerfEventGroup BuildPerfEventGroup(std::vector<EventArg> args) {
        auto& f = args.front();
        PerfEventGroup ret(f.config, f.type, f.name);
        for (int i = 1; i < args.size(); ++i) {
            f = args[i];
            ret.AddEvent(f.config, f.type, f.name);
        }
        return ret;
    }

    std::unordered_map<std::string, uint64_t> ReadEvents() {
        std::unordered_map<std::string, uint64_t> result_set;

        // read returns an array of {value, event_id tuples}
        std::vector<uint64_t> events(1 + 2 * NumEvents());
        int ret = read(fd_, events.data(), events.size() * 8);
        if (ret == -1) {
            std::ostringstream oss;
            oss << "failed to read events reason = " << strerror(errno);
            std::cerr << oss.str() << std::endl;
            return {};
        }
        if (events[0] != NumEvents()) {
            std::cerr << "number of events recieved does not equal expected number" << std::endl;
            return {};
        }

        uint64_t id = -1;
        uint64_t value = -1;
        // skip first element, which holds nr, the number of events
        for (size_t i = 1; i < events.size(); ++i) {
            if ((i & 1) == 0) {
                id = events[i];
                auto it = std::find_if(event_info_map_.begin(), event_info_map_.end(),
                    [id](auto v) { return v.id == id; });
                if (it == event_info_map_.end()) {
                    throw std::runtime_error{"id found in result set not registered. this should not happen"};
                }
                result_set[it->event_name] = value;
            } else {
                value = events[i];
            }
        }
        return result_set;
    }

    void Enable() {
        // is PERF_IOC_FLAG_GROUP needed when operating on group leader?
        int ret = ioctl(fd_,  PERF_EVENT_IOC_RESET);
        if (ret == -1) {
            throw std::runtime_error{"failed to reset counters"};
        }
        ret = ioctl(fd_,  PERF_EVENT_IOC_ENABLE);
        if (ret == -1) {
            throw std::runtime_error{"failed to enable events"};
        }
    }

    void AddEvent(uint64_t config, uint32_t type, const std::string& name) {
        AddEvent(false, config, type, name);
    }

    void Reset() {
        int ret = ioctl(fd_, PERF_IOC_FLAG_GROUP | PERF_EVENT_IOC_RESET);
        if (ret == -1) {
            std::ostringstream oss;
            oss << "failed to reset counters";
            std::cerr << oss.str() << std::endl;
        }
    }
};

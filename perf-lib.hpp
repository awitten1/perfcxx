#pragma once

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <optional>
#include <ratio>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <map>
#include <vector>
#include <string>
#include <thread>


// https://man7.org/linux/man-pages/man2/perf_event_open.2.html
class PerfEventGroup {

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
        pe.disabled = 1;
        pe.sample_period = 0;
        pe.exclude_hv = 1;
        pe.exclude_kernel = 0;
        unsigned long flags = 0;


        /**
        From https://man7.org/linux/man-pages/man2/perf_event_open.2.html

        "Total time the event was enabled and running.  Normally
         these values are the same.  Multiplexing happens if the
         number of events is more than the number of available PMU
         counter slots.  In that case the events run only part of
         the time and the time_enabled and time running values can
         be used to scale an estimated value for the count."
         */
        pe.read_format = PERF_FORMAT_ID | PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;
        int cpu = -1; // any cpu
        pid_t pid = 0; // this process
        int group_fd = -1;
        int fd = syscall(SYS_perf_event_open, &pe, pid, cpu, group_fd, flags);
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

    explicit PerfEventGroup() {}

    ~PerfEventGroup() {
        for (auto it = event_info_map_.rbegin(); it != event_info_map_.rend(); ++it) {
            close(it->fd);
        }
    }

    struct EventArg {
        uint64_t config;
        uint32_t type;
        std::string name;
    };

    using u64 = uint64_t;
    struct read_format {
        u64 value;         /* The value of the event */
        u64 time_enabled;  /* if PERF_FORMAT_TOTAL_TIME_ENABLED */
        u64 time_running;  /* if PERF_FORMAT_TOTAL_TIME_RUNNING */
        u64 id;            /* if PERF_FORMAT_ID */
        //u64 lost;          /* if PERF_FORMAT_LOST */
    };




    std::map<std::string, std::optional<uint64_t>> ReadEvents() {
        std::map<std::string, std::optional<uint64_t>> result_set;

        for (auto& event : event_info_map_) {
            read_format buf;
            memset(&buf, 0, sizeof(buf));
            long bytes_read = read(event.fd, (void*)&buf, sizeof(buf));
            if (bytes_read != sizeof(buf)) {
                std::cerr << "didn't read all the bytes" << std::endl;
            }
            double scale_factor = (double)buf.time_enabled / buf.time_running;
            if (buf.time_running == 0) {
                result_set[event.event_name] = std::nullopt;
            } else {
                std::cout << scale_factor << std::endl;
                result_set[event.event_name] = (scale_factor)*buf.value;
            }
        }

        std::chrono::duration<double, std::milli> ms = disable_time_ - enable_time_;
        result_set["wall_clock_ms"] = ms.count();

        return result_set;
    }


    std::chrono::time_point<std::chrono::steady_clock> enable_time_;
    std::chrono::time_point<std::chrono::steady_clock> disable_time_;

    void Enable() {
        for (auto& event : event_info_map_) {
            ioctl(event.fd, PERF_EVENT_IOC_RESET);
        }
        for (auto& event : event_info_map_) {
            ioctl(event.fd, PERF_EVENT_IOC_ENABLE);
        }
        enable_time_ = std::chrono::steady_clock::now();
    }

    void Disable() {
        for (auto& event : event_info_map_) {
            ioctl(event.fd, PERF_EVENT_IOC_DISABLE);
        }
        disable_time_ = std::chrono::steady_clock::now();
    }

    void AddEvent(uint64_t config, uint32_t type, const std::string& name) {
        AddEvent(event_info_map_.empty(), config, type, name);
    }

};

PerfEventGroup& GetGlobalPerfEventGroup();


#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "service/login/login_service.h"

namespace ddz {

class ObservabilityService {
public:
    void Configure(bool enable_structured_log, int64_t metrics_report_interval_ms);

    bool structured_log_enabled() const;
    int64_t metrics_report_interval_ms() const;

    static void SetCurrentTraceId(const std::string& trace_id);
    static std::string CurrentTraceId();

    void Record(const std::string& event, ErrorCode code, int64_t latency_ms);
    std::optional<std::string> TryBuildMetricsSnapshot(int64_t now_ms);

private:
    struct EventStats {
        uint64_t total = 0;
        uint64_t success = 0;
        uint64_t fail = 0;
        std::vector<int64_t> latencies_ms;
    };

    static int64_t Percentile(const std::vector<int64_t>& values, int percentile);

private:
    mutable std::mutex mu_;
    bool enable_structured_log_ = true;
    int64_t metrics_report_interval_ms_ = 30000;
    int64_t last_report_ms_ = 0;
    std::unordered_map<std::string, EventStats> stats_;
};

}  // namespace ddz


#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <string>

namespace ddz {

class GatewayObservabilityService {
public:
    void Configure(bool enable_structured_log, int64_t metrics_report_interval_ms);

    bool structured_log_enabled() const;

    void RecordWsConnectionOpened();
    void RecordWsConnectionClosed();
    void RecordWsIn();
    void RecordWsOut();
    void RecordHttpRequest(const std::string& route, bool success);
    void RecordRateLimited();
    void RecordRefreshResult(bool success);
    void RecordUpstreamRttMs(int64_t latency_ms);

    std::optional<std::string> TryBuildMetricsSnapshot(int64_t now_ms);

private:
    static int64_t Percentile(const std::deque<int64_t>& values, int percentile);

private:
    mutable std::mutex mu_;
    bool enable_structured_log_ = true;
    int64_t metrics_report_interval_ms_ = 30000;
    int64_t last_report_ms_ = 0;

    int64_t ws_conn_current_ = 0;
    uint64_t ws_conn_total_ = 0;
    uint64_t ws_in_total_ = 0;
    uint64_t ws_out_total_ = 0;
    uint64_t rate_limited_total_ = 0;
    uint64_t refresh_total_ = 0;
    uint64_t refresh_success_ = 0;

    std::map<std::string, uint64_t> http_req_total_by_route_;
    std::map<std::string, uint64_t> http_req_fail_by_route_;
    std::deque<int64_t> upstream_rtt_ms_;
};

}  // namespace ddz

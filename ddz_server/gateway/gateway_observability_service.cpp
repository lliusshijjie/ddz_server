#include "gateway/gateway_observability_service.h"

#include <algorithm>
#include <sstream>
#include <vector>

namespace ddz {

void GatewayObservabilityService::Configure(bool enable_structured_log, int64_t metrics_report_interval_ms) {
    std::lock_guard<std::mutex> lock(mu_);
    enable_structured_log_ = enable_structured_log;
    if (metrics_report_interval_ms > 0) {
        metrics_report_interval_ms_ = metrics_report_interval_ms;
    }
}

bool GatewayObservabilityService::structured_log_enabled() const {
    std::lock_guard<std::mutex> lock(mu_);
    return enable_structured_log_;
}

void GatewayObservabilityService::RecordWsConnectionOpened() {
    std::lock_guard<std::mutex> lock(mu_);
    ws_conn_current_ += 1;
    ws_conn_total_ += 1;
}

void GatewayObservabilityService::RecordWsConnectionClosed() {
    std::lock_guard<std::mutex> lock(mu_);
    ws_conn_current_ = std::max<int64_t>(0, ws_conn_current_ - 1);
}

void GatewayObservabilityService::RecordWsIn() {
    std::lock_guard<std::mutex> lock(mu_);
    ws_in_total_ += 1;
}

void GatewayObservabilityService::RecordWsOut() {
    std::lock_guard<std::mutex> lock(mu_);
    ws_out_total_ += 1;
}

void GatewayObservabilityService::RecordHttpRequest(const std::string& route, bool success) {
    std::lock_guard<std::mutex> lock(mu_);
    http_req_total_by_route_[route] += 1;
    if (!success) {
        http_req_fail_by_route_[route] += 1;
    }
}

void GatewayObservabilityService::RecordRateLimited() {
    std::lock_guard<std::mutex> lock(mu_);
    rate_limited_total_ += 1;
}

void GatewayObservabilityService::RecordRefreshResult(bool success) {
    std::lock_guard<std::mutex> lock(mu_);
    refresh_total_ += 1;
    if (success) {
        refresh_success_ += 1;
    }
}

void GatewayObservabilityService::RecordUpstreamRttMs(int64_t latency_ms) {
    if (latency_ms < 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(mu_);
    upstream_rtt_ms_.push_back(latency_ms);
    if (upstream_rtt_ms_.size() > 20000) {
        upstream_rtt_ms_.erase(upstream_rtt_ms_.begin(), upstream_rtt_ms_.begin() + 10000);
    }
}

std::optional<std::string> GatewayObservabilityService::TryBuildMetricsSnapshot(int64_t now_ms) {
    std::lock_guard<std::mutex> lock(mu_);
    if (last_report_ms_ != 0 && now_ms - last_report_ms_ < metrics_report_interval_ms_) {
        return std::nullopt;
    }
    last_report_ms_ = now_ms;

    std::ostringstream oss;
    oss << "event=gateway_metrics_snapshot trace_id=system"
        << " ws_conn_current=" << ws_conn_current_
        << " ws_conn_total=" << ws_conn_total_
        << " ws_in_total=" << ws_in_total_
        << " ws_out_total=" << ws_out_total_
        << " rate_limited_total=" << rate_limited_total_;

    if (!upstream_rtt_ms_.empty()) {
        oss << " upstream_rtt_p50=" << Percentile(upstream_rtt_ms_, 50)
            << " upstream_rtt_p95=" << Percentile(upstream_rtt_ms_, 95)
            << " upstream_rtt_p99=" << Percentile(upstream_rtt_ms_, 99);
    } else {
        oss << " upstream_rtt_p50=0 upstream_rtt_p95=0 upstream_rtt_p99=0";
    }

    const double success_rate = refresh_total_ == 0
                                    ? 1.0
                                    : static_cast<double>(refresh_success_) / static_cast<double>(refresh_total_);
    oss << " reconnect_refresh_success_rate=" << success_rate;

    for (const auto& item : http_req_total_by_route_) {
        const uint64_t fail = http_req_fail_by_route_[item.first];
        oss << " http_req_total_by_route." << item.first << "=" << item.second
            << " http_req_fail_by_route." << item.first << "=" << fail;
    }
    return oss.str();
}

int64_t GatewayObservabilityService::Percentile(const std::deque<int64_t>& values, int percentile) {
    if (values.empty()) {
        return 0;
    }
    std::vector<int64_t> sorted(values.begin(), values.end());
    std::sort(sorted.begin(), sorted.end());
    size_t pos = (static_cast<size_t>(percentile) * (sorted.size() - 1)) / 100;
    if (pos >= sorted.size()) {
        pos = sorted.size() - 1;
    }
    return sorted[pos];
}

}  // namespace ddz

#include "service/observability/observability_service.h"

#include <algorithm>
#include <sstream>

namespace ddz {
namespace {

thread_local std::string g_trace_id;

}  // namespace

void ObservabilityService::Configure(bool enable_structured_log, int64_t metrics_report_interval_ms) {
    std::lock_guard<std::mutex> lock(mu_);
    enable_structured_log_ = enable_structured_log;
    if (metrics_report_interval_ms > 0) {
        metrics_report_interval_ms_ = metrics_report_interval_ms;
    }
}

bool ObservabilityService::structured_log_enabled() const {
    std::lock_guard<std::mutex> lock(mu_);
    return enable_structured_log_;
}

int64_t ObservabilityService::metrics_report_interval_ms() const {
    std::lock_guard<std::mutex> lock(mu_);
    return metrics_report_interval_ms_;
}

void ObservabilityService::SetCurrentTraceId(const std::string& trace_id) {
    g_trace_id = trace_id;
}

std::string ObservabilityService::CurrentTraceId() {
    return g_trace_id;
}

void ObservabilityService::Record(const std::string& event, ErrorCode code, int64_t latency_ms) {
    std::lock_guard<std::mutex> lock(mu_);
    auto& s = stats_[event];
    s.total += 1;
    if (code == ErrorCode::OK) {
        s.success += 1;
    } else {
        s.fail += 1;
    }
    if (latency_ms >= 0) {
        s.latencies_ms.push_back(latency_ms);
        if (s.latencies_ms.size() > 20000) {
            s.latencies_ms.erase(s.latencies_ms.begin(), s.latencies_ms.begin() + 10000);
        }
    }
}

std::optional<std::string> ObservabilityService::TryBuildMetricsSnapshot(int64_t now_ms) {
    std::lock_guard<std::mutex> lock(mu_);
    if (last_report_ms_ != 0 && now_ms - last_report_ms_ < metrics_report_interval_ms_) {
        return std::nullopt;
    }
    last_report_ms_ = now_ms;

    std::ostringstream oss;
    oss << "event=metrics_snapshot trace_id=system";
    for (const auto& kv : stats_) {
        const auto& name = kv.first;
        const auto& s = kv.second;
        if (s.total == 0) {
            continue;
        }
        oss << " " << name
            << ".total=" << s.total
            << " " << name << ".success=" << s.success
            << " " << name << ".fail=" << s.fail;
        if (!s.latencies_ms.empty()) {
            oss << " " << name << ".p50_ms=" << Percentile(s.latencies_ms, 50)
                << " " << name << ".p95_ms=" << Percentile(s.latencies_ms, 95)
                << " " << name << ".p99_ms=" << Percentile(s.latencies_ms, 99);
        }
    }
    return oss.str();
}

int64_t ObservabilityService::Percentile(const std::vector<int64_t>& values, int percentile) {
    if (values.empty()) {
        return 0;
    }
    std::vector<int64_t> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    size_t pos = (static_cast<size_t>(percentile) * (sorted.size() - 1)) / 100;
    if (pos >= sorted.size()) {
        pos = sorted.size() - 1;
    }
    return sorted[pos];
}

}  // namespace ddz


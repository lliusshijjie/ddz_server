#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <cstdint>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "gateway/gateway_config.h"
#include "gateway/gateway_observability_service.h"
#include "service/login/auth_token_service.h"

namespace ddz {

class GatewayServer {
public:
    GatewayServer() = default;
    ~GatewayServer();

    bool Start(const std::string& config_path, std::string* err);
    void Stop();

private:
    void AcceptLoop();
    void MetricsLoop();

private:
    GatewayConfig config_;
    AuthTokenService auth_token_service_;
    GatewayObservabilityService observability_;
    std::atomic<bool> running_{false};
    std::thread accept_thread_;
    std::thread metrics_thread_;
    std::vector<std::thread> session_threads_;
    std::mutex session_mu_;
    std::mutex security_mu_;
    std::unordered_map<std::string, int32_t> ws_conn_by_ip_;
    std::unordered_map<std::string, std::deque<int64_t>> http_rate_by_ip_;
    std::unordered_map<std::string, int64_t> banned_until_ms_by_ip_;
};

}  // namespace ddz

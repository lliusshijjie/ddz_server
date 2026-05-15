#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ddz {

struct GatewaySecurityConfig {
    std::vector<std::string> allowed_origins;
    int32_t max_ws_connections_per_ip = 64;
    int32_t max_ws_messages_per_sec = 60;
    int32_t max_http_qps_per_ip = 30;
    int32_t ban_seconds_on_abuse = 30;
    bool trust_x_forwarded_for = false;
};

struct GatewayObservabilityConfig {
    bool enable_structured_log = true;
    int64_t metrics_report_interval_ms = 30000;
};

struct GatewayConfig {
    std::string listen_host = "0.0.0.0";
    uint16_t listen_port = 9010;
    std::string upstream_host = "127.0.0.1";
    uint16_t upstream_port = 9000;
    uint32_t max_packet_size = 65536;
    int64_t heartbeat_timeout_ms = 30000;
    int64_t reconnect_backoff_ms = 1000;
    std::string dev_key = "dev_gateway_key";
    std::string token_secret = "dev_change_me";
    int64_t token_ttl_seconds = 604800;
    std::string log_level = "info";
    std::string log_dir = "./logs";
    GatewaySecurityConfig security;
    GatewayObservabilityConfig observability;
};

class GatewayConfigLoader {
public:
    static bool LoadFromFile(const std::string& path, GatewayConfig& out, std::string* err);
};

}  // namespace ddz

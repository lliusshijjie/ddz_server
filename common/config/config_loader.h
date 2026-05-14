#pragma once

#include <cstdint>
#include <string>

namespace ddz {

struct ServerConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 9000;
    int io_threads = 2;
    uint32_t max_packet_size = 65536;
    int64_t heartbeat_timeout_ms = 30000;
    int64_t match_timeout_ms = 30000;
};

struct LogConfig {
    std::string level = "info";
    std::string dir = "./logs";
};

struct MysqlConfig {
    bool enabled = false;
    std::string host = "127.0.0.1";
    uint16_t port = 3306;
    std::string user = "ddz";
    std::string password = "ddz_pass";
    std::string database = "ddz";
    int pool_size = 4;
};

struct RedisConfig {
    bool enabled = false;
    std::string host = "127.0.0.1";
    uint16_t port = 6379;
    int db = 0;
    std::string password;
};

struct AuthConfig {
    std::string token_secret = "dev_change_me";
    int64_t token_ttl_seconds = 604800;
};

struct ObservabilityConfig {
    bool enable_structured_log = true;
    int64_t metrics_report_interval_ms = 30000;
};

struct PerfConfig {
    bool soft_threshold_enabled = true;
};

struct AppConfig {
    ServerConfig server;
    LogConfig log;
    MysqlConfig mysql;
    RedisConfig redis;
    AuthConfig auth;
    ObservabilityConfig observability;
    PerfConfig perf;
};

class ConfigLoader {
public:
    static bool LoadFromFile(const std::string& path, AppConfig& out, std::string* err);
};

}  // namespace ddz

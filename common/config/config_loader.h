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

struct AppConfig {
    ServerConfig server;
    LogConfig log;
    MysqlConfig mysql;
    RedisConfig redis;
};

class ConfigLoader {
public:
    static bool LoadFromFile(const std::string& path, AppConfig& out, std::string* err);
};

}  // namespace ddz

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

struct AppConfig {
    ServerConfig server;
    LogConfig log;
};

class ConfigLoader {
public:
    static bool LoadFromFile(const std::string& path, AppConfig& out, std::string* err);
};

}  // namespace ddz


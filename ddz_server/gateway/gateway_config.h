#pragma once

#include <cstdint>
#include <string>

namespace ddz {

struct GatewayConfig {
    std::string listen_host = "0.0.0.0";
    uint16_t listen_port = 9010;
    std::string upstream_host = "127.0.0.1";
    uint16_t upstream_port = 9000;
    uint32_t max_packet_size = 65536;
    std::string dev_key = "dev_gateway_key";
    std::string token_secret = "dev_change_me";
    int64_t token_ttl_seconds = 604800;
    std::string log_level = "info";
    std::string log_dir = "./logs";
};

class GatewayConfigLoader {
public:
    static bool LoadFromFile(const std::string& path, GatewayConfig& out, std::string* err);
};

}  // namespace ddz

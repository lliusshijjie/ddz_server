#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

#include "gateway/gateway_config.h"

int main() {
    const std::string path = "tmp_gateway_config_test.yaml";
    {
        std::ofstream out(path);
        out << "gateway:\n"
            << "  listen_host: 127.0.0.1\n"
            << "  listen_port: 9011\n"
            << "  upstream_host: 127.0.0.1\n"
            << "  upstream_port: 9000\n"
            << "  max_packet_size: 65536\n"
            << "  heartbeat_timeout_ms: 27000\n"
            << "  reconnect_backoff_ms: 1200\n"
            << "  dev_key: test_key\n"
            << "auth:\n"
            << "  token_secret: sec\n"
            << "  token_ttl_seconds: 60\n"
            << "log:\n"
            << "  level: info\n"
            << "  dir: ./logs\n"
            << "security:\n"
            << "  allowed_origins:\n"
            << "    - \"http://localhost:5173\"\n"
            << "    - \"http://localhost:5173/\"\n"
            << "    - \"https://example.com\"\n"
            << "  max_ws_connections_per_ip: 8\n"
            << "  max_ws_messages_per_sec: 10\n"
            << "  max_http_qps_per_ip: 5\n"
            << "  ban_seconds_on_abuse: 20\n"
            << "  trust_x_forwarded_for: true\n"
            << "observability:\n"
            << "  enable_structured_log: false\n"
            << "  metrics_report_interval_ms: 12345\n";
    }

    ddz::GatewayConfig cfg;
    std::string err;
    const bool ok = ddz::GatewayConfigLoader::LoadFromFile(path, cfg, &err);
    assert(ok);
    assert(err.empty());
    assert(cfg.listen_port == 9011);
    assert(cfg.heartbeat_timeout_ms == 27000);
    assert(cfg.reconnect_backoff_ms == 1200);
    assert(cfg.security.allowed_origins.size() == 2);
    assert(cfg.security.allowed_origins[0] == "http://localhost:5173");
    assert(cfg.security.allowed_origins[1] == "https://example.com");
    assert(cfg.security.max_ws_connections_per_ip == 8);
    assert(cfg.security.max_ws_messages_per_sec == 10);
    assert(cfg.security.max_http_qps_per_ip == 5);
    assert(cfg.security.ban_seconds_on_abuse == 20);
    assert(cfg.security.trust_x_forwarded_for == true);
    assert(cfg.observability.enable_structured_log == false);
    assert(cfg.observability.metrics_report_interval_ms == 12345);

    std::remove(path.c_str());
    std::cout << "test_p0_gateway_config_loader passed" << std::endl;
    return 0;
}

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

#include "common/config/config_loader.h"

int main() {
    const std::string path = "test_p0_server.yaml";
    {
        std::ofstream out(path, std::ios::binary);
        assert(out.is_open());
        out
            << "server:\n"
            << "  host: 127.0.0.1\n"
            << "  port: 9010\n"
            << "  io_threads: 4\n"
            << "  max_packet_size: 65536\n"
            << "  heartbeat_timeout_ms: 31000\n"
            << "  match_timeout_ms: 32000\n"
            << "log:\n"
            << "  level: debug\n"
            << "  dir: ./logs_test\n"
            << "auth:\n"
            << "  token_secret: \"unit_secret\"\n"
            << "  token_ttl_seconds: 12345\n"
            << "observability:\n"
            << "  enable_structured_log: false\n"
            << "  metrics_report_interval_ms: 15000\n"
            << "perf:\n"
            << "  soft_threshold_enabled: false\n";
    }

    ddz::AppConfig cfg;
    std::string err;
    const bool ok = ddz::ConfigLoader::LoadFromFile(path, cfg, &err);
    std::remove(path.c_str());
    assert(ok);
    assert(err.empty());

    assert(cfg.server.host == "127.0.0.1");
    assert(cfg.server.port == 9010);
    assert(cfg.server.io_threads == 4);
    assert(cfg.server.max_packet_size == 65536);
    assert(cfg.server.heartbeat_timeout_ms == 31000);
    assert(cfg.server.match_timeout_ms == 32000);
    assert(cfg.log.level == "debug");
    assert(cfg.log.dir == "./logs_test");
    assert(cfg.auth.token_secret == "unit_secret");
    assert(cfg.auth.token_ttl_seconds == 12345);
    assert(cfg.observability.enable_structured_log == false);
    assert(cfg.observability.metrics_report_interval_ms == 15000);
    assert(cfg.perf.soft_threshold_enabled == false);

    std::cout << "test_p0_config_loader passed" << std::endl;
    return 0;
}

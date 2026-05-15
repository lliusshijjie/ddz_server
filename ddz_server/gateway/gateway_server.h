#pragma once

#include <atomic>
#include <mutex>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "gateway/gateway_config.h"
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

private:
    GatewayConfig config_;
    AuthTokenService auth_token_service_;
    std::atomic<bool> running_{false};
    std::thread accept_thread_;
    std::vector<std::thread> session_threads_;
    std::mutex session_mu_;
};

}  // namespace ddz

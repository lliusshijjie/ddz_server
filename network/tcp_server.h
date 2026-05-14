#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "network/tcp_connection.h"

namespace ddz {

class TcpServer {
public:
    using MessageCallback = std::function<void(int64_t connection_id, const Packet&)>;
    using CloseCallback = std::function<void(int64_t connection_id)>;

    TcpServer(std::string host, uint16_t port, uint32_t max_packet_size);
    ~TcpServer();

    bool Start(std::string* err);
    void Stop();

    void SetMessageCallback(MessageCallback cb) { on_message_ = std::move(cb); }
    void SetCloseCallback(CloseCallback cb) { on_close_ = std::move(cb); }

    bool SendPacket(int64_t connection_id, const Packet& packet);
    void CloseConnection(int64_t connection_id);
    void CloseIdleConnections(int64_t heartbeat_timeout_ms);

private:
    void AcceptLoop();
    void RemoveConnection(int64_t connection_id, bool stop_connection = true);
    void CloseSocketInternal();

private:
    std::string host_;
    uint16_t port_;
    uint32_t max_packet_size_;

    SocketHandle listen_socket_ = kInvalidSocket;
    std::atomic<bool> running_{false};
    std::thread accept_thread_;
    std::atomic<int64_t> next_connection_id_{1};

    std::mutex mu_;
    std::unordered_map<int64_t, std::shared_ptr<TcpConnection>> connections_;

    MessageCallback on_message_;
    CloseCallback on_close_;
};

}  // namespace ddz

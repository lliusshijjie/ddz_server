#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "network/tcp_connection.h"

namespace ddz {

class TcpServer {
public:
    using MessageCallback = std::function<void(int64_t connection_id, const Packet&)>;
    using CloseCallback = std::function<void(int64_t connection_id)>;

    TcpServer(std::string host, uint16_t port, uint32_t max_packet_size, int io_threads);
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
    void WorkerLoop(size_t worker_idx);
    void RemoveConnection(int64_t connection_id, bool stop_connection = true);
    void CloseSocketInternal();
    void NotifyConnectionClosed(int64_t connection_id);

private:
    std::string host_;
    uint16_t port_;
    uint32_t max_packet_size_;
    int io_threads_ = 1;

    SocketHandle listen_socket_ = kInvalidSocket;
    std::atomic<bool> running_{false};
    std::thread accept_thread_;
    std::vector<std::thread> worker_threads_;
    std::atomic<int64_t> next_connection_id_{1};
    std::atomic<size_t> next_worker_idx_{0};

    std::mutex mu_;
    std::unordered_map<int64_t, std::shared_ptr<TcpConnection>> connections_;
    std::unordered_map<int64_t, size_t> owner_worker_idx_by_conn_;
    std::vector<std::set<int64_t>> worker_connections_;
    std::vector<std::mutex> worker_mu_;

    MessageCallback on_message_;
    CloseCallback on_close_;
};

}  // namespace ddz

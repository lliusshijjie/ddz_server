#include "network/tcp_server.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#include "common/util/time_util.h"
#include "log/logger.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace ddz {
namespace {

void ShutdownSocket(SocketHandle socket_handle) {
#ifdef _WIN32
    shutdown(socket_handle, SD_BOTH);
#else
    shutdown(socket_handle, SHUT_RDWR);
#endif
}

void CloseSocket(SocketHandle socket_handle) {
#ifdef _WIN32
    closesocket(socket_handle);
#else
    close(socket_handle);
#endif
}

}  // namespace

TcpServer::TcpServer(std::string host, uint16_t port, uint32_t max_packet_size, int io_threads)
    : host_(std::move(host)),
      port_(port),
      max_packet_size_(max_packet_size),
      io_threads_((std::max)(1, io_threads)) {}

TcpServer::~TcpServer() {
    Stop();
}

bool TcpServer::Start(std::string* err) {
    if (running_.exchange(true)) {
        return true;
    }
    worker_connections_.assign(static_cast<size_t>(io_threads_), {});
    worker_mu_ = std::vector<std::mutex>(static_cast<size_t>(io_threads_));
    worker_threads_.clear();

#ifdef _WIN32
    static std::once_flag wsa_once;
    std::call_once(wsa_once, []() {
        WSADATA wsa_data{};
        WSAStartup(MAKEWORD(2, 2), &wsa_data);
    });
#endif

    listen_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket_ == kInvalidSocket) {
        if (err != nullptr) *err = "create listen socket failed";
        running_.store(false);
        return false;
    }

    int reuse = 1;
    setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    if (host_ == "0.0.0.0" || host_.empty()) {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) != 1) {
            if (err != nullptr) *err = "invalid host: " + host_;
            running_.store(false);
            CloseSocketInternal();
            return false;
        }
    }

    if (bind(listen_socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        if (err != nullptr) *err = "bind failed";
        running_.store(false);
        CloseSocketInternal();
        return false;
    }

    if (listen(listen_socket_, SOMAXCONN) != 0) {
        if (err != nullptr) *err = "listen failed";
        running_.store(false);
        CloseSocketInternal();
        return false;
    }

    for (int i = 0; i < io_threads_; ++i) {
        worker_threads_.emplace_back(&TcpServer::WorkerLoop, this, static_cast<size_t>(i));
    }
    accept_thread_ = std::thread(&TcpServer::AcceptLoop, this);
    return true;
}

void TcpServer::Stop() {
    const bool was_running = running_.exchange(false);
    if (was_running && listen_socket_ != kInvalidSocket) {
        ShutdownSocket(listen_socket_);
        CloseSocketInternal();
    }

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    for (auto& worker : worker_threads_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    worker_threads_.clear();

    std::vector<std::shared_ptr<TcpConnection>> all_conn;
    {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& kv : connections_) {
            all_conn.push_back(kv.second);
        }
        connections_.clear();
        owner_worker_idx_by_conn_.clear();
    }
    for (size_t i = 0; i < worker_connections_.size(); ++i) {
        std::lock_guard<std::mutex> wlock(worker_mu_[i]);
        worker_connections_[i].clear();
    }
    for (auto& conn : all_conn) {
        conn->Stop();
    }

    CloseSocketInternal();
}

bool TcpServer::SendPacket(int64_t connection_id, const Packet& packet) {
    std::shared_ptr<TcpConnection> conn;
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = connections_.find(connection_id);
        if (it == connections_.end()) {
            return false;
        }
        conn = it->second;
    }
    return conn->SendPacket(packet);
}

void TcpServer::CloseConnection(int64_t connection_id) {
    RemoveConnection(connection_id);
}

void TcpServer::CloseIdleConnections(int64_t heartbeat_timeout_ms) {
    const int64_t now_ms = NowMs();
    std::vector<int64_t> to_close;

    {
        std::lock_guard<std::mutex> lock(mu_);
        for (const auto& kv : connections_) {
            const int64_t diff = now_ms - kv.second->LastHeartbeatMs();
            if (diff > heartbeat_timeout_ms) {
                to_close.push_back(kv.first);
            }
        }
    }

    for (const auto& connection_id : to_close) {
        Logger::Instance().Info("close idle conn_id=" + std::to_string(connection_id));
        RemoveConnection(connection_id);
    }
}

void TcpServer::AcceptLoop() {
    while (running_.load()) {
        sockaddr_in client_addr{};
#ifdef _WIN32
        int addr_len = sizeof(client_addr);
#else
        socklen_t addr_len = sizeof(client_addr);
#endif
        SocketHandle client_socket = accept(listen_socket_, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
        if (client_socket == kInvalidSocket) {
            if (running_.load()) {
                Logger::Instance().Warn("accept failed");
            }
            continue;
        }

        const int64_t connection_id = next_connection_id_.fetch_add(1);
        auto conn = std::make_shared<TcpConnection>(connection_id, client_socket, max_packet_size_);
        conn->Start();
        if (!conn->running()) {
            conn->Stop();
            Logger::Instance().Warn("connection start failed conn_id=" + std::to_string(connection_id));
            continue;
        }

        const size_t worker_idx =
            next_worker_idx_.fetch_add(1) % static_cast<size_t>((std::max)(1, io_threads_));

        {
            std::lock_guard<std::mutex> lock(mu_);
            connections_[connection_id] = conn;
            owner_worker_idx_by_conn_[connection_id] = worker_idx;
        }
        {
            std::lock_guard<std::mutex> wlock(worker_mu_[worker_idx]);
            worker_connections_[worker_idx].insert(connection_id);
        }
        Logger::Instance().Info("connection accepted conn_id=" + std::to_string(connection_id));
    }
}

void TcpServer::WorkerLoop(size_t worker_idx) {
    while (running_.load()) {
        std::vector<int64_t> conn_ids;
        {
            std::lock_guard<std::mutex> lock(worker_mu_[worker_idx]);
            conn_ids.reserve(worker_connections_[worker_idx].size());
            for (const auto conn_id : worker_connections_[worker_idx]) {
                conn_ids.push_back(conn_id);
            }
        }

        if (conn_ids.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        fd_set readfds;
        FD_ZERO(&readfds);
#ifndef _WIN32
        int max_fd = -1;
#endif

        std::vector<std::shared_ptr<TcpConnection>> conns;
        conns.reserve(conn_ids.size());
        {
            std::lock_guard<std::mutex> lock(mu_);
            for (const auto conn_id : conn_ids) {
                auto it = connections_.find(conn_id);
                if (it == connections_.end()) {
                    continue;
                }
                conns.push_back(it->second);
                FD_SET(it->second->socket_handle(), &readfds);
#ifndef _WIN32
                if (it->second->socket_handle() > max_fd) {
                    max_fd = it->second->socket_handle();
                }
#endif
            }
        }
        if (conns.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 50000;  // 50ms
#ifdef _WIN32
        const int ready = select(0, &readfds, nullptr, nullptr, &tv);
#else
        const int ready = select(max_fd + 1, &readfds, nullptr, nullptr, &tv);
#endif
        if (ready <= 0) {
            continue;
        }

        for (const auto& conn : conns) {
            if (!FD_ISSET(conn->socket_handle(), &readfds)) {
                continue;
            }
            std::vector<Packet> packets;
            bool peer_closed = false;
            std::string err;
            if (!conn->ReadAvailablePackets(&packets, &peer_closed, &err)) {
                Logger::Instance().Warn(
                    "read failed conn_id=" + std::to_string(conn->ConnectionId()) + " err=" + err);
                NotifyConnectionClosed(conn->ConnectionId());
                continue;
            }
            if (peer_closed) {
                NotifyConnectionClosed(conn->ConnectionId());
                continue;
            }
            for (const auto& packet : packets) {
                if (on_message_ != nullptr) {
                    on_message_(conn->ConnectionId(), packet);
                }
            }
        }
    }
}

void TcpServer::RemoveConnection(int64_t connection_id, bool stop_connection) {
    std::shared_ptr<TcpConnection> conn;
    size_t worker_idx = 0;
    bool has_worker = false;
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = connections_.find(connection_id);
        if (it == connections_.end()) {
            return;
        }
        conn = it->second;
        connections_.erase(it);

        auto wit = owner_worker_idx_by_conn_.find(connection_id);
        if (wit != owner_worker_idx_by_conn_.end()) {
            has_worker = true;
            worker_idx = wit->second;
            owner_worker_idx_by_conn_.erase(wit);
        }
    }
    if (has_worker && worker_idx < worker_connections_.size()) {
        std::lock_guard<std::mutex> wlock(worker_mu_[worker_idx]);
        worker_connections_[worker_idx].erase(connection_id);
    }
    if (stop_connection) {
        conn->Stop();
    }
    Logger::Instance().Info("connection removed conn_id=" + std::to_string(connection_id));
}

void TcpServer::NotifyConnectionClosed(int64_t connection_id) {
    RemoveConnection(connection_id, true);
    if (on_close_ != nullptr) {
        on_close_(connection_id);
    }
}

void TcpServer::CloseSocketInternal() {
    if (listen_socket_ != kInvalidSocket) {
        CloseSocket(listen_socket_);
        listen_socket_ = kInvalidSocket;
    }
}

}  // namespace ddz

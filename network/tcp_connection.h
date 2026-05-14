#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "network/packet.h"

#ifdef _WIN32
#include <winsock2.h>
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

namespace ddz {

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    using MessageCallback = std::function<void(int64_t connection_id, const Packet&)>;
    using CloseCallback = std::function<void(int64_t connection_id)>;

    TcpConnection(int64_t connection_id,
                  SocketHandle socket_handle,
                  uint32_t max_packet_size,
                  MessageCallback on_message,
                  CloseCallback on_close);

    ~TcpConnection();

    void Start();
    void Stop();
    bool SendPacket(const Packet& packet);

    int64_t ConnectionId() const { return connection_id_; }
    int64_t LastHeartbeatMs() const { return last_heartbeat_ms_.load(); }
    void TouchHeartbeat();

private:
    void ReadLoop();
    void CloseSocketInternal();

private:
    int64_t connection_id_;
    SocketHandle socket_handle_;
    uint32_t max_packet_size_;
    MessageCallback on_message_;
    CloseCallback on_close_;

    std::atomic<bool> running_{false};
    std::atomic<int64_t> last_heartbeat_ms_{0};
    std::thread read_thread_;
    std::mutex send_mu_;
    std::vector<uint8_t> read_buffer_;
};

}  // namespace ddz

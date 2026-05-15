#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
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
    TcpConnection(int64_t connection_id,
                  SocketHandle socket_handle,
                  uint32_t max_packet_size);

    ~TcpConnection();

    void Start();
    void Stop();
    bool SendPacket(const Packet& packet);
    bool ReadAvailablePackets(std::vector<Packet>* out_packets, bool* peer_closed, std::string* err);

    int64_t ConnectionId() const { return connection_id_; }
    int64_t LastHeartbeatMs() const { return last_heartbeat_ms_.load(); }
    SocketHandle socket_handle() const { return socket_handle_; }
    bool running() const { return running_.load(); }
    void TouchHeartbeat();

private:
    bool SetNonBlocking(std::string* err);
    void CloseSocketInternal();

private:
    int64_t connection_id_;
    SocketHandle socket_handle_;
    uint32_t max_packet_size_;

    std::atomic<bool> running_{false};
    std::atomic<int64_t> last_heartbeat_ms_{0};
    std::mutex send_mu_;
    std::mutex read_mu_;
    std::vector<uint8_t> read_buffer_;
};

}  // namespace ddz

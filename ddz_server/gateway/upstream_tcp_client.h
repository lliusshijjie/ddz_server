#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

#include "network/packet.h"

namespace ddz {

class UpstreamTcpClient {
public:
    using PacketCallback = std::function<void(const Packet&)>;

    UpstreamTcpClient() = default;
    ~UpstreamTcpClient();

    bool Connect(const std::string& host,
                 uint16_t port,
                 uint32_t max_packet_size,
                 PacketCallback on_packet,
                 std::string* err);
    bool SendPacket(const Packet& packet, std::string* err);
    void Close();
    bool running() const { return running_.load(); }

private:
    void ReadLoop();

private:
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::socket socket_{io_context_};
    std::atomic<bool> running_{false};
    std::thread read_thread_;
    std::mutex send_mu_;
    uint32_t max_packet_size_ = 65536;
    PacketCallback on_packet_;
    std::vector<uint8_t> read_buffer_;
};

}  // namespace ddz

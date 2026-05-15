#include "gateway/upstream_tcp_client.h"

#include <array>

#include "network/packet_codec.h"

namespace ddz {

UpstreamTcpClient::~UpstreamTcpClient() {
    Close();
}

bool UpstreamTcpClient::Connect(const std::string& host,
                                uint16_t port,
                                uint32_t max_packet_size,
                                PacketCallback on_packet,
                                std::string* err) {
    if (running_.load()) {
        return true;
    }
    max_packet_size_ = max_packet_size;
    on_packet_ = std::move(on_packet);
    read_buffer_.clear();

    boost::system::error_code ec;
    boost::asio::ip::tcp::resolver resolver(io_context_);
    const auto endpoints = resolver.resolve(host, std::to_string(port), ec);
    if (ec) {
        if (err != nullptr) *err = "resolve upstream failed: " + ec.message();
        return false;
    }
    boost::asio::connect(socket_, endpoints, ec);
    if (ec) {
        if (err != nullptr) *err = "connect upstream failed: " + ec.message();
        return false;
    }
    running_.store(true);
    read_thread_ = std::thread(&UpstreamTcpClient::ReadLoop, this);
    return true;
}

bool UpstreamTcpClient::SendPacket(const Packet& packet, std::string* err) {
    if (!running_.load()) {
        if (err != nullptr) *err = "upstream not running";
        return false;
    }
    std::vector<uint8_t> encoded;
    std::string encode_err;
    if (!PacketCodec::Encode(packet, max_packet_size_, encoded, &encode_err)) {
        if (err != nullptr) *err = encode_err;
        return false;
    }
    std::lock_guard<std::mutex> lock(send_mu_);
    boost::system::error_code ec;
    boost::asio::write(socket_, boost::asio::buffer(encoded), ec);
    if (ec) {
        if (err != nullptr) *err = "write upstream failed: " + ec.message();
        return false;
    }
    return true;
}

void UpstreamTcpClient::Close() {
    const bool was_running = running_.exchange(false);
    if (!was_running && !socket_.is_open()) {
        return;
    }
    boost::system::error_code ec;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    socket_.close(ec);
    if (read_thread_.joinable()) {
        read_thread_.join();
    }
}

void UpstreamTcpClient::ReadLoop() {
    std::array<uint8_t, 4096> buf{};
    while (running_.load()) {
        boost::system::error_code ec;
        const size_t n = socket_.read_some(boost::asio::buffer(buf), ec);
        if (ec) {
            break;
        }
        if (n == 0) {
            break;
        }
        read_buffer_.insert(read_buffer_.end(), buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(n));
        std::vector<Packet> packets;
        std::string decode_err;
        if (!PacketCodec::Decode(read_buffer_, max_packet_size_, packets, &decode_err)) {
            break;
        }
        if (on_packet_ != nullptr) {
            for (const auto& packet : packets) {
                on_packet_(packet);
            }
        }
    }
    running_.store(false);
}

}  // namespace ddz

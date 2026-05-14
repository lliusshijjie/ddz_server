#include "network/tcp_connection.h"

#include <chrono>
#include <string>

#include "common/util/time_util.h"
#include "log/logger.h"
#include "network/packet_codec.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#else
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

TcpConnection::TcpConnection(int64_t connection_id,
                             SocketHandle socket_handle,
                             uint32_t max_packet_size,
                             MessageCallback on_message,
                             CloseCallback on_close)
    : connection_id_(connection_id),
      socket_handle_(socket_handle),
      max_packet_size_(max_packet_size),
      on_message_(std::move(on_message)),
      on_close_(std::move(on_close)) {
    TouchHeartbeat();
}

TcpConnection::~TcpConnection() {
    Stop();
}

void TcpConnection::Start() {
    if (running_.exchange(true)) {
        return;
    }
    auto self = shared_from_this();
    read_thread_ = std::thread([self]() {
        self->ReadLoop();
    });
}

void TcpConnection::Stop() {
    const bool was_running = running_.exchange(false);
    if (was_running && socket_handle_ != kInvalidSocket) {
        ShutdownSocket(socket_handle_);
    }

    if (read_thread_.joinable()) {
        if (std::this_thread::get_id() == read_thread_.get_id()) {
            read_thread_.detach();
        } else {
            read_thread_.join();
        }
    }

    CloseSocketInternal();
}

bool TcpConnection::SendPacket(const Packet& packet) {
    std::vector<uint8_t> encoded;
    std::string err;
    if (!PacketCodec::Encode(packet, max_packet_size_, encoded, &err)) {
        Logger::Instance().Warn("encode packet failed: " + err);
        return false;
    }

    std::lock_guard<std::mutex> lock(send_mu_);
    if (!running_.load()) {
        return false;
    }

    size_t sent_total = 0;
    while (sent_total < encoded.size()) {
#ifdef _WIN32
        const int sent = send(socket_handle_,
                              reinterpret_cast<const char*>(  encoded.data() + sent_total),
                              static_cast<int>(encoded.size() - sent_total),
                              0);
#else
        const ssize_t sent = send(socket_handle_,
                                  encoded.data() + sent_total,
                                  encoded.size() - sent_total,
                                  0);
#endif
        if (sent <= 0) {
            Logger::Instance().Warn("send failed conn_id=" + std::to_string(connection_id_));
            return false;
        }
        sent_total += static_cast<size_t>(sent);
    }
    return true;
}

void TcpConnection::TouchHeartbeat() {
    last_heartbeat_ms_.store(NowMs());
}

void TcpConnection::ReadLoop() {
    std::vector<uint8_t> recv_buf(4096);

    while (running_.load()) {
#ifdef _WIN32
        const int read_n = recv(socket_handle_, reinterpret_cast<char*>(recv_buf.data()), static_cast<int>(recv_buf.size()), 0);
#else
        const ssize_t read_n = recv(socket_handle_, recv_buf.data(), recv_buf.size(), 0);
#endif
        if (read_n <= 0) {
            break;
        }

        TouchHeartbeat();
        read_buffer_.insert(read_buffer_.end(), recv_buf.begin(), recv_buf.begin() + read_n);

        std::vector<Packet> packets;
        std::string err;
        if (!PacketCodec::Decode(read_buffer_, max_packet_size_, packets, &err)) {
            Logger::Instance().Warn("decode failed conn_id=" + std::to_string(connection_id_) + " err=" + err);
            break;
        }

        for (const auto& packet : packets) {
            if (on_message_ != nullptr) {
                on_message_(connection_id_, packet);
            }
        }
    }

    running_.store(false);
    CloseSocketInternal();
    if (on_close_ != nullptr) {
        on_close_(connection_id_);
    }
}

void TcpConnection::CloseSocketInternal() {
    if (socket_handle_ != kInvalidSocket) {
        CloseSocket(socket_handle_);
        socket_handle_ = kInvalidSocket;
    }
}

}  // namespace ddz

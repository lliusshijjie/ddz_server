#include "network/tcp_connection.h"

#include <cerrno>
#include <string>

#include "common/util/time_util.h"
#include "log/logger.h"
#include "network/packet_codec.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <fcntl.h>
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

bool IsWouldBlockError() {
#ifdef _WIN32
    const int err = WSAGetLastError();
    return err == WSAEWOULDBLOCK || err == WSAEINTR;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
#endif
}

}  // namespace

TcpConnection::TcpConnection(int64_t connection_id,
                             SocketHandle socket_handle,
                             uint32_t max_packet_size)
    : connection_id_(connection_id),
      socket_handle_(socket_handle),
      max_packet_size_(max_packet_size) {
    TouchHeartbeat();
}

TcpConnection::~TcpConnection() {
    Stop();
}

void TcpConnection::Start() {
    if (running_.load()) {
        return;
    }
    std::string err;
    if (!SetNonBlocking(&err)) {
        Logger::Instance().Warn("set non-blocking failed conn_id=" + std::to_string(connection_id_) + " err=" + err);
        return;
    }
    running_.store(true);
}

void TcpConnection::Stop() {
    const bool was_running = running_.exchange(false);
    if (was_running && socket_handle_ != kInvalidSocket) {
        ShutdownSocket(socket_handle_);
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

bool TcpConnection::ReadAvailablePackets(std::vector<Packet>* out_packets, bool* peer_closed, std::string* err) {
    if (out_packets != nullptr) {
        out_packets->clear();
    }
    if (peer_closed != nullptr) {
        *peer_closed = false;
    }
    if (!running_.load()) {
        if (err != nullptr) {
            *err = "connection not running";
        }
        return false;
    }
    if (out_packets == nullptr || peer_closed == nullptr) {
        if (err != nullptr) {
            *err = "invalid output pointers";
        }
        return false;
    }

    std::lock_guard<std::mutex> lock(read_mu_);
    std::vector<uint8_t> recv_buf(4096);
    bool got_bytes = false;
    while (running_.load()) {
#ifdef _WIN32
        const int read_n = recv(socket_handle_, reinterpret_cast<char*>(recv_buf.data()), static_cast<int>(recv_buf.size()), 0);
#else
        const ssize_t read_n = recv(socket_handle_, recv_buf.data(), recv_buf.size(), 0);
#endif
        if (read_n == 0) {
            *peer_closed = true;
            return true;
        }
        if (read_n < 0) {
            if (IsWouldBlockError()) {
                break;
            }
            if (err != nullptr) {
                *err = "socket recv failed";
            }
            return false;
        }

        if (read_n > 0) {
            got_bytes = true;
            TouchHeartbeat();
            read_buffer_.insert(read_buffer_.end(), recv_buf.begin(), recv_buf.begin() + read_n);
        }
        if (read_n < static_cast<decltype(read_n)>(recv_buf.size())) {
            break;
        }
    }

    if (!got_bytes && read_buffer_.empty()) {
        return true;
    }

    std::string decode_err;
    if (!PacketCodec::Decode(read_buffer_, max_packet_size_, *out_packets, &decode_err)) {
        if (err != nullptr) {
            *err = decode_err;
        }
        return false;
    }
    return true;
}

bool TcpConnection::SetNonBlocking(std::string* err) {
#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(socket_handle_, FIONBIO, &mode) != 0) {
        if (err != nullptr) {
            *err = "ioctlsocket FIONBIO failed";
        }
        return false;
    }
#else
    int flags = fcntl(socket_handle_, F_GETFL, 0);
    if (flags < 0) {
        if (err != nullptr) {
            *err = "fcntl F_GETFL failed";
        }
        return false;
    }
    if (fcntl(socket_handle_, F_SETFL, flags | O_NONBLOCK) < 0) {
        if (err != nullptr) {
            *err = "fcntl F_SETFL O_NONBLOCK failed";
        }
        return false;
    }
#endif
    return true;
}

void TcpConnection::CloseSocketInternal() {
    if (socket_handle_ != kInvalidSocket) {
        CloseSocket(socket_handle_);
        socket_handle_ = kInvalidSocket;
    }
}

}  // namespace ddz

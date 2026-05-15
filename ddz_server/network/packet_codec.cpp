#include "network/packet_codec.h"

#include <cstring>

namespace ddz {
namespace {

void WriteU32BE(uint8_t* dst, uint32_t value) {
    dst[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
    dst[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    dst[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    dst[3] = static_cast<uint8_t>(value & 0xFF);
}

void WriteU64BE(uint8_t* dst, uint64_t value) {
    dst[0] = static_cast<uint8_t>((value >> 56) & 0xFF);
    dst[1] = static_cast<uint8_t>((value >> 48) & 0xFF);
    dst[2] = static_cast<uint8_t>((value >> 40) & 0xFF);
    dst[3] = static_cast<uint8_t>((value >> 32) & 0xFF);
    dst[4] = static_cast<uint8_t>((value >> 24) & 0xFF);
    dst[5] = static_cast<uint8_t>((value >> 16) & 0xFF);
    dst[6] = static_cast<uint8_t>((value >> 8) & 0xFF);
    dst[7] = static_cast<uint8_t>(value & 0xFF);
}

uint32_t ReadU32BE(const uint8_t* src) {
    return (static_cast<uint32_t>(src[0]) << 24) |
           (static_cast<uint32_t>(src[1]) << 16) |
           (static_cast<uint32_t>(src[2]) << 8) |
           static_cast<uint32_t>(src[3]);
}

uint64_t ReadU64BE(const uint8_t* src) {
    return (static_cast<uint64_t>(src[0]) << 56) |
           (static_cast<uint64_t>(src[1]) << 48) |
           (static_cast<uint64_t>(src[2]) << 40) |
           (static_cast<uint64_t>(src[3]) << 32) |
           (static_cast<uint64_t>(src[4]) << 24) |
           (static_cast<uint64_t>(src[5]) << 16) |
           (static_cast<uint64_t>(src[6]) << 8) |
           static_cast<uint64_t>(src[7]);
}

}  // namespace

bool PacketCodec::Encode(const Packet& packet, uint32_t max_packet_size, std::vector<uint8_t>& out, std::string* err) {
    const uint32_t packet_len = static_cast<uint32_t>(kHeaderSize + packet.body.size());
    if (packet_len < kHeaderSize || packet_len > max_packet_size) {
        if (err != nullptr) {
            *err = "packet size out of range";
        }
        return false;
    }

    out.resize(packet_len);
    WriteU32BE(out.data(), packet_len);
    WriteU32BE(out.data() + 4, packet.msg_id);
    WriteU32BE(out.data() + 8, packet.seq_id);
    WriteU64BE(out.data() + 12, static_cast<uint64_t>(packet.player_id));
    if (!packet.body.empty()) {
        std::memcpy(out.data() + kHeaderSize, packet.body.data(), packet.body.size());
    }
    return true;
}

bool PacketCodec::Decode(std::vector<uint8_t>& buffer, uint32_t max_packet_size, std::vector<Packet>& out, std::string* err) {
    size_t offset = 0;
    out.clear();

    while (buffer.size() - offset >= kHeaderSize) {
        const uint8_t* base = buffer.data() + offset;
        const uint32_t packet_len = ReadU32BE(base);
        if (packet_len < kHeaderSize || packet_len > max_packet_size) {
            if (err != nullptr) {
                *err = "invalid packet length";
            }
            return false;
        }

        if (buffer.size() - offset < packet_len) {
            break;
        }

        Packet packet;
        packet.msg_id = ReadU32BE(base + 4);
        packet.seq_id = ReadU32BE(base + 8);
        packet.player_id = static_cast<int64_t>(ReadU64BE(base + 12));
        const size_t body_len = packet_len - kHeaderSize;
        if (body_len > 0) {
            packet.body.assign(reinterpret_cast<const char*>(base + kHeaderSize), body_len);
        }
        out.push_back(std::move(packet));
        offset += packet_len;
    }

    if (offset > 0) {
        buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(offset));
    }
    return true;
}

}  // namespace ddz


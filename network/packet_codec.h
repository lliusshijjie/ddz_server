#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "network/packet.h"

namespace ddz {

class PacketCodec {
public:
    static constexpr uint32_t kHeaderSize = 20;

    static bool Encode(const Packet& packet, uint32_t max_packet_size, std::vector<uint8_t>& out, std::string* err);

    // Decode all complete packets from buffer, and keep remaining bytes in buffer.
    static bool Decode(std::vector<uint8_t>& buffer, uint32_t max_packet_size, std::vector<Packet>& out, std::string* err);
};

}  // namespace ddz


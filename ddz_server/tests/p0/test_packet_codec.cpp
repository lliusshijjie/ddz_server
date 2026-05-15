#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "network/packet_codec.h"

namespace {

void WriteU32BE(uint8_t* dst, uint32_t value) {
    dst[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
    dst[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    dst[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    dst[3] = static_cast<uint8_t>(value & 0xFF);
}

void TestRoundTrip() {
    ddz::Packet in;
    in.msg_id = 1101;
    in.seq_id = 42;
    in.player_id = 10001;
    in.body = "ping";

    std::vector<uint8_t> encoded;
    std::string err;
    const bool ok_encode = ddz::PacketCodec::Encode(in, 65536, encoded, &err);
    assert(ok_encode);

    std::vector<ddz::Packet> out;
    std::vector<uint8_t> buffer = encoded;
    const bool ok_decode = ddz::PacketCodec::Decode(buffer, 65536, out, &err);
    assert(ok_decode);
    assert(buffer.empty());
    assert(out.size() == 1);
    assert(out[0].msg_id == in.msg_id);
    assert(out[0].seq_id == in.seq_id);
    assert(out[0].player_id == in.player_id);
    assert(out[0].body == in.body);
}

void TestStickyAndHalfPacket() {
    ddz::Packet p1;
    p1.msg_id = 1;
    p1.seq_id = 11;
    p1.player_id = 111;
    p1.body = "abc";

    ddz::Packet p2;
    p2.msg_id = 2;
    p2.seq_id = 22;
    p2.player_id = 222;
    p2.body = "defgh";

    std::vector<uint8_t> e1;
    std::vector<uint8_t> e2;
    std::string err;
    assert(ddz::PacketCodec::Encode(p1, 65536, e1, &err));
    assert(ddz::PacketCodec::Encode(p2, 65536, e2, &err));

    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), e1.begin(), e1.end());
    buffer.insert(buffer.end(), e2.begin(), e2.begin() + 4);

    std::vector<ddz::Packet> out;
    assert(ddz::PacketCodec::Decode(buffer, 65536, out, &err));
    assert(out.size() == 1);
    assert(out[0].msg_id == p1.msg_id);
    assert(!buffer.empty());

    buffer.insert(buffer.end(), e2.begin() + 4, e2.end());
    out.clear();
    assert(ddz::PacketCodec::Decode(buffer, 65536, out, &err));
    assert(out.size() == 1);
    assert(out[0].msg_id == p2.msg_id);
    assert(buffer.empty());
}

void TestInvalidLength() {
    std::vector<uint8_t> buffer(ddz::PacketCodec::kHeaderSize, 0);
    WriteU32BE(buffer.data(), 999999);  // > max_packet_size
    std::vector<ddz::Packet> out;
    std::string err;
    const bool ok = ddz::PacketCodec::Decode(buffer, 1024, out, &err);
    assert(!ok);
}

}  // namespace

int main() {
    TestRoundTrip();
    TestStickyAndHalfPacket();
    TestInvalidLength();
    std::cout << "test_p0_packet_codec passed" << std::endl;
    return 0;
}


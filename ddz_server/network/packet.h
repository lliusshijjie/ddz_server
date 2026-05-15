#pragma once

#include <cstdint>
#include <string>

namespace ddz {

struct Packet {
    uint32_t msg_id = 0;
    uint32_t seq_id = 0;
    int64_t player_id = 0;
    std::string body;
};

}  // namespace ddz


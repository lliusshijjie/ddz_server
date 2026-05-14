#pragma once

#include <cstdint>
#include <vector>

namespace ddz {

enum class RoomState {
    Waiting = 0,
    Started = 1,
    Playing = 2,
    Settlement = 3,
    Finished = 4
};

struct Room {
    int64_t room_id = 0;
    int32_t mode = 0;
    RoomState state = RoomState::Waiting;
    std::vector<int64_t> players;
};

struct RoomSnapshot {
    int64_t room_id = 0;
    int32_t mode = 0;
    int32_t room_state = 0;
    std::vector<int64_t> players;
    int64_t current_operator_player_id = 0;
    int32_t remain_seconds = 0;
};

}  // namespace ddz

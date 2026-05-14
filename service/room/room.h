#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ddz {

enum class RoomState {
    WaitStart = 0,
    CallScore = 1,
    RobLandlord = 2,
    Playing = 3,
    Settling = 4,
    Finished = 5
};

struct Room {
    int64_t room_id = 0;
    int32_t mode = 0;
    RoomState state = RoomState::WaitStart;
    std::vector<int64_t> players;
    int64_t current_operator_player_id = 0;
    int32_t state_action_count = 0;
    bool dealt = false;
    std::unordered_map<int64_t, std::vector<int32_t>> hands;
    std::vector<int32_t> landlord_bottom_cards;
    int64_t landlord_player_id = 0;
    int32_t highest_call_score = 0;
    int64_t highest_call_player_id = 0;
    int32_t base_coin = 1;
    int64_t last_play_player_id = 0;
    std::vector<int32_t> last_play_cards;
    int32_t consecutive_pass_count = 0;
};

struct RoomSnapshot {
    int64_t room_id = 0;
    int32_t mode = 0;
    int32_t room_state = 0;
    std::vector<int64_t> players;
    int64_t current_operator_player_id = 0;
    int32_t remain_seconds = 0;
    int64_t landlord_player_id = 0;
    int32_t base_coin = 0;
};

enum class PlayerActionType : int32_t {
    CallScore = 1,
    RobLandlord = 2,
    PlayCards = 3,
    Pass = 4
};

struct PlayerActionRequest {
    PlayerActionType action_type = PlayerActionType::CallScore;
    int32_t call_score = -1;
    int32_t rob = -1;
    std::vector<int32_t> cards;
};

struct PlayerActionResult {
    RoomSnapshot snapshot;
    bool game_over = false;
    int64_t winner_player_id = 0;
    int64_t base_coin = 0;
};

}  // namespace ddz

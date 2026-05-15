#pragma once

#include <cstdint>
#include <vector>

namespace ddz {

struct GameRecord {
    int64_t record_id = 0;
    int64_t room_id = 0;
    int32_t game_mode = 0;
    int64_t winner_player_id = 0;
    int64_t started_at_ms = 0;
    int64_t ended_at_ms = 0;
    std::vector<int64_t> players;
};

struct CoinLog {
    int64_t id = 0;
    int64_t player_id = 0;
    int64_t room_id = 0;
    int64_t change_amount = 0;
    int64_t before_coin = 0;
    int64_t after_coin = 0;
    int64_t created_at_ms = 0;
};

}  // namespace ddz

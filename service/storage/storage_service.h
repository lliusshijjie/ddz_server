#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
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

class StorageService {
public:
    int64_t SaveGameRecord(const GameRecord& record);
    int64_t SaveCoinLog(const CoinLog& log);
    void SavePlayerCoin(int64_t player_id, int64_t coin);

    std::optional<GameRecord> GetGameRecord(int64_t record_id) const;
    std::vector<CoinLog> GetCoinLogsByRoomId(int64_t room_id) const;
    std::optional<int64_t> GetPlayerCoin(int64_t player_id) const;

private:
    mutable std::mutex mu_;
    int64_t next_record_id_ = 1;
    int64_t next_coin_log_id_ = 1;
    std::unordered_map<int64_t, GameRecord> game_records_;
    std::vector<CoinLog> coin_logs_;
    std::unordered_map<int64_t, int64_t> player_coins_;
};

}  // namespace ddz


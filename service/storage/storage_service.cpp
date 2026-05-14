#include "service/storage/storage_service.h"

namespace ddz {

int64_t StorageService::SaveGameRecord(const GameRecord& record) {
    std::lock_guard<std::mutex> lock(mu_);
    GameRecord copy = record;
    copy.record_id = next_record_id_++;
    game_records_[copy.record_id] = copy;
    return copy.record_id;
}

int64_t StorageService::SaveCoinLog(const CoinLog& log) {
    std::lock_guard<std::mutex> lock(mu_);
    CoinLog copy = log;
    copy.id = next_coin_log_id_++;
    coin_logs_.push_back(copy);
    return copy.id;
}

void StorageService::SavePlayerCoin(int64_t player_id, int64_t coin) {
    std::lock_guard<std::mutex> lock(mu_);
    player_coins_[player_id] = coin;
}

std::optional<GameRecord> StorageService::GetGameRecord(int64_t record_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = game_records_.find(record_id);
    if (it == game_records_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<CoinLog> StorageService::GetCoinLogsByRoomId(int64_t room_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<CoinLog> out;
    for (const auto& log : coin_logs_) {
        if (log.room_id == room_id) {
            out.push_back(log);
        }
    }
    return out;
}

std::optional<int64_t> StorageService::GetPlayerCoin(int64_t player_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = player_coins_.find(player_id);
    if (it == player_coins_.end()) {
        return std::nullopt;
    }
    return it->second;
}

}  // namespace ddz


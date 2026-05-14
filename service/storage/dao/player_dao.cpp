#include "service/storage/dao/player_dao.h"

namespace ddz {

bool PlayerDao::UpsertCoin(int64_t player_id, int64_t coin) {
    if (player_id <= 0) {
        return false;
    }
    player_coins_[player_id] = coin;
    return true;
}

std::optional<int64_t> PlayerDao::GetCoin(int64_t player_id) const {
    auto it = player_coins_.find(player_id);
    if (it == player_coins_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool PlayerDao::Erase(int64_t player_id) {
    return player_coins_.erase(player_id) > 0;
}

}  // namespace ddz

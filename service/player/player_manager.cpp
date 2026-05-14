#include "service/player/player_manager.h"

namespace ddz {

void PlayerManager::UpsertPlayer(int64_t player_id, const std::string& nickname, int64_t coin) {
    std::lock_guard<std::mutex> lock(mu_);
    PlayerData& player = players_[player_id];
    player.player_id = player_id;
    if (!nickname.empty()) {
        player.nickname = nickname;
    }
    if (player.nickname.empty()) {
        player.nickname = "player_" + std::to_string(player_id);
    }
    if (coin > 0) {
        player.coin = coin;
    } else if (player.coin == 0) {
        player.coin = 1000;
    }
}

std::optional<PlayerData> PlayerManager::GetPlayer(int64_t player_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = players_.find(player_id);
    if (it == players_.end()) {
        return std::nullopt;
    }
    return it->second;
}

PlayerState PlayerManager::GetState(int64_t player_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = players_.find(player_id);
    if (it == players_.end()) {
        return PlayerState::Offline;
    }
    return it->second.state;
}

int64_t PlayerManager::GetCoin(int64_t player_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = players_.find(player_id);
    if (it == players_.end()) {
        return 0;
    }
    return it->second.coin;
}

bool PlayerManager::SetState(int64_t player_id, PlayerState next) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = players_.find(player_id);
    if (it == players_.end()) {
        return false;
    }
    if (!IsTransitionAllowed(it->second.state, next)) {
        return false;
    }
    it->second.state = next;
    return true;
}

void PlayerManager::ForceState(int64_t player_id, PlayerState next) {
    std::lock_guard<std::mutex> lock(mu_);
    PlayerData& p = players_[player_id];
    p.player_id = player_id;
    if (p.nickname.empty()) {
        p.nickname = "player_" + std::to_string(player_id);
    }
    if (p.coin == 0) {
        p.coin = 1000;
    }
    p.state = next;
}

bool PlayerManager::AddCoin(int64_t player_id, int64_t delta, int64_t* after_coin) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = players_.find(player_id);
    if (it == players_.end()) {
        return false;
    }
    it->second.coin += delta;
    if (after_coin != nullptr) {
        *after_coin = it->second.coin;
    }
    return true;
}

bool PlayerManager::IsTransitionAllowed(PlayerState from, PlayerState to) const {
    if (from == to) {
        return true;
    }
    switch (from) {
    case PlayerState::Offline:
        return to == PlayerState::Lobby || to == PlayerState::Playing;
    case PlayerState::Lobby:
        return to == PlayerState::Matching || to == PlayerState::Offline;
    case PlayerState::Matching:
        return to == PlayerState::Lobby || to == PlayerState::InRoom || to == PlayerState::Offline;
    case PlayerState::InRoom:
        return to == PlayerState::Playing || to == PlayerState::Offline || to == PlayerState::Lobby;
    case PlayerState::Playing:
        return to == PlayerState::Settlement || to == PlayerState::Offline;
    case PlayerState::Settlement:
        return to == PlayerState::Lobby || to == PlayerState::Offline;
    default:
        return false;
    }
}

}  // namespace ddz

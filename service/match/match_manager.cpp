#include "service/match/match_manager.h"

#include <algorithm>

namespace ddz {

bool MatchManager::Join(const MatchPlayer& player) {
    std::lock_guard<std::mutex> lock(mu_);
    if (matching_index_.find(player.player_id) != matching_index_.end()) {
        return false;
    }
    auto* q = QueueByMode(player.mode);
    if (q == nullptr) {
        return false;
    }
    q->push_back(player);
    matching_index_[player.player_id] = player.mode;
    return true;
}

bool MatchManager::Cancel(int64_t player_id) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = matching_index_.find(player_id);
    if (it == matching_index_.end()) {
        return false;
    }

    auto* q = QueueByMode(it->second);
    if (q == nullptr) {
        matching_index_.erase(it);
        return false;
    }

    q->erase(std::remove_if(q->begin(), q->end(),
                            [player_id](const MatchPlayer& p) { return p.player_id == player_id; }),
             q->end());
    matching_index_.erase(it);
    return true;
}

bool MatchManager::IsMatching(int64_t player_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    return matching_index_.find(player_id) != matching_index_.end();
}

std::optional<std::vector<int64_t>> MatchManager::TryPopMatchedPlayers(int32_t mode) {
    std::lock_guard<std::mutex> lock(mu_);
    auto* q = QueueByMode(mode);
    const int need = RequiredCount(mode);
    if (q == nullptr || need <= 0 || static_cast<int>(q->size()) < need) {
        return std::nullopt;
    }

    std::vector<int64_t> players;
    players.reserve(static_cast<size_t>(need));
    for (int i = 0; i < need; ++i) {
        const auto p = q->front();
        q->pop_front();
        players.push_back(p.player_id);
        matching_index_.erase(p.player_id);
    }
    return players;
}

int MatchManager::RequiredCount(int32_t mode) {
    if (mode == 3) return 3;
    if (mode == 4) return 4;
    return 0;
}

std::deque<MatchPlayer>* MatchManager::QueueByMode(int32_t mode) {
    if (mode == 3) return &queue_three_;
    if (mode == 4) return &queue_four_;
    return nullptr;
}

const std::deque<MatchPlayer>* MatchManager::QueueByMode(int32_t mode) const {
    if (mode == 3) return &queue_three_;
    if (mode == 4) return &queue_four_;
    return nullptr;
}

}  // namespace ddz


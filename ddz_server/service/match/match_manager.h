#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace ddz {

struct MatchPlayer {
    int64_t player_id = 0;
    int64_t join_time_ms = 0;
    int64_t coin = 0;
    int32_t mode = 0;
};

class MatchManager {
public:
    bool Join(const MatchPlayer& player);
    bool Cancel(int64_t player_id);
    bool IsMatching(int64_t player_id) const;
    std::optional<std::vector<int64_t>> TryPopMatchedPlayers(int32_t mode);
    std::vector<MatchPlayer> PopTimeoutPlayers(int64_t now_ms, int64_t timeout_ms);

private:
    static int RequiredCount(int32_t mode);
    std::deque<MatchPlayer>* QueueByMode(int32_t mode);
    const std::deque<MatchPlayer>* QueueByMode(int32_t mode) const;

private:
    mutable std::mutex mu_;
    std::deque<MatchPlayer> queue_three_;
    std::deque<MatchPlayer> queue_four_;
    std::unordered_map<int64_t, int32_t> matching_index_;  // player_id -> mode
};

}  // namespace ddz

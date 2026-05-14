#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace ddz {

enum class PlayerState {
    Offline = 0,
    Lobby = 1,
    Matching = 2,
    InRoom = 3,
    Playing = 4,
    Settlement = 5
};

struct PlayerData {
    int64_t player_id = 0;
    std::string nickname;
    int64_t coin = 0;
    PlayerState state = PlayerState::Offline;
};

class PlayerManager {
public:
    void UpsertPlayer(int64_t player_id, const std::string& nickname, int64_t coin);
    std::optional<PlayerData> GetPlayer(int64_t player_id) const;
    PlayerState GetState(int64_t player_id) const;
    int64_t GetCoin(int64_t player_id) const;

    bool SetState(int64_t player_id, PlayerState next);
    void ForceState(int64_t player_id, PlayerState next);
    bool AddCoin(int64_t player_id, int64_t delta, int64_t* after_coin);

private:
    bool IsTransitionAllowed(PlayerState from, PlayerState to) const;

private:
    mutable std::mutex mu_;
    std::unordered_map<int64_t, PlayerData> players_;
};

}  // namespace ddz

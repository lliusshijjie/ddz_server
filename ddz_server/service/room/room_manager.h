#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "service/room/room.h"

namespace ddz {

class RoomManager {
public:
    int64_t CreateRoom(int32_t mode, const std::vector<int64_t>& players);
    bool DestroyRoom(int64_t room_id);

    std::optional<Room> GetRoomById(int64_t room_id) const;
    std::optional<Room> GetRoomByPlayer(int64_t player_id) const;
    std::optional<int64_t> GetRoomIdByPlayer(int64_t player_id) const;
    std::optional<RoomSnapshot> BuildSnapshotByRoomId(int64_t room_id) const;
    std::optional<RoomSnapshot> BuildSnapshotByPlayerId(int64_t player_id) const;
    std::optional<RoomPushSnapshotV2> BuildPushSnapshotV2ByRoomId(int64_t room_id) const;
    std::optional<RoomPushSnapshotV2> BuildPushSnapshotV2ByPlayerId(int64_t player_id) const;
    std::optional<RoomSnapshot> ApplyPlayerAction(int64_t room_id,
                                                  int64_t player_id,
                                                  PlayerActionType action_type,
                                                  std::string* err);
    std::optional<PlayerActionResult> ApplyPlayerAction(int64_t room_id,
                                                        int64_t player_id,
                                                        const PlayerActionRequest& request,
                                                        std::string* err);
    std::optional<std::vector<int32_t>> GetPlayerHand(int64_t room_id, int64_t player_id) const;
    bool MarkPlayerOffline(int64_t player_id, int64_t offline_at_ms);
    bool MarkPlayerOnline(int64_t player_id);
    bool MarkPlayerTrustee(int64_t player_id, bool trustee);
    std::optional<PlayerActionResult> ApplyTrusteeAction(int64_t room_id, int64_t player_id, std::string* err);
    std::optional<int64_t> PickWinnerForOfflineLose(int64_t room_id, int64_t loser_player_id) const;
    bool MarkSettling(int64_t room_id);
    bool MarkFinished(int64_t room_id);

private:
    mutable std::mutex mu_;
    int64_t next_room_id_ = 1;
    std::unordered_map<int64_t, Room> rooms_;
    std::unordered_map<int64_t, int64_t> player_to_room_;
};

}  // namespace ddz

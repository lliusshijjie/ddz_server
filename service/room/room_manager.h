#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
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

private:
    mutable std::mutex mu_;
    int64_t next_room_id_ = 1;
    std::unordered_map<int64_t, Room> rooms_;
    std::unordered_map<int64_t, int64_t> player_to_room_;
};

}  // namespace ddz

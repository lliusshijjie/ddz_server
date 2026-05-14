#include "service/room/room_manager.h"

namespace ddz {

int64_t RoomManager::CreateRoom(int32_t mode, const std::vector<int64_t>& players) {
    std::lock_guard<std::mutex> lock(mu_);
    const int64_t room_id = next_room_id_++;
    Room room;
    room.room_id = room_id;
    room.mode = mode;
    room.state = RoomState::Waiting;
    room.players = players;
    rooms_[room_id] = room;
    for (const auto player_id : players) {
        player_to_room_[player_id] = room_id;
    }
    return room_id;
}

bool RoomManager::DestroyRoom(int64_t room_id) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) {
        return false;
    }
    for (const auto player_id : it->second.players) {
        player_to_room_.erase(player_id);
    }
    rooms_.erase(it);
    return true;
}

std::optional<Room> RoomManager::GetRoomById(int64_t room_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<Room> RoomManager::GetRoomByPlayer(int64_t player_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto pit = player_to_room_.find(player_id);
    if (pit == player_to_room_.end()) {
        return std::nullopt;
    }
    auto rit = rooms_.find(pit->second);
    if (rit == rooms_.end()) {
        return std::nullopt;
    }
    return rit->second;
}

std::optional<int64_t> RoomManager::GetRoomIdByPlayer(int64_t player_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = player_to_room_.find(player_id);
    if (it == player_to_room_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<RoomSnapshot> RoomManager::BuildSnapshotByRoomId(int64_t room_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) {
        return std::nullopt;
    }
    RoomSnapshot snapshot;
    snapshot.room_id = it->second.room_id;
    snapshot.mode = it->second.mode;
    snapshot.room_state = static_cast<int32_t>(it->second.state);
    snapshot.players = it->second.players;
    return snapshot;
}

std::optional<RoomSnapshot> RoomManager::BuildSnapshotByPlayerId(int64_t player_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto pit = player_to_room_.find(player_id);
    if (pit == player_to_room_.end()) {
        return std::nullopt;
    }
    auto rit = rooms_.find(pit->second);
    if (rit == rooms_.end()) {
        return std::nullopt;
    }
    RoomSnapshot snapshot;
    snapshot.room_id = rit->second.room_id;
    snapshot.mode = rit->second.mode;
    snapshot.room_state = static_cast<int32_t>(rit->second.state);
    snapshot.players = rit->second.players;
    return snapshot;
}

}  // namespace ddz

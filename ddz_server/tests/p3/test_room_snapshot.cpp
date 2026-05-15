#include <cassert>
#include <iostream>

#include "service/room/room_manager.h"

int main() {
    ddz::RoomManager room_manager;
    const int64_t room_id = room_manager.CreateRoom(3, {1001, 1002, 1003});
    assert(room_id > 0);

    auto by_room = room_manager.BuildSnapshotByRoomId(room_id);
    assert(by_room.has_value());
    assert(by_room->room_id == room_id);
    assert(by_room->mode == 3);
    assert(by_room->players.size() == 3);

    auto by_player = room_manager.BuildSnapshotByPlayerId(1002);
    assert(by_player.has_value());
    assert(by_player->room_id == room_id);
    assert(by_player->players.size() == 3);

    room_manager.DestroyRoom(room_id);
    assert(!room_manager.BuildSnapshotByRoomId(room_id).has_value());
    assert(!room_manager.BuildSnapshotByPlayerId(1002).has_value());

    std::cout << "test_p3_room_snapshot passed" << std::endl;
    return 0;
}


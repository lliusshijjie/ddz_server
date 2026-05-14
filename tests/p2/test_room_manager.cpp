#include <cassert>
#include <iostream>

#include "service/room/room_manager.h"

int main() {
    ddz::RoomManager rm;

    const int64_t room_id = rm.CreateRoom(3, {1001, 1002, 1003});
    assert(room_id > 0);

    auto room = rm.GetRoomById(room_id);
    assert(room.has_value());
    assert(room->mode == 3);
    assert(room->players.size() == 3);

    auto room_by_player = rm.GetRoomByPlayer(1002);
    assert(room_by_player.has_value());
    assert(room_by_player->room_id == room_id);

    assert(rm.DestroyRoom(room_id));
    assert(!rm.GetRoomById(room_id).has_value());
    assert(!rm.GetRoomByPlayer(1002).has_value());

    std::cout << "test_p2_room_manager passed" << std::endl;
    return 0;
}


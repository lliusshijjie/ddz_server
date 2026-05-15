#include <cassert>
#include <iostream>
#include <string>

#include "service/room/room_manager.h"

int main() {
    ddz::RoomManager room_manager;
    const int64_t room_id = room_manager.CreateRoom(3, {1001, 1002, 1003});
    assert(room_id > 0);

    const auto initial = room_manager.BuildSnapshotByRoomId(room_id);
    assert(initial.has_value());
    assert(initial->room_state == static_cast<int32_t>(ddz::RoomState::WaitStart));
    assert(initial->current_operator_player_id == 1001);

    std::string err;
    auto s1 = room_manager.ApplyPlayerAction(room_id, 1001, ddz::PlayerActionType::CallScore, &err);
    assert(s1.has_value());
    assert(s1->room_state == static_cast<int32_t>(ddz::RoomState::CallScore));
    assert(s1->current_operator_player_id == 1002);

    auto s2 = room_manager.ApplyPlayerAction(room_id, 1002, ddz::PlayerActionType::CallScore, &err);
    assert(s2.has_value());
    assert(s2->room_state == static_cast<int32_t>(ddz::RoomState::CallScore));
    assert(s2->current_operator_player_id == 1003);

    auto s3 = room_manager.ApplyPlayerAction(room_id, 1003, ddz::PlayerActionType::CallScore, &err);
    assert(s3.has_value());
    assert(s3->room_state == static_cast<int32_t>(ddz::RoomState::RobLandlord));
    assert(s3->current_operator_player_id == 1001);

    room_manager.ApplyPlayerAction(room_id, 1001, ddz::PlayerActionType::RobLandlord, &err);
    room_manager.ApplyPlayerAction(room_id, 1002, ddz::PlayerActionType::RobLandlord, &err);
    auto s6 = room_manager.ApplyPlayerAction(room_id, 1003, ddz::PlayerActionType::RobLandlord, &err);
    assert(s6.has_value());
    assert(s6->room_state == static_cast<int32_t>(ddz::RoomState::Playing));
    assert(s6->current_operator_player_id == 1001);

    auto bad = room_manager.ApplyPlayerAction(room_id, 1001, ddz::PlayerActionType::CallScore, &err);
    assert(!bad.has_value());

    auto p1 = room_manager.ApplyPlayerAction(room_id, 1001, ddz::PlayerActionType::PlayCards, &err);
    assert(p1.has_value());
    assert(p1->current_operator_player_id == 1002);

    auto out_of_turn = room_manager.ApplyPlayerAction(room_id, 1001, ddz::PlayerActionType::Pass, &err);
    assert(!out_of_turn.has_value());

    std::cout << "test_p4_room_action_state_machine passed" << std::endl;
    return 0;
}

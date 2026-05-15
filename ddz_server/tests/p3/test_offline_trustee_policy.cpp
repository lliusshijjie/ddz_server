#include <cassert>
#include <iostream>
#include <string>

#include "service/room/room_manager.h"

namespace {

void EnterPlaying(ddz::RoomManager& rm, int64_t room_id) {
    std::string err;
    ddz::PlayerActionRequest req;
    req.action_type = ddz::PlayerActionType::CallScore;
    req.call_score = 1;
    assert(rm.ApplyPlayerAction(room_id, 1001, req, &err).has_value());
    req.call_score = 0;
    assert(rm.ApplyPlayerAction(room_id, 1002, req, &err).has_value());
    assert(rm.ApplyPlayerAction(room_id, 1003, req, &err).has_value());

    req = ddz::PlayerActionRequest{};
    req.action_type = ddz::PlayerActionType::RobLandlord;
    req.rob = 0;
    assert(rm.ApplyPlayerAction(room_id, 1001, req, &err).has_value());
    assert(rm.ApplyPlayerAction(room_id, 1002, req, &err).has_value());
    assert(rm.ApplyPlayerAction(room_id, 1003, req, &err).has_value());
}

}  // namespace

int main() {
    ddz::RoomManager rm;
    const int64_t room_id = rm.CreateRoom(3, {1001, 1002, 1003});
    EnterPlaying(rm, room_id);

    auto before = rm.BuildSnapshotByRoomId(room_id);
    assert(before.has_value());

    assert(rm.MarkPlayerOffline(1001, 1000));
    assert(rm.MarkPlayerTrustee(1001, true));
    auto snap = rm.BuildSnapshotByRoomId(room_id);
    assert(snap.has_value());
    assert(snap->snapshot_version > before->snapshot_version);
    assert(snap->players_online_bitmap == "011");
    assert(snap->trustee_players == "1001");

    std::string err;
    const auto action = rm.ApplyTrusteeAction(room_id, 1001, &err);
    assert(action.has_value());

    const auto winner = rm.PickWinnerForOfflineLose(room_id, 1001);
    assert(winner.has_value());
    assert(winner.value() != 1001);

    std::cout << "test_p3_offline_trustee_policy passed" << std::endl;
    return 0;
}

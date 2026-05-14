#include <cassert>
#include <iostream>

#include "service/match/match_service.h"
#include "service/player/player_manager.h"
#include "service/room/room_manager.h"
#include "service/session/session_manager.h"

int main() {
    ddz::SessionManager sm;
    ddz::PlayerManager pm;
    ddz::MatchManager mm;
    ddz::RoomManager rm;
    ddz::MatchService ms(sm, pm, mm, rm);

    for (int i = 1; i <= 4; ++i) {
        const int64_t player_id = 1000 + i;
        const int64_t conn_id = 2000 + i;
        pm.UpsertPlayer(player_id, "p" + std::to_string(i), 1000);
        pm.ForceState(player_id, ddz::PlayerState::Lobby);
        sm.BindLogin(player_id, conn_id, 100);
    }

    auto r1 = ms.HandleMatch(2001, "mode=3", 101);
    assert(r1.code == ddz::ErrorCode::OK);
    assert(!r1.matched);
    assert(pm.GetState(1001) == ddz::PlayerState::Matching);

    auto r2 = ms.HandleMatch(2002, "mode=3", 102);
    assert(r2.code == ddz::ErrorCode::OK);
    assert(!r2.matched);

    auto r3 = ms.HandleMatch(2003, "mode=3", 103);
    assert(r3.code == ddz::ErrorCode::OK);
    assert(r3.matched);
    assert(r3.room_id > 0);
    assert(r3.matched_players.size() == 3);
    assert(pm.GetState(1001) == ddz::PlayerState::InRoom);
    assert(sm.GetSessionByPlayer(1001)->room_id == r3.room_id);
    assert(rm.GetRoomById(r3.room_id).has_value());

    auto bad = ms.HandleMatch(2004, "mode=9", 104);
    assert(bad.code == ddz::ErrorCode::MATCH_MODE_INVALID);

    auto q = ms.HandleMatch(2004, "mode=4", 105);
    assert(q.code == ddz::ErrorCode::OK);
    assert(!q.matched);
    assert(pm.GetState(1004) == ddz::PlayerState::Matching);
    auto cancel = ms.HandleCancelMatch(2004);
    assert(cancel.code == ddz::ErrorCode::OK);
    assert(pm.GetState(1004) == ddz::PlayerState::Lobby);

    std::cout << "test_p2_match_service passed" << std::endl;
    return 0;
}


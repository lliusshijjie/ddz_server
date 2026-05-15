#include <cassert>
#include <iostream>
#include <string>

#include "service/player/player_manager.h"
#include "service/room/room_manager.h"
#include "service/session/session_manager.h"
#include "service/settlement/settlement_service.h"
#include "service/storage/storage_service.h"

namespace {

void PreparePlayers(ddz::SessionManager& sm, ddz::PlayerManager& pm, int64_t room_id) {
    for (int i = 0; i < 3; ++i) {
        const int64_t pid = 1001 + i;
        pm.UpsertPlayer(pid, "p" + std::to_string(i + 1), 1000);
        pm.ForceState(pid, ddz::PlayerState::InRoom);
        sm.BindLogin(pid, 2001 + i, 100);
        sm.SetRoomId(pid, room_id);
    }
}

}  // namespace

int main() {
    ddz::SessionManager sm;
    ddz::PlayerManager pm;
    ddz::RoomManager rm;
    ddz::StorageService storage;
    ddz::SettlementService settle(sm, pm, rm, storage);

    const int64_t room_id = rm.CreateRoom(3, {1001, 1002, 1003});
    PreparePlayers(sm, pm, room_id);

    std::string err;
    ddz::PlayerActionRequest req;

    // call score
    req.action_type = ddz::PlayerActionType::CallScore;
    req.call_score = 2;
    assert(rm.ApplyPlayerAction(room_id, 1001, req, &err).has_value());
    req.call_score = 1;
    assert(rm.ApplyPlayerAction(room_id, 1002, req, &err).has_value());
    req.call_score = 0;
    assert(rm.ApplyPlayerAction(room_id, 1003, req, &err).has_value());

    // rob landlord
    req = ddz::PlayerActionRequest{};
    req.action_type = ddz::PlayerActionType::RobLandlord;
    req.rob = 0;
    assert(rm.ApplyPlayerAction(room_id, 1001, req, &err).has_value());
    assert(rm.ApplyPlayerAction(room_id, 1002, req, &err).has_value());
    const auto entering_play = rm.ApplyPlayerAction(room_id, 1003, req, &err);
    assert(entering_play.has_value());
    assert(entering_play->snapshot.room_state == static_cast<int32_t>(ddz::RoomState::Playing));

    // play -> pass -> pass until someone wins
    int guard = 300;
    ddz::PlayerActionResult last_action;
    bool finished = false;
    while (guard-- > 0 && !finished) {
        const auto room = rm.GetRoomById(room_id);
        assert(room.has_value());
        const int64_t leader = room->current_operator_player_id;
        const auto hand = rm.GetPlayerHand(room_id, leader);
        assert(hand.has_value() && !hand->empty());

        ddz::PlayerActionRequest play;
        play.action_type = ddz::PlayerActionType::PlayCards;
        play.cards = {hand->front()};
        const auto r1 = rm.ApplyPlayerAction(room_id, leader, play, &err);
        assert(r1.has_value());
        last_action = r1.value();
        if (r1->game_over) {
            finished = true;
            break;
        }

        const int64_t p2 = r1->snapshot.current_operator_player_id;
        ddz::PlayerActionRequest pass;
        pass.action_type = ddz::PlayerActionType::Pass;
        const auto r2 = rm.ApplyPlayerAction(room_id, p2, pass, &err);
        assert(r2.has_value());

        const int64_t p3 = r2->snapshot.current_operator_player_id;
        const auto r3 = rm.ApplyPlayerAction(room_id, p3, pass, &err);
        assert(r3.has_value());
    }

    assert(finished);
    assert(last_action.game_over);
    assert(last_action.winner_player_id > 0);

    const auto settle_result = settle.SettleByServerResult(
        ddz::ServerSettlementDecision{room_id, last_action.winner_player_id, last_action.base_coin}, 500);
    assert(settle_result.code == ddz::ErrorCode::OK);
    assert(settle_result.record_id > 0);
    assert(settle_result.players.size() == 3);
    assert(!rm.GetRoomById(room_id).has_value());

    std::cout << "test_p4_full_game_e2e passed" << std::endl;
    return 0;
}


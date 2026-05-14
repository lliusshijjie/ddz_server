#include <cassert>
#include <iostream>
#include <string>

#include "service/login/auth_token_service.h"
#include "service/player/player_manager.h"
#include "service/reconnect/reconnect_service.h"
#include "service/room/room_manager.h"
#include "service/session/session_manager.h"
#include "service/settlement/settlement_service.h"
#include "service/storage/storage_service.h"

namespace {

void TestStorageTxFailureRollback() {
    ddz::SessionManager sm;
    ddz::PlayerManager pm;
    ddz::RoomManager rm;
    ddz::StorageService ss;
    ddz::SettlementService settle(sm, pm, rm, ss);

    pm.UpsertPlayer(1001, "a", 1000);
    pm.UpsertPlayer(1002, "b", 1000);
    pm.UpsertPlayer(1003, "c", 1000);
    pm.ForceState(1001, ddz::PlayerState::InRoom);
    pm.ForceState(1002, ddz::PlayerState::InRoom);
    pm.ForceState(1003, ddz::PlayerState::InRoom);
    sm.BindLogin(1001, 11, 100);
    sm.BindLogin(1002, 12, 100);
    sm.BindLogin(1003, 13, 100);

    const int64_t room_id = rm.CreateRoom(3, {1001, 1002, 1003});
    sm.SetRoomId(1001, room_id);
    sm.SetRoomId(1002, room_id);
    sm.SetRoomId(1003, room_id);

    ss.SetTxFailpointForTesting(ddz::StorageTxFailpoint::AfterFirstCoinLogInserted);
    const auto r = settle.Settle(ddz::SettlementRequest{room_id, 1001, 10}, 200);
    assert(r.code == ddz::ErrorCode::SETTLEMENT_FAILED);

    assert(pm.GetCoin(1001) == 1000);
    assert(pm.GetCoin(1002) == 1000);
    assert(pm.GetCoin(1003) == 1000);
    assert(rm.GetRoomById(room_id).has_value());
    assert(ss.GetCoinLogsByRoomId(room_id).empty());
}

void TestSessionInvalidation() {
    ddz::SessionManager sm;
    ddz::PlayerManager pm;
    ddz::RoomManager rm;
    ddz::AuthTokenService auth("fault_secret", 1);
    ddz::ReconnectService reconnect(sm, auth, pm, rm);

    const auto t1 = auth.Issue(2001, 1000);
    auto expired = reconnect.HandleReconnect(
        9001, "player_id=2001;token=" + t1.token + ";room_id=0;last_snapshot_version=0", 3000);
    assert(expired.code == ddz::ErrorCode::TOKEN_EXPIRED);

    const auto t2 = auth.Issue(2002, 2000);
    auto no_session = reconnect.HandleReconnect(
        9002, "player_id=2002;token=" + t2.token + ";room_id=0;last_snapshot_version=0", 2001);
    assert(no_session.code == ddz::ErrorCode::NOT_LOGIN);
}

void EnterPlaying(ddz::RoomManager& rm, int64_t room_id) {
    std::string err;
    ddz::PlayerActionRequest req;

    req.action_type = ddz::PlayerActionType::CallScore;
    req.call_score = 1;
    assert(rm.ApplyPlayerAction(room_id, 3001, req, &err).has_value());
    req.call_score = 0;
    assert(rm.ApplyPlayerAction(room_id, 3002, req, &err).has_value());
    assert(rm.ApplyPlayerAction(room_id, 3003, req, &err).has_value());

    req = ddz::PlayerActionRequest{};
    req.action_type = ddz::PlayerActionType::RobLandlord;
    req.rob = 0;
    assert(rm.ApplyPlayerAction(room_id, 3001, req, &err).has_value());
    assert(rm.ApplyPlayerAction(room_id, 3002, req, &err).has_value());
    assert(rm.ApplyPlayerAction(room_id, 3003, req, &err).has_value());
}

void TestIllegalActionRejected() {
    ddz::RoomManager rm;
    const int64_t room_id = rm.CreateRoom(3, {3001, 3002, 3003});
    EnterPlaying(rm, room_id);

    std::string err;
    ddz::PlayerActionRequest bad;
    bad.action_type = ddz::PlayerActionType::Pass;

    const auto out_of_turn = rm.ApplyPlayerAction(room_id, 3002, bad, &err);
    assert(!out_of_turn.has_value());

    ddz::PlayerActionRequest invalid_combo;
    invalid_combo.action_type = ddz::PlayerActionType::PlayCards;
    invalid_combo.cards = {0, 1};
    const auto invalid = rm.ApplyPlayerAction(room_id, 3001, invalid_combo, &err);
    assert(!invalid.has_value());
}

}  // namespace

int main() {
    TestStorageTxFailureRollback();
    TestSessionInvalidation();
    TestIllegalActionRejected();
    std::cout << "test_p5_fault_drills passed" << std::endl;
    return 0;
}

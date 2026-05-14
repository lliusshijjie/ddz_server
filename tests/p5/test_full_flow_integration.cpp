#include <cassert>
#include <iostream>

#include "service/login/login_service.h"
#include "service/match/match_manager.h"
#include "service/match/match_service.h"
#include "service/player/player_manager.h"
#include "service/reconnect/reconnect_service.h"
#include "service/room/room_manager.h"
#include "service/session/session_manager.h"
#include "service/settlement/settlement_service.h"
#include "service/storage/storage_service.h"

int main() {
    ddz::SessionManager session_manager;
    ddz::PlayerManager player_manager;
    ddz::RoomManager room_manager;
    ddz::MatchManager match_manager;
    ddz::StorageService storage_service;

    ddz::LoginService login_service(session_manager, player_manager);
    ddz::MatchService match_service(session_manager, player_manager, match_manager, room_manager);
    ddz::ReconnectService reconnect_service(session_manager, player_manager, room_manager);
    ddz::SettlementService settlement_service(session_manager, player_manager, room_manager, storage_service);

    // 1) login 4 players
    for (int i = 1; i <= 4; ++i) {
        const int64_t player_id = 1000 + i;
        const int64_t conn_id = 2000 + i;
        const auto r = login_service.HandleLogin(conn_id, "token=" + std::to_string(player_id), 100 + i);
        assert(r.code == ddz::ErrorCode::OK);
        assert(r.player_id == player_id);
    }

    // 2) first 3 players match into a room
    auto m1 = match_service.HandleMatch(2001, "mode=3", 200);
    auto m2 = match_service.HandleMatch(2002, "mode=3", 201);
    auto m3 = match_service.HandleMatch(2003, "mode=3", 202);
    assert(m1.code == ddz::ErrorCode::OK && !m1.matched);
    assert(m2.code == ddz::ErrorCode::OK && !m2.matched);
    assert(m3.code == ddz::ErrorCode::OK && m3.matched);
    const int64_t room_id = m3.room_id;
    assert(room_id > 0);
    assert(room_manager.GetRoomById(room_id).has_value());

    // 3) player 1002 disconnect + reconnect
    assert(session_manager.MarkOfflineByConnection(2002, 300));
    player_manager.ForceState(1002, ddz::PlayerState::Offline);
    const auto rc = reconnect_service.HandleReconnect(2102, "token=1002", 301);
    assert(rc.code == ddz::ErrorCode::OK);
    assert(rc.player_id == 1002);
    assert(rc.room_id == room_id);
    assert(rc.snapshot.has_value());
    assert(rc.snapshot->room_id == room_id);
    assert(player_manager.GetState(1002) == ddz::PlayerState::InRoom);

    // 4) settlement
    ddz::SettlementRequest req;
    req.room_id = room_id;
    req.winner_player_id = 1001;
    req.base_coin = 10;
    const auto sr = settlement_service.Settle(req, 400);
    assert(sr.code == ddz::ErrorCode::OK);
    assert(sr.players.size() == 3);
    assert(!room_manager.GetRoomById(room_id).has_value());

    // winner +20, others -10
    assert(player_manager.GetCoin(1001) == 1020);
    assert(player_manager.GetCoin(1002) == 990);
    assert(player_manager.GetCoin(1003) == 990);
    assert(player_manager.GetState(1001) == ddz::PlayerState::Lobby);
    assert(player_manager.GetState(1002) == ddz::PlayerState::Lobby);
    assert(player_manager.GetState(1003) == ddz::PlayerState::Lobby);

    const auto rec = storage_service.GetGameRecord(sr.record_id);
    assert(rec.has_value());
    assert(rec->room_id == room_id);
    const auto logs = storage_service.GetCoinLogsByRoomId(room_id);
    assert(logs.size() == 3);

    std::cout << "test_p5_full_flow_integration passed" << std::endl;
    return 0;
}


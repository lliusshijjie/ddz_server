#include <cassert>
#include <iostream>

#include "service/player/player_manager.h"
#include "service/room/room_manager.h"
#include "service/session/session_manager.h"
#include "service/settlement/settlement_service.h"
#include "service/storage/storage_service.h"

int main() {
    ddz::SessionManager session_manager;
    ddz::PlayerManager player_manager;
    ddz::RoomManager room_manager;
    ddz::StorageService storage_service;
    ddz::SettlementService settlement_service(session_manager, player_manager, room_manager, storage_service);

    // prepare room + players
    player_manager.UpsertPlayer(1001, "p1", 1000);
    player_manager.UpsertPlayer(1002, "p2", 1000);
    player_manager.UpsertPlayer(1003, "p3", 1000);
    player_manager.ForceState(1001, ddz::PlayerState::InRoom);
    player_manager.ForceState(1002, ddz::PlayerState::InRoom);
    player_manager.ForceState(1003, ddz::PlayerState::InRoom);

    session_manager.BindLogin(1001, 11, 100);
    session_manager.BindLogin(1002, 12, 100);
    session_manager.BindLogin(1003, 13, 100);

    const int64_t room_id = room_manager.CreateRoom(3, {1001, 1002, 1003});
    session_manager.SetRoomId(1001, room_id);
    session_manager.SetRoomId(1002, room_id);
    session_manager.SetRoomId(1003, room_id);

    ddz::SettlementRequest req;
    req.room_id = room_id;
    req.winner_player_id = 1001;
    req.base_coin = 10;

    const auto result = settlement_service.Settle(req, 200);
    assert(result.code == ddz::ErrorCode::OK);
    assert(result.record_id > 0);
    assert(result.players.size() == 3);

    // winner +20, losers -10
    assert(player_manager.GetCoin(1001) == 1020);
    assert(player_manager.GetCoin(1002) == 990);
    assert(player_manager.GetCoin(1003) == 990);

    // state back to lobby, room mapping cleared, room destroyed
    assert(player_manager.GetState(1001) == ddz::PlayerState::Lobby);
    assert(player_manager.GetState(1002) == ddz::PlayerState::Lobby);
    assert(player_manager.GetState(1003) == ddz::PlayerState::Lobby);
    assert(session_manager.GetSessionByPlayer(1001)->room_id == 0);
    assert(!room_manager.GetRoomById(room_id).has_value());

    auto rec = storage_service.GetGameRecord(result.record_id);
    assert(rec.has_value());
    assert(rec->winner_player_id == 1001);
    auto logs = storage_service.GetCoinLogsByRoomId(room_id);
    assert(logs.size() == 3);

    // invalid winner
    const int64_t room2 = room_manager.CreateRoom(3, {2001, 2002, 2003});
    auto bad = settlement_service.Settle(ddz::SettlementRequest{room2, 9999, 10}, 300);
    assert(bad.code == ddz::ErrorCode::SETTLEMENT_FAILED);

    // invalid room
    auto no_room = settlement_service.Settle(ddz::SettlementRequest{999999, 1001, 10}, 400);
    assert(no_room.code == ddz::ErrorCode::ROOM_NOT_FOUND);

    std::cout << "test_p4_settlement_service passed" << std::endl;
    return 0;
}


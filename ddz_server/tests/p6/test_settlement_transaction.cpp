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

    player_manager.UpsertPlayer(101, "p1", 1000);
    player_manager.UpsertPlayer(102, "p2", 1000);
    player_manager.UpsertPlayer(103, "p3", 1000);
    player_manager.ForceState(101, ddz::PlayerState::InRoom);
    player_manager.ForceState(102, ddz::PlayerState::InRoom);
    player_manager.ForceState(103, ddz::PlayerState::InRoom);
    session_manager.BindLogin(101, 1, 100);
    session_manager.BindLogin(102, 2, 100);
    session_manager.BindLogin(103, 3, 100);

    const int64_t room_id = room_manager.CreateRoom(3, {101, 102, 103});
    session_manager.SetRoomId(101, room_id);
    session_manager.SetRoomId(102, room_id);
    session_manager.SetRoomId(103, room_id);

    auto ok = settlement_service.Settle(ddz::SettlementRequest{room_id, 101, 10}, 200);
    assert(ok.code == ddz::ErrorCode::OK);
    assert(ok.record_id > 0);
    assert(player_manager.GetCoin(101) == 1020);
    assert(player_manager.GetCoin(102) == 990);
    assert(player_manager.GetCoin(103) == 990);

    player_manager.UpsertPlayer(201, "q1", 1000);
    player_manager.UpsertPlayer(202, "q2", 1000);
    player_manager.UpsertPlayer(203, "q3", 1000);
    player_manager.ForceState(201, ddz::PlayerState::InRoom);
    player_manager.ForceState(202, ddz::PlayerState::InRoom);
    player_manager.ForceState(203, ddz::PlayerState::InRoom);
    session_manager.BindLogin(201, 11, 300);
    session_manager.BindLogin(202, 12, 300);
    session_manager.BindLogin(203, 13, 300);
    const int64_t room_id2 = room_manager.CreateRoom(3, {201, 202, 203});
    session_manager.SetRoomId(201, room_id2);
    session_manager.SetRoomId(202, room_id2);
    session_manager.SetRoomId(203, room_id2);

    storage_service.SetTxFailpointForTesting(ddz::StorageTxFailpoint::AfterFirstCoinLogInserted);
    auto bad = settlement_service.Settle(ddz::SettlementRequest{room_id2, 201, 10}, 400);
    assert(bad.code == ddz::ErrorCode::SETTLEMENT_FAILED);
    storage_service.SetTxFailpointForTesting(ddz::StorageTxFailpoint::None);

    assert(player_manager.GetCoin(201) == 1000);
    assert(player_manager.GetCoin(202) == 1000);
    assert(player_manager.GetCoin(203) == 1000);
    assert(room_manager.GetRoomById(room_id2).has_value());
    assert(storage_service.GetCoinLogsByRoomId(room_id2).empty());

    std::cout << "test_p6_settlement_transaction passed" << std::endl;
    return 0;
}

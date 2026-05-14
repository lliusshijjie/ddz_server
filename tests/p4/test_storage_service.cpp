#include <cassert>
#include <iostream>

#include "service/storage/storage_service.h"

int main() {
    ddz::StorageService storage;

    ddz::GameRecord record;
    record.room_id = 11;
    record.game_mode = 3;
    record.winner_player_id = 1001;
    record.players = {1001, 1002, 1003};
    record.started_at_ms = 100;
    record.ended_at_ms = 200;
    const int64_t record_id = storage.SaveGameRecord(record);
    assert(record_id > 0);

    auto rec = storage.GetGameRecord(record_id);
    assert(rec.has_value());
    assert(rec->room_id == 11);
    assert(rec->players.size() == 3);

    ddz::CoinLog log;
    log.player_id = 1001;
    log.room_id = 11;
    log.change_amount = 20;
    log.before_coin = 1000;
    log.after_coin = 1020;
    log.created_at_ms = 210;
    const int64_t log_id = storage.SaveCoinLog(log);
    assert(log_id > 0);

    auto logs = storage.GetCoinLogsByRoomId(11);
    assert(logs.size() == 1);
    assert(logs[0].change_amount == 20);

    storage.SavePlayerCoin(1001, 1020);
    auto coin = storage.GetPlayerCoin(1001);
    assert(coin.has_value());
    assert(coin.value() == 1020);

    std::cout << "test_p4_storage_service passed" << std::endl;
    return 0;
}


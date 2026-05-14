#include <cassert>
#include <iostream>

#include "common/config/config_loader.h"
#include "service/storage/storage_service.h"

int main() {
    ddz::StorageService storage;

    ddz::AppConfig cfg;
    cfg.mysql.enabled = true;
    cfg.mysql.pool_size = 2;
    std::string err;
    assert(storage.Init(cfg, &err));

    storage.SavePlayerCoin(1001, 1100);
    auto coin = storage.GetPlayerCoin(1001);
    assert(coin.has_value());
    assert(coin.value() == 1100);

    ddz::GameRecord record;
    record.room_id = 1;
    record.game_mode = 3;
    record.winner_player_id = 1001;
    record.started_at_ms = 1000;
    record.ended_at_ms = 1200;
    record.players = {1001, 1002, 1003};
    const int64_t record_id = storage.SaveGameRecord(record);
    assert(record_id > 0);

    auto rec = storage.GetGameRecord(record_id);
    assert(rec.has_value());
    assert(rec->room_id == 1);
    assert(rec->winner_player_id == 1001);

    ddz::CoinLog log;
    log.player_id = 1001;
    log.room_id = 1;
    log.change_amount = 20;
    log.before_coin = 1080;
    log.after_coin = 1100;
    log.created_at_ms = 1200;
    const int64_t log_id = storage.SaveCoinLog(log);
    assert(log_id > 0);

    const auto logs = storage.GetCoinLogsByRoomId(1);
    assert(logs.size() == 1);
    assert(logs[0].change_amount == 20);

    std::cout << "test_p6_mysql_dao passed" << std::endl;
    return 0;
}

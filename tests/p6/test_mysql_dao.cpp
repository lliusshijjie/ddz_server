#include <cassert>
#include <iostream>

#include "common/config/config_loader.h"
#include "common/util/time_util.h"
#include "service/storage/storage_service.h"

int main() {
    ddz::StorageService storage;

    ddz::AppConfig cfg;
    cfg.mysql.enabled = true;
    cfg.mysql.pool_size = 2;
    std::string err;
    if (!storage.Init(cfg, &err)) {
        std::cout << "test_p6_mysql_dao skipped: mysql unavailable: " << err << std::endl;
        return 0;
    }

    const int64_t player_id = 1001 + (ddz::NowMs() % 100000);
    const int64_t room_id = 100000 + (ddz::NowMs() % 1000000);

    storage.SavePlayerCoin(player_id, 1100);
    auto coin = storage.GetPlayerCoin(player_id);
    assert(coin.has_value());
    assert(coin.value() == 1100);

    ddz::GameRecord record;
    record.room_id = room_id;
    record.game_mode = 3;
    record.winner_player_id = player_id;
    record.started_at_ms = 1000;
    record.ended_at_ms = 1200;
    record.players = {player_id, player_id + 1, player_id + 2};
    const int64_t record_id = storage.SaveGameRecord(record);
    assert(record_id > 0);

    auto rec = storage.GetGameRecord(record_id);
    assert(rec.has_value());
    assert(rec->room_id == room_id);
    assert(rec->winner_player_id == player_id);

    ddz::CoinLog log;
    log.player_id = player_id;
    log.room_id = room_id;
    log.change_amount = 20;
    log.before_coin = 1080;
    log.after_coin = 1100;
    log.created_at_ms = 1200;
    const int64_t log_id = storage.SaveCoinLog(log);
    assert(log_id > 0);

    const auto logs = storage.GetCoinLogsByRoomId(room_id);
    assert(logs.size() == 1);
    assert(logs[0].change_amount == 20);

    std::cout << "test_p6_mysql_dao passed" << std::endl;
    return 0;
}

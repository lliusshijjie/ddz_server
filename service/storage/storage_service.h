#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "common/config/config_loader.h"
#include "service/storage/dao/coin_log_dao.h"
#include "service/storage/dao/game_record_dao.h"
#include "service/storage/dao/player_dao.h"
#include "service/storage/mysql/mysql_connection_pool.h"
#include "service/storage/storage_models.h"

namespace ddz {

enum class StorageTxFailpoint {
    None = 0,
    AfterRecordInserted = 1,
    AfterFirstCoinLogInserted = 2
};

struct SettlementCoinChange {
    int64_t player_id = 0;
    int64_t room_id = 0;
    int64_t delta = 0;
    int64_t before_coin = 0;
    int64_t after_coin = 0;
};

struct SettlementPersistRequest {
    int64_t room_id = 0;
    int32_t game_mode = 0;
    int64_t winner_player_id = 0;
    int64_t started_at_ms = 0;
    int64_t ended_at_ms = 0;
    std::vector<int64_t> players;
    std::vector<SettlementCoinChange> coin_changes;
};

class StorageService {
public:
    bool Init(const AppConfig& config, std::string* err);

    int64_t SaveGameRecord(const GameRecord& record);
    int64_t SaveCoinLog(const CoinLog& log);
    void SavePlayerCoin(int64_t player_id, int64_t coin);

    std::optional<GameRecord> GetGameRecord(int64_t record_id) const;
    std::vector<CoinLog> GetCoinLogsByRoomId(int64_t room_id) const;
    std::optional<int64_t> GetPlayerCoin(int64_t player_id) const;

    bool PersistSettlementTransaction(const SettlementPersistRequest& req, int64_t* out_record_id, std::string* err);
    void SetTxFailpointForTesting(StorageTxFailpoint failpoint);

private:
    mutable std::mutex mu_;
    MySqlConnectionPool mysql_pool_;
    PlayerDao player_dao_;
    GameRecordDao game_record_dao_;
    CoinLogDao coin_log_dao_;
    StorageTxFailpoint failpoint_ = StorageTxFailpoint::None;
};

}  // namespace ddz

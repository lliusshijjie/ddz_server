#include "service/storage/storage_service.h"

namespace ddz {

bool StorageService::Init(const AppConfig& config, std::string* err) {
    MysqlRuntimeConfig mysql_cfg;
    mysql_cfg.enabled = config.mysql.enabled;
    mysql_cfg.host = config.mysql.host;
    mysql_cfg.port = config.mysql.port;
    mysql_cfg.user = config.mysql.user;
    mysql_cfg.password = config.mysql.password;
    mysql_cfg.database = config.mysql.database;
    mysql_cfg.pool_size = config.mysql.pool_size;
    if (!mysql_pool_.Init(mysql_cfg, err)) {
        return false;
    }
    player_dao_.BindPool(&mysql_pool_);
    game_record_dao_.BindPool(&mysql_pool_);
    coin_log_dao_.BindPool(&mysql_pool_);
    return true;
}

int64_t StorageService::SaveGameRecord(const GameRecord& record) {
    std::lock_guard<std::mutex> lock(mu_);
    return game_record_dao_.Insert(record);
}

int64_t StorageService::SaveCoinLog(const CoinLog& log) {
    std::lock_guard<std::mutex> lock(mu_);
    return coin_log_dao_.Insert(log);
}

void StorageService::SavePlayerCoin(int64_t player_id, int64_t coin) {
    std::lock_guard<std::mutex> lock(mu_);
    player_dao_.UpsertCoin(player_id, coin);
}

std::optional<GameRecord> StorageService::GetGameRecord(int64_t record_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    return game_record_dao_.GetById(record_id);
}

std::vector<CoinLog> StorageService::GetCoinLogsByRoomId(int64_t room_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    return coin_log_dao_.GetByRoomId(room_id);
}

std::optional<int64_t> StorageService::GetPlayerCoin(int64_t player_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    return player_dao_.GetCoin(player_id);
}

bool StorageService::PersistSettlementTransaction(const SettlementPersistRequest& req, int64_t* out_record_id, std::string* err) {
    std::lock_guard<std::mutex> lock(mu_);
    if (req.room_id <= 0 || req.winner_player_id <= 0 || req.coin_changes.empty()) {
        if (err != nullptr) {
            *err = "invalid settlement persist request";
        }
        return false;
    }

    const bool mysql_enabled = mysql_pool_.enabled();
    MySqlConnectionPool::Lease lease;
    if (mysql_enabled) {
        lease = mysql_pool_.Acquire();
        if (!lease.valid) {
            if (err != nullptr) {
                *err = "mysql pool exhausted";
            }
            return false;
        }
    }

    if (mysql_enabled) {
        auto join_players_csv = [](const std::vector<int64_t>& players) {
            std::string out;
            for (size_t i = 0; i < players.size(); ++i) {
                if (i > 0) {
                    out.push_back(',');
                }
                out += std::to_string(players[i]);
            }
            return out;
        };

        std::string sql;
        sql += "START TRANSACTION;";
        sql += "INSERT INTO game_record(room_id, game_mode, winner_player_id, started_at_ms, ended_at_ms, players_csv) VALUES(" +
               std::to_string(req.room_id) + "," +
               std::to_string(req.game_mode) + "," +
               std::to_string(req.winner_player_id) + "," +
               std::to_string(req.started_at_ms) + "," +
               std::to_string(req.ended_at_ms) + ",'" + join_players_csv(req.players) + "');";
        sql += "SET @rid = LAST_INSERT_ID();";
        if (failpoint_ == StorageTxFailpoint::AfterRecordInserted) {
            sql += "INSERT INTO __ddz_failpoint_after_record__(id) VALUES (1);";
        }
        for (size_t i = 0; i < req.coin_changes.size(); ++i) {
            const auto& c = req.coin_changes[i];
            sql += "INSERT INTO coin_log(player_id, room_id, change_amount, before_coin, after_coin, created_at_ms) VALUES(" +
                   std::to_string(c.player_id) + "," +
                   std::to_string(c.room_id) + "," +
                   std::to_string(c.delta) + "," +
                   std::to_string(c.before_coin) + "," +
                   std::to_string(c.after_coin) + "," +
                   std::to_string(req.ended_at_ms) + ");";
            if (failpoint_ == StorageTxFailpoint::AfterFirstCoinLogInserted && i == 0) {
                sql += "INSERT INTO __ddz_failpoint_after_coin_log__(id) VALUES (1);";
            }
            sql += "INSERT INTO player(player_id, coin, updated_at_ms) VALUES(" +
                   std::to_string(c.player_id) + "," +
                   std::to_string(c.after_coin) + "," +
                   std::to_string(req.ended_at_ms) + ") "
                   "ON DUPLICATE KEY UPDATE coin=VALUES(coin), updated_at_ms=VALUES(updated_at_ms);";
        }
        sql += "COMMIT;";
        sql += "SELECT @rid;";

        std::vector<std::string> lines;
        if (!mysql_pool_.ExecuteSql(lease, sql, &lines, err)) {
            mysql_pool_.Release(lease);
            return false;
        }
        if (lines.empty()) {
            mysql_pool_.Release(lease);
            if (err != nullptr) {
                *err = "mysql tx no record id returned";
            }
            return false;
        }
        try {
            if (out_record_id != nullptr) {
                *out_record_id = std::stoll(lines.back());
            }
        } catch (...) {
            mysql_pool_.Release(lease);
            if (err != nullptr) {
                *err = "parse mysql record id failed";
            }
            return false;
        }
        mysql_pool_.Release(lease);
        return true;
    }

    std::vector<int64_t> inserted_coin_log_ids;
    std::vector<std::pair<int64_t, std::optional<int64_t>>> old_player_coins;
    int64_t inserted_record_id = 0;

    auto rollback = [&]() {
        for (size_t i = inserted_coin_log_ids.size(); i > 0; --i) {
            coin_log_dao_.DeleteById(inserted_coin_log_ids[i - 1]);
        }
        if (inserted_record_id > 0) {
            game_record_dao_.DeleteById(inserted_record_id);
        }
        for (size_t i = old_player_coins.size(); i > 0; --i) {
            const auto& item = old_player_coins[i - 1];
            if (item.second.has_value()) {
                player_dao_.UpsertCoin(item.first, item.second.value());
            } else {
                player_dao_.Erase(item.first);
            }
        }
    };

    GameRecord record;
    record.room_id = req.room_id;
    record.game_mode = req.game_mode;
    record.winner_player_id = req.winner_player_id;
    record.started_at_ms = req.started_at_ms;
    record.ended_at_ms = req.ended_at_ms;
    record.players = req.players;
    inserted_record_id = game_record_dao_.Insert(record);
    if (inserted_record_id <= 0) {
        if (err != nullptr) {
            *err = "insert game record failed";
        }
        return false;
    }
    if (failpoint_ == StorageTxFailpoint::AfterRecordInserted) {
        rollback();
        if (err != nullptr) {
            *err = "tx failpoint after record inserted";
        }
        return false;
    }

    for (size_t i = 0; i < req.coin_changes.size(); ++i) {
        const auto& c = req.coin_changes[i];
        old_player_coins.push_back({c.player_id, player_dao_.GetCoin(c.player_id)});
        if (!player_dao_.UpsertCoin(c.player_id, c.after_coin)) {
            rollback();
            if (err != nullptr) {
                *err = "upsert player coin failed";
            }
            return false;
        }

        CoinLog log;
        log.player_id = c.player_id;
        log.room_id = c.room_id;
        log.change_amount = c.delta;
        log.before_coin = c.before_coin;
        log.after_coin = c.after_coin;
        log.created_at_ms = req.ended_at_ms;
        const int64_t log_id = coin_log_dao_.Insert(log);
        if (log_id <= 0) {
            rollback();
            if (err != nullptr) {
                *err = "insert coin log failed";
            }
            return false;
        }
        inserted_coin_log_ids.push_back(log_id);

        if (failpoint_ == StorageTxFailpoint::AfterFirstCoinLogInserted && i == 0) {
            rollback();
            if (err != nullptr) {
                *err = "tx failpoint after first coin log inserted";
            }
            return false;
        }
    }

    if (out_record_id != nullptr) {
        *out_record_id = inserted_record_id;
    }
    return true;
}

void StorageService::SetTxFailpointForTesting(StorageTxFailpoint failpoint) {
    std::lock_guard<std::mutex> lock(mu_);
    failpoint_ = failpoint;
}

}  // namespace ddz

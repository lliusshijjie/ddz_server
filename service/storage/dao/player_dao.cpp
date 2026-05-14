#include "service/storage/dao/player_dao.h"

#include <vector>

namespace ddz {
namespace {

std::optional<int64_t> ParseI64(const std::string& s) {
    try {
        size_t idx = 0;
        const int64_t v = std::stoll(s, &idx);
        if (idx != s.size()) {
            return std::nullopt;
        }
        return v;
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace

void PlayerDao::BindPool(MySqlConnectionPool* pool) {
    mysql_pool_ = pool;
}

bool PlayerDao::UpsertCoin(int64_t player_id, int64_t coin) {
    if (player_id <= 0) {
        return false;
    }
    if (mysql_pool_ != nullptr && mysql_pool_->enabled()) {
        std::string err;
        std::vector<std::string> ignored;
        const std::string sql =
            "INSERT INTO player(player_id, coin, updated_at_ms) VALUES(" +
            std::to_string(player_id) + "," + std::to_string(coin) + ",0)"
            " ON DUPLICATE KEY UPDATE coin=VALUES(coin), updated_at_ms=VALUES(updated_at_ms);";
        return mysql_pool_->ExecuteSql(sql, &ignored, &err);
    }
    player_coins_[player_id] = coin;
    return true;
}

std::optional<int64_t> PlayerDao::GetCoin(int64_t player_id) const {
    if (mysql_pool_ != nullptr && mysql_pool_->enabled()) {
        std::string err;
        std::vector<std::string> lines;
        const std::string sql = "SELECT coin FROM player WHERE player_id=" + std::to_string(player_id) + " LIMIT 1;";
        if (!mysql_pool_->ExecuteSql(sql, &lines, &err)) {
            return std::nullopt;
        }
        if (lines.empty()) {
            return std::nullopt;
        }
        return ParseI64(lines.front());
    }
    auto it = player_coins_.find(player_id);
    if (it == player_coins_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool PlayerDao::Erase(int64_t player_id) {
    if (mysql_pool_ != nullptr && mysql_pool_->enabled()) {
        std::string err;
        std::vector<std::string> ignored;
        const std::string sql = "DELETE FROM player WHERE player_id=" + std::to_string(player_id) + ";";
        return mysql_pool_->ExecuteSql(sql, &ignored, &err);
    }
    return player_coins_.erase(player_id) > 0;
}

}  // namespace ddz

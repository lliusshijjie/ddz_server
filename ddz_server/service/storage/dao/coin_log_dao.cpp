#include "service/storage/dao/coin_log_dao.h"

namespace ddz {
namespace {

std::vector<std::string> SplitByTab(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char ch : line) {
        if (ch == '\t') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    out.push_back(cur);
    return out;
}

bool ParseI64(const std::string& s, int64_t* out) {
    try {
        size_t idx = 0;
        const int64_t v = std::stoll(s, &idx);
        if (idx != s.size()) {
            return false;
        }
        *out = v;
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace

void CoinLogDao::BindPool(MySqlConnectionPool* pool) {
    mysql_pool_ = pool;
}

int64_t CoinLogDao::Insert(const CoinLog& log) {
    if (mysql_pool_ != nullptr && mysql_pool_->enabled()) {
        std::vector<std::string> lines;
        std::string err;
        const std::string sql =
            "INSERT INTO coin_log(player_id, room_id, change_amount, before_coin, after_coin, created_at_ms) VALUES(" +
            std::to_string(log.player_id) + "," +
            std::to_string(log.room_id) + "," +
            std::to_string(log.change_amount) + "," +
            std::to_string(log.before_coin) + "," +
            std::to_string(log.after_coin) + "," +
            std::to_string(log.created_at_ms) + ");"
            "SELECT LAST_INSERT_ID();";
        if (!mysql_pool_->ExecuteSql(sql, &lines, &err) || lines.empty()) {
            return 0;
        }
        int64_t id = 0;
        if (!ParseI64(lines.back(), &id)) {
            return 0;
        }
        return id;
    }

    CoinLog copy = log;
    copy.id = next_coin_log_id_++;
    coin_logs_[copy.id] = copy;
    return copy.id;
}

std::vector<CoinLog> CoinLogDao::GetByRoomId(int64_t room_id) const {
    if (mysql_pool_ != nullptr && mysql_pool_->enabled()) {
        std::vector<std::string> lines;
        std::string err;
        const std::string sql =
            "SELECT id,player_id,room_id,change_amount,before_coin,after_coin,created_at_ms FROM coin_log "
            "WHERE room_id=" + std::to_string(room_id) + " ORDER BY id ASC;";
        if (!mysql_pool_->ExecuteSql(sql, &lines, &err)) {
            return {};
        }
        std::vector<CoinLog> out;
        out.reserve(lines.size());
        for (const auto& line : lines) {
            const auto fields = SplitByTab(line);
            if (fields.size() != 7) {
                continue;
            }
            CoinLog log;
            if (!ParseI64(fields[0], &log.id) ||
                !ParseI64(fields[1], &log.player_id) ||
                !ParseI64(fields[2], &log.room_id) ||
                !ParseI64(fields[3], &log.change_amount) ||
                !ParseI64(fields[4], &log.before_coin) ||
                !ParseI64(fields[5], &log.after_coin) ||
                !ParseI64(fields[6], &log.created_at_ms)) {
                continue;
            }
            out.push_back(log);
        }
        return out;
    }

    std::vector<CoinLog> out;
    for (const auto& item : coin_logs_) {
        if (item.second.room_id == room_id) {
            out.push_back(item.second);
        }
    }
    return out;
}

bool CoinLogDao::DeleteById(int64_t id) {
    if (mysql_pool_ != nullptr && mysql_pool_->enabled()) {
        std::vector<std::string> lines;
        std::string err;
        const std::string sql = "DELETE FROM coin_log WHERE id=" + std::to_string(id) + ";";
        return mysql_pool_->ExecuteSql(sql, &lines, &err);
    }
    return coin_logs_.erase(id) > 0;
}

}  // namespace ddz

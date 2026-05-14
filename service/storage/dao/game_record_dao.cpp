#include "service/storage/dao/game_record_dao.h"

#include <sstream>
#include <vector>

namespace ddz {
namespace {

std::string JoinPlayers(const std::vector<int64_t>& players) {
    std::string out;
    for (size_t i = 0; i < players.size(); ++i) {
        if (i > 0) {
            out.push_back(',');
        }
        out += std::to_string(players[i]);
    }
    return out;
}

std::vector<int64_t> ParsePlayers(const std::string& csv) {
    std::vector<int64_t> out;
    if (csv.empty()) {
        return out;
    }
    std::stringstream ss(csv);
    std::string part;
    while (std::getline(ss, part, ',')) {
        try {
            size_t idx = 0;
            const int64_t value = std::stoll(part, &idx);
            if (idx == part.size()) {
                out.push_back(value);
            }
        } catch (...) {
            return {};
        }
    }
    return out;
}

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

void GameRecordDao::BindPool(MySqlConnectionPool* pool) {
    mysql_pool_ = pool;
}

int64_t GameRecordDao::Insert(const GameRecord& record) {
    if (mysql_pool_ != nullptr && mysql_pool_->enabled()) {
        std::vector<std::string> lines;
        std::string err;
        const std::string sql =
            "INSERT INTO game_record(room_id, game_mode, winner_player_id, started_at_ms, ended_at_ms, players_csv) VALUES(" +
            std::to_string(record.room_id) + "," +
            std::to_string(record.game_mode) + "," +
            std::to_string(record.winner_player_id) + "," +
            std::to_string(record.started_at_ms) + "," +
            std::to_string(record.ended_at_ms) + ",'" + JoinPlayers(record.players) + "');"
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

    GameRecord copy = record;
    copy.record_id = next_record_id_++;
    game_records_[copy.record_id] = copy;
    return copy.record_id;
}

std::optional<GameRecord> GameRecordDao::GetById(int64_t record_id) const {
    if (mysql_pool_ != nullptr && mysql_pool_->enabled()) {
        std::vector<std::string> lines;
        std::string err;
        const std::string sql =
            "SELECT record_id,room_id,game_mode,winner_player_id,started_at_ms,ended_at_ms,players_csv "
            "FROM game_record WHERE record_id=" + std::to_string(record_id) + " LIMIT 1;";
        if (!mysql_pool_->ExecuteSql(sql, &lines, &err) || lines.empty()) {
            return std::nullopt;
        }
        const auto fields = SplitByTab(lines.front());
        if (fields.size() != 7) {
            return std::nullopt;
        }
        GameRecord out;
        int64_t game_mode_i64 = 0;
        if (!ParseI64(fields[0], &out.record_id) ||
            !ParseI64(fields[1], &out.room_id) ||
            !ParseI64(fields[2], &game_mode_i64) ||
            !ParseI64(fields[3], &out.winner_player_id) ||
            !ParseI64(fields[4], &out.started_at_ms) ||
            !ParseI64(fields[5], &out.ended_at_ms)) {
            return std::nullopt;
        }
        out.game_mode = static_cast<int32_t>(game_mode_i64);
        out.players = ParsePlayers(fields[6]);
        return out;
    }

    auto it = game_records_.find(record_id);
    if (it == game_records_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool GameRecordDao::DeleteById(int64_t record_id) {
    if (mysql_pool_ != nullptr && mysql_pool_->enabled()) {
        std::vector<std::string> lines;
        std::string err;
        const std::string sql = "DELETE FROM game_record WHERE record_id=" + std::to_string(record_id) + ";";
        return mysql_pool_->ExecuteSql(sql, &lines, &err);
    }
    return game_records_.erase(record_id) > 0;
}

}  // namespace ddz

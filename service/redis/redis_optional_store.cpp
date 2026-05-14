#include "service/redis/redis_optional_store.h"

#include <cstdio>
#include <sstream>

#include "common/util/time_util.h"

namespace ddz {
namespace {

constexpr const char* kRedisContainer = "ddz_redis";

std::string TrimRightNewline(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
        s.pop_back();
    }
    return s;
}

}  // namespace

bool RedisOptionalStore::Init(const RedisConfig& cfg, std::string* err) {
    std::lock_guard<std::mutex> lock(mu_);
    cfg_ = cfg;
    enabled_ = cfg.enabled;
    if (cfg.port == 0) {
        if (err != nullptr) {
            *err = "redis.port must be > 0";
        }
        return false;
    }
    if (!enabled_) {
        return true;
    }
    std::vector<std::string> lines;
    if (!RunRedisCli("PING", &lines, err)) {
        return false;
    }
    if (lines.empty() || lines.back() != "PONG") {
        if (err != nullptr) {
            *err = "redis ping failed";
        }
        return false;
    }
    return true;
}

void RedisOptionalStore::SeedTokenForTesting(int64_t player_id, const std::string& token) {
    std::string ignored;
    StoreToken(player_id, token, 7LL * 24LL * 3600LL * 1000LL, &ignored);
}

bool RedisOptionalStore::StoreToken(int64_t player_id, const std::string& token, int64_t ttl_ms, std::string* err) {
    std::lock_guard<std::mutex> lock(mu_);
    if (player_id <= 0 || token.empty() || ttl_ms <= 0) {
        if (err != nullptr) {
            *err = "invalid token payload";
        }
        return false;
    }
    if (!enabled_) {
        token_by_player_[player_id] = ExpiringToken{token, ExpireAtMsFromNow(ttl_ms)};
        return true;
    }

    std::vector<std::string> lines;
    const std::string key = "token:" + std::to_string(player_id);
    const std::string args = "PSETEX " + key + " " + std::to_string(ttl_ms) + " \"" +
                             EscapeForDoubleQuotedShell(token) + "\"";
    return RunRedisCli(args, &lines, err);
}

bool RedisOptionalStore::ValidateToken(int64_t player_id, const std::string& token) const {
    std::lock_guard<std::mutex> lock(mu_);
    if (!enabled_) {
        auto it = token_by_player_.find(player_id);
        if (it == token_by_player_.end()) {
            return true;
        }
        if (IsExpired(it->second.expire_at_ms)) {
            return true;
        }
        return it->second.token == token;
    }

    std::vector<std::string> lines;
    std::string err;
    const std::string args = "GET token:" + std::to_string(player_id);
    if (!RunRedisCli(args, &lines, &err)) {
        return false;
    }
    if (lines.empty()) {
        return false;
    }
    const std::string stored = lines.back();
    if (stored == "(nil)") {
        return false;
    }
    return stored == token;
}

bool RedisOptionalStore::SetOnline(int64_t player_id, bool online, int64_t ttl_ms, std::string* err) {
    std::lock_guard<std::mutex> lock(mu_);
    if (player_id <= 0 || ttl_ms <= 0) {
        if (err != nullptr) {
            *err = "invalid online payload";
        }
        return false;
    }
    if (!enabled_) {
        online_by_player_[player_id] = ExpiringOnline{online, ExpireAtMsFromNow(ttl_ms)};
        return true;
    }

    std::vector<std::string> lines;
    const std::string args =
        "PSETEX online:" + std::to_string(player_id) + " " + std::to_string(ttl_ms) + " " +
        (online ? "1" : "0");
    return RunRedisCli(args, &lines, err);
}

void RedisOptionalStore::SetOnline(int64_t player_id, bool online) {
    std::string ignored;
    SetOnline(player_id, online, 7LL * 24LL * 3600LL * 1000LL, &ignored);
}

bool RedisOptionalStore::IsOnline(int64_t player_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    if (!enabled_) {
        auto it = online_by_player_.find(player_id);
        if (it == online_by_player_.end()) {
            return false;
        }
        if (IsExpired(it->second.expire_at_ms)) {
            return false;
        }
        return it->second.online;
    }

    std::vector<std::string> lines;
    std::string err;
    if (!RunRedisCli("GET online:" + std::to_string(player_id), &lines, &err) || lines.empty()) {
        return false;
    }
    return lines.back() == "1";
}

bool RedisOptionalStore::UpsertSession(int64_t player_id, int64_t room_id, int64_t ttl_ms, std::string* err) {
    std::lock_guard<std::mutex> lock(mu_);
    if (player_id <= 0 || ttl_ms <= 0) {
        if (err != nullptr) {
            *err = "invalid session payload";
        }
        return false;
    }
    if (!enabled_) {
        session_by_player_[player_id] = ExpiringSession{room_id, ExpireAtMsFromNow(ttl_ms)};
        return true;
    }

    std::vector<std::string> lines;
    const std::string args =
        "PSETEX session:" + std::to_string(player_id) + " " + std::to_string(ttl_ms) + " " +
        std::to_string(room_id);
    return RunRedisCli(args, &lines, err);
}

std::optional<int64_t> RedisOptionalStore::GetSessionRoomId(int64_t player_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    if (!enabled_) {
        auto it = session_by_player_.find(player_id);
        if (it == session_by_player_.end()) {
            return std::nullopt;
        }
        if (IsExpired(it->second.expire_at_ms)) {
            return std::nullopt;
        }
        return it->second.room_id;
    }

    std::vector<std::string> lines;
    std::string err;
    if (!RunRedisCli("GET session:" + std::to_string(player_id), &lines, &err) || lines.empty()) {
        return std::nullopt;
    }
    if (lines.back() == "(nil)") {
        return std::nullopt;
    }
    try {
        return std::stoll(lines.back());
    } catch (...) {
        return std::nullopt;
    }
}

bool RedisOptionalStore::enabled() const {
    std::lock_guard<std::mutex> lock(mu_);
    return enabled_;
}

int64_t RedisOptionalStore::ExpireAtMsFromNow(int64_t ttl_ms) {
    return NowMs() + ttl_ms;
}

bool RedisOptionalStore::IsExpired(int64_t expire_at_ms) {
    if (expire_at_ms <= 0) {
        return true;
    }
    return NowMs() > expire_at_ms;
}

std::string RedisOptionalStore::EscapeForDoubleQuotedShell(const std::string& input) {
    std::string out;
    out.reserve(input.size() * 2);
    for (char ch : input) {
        if (ch == '"' || ch == '\\') {
            out.push_back('\\');
        }
        out.push_back(ch);
    }
    return out;
}

bool RedisOptionalStore::RunRedisCli(const std::string& args, std::vector<std::string>* out_lines, std::string* err) const {
    if (out_lines != nullptr) {
        out_lines->clear();
    }
    std::ostringstream cmd;
    cmd << "docker exec " << kRedisContainer << " redis-cli " << args << " 2>&1";
    FILE* pipe = _popen(cmd.str().c_str(), "r");
    if (pipe == nullptr) {
        if (err != nullptr) {
            *err = "failed to run redis-cli";
        }
        return false;
    }
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        if (out_lines != nullptr) {
            out_lines->push_back(TrimRightNewline(buffer));
        }
    }
    const int exit_code = _pclose(pipe);
    if (exit_code != 0) {
        if (err != nullptr) {
            std::string details;
            if (out_lines != nullptr && !out_lines->empty()) {
                details = out_lines->back();
            }
            *err = "redis-cli command failed exit_code=" + std::to_string(exit_code) + " " + details;
        }
        return false;
    }
    return true;
}

}  // namespace ddz

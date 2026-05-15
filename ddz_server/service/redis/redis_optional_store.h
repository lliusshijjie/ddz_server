#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/config/config_loader.h"

namespace ddz {

class RedisOptionalStore {
public:
    bool Init(const RedisConfig& cfg, std::string* err);

    void SeedTokenForTesting(int64_t player_id, const std::string& token);
    bool StoreToken(int64_t player_id, const std::string& token, int64_t ttl_ms, std::string* err);
    bool ValidateToken(int64_t player_id, const std::string& token) const;

    bool SetOnline(int64_t player_id, bool online, int64_t ttl_ms, std::string* err);
    void SetOnline(int64_t player_id, bool online);
    bool IsOnline(int64_t player_id) const;

    bool UpsertSession(int64_t player_id, int64_t room_id, int64_t ttl_ms, std::string* err);
    std::optional<int64_t> GetSessionRoomId(int64_t player_id) const;

    bool enabled() const;

private:
    struct ExpiringToken {
        std::string token;
        int64_t expire_at_ms = 0;
    };

    struct ExpiringOnline {
        bool online = false;
        int64_t expire_at_ms = 0;
    };

    struct ExpiringSession {
        int64_t room_id = 0;
        int64_t expire_at_ms = 0;
    };

    static int64_t ExpireAtMsFromNow(int64_t ttl_ms);
    static bool IsExpired(int64_t expire_at_ms);

    bool RunRedisCli(const std::string& args, std::vector<std::string>* out_lines, std::string* err) const;
    static std::string EscapeForDoubleQuotedShell(const std::string& input);

private:
    mutable std::mutex mu_;
    bool enabled_ = false;
    RedisConfig cfg_;
    std::unordered_map<int64_t, ExpiringToken> token_by_player_;
    std::unordered_map<int64_t, ExpiringOnline> online_by_player_;
    std::unordered_map<int64_t, ExpiringSession> session_by_player_;
};

}  // namespace ddz

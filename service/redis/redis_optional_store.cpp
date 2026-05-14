#include "service/redis/redis_optional_store.h"

namespace ddz {

bool RedisOptionalStore::Init(const RedisConfig& cfg, std::string* err) {
    std::lock_guard<std::mutex> lock(mu_);
    enabled_ = cfg.enabled;
    if (cfg.port == 0) {
        if (err != nullptr) {
            *err = "redis.port must be > 0";
        }
        return false;
    }
    return true;
}

void RedisOptionalStore::SeedTokenForTesting(int64_t player_id, const std::string& token) {
    std::lock_guard<std::mutex> lock(mu_);
    token_by_player_[player_id] = token;
}

bool RedisOptionalStore::ValidateToken(int64_t player_id, const std::string& token) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = token_by_player_.find(player_id);
    if (it == token_by_player_.end()) {
        return !enabled_;
    }
    return it->second == token;
}

void RedisOptionalStore::SetOnline(int64_t player_id, bool online) {
    std::lock_guard<std::mutex> lock(mu_);
    online_by_player_[player_id] = online;
}

bool RedisOptionalStore::IsOnline(int64_t player_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = online_by_player_.find(player_id);
    if (it == online_by_player_.end()) {
        return false;
    }
    return it->second;
}

bool RedisOptionalStore::enabled() const {
    std::lock_guard<std::mutex> lock(mu_);
    return enabled_;
}

}  // namespace ddz

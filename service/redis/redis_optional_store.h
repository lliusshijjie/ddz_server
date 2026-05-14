#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

#include "common/config/config_loader.h"

namespace ddz {

class RedisOptionalStore {
public:
    bool Init(const RedisConfig& cfg, std::string* err);

    void SeedTokenForTesting(int64_t player_id, const std::string& token);
    bool ValidateToken(int64_t player_id, const std::string& token) const;

    void SetOnline(int64_t player_id, bool online);
    bool IsOnline(int64_t player_id) const;
    bool enabled() const;

private:
    mutable std::mutex mu_;
    bool enabled_ = false;
    std::unordered_map<int64_t, std::string> token_by_player_;
    std::unordered_map<int64_t, bool> online_by_player_;
};

}  // namespace ddz

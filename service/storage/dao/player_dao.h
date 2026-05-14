#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace ddz {

class PlayerDao {
public:
    bool UpsertCoin(int64_t player_id, int64_t coin);
    std::optional<int64_t> GetCoin(int64_t player_id) const;
    bool Erase(int64_t player_id);

private:
    std::unordered_map<int64_t, int64_t> player_coins_;
};

}  // namespace ddz

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include "service/storage/mysql/mysql_connection_pool.h"

namespace ddz {

class PlayerDao {
public:
    void BindPool(MySqlConnectionPool* pool);
    bool UpsertCoin(int64_t player_id, int64_t coin);
    std::optional<int64_t> GetCoin(int64_t player_id) const;
    bool Erase(int64_t player_id);

private:
    MySqlConnectionPool* mysql_pool_ = nullptr;
    std::unordered_map<int64_t, int64_t> player_coins_;
};

}  // namespace ddz

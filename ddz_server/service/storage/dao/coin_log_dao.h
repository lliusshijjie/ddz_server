#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "service/storage/mysql/mysql_connection_pool.h"
#include "service/storage/storage_models.h"

namespace ddz {

class CoinLogDao {
public:
    void BindPool(MySqlConnectionPool* pool);
    int64_t Insert(const CoinLog& log);
    std::vector<CoinLog> GetByRoomId(int64_t room_id) const;
    bool DeleteById(int64_t id);

private:
    MySqlConnectionPool* mysql_pool_ = nullptr;
    int64_t next_coin_log_id_ = 1;
    std::unordered_map<int64_t, CoinLog> coin_logs_;
};

}  // namespace ddz

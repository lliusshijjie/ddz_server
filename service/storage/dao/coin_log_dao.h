#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "service/storage/storage_models.h"

namespace ddz {

class CoinLogDao {
public:
    int64_t Insert(const CoinLog& log);
    std::vector<CoinLog> GetByRoomId(int64_t room_id) const;
    bool DeleteById(int64_t id);

private:
    int64_t next_coin_log_id_ = 1;
    std::unordered_map<int64_t, CoinLog> coin_logs_;
};

}  // namespace ddz

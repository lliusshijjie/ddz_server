#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

#include "service/storage/storage_models.h"

namespace ddz {

class GameRecordDao {
public:
    int64_t Insert(const GameRecord& record);
    std::optional<GameRecord> GetById(int64_t record_id) const;
    bool DeleteById(int64_t record_id);

private:
    int64_t next_record_id_ = 1;
    std::unordered_map<int64_t, GameRecord> game_records_;
};

}  // namespace ddz

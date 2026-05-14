#include "service/storage/dao/game_record_dao.h"

namespace ddz {

int64_t GameRecordDao::Insert(const GameRecord& record) {
    GameRecord copy = record;
    copy.record_id = next_record_id_++;
    game_records_[copy.record_id] = copy;
    return copy.record_id;
}

std::optional<GameRecord> GameRecordDao::GetById(int64_t record_id) const {
    auto it = game_records_.find(record_id);
    if (it == game_records_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool GameRecordDao::DeleteById(int64_t record_id) {
    return game_records_.erase(record_id) > 0;
}

}  // namespace ddz

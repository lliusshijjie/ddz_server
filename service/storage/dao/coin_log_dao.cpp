#include "service/storage/dao/coin_log_dao.h"

namespace ddz {

int64_t CoinLogDao::Insert(const CoinLog& log) {
    CoinLog copy = log;
    copy.id = next_coin_log_id_++;
    coin_logs_[copy.id] = copy;
    return copy.id;
}

std::vector<CoinLog> CoinLogDao::GetByRoomId(int64_t room_id) const {
    std::vector<CoinLog> out;
    for (const auto& item : coin_logs_) {
        if (item.second.room_id == room_id) {
            out.push_back(item.second);
        }
    }
    return out;
}

bool CoinLogDao::DeleteById(int64_t id) {
    return coin_logs_.erase(id) > 0;
}

}  // namespace ddz

#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "service/login/login_service.h"
#include "service/player/player_manager.h"
#include "service/room/room_manager.h"
#include "service/session/session_manager.h"
#include "service/storage/storage_service.h"

namespace ddz {

struct SettlementRequest {
    int64_t room_id = 0;
    int64_t winner_player_id = 0;
    int64_t base_coin = 0;
};

struct SettlementPlayerResult {
    int64_t player_id = 0;
    int64_t change_amount = 0;
    int64_t after_coin = 0;
};

struct SettlementResult {
    ErrorCode code = ErrorCode::UNKNOWN_ERROR;
    int64_t room_id = 0;
    int64_t winner_player_id = 0;
    int64_t record_id = 0;
    std::vector<SettlementPlayerResult> players;
};

class SettlementService {
public:
    SettlementService(SessionManager& session_manager,
                      PlayerManager& player_manager,
                      RoomManager& room_manager,
                      StorageService& storage_service)
        : session_manager_(session_manager),
          player_manager_(player_manager),
          room_manager_(room_manager),
          storage_service_(storage_service) {}

    SettlementResult Settle(const SettlementRequest& req, int64_t now_ms);

private:
    SessionManager& session_manager_;
    PlayerManager& player_manager_;
    RoomManager& room_manager_;
    StorageService& storage_service_;
};

}  // namespace ddz


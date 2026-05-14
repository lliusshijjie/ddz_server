#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "service/login/login_service.h"
#include "service/match/match_manager.h"
#include "service/player/player_manager.h"
#include "service/room/room_manager.h"
#include "service/session/session_manager.h"

namespace ddz {

struct MatchResult {
    ErrorCode code = ErrorCode::UNKNOWN_ERROR;
    int64_t player_id = 0;
    bool matched = false;
    int64_t room_id = 0;
    int32_t mode = 0;
    std::vector<int64_t> matched_players;
};

struct CancelMatchResult {
    ErrorCode code = ErrorCode::UNKNOWN_ERROR;
    int64_t player_id = 0;
};

class MatchService {
public:
    MatchService(SessionManager& session_manager,
                 PlayerManager& player_manager,
                 MatchManager& match_manager,
                 RoomManager& room_manager)
        : session_manager_(session_manager),
          player_manager_(player_manager),
          match_manager_(match_manager),
          room_manager_(room_manager) {}

    MatchResult HandleMatch(int64_t connection_id, const std::string& request_body, int64_t now_ms);
    CancelMatchResult HandleCancelMatch(int64_t connection_id);

private:
    static std::optional<int32_t> ParseMode(const std::string& request_body);

private:
    SessionManager& session_manager_;
    PlayerManager& player_manager_;
    MatchManager& match_manager_;
    RoomManager& room_manager_;
};

}  // namespace ddz


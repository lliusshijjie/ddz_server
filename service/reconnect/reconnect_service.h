#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "service/login/auth_token_service.h"
#include "service/login/login_service.h"
#include "service/player/player_manager.h"
#include "service/room/room_manager.h"
#include "service/session/session_manager.h"

namespace ddz {

struct ReconnectResult {
    ErrorCode code = ErrorCode::UNKNOWN_ERROR;
    int64_t player_id = 0;
    int64_t room_id = 0;
    std::optional<int64_t> old_connection_to_kick;
    std::optional<RoomSnapshot> snapshot;
};

class ReconnectService {
public:
    ReconnectService(SessionManager& session_manager,
                     AuthTokenService& auth_token_service,
                     PlayerManager& player_manager,
                     RoomManager& room_manager)
        : session_manager_(session_manager),
          auth_token_service_(auth_token_service),
          player_manager_(player_manager),
          room_manager_(room_manager) {}

    ReconnectResult HandleReconnect(int64_t connection_id, const std::string& request_body, int64_t now_ms);

private:
    static std::optional<std::string> ParseTokenFromBody(const std::string& request_body);

private:
    SessionManager& session_manager_;
    AuthTokenService& auth_token_service_;
    PlayerManager& player_manager_;
    RoomManager& room_manager_;
};

}  // namespace ddz

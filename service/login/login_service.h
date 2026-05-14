#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "service/login/auth_token_service.h"
#include "service/player/player_manager.h"
#include "service/session/session_manager.h"

namespace ddz {

enum class ErrorCode : int32_t {
    OK = 0,
    UNKNOWN_ERROR = 1,
    INVALID_PACKET = 2,
    INVALID_TOKEN = 3,
    NOT_LOGIN = 4,
    ALREADY_LOGIN = 5,
    TOKEN_EXPIRED = 6,
    INVALID_PLAYER_STATE = 1002,
    PLAYER_ALREADY_MATCHING = 1003,
    PLAYER_ALREADY_IN_ROOM = 1004,
    MATCH_MODE_INVALID = 2001,
    MATCH_CANCEL_FAILED = 2002,
    MATCH_TIMEOUT = 2003,
    ROOM_NOT_FOUND = 3001,
    SETTLEMENT_FAILED = 5002
};

struct LoginResult {
    ErrorCode code = ErrorCode::UNKNOWN_ERROR;
    int64_t player_id = 0;
    std::string nickname;
    int64_t coin = 0;
    std::string token;
    int64_t expire_at_ms = 0;
    std::optional<int64_t> old_connection_to_kick;
};

class LoginService {
public:
    LoginService(SessionManager& session_manager, PlayerManager& player_manager, AuthTokenService& auth_token_service)
        : session_manager_(session_manager), player_manager_(player_manager), auth_token_service_(auth_token_service) {}

    LoginResult HandleLogin(int64_t connection_id, const std::string& request_body, int64_t now_ms);

private:
    static std::optional<int64_t> ParsePlayerId(const std::string& request_body);

private:
    SessionManager& session_manager_;
    PlayerManager& player_manager_;
    AuthTokenService& auth_token_service_;
};

}  // namespace ddz

#include "service/login/login_service.h"

#include "common/util/kv_codec.h"

namespace ddz {

LoginResult LoginService::HandleLogin(int64_t connection_id, const std::string& request_body, int64_t now_ms) {
    LoginResult result;

    const auto kv = ParseKvBody(request_body);
    const auto token_it = kv.find("token");
    if (token_it == kv.end()) {
        result.code = ErrorCode::INVALID_TOKEN;
        return result;
    }

    const auto parsed_player_id = ParsePlayerIdFromToken(token_it->second);
    if (!parsed_player_id.has_value()) {
        result.code = ErrorCode::INVALID_TOKEN;
        return result;
    }

    const int64_t player_id = parsed_player_id.value();
    std::string nickname = "player_" + std::to_string(player_id);
    auto nick_it = kv.find("nickname");
    if (nick_it != kv.end() && !nick_it->second.empty()) {
        nickname = nick_it->second;
    }

    const int64_t coin = 1000;
    player_manager_.UpsertPlayer(player_id, nickname, coin);
    player_manager_.ForceState(player_id, PlayerState::Lobby);
    const auto old_conn = session_manager_.BindLogin(player_id, connection_id, now_ms);

    result.code = ErrorCode::OK;
    result.player_id = player_id;
    result.nickname = nickname;
    result.coin = coin;
    result.old_connection_to_kick = old_conn;
    return result;
}

std::optional<int64_t> LoginService::ParsePlayerIdFromToken(const std::string& token) {
    if (token.empty()) {
        return std::nullopt;
    }
    try {
        size_t idx = 0;
        const int64_t id = std::stoll(token, &idx);
        if (idx != token.size() || id <= 0) {
            return std::nullopt;
        }
        return id;
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace ddz


#include "service/login/login_service.h"

#include "common/util/kv_codec.h"

namespace ddz {

LoginResult LoginService::HandleLogin(int64_t connection_id, const std::string& request_body, int64_t now_ms) {
    LoginResult result;
    const auto parsed_player_id = ParsePlayerId(request_body);
    if (!parsed_player_id.has_value()) {
        result.code = ErrorCode::INVALID_PACKET;
        return result;
    }

    const auto kv = ParseKvBody(request_body);
    const int64_t player_id = parsed_player_id.value();
    std::string nickname = "player_" + std::to_string(player_id);
    auto nick_it = kv.find("nickname");
    if (nick_it != kv.end() && !nick_it->second.empty()) {
        nickname = nick_it->second;
    }

    const auto existing_player = player_manager_.GetPlayer(player_id);
    const int64_t coin = existing_player.has_value() ? existing_player->coin : 1000;
    player_manager_.UpsertPlayer(player_id, nickname, existing_player.has_value() ? 0 : coin);
    player_manager_.ForceState(player_id, PlayerState::Lobby);
    const auto old_conn = session_manager_.BindLogin(player_id, connection_id, now_ms);
    const auto token_result = auth_token_service_.Issue(player_id, now_ms);
    if (token_result.token.empty()) {
        result.code = ErrorCode::UNKNOWN_ERROR;
        return result;
    }

    result.code = ErrorCode::OK;
    result.player_id = player_id;
    result.nickname = nickname;
    result.coin = coin;
    result.token = token_result.token;
    result.expire_at_ms = token_result.expire_at_ms;
    result.old_connection_to_kick = old_conn;
    return result;
}

std::optional<int64_t> LoginService::ParsePlayerId(const std::string& request_body) {
    const auto kv = ParseKvBody(request_body);
    const auto it = kv.find("player_id");
    if (it == kv.end() || it->second.empty()) {
        return std::nullopt;
    }
    try {
        size_t idx = 0;
        const int64_t id = std::stoll(it->second, &idx);
        if (idx != it->second.size() || id <= 0) {
            return std::nullopt;
        }
        return id;
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace ddz

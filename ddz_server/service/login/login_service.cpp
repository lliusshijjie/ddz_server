#include "service/login/login_service.h"

#include "common/util/kv_codec.h"

namespace ddz {

LoginResult LoginService::HandleLogin(int64_t connection_id, const std::string& request_body, int64_t now_ms) {
    LoginResult result;
    const auto login_ticket = ParseLoginTicket(request_body);
    if (!login_ticket.has_value()) {
        result.code = ErrorCode::INVALID_PACKET;
        return result;
    }

    const auto verify = auth_token_service_.Verify(
        login_ticket.value(), now_ms, AuthTokenService::kPurposeLogin);
    if (verify.expired) {
        result.code = ErrorCode::TOKEN_EXPIRED;
        return result;
    }
    if (!verify.ok) {
        result.code = ErrorCode::INVALID_TOKEN;
        return result;
    }

    const auto kv = ParseKvBody(request_body);
    const int64_t player_id = verify.player_id;
    std::string nickname = "player_" + std::to_string(player_id);
    auto nick_it = kv.find("nickname");
    if (nick_it != kv.end() && !nick_it->second.empty()) {
        nickname = nick_it->second;
    }

    const auto old_session = session_manager_.GetSessionByPlayer(player_id);
    const int64_t old_room_id = old_session.has_value() ? old_session->room_id : 0;

    const auto existing_player = player_manager_.GetPlayer(player_id);
    const int64_t coin = existing_player.has_value() ? existing_player->coin : 1000;
    player_manager_.UpsertPlayer(player_id, nickname, existing_player.has_value() ? 0 : coin);
    if (old_room_id > 0) {
        player_manager_.ForceState(player_id, PlayerState::InRoom);
    } else {
        player_manager_.ForceState(player_id, PlayerState::Lobby);
    }
    const auto old_conn = session_manager_.BindLogin(player_id, connection_id, now_ms);
    const auto token_result = auth_token_service_.IssueSessionToken(player_id, now_ms);
    if (token_result.token.empty()) {
        result.code = ErrorCode::UNKNOWN_ERROR;
        return result;
    }

    result.code = ErrorCode::OK;
    result.player_id = player_id;
    result.nickname = nickname;
    result.coin = coin;
    result.room_id = old_room_id;
    result.reconnect_mode = (old_room_id > 0);
    result.token = token_result.token;
    result.expire_at_ms = token_result.expire_at_ms;
    result.old_connection_to_kick = old_conn;

    if (redis_store_ != nullptr && redis_store_->enabled()) {
        std::string redis_err;
        const int64_t ttl_ms = auth_token_service_.ttl_seconds() * 1000;
        if (!redis_store_->StoreToken(player_id, result.token, ttl_ms, &redis_err) ||
            !redis_store_->SetOnline(player_id, true, ttl_ms, &redis_err) ||
            !redis_store_->UpsertSession(player_id, result.room_id, ttl_ms, &redis_err)) {
            result.code = ErrorCode::UNKNOWN_ERROR;
            result.token.clear();
            result.expire_at_ms = 0;
            return result;
        }
    }

    return result;
}

std::optional<std::string> LoginService::ParseLoginTicket(const std::string& request_body) {
    const auto kv = ParseKvBody(request_body);
    const auto it = kv.find("login_ticket");
    if (it == kv.end() || it->second.empty()) {
        return std::nullopt;
    }
    return it->second;
}

}  // namespace ddz

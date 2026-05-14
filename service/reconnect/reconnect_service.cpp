#include "service/reconnect/reconnect_service.h"

#include "common/util/kv_codec.h"

namespace ddz {

ReconnectResult ReconnectService::HandleReconnect(int64_t connection_id, const std::string& request_body, int64_t now_ms) {
    ReconnectResult result;
    const auto token = ParseTokenFromBody(request_body);
    if (!token.has_value()) {
        result.code = ErrorCode::INVALID_TOKEN;
        return result;
    }

    const auto verify = auth_token_service_.Verify(token.value(), now_ms);
    if (verify.expired) {
        result.code = ErrorCode::TOKEN_EXPIRED;
        return result;
    }
    if (!verify.ok) {
        result.code = ErrorCode::INVALID_TOKEN;
        return result;
    }

    const int64_t player_id = verify.player_id;
    if (redis_store_ != nullptr && redis_store_->enabled() && !redis_store_->ValidateToken(player_id, token.value())) {
        result.code = ErrorCode::INVALID_TOKEN;
        return result;
    }

    const auto session = session_manager_.GetSessionByPlayer(player_id);
    if (!session.has_value()) {
        result.code = ErrorCode::NOT_LOGIN;
        return result;
    }

    result.old_connection_to_kick = session_manager_.RebindForReconnect(player_id, connection_id, now_ms);
    result.player_id = player_id;
    result.room_id = session->room_id;
    result.code = ErrorCode::OK;

    if (session->room_id > 0) {
        player_manager_.ForceState(player_id, PlayerState::InRoom);
        result.snapshot = room_manager_.BuildSnapshotByRoomId(session->room_id);
    } else {
        player_manager_.ForceState(player_id, PlayerState::Lobby);
    }

    if (redis_store_ != nullptr && redis_store_->enabled()) {
        const int64_t ttl_ms = auth_token_service_.ttl_seconds() * 1000;
        std::string redis_err;
        if (!redis_store_->SetOnline(player_id, true, ttl_ms, &redis_err) ||
            !redis_store_->UpsertSession(player_id, result.room_id, ttl_ms, &redis_err)) {
            result.code = ErrorCode::UNKNOWN_ERROR;
        }
    }

    return result;
}

std::optional<std::string> ReconnectService::ParseTokenFromBody(const std::string& request_body) {
    const auto kv = ParseKvBody(request_body);
    const auto token_it = kv.find("token");
    if (token_it == kv.end() || token_it->second.empty()) {
        return std::nullopt;
    }
    return token_it->second;
}

}  // namespace ddz

#include "service/reconnect/reconnect_service.h"

#include "common/util/kv_codec.h"

namespace ddz {
namespace {

struct ParsedReconnectRequest {
    int64_t player_id = 0;
    std::string token;
    int64_t room_id = 0;
    int64_t last_snapshot_version = 0;
};

bool ParseI64Strict(const std::string& text, int64_t* out) {
    try {
        size_t idx = 0;
        const int64_t v = std::stoll(text, &idx);
        if (idx != text.size()) {
            return false;
        }
        *out = v;
        return true;
    } catch (...) {
        return false;
    }
}

std::optional<ParsedReconnectRequest> ParseReconnectRequest(const std::string& request_body) {
    const auto kv = ParseKvBody(request_body);
    auto it_player = kv.find("player_id");
    auto it_token = kv.find("token");
    auto it_room = kv.find("room_id");
    auto it_ver = kv.find("last_snapshot_version");
    if (it_player == kv.end() || it_token == kv.end() || it_room == kv.end() || it_ver == kv.end()) {
        return std::nullopt;
    }
    ParsedReconnectRequest req;
    if (it_token->second.empty()) {
        return std::nullopt;
    }
    if (!ParseI64Strict(it_player->second, &req.player_id) ||
        !ParseI64Strict(it_room->second, &req.room_id) ||
        !ParseI64Strict(it_ver->second, &req.last_snapshot_version)) {
        return std::nullopt;
    }
    req.token = it_token->second;
    return req;
}

}  // namespace

ReconnectResult ReconnectService::HandleReconnect(int64_t connection_id, const std::string& request_body, int64_t now_ms) {
    ReconnectResult result;
    const auto parsed = ParseReconnectRequest(request_body);
    if (!parsed.has_value()) {
        result.code = ErrorCode::INVALID_TOKEN;
        return result;
    }

    const auto verify = auth_token_service_.Verify(
        parsed->token, now_ms, AuthTokenService::kPurposeSession);
    if (verify.expired) {
        result.code = ErrorCode::TOKEN_EXPIRED;
        return result;
    }
    if (!verify.ok) {
        result.code = ErrorCode::INVALID_TOKEN;
        return result;
    }

    const int64_t player_id = verify.player_id;
    if (player_id != parsed->player_id) {
        result.code = ErrorCode::INVALID_TOKEN;
        return result;
    }
    if (redis_store_ != nullptr && redis_store_->enabled() && !redis_store_->ValidateToken(player_id, parsed->token)) {
        result.code = ErrorCode::INVALID_TOKEN;
        return result;
    }

    const auto session = session_manager_.GetSessionByPlayer(player_id);
    if (!session.has_value()) {
        result.code = ErrorCode::NOT_LOGIN;
        return result;
    }
    if (session->room_id != parsed->room_id) {
        result.code = ErrorCode::RECONNECT_ROOM_MISMATCH;
        return result;
    }

    result.old_connection_to_kick = session_manager_.RebindForReconnect(player_id, connection_id, now_ms);
    result.player_id = player_id;
    result.room_id = session->room_id;
    result.code = ErrorCode::OK;

    if (session->room_id > 0) {
        player_manager_.ForceState(player_id, PlayerState::InRoom);
        result.snapshot = room_manager_.BuildSnapshotByRoomId(session->room_id);
        if (result.snapshot.has_value() && result.snapshot->snapshot_version != parsed->last_snapshot_version) {
            result.code = ErrorCode::SNAPSHOT_VERSION_CONFLICT;
        }
    } else {
        player_manager_.ForceState(player_id, PlayerState::Lobby);
        result.code = (parsed->last_snapshot_version == 0) ? ErrorCode::OK : ErrorCode::SNAPSHOT_VERSION_CONFLICT;
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

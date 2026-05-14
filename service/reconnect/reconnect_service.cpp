#include "service/reconnect/reconnect_service.h"

#include "common/util/kv_codec.h"

namespace ddz {

ReconnectResult ReconnectService::HandleReconnect(int64_t connection_id, const std::string& request_body, int64_t now_ms) {
    ReconnectResult result;
    const auto player_id_opt = ParsePlayerIdFromTokenBody(request_body);
    if (!player_id_opt.has_value()) {
        result.code = ErrorCode::INVALID_TOKEN;
        return result;
    }

    const int64_t player_id = player_id_opt.value();
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
    return result;
}

std::optional<int64_t> ReconnectService::ParsePlayerIdFromTokenBody(const std::string& request_body) {
    const auto kv = ParseKvBody(request_body);
    const auto token_it = kv.find("token");
    if (token_it == kv.end() || token_it->second.empty()) {
        return std::nullopt;
    }
    try {
        size_t idx = 0;
        const int64_t id = std::stoll(token_it->second, &idx);
        if (idx != token_it->second.size() || id <= 0) {
            return std::nullopt;
        }
        return id;
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace ddz


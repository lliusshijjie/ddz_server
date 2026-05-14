#include "service/match/match_service.h"

#include "common/util/kv_codec.h"

namespace ddz {

MatchResult MatchService::HandleMatch(int64_t connection_id, const std::string& request_body, int64_t now_ms) {
    MatchResult result;

    const auto player_id_opt = session_manager_.GetPlayerIdByConnection(connection_id);
    if (!player_id_opt.has_value()) {
        result.code = ErrorCode::NOT_LOGIN;
        return result;
    }
    const int64_t player_id = player_id_opt.value();
    result.player_id = player_id;

    const auto mode_opt = ParseMode(request_body);
    if (!mode_opt.has_value()) {
        result.code = ErrorCode::MATCH_MODE_INVALID;
        return result;
    }
    const int32_t mode = mode_opt.value();
    result.mode = mode;

    if (player_manager_.GetState(player_id) != PlayerState::Lobby) {
        result.code = ErrorCode::INVALID_PLAYER_STATE;
        return result;
    }

    const auto player = player_manager_.GetPlayer(player_id);
    const int64_t coin = player.has_value() ? player->coin : 0;
    const bool joined = match_manager_.Join(MatchPlayer{player_id, now_ms, coin, mode});
    if (!joined) {
        result.code = ErrorCode::PLAYER_ALREADY_MATCHING;
        return result;
    }

    if (!player_manager_.SetState(player_id, PlayerState::Matching)) {
        match_manager_.Cancel(player_id);
        result.code = ErrorCode::INVALID_PLAYER_STATE;
        return result;
    }

    const auto matched_players = match_manager_.TryPopMatchedPlayers(mode);
    if (!matched_players.has_value()) {
        result.code = ErrorCode::OK;
        return result;
    }

    const int64_t room_id = room_manager_.CreateRoom(mode, matched_players.value());
    for (const auto pid : matched_players.value()) {
        session_manager_.SetRoomId(pid, room_id);
        player_manager_.ForceState(pid, PlayerState::InRoom);
    }

    result.code = ErrorCode::OK;
    result.matched = true;
    result.room_id = room_id;
    result.matched_players = matched_players.value();
    return result;
}

CancelMatchResult MatchService::HandleCancelMatch(int64_t connection_id) {
    CancelMatchResult result;
    const auto player_id_opt = session_manager_.GetPlayerIdByConnection(connection_id);
    if (!player_id_opt.has_value()) {
        result.code = ErrorCode::NOT_LOGIN;
        return result;
    }
    result.player_id = player_id_opt.value();

    if (player_manager_.GetState(result.player_id) != PlayerState::Matching) {
        result.code = ErrorCode::INVALID_PLAYER_STATE;
        return result;
    }
    if (!match_manager_.Cancel(result.player_id)) {
        result.code = ErrorCode::MATCH_CANCEL_FAILED;
        return result;
    }
    player_manager_.ForceState(result.player_id, PlayerState::Lobby);
    result.code = ErrorCode::OK;
    return result;
}

std::optional<int32_t> MatchService::ParseMode(const std::string& request_body) {
    const auto kv = ParseKvBody(request_body);
    const auto it = kv.find("mode");
    if (it == kv.end()) {
        return std::nullopt;
    }
    try {
        const int mode = std::stoi(it->second);
        if (mode == 3 || mode == 4) {
            return mode;
        }
        return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace ddz


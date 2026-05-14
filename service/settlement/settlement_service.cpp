#include "service/settlement/settlement_service.h"

#include <algorithm>

namespace ddz {

SettlementResult SettlementService::Settle(const SettlementRequest& req, int64_t now_ms) {
    SettlementResult result;
    result.room_id = req.room_id;
    result.winner_player_id = req.winner_player_id;

    if (req.room_id <= 0 || req.winner_player_id <= 0 || req.base_coin <= 0) {
        result.code = ErrorCode::INVALID_PACKET;
        return result;
    }

    const auto room_opt = room_manager_.GetRoomById(req.room_id);
    if (!room_opt.has_value()) {
        result.code = ErrorCode::ROOM_NOT_FOUND;
        return result;
    }
    const auto& room = room_opt.value();
    const bool winner_in_room =
        std::find(room.players.begin(), room.players.end(), req.winner_player_id) != room.players.end();
    if (!winner_in_room) {
        result.code = ErrorCode::SETTLEMENT_FAILED;
        return result;
    }

    const int64_t winner_gain = req.base_coin * static_cast<int64_t>(room.players.size() - 1);

    GameRecord record;
    record.room_id = room.room_id;
    record.game_mode = room.mode;
    record.winner_player_id = req.winner_player_id;
    record.started_at_ms = now_ms;
    record.ended_at_ms = now_ms;
    record.players = room.players;

    const int64_t record_id = storage_service_.SaveGameRecord(record);
    result.record_id = record_id;

    for (const auto player_id : room.players) {
        const int64_t delta = (player_id == req.winner_player_id) ? winner_gain : -req.base_coin;
        const int64_t before = player_manager_.GetCoin(player_id);
        int64_t after = before;
        if (!player_manager_.AddCoin(player_id, delta, &after)) {
            result.code = ErrorCode::SETTLEMENT_FAILED;
            return result;
        }
        storage_service_.SavePlayerCoin(player_id, after);

        CoinLog log;
        log.player_id = player_id;
        log.room_id = room.room_id;
        log.change_amount = delta;
        log.before_coin = before;
        log.after_coin = after;
        log.created_at_ms = now_ms;
        storage_service_.SaveCoinLog(log);

        result.players.push_back(SettlementPlayerResult{
            player_id, delta, after
        });

        session_manager_.ClearRoomId(player_id);
        player_manager_.ForceState(player_id, PlayerState::Lobby);
    }

    room_manager_.DestroyRoom(room.room_id);
    result.code = ErrorCode::OK;
    return result;
}

}  // namespace ddz


#include "service/settlement/settlement_service.h"

#include <algorithm>
#include <utility>

namespace ddz {

SettlementResult SettlementService::Settle(const SettlementRequest& req, int64_t now_ms) {
    SettlementResult result;
    result.room_id = req.room_id;
    result.winner_player_id = req.winner_player_id;

    if (req.room_id <= 0 || req.winner_player_id <= 0 || req.base_coin <= 0) {
        result.code = ErrorCode::INVALID_PACKET;
        return result;
    }

    if (!TryBeginSettlement(req.room_id)) {
        result.code = ErrorCode::SETTLEMENT_FAILED;
        return result;
    }

    do {
        const auto room_opt = room_manager_.GetRoomById(req.room_id);
        if (!room_opt.has_value()) {
            result.code = ErrorCode::ROOM_NOT_FOUND;
            break;
        }
        const auto& room = room_opt.value();
        const bool winner_in_room =
            std::find(room.players.begin(), room.players.end(), req.winner_player_id) != room.players.end();
        if (!winner_in_room) {
            result.code = ErrorCode::SETTLEMENT_FAILED;
            break;
        }

        const int64_t winner_gain = req.base_coin * static_cast<int64_t>(room.players.size() - 1);
        SettlementPersistRequest persist_req;
        persist_req.room_id = room.room_id;
        persist_req.game_mode = room.mode;
        persist_req.winner_player_id = req.winner_player_id;
        persist_req.started_at_ms = now_ms;
        persist_req.ended_at_ms = now_ms;
        persist_req.players = room.players;

        std::vector<std::pair<int64_t, int64_t>> coin_changes_for_memory;
        std::vector<int64_t> before_coins;
        before_coins.reserve(room.players.size());
        for (const auto player_id : room.players) {
            const auto player = player_manager_.GetPlayer(player_id);
            if (!player.has_value()) {
                result.code = ErrorCode::SETTLEMENT_FAILED;
                break;
            }

            const int64_t delta = (player_id == req.winner_player_id) ? winner_gain : -req.base_coin;
            const int64_t before = player->coin;
            const int64_t after = before + delta;

            persist_req.coin_changes.push_back(SettlementCoinChange{
                player_id, room.room_id, delta, before, after
            });
            coin_changes_for_memory.push_back({player_id, delta});
            before_coins.push_back(before);
        }
        if (persist_req.coin_changes.size() != room.players.size()) {
            break;
        }

        std::string persist_err;
        if (!storage_service_.PersistSettlementTransaction(persist_req, &result.record_id, &persist_err)) {
            result.code = ErrorCode::SETTLEMENT_FAILED;
            break;
        }

        std::vector<int64_t> after_coins;
        if (!player_manager_.BatchAddCoin(coin_changes_for_memory, &after_coins) ||
            after_coins.size() != room.players.size()) {
            result.code = ErrorCode::SETTLEMENT_FAILED;
            for (size_t i = 0; i < coin_changes_for_memory.size() && i < before_coins.size(); ++i) {
                const int64_t current = player_manager_.GetCoin(coin_changes_for_memory[i].first);
                const int64_t rollback_delta = before_coins[i] - current;
                int64_t ignored = 0;
                player_manager_.AddCoin(coin_changes_for_memory[i].first, rollback_delta, &ignored);
            }
            break;
        }

        result.players.clear();
        for (size_t i = 0; i < room.players.size(); ++i) {
            const auto player_id = room.players[i];
            result.players.push_back(SettlementPlayerResult{
                player_id,
                coin_changes_for_memory[i].second,
                after_coins[i]
            });
            session_manager_.ClearRoomId(player_id);
            player_manager_.ForceState(player_id, PlayerState::Lobby);
        }

        room_manager_.DestroyRoom(room.room_id);
        result.code = ErrorCode::OK;
    } while (false);

    EndSettlement(req.room_id);
    return result;
}

bool SettlementService::TryBeginSettlement(int64_t room_id) {
    std::lock_guard<std::mutex> lock(settle_mu_);
    return settling_rooms_.insert(room_id).second;
}

void SettlementService::EndSettlement(int64_t room_id) {
    std::lock_guard<std::mutex> lock(settle_mu_);
    settling_rooms_.erase(room_id);
}

}  // namespace ddz

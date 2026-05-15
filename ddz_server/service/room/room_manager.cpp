#include "service/room/room_manager.h"

#include <algorithm>
#include <array>
#include <random>
#include <sstream>
#include <unordered_map>

#include "service/rule/ddz_rule_engine.h"

namespace ddz {
namespace {

int64_t NextOperator(const Room& room, int64_t current_player_id) {
    if (room.players.empty()) {
        return 0;
    }
    auto it = std::find(room.players.begin(), room.players.end(), current_player_id);
    if (it == room.players.end()) {
        return room.players.front();
    }
    ++it;
    if (it == room.players.end()) {
        return room.players.front();
    }
    return *it;
}

void SortCards(std::vector<int32_t>* cards) {
    std::sort(cards->begin(), cards->end(), [](int32_t a, int32_t b) {
        const int32_t ra = DdzRuleEngine::CardRank(a);
        const int32_t rb = DdzRuleEngine::CardRank(b);
        if (ra != rb) {
            return ra < rb;
        }
        return a < b;
    });
}

void DealCards(Room* room) {
    if (room == nullptr || room->players.size() != 3 || room->dealt) {
        return;
    }
    std::array<int32_t, 54> deck{};
    for (int32_t i = 0; i < 54; ++i) {
        deck[static_cast<size_t>(i)] = i;
    }
    std::mt19937_64 rng(static_cast<uint64_t>(room->room_id * 7919 + 97));
    std::shuffle(deck.begin(), deck.end(), rng);

    for (const auto pid : room->players) {
        room->hands[pid].clear();
    }
    for (int32_t i = 0; i < 51; ++i) {
        const int64_t pid = room->players[static_cast<size_t>(i % 3)];
        room->hands[pid].push_back(deck[static_cast<size_t>(i)]);
    }
    room->landlord_bottom_cards = {deck[51], deck[52], deck[53]};
    for (const auto pid : room->players) {
        SortCards(&room->hands[pid]);
    }
    room->dealt = true;
}

RoomSnapshot MakeSnapshot(const Room& room) {
    RoomSnapshot snapshot;
    snapshot.room_id = room.room_id;
    snapshot.mode = room.mode;
    snapshot.room_state = static_cast<int32_t>(room.state);
    snapshot.players = room.players;
    snapshot.current_operator_player_id = room.current_operator_player_id;
    snapshot.landlord_player_id = room.landlord_player_id;
    snapshot.base_coin = room.base_coin;
    snapshot.snapshot_version = room.snapshot_version;
    snapshot.last_action_seq = room.last_action_seq;

    std::string online_bitmap;
    std::string trustee_list;
    int64_t nearest_deadline = 0;
    for (size_t i = 0; i < room.players.size(); ++i) {
        const int64_t pid = room.players[i];
        auto online_it = room.online_by_player.find(pid);
        const bool online = (online_it != room.online_by_player.end()) ? online_it->second : true;
        online_bitmap.push_back(online ? '1' : '0');

        auto trustee_it = room.trustee_by_player.find(pid);
        const bool trustee = (trustee_it != room.trustee_by_player.end()) ? trustee_it->second : false;
        if (trustee) {
            if (!trustee_list.empty()) {
                trustee_list.push_back(',');
            }
            trustee_list += std::to_string(pid);
        }

        auto off_it = room.offline_at_ms_by_player.find(pid);
        if (off_it != room.offline_at_ms_by_player.end() && off_it->second > 0) {
            const int64_t deadline = off_it->second + 120000;
            if (nearest_deadline == 0 || deadline < nearest_deadline) {
                nearest_deadline = deadline;
            }
        }
    }
    snapshot.players_online_bitmap = online_bitmap;
    snapshot.trustee_players = trustee_list;
    snapshot.nearest_offline_deadline_ms = nearest_deadline;
    return snapshot;
}

RoomPushSnapshotV2 MakePushSnapshotV2(const Room& room) {
    RoomPushSnapshotV2 v2;
    v2.base = MakeSnapshot(room);
    v2.last_play_player_id = room.last_play_player_id;
    v2.last_play_cards = room.last_play_cards;
    v2.action_deadline_ms = v2.base.nearest_offline_deadline_ms;
    if (room.state == RoomState::Playing || room.state == RoomState::Settling || room.state == RoomState::Finished) {
        v2.landlord_bottom_cards = room.landlord_bottom_cards;
    }
    v2.player_card_counts.reserve(room.players.size());
    for (const auto player_id : room.players) {
        auto it = room.hands.find(player_id);
        const int32_t card_count = (it == room.hands.end()) ? 0 : static_cast<int32_t>(it->second.size());
        v2.player_card_counts.push_back({player_id, card_count});
    }
    return v2;
}

void BumpSnapshotVersion(Room* room, bool count_as_action) {
    if (room == nullptr) {
        return;
    }
    room->snapshot_version += 1;
    if (count_as_action) {
        room->last_action_seq += 1;
    }
}

bool RemoveCards(std::vector<int32_t>* hand, const std::vector<int32_t>& cards) {
    if (hand == nullptr) {
        return false;
    }
    std::unordered_map<int32_t, int32_t> cnt;
    for (const int32_t c : *hand) {
        cnt[c] += 1;
    }
    for (const int32_t c : cards) {
        auto it = cnt.find(c);
        if (it == cnt.end() || it->second <= 0) {
            return false;
        }
        it->second -= 1;
    }
    for (const int32_t c : cards) {
        auto it = std::find(hand->begin(), hand->end(), c);
        if (it == hand->end()) {
            return false;
        }
        hand->erase(it);
    }
    return true;
}

}  // namespace

int64_t RoomManager::CreateRoom(int32_t mode, const std::vector<int64_t>& players) {
    std::lock_guard<std::mutex> lock(mu_);
    const int64_t room_id = next_room_id_++;
    Room room;
    room.room_id = room_id;
    room.mode = mode;
    room.state = RoomState::WaitStart;
    room.players = players;
    room.current_operator_player_id = players.empty() ? 0 : players.front();
    room.base_coin = 1;
    room.snapshot_version = 1;
    room.last_action_seq = 0;
    for (const auto player_id : players) {
        room.online_by_player[player_id] = true;
        room.trustee_by_player[player_id] = false;
        room.offline_at_ms_by_player[player_id] = 0;
    }
    rooms_[room_id] = room;
    for (const auto player_id : players) {
        player_to_room_[player_id] = room_id;
    }
    return room_id;
}

bool RoomManager::DestroyRoom(int64_t room_id) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) {
        return false;
    }
    for (const auto player_id : it->second.players) {
        player_to_room_.erase(player_id);
    }
    rooms_.erase(it);
    return true;
}

std::optional<Room> RoomManager::GetRoomById(int64_t room_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<Room> RoomManager::GetRoomByPlayer(int64_t player_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto pit = player_to_room_.find(player_id);
    if (pit == player_to_room_.end()) {
        return std::nullopt;
    }
    auto rit = rooms_.find(pit->second);
    if (rit == rooms_.end()) {
        return std::nullopt;
    }
    return rit->second;
}

std::optional<int64_t> RoomManager::GetRoomIdByPlayer(int64_t player_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = player_to_room_.find(player_id);
    if (it == player_to_room_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<RoomSnapshot> RoomManager::BuildSnapshotByRoomId(int64_t room_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) {
        return std::nullopt;
    }
    return MakeSnapshot(it->second);
}

std::optional<RoomSnapshot> RoomManager::BuildSnapshotByPlayerId(int64_t player_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto pit = player_to_room_.find(player_id);
    if (pit == player_to_room_.end()) {
        return std::nullopt;
    }
    auto rit = rooms_.find(pit->second);
    if (rit == rooms_.end()) {
        return std::nullopt;
    }
    return MakeSnapshot(rit->second);
}

std::optional<RoomPushSnapshotV2> RoomManager::BuildPushSnapshotV2ByRoomId(int64_t room_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) {
        return std::nullopt;
    }
    return MakePushSnapshotV2(it->second);
}

std::optional<RoomPushSnapshotV2> RoomManager::BuildPushSnapshotV2ByPlayerId(int64_t player_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto pit = player_to_room_.find(player_id);
    if (pit == player_to_room_.end()) {
        return std::nullopt;
    }
    auto rit = rooms_.find(pit->second);
    if (rit == rooms_.end()) {
        return std::nullopt;
    }
    return MakePushSnapshotV2(rit->second);
}

std::optional<PlayerActionResult> RoomManager::ApplyPlayerAction(int64_t room_id,
                                                                 int64_t player_id,
                                                                 const PlayerActionRequest& request,
                                                                 std::string* err) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) {
        if (err != nullptr) {
            *err = "room not found";
        }
        return std::nullopt;
    }
    Room& room = it->second;

    if (std::find(room.players.begin(), room.players.end(), player_id) == room.players.end()) {
        if (err != nullptr) {
            *err = "player not in room";
        }
        return std::nullopt;
    }
    if (room.current_operator_player_id != player_id) {
        if (err != nullptr) {
            *err = "not player turn";
        }
        return std::nullopt;
    }
    if (room.state == RoomState::Settling || room.state == RoomState::Finished) {
        if (err != nullptr) {
            *err = "room state not allow action";
        }
        return std::nullopt;
    }

    if (room.state == RoomState::WaitStart) {
        room.state = RoomState::CallScore;
        DealCards(&room);
    }

    PlayerActionResult result;
    if (room.state == RoomState::CallScore) {
        if (request.action_type != PlayerActionType::CallScore) {
            if (err != nullptr) {
                *err = "action not allowed in CALL_SCORE";
            }
            return std::nullopt;
        }
        if (request.call_score < 0 || request.call_score > 3) {
            if (err != nullptr) {
                *err = "invalid call score";
            }
            return std::nullopt;
        }
        if (request.call_score > room.highest_call_score) {
            room.highest_call_score = request.call_score;
            room.highest_call_player_id = player_id;
        }

        room.state_action_count += 1;
        if (request.call_score == 3 || room.state_action_count >= static_cast<int32_t>(room.players.size())) {
            room.state = RoomState::RobLandlord;
            room.state_action_count = 0;
            if (room.highest_call_player_id == 0 && !room.players.empty()) {
                room.highest_call_player_id = room.players.front();
            }
            room.base_coin = std::max(1, room.highest_call_score);
            room.current_operator_player_id = room.highest_call_player_id;
        } else {
            room.current_operator_player_id = NextOperator(room, player_id);
        }
        BumpSnapshotVersion(&room, true);
        result.snapshot = MakeSnapshot(room);
        return result;
    }

    if (room.state == RoomState::RobLandlord) {
        if (request.action_type != PlayerActionType::RobLandlord) {
            if (err != nullptr) {
                *err = "action not allowed in ROB_LANDLORD";
            }
            return std::nullopt;
        }
        if (request.rob != 0 && request.rob != 1) {
            if (err != nullptr) {
                *err = "invalid rob flag";
            }
            return std::nullopt;
        }
        if (request.rob == 1) {
            room.landlord_player_id = player_id;
            room.base_coin = std::max(1, room.base_coin) * 2;
        }
        room.state_action_count += 1;
        if (room.state_action_count >= static_cast<int32_t>(room.players.size())) {
            if (room.landlord_player_id == 0) {
                room.landlord_player_id = room.highest_call_player_id;
            }
            if (room.landlord_player_id == 0 && !room.players.empty()) {
                room.landlord_player_id = room.players.front();
            }
            auto hit = room.hands.find(room.landlord_player_id);
            if (hit == room.hands.end()) {
                if (err != nullptr) {
                    *err = "landlord hand not found";
                }
                return std::nullopt;
            }
            hit->second.insert(hit->second.end(), room.landlord_bottom_cards.begin(), room.landlord_bottom_cards.end());
            SortCards(&hit->second);
            room.state = RoomState::Playing;
            room.state_action_count = 0;
            room.current_operator_player_id = room.landlord_player_id;
            room.last_play_cards.clear();
            room.last_play_player_id = 0;
            room.consecutive_pass_count = 0;
        } else {
            room.current_operator_player_id = NextOperator(room, player_id);
        }
        BumpSnapshotVersion(&room, true);
        result.snapshot = MakeSnapshot(room);
        return result;
    }

    if (room.state != RoomState::Playing) {
        if (err != nullptr) {
            *err = "room state not playable";
        }
        return std::nullopt;
    }

    if (request.action_type == PlayerActionType::Pass) {
        if (!request.cards.empty()) {
            if (err != nullptr) {
                *err = "pass should not carry cards";
            }
            return std::nullopt;
        }
        if (room.last_play_cards.empty()) {
            if (err != nullptr) {
                *err = "cannot pass on empty table";
            }
            return std::nullopt;
        }
        if (room.last_play_player_id == player_id) {
            if (err != nullptr) {
                *err = "leading player cannot pass";
            }
            return std::nullopt;
        }

        room.consecutive_pass_count += 1;
        if (room.consecutive_pass_count >= static_cast<int32_t>(room.players.size()) - 1) {
            room.current_operator_player_id = room.last_play_player_id;
            room.last_play_cards.clear();
            room.last_play_player_id = 0;
            room.consecutive_pass_count = 0;
        } else {
            room.current_operator_player_id = NextOperator(room, player_id);
        }
        BumpSnapshotVersion(&room, true);
        result.snapshot = MakeSnapshot(room);
        return result;
    }

    if (request.action_type != PlayerActionType::PlayCards) {
        if (err != nullptr) {
            *err = "action not allowed in PLAYING";
        }
        return std::nullopt;
    }
    if (request.cards.empty()) {
        if (err != nullptr) {
            *err = "play cards cannot be empty";
        }
        return std::nullopt;
    }

    auto hand_it = room.hands.find(player_id);
    if (hand_it == room.hands.end()) {
        if (err != nullptr) {
            *err = "player hand not found";
        }
        return std::nullopt;
    }

    const CardCombo play_combo = DdzRuleEngine::Analyze(request.cards);
    if (!play_combo.valid) {
        if (err != nullptr) {
            *err = "invalid card combo";
        }
        return std::nullopt;
    }

    if (!room.last_play_cards.empty() && room.last_play_player_id != player_id) {
        const CardCombo prev_combo = DdzRuleEngine::Analyze(room.last_play_cards);
        if (!DdzRuleEngine::CanBeat(play_combo, prev_combo)) {
            if (err != nullptr) {
                *err = "combo cannot beat previous";
            }
            return std::nullopt;
        }
    }

    if (!RemoveCards(&hand_it->second, request.cards)) {
        if (err != nullptr) {
            *err = "cards not in hand";
        }
        return std::nullopt;
    }

    room.last_play_cards = request.cards;
    room.last_play_player_id = player_id;
    room.consecutive_pass_count = 0;

    if (hand_it->second.empty()) {
        room.state = RoomState::Settling;
        room.current_operator_player_id = player_id;
        BumpSnapshotVersion(&room, true);
        result.snapshot = MakeSnapshot(room);
        result.game_over = true;
        result.winner_player_id = player_id;
        result.base_coin = std::max(1, room.base_coin);
        return result;
    }

    room.current_operator_player_id = NextOperator(room, player_id);
    BumpSnapshotVersion(&room, true);
    result.snapshot = MakeSnapshot(room);
    return result;
}

std::optional<RoomSnapshot> RoomManager::ApplyPlayerAction(int64_t room_id,
                                                           int64_t player_id,
                                                           PlayerActionType action_type,
                                                           std::string* err) {
    PlayerActionRequest req;
    req.action_type = action_type;
    if (action_type == PlayerActionType::CallScore) {
        req.call_score = 1;
    } else if (action_type == PlayerActionType::RobLandlord) {
        req.rob = 0;
    } else if (action_type == PlayerActionType::PlayCards) {
        const auto hand_opt = GetPlayerHand(room_id, player_id);
        if (!hand_opt.has_value() || hand_opt->empty()) {
            if (err != nullptr) {
                *err = "no cards in hand";
            }
            return std::nullopt;
        }
        req.cards = {hand_opt->front()};
    }
    const auto r = ApplyPlayerAction(room_id, player_id, req, err);
    if (!r.has_value()) {
        return std::nullopt;
    }
    return r->snapshot;
}

std::optional<std::vector<int32_t>> RoomManager::GetPlayerHand(int64_t room_id, int64_t player_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto rit = rooms_.find(room_id);
    if (rit == rooms_.end()) {
        return std::nullopt;
    }
    auto hit = rit->second.hands.find(player_id);
    if (hit == rit->second.hands.end()) {
        return std::nullopt;
    }
    return hit->second;
}

bool RoomManager::MarkPlayerOffline(int64_t player_id, int64_t offline_at_ms) {
    std::lock_guard<std::mutex> lock(mu_);
    auto pit = player_to_room_.find(player_id);
    if (pit == player_to_room_.end()) {
        return false;
    }
    auto rit = rooms_.find(pit->second);
    if (rit == rooms_.end()) {
        return false;
    }
    Room& room = rit->second;
    auto it = room.online_by_player.find(player_id);
    if (it == room.online_by_player.end()) {
        return false;
    }
    if (!it->second && room.offline_at_ms_by_player[player_id] > 0) {
        return true;
    }
    it->second = false;
    room.offline_at_ms_by_player[player_id] = offline_at_ms;
    BumpSnapshotVersion(&room, false);
    return true;
}

bool RoomManager::MarkPlayerOnline(int64_t player_id) {
    std::lock_guard<std::mutex> lock(mu_);
    auto pit = player_to_room_.find(player_id);
    if (pit == player_to_room_.end()) {
        return false;
    }
    auto rit = rooms_.find(pit->second);
    if (rit == rooms_.end()) {
        return false;
    }
    Room& room = rit->second;
    auto it = room.online_by_player.find(player_id);
    if (it == room.online_by_player.end()) {
        return false;
    }
    if (it->second && !room.trustee_by_player[player_id]) {
        room.offline_at_ms_by_player[player_id] = 0;
        return true;
    }
    it->second = true;
    room.trustee_by_player[player_id] = false;
    room.offline_at_ms_by_player[player_id] = 0;
    BumpSnapshotVersion(&room, false);
    return true;
}

bool RoomManager::MarkPlayerTrustee(int64_t player_id, bool trustee) {
    std::lock_guard<std::mutex> lock(mu_);
    auto pit = player_to_room_.find(player_id);
    if (pit == player_to_room_.end()) {
        return false;
    }
    auto rit = rooms_.find(pit->second);
    if (rit == rooms_.end()) {
        return false;
    }
    Room& room = rit->second;
    auto it = room.trustee_by_player.find(player_id);
    if (it == room.trustee_by_player.end()) {
        return false;
    }
    if (it->second == trustee) {
        return true;
    }
    it->second = trustee;
    BumpSnapshotVersion(&room, false);
    return true;
}

std::optional<PlayerActionResult> RoomManager::ApplyTrusteeAction(int64_t room_id, int64_t player_id, std::string* err) {
    PlayerActionRequest req;
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto rit = rooms_.find(room_id);
        if (rit == rooms_.end()) {
            if (err != nullptr) {
                *err = "room not found";
            }
            return std::nullopt;
        }
        const Room& room = rit->second;
        if (room.current_operator_player_id != player_id) {
            if (err != nullptr) {
                *err = "not trustee player turn";
            }
            return std::nullopt;
        }
        if (room.state == RoomState::CallScore) {
            req.action_type = PlayerActionType::CallScore;
            req.call_score = 0;
        } else if (room.state == RoomState::RobLandlord) {
            req.action_type = PlayerActionType::RobLandlord;
            req.rob = 0;
        } else if (room.state == RoomState::Playing) {
            if (room.last_play_cards.empty() || room.last_play_player_id == player_id) {
                auto hand_it = room.hands.find(player_id);
                if (hand_it == room.hands.end() || hand_it->second.empty()) {
                    if (err != nullptr) {
                        *err = "trustee hand empty";
                    }
                    return std::nullopt;
                }
                req.action_type = PlayerActionType::PlayCards;
                req.cards = {hand_it->second.front()};
            } else {
                const CardCombo prev_combo = DdzRuleEngine::Analyze(room.last_play_cards);
                bool found = false;
                auto hand_it = room.hands.find(player_id);
                if (hand_it != room.hands.end()) {
                    for (const auto c : hand_it->second) {
                        CardCombo challenge = DdzRuleEngine::Analyze({c});
                        if (challenge.valid && DdzRuleEngine::CanBeat(challenge, prev_combo)) {
                            req.action_type = PlayerActionType::PlayCards;
                            req.cards = {c};
                            found = true;
                            break;
                        }
                    }
                }
                if (!found) {
                    req.action_type = PlayerActionType::Pass;
                }
            }
        } else {
            if (err != nullptr) {
                *err = "room state not support trustee action";
            }
            return std::nullopt;
        }
    }
    return ApplyPlayerAction(room_id, player_id, req, err);
}

std::optional<int64_t> RoomManager::PickWinnerForOfflineLose(int64_t room_id, int64_t loser_player_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto rit = rooms_.find(room_id);
    if (rit == rooms_.end()) {
        return std::nullopt;
    }
    for (const auto pid : rit->second.players) {
        if (pid != loser_player_id) {
            return pid;
        }
    }
    return std::nullopt;
}

bool RoomManager::MarkSettling(int64_t room_id) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) {
        return false;
    }
    it->second.state = RoomState::Settling;
    BumpSnapshotVersion(&it->second, false);
    return true;
}

bool RoomManager::MarkFinished(int64_t room_id) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = rooms_.find(room_id);
    if (it == rooms_.end()) {
        return false;
    }
    it->second.state = RoomState::Finished;
    BumpSnapshotVersion(&it->second, false);
    return true;
}

}  // namespace ddz

#include <cassert>
#include <iostream>
#include <string>

#include "service/room/room_manager.h"

namespace {

void EnterPlaying(ddz::RoomManager& rm, int64_t room_id) {
    std::string err;
    ddz::PlayerActionRequest req;

    req.action_type = ddz::PlayerActionType::CallScore;
    req.call_score = 1;
    assert(rm.ApplyPlayerAction(room_id, 1001, req, &err).has_value());

    req.call_score = 0;
    assert(rm.ApplyPlayerAction(room_id, 1002, req, &err).has_value());
    assert(rm.ApplyPlayerAction(room_id, 1003, req, &err).has_value());

    req = ddz::PlayerActionRequest{};
    req.action_type = ddz::PlayerActionType::RobLandlord;
    req.rob = 0;
    assert(rm.ApplyPlayerAction(room_id, 1001, req, &err).has_value());
    assert(rm.ApplyPlayerAction(room_id, 1002, req, &err).has_value());
    const auto s = rm.ApplyPlayerAction(room_id, 1003, req, &err);
    assert(s.has_value());
    assert(s->snapshot.room_state == static_cast<int32_t>(ddz::RoomState::Playing));
    assert(s->snapshot.current_operator_player_id == 1001);
}

}  // namespace

int main() {
    ddz::RoomManager rm;
    const int64_t room_id = rm.CreateRoom(3, {1001, 1002, 1003});
    assert(room_id > 0);
    EnterPlaying(rm, room_id);

    std::string err;

    // 1) out of turn action
    ddz::PlayerActionRequest req;
    req.action_type = ddz::PlayerActionType::Pass;
    auto out_of_turn = rm.ApplyPlayerAction(room_id, 1002, req, &err);
    assert(!out_of_turn.has_value());

    // 2) pass on empty table (no previous play)
    auto pass_on_empty = rm.ApplyPlayerAction(room_id, 1001, req, &err);
    assert(!pass_on_empty.has_value());

    // 3) invalid combo (two unrelated cards)
    req = ddz::PlayerActionRequest{};
    req.action_type = ddz::PlayerActionType::PlayCards;
    req.cards = {0, 1};
    auto bad_combo = rm.ApplyPlayerAction(room_id, 1001, req, &err);
    assert(!bad_combo.has_value());

    // 4) cards not in hand
    req.cards = {60};
    auto forged_cards = rm.ApplyPlayerAction(room_id, 1001, req, &err);
    assert(!forged_cards.has_value());

    // 5) play valid single then attempt lower single can't beat
    auto hand_1001 = rm.GetPlayerHand(room_id, 1001);
    assert(hand_1001.has_value() && !hand_1001->empty());
    int32_t p1_card = hand_1001->back();
    req.cards = {p1_card};
    auto p1 = rm.ApplyPlayerAction(room_id, 1001, req, &err);
    assert(p1.has_value());
    assert(p1->snapshot.current_operator_player_id == 1002);

    auto hand_1002 = rm.GetPlayerHand(room_id, 1002);
    assert(hand_1002.has_value() && !hand_1002->empty());
    int32_t p2_low = hand_1002->front();
    req.cards = {p2_low};
    auto p2 = rm.ApplyPlayerAction(room_id, 1002, req, &err);
    assert(!p2.has_value());

    std::cout << "test_p4_forged_action_regression passed" << std::endl;
    return 0;
}

#include <cassert>
#include <iostream>
#include <numeric>
#include <string>

#include "service/room/room_manager.h"

int main() {
    ddz::RoomManager room_manager;
    const int64_t room_id = room_manager.CreateRoom(3, {1001, 1002, 1003});
    assert(room_id > 0);

    // Before dealing cards, all card counts should be zero and bottom cards hidden.
    {
        const auto v2 = room_manager.BuildPushSnapshotV2ByRoomId(room_id);
        assert(v2.has_value());
        assert(v2->landlord_bottom_cards.empty());
        int32_t total = 0;
        for (const auto& entry : v2->player_card_counts) {
            total += entry.second;
        }
        assert(total == 0);
    }

    std::string err;
    ddz::PlayerActionRequest req;
    req.action_type = ddz::PlayerActionType::CallScore;
    req.call_score = 3;
    const auto after_call = room_manager.ApplyPlayerAction(room_id, 1001, req, &err);
    assert(after_call.has_value());

    // Cards should be dealt once CALL_SCORE starts.
    {
        const auto v2 = room_manager.BuildPushSnapshotV2ByRoomId(room_id);
        assert(v2.has_value());
        int32_t total = 0;
        for (const auto& entry : v2->player_card_counts) {
            total += entry.second;
        }
        assert(total == 51);
        assert(v2->landlord_bottom_cards.empty());
    }

    req = ddz::PlayerActionRequest{};
    req.action_type = ddz::PlayerActionType::RobLandlord;
    req.rob = 1;
    assert(room_manager.ApplyPlayerAction(room_id, 1001, req, &err).has_value());
    req.rob = 0;
    assert(room_manager.ApplyPlayerAction(room_id, 1002, req, &err).has_value());
    assert(room_manager.ApplyPlayerAction(room_id, 1003, req, &err).has_value());

    // In PLAYING state, bottom cards should be public and landlord has 20 cards.
    {
        const auto v2 = room_manager.BuildPushSnapshotV2ByRoomId(room_id);
        assert(v2.has_value());
        assert(v2->base.room_state == static_cast<int32_t>(ddz::RoomState::Playing));
        assert(v2->landlord_bottom_cards.size() == 3);
        bool landlord_checked = false;
        for (const auto& entry : v2->player_card_counts) {
            if (entry.first == v2->base.landlord_player_id) {
                assert(entry.second == 20);
                landlord_checked = true;
            }
        }
        assert(landlord_checked);
    }

    std::cout << "test_p3_room_push_v2_snapshot passed" << std::endl;
    return 0;
}

#include <cassert>
#include <iostream>

#include "service/player/player_manager.h"
#include "service/reconnect/reconnect_service.h"
#include "service/room/room_manager.h"
#include "service/session/session_manager.h"

int main() {
    ddz::SessionManager session_manager;
    ddz::PlayerManager player_manager;
    ddz::RoomManager room_manager;
    ddz::AuthTokenService auth("unit_test_secret", 10);
    ddz::ReconnectService reconnect_service(session_manager, auth, player_manager, room_manager);

    // 1) invalid token
    {
        const auto r = reconnect_service.HandleReconnect(2001, "token=abc", 1000);
        assert(r.code == ddz::ErrorCode::INVALID_TOKEN);
    }

    // 2) no session
    {
        const auto t = auth.Issue(9999, 1000);
        const auto r = reconnect_service.HandleReconnect(2002, "token=" + t.token, 1001);
        assert(r.code == ddz::ErrorCode::NOT_LOGIN);
    }

    // 3) offline reconnect with room snapshot
    {
        player_manager.UpsertPlayer(1001, "alice", 1000);
        player_manager.ForceState(1001, ddz::PlayerState::InRoom);
        session_manager.BindLogin(1001, 3001, 1002);
        const int64_t room_id = room_manager.CreateRoom(3, {1001, 1002, 1003});
        session_manager.SetRoomId(1001, room_id);
        session_manager.MarkOfflineByConnection(3001, 1003);
        player_manager.ForceState(1001, ddz::PlayerState::Offline);

        const auto token = auth.Issue(1001, 1000).token;
        const auto r = reconnect_service.HandleReconnect(3002, "token=" + token, 1004);
        assert(r.code == ddz::ErrorCode::OK);
        assert(r.player_id == 1001);
        assert(r.room_id == room_id);
        assert(!r.old_connection_to_kick.has_value());
        assert(r.snapshot.has_value());
        assert(r.snapshot->room_id == room_id);
        assert(r.snapshot->players.size() == 3);
        assert(session_manager.GetConnectionIdByPlayer(1001).value() == 3002);
        assert(player_manager.GetState(1001) == ddz::PlayerState::InRoom);
    }

    // 4) online reconnect should return old connection to kick
    {
        player_manager.UpsertPlayer(2001, "bob", 1000);
        player_manager.ForceState(2001, ddz::PlayerState::Lobby);
        session_manager.BindLogin(2001, 4001, 1010);

        const auto token = auth.Issue(2001, 1000).token;
        const auto r = reconnect_service.HandleReconnect(4002, "token=" + token, 1011);
        assert(r.code == ddz::ErrorCode::OK);
        assert(r.old_connection_to_kick.has_value());
        assert(r.old_connection_to_kick.value() == 4001);
        assert(session_manager.GetConnectionIdByPlayer(2001).value() == 4002);
        assert(player_manager.GetState(2001) == ddz::PlayerState::Lobby);
    }

    // 5) expired token
    {
        const auto token = auth.Issue(2001, 1000).token;
        const auto r = reconnect_service.HandleReconnect(5001, "token=" + token, 12001);
        assert(r.code == ddz::ErrorCode::TOKEN_EXPIRED);
    }

    std::cout << "test_p3_reconnect_service passed" << std::endl;
    return 0;
}

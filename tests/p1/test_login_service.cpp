#include <cassert>
#include <iostream>

#include "service/login/login_service.h"
#include "service/player/player_manager.h"
#include "service/session/session_manager.h"

int main() {
    ddz::SessionManager sm;
    ddz::PlayerManager pm;
    ddz::LoginService login(sm, pm);

    {
        const auto r = login.HandleLogin(10, "token=1001;nickname=alice", 100);
        assert(r.code == ddz::ErrorCode::OK);
        assert(r.player_id == 1001);
        assert(r.nickname == "alice");
        assert(!r.old_connection_to_kick.has_value());
        assert(sm.GetConnectionIdByPlayer(1001).value() == 10);
    }

    {
        const auto r = login.HandleLogin(11, "token=1001;nickname=alice2", 200);
        assert(r.code == ddz::ErrorCode::OK);
        assert(r.old_connection_to_kick.has_value());
        assert(r.old_connection_to_kick.value() == 10);
        assert(sm.GetConnectionIdByPlayer(1001).value() == 11);
    }

    {
        const auto r = login.HandleLogin(20, "token=abc;nickname=bob", 300);
        assert(r.code == ddz::ErrorCode::INVALID_TOKEN);
    }

    std::cout << "test_p1_login_service passed" << std::endl;
    return 0;
}


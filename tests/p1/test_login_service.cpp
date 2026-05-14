#include <cassert>
#include <iostream>

#include "service/login/login_service.h"
#include "service/player/player_manager.h"
#include "service/session/session_manager.h"

int main() {
    ddz::SessionManager sm;
    ddz::PlayerManager pm;
    ddz::AuthTokenService auth("unit_test_secret", 7 * 24 * 3600);
    ddz::LoginService login(sm, pm, auth);

    {
        const auto r = login.HandleLogin(10, "player_id=1001;nickname=alice", 100);
        assert(r.code == ddz::ErrorCode::OK);
        assert(r.player_id == 1001);
        assert(r.nickname == "alice");
        assert(r.coin == 1000);
        assert(!r.token.empty());
        assert(r.expire_at_ms > 100);
        assert(!r.old_connection_to_kick.has_value());
        assert(sm.GetConnectionIdByPlayer(1001).value() == 10);
        const auto verify = auth.Verify(r.token, 101);
        assert(verify.ok);
        assert(verify.player_id == 1001);
    }

    assert(pm.AddCoin(1001, 250, nullptr));

    {
        const auto r = login.HandleLogin(11, "player_id=1001;nickname=alice2", 200);
        assert(r.code == ddz::ErrorCode::OK);
        assert(r.coin == 1250);
        assert(r.old_connection_to_kick.has_value());
        assert(r.old_connection_to_kick.value() == 10);
        assert(sm.GetConnectionIdByPlayer(1001).value() == 11);
    }

    {
        const auto r = login.HandleLogin(20, "player_id=abc;nickname=bob", 300);
        assert(r.code == ddz::ErrorCode::INVALID_PACKET);
    }

    std::cout << "test_p1_login_service passed" << std::endl;
    return 0;
}

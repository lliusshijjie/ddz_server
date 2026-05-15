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
        const auto ticket = auth.IssueLoginTicket(1001, 99);
        const auto r = login.HandleLogin(10, "login_ticket=" + ticket.token + ";nickname=alice", 100);
        assert(r.code == ddz::ErrorCode::OK);
        assert(r.player_id == 1001);
        assert(r.nickname == "alice");
        assert(r.coin == 1000);
        assert(r.room_id == 0);
        assert(!r.reconnect_mode);
        assert(!r.token.empty());
        assert(r.expire_at_ms > 100);
        assert(!r.old_connection_to_kick.has_value());
        assert(sm.GetConnectionIdByPlayer(1001).value() == 10);
        const auto verify = auth.Verify(r.token, 101, ddz::AuthTokenService::kPurposeSession);
        assert(verify.ok);
        assert(verify.player_id == 1001);
        assert(verify.purpose == ddz::AuthTokenService::kPurposeSession);
    }

    assert(pm.AddCoin(1001, 250, nullptr));

    {
        const auto ticket = auth.IssueLoginTicket(1001, 199);
        const auto r = login.HandleLogin(11, "login_ticket=" + ticket.token + ";nickname=alice2", 200);
        assert(r.code == ddz::ErrorCode::OK);
        assert(r.coin == 1250);
        assert(r.old_connection_to_kick.has_value());
        assert(r.old_connection_to_kick.value() == 10);
        assert(sm.GetConnectionIdByPlayer(1001).value() == 11);
        assert(!r.reconnect_mode);
    }

    {
        sm.SetRoomId(1001, 77);
        pm.ForceState(1001, ddz::PlayerState::InRoom);
        const auto ticket = auth.IssueLoginTicket(1001, 299);
        const auto r = login.HandleLogin(12, "login_ticket=" + ticket.token + ";nickname=alice3", 300);
        assert(r.code == ddz::ErrorCode::OK);
        assert(r.room_id == 77);
        assert(r.reconnect_mode);
        assert(sm.GetSessionByPlayer(1001)->room_id == 77);
        assert(pm.GetState(1001) == ddz::PlayerState::InRoom);
    }

    {
        const auto bad_ticket = auth.IssueSessionToken(1002, 300);
        const auto r = login.HandleLogin(20, "login_ticket=" + bad_ticket.token + ";nickname=bob", 301);
        assert(r.code == ddz::ErrorCode::INVALID_TOKEN);
    }

    {
        const auto r = login.HandleLogin(21, "player_id=abc;nickname=bob", 302);
        assert(r.code == ddz::ErrorCode::INVALID_PACKET);
    }

    std::cout << "test_p1_login_service passed" << std::endl;
    return 0;
}

#include <cassert>
#include <iostream>

#include "service/session/session_manager.h"

int main() {
    ddz::SessionManager sm;

    auto old_conn = sm.BindLogin(1001, 11, 100);
    assert(!old_conn.has_value());
    assert(sm.IsLoggedInConnection(11));
    assert(sm.GetPlayerIdByConnection(11).value() == 1001);

    old_conn = sm.BindLogin(1001, 12, 200);
    assert(old_conn.has_value());
    assert(old_conn.value() == 11);
    assert(!sm.IsLoggedInConnection(11));
    assert(sm.IsLoggedInConnection(12));
    assert(sm.GetConnectionIdByPlayer(1001).value() == 12);

    assert(sm.UpdateHeartbeatByConnection(12, 300));
    auto s = sm.GetSessionByPlayer(1001);
    assert(s.has_value());
    assert(s->last_heartbeat_ms == 300);

    assert(sm.MarkOfflineByConnection(12, 400));
    s = sm.GetSessionByPlayer(1001);
    assert(s.has_value());
    assert(!s->online);

    std::cout << "test_p1_session_manager passed" << std::endl;
    return 0;
}


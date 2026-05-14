#include <cassert>
#include <cstdint>
#include <iostream>

#include "network/dispatcher.h"

int main() {
    constexpr uint32_t MSG_HEARTBEAT_REQ = 1101;

    ddz::Dispatcher dispatcher;
    dispatcher.Register(MSG_HEARTBEAT_REQ, [](const ddz::Packet& request, ddz::Packet& response, int64_t connection_id) {
        assert(connection_id == 7);
        response.msg_id = 1102;
        response.seq_id = request.seq_id;
        response.player_id = request.player_id;
        response.body = "ok";
        return true;
    });

    ddz::Packet req;
    req.msg_id = MSG_HEARTBEAT_REQ;
    req.seq_id = 99;
    req.player_id = 12345;

    ddz::Packet resp;
    const bool handled = dispatcher.Dispatch(req, resp, 7);
    assert(handled);
    assert(resp.msg_id == 1102);
    assert(resp.seq_id == req.seq_id);
    assert(resp.player_id == req.player_id);
    assert(resp.body == "ok");

    ddz::Packet unknown_req;
    unknown_req.msg_id = 9999;
    unknown_req.seq_id = 1;

    ddz::Packet unknown_resp;
    const bool unknown_handled = dispatcher.Dispatch(unknown_req, unknown_resp, 7);
    assert(unknown_handled);
    assert(unknown_resp.msg_id == 10000);

    std::cout << "test_p0_dispatcher_heartbeat passed" << std::endl;
    return 0;
}

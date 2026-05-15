#include "network/dispatcher.h"

#include <exception>

#include "log/logger.h"

namespace ddz {

void Dispatcher::Register(uint32_t msg_id, Handler handler) {
    std::lock_guard<std::mutex> lock(mu_);
    handlers_[msg_id] = std::move(handler);
}

bool Dispatcher::Dispatch(const Packet& request, Packet& response, int64_t connection_id) {
    Handler handler;
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = handlers_.find(request.msg_id);
        if (it == handlers_.end()) {
            response.msg_id = request.msg_id + 1;
            response.seq_id = request.seq_id;
            response.player_id = request.player_id;
            response.body = "ERR:UNHANDLED_MSG";
            Logger::Instance().Warn("unhandled msg_id=" + std::to_string(request.msg_id));
            return true;
        }
        handler = it->second;
    }

    try {
        return handler(request, response, connection_id);
    } catch (const std::exception& ex) {
        Logger::Instance().Error(std::string("handler exception: ") + ex.what());
    } catch (...) {
        Logger::Instance().Error("handler unknown exception");
    }

    response.msg_id = request.msg_id + 1;
    response.seq_id = request.seq_id;
    response.player_id = request.player_id;
    response.body = "ERR:INTERNAL";
    return true;
}

}  // namespace ddz


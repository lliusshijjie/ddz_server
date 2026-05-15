#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>

#include "network/packet.h"

namespace ddz {

class Dispatcher {
public:
    using Handler = std::function<bool(const Packet&, Packet&, int64_t connection_id)>;

    void Register(uint32_t msg_id, Handler handler);
    bool Dispatch(const Packet& request, Packet& response, int64_t connection_id);

private:
    std::mutex mu_;
    std::unordered_map<uint32_t, Handler> handlers_;
};

}  // namespace ddz


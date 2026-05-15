#pragma once

#include <chrono>
#include <cstdint>

namespace ddz {

inline int64_t NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace ddz


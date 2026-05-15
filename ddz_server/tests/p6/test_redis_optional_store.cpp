#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "service/redis/redis_optional_store.h"

int main() {
    ddz::RedisOptionalStore store;
    ddz::RedisConfig cfg;
    cfg.enabled = true;
    cfg.port = 6379;
    std::string err;
    if (!store.Init(cfg, &err)) {
        std::cout << "test_p6_redis_optional_store skipped: redis unavailable: " << err << std::endl;
        return 0;
    }
    assert(store.enabled());

    store.SeedTokenForTesting(1001, "token-1001");
    assert(store.ValidateToken(1001, "token-1001"));
    assert(!store.ValidateToken(1001, "token-xxxx"));
    assert(!store.ValidateToken(9999, "any"));

    assert(store.StoreToken(1002, "token-1002", 1, &err));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    assert(!store.ValidateToken(1002, "token-1002"));

    assert(store.SetOnline(1001, true, 1000, &err));
    assert(store.IsOnline(1001));
    assert(store.UpsertSession(1001, 11, 1000, &err));
    auto room_id = store.GetSessionRoomId(1001);
    assert(room_id.has_value());
    assert(room_id.value() == 11);

    store.SetOnline(1001, false);
    assert(!store.IsOnline(1001));

    ddz::RedisOptionalStore disabled_store;
    ddz::RedisConfig disabled_cfg;
    disabled_cfg.enabled = false;
    assert(disabled_store.Init(disabled_cfg, &err));
    assert(!disabled_store.enabled());
    assert(!disabled_store.ValidateToken(9999, "fallback-allow"));
    assert(disabled_store.StoreToken(1003, "token-1003", 1, &err));
    assert(disabled_store.ValidateToken(1003, "token-1003"));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    assert(!disabled_store.ValidateToken(1003, "token-1003"));

    std::cout << "test_p6_redis_optional_store passed" << std::endl;
    return 0;
}

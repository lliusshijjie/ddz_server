#include <cassert>
#include <iostream>
#include <string>

#include "service/redis/redis_optional_store.h"

int main() {
    ddz::RedisOptionalStore store;
    ddz::RedisConfig cfg;
    cfg.enabled = true;
    cfg.port = 6379;
    std::string err;
    assert(store.Init(cfg, &err));
    assert(store.enabled());

    store.SeedTokenForTesting(1001, "token-1001");
    assert(store.ValidateToken(1001, "token-1001"));
    assert(!store.ValidateToken(1001, "token-xxxx"));
    assert(!store.ValidateToken(9999, "any"));

    store.SetOnline(1001, true);
    assert(store.IsOnline(1001));
    store.SetOnline(1001, false);
    assert(!store.IsOnline(1001));

    ddz::RedisOptionalStore disabled_store;
    ddz::RedisConfig disabled_cfg;
    disabled_cfg.enabled = false;
    assert(disabled_store.Init(disabled_cfg, &err));
    assert(!disabled_store.enabled());
    assert(disabled_store.ValidateToken(9999, "fallback-allow"));

    std::cout << "test_p6_redis_optional_store passed" << std::endl;
    return 0;
}

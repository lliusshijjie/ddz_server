#include "service/storage/mysql/mysql_connection_pool.h"

namespace ddz {

bool MySqlConnectionPool::Init(const MysqlRuntimeConfig& cfg, std::string* err) {
    std::lock_guard<std::mutex> lock(mu_);
    enabled_ = cfg.enabled;
    if (cfg.pool_size <= 0) {
        if (err != nullptr) {
            *err = "mysql.pool_size must be > 0";
        }
        return false;
    }
    in_use_.assign(static_cast<size_t>(cfg.pool_size), false);
    return true;
}

MySqlConnectionPool::Lease MySqlConnectionPool::Acquire() {
    std::lock_guard<std::mutex> lock(mu_);
    if (in_use_.empty()) {
        return Lease{-1, true};
    }
    for (size_t i = 0; i < in_use_.size(); ++i) {
        if (!in_use_[i]) {
            in_use_[i] = true;
            return Lease{static_cast<int32_t>(i), true};
        }
    }
    return Lease{};
}

void MySqlConnectionPool::Release(const Lease& lease) {
    if (!lease.valid || lease.slot < 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(mu_);
    const size_t idx = static_cast<size_t>(lease.slot);
    if (idx < in_use_.size()) {
        in_use_[idx] = false;
    }
}

bool MySqlConnectionPool::enabled() const {
    std::lock_guard<std::mutex> lock(mu_);
    return enabled_;
}

int MySqlConnectionPool::size() const {
    std::lock_guard<std::mutex> lock(mu_);
    return static_cast<int>(in_use_.size());
}

}  // namespace ddz

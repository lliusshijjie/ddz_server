#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ddz {

struct MysqlRuntimeConfig {
    bool enabled = false;
    std::string host = "127.0.0.1";
    uint16_t port = 3306;
    std::string user = "ddz";
    std::string password = "ddz_pass";
    std::string database = "ddz";
    int pool_size = 4;
};

class MySqlConnectionPool {
public:
    struct Lease {
        int32_t slot = -1;
        bool valid = false;
    };

    bool Init(const MysqlRuntimeConfig& cfg, std::string* err);
    Lease Acquire();
    void Release(const Lease& lease);
    bool enabled() const;
    int size() const;

private:
    mutable std::mutex mu_;
    bool enabled_ = false;
    std::vector<bool> in_use_;
};

}  // namespace ddz

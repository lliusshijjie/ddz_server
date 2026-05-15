#pragma once

#include <cstdint>
#include <mutex>
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
    bool ExecuteSql(const Lease& lease, const std::string& sql, std::vector<std::string>* out_lines, std::string* err) const;
    bool ExecuteSql(const std::string& sql, std::vector<std::string>* out_lines, std::string* err) const;
    bool enabled() const;
    int size() const;

private:
    static std::string EscapeForDoubleQuotedShell(const std::string& input);
    bool RunMysqlCli(const std::string& sql, std::vector<std::string>* out_lines, std::string* err) const;

private:
    mutable std::mutex mu_;
    bool enabled_ = false;
    MysqlRuntimeConfig cfg_;
    std::vector<bool> in_use_;
};

}  // namespace ddz

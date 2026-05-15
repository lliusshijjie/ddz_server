#include "service/storage/mysql/mysql_connection_pool.h"

#include <cstdio>
#include <sstream>

namespace ddz {
namespace {

constexpr const char* kMysqlContainer = "ddz_mysql";

#if defined(_WIN32)
FILE* OpenPipe(const char* command, const char* mode) {
    return _popen(command, mode);
}

int ClosePipe(FILE* pipe) {
    return _pclose(pipe);
}
#else
FILE* OpenPipe(const char* command, const char* mode) {
    return popen(command, mode);
}

int ClosePipe(FILE* pipe) {
    return pclose(pipe);
}
#endif

std::string TrimRightNewline(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
        s.pop_back();
    }
    return s;
}

}  // namespace

bool MySqlConnectionPool::Init(const MysqlRuntimeConfig& cfg, std::string* err) {
    std::lock_guard<std::mutex> lock(mu_);
    cfg_ = cfg;
    enabled_ = cfg.enabled;
    if (cfg.pool_size <= 0) {
        if (err != nullptr) {
            *err = "mysql.pool_size must be > 0";
        }
        return false;
    }
    in_use_.assign(static_cast<size_t>(cfg.pool_size), false);
    if (!enabled_) {
        return true;
    }

    std::vector<std::string> lines;
    if (!RunMysqlCli("SELECT 1;", &lines, err)) {
        return false;
    }
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

bool MySqlConnectionPool::ExecuteSql(const Lease& lease,
                                     const std::string& sql,
                                     std::vector<std::string>* out_lines,
                                     std::string* err) const {
    if (!lease.valid) {
        if (err != nullptr) {
            *err = "invalid mysql lease";
        }
        return false;
    }
    return ExecuteSql(sql, out_lines, err);
}

bool MySqlConnectionPool::ExecuteSql(const std::string& sql, std::vector<std::string>* out_lines, std::string* err) const {
    std::lock_guard<std::mutex> lock(mu_);
    if (!enabled_) {
        if (err != nullptr) {
            *err = "mysql disabled";
        }
        return false;
    }
    return RunMysqlCli(sql, out_lines, err);
}

bool MySqlConnectionPool::enabled() const {
    std::lock_guard<std::mutex> lock(mu_);
    return enabled_;
}

int MySqlConnectionPool::size() const {
    std::lock_guard<std::mutex> lock(mu_);
    return static_cast<int>(in_use_.size());
}

std::string MySqlConnectionPool::EscapeForDoubleQuotedShell(const std::string& input) {
    std::string out;
    out.reserve(input.size() * 2);
    for (char ch : input) {
        if (ch == '"' || ch == '\\') {
            out.push_back('\\');
        }
        out.push_back(ch);
    }
    return out;
}

bool MySqlConnectionPool::RunMysqlCli(const std::string& sql, std::vector<std::string>* out_lines, std::string* err) const {
    if (out_lines != nullptr) {
        out_lines->clear();
    }
    const std::string escaped_sql = EscapeForDoubleQuotedShell(sql);
    std::ostringstream cmd;
    cmd << "docker exec -e MYSQL_PWD=" << cfg_.password
        << " " << kMysqlContainer
        << " mysql -N -B -u" << cfg_.user
        << " " << cfg_.database
        << " -e \"" << escaped_sql << "\" 2>&1";

    FILE* pipe = OpenPipe(cmd.str().c_str(), "r");
    if (pipe == nullptr) {
        if (err != nullptr) {
            *err = "failed to run mysql cli";
        }
        return false;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        const std::string line = TrimRightNewline(buffer);
        if (out_lines != nullptr) {
            out_lines->push_back(line);
        }
    }

    const int exit_code = ClosePipe(pipe);
    if (exit_code != 0) {
        if (err != nullptr) {
            std::string details;
            if (out_lines != nullptr && !out_lines->empty()) {
                details = out_lines->back();
            }
            *err = "mysql cli command failed exit_code=" + std::to_string(exit_code) + " " + details;
        }
        return false;
    }
    return true;
}

}  // namespace ddz

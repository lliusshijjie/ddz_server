#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace ddz {

struct TokenIssueResult {
    std::string token;
    int64_t expire_at_ms = 0;
};

struct TokenVerifyResult {
    bool ok = false;
    bool expired = false;
    int64_t player_id = 0;
};

class AuthTokenService {
public:
    AuthTokenService() = default;
    AuthTokenService(std::string secret, int64_t ttl_seconds);

    void Configure(std::string secret, int64_t ttl_seconds);
    TokenIssueResult Issue(int64_t player_id, int64_t now_ms) const;
    TokenVerifyResult Verify(const std::string& token, int64_t now_ms) const;

private:
    static std::string GenerateNonce(int64_t player_id, int64_t now_ms);
    std::string Sign(const std::string& payload) const;

private:
    std::string secret_ = "dev_change_me";
    int64_t ttl_seconds_ = 604800;
};

}  // namespace ddz


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
    std::string purpose;
};

class AuthTokenService {
public:
    static constexpr const char* kPurposeLogin = "login";
    static constexpr const char* kPurposeSession = "session";

    AuthTokenService() = default;
    AuthTokenService(std::string secret, int64_t ttl_seconds);

    void Configure(std::string secret, int64_t ttl_seconds);
    TokenIssueResult IssueLoginTicket(int64_t player_id, int64_t now_ms) const;
    TokenIssueResult IssueSessionToken(int64_t player_id, int64_t now_ms) const;
    TokenIssueResult Issue(int64_t player_id, int64_t now_ms) const;
    TokenVerifyResult Verify(const std::string& token, int64_t now_ms) const;
    TokenVerifyResult Verify(const std::string& token, int64_t now_ms, const std::string& required_purpose) const;
    int64_t ttl_seconds() const;

private:
    TokenIssueResult IssueWithPurpose(int64_t player_id, int64_t now_ms, const std::string& purpose) const;
    static std::string GenerateNonce(int64_t player_id, int64_t now_ms);
    static bool IsValidPurpose(const std::string& purpose);
    std::string Sign(const std::string& payload) const;

private:
    std::string secret_ = "dev_change_me";
    int64_t ttl_seconds_ = 604800;
};

}  // namespace ddz

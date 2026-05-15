#include "service/login/auth_token_service.h"

#include <atomic>
#include <sstream>
#include <utility>
#include <vector>

#include "common/util/sha256.h"

namespace ddz {
namespace {

std::vector<std::string> Split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::string cur;
    for (const char ch : s) {
        if (ch == delim) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    out.push_back(cur);
    return out;
}

}  // namespace

AuthTokenService::AuthTokenService(std::string secret, int64_t ttl_seconds) {
    Configure(std::move(secret), ttl_seconds);
}

void AuthTokenService::Configure(std::string secret, int64_t ttl_seconds) {
    if (!secret.empty()) {
        secret_ = std::move(secret);
    }
    if (ttl_seconds > 0) {
        ttl_seconds_ = ttl_seconds;
    }
}

TokenIssueResult AuthTokenService::IssueLoginTicket(int64_t player_id, int64_t now_ms) const {
    return IssueWithPurpose(player_id, now_ms, kPurposeLogin);
}

TokenIssueResult AuthTokenService::IssueSessionToken(int64_t player_id, int64_t now_ms) const {
    return IssueWithPurpose(player_id, now_ms, kPurposeSession);
}

TokenIssueResult AuthTokenService::Issue(int64_t player_id, int64_t now_ms) const {
    return IssueSessionToken(player_id, now_ms);
}

TokenIssueResult AuthTokenService::IssueWithPurpose(int64_t player_id, int64_t now_ms, const std::string& purpose) const {
    TokenIssueResult result;
    if (player_id <= 0 || !IsValidPurpose(purpose)) {
        return result;
    }
    result.expire_at_ms = now_ms + ttl_seconds_ * 1000;
    const std::string nonce = GenerateNonce(player_id, now_ms);
    const std::string payload =
        "v2." + purpose + "." + std::to_string(player_id) + "." + std::to_string(result.expire_at_ms) + "." + nonce;
    const std::string sig = Sign(payload);
    result.token = payload + "." + sig;
    return result;
}

TokenVerifyResult AuthTokenService::Verify(const std::string& token, int64_t now_ms) const {
    return Verify(token, now_ms, "");
}

TokenVerifyResult AuthTokenService::Verify(const std::string& token, int64_t now_ms, const std::string& required_purpose) const {
    TokenVerifyResult result;
    if (token.empty()) {
        return result;
    }
    const auto parts = Split(token, '.');

    std::string purpose;
    std::string payload;
    std::string sig;
    int64_t player_id = 0;
    int64_t expire_at_ms = 0;

    // Backward compatibility for old session tokens.
    if (parts.size() == 5 && parts[0] == "v1") {
        purpose = kPurposeSession;
        payload = parts[0] + "." + parts[1] + "." + parts[2] + "." + parts[3];
        sig = parts[4];
        try {
            size_t idx1 = 0;
            size_t idx2 = 0;
            player_id = std::stoll(parts[1], &idx1);
            expire_at_ms = std::stoll(parts[2], &idx2);
            if (idx1 != parts[1].size() || idx2 != parts[2].size() || player_id <= 0 || expire_at_ms <= 0) {
                return result;
            }
        } catch (...) {
            return result;
        }
    } else if (parts.size() == 6 && parts[0] == "v2") {
        purpose = parts[1];
        if (!IsValidPurpose(purpose)) {
            return result;
        }
        payload = parts[0] + "." + parts[1] + "." + parts[2] + "." + parts[3] + "." + parts[4];
        sig = parts[5];
        try {
            size_t idx1 = 0;
            size_t idx2 = 0;
            player_id = std::stoll(parts[2], &idx1);
            expire_at_ms = std::stoll(parts[3], &idx2);
            if (idx1 != parts[2].size() || idx2 != parts[3].size() || player_id <= 0 || expire_at_ms <= 0) {
                return result;
            }
        } catch (...) {
            return result;
        }
    } else {
        return result;
    }

    if (!required_purpose.empty() && purpose != required_purpose) {
        return result;
    }

    const std::string expected_sig = Sign(payload);
    if (sig != expected_sig) {
        return result;
    }

    if (now_ms > expire_at_ms) {
        result.expired = true;
        return result;
    }

    result.ok = true;
    result.player_id = player_id;
    result.purpose = purpose;
    return result;
}

int64_t AuthTokenService::ttl_seconds() const {
    return ttl_seconds_;
}

std::string AuthTokenService::GenerateNonce(int64_t player_id, int64_t now_ms) {
    static std::atomic<uint64_t> seq{1};
    const uint64_t s = seq.fetch_add(1);
    return std::to_string(player_id) + "_" + std::to_string(now_ms) + "_" + std::to_string(s);
}

bool AuthTokenService::IsValidPurpose(const std::string& purpose) {
    if (purpose.empty()) {
        return false;
    }
    for (char ch : purpose) {
        const bool ok = (ch >= 'a' && ch <= 'z') ||
                        (ch >= 'A' && ch <= 'Z') ||
                        (ch >= '0' && ch <= '9') ||
                        ch == '_' || ch == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}

std::string AuthTokenService::Sign(const std::string& payload) const {
    return Sha256Hex(secret_ + "|" + payload + "|" + secret_);
}

}  // namespace ddz

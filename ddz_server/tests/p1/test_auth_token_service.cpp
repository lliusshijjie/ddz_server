#include <cassert>
#include <iostream>
#include <string>

#include "service/login/auth_token_service.h"

int main() {
    ddz::AuthTokenService auth("unit_test_secret", 10);

    const auto login_ticket = auth.IssueLoginTicket(1001, 1000);
    assert(!login_ticket.token.empty());
    assert(login_ticket.expire_at_ms == 11000);

    auto login_verify = auth.Verify(login_ticket.token, 1001, ddz::AuthTokenService::kPurposeLogin);
    assert(login_verify.ok);
    assert(!login_verify.expired);
    assert(login_verify.player_id == 1001);
    assert(login_verify.purpose == ddz::AuthTokenService::kPurposeLogin);

    auto wrong_purpose = auth.Verify(login_ticket.token, 1001, ddz::AuthTokenService::kPurposeSession);
    assert(!wrong_purpose.ok);
    assert(!wrong_purpose.expired);

    const auto session_token = auth.IssueSessionToken(1001, 1000);
    assert(!session_token.token.empty());

    auto session_verify = auth.Verify(session_token.token, 1001, ddz::AuthTokenService::kPurposeSession);
    assert(session_verify.ok);
    assert(session_verify.player_id == 1001);
    assert(session_verify.purpose == ddz::AuthTokenService::kPurposeSession);

    auto expired = auth.Verify(session_token.token, 11001, ddz::AuthTokenService::kPurposeSession);
    assert(!expired.ok);
    assert(expired.expired);

    std::string tampered = session_token.token;
    tampered.back() = (tampered.back() == '0') ? '1' : '0';
    auto bad = auth.Verify(tampered, 1001, ddz::AuthTokenService::kPurposeSession);
    assert(!bad.ok);
    assert(!bad.expired);

    std::cout << "test_p1_auth_token_service passed" << std::endl;
    return 0;
}

#pragma once

#include <cstdint>
#include <string>

namespace ddz {

struct GatewayEnvelope {
    uint32_t msg_id = 0;
    uint32_t seq_id = 0;
    int64_t player_id = 0;
    std::string body;
};

bool ParseEnvelopeJson(const std::string& text, GatewayEnvelope* out, std::string* err);
std::string BuildEnvelopeJson(const GatewayEnvelope& envelope);

bool ParseLoginTicketRequestJson(const std::string& text, int64_t* player_id, std::string* nickname, std::string* err);
std::string BuildLoginTicketResponseJson(
    int32_t code,
    int64_t player_id,
    const std::string& login_ticket,
    int64_t expire_at_ms,
    const std::string& message);

}  // namespace ddz

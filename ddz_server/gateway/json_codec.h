#pragma once

#include <cstdint>
#include <string>

namespace ddz {

struct GatewayEnvelope {
    uint32_t msg_id = 0;
    uint32_t seq_id = 0;
    int64_t player_id = 0;
    std::string body;
    std::string h5_request_id;
    std::string gateway_trace_id;
    std::string server_trace_id;
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

std::string BuildSessionRefreshResponseJson(
    int32_t code,
    int64_t player_id,
    int64_t room_id,
    const std::string& token,
    int64_t expire_at_ms,
    const std::string& reason,
    const std::string& gateway_trace_id,
    const std::string& server_trace_id);

}  // namespace ddz

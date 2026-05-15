#include "gateway/json_codec.h"

#include <cctype>
#include <map>
#include <sstream>
#include <stdexcept>

namespace ddz {
namespace {

struct JsonValue {
    bool is_string = false;
    std::string value;
};

class JsonObjectParser {
public:
    explicit JsonObjectParser(const std::string& text) : text_(text) {}

    bool Parse(std::map<std::string, JsonValue>* out, std::string* err) {
        SkipWs();
        if (!ConsumeChar('{')) {
            if (err != nullptr) *err = "json must start with '{'";
            return false;
        }
        SkipWs();
        if (PeekChar() == '}') {
            ConsumeChar('}');
            return true;
        }
        while (pos_ < text_.size()) {
            std::string key;
            if (!ParseString(&key, err)) {
                return false;
            }
            SkipWs();
            if (!ConsumeChar(':')) {
                if (err != nullptr) *err = "missing ':'";
                return false;
            }
            SkipWs();
            JsonValue value;
            if (PeekChar() == '"') {
                value.is_string = true;
                if (!ParseString(&value.value, err)) {
                    return false;
                }
            } else {
                value.is_string = false;
                if (!ParseRawToken(&value.value, err)) {
                    return false;
                }
            }
            (*out)[key] = value;
            SkipWs();
            if (ConsumeChar(',')) {
                SkipWs();
                continue;
            }
            if (ConsumeChar('}')) {
                SkipWs();
                if (pos_ != text_.size()) {
                    if (err != nullptr) *err = "unexpected trailing characters";
                    return false;
                }
                return true;
            }
            if (err != nullptr) *err = "missing ',' or '}'";
            return false;
        }
        if (err != nullptr) *err = "unexpected end of json";
        return false;
    }

private:
    char PeekChar() const {
        if (pos_ >= text_.size()) {
            return '\0';
        }
        return text_[pos_];
    }

    bool ConsumeChar(char ch) {
        if (PeekChar() != ch) {
            return false;
        }
        ++pos_;
        return true;
    }

    void SkipWs() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_])) != 0) {
            ++pos_;
        }
    }

    bool ParseString(std::string* out, std::string* err) {
        if (!ConsumeChar('"')) {
            if (err != nullptr) *err = "expected string";
            return false;
        }
        std::string value;
        while (pos_ < text_.size()) {
            const char ch = text_[pos_++];
            if (ch == '"') {
                *out = value;
                return true;
            }
            if (ch == '\\') {
                if (pos_ >= text_.size()) {
                    if (err != nullptr) *err = "invalid escape";
                    return false;
                }
                const char esc = text_[pos_++];
                switch (esc) {
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                case '/': value.push_back('/'); break;
                case 'b': value.push_back('\b'); break;
                case 'f': value.push_back('\f'); break;
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default:
                    if (err != nullptr) *err = "unsupported escape sequence";
                    return false;
                }
                continue;
            }
            value.push_back(ch);
        }
        if (err != nullptr) *err = "unterminated string";
        return false;
    }

    bool ParseRawToken(std::string* out, std::string* err) {
        const size_t begin = pos_;
        while (pos_ < text_.size()) {
            const char ch = text_[pos_];
            if (ch == ',' || ch == '}' || std::isspace(static_cast<unsigned char>(ch)) != 0) {
                break;
            }
            ++pos_;
        }
        if (begin == pos_) {
            if (err != nullptr) *err = "empty token";
            return false;
        }
        *out = text_.substr(begin, pos_ - begin);
        return true;
    }

private:
    const std::string& text_;
    size_t pos_ = 0;
};

bool ParseI64Strict(const std::string& text, int64_t* out) {
    try {
        size_t idx = 0;
        const int64_t value = std::stoll(text, &idx);
        if (idx != text.size()) {
            return false;
        }
        *out = value;
        return true;
    } catch (...) {
        return false;
    }
}

bool ParseU32Strict(const std::string& text, uint32_t* out) {
    try {
        size_t idx = 0;
        const unsigned long long value = std::stoull(text, &idx);
        if (idx != text.size() || value > 0xFFFFFFFFULL) {
            return false;
        }
        *out = static_cast<uint32_t>(value);
        return true;
    } catch (...) {
        return false;
    }
}

std::string EscapeJson(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (const char ch : text) {
        switch (ch) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out.push_back(ch); break;
        }
    }
    return out;
}

}  // namespace

bool ParseEnvelopeJson(const std::string& text, GatewayEnvelope* out, std::string* err) {
    if (out == nullptr) {
        if (err != nullptr) *err = "output is null";
        return false;
    }
    std::map<std::string, JsonValue> fields;
    JsonObjectParser parser(text);
    if (!parser.Parse(&fields, err)) {
        return false;
    }
    const auto it_msg = fields.find("msg_id");
    const auto it_seq = fields.find("seq_id");
    const auto it_pid = fields.find("player_id");
    const auto it_body = fields.find("body");
    if (it_msg == fields.end() || it_seq == fields.end() || it_pid == fields.end() || it_body == fields.end()) {
        if (err != nullptr) *err = "missing required fields";
        return false;
    }
    uint32_t msg_id = 0;
    uint32_t seq_id = 0;
    int64_t player_id = 0;
    if (!ParseU32Strict(it_msg->second.value, &msg_id) ||
        !ParseU32Strict(it_seq->second.value, &seq_id) ||
        !ParseI64Strict(it_pid->second.value, &player_id)) {
        if (err != nullptr) *err = "invalid number field";
        return false;
    }
    if (!it_body->second.is_string) {
        if (err != nullptr) *err = "body must be string";
        return false;
    }
    out->msg_id = msg_id;
    out->seq_id = seq_id;
    out->player_id = player_id;
    out->body = it_body->second.value;
    const auto it_h5 = fields.find("h5_request_id");
    if (it_h5 != fields.end() && it_h5->second.is_string) {
        out->h5_request_id = it_h5->second.value;
    } else {
        out->h5_request_id.clear();
    }
    const auto it_gateway = fields.find("gateway_trace_id");
    if (it_gateway != fields.end() && it_gateway->second.is_string) {
        out->gateway_trace_id = it_gateway->second.value;
    } else {
        out->gateway_trace_id.clear();
    }
    const auto it_server = fields.find("server_trace_id");
    if (it_server != fields.end() && it_server->second.is_string) {
        out->server_trace_id = it_server->second.value;
    } else {
        out->server_trace_id.clear();
    }
    return true;
}

std::string BuildEnvelopeJson(const GatewayEnvelope& envelope) {
    std::ostringstream oss;
    oss << "{"
        << "\"msg_id\":" << envelope.msg_id << ","
        << "\"seq_id\":" << envelope.seq_id << ","
        << "\"player_id\":" << envelope.player_id << ","
        << "\"body\":\"" << EscapeJson(envelope.body) << "\"";
    if (!envelope.h5_request_id.empty()) {
        oss << ",\"h5_request_id\":\"" << EscapeJson(envelope.h5_request_id) << "\"";
    }
    if (!envelope.gateway_trace_id.empty()) {
        oss << ",\"gateway_trace_id\":\"" << EscapeJson(envelope.gateway_trace_id) << "\"";
    }
    if (!envelope.server_trace_id.empty()) {
        oss << ",\"server_trace_id\":\"" << EscapeJson(envelope.server_trace_id) << "\"";
    }
    oss
        << "}";
    return oss.str();
}

bool ParseLoginTicketRequestJson(const std::string& text, int64_t* player_id, std::string* nickname, std::string* err) {
    if (player_id == nullptr || nickname == nullptr) {
        if (err != nullptr) *err = "invalid output arguments";
        return false;
    }
    std::map<std::string, JsonValue> fields;
    JsonObjectParser parser(text);
    if (!parser.Parse(&fields, err)) {
        return false;
    }
    const auto it_pid = fields.find("player_id");
    if (it_pid == fields.end()) {
        if (err != nullptr) *err = "missing player_id";
        return false;
    }
    if (!ParseI64Strict(it_pid->second.value, player_id) || *player_id <= 0) {
        if (err != nullptr) *err = "invalid player_id";
        return false;
    }
    auto it_nickname = fields.find("nickname");
    if (it_nickname != fields.end() && it_nickname->second.is_string) {
        *nickname = it_nickname->second.value;
    } else {
        nickname->clear();
    }
    return true;
}

std::string BuildLoginTicketResponseJson(
    int32_t code, int64_t player_id, const std::string& login_ticket, int64_t expire_at_ms, const std::string& message) {
    std::ostringstream oss;
    oss << "{"
        << "\"code\":" << code << ","
        << "\"player_id\":" << player_id << ","
        << "\"login_ticket\":\"" << EscapeJson(login_ticket) << "\","
        << "\"expire_at_ms\":" << expire_at_ms << ","
        << "\"message\":\"" << EscapeJson(message) << "\""
        << "}";
    return oss.str();
}

std::string BuildSessionRefreshResponseJson(
    int32_t code,
    int64_t player_id,
    int64_t room_id,
    const std::string& token,
    int64_t expire_at_ms,
    const std::string& reason,
    const std::string& gateway_trace_id,
    const std::string& server_trace_id) {
    std::ostringstream oss;
    oss << "{"
        << "\"code\":" << code << ","
        << "\"player_id\":" << player_id << ","
        << "\"room_id\":" << room_id << ","
        << "\"token\":\"" << EscapeJson(token) << "\","
        << "\"expire_at_ms\":" << expire_at_ms << ","
        << "\"reason\":\"" << EscapeJson(reason) << "\"";
    if (!gateway_trace_id.empty()) {
        oss << ",\"gateway_trace_id\":\"" << EscapeJson(gateway_trace_id) << "\"";
    }
    if (!server_trace_id.empty()) {
        oss << ",\"server_trace_id\":\"" << EscapeJson(server_trace_id) << "\"";
    }
    oss << "}";
    return oss.str();
}

}  // namespace ddz

#include "gateway/gateway_config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace ddz {
namespace {

std::string Trim(const std::string& s) {
    const auto begin = std::find_if_not(s.begin(), s.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
    if (begin == s.end()) {
        return "";
    }
    const auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
    return std::string(begin, end);
}

std::string StripComment(const std::string& s) {
    const auto pos = s.find('#');
    return pos == std::string::npos ? s : s.substr(0, pos);
}

std::string Unquote(std::string s) {
    s = Trim(s);
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

bool ParseBool(const std::string& s) {
    std::string v = s;
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

bool StartsWith(const std::string& text, const std::string& prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

std::string NormalizeOrigin(std::string value) {
    value = Trim(value);
    if (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

}  // namespace

bool GatewayConfigLoader::LoadFromFile(const std::string& path, GatewayConfig& out, std::string* err) {
    std::ifstream in(path);
    if (!in.is_open()) {
        if (err != nullptr) {
            *err = "failed to open config file: " + path;
        }
        return false;
    }

    std::string line;
    std::string section;
    std::string active_list_key;
    int line_no = 0;

    try {
        while (std::getline(in, line)) {
            ++line_no;
            std::string cleaned = Trim(StripComment(line));
            if (cleaned.empty()) {
                continue;
            }

            if (!active_list_key.empty()) {
                if (StartsWith(cleaned, "-")) {
                    std::string item = Trim(cleaned.substr(1));
                    item = NormalizeOrigin(Unquote(item));
                    if (!item.empty() && section == "security" && active_list_key == "allowed_origins") {
                        out.security.allowed_origins.push_back(item);
                        continue;
                    }
                } else if (cleaned.find(':') == std::string::npos) {
                    continue;
                } else {
                    active_list_key.clear();
                }
            }

            const size_t non_ws_pos = line.find_first_not_of(" \t");
            const bool top_level_line = (non_ws_pos == 0);
            if (top_level_line && cleaned.back() == ':' && cleaned.find(':') == cleaned.size() - 1) {
                section = Trim(cleaned.substr(0, cleaned.size() - 1));
                active_list_key.clear();
                continue;
            }

            const auto colon = cleaned.find(':');
            if (colon == std::string::npos) {
                continue;
            }
            const std::string key = Trim(cleaned.substr(0, colon));
            const std::string value = Unquote(cleaned.substr(colon + 1));

            if (section == "gateway") {
                if (key == "listen_host") {
                    out.listen_host = value;
                } else if (key == "listen_port") {
                    out.listen_port = static_cast<uint16_t>(std::stoul(value));
                } else if (key == "upstream_host") {
                    out.upstream_host = value;
                } else if (key == "upstream_port") {
                    out.upstream_port = static_cast<uint16_t>(std::stoul(value));
                } else if (key == "max_packet_size") {
                    out.max_packet_size = static_cast<uint32_t>(std::stoul(value));
                } else if (key == "heartbeat_timeout_ms") {
                    out.heartbeat_timeout_ms = std::stoll(value);
                } else if (key == "reconnect_backoff_ms") {
                    out.reconnect_backoff_ms = std::stoll(value);
                } else if (key == "dev_key") {
                    out.dev_key = value;
                }
            } else if (section == "auth") {
                if (key == "token_secret") {
                    out.token_secret = value;
                } else if (key == "token_ttl_seconds") {
                    out.token_ttl_seconds = std::stoll(value);
                }
            } else if (section == "log") {
                if (key == "level") {
                    out.log_level = value;
                } else if (key == "dir") {
                    out.log_dir = value;
                }
            } else if (section == "security") {
                if (key == "allowed_origins") {
                    out.security.allowed_origins.clear();
                    active_list_key = "allowed_origins";
                } else if (key == "max_ws_connections_per_ip") {
                    out.security.max_ws_connections_per_ip = std::stoi(value);
                } else if (key == "max_ws_messages_per_sec") {
                    out.security.max_ws_messages_per_sec = std::stoi(value);
                } else if (key == "max_http_qps_per_ip") {
                    out.security.max_http_qps_per_ip = std::stoi(value);
                } else if (key == "ban_seconds_on_abuse") {
                    out.security.ban_seconds_on_abuse = std::stoi(value);
                } else if (key == "trust_x_forwarded_for") {
                    out.security.trust_x_forwarded_for = ParseBool(value);
                }
            } else if (section == "observability") {
                if (key == "enable_structured_log") {
                    out.observability.enable_structured_log = ParseBool(value);
                } else if (key == "metrics_report_interval_ms") {
                    out.observability.metrics_report_interval_ms = std::stoll(value);
                }
            }
        }
    } catch (const std::exception& ex) {
        if (err != nullptr) {
            std::ostringstream oss;
            oss << "config parse error at line " << line_no << ": " << ex.what();
            *err = oss.str();
        }
        return false;
    }

    std::set<std::string> dedup;
    std::vector<std::string> normalized;
    normalized.reserve(out.security.allowed_origins.size());
    for (const auto& raw : out.security.allowed_origins) {
        std::string origin = NormalizeOrigin(raw);
        if (origin.empty()) {
            continue;
        }
        if (!dedup.insert(origin).second) {
            continue;
        }
        normalized.push_back(origin);
    }
    out.security.allowed_origins = normalized;

    if (out.listen_port == 0 ||
        out.upstream_port == 0 ||
        out.max_packet_size < 20 ||
        out.token_ttl_seconds <= 0 ||
        out.heartbeat_timeout_ms <= 0 ||
        out.reconnect_backoff_ms <= 0 ||
        out.security.allowed_origins.empty() ||
        out.security.max_ws_connections_per_ip <= 0 ||
        out.security.max_ws_messages_per_sec <= 0 ||
        out.security.max_http_qps_per_ip <= 0 ||
        out.security.ban_seconds_on_abuse <= 0 ||
        out.observability.metrics_report_interval_ms <= 0) {
        if (err != nullptr) {
            *err = "invalid gateway config";
        }
        return false;
    }
    return true;
}

}  // namespace ddz

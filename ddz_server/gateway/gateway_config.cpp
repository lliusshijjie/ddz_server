#include "gateway/gateway_config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
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
    int line_no = 0;

    try {
        while (std::getline(in, line)) {
            ++line_no;
            std::string cleaned = Trim(StripComment(line));
            if (cleaned.empty()) {
                continue;
            }

            if (cleaned.back() == ':' && cleaned.find(':') == cleaned.size() - 1) {
                section = Trim(cleaned.substr(0, cleaned.size() - 1));
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

    if (out.listen_port == 0 || out.upstream_port == 0 || out.max_packet_size < 20 || out.token_ttl_seconds <= 0) {
        if (err != nullptr) {
            *err = "invalid gateway config";
        }
        return false;
    }
    return true;
}

}  // namespace ddz

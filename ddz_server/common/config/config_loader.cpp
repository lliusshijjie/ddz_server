#include "common/config/config_loader.h"

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

bool ParseBool(const std::string& s) {
    std::string v = s;
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

}  // namespace

bool ConfigLoader::LoadFromFile(const std::string& path, AppConfig& out, std::string* err) {
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

            if (section == "server") {
                if (key == "host") {
                    out.server.host = value;
                } else if (key == "port") {
                    out.server.port = static_cast<uint16_t>(std::stoul(value));
                } else if (key == "io_threads") {
                    out.server.io_threads = std::stoi(value);
                } else if (key == "max_packet_size") {
                    out.server.max_packet_size = static_cast<uint32_t>(std::stoul(value));
                } else if (key == "heartbeat_timeout_ms") {
                    out.server.heartbeat_timeout_ms = std::stoll(value);
                } else if (key == "match_timeout_ms") {
                    out.server.match_timeout_ms = std::stoll(value);
                }
            } else if (section == "log") {
                if (key == "level") {
                    out.log.level = value;
                } else if (key == "dir") {
                    out.log.dir = value;
                }
            } else if (section == "mysql") {
                if (key == "enabled") {
                    out.mysql.enabled = ParseBool(value);
                } else if (key == "host") {
                    out.mysql.host = value;
                } else if (key == "port") {
                    out.mysql.port = static_cast<uint16_t>(std::stoul(value));
                } else if (key == "user") {
                    out.mysql.user = value;
                } else if (key == "password") {
                    out.mysql.password = value;
                } else if (key == "database") {
                    out.mysql.database = value;
                } else if (key == "pool_size") {
                    out.mysql.pool_size = std::stoi(value);
                }
            } else if (section == "redis") {
                if (key == "enabled") {
                    out.redis.enabled = ParseBool(value);
                } else if (key == "host") {
                    out.redis.host = value;
                } else if (key == "port") {
                    out.redis.port = static_cast<uint16_t>(std::stoul(value));
                } else if (key == "db") {
                    out.redis.db = std::stoi(value);
                } else if (key == "password") {
                    out.redis.password = value;
                }
            } else if (section == "auth") {
                if (key == "token_secret") {
                    out.auth.token_secret = value;
                } else if (key == "token_ttl_seconds") {
                    out.auth.token_ttl_seconds = std::stoll(value);
                }
            } else if (section == "observability") {
                if (key == "enable_structured_log") {
                    out.observability.enable_structured_log = ParseBool(value);
                } else if (key == "metrics_report_interval_ms") {
                    out.observability.metrics_report_interval_ms = std::stoll(value);
                }
            } else if (section == "perf") {
                if (key == "soft_threshold_enabled") {
                    out.perf.soft_threshold_enabled = ParseBool(value);
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

    return true;
}

}  // namespace ddz

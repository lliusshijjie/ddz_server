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
                }
            } else if (section == "log") {
                if (key == "level") {
                    out.log.level = value;
                } else if (key == "dir") {
                    out.log.dir = value;
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


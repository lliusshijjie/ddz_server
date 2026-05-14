#pragma once

#include <map>
#include <sstream>
#include <string>
#include <utility>

namespace ddz {

inline std::map<std::string, std::string> ParseKvBody(const std::string& body) {
    std::map<std::string, std::string> kv;
    std::stringstream ss(body);
    std::string item;
    while (std::getline(ss, item, ';')) {
        if (item.empty()) {
            continue;
        }
        const auto pos = item.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        kv[item.substr(0, pos)] = item.substr(pos + 1);
    }
    return kv;
}

inline std::string BuildKvBody(const std::initializer_list<std::pair<std::string, std::string>>& fields) {
    std::string out;
    bool first = true;
    for (const auto& kv : fields) {
        if (!first) {
            out.push_back(';');
        }
        first = false;
        out += kv.first;
        out.push_back('=');
        out += kv.second;
    }
    return out;
}

}  // namespace ddz


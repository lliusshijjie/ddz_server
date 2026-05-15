#include "log/logger.h"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace ddz {

Logger& Logger::Instance() {
    static Logger logger;
    return logger;
}

bool Logger::Init(const std::string& level_str, const std::string& dir, std::string* err) {
    std::lock_guard<std::mutex> lock(mu_);

    level_ = ParseLevel(level_str);
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        if (err != nullptr) {
            *err = "failed to create log dir: " + dir;
        }
        return false;
    }

    const std::string file_path = dir + "/server.log";
    file_.open(file_path, std::ios::app);
    if (!file_.is_open()) {
        if (err != nullptr) {
            *err = "failed to open log file: " + file_path;
        }
        return false;
    }

    initialized_ = true;
    return true;
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(mu_);
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
    initialized_ = false;
}

void Logger::Debug(const std::string& msg) { Write(Level::Debug, msg); }
void Logger::Info(const std::string& msg) { Write(Level::Info, msg); }
void Logger::Warn(const std::string& msg) { Write(Level::Warn, msg); }
void Logger::Error(const std::string& msg) { Write(Level::Error, msg); }

void Logger::Write(Level level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!initialized_ || level < level_) {
        return;
    }
    const std::string line = FormatNow() + " [" + LevelName(level) + "] " + msg;
    if (level >= Level::Warn) {
        std::cerr << line << std::endl;
    } else {
        std::cout << line << std::endl;
    }
    file_ << line << '\n';
    file_.flush();
}

Logger::Level Logger::ParseLevel(const std::string& level_str) {
    if (level_str == "debug") return Level::Debug;
    if (level_str == "warn") return Level::Warn;
    if (level_str == "error") return Level::Error;
    return Level::Info;
}

const char* Logger::LevelName(Level level) {
    switch (level) {
    case Level::Debug: return "DEBUG";
    case Level::Info: return "INFO";
    case Level::Warn: return "WARN";
    case Level::Error: return "ERROR";
    default: return "INFO";
    }
}

std::string Logger::FormatNow() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto tt = system_clock::to_time_t(now);
    std::tm tm_val{};
#ifdef _WIN32
    localtime_s(&tm_val, &tt);
#else
    localtime_r(&tt, &tm_val);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_val, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

}  // namespace ddz


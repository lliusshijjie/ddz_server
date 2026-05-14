#pragma once

#include <fstream>
#include <mutex>
#include <string>

namespace ddz {

class Logger {
public:
    enum class Level {
        Debug = 0,
        Info = 1,
        Warn = 2,
        Error = 3
    };

    static Logger& Instance();

    bool Init(const std::string& level_str, const std::string& dir, std::string* err);
    void Shutdown();

    void Debug(const std::string& msg);
    void Info(const std::string& msg);
    void Warn(const std::string& msg);
    void Error(const std::string& msg);

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void Write(Level level, const std::string& msg);
    static Level ParseLevel(const std::string& level_str);
    static const char* LevelName(Level level);
    static std::string FormatNow();

private:
    std::mutex mu_;
    Level level_ = Level::Info;
    std::ofstream file_;
    bool initialized_ = false;
};

}  // namespace ddz


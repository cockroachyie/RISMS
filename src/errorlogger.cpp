#include "Errorlogger.h"

#include "FileStore.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace risms {

namespace {

std::string timestamp() {

    auto now = std::chrono::system_clock::now();

    std::time_t time =
        std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};

#if defined(_WIN32)
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif

    std::ostringstream oss;

    oss << std::put_time(
        &localTime,
        "%Y-%m-%d %H:%M:%S");

    return oss.str();
}

} // namespace

void ErrorLogger::info(
    const std::string& component,
    const std::string& message) {

    write("INFO", component, message);
}

void ErrorLogger::warning(
    const std::string& component,
    const std::string& message) {

    write("WARNING", component, message);
}

void ErrorLogger::error(
    const std::string& component,
    const std::string& message) {

    write("ERROR", component, message);
}

void ErrorLogger::write(
    const std::string& level,
    const std::string& component,
    const std::string& message) {

    std::string cleanMessage = message;

    for (char& c : cleanMessage) {
        if (c == '|') {
            c = '/';
        }
    }

    std::string line =
        timestamp() + "|" +
        level + "|" +
        component + "|" +
        cleanMessage;

    // Logging must never crash the application.
    (void)FileStore::appendLine(
        "data/error.log",
        line);
}

} // namespace risms

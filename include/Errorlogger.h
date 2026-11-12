#ifndef RISMS_ERROR_LOGGER_H
#define RISMS_ERROR_LOGGER_H

#include <string>

namespace risms {

class ErrorLogger {
public:
    static void info(
        const std::string& component,
        const std::string& message);

    static void warning(
        const std::string& component,
        const std::string& message);

    static void error(
        const std::string& component,
        const std::string& message);

private:
    static void write(
        const std::string& level,
        const std::string& component,
        const std::string& message);
};

} // namespace risms

#endif

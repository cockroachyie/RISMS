#ifndef RISMS_CONSOLE_UI_H
#define RISMS_CONSOLE_UI_H

#include <string>

namespace risms {

// Static helpers for all console input/output. Centralizing these means
// no other class touches std::cin/std::cout directly, so prompt format,
// validation and (later) input sanitization only live in one place.
class ConsoleUI {
public:
    static std::string readLine(const std::string& prompt);
    static std::string readNonEmptyLine(const std::string& prompt);
    static int readInt(const std::string& prompt, int min, int max);

    // Reads a password without echoing it to the terminal (falls back to
    // plain input if the terminal doesn't support masking, e.g. piped stdin).
    static std::string readPassword(const std::string& prompt);

    static void printError(const std::string& msg);
    static void printSuccess(const std::string& msg);
    static void printInfo(const std::string& msg);
    static void printHeader(const std::string& title);

    // Yes/no prompt. Accepts y/n (case-insensitive), repeats on anything else.
    static bool confirm(const std::string& prompt);
};

} // namespace risms

#endif // RISMS_CONSOLE_UI_H

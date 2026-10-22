#include "ConsoleUI.h"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

#if defined(_WIN32)
    #include <conio.h>
#else
    #include <termios.h>
    #include <unistd.h>
#endif

namespace risms {

std::string ConsoleUI::readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) {
        // Stdin closed/EOF: treat as empty input so callers can handle it
        // the same way as a blank line rather than crashing.
        std::cin.clear();
        line.clear();
    }
    return line;
}

std::string ConsoleUI::readNonEmptyLine(const std::string& prompt) {
    while (true) {
        std::string line = readLine(prompt);
        // trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            printError("This field cannot be empty.");
            continue;
        }
        size_t end = line.find_last_not_of(" \t\r\n");
        return line.substr(start, end - start + 1);
    }
}

int ConsoleUI::readInt(const std::string& prompt, int min, int max) {
    while (true) {
        std::string line = readLine(prompt);
        std::istringstream iss(line);
        int value;
        char leftover;
        if ((iss >> value) && !(iss >> leftover)) {
            if (value >= min && value <= max) {
                return value;
            }
            printError("Enter a number between " + std::to_string(min) +
                       " and " + std::to_string(max) + ".");
        } else {
            printError("Enter a valid whole number.");
        }
    }
}

std::string ConsoleUI::readPassword(const std::string& prompt) {
    std::cout << prompt;
    std::string password;

#if defined(_WIN32)
    char ch;
    while ((ch = static_cast<char>(_getch())) != '\r' && ch != '\n') {
        if (ch == '\b') { // backspace
            if (!password.empty()) {
                password.pop_back();
                std::cout << "\b \b";
            }
            continue;
        }
        password.push_back(ch);
        std::cout << '*';
    }
    std::cout << std::endl;
#else
    termios oldSettings{};
    bool haveTty = (tcgetattr(STDIN_FILENO, &oldSettings) == 0);

    if (haveTty) {
        termios noEcho = oldSettings;
        noEcho.c_lflag &= ~static_cast<tcflag_t>(ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &noEcho);
    }

    std::getline(std::cin, password);

    if (haveTty) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);
        std::cout << std::endl;
    }
#endif

    return password;
}

void ConsoleUI::printError(const std::string& msg) {
    std::cout << "[ERROR] " << msg << std::endl;
}

void ConsoleUI::printSuccess(const std::string& msg) {
    std::cout << "[OK] " << msg << std::endl;
}

void ConsoleUI::printInfo(const std::string& msg) {
    std::cout << "[INFO] " << msg << std::endl;
}

void ConsoleUI::printHeader(const std::string& title) {
    std::string bar(title.size() + 4, '=');
    std::cout << "\n" << bar << "\n  " << title << "\n" << bar << "\n";
}

bool ConsoleUI::confirm(const std::string& prompt) {
    while (true) {
        std::string line = readLine(prompt + " (y/n): ");
        std::transform(line.begin(), line.end(), line.begin(),
                        [](unsigned char c) { return std::tolower(c); });
        if (line == "y" || line == "yes") return true;
        if (line == "n" || line == "no") return false;
        printError("Please answer y or n.");
    }
}

} // namespace risms

#include "MenuController.h"
#include "ConsoleUI.h"
#include "ErrorCode.h"

namespace risms {

MenuController::MenuController(AuthService& authService)
    : authService_(authService) {}

Result<User> MenuController::loginGate() {
    ConsoleUI::printHeader("RISMS Login");

    while (true) {
        std::string username = ConsoleUI::readNonEmptyLine("Username (or 'quit' to exit): ");
        if (username == "quit") {
            return Result<User>::Fail(ErrorCode::InvalidInput, "User chose to quit.");
        }

        std::string password = ConsoleUI::readPassword("Password: ");

        auto result = authService_.login(username, password);
        if (result.isOk()) {
            ConsoleUI::printSuccess("Welcome, " + result.value().username + "!");
            return result;
        }

        // UserNotFound and InvalidCredentials are shown with the same
        // generic wording to avoid confirming which usernames exist.
        if (result.error() == ErrorCode::UserNotFound) {
            ConsoleUI::printError("Incorrect username or password.");
        } else {
            ConsoleUI::printError(result.message());
        }

        if (result.error() == ErrorCode::AccountLocked) {
            // Still loop back to the prompt (another user may want to try),
            // rather than exiting the program outright.
            continue;
        }
    }
}

void MenuController::run() {
    auto loginResult = loginGate();
    if (loginResult.isError()) {
        ConsoleUI::printInfo("Goodbye.");
        return;
    }

    // Sprint 1 scope ends at the login gate. The real inventory menu
    // (product records, stock in/out, history, low-stock summary) is
    // wired in during a later sprint.
    ConsoleUI::printHeader("Main Menu");
    ConsoleUI::printInfo("Logged in as: " + loginResult.value().username);
    ConsoleUI::printInfo("Main menu not yet implemented (upcoming sprint).");
}

} // namespace risms

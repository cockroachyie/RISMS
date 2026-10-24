#ifndef RISMS_MENU_CONTROLLER_H
#define RISMS_MENU_CONTROLLER_H

#include "AuthService.h"
#include "Result.h"
#include "User.h"

namespace risms {

// Top-level application loop. Sprint 1 only implements the login gate:
// no caller reaches the (future) main inventory menu without a successful
// AuthService::login(). Later sprints plug the main menu into run().
class MenuController {
public:
    explicit MenuController(AuthService& authService);

    void run();

private:
    AuthService& authService_;

    // Prompts for credentials in a loop until login succeeds or the user
    // chooses to quit. Returns Ok(User) on success, Fail(...) on quit.
    Result<User> loginGate();
};

} // namespace risms

#endif // RISMS_MENU_CONTROLLER_H

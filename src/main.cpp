#include <iostream>

#include "ConfigStore.h"
#include "ConsoleUI.h"
#include "UserRepository.h"
#include "AuthService.h"
#include "ProductRepository.h"
#include "ProductService.h"
#include "MenuController.h"

using namespace risms;

namespace {

// Sprint 1 has no "create user" story yet, so if the user file is empty
// we seed one default account purely so the login gate is testable
// end-to-end. Remove this once an admin/user-creation flow exists.
void seedDefaultUserIfEmpty(UserRepository& repo) {
    if (repo.count() > 0) return;

    User admin;
    admin.username = "admin";
    admin.role = "admin";
    AuthService::setPassword(admin, "Admin@123");

    auto result = repo.addUser(admin);
    if (result.isOk()) {
        ConsoleUI::printInfo("No users found - seeded default account (admin / Admin@123).");
    } else {
        ConsoleUI::printError("Failed to seed default user: " + result.message());
    }
}

} // namespace

int main() {
    auto& config = ConfigStore::instance();
    auto configResult = config.load("data/config.txt");
    if (configResult.isError()) {
        ConsoleUI::printError("Config load failed (" + configResult.message() +
                               "). Using built-in defaults.");
    }

    std::string usersFile = config.getString("users_file", "data/users.txt");
    std::string productsFile = config.getString("products_file", "data/products.txt");
    int maxAttempts = config.getInt("max_login_attempts", 3);
    int lockoutSeconds = config.getInt("lockout_duration_seconds", 300);

    UserRepository userRepo(usersFile);
    auto userLoadResult = userRepo.load();
    if (userLoadResult.isError()) {
        ConsoleUI::printError("Could not load user file: " + userLoadResult.message());
        return 1;
    }
    seedDefaultUserIfEmpty(userRepo);

    ProductRepository productRepo(productsFile);
    auto productLoadResult = productRepo.load();
    if (productLoadResult.isError()) {
        ConsoleUI::printError("Could not load product file: " + productLoadResult.message());
        return 1;
    }

    AuthService authService(userRepo, maxAttempts, lockoutSeconds);
    ProductService productService(productRepo);
    MenuController menu(authService, productService);
    menu.run();

    return 0;
}

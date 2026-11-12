#include "MenuController.h"
#include "ConsoleUI.h"
#include "ErrorCode.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace risms {

MenuController::MenuController(
    AuthService& authService,
    ProductService& productService,
    StockService& stockService,
    ReportService& reportService)
    : authService_(authService),
      productService_(productService),
      stockService_(stockService),
      reportService_(reportService) {}

// ---------------------------------------------------------------------
// Login gate
// ---------------------------------------------------------------------

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
    }
}

void MenuController::run() {
    try {
        auto loginResult = loginGate();
        if (loginResult.isError()) {
            ConsoleUI::printInfo("Goodbye.");
            return;
        }
        runMainMenu(loginResult.value());
    } catch (const ConsoleInputClosed&) {
        // Input stream ran out (Ctrl+D, or a piped script with no more
        // lines) partway through a prompt. Exit cleanly instead of the
        // read loops spinning forever on an exhausted stream.
        ConsoleUI::printInfo("Input closed. Goodbye.");
    }
}

// ---------------------------------------------------------------------
// Main menu
// ---------------------------------------------------------------------

void MenuController::runMainMenu(const User& user) {
    while (true) {

        ConsoleUI::printHeader("Main Menu");

        ConsoleUI::printInfo(
            "Logged in as: " + user.username);

        std::cout
            << "1. Add product\n"
            << "2. Update product\n"
            << "3. Remove product\n"
            << "4. Search product\n"
            << "5. Stock in\n"
            << "6. Stock out\n"
            << "7. Check stock\n"
            << "8. Stock history\n"
            << "9. List all products\n"
            << "10. Inventory summary\n"
            << "0. Logout and exit\n";

        int choice =
            ConsoleUI::readInt(
                "Choose an option: ",
                0,
                10);

        switch (choice) {

            case 1:
                handleAddProduct();
                break;

            case 2:
                handleUpdateProduct();
                break;

            case 3:
                handleRemoveProduct();
                break;

            case 4:
                handleSearchByName();
                break;

            case 5:
                handleStockIn(user);
                break;

            case 6:
                handleStockOut(user);
                break;

            case 7:
                handleCheckStock();
                break;

            case 8:
                handleStockHistory();
                break;

            case 9:
                handleListAll();
                break;

            case 10:
                handleInventorySummary();
                break;

            case 0:
                ConsoleUI::printInfo("Goodbye.");
                return;
        }
    }
}

// ---------------------------------------------------------------------
// Product field prompting (shared by add and update)
// ---------------------------------------------------------------------

Product MenuController::promptForProductFields(const Product& existing) {
    bool isUpdate = !existing.id.empty();
    Product p = existing;

    auto promptString = [&](const std::string& label, std::string& field, bool required) {
        while (true) {
            std::string suffix = isUpdate ? (" [" + field + "]") : "";
            std::string line = ConsoleUI::readLine(label + suffix + ": ");
            if (line.empty()) {
                if (isUpdate || !required) return; // keep current value (or leave blank if optional)
                ConsoleUI::printError(label + " cannot be empty.");
                continue;
            }
            field = line;
            return;
        }
    };

    auto promptDouble = [&](const std::string& label, double& field) {
        while (true) {
            std::ostringstream cur;
            cur << std::fixed << std::setprecision(2) << field;
            std::string suffix = isUpdate ? (" [" + cur.str() + "]") : "";
            std::string line = ConsoleUI::readLine(label + suffix + ": ");
            if (line.empty()) {
                if (isUpdate) return;
                ConsoleUI::printError(label + " cannot be empty.");
                continue;
            }
            try {
                size_t pos = 0;
                double v = std::stod(line, &pos);
                if (pos != line.size()) throw std::invalid_argument("trailing characters");
                field = v;
                return;
            } catch (...) {
                ConsoleUI::printError("Enter a valid number.");
            }
        }
    };

    auto promptInt = [&](const std::string& label, int& field) {
        while (true) {
            std::string suffix = isUpdate ? (" [" + std::to_string(field) + "]") : "";
            std::string line = ConsoleUI::readLine(label + suffix + ": ");
            if (line.empty()) {
                if (isUpdate) return;
                ConsoleUI::printError(label + " cannot be empty.");
                continue;
            }
            try {
                size_t pos = 0;
                int v = std::stoi(line, &pos);
                if (pos != line.size()) throw std::invalid_argument("trailing characters");
                field = v;
                return;
            } catch (...) {
                ConsoleUI::printError("Enter a valid whole number.");
            }
        }
    };

    promptString("SKU", p.sku, true);
    promptString("Name", p.name, true);
    promptString("Category", p.category, false);
    promptDouble("Unit price", p.unitPrice);
    promptInt("Low-stock threshold", p.lowStockThreshold);

    return p;
}

// ---------------------------------------------------------------------
// Menu actions
// ---------------------------------------------------------------------

void MenuController::handleAddProduct() {
    ConsoleUI::printHeader("Add Product");
    Product input = promptForProductFields(Product{}); // empty id => add mode
    auto result = productService_.addProduct(input);
    if (result.isOk()) {
        ConsoleUI::printSuccess("Product added with id " + result.value().id + ".");
        printProductDetail(result.value());
    } else {
        ConsoleUI::printError(result.message());
    }
}

void MenuController::handleUpdateProduct() {
    ConsoleUI::printHeader("Update Product");
    std::string id = ConsoleUI::readNonEmptyLine("Product id to update: ");

    auto existing = productService_.findById(id);
    if (existing.isError()) {
        ConsoleUI::printError(existing.message());
        return;
    }

    ConsoleUI::printInfo("Press Enter on any field to keep its current value.");
    Product updated = promptForProductFields(existing.value());

    auto result = productService_.updateProduct(updated);
    if (result.isOk()) {
        ConsoleUI::printSuccess("Product " + result.value().id + " updated.");
        printProductDetail(result.value());
    } else {
        ConsoleUI::printError(result.message());
    }
}

void MenuController::handleRemoveProduct() {
    ConsoleUI::printHeader("Remove Product");
    std::string id = ConsoleUI::readNonEmptyLine("Product id to remove: ");

    auto existing = productService_.findById(id);
    if (existing.isError()) {
        ConsoleUI::printError(existing.message());
        return;
    }

    printProductDetail(existing.value());
    if (!ConsoleUI::confirm("Remove this product?")) {
        ConsoleUI::printInfo("Cancelled.");
        return;
    }

    auto result = productService_.removeProduct(id);
    if (result.isOk()) {
        ConsoleUI::printSuccess("Product " + id + " removed.");
    } else {
        ConsoleUI::printError(result.message());
    }
}

void MenuController::handleListAll() {
    ConsoleUI::printHeader("All Products");
    auto products = productService_.listAll();
    if (products.empty()) {
        ConsoleUI::printInfo("No products recorded yet.");
        return;
    }
    for (const auto& p : products) printProductRow(p);
}

void MenuController::handleFindById() {
    ConsoleUI::printHeader("Product Detail");
    std::string id = ConsoleUI::readNonEmptyLine("Product id: ");
    auto result = productService_.findById(id);
    if (result.isError()) {
        ConsoleUI::printError(result.message());
        return;
    }
    printProductDetail(result.value());
}

void MenuController::handleSearchByName() {
    ConsoleUI::printHeader("Search Products by Name");
    std::string query = ConsoleUI::readNonEmptyLine("Search text: ");
    auto results = productService_.searchByName(query);
    if (results.empty()) {
        ConsoleUI::printInfo("No products matched \"" + query + "\".");
        return;
    }
    for (const auto& p : results) printProductRow(p);
}

// ---------------------------------------------------------------------
// Display helpers
// ---------------------------------------------------------------------

void MenuController::printProductRow(const Product& p) {
    std::ostringstream oss;
    oss << p.id << "  " << p.sku << "  " << p.name
        << "  qty=" << p.quantity
        << "  price=" << std::fixed << std::setprecision(2) << p.unitPrice
        << (p.isLowStock() ? "  [LOW STOCK]" : "");
    ConsoleUI::printInfo(oss.str());
}

void MenuController::printProductDetail(const Product& p) {
    std::cout << "\n"
              << "  ID:                  " << p.id << "\n"
              << "  SKU:                 " << p.sku << "\n"
              << "  Name:                " << p.name << "\n"
              << "  Category:            " << (p.category.empty() ? "(none)" : p.category) << "\n"
              << "  Unit Price:          " << std::fixed << std::setprecision(2) << p.unitPrice << "\n"
              << "  Quantity:            " << p.quantity << "\n"
              << "  Low-stock threshold: " << p.lowStockThreshold << "\n"
              << "  Status:              " << (p.isLowStock() ? "LOW STOCK" : "OK") << "\n"
              << std::endl;
}
void MenuController::handleStockIn(const User& user) {

    ConsoleUI::printHeader("Stock In");

    std::string productId =
        ConsoleUI::readNonEmptyLine("Product id: ");

    int quantity =
        ConsoleUI::readInt(
            "Quantity: ",
            1,
            1000000000);

    std::string reason =
        ConsoleUI::readNonEmptyLine("Reason: ");

    auto result =
        stockService_.stockIn(
            productId,
            quantity,
            user.username,
            reason);

    if (result.isError()) {
        ConsoleUI::printError(result.message());
        return;
    }

    ConsoleUI::printSuccess(
        "Stock added successfully.");

    ConsoleUI::printInfo(
        "New quantity: " +
        std::to_string(
            result.value().resultingQuantity));
}
void MenuController::handleStockOut(const User& user) {

    ConsoleUI::printHeader("Stock Out");

    std::string productId =
        ConsoleUI::readNonEmptyLine("Product id: ");

    int quantity =
        ConsoleUI::readInt(
            "Quantity: ",
            1,
            1000000000);

    std::string reason =
        ConsoleUI::readNonEmptyLine("Reason: ");

    auto result =
        stockService_.stockOut(
            productId,
            quantity,
            user.username,
            reason);

    if (result.isError()) {
        ConsoleUI::printError(result.message());
        return;
    }

    ConsoleUI::printSuccess(
        "Stock removed successfully.");

    ConsoleUI::printInfo(
        "Remaining quantity: " +
        std::to_string(
            result.value().resultingQuantity));
}
void MenuController::handleCheckStock() {

    ConsoleUI::printHeader("Check Stock");

    std::string productId =
        ConsoleUI::readNonEmptyLine("Product id: ");

    auto result =
        stockService_.getQuantity(productId);

    if (result.isError()) {
        ConsoleUI::printError(result.message());
        return;
    }

    ConsoleUI::printInfo(
        "Current quantity: " +
        std::to_string(result.value()));
}
void MenuController::handleStockHistory() {

    ConsoleUI::printHeader("Stock History");

    std::string productId =
        ConsoleUI::readNonEmptyLine("Product id: ");

    auto result =
        reportService_.historyFor(productId);

    if (result.isError()) {
        ConsoleUI::printError(result.message());
        return;
    }

    const auto& history = result.value();

    if (history.empty()) {
        ConsoleUI::printInfo(
            "No stock movements found.");

        return;
    }

    for (const auto& movement : history) {

        std::cout
            << movement.movementId
            << " | "
            << movement.type
            << " | Qty change: "
            << movement.quantityChange
            << " | Result: "
            << movement.resultingQuantity
            << " | "
            << movement.timestamp
            << " | User: "
            << movement.userId
            << " | "
            << movement.reason
            << '\n';
    }
}
void MenuController::handleInventorySummary() {

    ConsoleUI::printHeader(
        "Inventory Summary");

    auto result =
        reportService_.inventorySummary();

    if (result.isError()) {
        ConsoleUI::printError(result.message());
        return;
    }

    const auto& summary = result.value();

    std::cout
        << "Total products: "
        << summary.totalProducts
        << '\n'

        << "Total units: "
        << summary.totalUnits
        << '\n'

        << "Total inventory value: "
        << std::fixed
        << std::setprecision(2)
        << summary.totalValue
        << '\n'

        << "Low-stock products: "
        << summary.lowStockProducts
        << '\n';
}

} // namespace risms

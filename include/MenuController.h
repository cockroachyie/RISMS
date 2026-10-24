#ifndef RISMS_MENU_CONTROLLER_H
#define RISMS_MENU_CONTROLLER_H

#include "AuthService.h"
#include "ProductService.h"
#include "Result.h"
#include "User.h"
#include "Product.h"

namespace risms {

// Top-level application loop. No caller reaches the main menu without a
// successful AuthService::login(). After login, the main menu exposes the
// Sprint 1 product operations (add/update/remove/list/find/search) built
// on ProductService. Stock in/out, movement history and the low-stock
// summary are out of scope for Sprint 1 and are not wired in here.
class MenuController {
public:
    MenuController(AuthService& authService, ProductService& productService);

    void run();

private:
    AuthService& authService_;
    ProductService& productService_;

    Result<User> loginGate();
    void runMainMenu(const User& user);

    void handleAddProduct();
    void handleUpdateProduct();
    void handleRemoveProduct();
    void handleListAll();
    void handleFindById(); // doubles as the product detail view
    void handleSearchByName();

    Product promptForProductFields(const Product& existing); // existing used to prefill on update (empty id => add)
    void printProductDetail(const Product& p);
    void printProductRow(const Product& p);
};

} // namespace risms

#endif // RISMS_MENU_CONTROLLER_H

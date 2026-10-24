#ifndef RISMS_PRODUCT_H
#define RISMS_PRODUCT_H

#include <string>

namespace risms {

// A single product record as persisted in the products file.
// id is assigned by ProductRepository on creation (e.g. "P0001") and
// never changes afterward. sku is the human-chosen stock-keeping code
// and must be unique across products.
struct Product {
    std::string id;
    std::string sku;
    std::string name;
    std::string category;
    double unitPrice = 0.0;
    int quantity = 0;
    int lowStockThreshold = 0;

    bool isLowStock() const { return quantity <= lowStockThreshold; }
};

} // namespace risms

#endif // RISMS_PRODUCT_H

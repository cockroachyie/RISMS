#ifndef RISMS_PRODUCT_SERVICE_H
#define RISMS_PRODUCT_SERVICE_H

#include <string>
#include <vector>
#include "Result.h"
#include "Product.h"
#include "ProductRepository.h"

namespace risms {

// The layer MenuController (or any future UI) talks to for product
// operations. Runs Validator checks before touching the repository, so
// callers never need to validate separately.
class ProductService {
public:
    explicit ProductService(ProductRepository& repo);

    // Validates input, rejects a duplicate sku, assigns an id, persists.
    Result<Product> addProduct(Product input);

    // Validates input, requires input.id to already exist, persists.
    Result<Product> updateProduct(Product input);

    Result<void> removeProduct(const std::string& id);

    Result<Product> findById(const std::string& id) const;
    std::vector<Product> searchByName(const std::string& query) const;
    std::vector<Product> listAll() const;

private:
    ProductRepository& repo_;
};

} // namespace risms

#endif // RISMS_PRODUCT_SERVICE_H

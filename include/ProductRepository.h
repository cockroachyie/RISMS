#ifndef RISMS_PRODUCT_REPOSITORY_H
#define RISMS_PRODUCT_REPOSITORY_H

#include <string>
#include <vector>
#include "Result.h"
#include "Product.h"
#include "FileStore.h"

namespace risms {

// Loads/persists Product records to a pipe-delimited local file:
//   id|sku|name|category|unitPrice|quantity|lowStockThreshold
// Enforces SKU uniqueness and assigns ids (P0001, P0002, ...) on creation.
// No validation of field content here - that's Validator's job; this class
// only owns persistence and identity/uniqueness.
class ProductRepository {
public:
    explicit ProductRepository(std::string filePath);

    Result<void> load();
    Result<void> save() const;

    Result<Product> findById(const std::string& id) const;
    Result<Product> findBySku(const std::string& sku) const;
    std::vector<Product> listAll() const { return products_; }

    // Case-insensitive substring match on name. An empty result is not an
    // error - it just means nothing matched.
    std::vector<Product> searchByName(const std::string& query) const;

    // Assigns a fresh id to product (any id the caller set is overwritten),
    // rejects a duplicate sku, persists, and returns the stored record.
    Result<Product> addProduct(Product product);

    // Replaces the record matching product.id. Fails with ProductNotFound
    // if no such id exists, or DuplicateSku if product.sku is already used
    // by a different product.
    Result<void> updateProduct(const Product& product);

    Result<void> removeProduct(const std::string& id);

    size_t count() const { return products_.size(); }

private:
    std::string filePath_;
    std::vector<Product> products_;

    std::string generateNextId() const;
    static std::string serialize(const Product& p);
    static Result<Product> deserialize(const std::string& line);
};

} // namespace risms

#endif // RISMS_PRODUCT_REPOSITORY_H

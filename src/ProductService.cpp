#include "ProductService.h"
#include "Validator.h"

namespace risms {

ProductService::ProductService(ProductRepository& repo) : repo_(repo) {}

Result<Product> ProductService::addProduct(Product input) {
    auto validation = Validator::validateProduct(input);
    if (validation.isError()) {
        return Result<Product>::Fail(validation.error(), validation.message());
    }
    return repo_.addProduct(input);
}

Result<Product> ProductService::updateProduct(Product input) {
    if (input.id.empty()) {
        return Result<Product>::Fail(ErrorCode::InvalidInput, "Product id is required to update a product.");
    }

    auto existing = repo_.findById(input.id);
    if (existing.isError()) {
        return Result<Product>::Fail(existing.error(), existing.message());
    }

    auto validation = Validator::validateProduct(input);
    if (validation.isError()) {
        return Result<Product>::Fail(validation.error(), validation.message());
    }

    auto updateResult = repo_.updateProduct(input);
    if (updateResult.isError()) {
        return Result<Product>::Fail(updateResult.error(), updateResult.message());
    }
    return Result<Product>::Ok(input);
}

Result<void> ProductService::removeProduct(const std::string& id) {
    return repo_.removeProduct(id);
}

Result<Product> ProductService::findById(const std::string& id) const {
    return repo_.findById(id);
}

std::vector<Product> ProductService::searchByName(const std::string& query) const {
    return repo_.searchByName(query);
}

std::vector<Product> ProductService::listAll() const {
    return repo_.listAll();
}

} // namespace risms

#include "Validator.h"

namespace risms {

namespace {

bool containsDelimiterOrNewline(const std::string& s) {
    return s.find('|') != std::string::npos || s.find('\n') != std::string::npos;
}

} // namespace

Result<void> Validator::validateProduct(const Product& p) {
    if (p.name.empty()) {
        return Result<void>::Fail(ErrorCode::InvalidInput, "Product name cannot be empty.");
    }
    if (p.name.size() > 100) {
        return Result<void>::Fail(ErrorCode::InvalidInput, "Product name is too long (max 100 characters).");
    }
    if (containsDelimiterOrNewline(p.name)) {
        return Result<void>::Fail(ErrorCode::InvalidInput, "Product name cannot contain '|' or newlines.");
    }

    if (p.sku.empty()) {
        return Result<void>::Fail(ErrorCode::InvalidInput, "SKU cannot be empty.");
    }
    if (p.sku.size() > 50) {
        return Result<void>::Fail(ErrorCode::InvalidInput, "SKU is too long (max 50 characters).");
    }
    if (containsDelimiterOrNewline(p.sku)) {
        return Result<void>::Fail(ErrorCode::InvalidInput, "SKU cannot contain '|' or newlines.");
    }

    if (p.category.size() > 50) {
        return Result<void>::Fail(ErrorCode::InvalidInput, "Category is too long (max 50 characters).");
    }
    if (containsDelimiterOrNewline(p.category)) {
        return Result<void>::Fail(ErrorCode::InvalidInput, "Category cannot contain '|' or newlines.");
    }

    if (p.unitPrice < 0.0) {
        return Result<void>::Fail(ErrorCode::InvalidInput, "Unit price cannot be negative.");
    }

    if (p.quantity < 0) {
        return Result<void>::Fail(ErrorCode::InvalidInput, "Quantity cannot be negative.");
    }

    if (p.lowStockThreshold < 0) {
        return Result<void>::Fail(ErrorCode::InvalidInput, "Low-stock threshold cannot be negative.");
    }

    return Result<void>::Ok();
}

} // namespace risms

#include "StockService.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>

namespace risms {

StockService::StockService(
    ProductRepository& productRepository,
    LedgerRepository& ledgerRepository)
    : productRepository_(productRepository),
      ledgerRepository_(ledgerRepository) {}

Result<StockMovement> StockService::stockIn(
    const std::string& productId,
    int quantity,
    const std::string& userId,
    const std::string& reason) {

    if (quantity <= 0) {
        return Result<StockMovement>::Fail(
            ErrorCode::InvalidInput,
            "Stock-in quantity must be greater than zero.");
    }

    auto productResult =
        productRepository_.findById(productId);

    if (productResult.isError()) {
        return Result<StockMovement>::Fail(
            productResult.error(),
            productResult.message());
    }

    Product oldProduct = productResult.value();
    Product updatedProduct = oldProduct;

    if (quantity >
        std::numeric_limits<int>::max() - oldProduct.quantity) {

        return Result<StockMovement>::Fail(
            ErrorCode::InvalidInput,
            "Stock quantity would overflow.");
    }

    updatedProduct.quantity =
        oldProduct.quantity + quantity;

    // Persist the new quantity first.
    auto updateResult =
        productRepository_.updateProduct(updatedProduct);

    if (updateResult.isError()) {
        return Result<StockMovement>::Fail(
            updateResult.error(),
            updateResult.message());
    }

    StockMovement movement;

    movement.movementId = "";
    movement.productId = productId;
    movement.type = "IN";
    movement.quantityChange = quantity;
    movement.resultingQuantity = updatedProduct.quantity;
    movement.timestamp = currentTimestamp();
    movement.userId = userId;
    movement.reason = reason;

    auto ledgerResult =
        ledgerRepository_.append(movement);

    if (ledgerResult.isError()) {

        // Roll back the product quantity if the ledger
        // cannot be persisted.
        auto rollbackResult =
            productRepository_.updateProduct(oldProduct);

        if (rollbackResult.isError()) {
            return Result<StockMovement>::Fail(
                ErrorCode::FileIOError,
                "Stock-in failed and rollback could not be completed.");
        }

        return Result<StockMovement>::Fail(
            ledgerResult.error(),
            "Stock-in failed because the ledger could not be updated.");
    }

    // Generate the movement ID after the append operation
    // if the repository accepted the movement without one.
    auto history = ledgerRepository_.listAll();

    if (!history.empty()) {
        movement.movementId = history.back().movementId;
    }

    return Result<StockMovement>::Ok(movement);
}

Result<StockMovement> StockService::stockOut(
    const std::string& productId,
    int quantity,
    const std::string& userId,
    const std::string& reason) {

    if (quantity <= 0) {
        return Result<StockMovement>::Fail(
            ErrorCode::InvalidInput,
            "Stock-out quantity must be greater than zero.");
    }

    auto productResult =
        productRepository_.findById(productId);

    if (productResult.isError()) {
        return Result<StockMovement>::Fail(
            productResult.error(),
            productResult.message());
    }

    Product oldProduct = productResult.value();

    if (quantity > oldProduct.quantity) {
        return Result<StockMovement>::Fail(
            ErrorCode::InsufficientStock,
            "Insufficient stock. Available quantity: " +
            std::to_string(oldProduct.quantity));
    }

    Product updatedProduct = oldProduct;

    updatedProduct.quantity =
        oldProduct.quantity - quantity;

    // Persist the reduced quantity.
    auto updateResult =
        productRepository_.updateProduct(updatedProduct);

    if (updateResult.isError()) {
        return Result<StockMovement>::Fail(
            updateResult.error(),
            updateResult.message());
    }

    StockMovement movement;

    movement.movementId = "";
    movement.productId = productId;
    movement.type = "OUT";
    movement.quantityChange = -quantity;
    movement.resultingQuantity = updatedProduct.quantity;
    movement.timestamp = currentTimestamp();
    movement.userId = userId;
    movement.reason = reason;

    auto ledgerResult =
        ledgerRepository_.append(movement);

    if (ledgerResult.isError()) {

        // Critical rollback requirement:
        // restore the original product quantity.
        auto rollbackResult =
            productRepository_.updateProduct(oldProduct);

        if (rollbackResult.isError()) {
            return Result<StockMovement>::Fail(
                ErrorCode::FileIOError,
                "Stock-out failed and rollback could not be completed.");
        }

        return Result<StockMovement>::Fail(
            ledgerResult.error(),
            "Stock-out failed because the ledger could not be updated.");
    }

    auto history = ledgerRepository_.listAll();

    if (!history.empty()) {
        movement.movementId = history.back().movementId;
    }

    return Result<StockMovement>::Ok(movement);
}

Result<int> StockService::getQuantity(
    const std::string& productId) const {

    auto productResult =
        productRepository_.findById(productId);

    if (productResult.isError()) {
        return Result<int>::Fail(
            productResult.error(),
            productResult.message());
    }

    return Result<int>::Ok(
        productResult.value().quantity);
}

std::string StockService::currentTimestamp() {

    auto now = std::chrono::system_clock::now();
    std::time_t time =
        std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};

#if defined(_WIN32)
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif

    std::ostringstream oss;

    oss << std::put_time(
        &localTime,
        "%Y-%m-%d %H:%M:%S");

    return oss.str();
}

} // namespace risms

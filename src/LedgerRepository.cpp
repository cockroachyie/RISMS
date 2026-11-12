#include "LedgerRepository.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace risms {

LedgerRepository::LedgerRepository(std::string filePath)
    : filePath_(std::move(filePath)) {}

Result<void> LedgerRepository::load() {
    movements_.clear();

    auto linesResult = FileStore::readLines(filePath_);

    if (linesResult.isError()) {
        // A missing ledger file is okay. It will be created on first append.
        if (linesResult.error() == ErrorCode::FileIOError) {
            return Result<void>::Ok();
        }

        return Result<void>::Fail(
            linesResult.error(),
            linesResult.message());
    }

    for (const auto& line : linesResult.value()) {
        if (line.empty()) {
            continue;
        }

        auto parsed = deserialize(line);

        // Corrupt lines are skipped rather than stopping startup.
        if (parsed.isError()) {
            continue;
        }

        movements_.push_back(parsed.value());
    }

    return Result<void>::Ok();
}

Result<void> LedgerRepository::append(
    const StockMovement& input) {

    StockMovement movement = input;

    if (movement.movementId.empty()) {
        movement.movementId = generateNextId();
    }

    std::string line = serialize(movement);

    auto result = FileStore::appendLine(filePath_, line);

    if (result.isError()) {
        return result;
    }

    movements_.push_back(movement);

    return Result<void>::Ok();
}

std::vector<StockMovement> LedgerRepository::listAll() const {
    return movements_;
}

std::vector<StockMovement> LedgerRepository::forProduct(
    const std::string& productId) const {

    std::vector<StockMovement> result;

    for (const auto& movement : movements_) {
        if (movement.productId == productId) {
            result.push_back(movement);
        }
    }

    // Newest first.
    std::reverse(result.begin(), result.end());

    return result;
}

size_t LedgerRepository::count() const {
    return movements_.size();
}

const std::string& LedgerRepository::path() const {
    return filePath_;
}

std::string LedgerRepository::generateNextId() const {
    int maxNumber = 0;

    for (const auto& movement : movements_) {
        if (movement.movementId.size() > 1 &&
            movement.movementId[0] == 'M') {

            try {
                int number =
                    std::stoi(movement.movementId.substr(1));

                maxNumber = std::max(maxNumber, number);
            } catch (...) {
                // Ignore malformed IDs.
            }
        }
    }

    std::ostringstream oss;
    oss << 'M'
        << std::setw(6)
        << std::setfill('0')
        << (maxNumber + 1);

    return oss.str();
}

std::string LedgerRepository::serialize(
    const StockMovement& movement) {

    // Pipe-delimited format:
    //
    // movementId|productId|type|quantityChange|
    // resultingQuantity|timestamp|userId|reason

    std::string reason = movement.reason;
    std::string userId = movement.userId;

    // Prevent the delimiter from breaking the record.
    std::replace(reason.begin(), reason.end(), '|', '/');
    std::replace(userId.begin(), userId.end(), '|', '/');

    std::ostringstream oss;

    oss << movement.movementId << '|'
        << movement.productId << '|'
        << movement.type << '|'
        << movement.quantityChange << '|'
        << movement.resultingQuantity << '|'
        << movement.timestamp << '|'
        << userId << '|'
        << reason;

    return oss.str();
}

Result<StockMovement> LedgerRepository::deserialize(
    const std::string& line) {

    std::vector<std::string> fields;

    std::stringstream ss(line);
    std::string field;

    while (std::getline(ss, field, '|')) {
        fields.push_back(field);
    }

    if (fields.size() != 8) {
        return Result<StockMovement>::Fail(
            ErrorCode::FileIOError,
            "Malformed ledger record.");
    }

    StockMovement movement;

    movement.movementId = fields[0];
    movement.productId = fields[1];
    movement.type = fields[2];
    movement.timestamp = fields[5];
    movement.userId = fields[6];
    movement.reason = fields[7];

    try {
        movement.quantityChange = std::stoi(fields[3]);
        movement.resultingQuantity = std::stoi(fields[4]);
    } catch (...) {
        return Result<StockMovement>::Fail(
            ErrorCode::FileIOError,
            "Malformed numeric field in ledger record.");
    }

    if (movement.movementId.empty() ||
        movement.productId.empty() ||
        (movement.type != "IN" && movement.type != "OUT")) {

        return Result<StockMovement>::Fail(
            ErrorCode::FileIOError,
            "Invalid ledger record.");
    }

    return Result<StockMovement>::Ok(movement);
}

} // namespace risms

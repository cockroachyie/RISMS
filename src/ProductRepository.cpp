#include "ProductRepository.h"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cctype>

namespace risms {

ProductRepository::ProductRepository(std::string filePath)
    : filePath_(std::move(filePath)) {}

std::string ProductRepository::serialize(const Product& p) {
    std::ostringstream oss;
    oss << p.id << '|' << p.sku << '|' << p.name << '|' << p.category << '|'
        << std::fixed << std::setprecision(2) << p.unitPrice << '|'
        << p.quantity << '|' << p.lowStockThreshold;
    return oss.str();
}

Result<Product> ProductRepository::deserialize(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, '|')) {
        fields.push_back(field);
    }

    if (fields.size() != 7) {
        return Result<Product>::Fail(ErrorCode::FileIOError,
                                      "Malformed product record: " + line);
    }

    Product p;
    p.id = fields[0];
    p.sku = fields[1];
    p.name = fields[2];
    p.category = fields[3];
    try {
        p.unitPrice = std::stod(fields[4]);
        p.quantity = std::stoi(fields[5]);
        p.lowStockThreshold = std::stoi(fields[6]);
    } catch (...) {
        return Result<Product>::Fail(ErrorCode::FileIOError,
                                      "Malformed numeric field in product record: " + line);
    }

    return Result<Product>::Ok(p);
}

Result<void> ProductRepository::load() {
    products_.clear();

    auto linesResult = FileStore::readLines(filePath_);
    if (linesResult.isError()) {
        return Result<void>::Fail(linesResult.error(), linesResult.message());
    }

    for (const auto& line : linesResult.value()) {
        auto parsed = deserialize(line);
        if (parsed.isError()) {
            return Result<void>::Fail(parsed.error(), parsed.message());
        }
        products_.push_back(parsed.value());
    }

    return Result<void>::Ok();
}

Result<void> ProductRepository::save() const {
    std::vector<std::string> lines;
    lines.reserve(products_.size());
    for (const auto& p : products_) {
        lines.push_back(serialize(p));
    }
    return FileStore::writeLinesAtomic(filePath_, lines);
}

Result<Product> ProductRepository::findById(const std::string& id) const {
    auto it = std::find_if(products_.begin(), products_.end(),
                            [&](const Product& p) { return p.id == id; });
    if (it == products_.end()) {
        return Result<Product>::Fail(ErrorCode::ProductNotFound, "No product with id: " + id);
    }
    return Result<Product>::Ok(*it);
}

Result<Product> ProductRepository::findBySku(const std::string& sku) const {
    auto it = std::find_if(products_.begin(), products_.end(),
                            [&](const Product& p) { return p.sku == sku; });
    if (it == products_.end()) {
        return Result<Product>::Fail(ErrorCode::ProductNotFound, "No product with sku: " + sku);
    }
    return Result<Product>::Ok(*it);
}

std::vector<Product> ProductRepository::searchByName(const std::string& query) const {
    std::string needle = query;
    std::transform(needle.begin(), needle.end(), needle.begin(),
                    [](unsigned char c) { return std::tolower(c); });

    std::vector<Product> matches;
    for (const auto& p : products_) {
        std::string haystack = p.name;
        std::transform(haystack.begin(), haystack.end(), haystack.begin(),
                        [](unsigned char c) { return std::tolower(c); });
        if (haystack.find(needle) != std::string::npos) {
            matches.push_back(p);
        }
    }
    return matches;
}

std::string ProductRepository::generateNextId() const {
    int maxNum = 0;
    for (const auto& p : products_) {
        if (p.id.size() > 1 && p.id[0] == 'P') {
            try {
                int num = std::stoi(p.id.substr(1));
                maxNum = std::max(maxNum, num);
            } catch (...) {
                // non-numeric suffix, ignore for id generation purposes
            }
        }
    }
    std::ostringstream oss;
    oss << 'P' << std::setw(4) << std::setfill('0') << (maxNum + 1);
    return oss.str();
}

Result<Product> ProductRepository::addProduct(Product product) {
    auto dup = std::find_if(products_.begin(), products_.end(),
                             [&](const Product& p) { return p.sku == product.sku; });
    if (dup != products_.end()) {
        return Result<Product>::Fail(ErrorCode::DuplicateSku,
                                      "A product with sku '" + product.sku + "' already exists.");
    }

    product.id = generateNextId();
    products_.push_back(product);

    auto saveResult = save();
    if (saveResult.isError()) {
        products_.pop_back(); // keep in-memory state consistent with disk on failure
        return Result<Product>::Fail(saveResult.error(), saveResult.message());
    }
    return Result<Product>::Ok(product);
}

Result<void> ProductRepository::updateProduct(const Product& product) {
    auto it = std::find_if(products_.begin(), products_.end(),
                            [&](const Product& p) { return p.id == product.id; });
    if (it == products_.end()) {
        return Result<void>::Fail(ErrorCode::ProductNotFound,
                                   "Cannot update unknown product id: " + product.id);
    }

    auto skuOwner = std::find_if(products_.begin(), products_.end(),
                                  [&](const Product& p) {
                                      return p.sku == product.sku && p.id != product.id;
                                  });
    if (skuOwner != products_.end()) {
        return Result<void>::Fail(ErrorCode::DuplicateSku,
                                   "SKU '" + product.sku + "' is already used by product " + skuOwner->id);
    }

    *it = product;
    return save();
}

Result<void> ProductRepository::removeProduct(const std::string& id) {
    auto it = std::find_if(products_.begin(), products_.end(),
                            [&](const Product& p) { return p.id == id; });
    if (it == products_.end()) {
        return Result<void>::Fail(ErrorCode::ProductNotFound, "No product with id: " + id);
    }
    products_.erase(it);
    return save();
}

} // namespace risms

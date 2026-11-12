#ifndef RISMS_STOCK_SERVICE_H
#define RISMS_STOCK_SERVICE_H

#include <string>

#include "Result.h"
#include "ProductRepository.h"
#include "LedgerRepository.h"

namespace risms {

class StockService {
public:
    StockService(
        ProductRepository& productRepository,
        LedgerRepository& ledgerRepository);

    Result<StockMovement> stockIn(
        const std::string& productId,
        int quantity,
        const std::string& userId,
        const std::string& reason);

    Result<StockMovement> stockOut(
        const std::string& productId,
        int quantity,
        const std::string& userId,
        const std::string& reason);

    Result<int> getQuantity(
        const std::string& productId) const;

private:
    ProductRepository& productRepository_;
    LedgerRepository& ledgerRepository_;

    static std::string currentTimestamp();
};

} // namespace risms

#endif // RISMS_STOCK_SERVICE_H
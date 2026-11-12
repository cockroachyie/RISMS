#ifndef RISMS_REPORT_SERVICE_H
#define RISMS_REPORT_SERVICE_H

#include <string>
#include <vector>

#include "Result.h"
#include "ProductRepository.h"
#include "LedgerRepository.h"

namespace risms {

struct InventorySummary {
    size_t totalProducts = 0;
    long long totalUnits = 0;
    double totalValue = 0.0;
    size_t lowStockProducts = 0;
};

class ReportService {
public:
    ReportService(
        ProductRepository& productRepository,
        LedgerRepository& ledgerRepository,
        int defaultLowStockThreshold = 10);

    Result<std::vector<StockMovement>> historyFor(
        const std::string& productId) const;

    Result<InventorySummary> inventorySummary() const;

private:
    ProductRepository& productRepository_;
    LedgerRepository& ledgerRepository_;
    int defaultLowStockThreshold_;
};

} // namespace risms

#endif

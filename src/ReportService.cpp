#include "ReportService.h"

namespace risms {

ReportService::ReportService(
    ProductRepository& productRepository,
    LedgerRepository& ledgerRepository,
    int defaultLowStockThreshold)
    : productRepository_(productRepository),
      ledgerRepository_(ledgerRepository),
      defaultLowStockThreshold_(defaultLowStockThreshold) {}

Result<std::vector<StockMovement>>
ReportService::historyFor(
    const std::string& productId) const {

    auto productResult =
        productRepository_.findById(productId);

    if (productResult.isError()) {
        return Result<std::vector<StockMovement>>::Fail(
            productResult.error(),
            productResult.message());
    }

    return Result<std::vector<StockMovement>>::Ok(
        ledgerRepository_.forProduct(productId));
}

Result<InventorySummary>
ReportService::inventorySummary() const {

    InventorySummary summary;

    auto products = productRepository_.listAll();

    summary.totalProducts = products.size();

    for (const auto& product : products) {

        summary.totalUnits += product.quantity;

        summary.totalValue +=
            product.unitPrice *
            static_cast<double>(product.quantity);

        int threshold =
            product.lowStockThreshold > 0
                ? product.lowStockThreshold
                : defaultLowStockThreshold_;

        if (product.quantity <= threshold) {
            ++summary.lowStockProducts;
        }
    }

    return Result<InventorySummary>::Ok(summary);
}

} // namespace risms

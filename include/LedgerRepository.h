#ifndef RISMS_LEDGER_REPOSITORY_H
#define RISMS_LEDGER_REPOSITORY_H

#include <string>
#include <vector>

#include "Result.h"
#include "FileStore.h"

namespace risms {

struct StockMovement {
    std::string movementId;
    std::string productId;
    std::string type;              // IN or OUT
    int quantityChange = 0;
    int resultingQuantity = 0;
    std::string timestamp;
    std::string userId;
    std::string reason;
};

class LedgerRepository {
public:
    explicit LedgerRepository(std::string filePath);

    Result<void> load();
    Result<void> append(const StockMovement& movement);

    std::vector<StockMovement> listAll() const;

    std::vector<StockMovement> forProduct(
        const std::string& productId) const;

    size_t count() const;

    const std::string& path() const;

private:
    std::string filePath_;
    std::vector<StockMovement> movements_;

    std::string generateNextId() const;

    static std::string serialize(const StockMovement& movement);

    static Result<StockMovement> deserialize(
        const std::string& line);
};

} // namespace risms

#endif // RISMS_LEDGER_REPOSITORY_H
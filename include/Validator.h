#ifndef RISMS_VALIDATOR_H
#define RISMS_VALIDATOR_H

#include "Product.h"
#include "Result.h"

namespace risms {

// Pure field-level validation, no file or repository access. Kept separate
// from ProductRepository/ProductService so the rules can be unit-tested
// (or reused by a future product-import feature) without touching disk.
class Validator {
public:
    // Checks name/sku/category are non-empty, within length limits, and
    // contain no '|' or newline (which would corrupt the pipe-delimited
    // storage format). Checks unitPrice, quantity and lowStockThreshold
    // are non-negative. Returns Fail(InvalidInput, <first problem found>)
    // on the first violation, Ok() if every field is valid.
    static Result<void> validateProduct(const Product& p);
};

} // namespace risms

#endif // RISMS_VALIDATOR_H

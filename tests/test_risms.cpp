// RISMS unit tests - self-contained (no external test framework).
// Build & run from repo root:
//   g++ -std=c++17 -Iinclude tests/test_risms.cpp $(ls src/*.cpp | grep -v main.cpp) -o risms_tests && ./risms_tests
//
// Test names map to the TC IDs in the RISMS Test Case Specification.

#include <cmath>
#include <cstdio>
#include <limits>
#include <sstream>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "AuthService.h"
#include "ConfigStore.h"
#include "FileStore.h"
#include "LedgerRepository.h"
#include "Product.h"
#include "ProductRepository.h"
#include "ProductService.h"
#include "ReportService.h"
#include "StockService.h"
#include "UserRepository.h"
#include "Validator.h"

namespace fs = std::filesystem;
using namespace risms;

// ---------------------------------------------------------------------
// Minimal test harness
// ---------------------------------------------------------------------

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

static std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

struct AssertionFailed {
    std::string msg;
};

#define TEST(name)                                         \
    static void name();                                    \
    static Registrar registrar_##name(#name, name);        \
    static void name()

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            throw AssertionFailed{std::string(__FILE__) + ":" +                  \
                                  std::to_string(__LINE__) + "  CHECK(" #cond ")"}; \
        }                                                                        \
    } while (0)

#define CHECK_EQ(a, b)                                                           \
    do {                                                                         \
        auto _va = (a);                                                          \
        auto _vb = (b);                                                          \
        if (!(_va == _vb)) {                                                     \
            std::ostringstream _oss;                                             \
            _oss << __FILE__ << ":" << __LINE__ << "  CHECK_EQ(" #a ", " #b ")"  \
                 << "  got [" << _va << "] vs [" << _vb << "]";                  \
            throw AssertionFailed{_oss.str()};                                   \
        }                                                                        \
    } while (0)

#define CHECK_NEAR(a, b, eps) CHECK(std::abs((a) - (b)) < (eps))

std::ostream& operator<<(std::ostream& os, ErrorCode c) { return os << toString(c); }

// ---------------------------------------------------------------------
// Fixtures
// ---------------------------------------------------------------------

// Each test runs in its own empty temp directory (cwd is switched to it),
// so data files and data/error.log never touch the real repo data.
class TempDir {
public:
    TempDir() {
        static int counter = 0;
        path_ = fs::temp_directory_path() /
                ("risms_test_" + std::to_string(::getpid()) + "_" + std::to_string(counter++));
        fs::remove_all(path_);
        fs::create_directories(path_ / "data");
        old_ = fs::current_path();
        fs::current_path(path_);
    }
    ~TempDir() {
        fs::current_path(old_);
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    std::string file(const std::string& name) const { return (path_ / "data" / name).string(); }

private:
    fs::path path_;
    fs::path old_;
};

static Product makeProduct(const std::string& sku, const std::string& name,
                           double price = 10.0, int threshold = 5) {
    Product p;
    p.sku = sku;
    p.name = name;
    p.category = "Grocery";
    p.unitPrice = price;
    p.quantity = 0;
    p.lowStockThreshold = threshold;
    return p;
}

static void writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::trunc);
    f << content;
}

static std::vector<std::string> readFile(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    return lines;
}

// Bundles the repositories + services a stock/report test needs.
struct Inventory {
    TempDir dir;
    ProductRepository products{dir.file("products.txt")};
    LedgerRepository ledger{dir.file("ledger.dat")};
    ProductService productService{products};
    StockService stock{products, ledger};
    ReportService report{products, ledger, 10};

    Inventory() {
        products.load();
        ledger.load();
    }

    std::string add(const std::string& sku, const std::string& name,
                    double price = 10.0, int threshold = 5) {
        auto r = productService.addProduct(makeProduct(sku, name, price, threshold));
        CHECK(r.isOk());
        return r.value().id;
    }
};

// =====================================================================
// Validator  (VFR02, BR02, BR03)
// =====================================================================

TEST(TC_PRD_01_validator_accepts_valid_product) {
    CHECK(Validator::validateProduct(makeProduct("SKU-001", "Rice 5kg")).isOk());
}

TEST(TC_PRD_02_validator_accepts_blank_category) {
    auto p = makeProduct("SKU-002", "Soap");
    p.category = "";
    CHECK(Validator::validateProduct(p).isOk());
}

TEST(TC_PRD_04_validator_rejects_empty_sku) {
    auto r = Validator::validateProduct(makeProduct("", "Rice"));
    CHECK(r.isError());
    CHECK_EQ(r.error(), ErrorCode::InvalidInput);
}

TEST(TC_PRD_05_validator_rejects_empty_name) {
    auto r = Validator::validateProduct(makeProduct("SKU-001", ""));
    CHECK(r.isError());
    CHECK_EQ(r.error(), ErrorCode::InvalidInput);
}

TEST(TC_PRD_06_validator_rejects_negative_price) {
    auto r = Validator::validateProduct(makeProduct("SKU-001", "Rice", -5.0));
    CHECK(r.isError());
    CHECK_EQ(r.message(), std::string("Unit price cannot be negative."));
}

TEST(TC_PRD_06b_validator_accepts_zero_price_boundary) {
    CHECK(Validator::validateProduct(makeProduct("SKU-001", "Free sample", 0.0)).isOk());
}

TEST(TC_PRD_09_validator_rejects_negative_threshold) {
    auto r = Validator::validateProduct(makeProduct("SKU-001", "Rice", 10.0, -2));
    CHECK(r.isError());
    CHECK_EQ(r.message(), std::string("Low-stock threshold cannot be negative."));
}

TEST(TC_PRD_09b_validator_rejects_negative_quantity) {
    auto p = makeProduct("SKU-001", "Rice");
    p.quantity = -1;
    CHECK(Validator::validateProduct(p).isError());
}

TEST(TC_PRD_10_validator_rejects_pipe_in_name) {
    CHECK(Validator::validateProduct(makeProduct("SKU-001", "Bad|Name")).isError());
}

TEST(TC_PRD_10b_validator_rejects_pipe_in_sku_and_category) {
    CHECK(Validator::validateProduct(makeProduct("SKU|1", "Rice")).isError());
    auto p = makeProduct("SKU-001", "Rice");
    p.category = "A|B";
    CHECK(Validator::validateProduct(p).isError());
}

TEST(TC_PRD_10c_validator_length_limits) {
    CHECK(Validator::validateProduct(makeProduct("SKU-001", std::string(100, 'a'))).isOk());
    CHECK(Validator::validateProduct(makeProduct("SKU-001", std::string(101, 'a'))).isError());
    CHECK(Validator::validateProduct(makeProduct(std::string(50, 's'), "Rice")).isOk());
    CHECK(Validator::validateProduct(makeProduct(std::string(51, 's'), "Rice")).isError());
}

// =====================================================================
// Product management  (FR02-FR04, UC02-UC04, VFR01, BR01)
// =====================================================================

TEST(TC_PRD_01_add_assigns_sequential_ids) {
    Inventory inv;
    CHECK_EQ(inv.add("SKU-001", "Rice 5kg"), std::string("P0001"));
    CHECK_EQ(inv.add("SKU-002", "Soap"), std::string("P0002"));
    CHECK_EQ(inv.products.count(), (size_t)2);
}

TEST(TC_PRD_03_duplicate_sku_rejected) {
    Inventory inv;
    inv.add("SKU-001", "Rice 5kg");
    auto r = inv.productService.addProduct(makeProduct("SKU-001", "Dup"));
    CHECK(r.isError());
    CHECK_EQ(r.error(), ErrorCode::DuplicateSku);
    CHECK_EQ(inv.products.count(), (size_t)1);
}

TEST(TC_PRD_03b_invalid_product_not_stored) {
    Inventory inv;
    auto r = inv.productService.addProduct(makeProduct("SKU-001", "Rice", -1.0));
    CHECK(r.isError());
    CHECK_EQ(inv.products.count(), (size_t)0);
}

TEST(TC_PRD_11_update_changes_fields) {
    Inventory inv;
    std::string id = inv.add("SKU-001", "Rice 5kg", 450.0);
    Product p = inv.products.findById(id).value();
    p.name = "Basmati Rice 5kg";
    p.unitPrice = 500.0;
    auto r = inv.productService.updateProduct(p);
    CHECK(r.isOk());
    Product stored = inv.products.findById(id).value();
    CHECK_EQ(stored.name, std::string("Basmati Rice 5kg"));
    CHECK_NEAR(stored.unitPrice, 500.0, 1e-9);
    CHECK_EQ(stored.sku, std::string("SKU-001"));
}

TEST(TC_PRD_12_update_to_existing_sku_rejected) {
    Inventory inv;
    std::string id1 = inv.add("SKU-001", "Rice");
    inv.add("SKU-002", "Soap");
    Product p = inv.products.findById(id1).value();
    p.sku = "SKU-002";
    auto r = inv.productService.updateProduct(p);
    CHECK(r.isError());
    CHECK_EQ(r.error(), ErrorCode::DuplicateSku);
    CHECK_EQ(inv.products.findById(id1).value().sku, std::string("SKU-001"));
}

TEST(TC_PRD_12b_update_keeping_own_sku_allowed) {
    Inventory inv;
    std::string id = inv.add("SKU-001", "Rice");
    Product p = inv.products.findById(id).value();
    p.category = "Staples";
    CHECK(inv.productService.updateProduct(p).isOk());
}

TEST(TC_PRD_13_update_unknown_product) {
    Inventory inv;
    auto p = makeProduct("SKU-001", "Rice");
    p.id = "P0099";
    auto r = inv.productService.updateProduct(p);
    CHECK(r.isError());
    CHECK_EQ(r.error(), ErrorCode::ProductNotFound);
}

TEST(TC_PRD_13b_update_requires_id) {
    Inventory inv;
    auto r = inv.productService.updateProduct(makeProduct("SKU-001", "Rice"));
    CHECK(r.isError());
    CHECK_EQ(r.error(), ErrorCode::InvalidInput);
}

TEST(TC_PRD_14_remove_existing_product) {
    Inventory inv;
    std::string id = inv.add("SKU-001", "Rice");
    CHECK(inv.productService.removeProduct(id).isOk());
    CHECK(inv.products.findById(id).isError());
    CHECK_EQ(inv.products.count(), (size_t)0);
}

TEST(TC_PRD_17_remove_unknown_product) {
    Inventory inv;
    auto r = inv.productService.removeProduct("P0099");
    CHECK(r.isError());
    CHECK_EQ(r.error(), ErrorCode::ProductNotFound);
}

TEST(TC_PRD_18_ids_not_reused_after_removing_middle) {
    Inventory inv;
    inv.add("SKU-001", "A");
    std::string id2 = inv.add("SKU-002", "B");
    inv.add("SKU-003", "C");
    inv.productService.removeProduct(id2);
    CHECK_EQ(inv.add("SKU-004", "D"), std::string("P0004"));
}

// =====================================================================
// Search / view / list  (FR05, FR06, FR11, VFR03, VFR04)
// =====================================================================

TEST(TC_SRC_01_search_exact_name) {
    Inventory inv;
    inv.add("SKU-001", "Rice 5kg");
    inv.add("SKU-002", "Soap");
    auto r = inv.productService.searchByName("Rice 5kg");
    CHECK_EQ(r.size(), (size_t)1);
    CHECK_EQ(r[0].id, std::string("P0001"));
}

TEST(TC_SRC_02_search_case_insensitive) {
    Inventory inv;
    inv.add("SKU-001", "Rice 5kg");
    CHECK_EQ(inv.productService.searchByName("RICE").size(), (size_t)1);
}

TEST(TC_SRC_03_search_partial_match_multiple) {
    Inventory inv;
    inv.add("SKU-001", "Rice 5kg");
    inv.add("SKU-002", "Brown Rice");
    inv.add("SKU-003", "Soap");
    CHECK_EQ(inv.productService.searchByName("rice").size(), (size_t)2);
}

TEST(TC_SRC_04_search_no_match) {
    Inventory inv;
    inv.add("SKU-001", "Rice 5kg");
    CHECK(inv.productService.searchByName("xyz").empty());
}

TEST(TC_SRC_05_find_by_id_returns_all_fields) {
    Inventory inv;
    std::string id = inv.add("SKU-001", "Rice 5kg", 450.0, 5);
    auto r = inv.productService.findById(id);
    CHECK(r.isOk());
    CHECK_EQ(r.value().sku, std::string("SKU-001"));
    CHECK_EQ(r.value().category, std::string("Grocery"));
    CHECK_NEAR(r.value().unitPrice, 450.0, 1e-9);
    CHECK_EQ(r.value().lowStockThreshold, 5);
}

TEST(TC_SRC_05b_find_by_unknown_id) {
    Inventory inv;
    CHECK_EQ(inv.productService.findById("P0099").error(), ErrorCode::ProductNotFound);
}

TEST(TC_SRC_06_list_all) {
    Inventory inv;
    inv.add("SKU-001", "Rice");
    inv.add("SKU-002", "Soap");
    CHECK_EQ(inv.productService.listAll().size(), (size_t)2);
}

TEST(TC_SRC_07_list_empty) {
    Inventory inv;
    CHECK(inv.productService.listAll().empty());
}

// =====================================================================
// Stock adjustment  (FR07-FR09, VFR05-VFR07, BR06-BR08, SEC-03)
// =====================================================================

TEST(TC_STK_01_stock_in_increases_quantity) {
    Inventory inv;
    std::string id = inv.add("SKU-001", "Rice");
    auto r = inv.stock.stockIn(id, 20, "admin", "Restock");
    CHECK(r.isOk());
    CHECK_EQ(r.value().resultingQuantity, 20);
    CHECK_EQ(inv.stock.getQuantity(id).value(), 20);
    inv.stock.stockIn(id, 5, "admin", "Restock");
    CHECK_EQ(inv.stock.getQuantity(id).value(), 25);
}

TEST(TC_STK_02_stock_in_is_logged) {
    Inventory inv;
    std::string id = inv.add("SKU-001", "Rice");
    auto r = inv.stock.stockIn(id, 20, "admin", "Restock");
    CHECK(r.isOk());
    const auto& m = r.value();
    CHECK_EQ(m.movementId, std::string("M000001"));
    CHECK_EQ(m.productId, id);
    CHECK_EQ(m.type, std::string("IN"));
    CHECK_EQ(m.quantityChange, 20);
    CHECK_EQ(m.userId, std::string("admin"));
    CHECK_EQ(m.reason, std::string("Restock"));
    CHECK_EQ(m.timestamp.size(), (size_t)19);  // YYYY-MM-DD HH:MM:SS

    auto lines = readFile(inv.ledger.path());
    CHECK_EQ(lines.size(), (size_t)1);
    CHECK_EQ(lines[0].rfind("M000001|P0001|IN|20|20|", 0), (size_t)0);
}

TEST(TC_STK_03_stock_in_rejects_zero_and_negative) {
    Inventory inv;
    std::string id = inv.add("SKU-001", "Rice");
    CHECK_EQ(inv.stock.stockIn(id, 0, "admin", "x").error(), ErrorCode::InvalidInput);
    CHECK_EQ(inv.stock.stockIn(id, -3, "admin", "x").error(), ErrorCode::InvalidInput);
    CHECK_EQ(inv.stock.getQuantity(id).value(), 0);
    CHECK_EQ(inv.ledger.count(), (size_t)0);
}

TEST(TC_STK_05_stock_in_unknown_product) {
    Inventory inv;
    auto r = inv.stock.stockIn("P0099", 4, "admin", "x");
    CHECK_EQ(r.error(), ErrorCode::ProductNotFound);
    CHECK_EQ(inv.ledger.count(), (size_t)0);
}

TEST(TC_STK_05b_stock_in_overflow_rejected) {
    Inventory inv;
    std::string id = inv.add("SKU-001", "Rice");
    CHECK(inv.stock.stockIn(id, std::numeric_limits<int>::max(), "admin", "x").isOk());
    auto r = inv.stock.stockIn(id, 1, "admin", "x");
    CHECK_EQ(r.error(), ErrorCode::InvalidInput);
    CHECK_EQ(inv.stock.getQuantity(id).value(), std::numeric_limits<int>::max());
}

TEST(TC_STK_06_stock_out_decreases_quantity) {
    Inventory inv;
    std::string id = inv.add("SKU-001", "Rice");
    inv.stock.stockIn(id, 20, "admin", "Restock");
    auto r = inv.stock.stockOut(id, 15, "admin", "Sale");
    CHECK(r.isOk());
    CHECK_EQ(r.value().type, std::string("OUT"));
    CHECK_EQ(r.value().quantityChange, -15);
    CHECK_EQ(r.value().resultingQuantity, 5);
    CHECK_EQ(inv.stock.getQuantity(id).value(), 5);
}

TEST(TC_STK_07_stock_out_exceeding_stock_rejected) {
    Inventory inv;
    std::string id = inv.add("SKU-001", "Rice");
    inv.stock.stockIn(id, 20, "admin", "Restock");
    auto r = inv.stock.stockOut(id, 50, "admin", "Sale");
    CHECK_EQ(r.error(), ErrorCode::InsufficientStock);
    CHECK_EQ(inv.stock.getQuantity(id).value(), 20);
    CHECK_EQ(inv.ledger.count(), (size_t)1);  // only the stock-in
}

TEST(TC_STK_08_stock_out_exact_quantity_boundary) {
    Inventory inv;
    std::string id = inv.add("SKU-001", "Rice");
    inv.stock.stockIn(id, 10, "admin", "Restock");
    auto r = inv.stock.stockOut(id, 10, "admin", "Sale");
    CHECK(r.isOk());
    CHECK_EQ(inv.stock.getQuantity(id).value(), 0);
}

TEST(TC_STK_09_stock_out_from_zero_rejected) {
    Inventory inv;
    std::string id = inv.add("SKU-001", "Rice");
    CHECK_EQ(inv.stock.stockOut(id, 1, "admin", "Sale").error(), ErrorCode::InsufficientStock);
}

TEST(TC_STK_09b_stock_out_rejects_zero_and_negative) {
    Inventory inv;
    std::string id = inv.add("SKU-001", "Rice");
    inv.stock.stockIn(id, 10, "admin", "Restock");
    CHECK_EQ(inv.stock.stockOut(id, 0, "admin", "x").error(), ErrorCode::InvalidInput);
    CHECK_EQ(inv.stock.stockOut(id, -1, "admin", "x").error(), ErrorCode::InvalidInput);
    CHECK_EQ(inv.stock.getQuantity(id).value(), 10);
}

TEST(TC_STK_10_check_stock) {
    Inventory inv;
    std::string id = inv.add("SKU-001", "Rice");
    inv.stock.stockIn(id, 7, "admin", "Restock");
    auto r = inv.stock.getQuantity(id);
    CHECK(r.isOk());
    CHECK_EQ(r.value(), 7);
}

TEST(TC_STK_11_check_stock_unknown_product) {
    Inventory inv;
    CHECK_EQ(inv.stock.getQuantity("P0099").error(), ErrorCode::ProductNotFound);
}

TEST(TC_STK_15_ledger_failure_rolls_back_quantity) {
    // SEC-04 / NFR03: if the ledger append fails, the product quantity
    // must be restored. Make the ledger path a directory so append fails.
    TempDir dir;
    std::string ledgerPath = dir.file("ledger_as_dir");
    fs::create_directories(ledgerPath);

    ProductRepository products(dir.file("products.txt"));
    LedgerRepository ledger(ledgerPath);
    products.load();
    ProductService ps(products);
    StockService stock(products, ledger);

    auto p = ps.addProduct(makeProduct("SKU-001", "Rice"));
    CHECK(p.isOk());
    auto r = stock.stockIn(p.value().id, 10, "admin", "Restock");
    CHECK(r.isError());
    CHECK_EQ(stock.getQuantity(p.value().id).value(), 0);

    ProductRepository reloaded(dir.file("products.txt"));
    reloaded.load();
    CHECK_EQ(reloaded.findById(p.value().id).value().quantity, 0);
}

// =====================================================================
// Stock history  (FR10, UC09)
// =====================================================================

TEST(TC_STK_12_history_newest_first) {
    Inventory inv;
    std::string id = inv.add("SKU-001", "Rice");
    inv.stock.stockIn(id, 20, "admin", "Restock");
    inv.stock.stockOut(id, 15, "admin", "Sale");
    auto r = inv.report.historyFor(id);
    CHECK(r.isOk());
    CHECK_EQ(r.value().size(), (size_t)2);
    CHECK_EQ(r.value()[0].movementId, std::string("M000002"));
    CHECK_EQ(r.value()[0].type, std::string("OUT"));
    CHECK_EQ(r.value()[1].movementId, std::string("M000001"));
}

TEST(TC_STK_12b_history_filters_by_product) {
    Inventory inv;
    std::string a = inv.add("SKU-001", "Rice");
    std::string b = inv.add("SKU-002", "Soap");
    inv.stock.stockIn(a, 5, "admin", "x");
    inv.stock.stockIn(b, 3, "admin", "x");
    inv.stock.stockIn(a, 2, "admin", "x");
    CHECK_EQ(inv.report.historyFor(a).value().size(), (size_t)2);
    CHECK_EQ(inv.report.historyFor(b).value().size(), (size_t)1);
}

TEST(TC_STK_13_history_empty) {
    Inventory inv;
    std::string id = inv.add("SKU-001", "Rice");
    auto r = inv.report.historyFor(id);
    CHECK(r.isOk());
    CHECK(r.value().empty());
}

TEST(TC_STK_14_history_unknown_product) {
    Inventory inv;
    CHECK_EQ(inv.report.historyFor("P0099").error(), ErrorCode::ProductNotFound);
}

// =====================================================================
// Inventory summary  (FR12, VFR09, BR10)
// =====================================================================

TEST(TC_SUM_01_summary_totals) {
    Inventory inv;
    std::string id = inv.add("SKU-001", "Rice 5kg", 450.0, 5);
    inv.stock.stockIn(id, 5, "admin", "Restock");
    auto s = inv.report.inventorySummary().value();
    CHECK_EQ(s.totalProducts, (size_t)1);
    CHECK_EQ(s.totalUnits, 5LL);
    CHECK_NEAR(s.totalValue, 2250.0, 1e-6);
    CHECK_EQ(s.lowStockProducts, (size_t)1);
}

TEST(TC_SUM_02_qty_equal_threshold_is_low) {
    Inventory inv;
    std::string id = inv.add("SKU-001", "Rice", 10.0, 5);
    inv.stock.stockIn(id, 5, "admin", "x");
    CHECK(inv.products.findById(id).value().isLowStock());
    CHECK_EQ(inv.report.inventorySummary().value().lowStockProducts, (size_t)1);
}

TEST(TC_SUM_03_qty_above_threshold_not_low) {
    Inventory inv;
    std::string id = inv.add("SKU-001", "Rice", 10.0, 5);
    inv.stock.stockIn(id, 12, "admin", "x");
    CHECK(!inv.products.findById(id).value().isLowStock());
    CHECK_EQ(inv.report.inventorySummary().value().lowStockProducts, (size_t)0);
}

// Expected to FAIL on current main: documents DEF-01.
TEST(TC_SUM_04_listing_and_summary_agree_for_zero_threshold) {
    Inventory inv;
    std::string id = inv.add("SKU-002", "Soap", 10.0, 0);
    inv.stock.stockIn(id, 5, "admin", "x");
    bool listedLow = inv.products.findById(id).value().isLowStock();
    bool summaryLow = inv.report.inventorySummary().value().lowStockProducts == 1;
    CHECK_EQ(listedLow, summaryLow);
}

TEST(TC_SUM_05_summary_empty_inventory) {
    Inventory inv;
    auto s = inv.report.inventorySummary().value();
    CHECK_EQ(s.totalProducts, (size_t)0);
    CHECK_EQ(s.totalUnits, 0LL);
    CHECK_NEAR(s.totalValue, 0.0, 1e-9);
    CHECK_EQ(s.lowStockProducts, (size_t)0);
}

// =====================================================================
// Persistence & robustness  (FR13, FR14, VFR08, NFR03, NFR04, SEC-04)
// =====================================================================

TEST(TC_PER_01_product_written_to_file) {
    Inventory inv;
    inv.add("SKU-001", "Rice 5kg", 450.0, 5);
    auto lines = readFile(inv.dir.file("products.txt"));
    CHECK_EQ(lines.size(), (size_t)1);
    CHECK_EQ(lines[0], std::string("P0001|SKU-001|Rice 5kg|Grocery|450.00|0|5"));
}

TEST(TC_PER_03_data_reloaded_after_restart) {
    TempDir dir;
    std::string id;
    {
        ProductRepository products(dir.file("products.txt"));
        LedgerRepository ledger(dir.file("ledger.dat"));
        products.load();
        ledger.load();
        ProductService ps(products);
        StockService stock(products, ledger);
        id = ps.addProduct(makeProduct("SKU-001", "Rice", 500.0)).value().id;
        stock.stockIn(id, 12, "admin", "GRN");
    }
    ProductRepository products(dir.file("products.txt"));
    LedgerRepository ledger(dir.file("ledger.dat"));
    CHECK(products.load().isOk());
    CHECK(ledger.load().isOk());
    auto p = products.findById(id);
    CHECK(p.isOk());
    CHECK_EQ(p.value().quantity, 12);
    CHECK_NEAR(p.value().unitPrice, 500.0, 1e-9);
    CHECK_EQ(ledger.forProduct(id).size(), (size_t)1);
    CHECK_EQ(ledger.forProduct(id)[0].reason, std::string("GRN"));
}

TEST(TC_PER_03b_movement_ids_continue_after_restart) {
    TempDir dir;
    std::string id;
    {
        ProductRepository products(dir.file("products.txt"));
        LedgerRepository ledger(dir.file("ledger.dat"));
        products.load();
        ledger.load();
        StockService stock(products, ledger);
        id = ProductService(products).addProduct(makeProduct("SKU-001", "Rice")).value().id;
        stock.stockIn(id, 1, "admin", "x");
    }
    ProductRepository products(dir.file("products.txt"));
    LedgerRepository ledger(dir.file("ledger.dat"));
    products.load();
    ledger.load();
    StockService stock(products, ledger);
    CHECK_EQ(stock.stockIn(id, 1, "admin", "x").value().movementId, std::string("M000002"));
}

TEST(TC_PER_04_missing_files_load_empty) {
    TempDir dir;
    ProductRepository products(dir.file("products.txt"));
    LedgerRepository ledger(dir.file("ledger.dat"));
    CHECK(products.load().isOk());
    CHECK(ledger.load().isOk());
    CHECK_EQ(products.count(), (size_t)0);
    CHECK_EQ(ledger.count(), (size_t)0);
}

TEST(TC_ROB_01_corrupt_product_line_skipped) {
    TempDir dir;
    writeFile(dir.file("products.txt"),
              "P0001|SKU-001|Rice|Grocery|450.00|5|5\n"
              "garbage line no pipes\n"
              "P0002|SKU-002|Soap||abc|1|0\n");
    ProductRepository products(dir.file("products.txt"));
    CHECK(products.load().isOk());
    CHECK_EQ(products.count(), (size_t)1);
    CHECK(products.findById("P0001").isOk());
}

TEST(TC_ROB_02_corrupt_ledger_line_skipped) {
    TempDir dir;
    writeFile(dir.file("ledger.dat"),
              "M000001|P0001|IN|20|20|2026-11-11 10:00:00|admin|Restock\n"
              "junk\n"
              "M000002|P0001|SIDEWAYS|1|21|2026-11-11 10:00:00|admin|x\n");
    LedgerRepository ledger(dir.file("ledger.dat"));
    CHECK(ledger.load().isOk());
    CHECK_EQ(ledger.count(), (size_t)1);
}

// Expected to FAIL on current main: documents DEF-02.
TEST(TC_ROB_03_pipe_in_reason_consistent_before_and_after_restart) {
    TempDir dir;
    std::string id;
    std::string reasonBefore;
    {
        ProductRepository products(dir.file("products.txt"));
        LedgerRepository ledger(dir.file("ledger.dat"));
        products.load();
        ledger.load();
        StockService stock(products, ledger);
        id = ProductService(products).addProduct(makeProduct("SKU-001", "Rice")).value().id;
        stock.stockIn(id, 3, "admin", "bad|reason");
        reasonBefore = ledger.forProduct(id)[0].reason;
    }
    LedgerRepository ledger(dir.file("ledger.dat"));
    ledger.load();
    CHECK_EQ(ledger.count(), (size_t)1);  // record was not broken by the '|'
    CHECK_EQ(ledger.forProduct(id)[0].reason, reasonBefore);
}

TEST(TC_ROB_04_atomic_write_leaves_no_temp_files) {
    Inventory inv;
    inv.add("SKU-001", "Rice");
    inv.add("SKU-002", "Soap");
    for (const auto& e : fs::directory_iterator(fs::path(inv.dir.file("products.txt")).parent_path())) {
        CHECK(e.path().filename().string().find(".tmp") == std::string::npos);
    }
}

TEST(TC_ROB_05_filestore_roundtrip_and_append) {
    TempDir dir;
    std::string path = dir.file("nested/dir/f.txt");
    CHECK(FileStore::writeLinesAtomic(path, {"a", "b"}).isOk());
    CHECK(FileStore::appendLine(path, "c").isOk());
    auto r = FileStore::readLines(path);
    CHECK(r.isOk());
    CHECK_EQ(r.value().size(), (size_t)3);
    CHECK_EQ(r.value()[2], std::string("c"));
}

// =====================================================================
// Authentication  (FR01, SEC-01)
// =====================================================================

struct AuthFixture {
    TempDir dir;
    UserRepository users{dir.file("users.txt")};
    AuthFixture() {
        users.load();
        User u;
        u.username = "admin";
        u.role = "admin";
        AuthService::setPassword(u, "Admin@123");
        users.addUser(u);
    }
};

TEST(TC_AUTH_02_valid_login) {
    AuthFixture f;
    AuthService auth(f.users, 3, 300);
    auto r = auth.login("admin", "Admin@123");
    CHECK(r.isOk());
    CHECK_EQ(r.value().username, std::string("admin"));
}

TEST(TC_AUTH_02b_password_not_stored_in_plaintext) {
    AuthFixture f;
    auto lines = readFile(f.dir.file("users.txt"));
    CHECK_EQ(lines.size(), (size_t)1);
    CHECK(lines[0].find("Admin@123") == std::string::npos);
}

TEST(TC_AUTH_03_wrong_password_counts_attempts) {
    AuthFixture f;
    AuthService auth(f.users, 3, 300);
    auto r1 = auth.login("admin", "wrong");
    CHECK_EQ(r1.error(), ErrorCode::InvalidCredentials);
    CHECK(r1.message().find("2 attempt(s) remaining") != std::string::npos);
    auto r2 = auth.login("admin", "wrong");
    CHECK(r2.message().find("1 attempt(s) remaining") != std::string::npos);
}

TEST(TC_AUTH_04_lockout_after_max_attempts) {
    AuthFixture f;
    AuthService auth(f.users, 3, 300);
    auth.login("admin", "wrong");
    auth.login("admin", "wrong");
    CHECK_EQ(auth.login("admin", "wrong").error(), ErrorCode::AccountLocked);
}

TEST(TC_AUTH_05_correct_password_rejected_while_locked) {
    AuthFixture f;
    AuthService auth(f.users, 3, 300);
    for (int i = 0; i < 3; ++i) auth.login("admin", "wrong");
    CHECK_EQ(auth.login("admin", "Admin@123").error(), ErrorCode::AccountLocked);
}

TEST(TC_AUTH_05b_lockout_expires) {
    AuthFixture f;
    AuthService auth(f.users, 3, 0);  // zero-second lockout expires immediately
    for (int i = 0; i < 3; ++i) auth.login("admin", "wrong");
    CHECK(auth.login("admin", "Admin@123").isOk());
}

TEST(TC_AUTH_05c_success_resets_failed_counter) {
    AuthFixture f;
    AuthService auth(f.users, 3, 300);
    auth.login("admin", "wrong");
    auth.login("admin", "wrong");
    CHECK(auth.login("admin", "Admin@123").isOk());
    auto r = auth.login("admin", "wrong");
    CHECK_EQ(r.error(), ErrorCode::InvalidCredentials);  // not locked
}

TEST(TC_AUTH_06_unknown_user) {
    AuthFixture f;
    AuthService auth(f.users, 3, 300);
    CHECK_EQ(auth.login("ghost", "x").error(), ErrorCode::UserNotFound);
}

TEST(TC_AUTH_08_lockout_persists_across_restart) {
    AuthFixture f;
    {
        AuthService auth(f.users, 3, 300);
        for (int i = 0; i < 3; ++i) auth.login("admin", "wrong");
    }
    UserRepository reloaded(f.dir.file("users.txt"));
    CHECK(reloaded.load().isOk());
    AuthService auth(reloaded, 3, 300);
    CHECK_EQ(auth.login("admin", "Admin@123").error(), ErrorCode::AccountLocked);
}

TEST(TC_AUTH_09_duplicate_user_rejected) {
    AuthFixture f;
    User u;
    u.username = "admin";
    AuthService::setPassword(u, "x");
    CHECK(f.users.addUser(u).isError());
}

static User staffActor() {
    User u;
    u.username = "clerk";
    u.role = kRoleInventoryStaff;
    return u;
}

TEST(TC_USR_01_admin_creates_user_who_can_login) {
    AuthFixture f;
    AuthService auth(f.users, 3, 300);
    User admin = auth.login("admin", "Admin@123").value();
    auto r = auth.createUser(admin, "alice", "Secret#1", kRoleInventoryStaff);
    CHECK(r.isOk());
    CHECK_EQ(r.value().role, kRoleInventoryStaff);
    CHECK(auth.login("alice", "Secret#1").isOk());
}

TEST(TC_USR_02_new_user_persisted_hashed) {
    AuthFixture f;
    AuthService auth(f.users, 3, 300);
    User admin = auth.login("admin", "Admin@123").value();
    CHECK(auth.createUser(admin, "bob", "Hunter22", "admin").isOk());

    auto lines = readFile(f.dir.file("users.txt"));
    CHECK_EQ(lines.size(), (size_t)2);
    CHECK(lines[1].find("Hunter22") == std::string::npos);

    UserRepository reloaded(f.dir.file("users.txt"));
    CHECK(reloaded.load().isOk());
    AuthService auth2(reloaded, 3, 300);
    auto r = auth2.login("bob", "Hunter22");
    CHECK(r.isOk());
    CHECK_EQ(r.value().role, std::string("admin"));
}

TEST(TC_USR_03_non_admin_cannot_create_user) {
    AuthFixture f;
    AuthService auth(f.users, 3, 300);
    auto r = auth.createUser(staffActor(), "mallory", "Secret#1", "admin");
    CHECK_EQ(r.error(), ErrorCode::PermissionDenied);
    CHECK_EQ(f.users.count(), (size_t)1);
}

TEST(TC_USR_04_duplicate_username_rejected) {
    AuthFixture f;
    AuthService auth(f.users, 3, 300);
    User admin = auth.login("admin", "Admin@123").value();
    CHECK_EQ(auth.createUser(admin, "admin", "Secret#1", kRoleInventoryStaff).error(),
             ErrorCode::DuplicateUser);
}

TEST(TC_USR_05_invalid_username_and_password_rejected) {
    AuthFixture f;
    AuthService auth(f.users, 3, 300);
    User admin = auth.login("admin", "Admin@123").value();
    CHECK_EQ(auth.createUser(admin, "ab", "Secret#1", kRoleInventoryStaff).error(), ErrorCode::InvalidInput);
    CHECK_EQ(auth.createUser(admin, "bad|name", "Secret#1", kRoleInventoryStaff).error(), ErrorCode::InvalidInput);
    CHECK_EQ(auth.createUser(admin, "has space", "Secret#1", kRoleInventoryStaff).error(), ErrorCode::InvalidInput);
    CHECK_EQ(auth.createUser(admin, "carol", "short", kRoleInventoryStaff).error(), ErrorCode::InvalidInput);
    CHECK_EQ(auth.createUser(admin, "carol", "pipe|pass", kRoleInventoryStaff).error(), ErrorCode::InvalidInput);
    CHECK_EQ(auth.createUser(admin, "carol", "Secret#1", "owner").error(), ErrorCode::InvalidInput);
    CHECK_EQ(f.users.count(), (size_t)1);
}

TEST(TC_USR_06_only_admin_and_inventory_staff_roles) {
    AuthFixture f;
    AuthService auth(f.users, 3, 300);
    User admin = auth.login("admin", "Admin@123").value();
    CHECK_EQ(auth.createUser(admin, "carol", "Secret#1", "user").error(), ErrorCode::InvalidInput);
    CHECK_EQ(auth.createUser(admin, "carol", "Secret#1", "staff").error(), ErrorCode::InvalidInput);

    // Unknown/legacy roles on disk load as Inventory Staff, never Admin.
    writeFile(f.dir.file("legacy.txt"), "dave|h|s|0|0|staff\neve|h|s|0|0|user\n");
    UserRepository legacy(f.dir.file("legacy.txt"));
    CHECK(legacy.load().isOk());
    CHECK_EQ(legacy.findByUsername("dave").value().role, kRoleInventoryStaff);
    CHECK_EQ(legacy.findByUsername("eve").value().role, kRoleInventoryStaff);
}

TEST(TC_USR_07_inventory_staff_permissions) {
    User staff = staffActor();
    CHECK(!staff.isAdmin());
    CHECK(!staff.canManageAccounts());
    CHECK(!staff.canRemoveProduct());
    CHECK(!staff.canViewInventorySummary());
    CHECK_EQ(roleDisplayName(staff.role), std::string("Inventory Staff"));

    User admin;
    admin.role = kRoleAdmin;
    CHECK(admin.canManageAccounts());
    CHECK(admin.canRemoveProduct());
    CHECK(admin.canViewInventorySummary());
}

TEST(TC_USR_08_admin_removes_account) {
    AuthFixture f;
    AuthService auth(f.users, 3, 300);
    User admin = auth.login("admin", "Admin@123").value();
    CHECK(auth.createUser(admin, "alice", "Secret#1", kRoleInventoryStaff).isOk());
    CHECK(auth.removeUser(admin, "alice").isOk());
    CHECK_EQ(auth.login("alice", "Secret#1").error(), ErrorCode::UserNotFound);

    UserRepository reloaded(f.dir.file("users.txt"));
    CHECK(reloaded.load().isOk());
    CHECK_EQ(reloaded.count(), (size_t)1);
}

TEST(TC_USR_09_remove_account_rules) {
    AuthFixture f;
    AuthService auth(f.users, 3, 300);
    User admin = auth.login("admin", "Admin@123").value();
    CHECK(auth.createUser(admin, "alice", "Secret#1", kRoleInventoryStaff).isOk());
    CHECK_EQ(auth.removeUser(staffActor(), "alice").error(), ErrorCode::PermissionDenied);
    CHECK_EQ(auth.removeUser(admin, "admin").error(), ErrorCode::InvalidInput);
    CHECK_EQ(auth.removeUser(admin, "ghost").error(), ErrorCode::UserNotFound);
    CHECK_EQ(f.users.count(), (size_t)2);
}

// =====================================================================
// Config  (2.6 assumptions: threshold / limits come from config)
// =====================================================================

TEST(TC_CFG_01_config_parses_values_comments_whitespace) {
    TempDir dir;
    writeFile(dir.file("config.txt"),
              "# comment\n"
              "  max_login_attempts = 5  \n"
              "low_stock_threshold=7 # trailing comment\n"
              "malformed line\n"
              "bad_int=abc\n");
    auto& cfg = ConfigStore::instance();
    CHECK(cfg.load(dir.file("config.txt")).isOk());
    CHECK_EQ(cfg.getInt("max_login_attempts", 3), 5);
    CHECK_EQ(cfg.getInt("low_stock_threshold", 10), 7);
    CHECK_EQ(cfg.getInt("bad_int", 42), 42);
    CHECK_EQ(cfg.getInt("missing", 9), 9);
    CHECK_EQ(cfg.getString("missing", "def"), std::string("def"));
}

TEST(TC_CFG_02_missing_config_file) {
    TempDir dir;
    CHECK_EQ(ConfigStore::instance().load(dir.file("nope.txt")).error(), ErrorCode::FileIOError);
}

// ---------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------

int main() {
    int passed = 0, failed = 0;
    std::vector<std::string> failures;

    for (const auto& t : registry()) {
        try {
            t.fn();
            ++passed;
            std::cout << "[PASS] " << t.name << "\n";
        } catch (const AssertionFailed& e) {
            ++failed;
            failures.push_back(t.name + "\n       " + e.msg);
            std::cout << "[FAIL] " << t.name << "\n       " << e.msg << "\n";
        } catch (const std::exception& e) {
            ++failed;
            failures.push_back(t.name + "\n       exception: " + e.what());
            std::cout << "[FAIL] " << t.name << "\n       exception: " << e.what() << "\n";
        }
    }

    std::cout << "\n" << passed << " passed, " << failed << " failed, "
              << (passed + failed) << " total\n";
    if (!failures.empty()) {
        std::cout << "\nFailures:\n";
        for (const auto& f : failures) std::cout << "  " << f << "\n";
    }
    return failed == 0 ? 0 : 1;
}

#ifndef RISMS_USER_H
#define RISMS_USER_H

#include <string>
#include <cstdint>

namespace risms {

// The only two roles in RISMS, as stored in the users file.
inline const std::string kRoleAdmin = "admin";
inline const std::string kRoleInventoryStaff = "inventory_staff";

inline bool isValidRole(const std::string& role) {
    return role == kRoleAdmin || role == kRoleInventoryStaff;
}

// A single account record as persisted in the users file.
// lockedUntilEpoch == 0 means "not locked". Any value > current time means
// the account is locked until that unix timestamp.
struct User {
    std::string username;
    std::string passwordHash;
    std::string salt;
    int failedAttempts = 0;
    int64_t lockedUntilEpoch = 0;
    std::string role = kRoleInventoryStaff;

    bool isAdmin() const { return role == kRoleAdmin; }

    // Inventory Staff can do everything an Admin can except:
    //   - add or remove accounts
    //   - remove products from inventory
    //   - view the inventory summary
    bool canManageAccounts() const { return isAdmin(); }
    bool canRemoveProduct() const { return isAdmin(); }
    bool canViewInventorySummary() const { return isAdmin(); }
};

// Human-readable role name for console output.
inline std::string roleDisplayName(const std::string& role) {
    if (role == kRoleAdmin) return "Admin";
    return "Inventory Staff";
}

} // namespace risms

#endif // RISMS_USER_H

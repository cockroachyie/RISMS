#ifndef RISMS_USER_H
#define RISMS_USER_H

#include <string>
#include <cstdint>

namespace risms {

// A single user record as persisted in the users file.
// lockedUntilEpoch == 0 means "not locked". Any value > current time means
// the account is locked until that unix timestamp.
struct User {
    std::string username;
    std::string passwordHash;
    std::string salt;
    int failedAttempts = 0;
    int64_t lockedUntilEpoch = 0;
    std::string role = "staff";
};

} // namespace risms

#endif // RISMS_USER_H

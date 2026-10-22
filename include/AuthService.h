#ifndef RISMS_AUTH_SERVICE_H
#define RISMS_AUTH_SERVICE_H

#include <string>
#include "Result.h"
#include "User.h"
#include "UserRepository.h"

namespace risms {

// Authenticates users against UserRepository and enforces a failed-attempt
// lockout policy: after maxAttempts_ consecutive wrong passwords, the
// account is locked for lockoutSeconds_ before another attempt is allowed.
class AuthService {
public:
    AuthService(UserRepository& userRepo, int maxAttempts, int lockoutSeconds);

    // Returns Ok(User) on success.
    // Returns Fail(UserNotFound | InvalidCredentials | AccountLocked) on failure.
    // A successful login resets failedAttempts/lock state and persists it.
    // A failed login increments failedAttempts and persists it; crossing
    // maxAttempts_ locks the account for lockoutSeconds_.
    Result<User> login(const std::string& username, const std::string& password);

    // Utility for seeding/creating accounts (hashes with a fresh salt).
    static void setPassword(User& user, const std::string& plainPassword);

private:
    UserRepository& userRepo_;
    int maxAttempts_;
    int lockoutSeconds_;

    static std::string generateSalt();
    static std::string hashPassword(const std::string& password, const std::string& salt);
    static int64_t nowEpoch();
};

} // namespace risms

#endif // RISMS_AUTH_SERVICE_H

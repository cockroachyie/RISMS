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

    // Admin-only account creation. Fails with PermissionDenied unless
    // actor.role == "admin", InvalidInput if username/password/role break
    // the rules below, or DuplicateUser if the username is taken.
    //   username: 3-32 chars, letters/digits/'_'/'.'/'-' only
    //   password: 6-64 chars, no '|' or newline
    //   role:     kRoleAdmin or kRoleInventoryStaff
    // On success the new user is persisted (hashed + salted) and returned.
    Result<User> createUser(const User& actor,
                            const std::string& username,
                            const std::string& password,
                            const std::string& role);

    // Admin-only account removal. Fails with PermissionDenied unless actor
    // is an admin, InvalidInput if the admin tries to remove their own
    // account, or UserNotFound if no such account exists.
    Result<void> removeUser(const User& actor, const std::string& username);

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

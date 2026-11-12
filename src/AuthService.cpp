#include "AuthService.h"

#include <cctype>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

namespace risms {

AuthService::AuthService(UserRepository& userRepo, int maxAttempts, int lockoutSeconds)
    : userRepo_(userRepo), maxAttempts_(maxAttempts), lockoutSeconds_(lockoutSeconds) {}

int64_t AuthService::nowEpoch() {
    return static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

std::string AuthService::generateSalt() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream oss;
    oss << std::hex << dist(gen);
    return oss.str();
}

// FNV-1a 64-bit hash of (salt + password). Not cryptographically secure,
// but keeps the project free of external crypto dependencies while still
// avoiding plaintext password storage, which is sufficient for this
// project's scope (local single-user console app, no network exposure).
std::string AuthService::hashPassword(const std::string& password, const std::string& salt) {
    const uint64_t FNV_OFFSET = 14695981039346656037ULL;
    const uint64_t FNV_PRIME = 1099511628211ULL;

    uint64_t hash = FNV_OFFSET;
    std::string combined = salt + password;
    for (unsigned char c : combined) {
        hash ^= c;
        hash *= FNV_PRIME;
    }

    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return oss.str();
}

void AuthService::setPassword(User& user, const std::string& plainPassword) {
    user.salt = generateSalt();
    user.passwordHash = hashPassword(plainPassword, user.salt);
}

Result<User> AuthService::createUser(const User& actor,
                                     const std::string& username,
                                     const std::string& password,
                                     const std::string& role) {
    if (!actor.canManageAccounts()) {
        return Result<User>::Fail(ErrorCode::PermissionDenied,
                                  "Only admins can add accounts.");
    }

    if (username.size() < 3 || username.size() > 32) {
        return Result<User>::Fail(ErrorCode::InvalidInput,
                                  "Username must be 3-32 characters.");
    }
    for (unsigned char c : username) {
        if (!(std::isalnum(c) || c == '_' || c == '.' || c == '-')) {
            return Result<User>::Fail(
                ErrorCode::InvalidInput,
                "Username may only contain letters, digits, '_', '.' or '-'.");
        }
    }

    if (password.size() < 6 || password.size() > 64) {
        return Result<User>::Fail(ErrorCode::InvalidInput,
                                  "Password must be 6-64 characters.");
    }
    if (password.find_first_of("|\r\n") != std::string::npos) {
        return Result<User>::Fail(ErrorCode::InvalidInput,
                                  "Password may not contain '|' or line breaks.");
    }

    if (!isValidRole(role)) {
        return Result<User>::Fail(ErrorCode::InvalidInput,
                                  "Role must be Admin or Inventory Staff.");
    }

    User user;
    user.username = username;
    user.role = role;
    setPassword(user, password);

    auto added = userRepo_.addUser(user);
    if (added.isError()) {
        return Result<User>::Fail(added.error(), added.message());
    }
    return Result<User>::Ok(user);
}

Result<void> AuthService::removeUser(const User& actor, const std::string& username) {
    if (!actor.canManageAccounts()) {
        return Result<void>::Fail(ErrorCode::PermissionDenied,
                                  "Only admins can remove accounts.");
    }
    if (username == actor.username) {
        return Result<void>::Fail(ErrorCode::InvalidInput,
                                  "You cannot remove your own account.");
    }
    return userRepo_.removeUser(username);
}

Result<User> AuthService::login(const std::string& username, const std::string& password) {
    auto found = userRepo_.findByUsername(username);
    if (found.isError()) {
        return Result<User>::Fail(ErrorCode::UserNotFound, "Unknown username.");
    }

    User user = found.value();
    int64_t now = nowEpoch();

    // Still within an active lockout window.
    if (user.lockedUntilEpoch > now) {
        int64_t remaining = user.lockedUntilEpoch - now;
        return Result<User>::Fail(
            ErrorCode::AccountLocked,
            "Account locked. Try again in " + std::to_string(remaining) + " second(s).");
    }

    // Lockout window has expired: clear it before evaluating this attempt.
    if (user.lockedUntilEpoch != 0 && user.lockedUntilEpoch <= now) {
        user.lockedUntilEpoch = 0;
        user.failedAttempts = 0;
    }

    std::string candidateHash = hashPassword(password, user.salt);
    if (candidateHash == user.passwordHash) {
        user.failedAttempts = 0;
        user.lockedUntilEpoch = 0;
        userRepo_.update(user); // persist reset state; login still succeeds if this fails
        return Result<User>::Ok(user);
    }

    // Wrong password: count the attempt and lock out if threshold crossed.
    user.failedAttempts += 1;
    if (user.failedAttempts >= maxAttempts_) {
        user.lockedUntilEpoch = now + lockoutSeconds_;
        userRepo_.update(user);
        return Result<User>::Fail(
            ErrorCode::AccountLocked,
            "Too many failed attempts. Account locked for " +
                std::to_string(lockoutSeconds_) + " second(s).");
    }

    userRepo_.update(user);
    int remaining = maxAttempts_ - user.failedAttempts;
    return Result<User>::Fail(
        ErrorCode::InvalidCredentials,
        "Incorrect password. " + std::to_string(remaining) + " attempt(s) remaining.");
}

} // namespace risms

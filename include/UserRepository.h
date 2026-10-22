#ifndef RISMS_USER_REPOSITORY_H
#define RISMS_USER_REPOSITORY_H

#include <string>
#include <vector>
#include "Result.h"
#include "User.h"

namespace risms {

// Loads/persists User records to a pipe-delimited local file:
//   username|passwordHash|salt|failedAttempts|lockedUntilEpoch|role
// No external database involved, per project scope.
class UserRepository {
public:
    explicit UserRepository(std::string filePath);

    // Loads all users from filePath_ into memory. If the file doesn't
    // exist yet, this is not an error (treated as zero users) so a fresh
    // checkout can bootstrap its own user file.
    Result<void> load();

    // Writes every in-memory user back to filePath_, overwriting it.
    Result<void> save() const;

    Result<User> findByUsername(const std::string& username) const;

    // Replaces the stored record for user.username with user and persists
    // immediately. Fails with UserNotFound if no such user exists yet
    // (use addUser for new accounts).
    Result<void> update(const User& user);

    // Adds a brand-new user and persists. Fails with DuplicateUser if the
    // username is already taken.
    Result<void> addUser(const User& user);

    size_t count() const { return users_.size(); }

private:
    std::string filePath_;
    std::vector<User> users_;

    static std::string serialize(const User& u);
    static Result<User> deserialize(const std::string& line);
};

} // namespace risms

#endif // RISMS_USER_REPOSITORY_H

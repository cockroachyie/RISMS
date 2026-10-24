#include "UserRepository.h"

#include <sstream>
#include <algorithm>

namespace risms {

UserRepository::UserRepository(std::string filePath)
    : filePath_(std::move(filePath)) {}

std::string UserRepository::serialize(const User& u) {
    std::ostringstream oss;
    oss << u.username << '|' << u.passwordHash << '|' << u.salt << '|'
        << u.failedAttempts << '|' << u.lockedUntilEpoch << '|' << u.role;
    return oss.str();
}

Result<User> UserRepository::deserialize(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, '|')) {
        fields.push_back(field);
    }

    if (fields.size() != 6) {
        return Result<User>::Fail(ErrorCode::FileIOError,
                                   "Malformed user record: " + line);
    }

    User u;
    u.username = fields[0];
    u.passwordHash = fields[1];
    u.salt = fields[2];
    try {
        u.failedAttempts = std::stoi(fields[3]);
        u.lockedUntilEpoch = std::stoll(fields[4]);
    } catch (...) {
        return Result<User>::Fail(ErrorCode::FileIOError,
                                   "Malformed numeric field in user record: " + line);
    }
    u.role = fields[5];

    return Result<User>::Ok(u);
}

Result<void> UserRepository::load() {
    users_.clear();

    auto linesResult = FileStore::readLines(filePath_);
    if (linesResult.isError()) {
        return Result<void>::Fail(linesResult.error(), linesResult.message());
    }

    for (const auto& line : linesResult.value()) {
        auto parsed = deserialize(line);
        if (parsed.isError()) {
            return Result<void>::Fail(parsed.error(), parsed.message());
        }
        users_.push_back(parsed.value());
    }

    return Result<void>::Ok();
}

Result<void> UserRepository::save() const {
    std::vector<std::string> lines;
    lines.reserve(users_.size());
    for (const auto& u : users_) {
        lines.push_back(serialize(u));
    }
    // Atomic write: readers (or a crash mid-write) never see a half-written
    // users file - they see either the previous version or the new one.
    return FileStore::writeLinesAtomic(filePath_, lines);
}

Result<User> UserRepository::findByUsername(const std::string& username) const {
    auto it = std::find_if(users_.begin(), users_.end(),
                            [&](const User& u) { return u.username == username; });
    if (it == users_.end()) {
        return Result<User>::Fail(ErrorCode::UserNotFound,
                                   "No user with username: " + username);
    }
    return Result<User>::Ok(*it);
}

Result<void> UserRepository::update(const User& user) {
    auto it = std::find_if(users_.begin(), users_.end(),
                            [&](const User& u) { return u.username == user.username; });
    if (it == users_.end()) {
        return Result<void>::Fail(ErrorCode::UserNotFound,
                                   "Cannot update unknown user: " + user.username);
    }
    *it = user;
    return save();
}

Result<void> UserRepository::addUser(const User& user) {
    auto it = std::find_if(users_.begin(), users_.end(),
                            [&](const User& u) { return u.username == user.username; });
    if (it != users_.end()) {
        return Result<void>::Fail(ErrorCode::DuplicateUser,
                                   "Username already exists: " + user.username);
    }
    users_.push_back(user);
    return save();
}

} // namespace risms

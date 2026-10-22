#ifndef RISMS_ERROR_CODE_H
#define RISMS_ERROR_CODE_H

#include <string>

namespace risms {

enum class ErrorCode {
    None = 0,
    InvalidInput,
    UserNotFound,
    InvalidCredentials,
    AccountLocked,
    FileIOError,
    ConfigError,
    DuplicateUser,
    Unknown
};

inline std::string toString(ErrorCode code) {
    switch (code) {
        case ErrorCode::None:               return "None";
        case ErrorCode::InvalidInput:        return "InvalidInput";
        case ErrorCode::UserNotFound:        return "UserNotFound";
        case ErrorCode::InvalidCredentials:  return "InvalidCredentials";
        case ErrorCode::AccountLocked:       return "AccountLocked";
        case ErrorCode::FileIOError:         return "FileIOError";
        case ErrorCode::ConfigError:         return "ConfigError";
        case ErrorCode::DuplicateUser:       return "DuplicateUser";
        default:                             return "Unknown";
    }
}

} // namespace risms

#endif // RISMS_ERROR_CODE_H

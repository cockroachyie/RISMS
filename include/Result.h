#ifndef RISMS_RESULT_H
#define RISMS_RESULT_H

#include <optional>
#include <string>
#include <stdexcept>
#include "ErrorCode.h"

namespace risms {

// A lightweight Result<T> type: either holds a value (success) or an
// ErrorCode + human-readable message (failure). Used instead of exceptions
// for expected/recoverable failures (bad input, missing user, locked out, etc).
template <typename T>
class Result {
public:
    static Result<T> Ok(T value) {
        Result<T> r;
        r.ok_ = true;
        r.value_ = std::move(value);
        r.error_ = ErrorCode::None;
        return r;
    }

    static Result<T> Fail(ErrorCode code, std::string message = "") {
        Result<T> r;
        r.ok_ = false;
        r.error_ = code;
        r.message_ = std::move(message);
        return r;
    }

    bool isOk() const { return ok_; }
    bool isError() const { return !ok_; }
    explicit operator bool() const { return ok_; }

    T& value() {
        if (!ok_) throw std::logic_error("Result::value() called on a failed Result");
        return value_.value();
    }
    const T& value() const {
        if (!ok_) throw std::logic_error("Result::value() called on a failed Result");
        return value_.value();
    }

    ErrorCode error() const { return error_; }
    const std::string& message() const { return message_; }

private:
    Result() = default;
    bool ok_ = false;
    std::optional<T> value_;
    ErrorCode error_ = ErrorCode::None;
    std::string message_;
};

// Specialization for operations that succeed/fail with no payload value.
template <>
class Result<void> {
public:
    static Result<void> Ok() {
        Result<void> r;
        r.ok_ = true;
        return r;
    }
    static Result<void> Fail(ErrorCode code, std::string message = "") {
        Result<void> r;
        r.ok_ = false;
        r.error_ = code;
        r.message_ = std::move(message);
        return r;
    }

    bool isOk() const { return ok_; }
    bool isError() const { return !ok_; }
    explicit operator bool() const { return ok_; }

    ErrorCode error() const { return error_; }
    const std::string& message() const { return message_; }

private:
    Result() = default;
    bool ok_ = false;
    ErrorCode error_ = ErrorCode::None;
    std::string message_;
};

} // namespace risms

#endif // RISMS_RESULT_H

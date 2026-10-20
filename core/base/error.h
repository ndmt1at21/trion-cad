#pragma once
// trion::Error và trion::Expected<T> (tập con của NT-009).
// Expected theo giao diện của std::expected (C++23) để sau này đổi thành bí danh.

#include "core/base/assert.h"

#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace trion {

struct Error {
    std::string module;  // "ui", "settings", "dwg"…
    std::string code;    // mã ổn định cho máy đọc, ví dụ "not-ui-thread", "comma"
    std::string message; // câu giải thích cho người dùng hoặc log
};

template <class T>
class [[nodiscard]] Expected {
public:
    Expected(T value) : v_(std::move(value)) {}
    Expected(Error error) : v_(std::move(error)) {}

    bool has_value() const noexcept { return v_.index() == 0; }
    explicit operator bool() const noexcept { return has_value(); }

    T& value() & { TRION_CHECK(has_value()); return std::get<0>(v_); }
    const T& value() const& { TRION_CHECK(has_value()); return std::get<0>(v_); }
    T&& value() && { TRION_CHECK(has_value()); return std::get<0>(std::move(v_)); }
    T& operator*() & { return value(); }
    const T& operator*() const& { return value(); }
    T* operator->() { return &value(); }
    const T* operator->() const { return &value(); }
    template <class U>
    T value_or(U&& fallback) const& { return has_value() ? std::get<0>(v_) : T(std::forward<U>(fallback)); }

    const Error& error() const { TRION_CHECK(!has_value()); return std::get<1>(v_); }

private:
    std::variant<T, Error> v_;
};

template <>
class [[nodiscard]] Expected<void> {
public:
    Expected() = default;
    Expected(Error error) : e_(std::move(error)) {}

    bool has_value() const noexcept { return !e_; }
    explicit operator bool() const noexcept { return has_value(); }
    const Error& error() const { TRION_CHECK(!has_value()); return *e_; }

private:
    std::optional<Error> e_;
};

} // namespace trion

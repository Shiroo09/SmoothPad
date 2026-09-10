#pragma once

#include <string>
#include <utility>
#include <variant>

namespace smoothpad {

template <typename T>
class Result {
 public:
  Result(T value) : data_(std::move(value)) {}
  static Result Fail(std::string error) {
    Result r;
    r.data_ = std::move(error);
    return r;
  }

  [[nodiscard]] bool ok() const noexcept { return std::holds_alternative<T>(data_); }
  [[nodiscard]] explicit operator bool() const noexcept { return ok(); }
  [[nodiscard]] T& value() { return std::get<T>(data_); }
  [[nodiscard]] const T& value() const { return std::get<T>(data_); }
  [[nodiscard]] const std::string& error() const { return std::get<std::string>(data_); }

 private:
  Result() : data_(std::string{}) {}
  std::variant<T, std::string> data_;
};

}  // namespace smoothpad

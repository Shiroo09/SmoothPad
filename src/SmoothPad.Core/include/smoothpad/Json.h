#pragma once

#include <cctype>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace smoothpad::json {

struct Value;

using Object = std::map<std::string, Value>;
using Array = std::vector<Value>;

struct Value {
  std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data{nullptr};

  Value() = default;
  Value(std::nullptr_t) : data(nullptr) {}
  Value(bool v) : data(v) {}
  Value(int v) : data(static_cast<double>(v)) {}
  Value(double v) : data(v) {}
  Value(const char* v) : data(std::string(v)) {}
  Value(std::string v) : data(std::move(v)) {}
  Value(Array v) : data(std::move(v)) {}
  Value(Object v) : data(std::move(v)) {}

  [[nodiscard]] bool is_null() const { return std::holds_alternative<std::nullptr_t>(data); }
  [[nodiscard]] bool is_bool() const { return std::holds_alternative<bool>(data); }
  [[nodiscard]] bool is_number() const { return std::holds_alternative<double>(data); }
  [[nodiscard]] bool is_string() const { return std::holds_alternative<std::string>(data); }
  [[nodiscard]] bool is_array() const { return std::holds_alternative<Array>(data); }
  [[nodiscard]] bool is_object() const { return std::holds_alternative<Object>(data); }

  [[nodiscard]] bool as_bool(bool fallback = false) const {
    return is_bool() ? std::get<bool>(data) : fallback;
  }
  [[nodiscard]] double as_number(double fallback = 0.0) const {
    return is_number() ? std::get<double>(data) : fallback;
  }
  [[nodiscard]] int as_int(int fallback = 0) const {
    return static_cast<int>(as_number(fallback));
  }
  [[nodiscard]] const std::string& as_string(const std::string& fallback) const {
    static const std::string empty;
    return is_string() ? std::get<std::string>(data) : (fallback.empty() ? empty : fallback);
  }
  [[nodiscard]] std::string as_string() const {
    return is_string() ? std::get<std::string>(data) : std::string{};
  }
  [[nodiscard]] const Object* as_object() const {
    return is_object() ? &std::get<Object>(data) : nullptr;
  }
  [[nodiscard]] Object* as_object() {
    return is_object() ? &std::get<Object>(data) : nullptr;
  }

  [[nodiscard]] const Value* get(std::string_view key) const {
    const auto* o = as_object();
    if (!o) return nullptr;
    const auto it = o->find(std::string(key));
    return it == o->end() ? nullptr : &it->second;
  }
};

[[nodiscard]] std::optional<Value> Parse(std::string_view text, std::string* error);
[[nodiscard]] std::string Stringify(const Value& v, bool pretty = true);

}  // namespace smoothpad::json

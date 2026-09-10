#include "smoothpad/Json.h"

#include <charconv>
#include <iomanip>
#include <sstream>

namespace smoothpad::json {
namespace {

struct Parser {
  std::string_view text;
  std::size_t i = 0;
  std::string* error = nullptr;

  void fail(const char* msg) {
    if (error && error->empty()) {
      *error = std::string(msg) + " at " + std::to_string(i);
    }
  }

  void skip() {
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) {
      ++i;
    }
  }

  bool match(char c) {
    skip();
    if (i < text.size() && text[i] == c) {
      ++i;
      return true;
    }
    return false;
  }

  std::optional<Value> parseValue() {
    skip();
    if (i >= text.size()) {
      fail("unexpected end");
      return std::nullopt;
    }
    const char c = text[i];
    if (c == '{') return parseObject();
    if (c == '[') return parseArray();
    if (c == '"') return parseString();
    if (c == 't' || c == 'f') return parseBool();
    if (c == 'n') return parseNull();
    if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
    fail("unexpected token");
    return std::nullopt;
  }

  std::optional<Value> parseObject() {
    if (!match('{')) {
      fail("expected '{'");
      return std::nullopt;
    }
    Object obj;
    skip();
    if (match('}')) {
      return Value{std::move(obj)};
    }
    for (;;) {
      skip();
      auto key = parseString();
      if (!key || !key->is_string()) {
        fail("expected object key");
        return std::nullopt;
      }
      if (!match(':')) {
        fail("expected ':'");
        return std::nullopt;
      }
      auto val = parseValue();
      if (!val) return std::nullopt;
      obj.emplace(key->as_string(), std::move(*val));
      skip();
      if (match('}')) break;
      if (!match(',')) {
        fail("expected ',' or '}'");
        return std::nullopt;
      }
    }
    return Value{std::move(obj)};
  }

  std::optional<Value> parseArray() {
    if (!match('[')) {
      fail("expected '['");
      return std::nullopt;
    }
    Array arr;
    skip();
    if (match(']')) {
      return Value{std::move(arr)};
    }
    for (;;) {
      auto val = parseValue();
      if (!val) return std::nullopt;
      arr.push_back(std::move(*val));
      skip();
      if (match(']')) break;
      if (!match(',')) {
        fail("expected ',' or ']'");
        return std::nullopt;
      }
    }
    return Value{std::move(arr)};
  }

  std::optional<Value> parseString() {
    if (!match('"')) {
      fail("expected string");
      return std::nullopt;
    }
    std::string out;
    while (i < text.size()) {
      const char c = text[i++];
      if (c == '"') {
        return Value{std::move(out)};
      }
      if (c == '\\') {
        if (i >= text.size()) {
          fail("unterminated escape");
          return std::nullopt;
        }
        const char e = text[i++];
        switch (e) {
          case '"':
          case '\\':
          case '/':
            out.push_back(e);
            break;
          case 'b':
            out.push_back('\b');
            break;
          case 'f':
            out.push_back('\f');
            break;
          case 'n':
            out.push_back('\n');
            break;
          case 'r':
            out.push_back('\r');
            break;
          case 't':
            out.push_back('\t');
            break;
          case 'u': {
            if (i + 4 > text.size()) {
              fail("bad unicode escape");
              return std::nullopt;
            }
            unsigned code = 0;
            for (int n = 0; n < 4; ++n) {
              const char h = text[i++];
              code <<= 4;
              if (h >= '0' && h <= '9') code |= static_cast<unsigned>(h - '0');
              else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned>(h - 'a' + 10);
              else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned>(h - 'A' + 10);
              else {
                fail("bad unicode escape");
                return std::nullopt;
              }
            }
            if (code < 0x80) {
              out.push_back(static_cast<char>(code));
            } else if (code < 0x800) {
              out.push_back(static_cast<char>(0xC0 | (code >> 6)));
              out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            } else {
              out.push_back(static_cast<char>(0xE0 | (code >> 12)));
              out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
              out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            break;
          }
          default:
            fail("bad escape");
            return std::nullopt;
        }
      } else {
        out.push_back(c);
      }
    }
    fail("unterminated string");
    return std::nullopt;
  }

  std::optional<Value> parseNumber() {
    skip();
    const std::size_t start = i;
    if (i < text.size() && text[i] == '-') ++i;
    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) ++i;
    if (i < text.size() && text[i] == '.') {
      ++i;
      while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) ++i;
    }
    if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
      ++i;
      if (i < text.size() && (text[i] == '+' || text[i] == '-')) ++i;
      while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) ++i;
    }
    double v = 0.0;
    const auto slice = text.substr(start, i - start);
    const auto r = std::from_chars(slice.data(), slice.data() + slice.size(), v);
    if (r.ec != std::errc{}) {
      fail("bad number");
      return std::nullopt;
    }
    return Value{v};
  }

  std::optional<Value> parseBool() {
    skip();
    if (text.substr(i, 4) == "true") {
      i += 4;
      return Value{true};
    }
    if (text.substr(i, 5) == "false") {
      i += 5;
      return Value{false};
    }
    fail("bad bool");
    return std::nullopt;
  }

  std::optional<Value> parseNull() {
    skip();
    if (text.substr(i, 4) == "null") {
      i += 4;
      return Value{nullptr};
    }
    fail("bad null");
    return std::nullopt;
  }
};

void Escape(std::ostream& os, std::string_view s) {
  os << '"';
  for (unsigned char c : s) {
    switch (c) {
      case '"':
        os << "\\\"";
        break;
      case '\\':
        os << "\\\\";
        break;
      case '\n':
        os << "\\n";
        break;
      case '\r':
        os << "\\r";
        break;
      case '\t':
        os << "\\t";
        break;
      default:
        if (c < 0x20) {
          os << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c)
             << std::dec;
        } else {
          os << static_cast<char>(c);
        }
        break;
    }
  }
  os << '"';
}

void Dump(std::ostream& os, const Value& v, bool pretty, int indent) {
  const auto pad = [&](int n) {
    if (pretty) {
      os << '\n';
      for (int i = 0; i < n; ++i) os << "  ";
    }
  };
  if (v.is_null()) {
    os << "null";
  } else if (v.is_bool()) {
    os << (v.as_bool() ? "true" : "false");
  } else if (v.is_number()) {
    os << std::setprecision(15) << v.as_number();
  } else if (v.is_string()) {
    Escape(os, v.as_string());
  } else if (v.is_array()) {
    const auto& a = std::get<Array>(v.data);
    os << '[';
    for (std::size_t i = 0; i < a.size(); ++i) {
      if (i) os << ',';
      pad(indent + 1);
      Dump(os, a[i], pretty, indent + 1);
    }
    if (!a.empty()) pad(indent);
    os << ']';
  } else if (v.is_object()) {
    const auto& o = std::get<Object>(v.data);
    os << '{';
    bool first = true;
    for (const auto& [k, val] : o) {
      if (!first) os << ',';
      first = false;
      pad(indent + 1);
      Escape(os, k);
      os << (pretty ? ": " : ":");
      Dump(os, val, pretty, indent + 1);
    }
    if (!o.empty()) pad(indent);
    os << '}';
  }
}

}  // namespace

std::optional<Value> Parse(std::string_view text, std::string* error) {
  Parser p{text, 0, error};
  auto v = p.parseValue();
  if (!v) return std::nullopt;
  p.skip();
  if (p.i != text.size()) {
    p.fail("trailing data");
    return std::nullopt;
  }
  return v;
}

std::string Stringify(const Value& v, bool pretty) {
  std::ostringstream os;
  Dump(os, v, pretty, 0);
  return os.str();
}

}  // namespace smoothpad::json

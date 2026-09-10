#pragma once

#include <functional>
#include <string>
#include <vector>

namespace smoothpad::test {

struct Case {
  const char* name;
  std::function<void()> fn;
};

std::vector<Case>& Registry();
extern int g_failed;
extern int g_passed;
void Fail(const char* file, int line, const char* expr);

struct Registrar {
  Registrar(const char* name, std::function<void()> fn) { Registry().push_back({name, std::move(fn)}); }
};

}  // namespace smoothpad::test

#define SP_TEST(name)                                                \
  static void name();                                                \
  static ::smoothpad::test::Registrar registrar_##name(#name, name); \
  static void name()

#define SP_CHECK(expr)                                    \
  do {                                                    \
    if (!(expr)) {                                        \
      ::smoothpad::test::Fail(__FILE__, __LINE__, #expr); \
    } else {                                              \
      ++::smoothpad::test::g_passed;                      \
    }                                                     \
  } while (0)

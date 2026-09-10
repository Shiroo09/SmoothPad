#include "Test.h"

#include <cstdio>

namespace smoothpad::test {

std::vector<Case>& Registry() {
  static std::vector<Case> r;
  return r;
}

int g_failed = 0;
int g_passed = 0;

void Fail(const char* file, int line, const char* expr) {
  std::fprintf(stderr, "  FAIL %s:%d  %s\n", file, line, expr);
  ++g_failed;
}

}  // namespace smoothpad::test

int main() {
  std::printf("SmoothPad tests: %zu cases\n", smoothpad::test::Registry().size());
  for (auto& c : smoothpad::test::Registry()) {
    std::printf("- %s\n", c.name);
    const int before = smoothpad::test::g_failed;
    c.fn();
    if (smoothpad::test::g_failed == before) {
      std::printf("  ok\n");
    }
  }
  std::printf("passed assertions: %d  failures: %d\n", smoothpad::test::g_passed, smoothpad::test::g_failed);
  return smoothpad::test::g_failed == 0 ? 0 : 1;
}

#include "harness.h"

#include <cstdio>

int main() {
  int failCount = 0;
  int total = 0;
  for (const auto &tc : ae::test::registry()) {
    ae::test::currentTestFailed() = false;
    ae::test::currentTestName() = tc.name;
    tc.fn();
    ++total;
    if (ae::test::currentTestFailed()) {
      ++failCount;
      std::fprintf(stderr, "[FALHOU] %s\n", tc.name);
    } else {
      std::printf("[ok] %s\n", tc.name);
    }
  }
  std::printf("\n%d/%d testes passaram\n", total - failCount, total);
  return failCount == 0 ? 0 : 1;
}

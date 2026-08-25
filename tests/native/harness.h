// Runner de testes mínimo, sem dependência externa (mesma filosofia do lado
// C#: ver tests/Aether.Tests).
#pragma once

#include <cstdio>
#include <string>
#include <vector>

namespace ae::test {

using TestFn = void (*)();

struct TestCase {
  const char *name;
  TestFn fn;
};

inline std::vector<TestCase> &registry() {
  static std::vector<TestCase> tests;
  return tests;
}

struct Registrar {
  Registrar(const char *name, TestFn fn) { registry().push_back({name, fn}); }
};

// Exceção não é usada (build roda com -fno-exceptions): falha de asserção
// registra e usa longjmp-like via retorno antecipado com flag global.
inline bool &currentTestFailed() {
  static bool failed = false;
  return failed;
}

inline const char *&currentTestName() {
  static const char *name = "";
  return name;
}

#define AE_TEST(name)                                                        \
  static void name();                                                        \
  static ::ae::test::Registrar name##_registrar(#name, &name);               \
  static void name()

#define AE_EXPECT_TRUE(cond, msg)                                             \
  do {                                                                        \
    if (!(cond)) {                                                           \
      std::fprintf(stderr, "  FALHA em %s:%d: %s\n", __FILE__, __LINE__, msg);\
      ::ae::test::currentTestFailed() = true;                                \
      return;                                                                \
    }                                                                        \
  } while (0)

#define AE_EXPECT_EQ(a, b, msg)                                              \
  do {                                                                       \
    if (!((a) == (b))) {                                                    \
      std::fprintf(stderr, "  FALHA em %s:%d: %s\n", __FILE__, __LINE__, msg);\
      ::ae::test::currentTestFailed() = true;                                \
      return;                                                                \
    }                                                                        \
  } while (0)

} // namespace ae::test

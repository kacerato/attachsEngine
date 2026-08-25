#include "core/base.h"

#include <cstdio>
#include <cstdlib>

namespace ae::detail {

[[noreturn]] void checkFail(const char *cond, const char *msg,
                             const char *file, int line) {
  // stderr e não stdout: em dispositivos móveis o log de erro precisa ir
  // para o canal que as ferramentas de crash-report capturam.
  std::fprintf(stderr, "AE_CHECK falhou: %s\n  motivo: %s\n  em %s:%d\n", cond,
               msg, file, line);
  std::fflush(stderr);
  std::abort();
}

} // namespace ae::detail

// Tipos base e macros de assert do núcleo nativo do Aether.
//
// Convenção: sem RTTI, sem exceções (ver native/CMakeLists.txt). Erros de
// caminho quente nunca lançam — retornam código de erro ou abortam via
// AE_CHECK quando a invariante violada é irrecuperável.
#pragma once

#include <cstdint>
#include <cstddef>

namespace ae {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using usize = std::size_t;

} // namespace ae

// AE_CHECK é o assert que sobrevive ao build de release: a engine roda no
// celular do usuário, e um crash silencioso lá é pior que uma verificação
// que custa alguns ciclos. Ele nunca é compilado para fora.
#define AE_CHECK(cond, msg)                                                   \
  do {                                                                        \
    if (!(cond)) {                                                           \
      ::ae::detail::checkFail(#cond, msg, __FILE__, __LINE__);               \
    }                                                                        \
  } while (0)

// AE_ASSERT só existe em builds de debug — custo zero em release, para
// invariantes caras de verificar que não protegem a estabilidade do usuário
// final, apenas ajudam o desenvolvedor.
#ifdef NDEBUG
#define AE_ASSERT(cond, msg) ((void)0)
#else
#define AE_ASSERT(cond, msg) AE_CHECK(cond, msg)
#endif

namespace ae::detail {

// Implementado em assert.cpp. Sem exceções: imprime e chama std::abort().
[[noreturn]] void checkFail(const char *cond, const char *msg,
                             const char *file, int line);

} // namespace ae::detail

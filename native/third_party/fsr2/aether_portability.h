// Shim de portabilidade da Astra para compilar o FSR 2 v2.2.1 com clang/NDK.
// Não é modificação do código da AMD: entra por -include só neste alvo e
// fornece as duas extensões do MSVC que o fonte usa (_countof e wcscpy_s de
// dois argumentos sobre array), além de <locale>/<codecvt>, que o fonte usa
// sem incluir (o MSVC os traz transitivamente).
#pragma once
#ifdef __cplusplus
#include <cstddef>
#include <cwchar>
#include <locale>
#include <codecvt>
#ifndef _countof
#define _countof(array) (sizeof(array) / sizeof((array)[0]))
#endif
template <std::size_t N> inline int wcscpy_s(wchar_t (&destination)[N], const wchar_t *source) {
  if (!source) { destination[0] = L'\0'; return 0; }
  std::wcsncpy(destination, source, N - 1);
  destination[N - 1] = L'\0';
  return 0;
}
#endif

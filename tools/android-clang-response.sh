#!/usr/bin/env bash
# Clang's NDK .cmd launcher has an 8K command-line limit on Windows. OpenSSL's
# link contains hundreds of objects; call clang.exe with a response file instead.
set -euo pipefail
response="$(mktemp "$PWD/.astra-clang-XXXXXX.rsp")"
trap 'rm -f -- "$response"' EXIT
for argument in "$@"; do
  # The private extension is not a standard -l search name. Bind the shared
  # crypto library explicitly, avoiding a second static crypto inside libssl.
  if [[ "$argument" == -lcrypto ]]; then argument=-l:libcrypto.so.astra.so; fi
  argument="${argument//\\/\\\\}"
  argument="${argument//\"/\\\"}"
  printf '"%s"\n' "$argument" >> "$response"
done
if command -v cygpath >/dev/null 2>&1; then response="$(cygpath -m "$response")"; fi
clang --target=aarch64-linux-android26 "@$response"

#pragma once

#include <android/log.h>
#include <cstdint>
#include <cstdlib>
#include <dlfcn.h>
#include <unwind.h>

namespace ae::platform::android {

// Diagnóstico de saída pelo exit(): o processo some sem tombstone, os
// destrutores estáticos rodam com as threads de renderização do sistema ainda
// vivas e o único rastro é um "FORTIFY: pthread_mutex_lock called on a
// destroyed mutex". Quem chamou exit() (runtime .NET com exceção não tratada,
// alocador do Box2D sem memória, biblioteca de terceiros) fica invisível.
// O handler de atexit roda dentro do próprio exit(): a pilha dele mostra o
// chamador. Vai para o logcat (tag Astra, FATAL), uma linha por quadro.
namespace exit_trace_detail {

struct Frames {
  uintptr_t pc[48];
  int count = 0;
};

inline _Unwind_Reason_Code collect(_Unwind_Context *context, void *argument) {
  auto *frames = static_cast<Frames *>(argument);
  const uintptr_t pc = _Unwind_GetIP(context);
  if (pc != 0) {
    if (frames->count == static_cast<int>(sizeof(frames->pc) / sizeof(frames->pc[0]))) return _URC_END_OF_STACK;
    frames->pc[frames->count++] = pc;
  }
  return _URC_NO_REASON;
}

inline void logExitCaller() {
  Frames frames;
  _Unwind_Backtrace(collect, &frames);
  __android_log_print(ANDROID_LOG_FATAL, "Astra", "[Exit] exit() chamado; pilha (%d quadros):", frames.count);
  for (int i = 0; i < frames.count; ++i) {
    Dl_info info{};
    const void *pc = reinterpret_cast<const void *>(frames.pc[i]);
    if (dladdr(pc, &info) != 0 && info.dli_fname != nullptr) {
      const uintptr_t offset = frames.pc[i] - reinterpret_cast<uintptr_t>(info.dli_fbase);
      __android_log_print(ANDROID_LOG_FATAL, "Astra", "[Exit]   #%02d %s+0x%zx %s", i, info.dli_fname,
                          static_cast<size_t>(offset), info.dli_sname != nullptr ? info.dli_sname : "");
    } else {
      __android_log_print(ANDROID_LOG_FATAL, "Astra", "[Exit]   #%02d %p", i, pc);
    }
  }
}

} // namespace exit_trace_detail

// Registrar no início do android_main, antes do runtime gerenciado.
inline void installExitTrace() {
  static bool installed = false;
  if (installed) return;
  installed = std::atexit(exit_trace_detail::logExitCaller) == 0;
}

} // namespace ae::platform::android

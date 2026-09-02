#pragma once

#include "core/base.h"

#include <string_view>

namespace ae::platform {

// Snapshot Linux/Android em bytes. Os campos permanecem zero quando o kernel
// não publica a chave correspondente; valid só é true quando o conjunto
// mínimo (RSS para processo, total/disponível para sistema) foi lido.
struct ProcessMemorySnapshot final {
  bool valid = false;
  u64 virtualBytes = 0;
  u64 residentBytes = 0;
  u64 peakResidentBytes = 0;
  u64 anonymousBytes = 0;
  u64 fileBytes = 0;
  u64 sharedBytes = 0;
  u64 swapBytes = 0;
};

struct SystemMemorySnapshot final {
  bool valid = false;
  u64 totalBytes = 0;
  u64 availableBytes = 0;
};

// Parsers puros para que o contrato /proc seja testável fora do Android.
bool parseProcStatus(std::string_view text, ProcessMemorySnapshot &out);
bool parseProcMemInfo(std::string_view text, SystemMemorySnapshot &out);

// Leitura de baixo custo usada somente no fechamento de cada janela de
// profiling (600 frames), nunca no hot path de cada frame.
bool readProcessMemorySnapshot(ProcessMemorySnapshot &out);
bool readSystemMemorySnapshot(SystemMemorySnapshot &out);

} // namespace ae::platform

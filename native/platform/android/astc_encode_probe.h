#pragma once

#include "rhi/device.h"
#include <limits>

namespace ae::platform::android {

// PoC-E do plano (item 0.2): "compressão ASTC em GPU — < 300 ms para 4096x4096". Executa o
// compute shader astc_encode.comp (native/rhi/shaders/astc_encode.comp) sobre uma textura
// sintética até 4096x4096 no processo Android, usando VulkanDevice do RHI. O chamador precisa
// garantir uso exclusivo da fila durante o probe. Mede o tempo
// real via GPU timestamp queries, e valida todos os texels através do PRÓPRIO
// hardware (criando uma imagem VK_FORMAT_ASTC_4x4_UNORM_BLOCK com os bytes gerados e lendo de
// volta via blit para RGBA8) — o driver Adreno real é o "decoder de referência" desta validação,
// não uma reimplementação própria do decodificador.
//
// Chamado sob demanda pelo worker de diagnóstico em android_main.cpp, com device/fila próprios.
// Não participa do loop de render. GPU time não inclui criação de recursos, upload ou readback.
// O corpus RGB opaco mistura blocos sólidos, rampas e checker; não é um importador de produção.
// `succeeded` exige medição válida E comparação completa dentro da tolerância estrutural.
struct AstcEncodeProbeResult {
  bool succeeded = false;
  double encodeMilliseconds = std::numeric_limits<double>::quiet_NaN();
  bool timingValid = false;
  uint32_t blockCount = 0;
  bool hardwareDecodeMatchesSource = false;
  float maxChannelDifference = 0.0f; // maior diferença em qualquer canal RGBA de qualquer texel, [0,255]
  uint64_t testedTexels = 0;
};

AstcEncodeProbeResult runAstcEncodeProbe(rhi::VulkanDevice &device, uint32_t textureWidth, uint32_t textureHeight);

} // namespace ae::platform::android

#pragma once

#include "rhi/device.h"

namespace ae::platform::android {

// PoC-E do plano (item 0.2): "compressão ASTC em GPU — < 300 ms para 4096x4096". Executa o
// compute shader astc_encode.comp (native/rhi/shaders/astc_encode.comp) sobre uma textura
// sintética 4096x4096 dentro do processo do shell Android já validado (mesmo VulkanDevice de
// AndroidVulkanSurface, não um caminho de inicialização Vulkan novo/não testado), mede o tempo
// real via GPU timestamp queries, e valida a saída decodificando um bloco através do PRÓPRIO
// hardware (criando uma imagem VK_FORMAT_ASTC_4x4_UNORM_BLOCK com os bytes gerados e lendo de
// volta via blit para RGBA8) — o driver Adreno real é o "decoder de referência" desta validação,
// não uma reimplementação própria do decodificador.
//
// Chamado uma vez, sob demanda, a partir de android_main.cpp — não é executado no loop de frame
// nem faz parte do pipeline de render de produto; existe só para coletar a evidência de
// viabilidade deste PoC. Ver docs/ESTADO.md para o resultado medido em hardware.
struct AstcEncodeProbeResult {
  bool succeeded = false;
  double encodeMilliseconds = 0.0;
  uint32_t blockCount = 0;
  bool hardwareDecodeMatchesSource = false;
  float maxChannelDifference = 0.0f; // maior |decodificado - original| entre os canais RGB amostrados, [0,255]
};

AstcEncodeProbeResult runAstcEncodeProbe(rhi::VulkanDevice &device, uint32_t textureWidth, uint32_t textureHeight);

} // namespace ae::platform::android

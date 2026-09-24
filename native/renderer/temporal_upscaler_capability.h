#pragma once
// Disponibilidade dos ampliadores temporais a partir de fatos do dispositivo.
//
// Lógica pura: o renderer preenche a sonda com o que o VkDevice HABILITOU e
// com os formatos que a cena realmente usa; a política e o painel leem o
// motivo. Os requisitos vêm das capacidades SPIR-V dos shaders vendorizados
// (third_party/arm_asr e third_party/fsr2), levantadas com spirv-dis:
//
// | Biblioteca | Exige |
// |---|---|
// | Arm ASR 25.06 | Float16 e Int16 em todos os passes; quad em compute e formatos estendidos na pirâmide de luminância; profundidade D32_SFLOAT (o backend cria a própria vista D32) |
// | AMD FSR 2.2.1 | formatos estendidos e escrita sem formato; GroupNonUniform + quad em compute; Int16 sempre que Float16 estiver habilitado (a biblioteca troca para as permutações de 16 bits sozinha) |
//
// As duas exigem cor de cena HDR RGBA16F: o renderer só lhes entrega luz linear.
#include "renderer/rendering_policy.h"

namespace ae::renderer {

struct TemporalUpscalerProbe final {
  bool armAsrBuilt = false;
  bool fsr2Built = false;
  bool shaderFloat16 = false;
  bool shaderInt16 = false;
  bool storageImageExtendedFormats = false;
  bool storageImageWriteWithoutFormat = false;
  bool computeSubgroupBasic = false;
  bool computeSubgroupQuad = false;
  bool hdrSceneColor = false;
  bool sampledDepth32 = false;
};

TemporalUpscalerAvailability probeArmAsr(const TemporalUpscalerProbe &probe);
TemporalUpscalerAvailability probeFsr2(const TemporalUpscalerProbe &probe);

} // namespace ae::renderer

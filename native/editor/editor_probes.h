#pragma once
// PK-6: sondas no editor (editor_probes.cpp). O bake segue o fluxo da Unity:
// "Assar" numa Reflection Probe Assada captura a cena (só objetos Static) e
// grava o cubemap em Iluminação/; "Assar sondas" num Light Probe Group captura
// cada ponto de todos os grupos da cena e grava um SH L2 por ponto. A captura
// acontece no renderer do aparelho; o editor só pede, recebe e grava.
#include "core/base.h"
#include "renderer/light_probes.h"
#include "renderer/reflection_probes.h"

#include <string>
#include <vector>

namespace ae::editor {

// Espelho do pedido do renderer Android, sem depender dele (o editor também
// compila no host).
struct EditorProbeCaptureRequest {
  u64 id = 0;
  u32 kind = 0; // 0 reflexo, 1 irradiância
  u64 objectId = 0, instance = 0;
  float position[3]{};
  u32 resolution = 128;
  float nearClip = .3f, farClip = 1000;
  u32 cullingMask = ~0u;
  bool staticOnly = true;
  bool solidBackground = false;
  float background[3]{};
  bool hdr = true;
};
struct EditorProbeCaptureResult {
  u64 id = 0;
  u32 kind = 0;
  bool success = false;
  std::string error;
  renderer::ReflectionProbeTexture texture;
  renderer::CubeFaces faces;
};
// O que o renderer mediu no último quadro, para o cartão do Inspector.
struct EditorProbeRendererStats {
  u32 sceneProbes = 0, residentProbes = 0, readyProbes = 0, facesCaptured = 0, mipsFiltered = 0;
  u32 drawsWithLightProbes = 0, drawsWithReflection = 0, lightProbes = 0, tetrahedra = 0;
  float cpuMs = 0;
  bool valid = false;
};

// Estado do componente de sonda selecionado, lido pelo Inspector e pelo viewport.
struct EditorProbeView {
  u64 entity = 0;
  u64 instance = 0;
  bool reflection = false, lightGroup = false;
  bool baking = false, hasBake = false, missing = false, stale = false;
  float progress = -1;
  float bakeSeconds = 0;
  std::string title, detail;
  // Light Probe Group: modo de edição (Edit Light Probes da Unity) e seleção.
  bool editing = false;
  u64 selectedPoint = 0;
  u32 selectedIndex = 0;
  u32 pointCount = 0;
  EditorProbeRendererStats stats{};
};

// Resolução da captura por ponto de sonda de luz: 32 px por face basta para L2.
inline constexpr u32 LightProbeCaptureResolution = 32;

} // namespace ae::editor

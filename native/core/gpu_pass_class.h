// Classes de passe de GPU medidas por frame — fonte única do contrato.
//
// Existe porque três lugares precisam concordar sobre "quais regiões o frame
// tem": o timer de timestamps (rhi), o relatório de perfil (profiler) e os
// marcadores de debug lidos por AGI/RenderDoc. Enquanto essas listas eram
// mantidas em paralelo à mão, nada impedia que um marcador de captura
// apontasse para uma região diferente da métrica de mesmo nome — o modo de
// falha exato que faz uma captura mentir sem erro nenhum.
//
// Vive em core/ e não em rhi/ porque profiler/ não pode depender de Vulkan.
#pragma once

#include "core/base.h"

namespace ae {

// A ordem É a ordem de gravação no frame: o timer grava um timestamp ao fim de
// cada classe e só aceita marcas em ordem crescente. Reordenar este enum
// reordena o significado das medições, então mudança aqui exige mudar a ordem
// real dos comandos no renderer.
//
// Uma classe sem trabalho no frame permanece com duração zero — nunca some do
// relatório. Zero é um dado ("não custou nada nesta pose"); ausência não é.
enum class GpuPassClass : u32 {
  Opaque,      // geometria opaca sólida (depth write, front-to-back)
  Coverage,    // prepass + shade de alpha-mask: a vegetação da cena-base
  Sky,         // céu em um triângulo fullscreen
  Transparent, // BLEND ordenado traseira→frontal
  Ui,          // HUD/joystick, dentro do render pass principal
  Hzb,         // cadeia de redução Hi-Z, fora do render pass principal
  Count
};
constexpr u32 GpuPassClassCount = static_cast<u32>(GpuPassClass::Count);

// Nome curto do marcador de debug. É o texto que aparece na árvore de uma
// captura AGI/RenderDoc; sem ele a captura é um bloco único e não atribui
// custo a nada.
constexpr const char *GpuPassClassLabels[] = {"Opaque", "Coverage", "Sky",
                                              "Transparent", "UI",   "HZB"};

// Nome da métrica no relatório de perfil. Fica ao lado do rótulo de propósito:
// a captura e o relatório precisam usar o mesmo vocabulário para que um possa
// verificar o outro.
constexpr const char *GpuPassClassMetricNames[] = {
    "gpu_opaque_ms", "gpu_coverage_ms", "gpu_sky_ms",
    "gpu_transparent_ms", "gpu_ui_ms", "gpu_hzb_ms"};

// Cor do marcador, só para leitura humana da captura. Opacos frios, vegetação
// verde, céu azul claro, transparência âmbar, UI cinza, HZB roxo.
constexpr float GpuPassClassLabelColors[][3] = {
    {0.24f, 0.52f, 0.86f}, {0.35f, 0.72f, 0.36f}, {0.52f, 0.78f, 0.95f},
    {0.93f, 0.68f, 0.24f}, {0.66f, 0.66f, 0.70f}, {0.62f, 0.40f, 0.85f}};

static_assert(sizeof(GpuPassClassLabels) / sizeof(GpuPassClassLabels[0]) ==
                  GpuPassClassCount,
              "GpuPassClassLabels precisa de uma entrada por classe de passe.");
static_assert(sizeof(GpuPassClassMetricNames) / sizeof(GpuPassClassMetricNames[0]) ==
                  GpuPassClassCount,
              "GpuPassClassMetricNames precisa de uma entrada por classe de passe.");
static_assert(sizeof(GpuPassClassLabelColors) / sizeof(GpuPassClassLabelColors[0]) ==
                  GpuPassClassCount,
              "GpuPassClassLabelColors precisa de uma entrada por classe de passe.");

constexpr const char *gpuPassClassLabel(GpuPassClass pass) {
  return GpuPassClassLabels[static_cast<u32>(pass)];
}

constexpr const char *gpuPassClassMetricName(GpuPassClass pass) {
  return GpuPassClassMetricNames[static_cast<u32>(pass)];
}

} // namespace ae

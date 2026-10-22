#pragma once
#include "core/base.h"
#include "resources/asset_registry.h"
#include "scene/material_parameters.h"
#include <array>
#include <string>
#include <string_view>

namespace ae::resources {
// Material do projeto (M07/M08, Entrega 2): recurso com identidade, revisão e
// arquivo próprio (`ASTRA_MATERIAL <versão>`, em `Materiais/<nome>.material`).
//
// É o alcance COMPARTILHADO de um slot: editar o recurso muda todos os slots que
// o referenciam, em qualquer instância. A substituição local continua no slot
// (`MaterialParameters::enabled`) e vence o recurso.
//
// Versão 1: fatores escalares (cor, rugosidade, metálico, normal, especular,
// emissão). Versão 2 (R4): mais as texturas dos quatro bindings por identidade
// de recurso de textura — "-" herda a da fonte, "none" tira. Versão 3 (R4): mais
// modo de alfa, faces e corte (0 herda). Versão 4 (R4): mais a amostragem de cada
// binding (UV, repetição, filtro; "000" herda). Versão 5 (R4): mais a
// transformação de UV de cada binding (deslocamento, escala, rotação). Arquivos
// antigos leem sem essas trocas; gravar sempre escreve a versão atual.
struct MaterialAsset {
  // Versão 6 (R4): canais, oclusão (origem, força, textura própria), inversão Y
  // do normal e origem do alfa. Versão 7 (G3a): origem da rugosidade
  // (rugosidade ou suavidade do Standard da Unity). Versão 8 (PK-9): shader
  // (Lit, Complex Lit, Simple Lit, Unlit, Particles Lit/Unlit) e as opções do
  // URP Lit: workflow, mistura do transparente, Render Face de trás, prioridade,
  // altura, detalhe, clear coat e as texturas extras. Antes dela tudo é Lit.
  static constexpr u32 FormatVersion = 8;
  AssetGuid guid;
  u32 revision = 1;
  std::string name;
  scene::MaterialParameters values;
  std::array<AssetGuid, scene::MaterialTextureCount> textures{};
  scene::MaterialSurface surface{};
  std::array<scene::MaterialSampling, scene::MaterialTextureCount> sampling{};
  scene::MaterialChannels channels{};
  AssetGuid occlusionTexture{};
  scene::MaterialShading shading{};
  // Altura, máscara de detalhe, detalhe de cor, normal de detalhe e mapa do
  // clear coat. Inválida = sem textura (o recurso não tem fonte de onde herdar).
  std::array<AssetGuid, scene::MaterialExtraTextureCount> extraTextures{};
  bool valid() const;
  // Qualquer binding, a oclusão própria ou uma textura extra.
  bool usesTexture(const AssetGuid &texture) const {
    if (!texture.valid()) return false;
    for (const auto &value : textures) if (value == texture) return true;
    for (const auto &value : extraTextures) if (value == texture) return true;
    return occlusionTexture == texture;
  }
  std::string serialize() const;
  static bool deserialize(std::string_view text, MaterialAsset &out);
};
} // namespace ae::resources

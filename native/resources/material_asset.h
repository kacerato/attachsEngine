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
// modo de alfa, faces e corte (0 herda). Arquivos antigos leem sem essas trocas;
// gravar sempre escreve a versão atual.
struct MaterialAsset {
  static constexpr u32 FormatVersion = 3;
  AssetGuid guid;
  u32 revision = 1;
  std::string name;
  scene::MaterialParameters values;
  std::array<AssetGuid, scene::MaterialTextureCount> textures{};
  scene::MaterialSurface surface{};
  bool valid() const;
  std::string serialize() const;
  static bool deserialize(std::string_view text, MaterialAsset &out);
};
} // namespace ae::resources

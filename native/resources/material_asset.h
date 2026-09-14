#pragma once
#include "core/base.h"
#include "resources/asset_registry.h"
#include "scene/material_parameters.h"
#include <string>
#include <string_view>

namespace ae::resources {
// Material do projeto (M07/M08, Entrega 2): recurso com identidade, revisão e
// arquivo próprio (`ASTRA_MATERIAL 1`, em `Materiais/<nome>.material`).
//
// É o alcance COMPARTILHADO de um slot: editar o recurso muda todos os slots que
// o referenciam, em qualquer instância. A substituição local continua no slot
// (`MaterialParameters::enabled`) e vence o recurso.
//
// Perfil desta entrega: fatores escalares (cor, rugosidade, metálico, normal,
// especular, emissão). Texturas, modo de alfa e dupla face chegam na Entrega 3;
// o formato versionado existe para recebê-los sem adivinhar arquivos antigos.
struct MaterialAsset {
  static constexpr u32 FormatVersion = 1;
  AssetGuid guid;
  u32 revision = 1;
  std::string name;
  scene::MaterialParameters values;
  bool valid() const;
  std::string serialize() const;
  static bool deserialize(std::string_view text, MaterialAsset &out);
};
} // namespace ae::resources

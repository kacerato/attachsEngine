#pragma once

#include "resources/asset_registry.h"

#include <string>
#include <string_view>

namespace ae::resources {

// Material físico compartilhado: o que a superfície faz no contato. Mesmo
// modelo do Perfil de ambiente: o corpo guarda uma cópia dos valores e o GUID;
// editar o recurso sincroniza as cópias, e recurso ausente conserva a cópia.
//
// Referências: Godot 4.5 PhysicsMaterial (atribuído ao corpo)
// https://docs.godotengine.org/en/4.5/classes/class_physicsmaterial.html
// Unity 6000.0 PhysicsMaterial (combinação de atrito e quique)
// https://docs.unity3d.com/6000.0/Documentation/Manual/class-PhysicsMaterial.html
struct PhysicsMaterialAsset final {
  static constexpr u32 FormatVersion = 1;
  AssetGuid guid{};
  u32 revision = 1;
  std::string name;
  float friction = .5f, restitution = 0;
  // 0 padrão do motor, 1 média, 2 mínimo, 3 multiplicar, 4 máximo.
  u32 frictionCombine = 0, restitutionCombine = 0;

  bool valid() const;
  std::string serialize() const;
  static bool deserialize(std::string_view text, PhysicsMaterialAsset &out);
};

} // namespace ae::resources

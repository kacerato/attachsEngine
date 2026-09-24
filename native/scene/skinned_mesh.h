#pragma once
#include "resources/skeletal_animation.h"
#include "scene/components.h"

#include <vector>

namespace ae::scene {
// Deformação por esqueleto sobre a Malha do mesmo objeto (G6-B).
//
// Na Unity o SkinnedMeshRenderer substitui o MeshRenderer. Aqui a Malha
// continua dona da geometria, dos slots e dos materiais, e este componente
// acrescenta só o que é de skin: a lista de ossos (objetos da cena, na ordem
// das juntas do skin da fonte), a qualidade (Skin Weights) e se o vetor de
// movimento do desenho considera a deformação (skinnedMotionVectors). A
// definição do skin — bind inversa e esferas por junta — é dado imutável da
// fonte importada e é resolvida pelo recurso do slot 0 da Malha.
//
// Adaptação explícita: rootBone e updateWhenOffscreen não existem porque os
// limites saem das esferas por junta (não da caixa do rootBone) e a paleta é
// avaliada para todo desenho visível ou não, sem custo de vértice na CPU.
enum class SkinQuality : u32 { Auto = 0, One = 1, Two = 2, Four = 4 };

class SkinnedMesh final : public ComponentValue {
public:
  static constexpr usize MaximumBones = resources::MaximumSkinJoints;
  // Objetos da cena na ordem das juntas; zero é osso ausente (a paleta usa a
  // pose de bind dele, e o Inspector relata a ausência).
  std::vector<u64> bones;
  SkinQuality quality = SkinQuality::Auto;
  bool skinnedMotionVectors = true;

  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<SkinnedMesh>(*this); }
  // Influências que o compute usa por vértice: Auto e Four são as 4 do import.
  u32 influences() const noexcept { return quality == SkinQuality::Auto ? 4u : static_cast<u32>(quality); }
  bool valid() const override {
    const auto q = static_cast<u32>(quality);
    return bones.size() <= MaximumBones && (q == 0 || q == 1 || q == 2 || q == 4);
  }
  void write(std::ostream &out) const override {
    out << static_cast<u32>(quality) << ' ' << skinnedMotionVectors << ' ' << bones.size();
    for (const auto bone : bones) out << ' ' << bone;
  }
  bool read(std::istream &in, u32 version) override {
    u32 q = 0;
    usize count = 0;
    if (version != 1 || !(in >> q >> skinnedMotionVectors >> count) || count > MaximumBones) return false;
    quality = static_cast<SkinQuality>(q);
    bones.assign(count, 0);
    for (auto &bone : bones) if (!(in >> bone)) return false;
    return valid();
  }
};

inline constexpr std::array<ComponentEnumOption, 4> skinQualityOptions{{
  {0, "Automática"}, {1, "1 osso"}, {2, "2 ossos"}, {4, "4 ossos"}}};
inline constexpr std::array<ComponentEnum, 1> skinnedMeshEnums{{
  {"quality", "Qualidade", skinQualityOptions,
   [](const ComponentValue &v) { return static_cast<u32>(static_cast<const SkinnedMesh &>(v).quality); },
   [](ComponentValue &v, u32 value) { static_cast<SkinnedMesh &>(v).quality = static_cast<SkinQuality>(value); },
   {"Skin", "", "Influências por vértice usadas na deformação (Skin Weights)"}}
}};
inline constexpr std::array<ComponentBoolean, 1> skinnedMeshBooleans{{
  {"skinned_motion_vectors", "Vetor de movimento do skin",
   [](const ComponentValue &v) { return static_cast<const SkinnedMesh &>(v).skinnedMotionVectors; },
   [](ComponentValue &v, bool value) { static_cast<SkinnedMesh &>(v).skinnedMotionVectors = value; },
   {"Skin", "", "A reprojeção temporal usa a pose anterior dos ossos, não só a do objeto",
    nullptr, nullptr, "render.motion_vectors", "platform/android/instanced_motion.inl → passe de movimento com pose anterior"}}
}};
inline const ComponentType SkinnedMesh::descriptor{
  "astra.render.skinned_mesh", 1, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<SkinnedMesh>(); },
  {}, skinnedMeshBooleans, skinnedMeshEnums
};
} // namespace ae::scene

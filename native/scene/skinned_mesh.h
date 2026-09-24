#pragma once
#include "resources/skeletal_animation.h"
#include "scene/components.h"

#include <array>
#include <cmath>
#include <vector>

namespace ae::scene {
// Deformação da Malha do mesmo objeto (G6-B): esqueleto e blend shapes.
//
// Na Unity o SkinnedMeshRenderer substitui o MeshRenderer e carrega os dois
// (bones + SetBlendShapeWeight). Aqui a Malha continua dona da geometria, dos
// slots e dos materiais, e este componente acrescenta só o que deforma: os
// ossos (objetos da cena, na ordem das juntas do skin da fonte), a qualidade
// (Skin Weights), os pesos dos blend shapes (0..100, a escala da Unity) e se o
// vetor de movimento considera a deformação (skinnedMotionVectors). A definição
// do skin e os deltas dos blend shapes são dado imutável da fonte, resolvidos
// pelo recurso do slot 0 da Malha.
//
// Adaptação explícita: rootBone e updateWhenOffscreen não existem porque os
// limites saem das esferas por junta e do deslocamento máximo de cada blend
// shape, e a deformação roda para todo desenho, visível ou não.
enum class SkinQuality : u32 { Auto = 0, One = 1, Two = 2, Four = 4 };

class SkinnedMesh final : public ComponentValue {
public:
  static constexpr usize MaximumBones = resources::MaximumSkinJoints;
  static constexpr usize MaximumBlendShapes = resources::MaximumMorphTargets;
  static constexpr float MaximumBlendShapeWeight = 1000;
  // Objetos da cena na ordem das juntas; zero é osso ausente (a paleta usa a
  // pose de bind dele, e o Inspector relata a ausência).
  std::vector<u64> bones;
  // Um peso por blend shape da malha, 0..100 (100 = alvo inteiro). Valores
  // fora dessa faixa extrapolam, como a Unity permite.
  std::vector<float> blendShapeWeights;
  SkinQuality quality = SkinQuality::Auto;
  bool skinnedMotionVectors = true;

  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<SkinnedMesh>(*this); }
  // Influências que o compute usa por vértice: Auto e Four são as 4 do import.
  u32 influences() const noexcept { return quality == SkinQuality::Auto ? 4u : static_cast<u32>(quality); }
  bool valid() const override {
    const auto q = static_cast<u32>(quality);
    if (bones.size() > MaximumBones || blendShapeWeights.size() > MaximumBlendShapes) return false;
    for (const float weight : blendShapeWeights)
      if (!std::isfinite(weight) || std::fabs(weight) > MaximumBlendShapeWeight) return false;
    return q == 0 || q == 1 || q == 2 || q == 4;
  }
  void write(std::ostream &out) const override {
    out << static_cast<u32>(quality) << ' ' << skinnedMotionVectors << ' ' << bones.size();
    for (const auto bone : bones) out << ' ' << bone;
    out << ' ' << blendShapeWeights.size();
    for (const float weight : blendShapeWeights) out << ' ' << weight;
  }
  bool read(std::istream &in, u32 version) override {
    u32 q = 0;
    usize count = 0;
    if ((version != 1 && version != 2) || !(in >> q >> skinnedMotionVectors >> count) || count > MaximumBones) return false;
    quality = static_cast<SkinQuality>(q);
    bones.assign(count, 0);
    for (auto &bone : bones) if (!(in >> bone)) return false;
    blendShapeWeights.clear();
    if (version >= 2) {
      usize shapes = 0;
      if (!(in >> shapes) || shapes > MaximumBlendShapes) return false;
      blendShapeWeights.resize(shapes);
      for (auto &weight : blendShapeWeights) if (!(in >> weight)) return false;
    }
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
  {"skinned_motion_vectors", "Vetor de movimento da deformação",
   [](const ComponentValue &v) { return static_cast<const SkinnedMesh &>(v).skinnedMotionVectors; },
   [](ComponentValue &v, bool value) { static_cast<SkinnedMesh &>(v).skinnedMotionVectors = value; },
   {"Skin", "", "A reprojeção temporal usa a pose anterior dos ossos e dos blend shapes, não só a do objeto",
    nullptr, nullptr, "render.motion_vectors", "platform/android/instanced_motion.inl → passe de movimento com pose anterior"}}
}};
// SetBlendShapeWeight/GetBlendShapeWeight da Unity: um endereço por blend shape.
inline constexpr std::array<ComponentSlotNumber, 1> skinnedMeshSlotNumbers{{
  {"blend_shape_weight", "Peso do blend shape", -SkinnedMesh::MaximumBlendShapeWeight, SkinnedMesh::MaximumBlendShapeWeight, 1,
   [](const ComponentValue &v) { return static_cast<u32>(static_cast<const SkinnedMesh &>(v).blendShapeWeights.size()); },
   [](const ComponentValue &v, u32 slot) {
     const auto &s = static_cast<const SkinnedMesh &>(v);
     return slot < s.blendShapeWeights.size() ? s.blendShapeWeights[slot] : 0.0f;
   },
   [](ComponentValue &v, u32 slot, float value) {
     auto &s = static_cast<SkinnedMesh &>(v);
     if (slot >= s.blendShapeWeights.size() || !std::isfinite(value)) return false;
     s.blendShapeWeights[slot] = value;
     return true;
   },
   {"Blend shapes", "%", "0 é a forma base, 100 é o alvo inteiro", nullptr, nullptr, {},
    "editor/editor_map_scene.cpp → pesos; platform/android/instanced_skinning.inl → compute"}}
}};
inline const ComponentType SkinnedMesh::descriptor{
  "astra.render.skinned_mesh", 2, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<SkinnedMesh>(); },
  {}, skinnedMeshBooleans, skinnedMeshEnums, nullptr, false, {}, {}, {}, skinnedMeshSlotNumbers
};
} // namespace ae::scene

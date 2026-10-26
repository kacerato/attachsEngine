#pragma once
// Comportamentos de efeito (G6a): os scripts que todo pacote de VFX da Unity
// traz, como componentes da engine. Referências estudadas (Unity 6000.0):
//  - Linha — LineRenderer (Manual/class-LineRenderer.html): pontos, largura do
//    início ao fim, gradiente de cor, Material e modo de textura Esticar/Repetir.
//  - Laser — o padrão "raio + LineRenderer + impacto" (Physics.Raycast em
//    ScriptReference/Physics.Raycast.html) dos pacotes de laser: a linha vai do
//    objeto ao primeiro colisor e o efeito de impacto acompanha o ponto.
//  - Projétil — "anda pela frente e, no primeiro toque, cria o impacto e some"
//    (OnCollisionEnter, ScriptReference/Collider.OnCollisionEnter.html).
//  - Instanciar ao colidir — OnParticleCollision + GetCollisionEvents
//    (ScriptReference/MonoBehaviour.OnParticleCollision.html): cada toque de
//    partícula cria um prefab no ponto.
//  - Som repetido — AudioSource.PlayOneShot em InvokeRepeating, com volume
//    sorteado (ScriptReference/AudioSource.PlayOneShot.html).
//  - Girar — Transform.Rotate (ScriptReference/Transform.Rotate.html) em graus
//    por segundo, no espaço local.
// A referência autoriza estudar o comportamento; o código é da Astra. Nada
// aqui roda fora do Play, exceto a Linha, que também é desenhada na edição.
// O runtime está em runtime/scene_vfx.cpp e runtime/scene_lines.cpp.
#include "scene/components.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ae::scene {

// Frente do objeto para Laser e Projétil. A Astra olha para +Z local; um efeito
// importado da Unity, refletido em Z na conversão, olha para −Z.
enum class VfxForwardAxis : u32 { PositiveZ = 0, NegativeZ = 1 };
inline constexpr std::array<ComponentEnumOption, 2> vfxForwardAxisOptions{{{0, "+Z (Astra)"}, {1, "−Z (Unity)"}}};

namespace detail {
// Persistência por tabela: números, booleanos e enums na ordem dos
// descritores, depois referências e recursos. Versão nova acrescenta no fim.
inline void writeVfxTables(const ComponentValue &v, std::ostream &out) {
  const auto &type = v.type();
  out << type.numbers.size();
  for (const auto &p : type.numbers) out << ' ' << p.read(v);
  out << ' ' << type.booleans.size();
  for (const auto &p : type.booleans) out << ' ' << (p.read(v) ? 1 : 0);
  out << ' ' << type.enums.size();
  for (const auto &p : type.enums) out << ' ' << p.read(v);
  out << ' ' << type.references.size();
  for (const auto &p : type.references) out << ' ' << p.read(v);
  u32 resources = 0;
  for (const auto &binding : type.resourceBindings) resources += binding.slotCount(v);
  out << ' ' << resources;
  for (const auto &binding : type.resourceBindings)
    for (u32 slot = 0; slot < binding.slotCount(v); ++slot) {
      const auto guid = binding.at(v, slot);
      out << ' ' << (guid.valid() ? guid.text() : std::string("-"));
    }
}
inline bool readVfxTables(ComponentValue &v, std::istream &in) {
  const auto &type = v.type();
  usize count = 0;
  if (!(in >> count) || count != type.numbers.size()) return false;
  for (const auto &p : type.numbers) if (!(in >> *p.write(v)) || !std::isfinite(*p.write(v))) return false;
  if (!(in >> count) || count != type.booleans.size()) return false;
  for (const auto &p : type.booleans) {
    u32 flag = 0;
    if (!(in >> flag) || flag > 1) return false;
    p.write(v, flag != 0);
  }
  if (!(in >> count) || count != type.enums.size()) return false;
  for (const auto &p : type.enums) {
    u32 value = 0;
    if (!(in >> value) || value > 1024) return false;
    p.write(v, value);
  }
  if (!(in >> count) || count != type.references.size()) return false;
  for (const auto &p : type.references) {
    u64 id = 0;
    if (!(in >> id) || id > std::numeric_limits<u32>::max()) return false;
    p.write(v, id);
  }
  u32 resources = 0, expected = 0;
  for (const auto &binding : type.resourceBindings) expected += binding.slotCount(v);
  if (!(in >> resources) || resources != expected) return false;
  for (const auto &binding : type.resourceBindings)
    for (u32 slot = 0; slot < binding.slotCount(v); ++slot) {
      std::string text;
      resources::AssetGuid guid{};
      if (!(in >> text) || (text != "-" && !resources::AssetGuid::parse(text, guid))) return false;
      if (!binding.write(v, slot, guid)) return false;
    }
  return true;
}
inline bool validVfxTables(const ComponentValue &v) {
  const auto &type = v.type();
  for (const auto &p : type.numbers) {
    const float value = p.read(v);
    if (!std::isfinite(value) || value < p.minimum || value > p.maximum) return false;
  }
  for (const auto &p : type.enums) {
    bool known = false;
    for (const auto &option : p.options) known = known || option.value == p.read(v);
    if (!known) return false;
  }
  for (const auto &p : type.references) if (p.read(v) > std::numeric_limits<u32>::max()) return false;
  return true;
}
} // namespace detail

// --- Linha -----------------------------------------------------------------
class LineRenderer final : public ComponentValue {
public:
  static constexpr u32 kMaximumPoints = 8;
#include "scene/generated/vfx_behaviors_LineRenderer_fields0.inc"
  // x, y, z de cada ponto (em mundo ou no espaço do objeto, por `worldSpace`).
  float points[kMaximumPoints * 3]{0, 0, 0, 0, 0, 1};
  float colorStart[4]{1, 1, 1, 1}, colorEnd[4]{1, 1, 1, 1};
  resources::AssetGuid material{};

  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<LineRenderer>(*this); }
  bool valid() const override { return detail::validVfxTables(*this) && pointCount == std::floor(pointCount); }
  void write(std::ostream &out) const override { detail::writeVfxTables(*this, out); }
  bool read(std::istream &in, u32 version) override {
    LineRenderer candidate;
    if (version != 1 || !detail::readVfxTables(candidate, in) || !candidate.valid()) return false;
    *this = candidate;
    return true;
  }
  u32 usedPoints() const noexcept { return static_cast<u32>(std::clamp(pointCount, 2.0f, static_cast<float>(kMaximumPoints))); }
};
inline const LineRenderer &lineRenderer(const ComponentValue &v) { return static_cast<const LineRenderer &>(v); }
template <u32 N> bool linePointVisible(const ComponentValue &v) { return lineRenderer(v).usedPoints() > N; }

// --- Laser -----------------------------------------------------------------
class Laser final : public ComponentValue {
public:
#include "scene/generated/vfx_behaviors_Laser_fields0.inc"
  VfxForwardAxis forwardAxis = VfxForwardAxis::PositiveZ;
  u64 hitEffect = 0;

  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<Laser>(*this); }
  bool valid() const override { return detail::validVfxTables(*this); }
  void write(std::ostream &out) const override { detail::writeVfxTables(*this, out); }
  bool read(std::istream &in, u32 version) override {
    Laser candidate;
    if (version != 1 || !detail::readVfxTables(candidate, in) || !candidate.valid()) return false;
    *this = candidate;
    return true;
  }
};

// --- Projétil --------------------------------------------------------------
class Projectile final : public ComponentValue {
public:
  static constexpr u32 kDetachSlots = 4;
#include "scene/generated/vfx_behaviors_Projectile_fields0.inc"
  float rotationOffset[3]{};
  VfxForwardAxis forwardAxis = VfxForwardAxis::PositiveZ;
  resources::AssetGuid impact{}, muzzle{};
  // Filhos que ficam na cena quando o projétil some (o rastro termina sozinho).
  std::array<u64, kDetachSlots> detach{};

  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<Projectile>(*this); }
  bool valid() const override { return detail::validVfxTables(*this); }
  void write(std::ostream &out) const override { detail::writeVfxTables(*this, out); }
  bool read(std::istream &in, u32 version) override {
    Projectile candidate;
    if (version != 1 || !detail::readVfxTables(candidate, in) || !candidate.valid()) return false;
    *this = candidate;
    return true;
  }
};

// --- Instanciar ao colidir -------------------------------------------------
class ParticleCollisionSpawn final : public ComponentValue {
public:
  static constexpr u32 kEffectSlots = 4;
#include "scene/generated/vfx_behaviors_ParticleCollisionSpawn_fields0.inc"
  float rotationOffset[3]{};
  VfxForwardAxis forwardAxis = VfxForwardAxis::PositiveZ;
  std::array<resources::AssetGuid, kEffectSlots> effects{};

  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<ParticleCollisionSpawn>(*this); }
  bool valid() const override { return detail::validVfxTables(*this) && maxPerFrame == std::floor(maxPerFrame); }
  void write(std::ostream &out) const override { detail::writeVfxTables(*this, out); }
  bool read(std::istream &in, u32 version) override {
    ParticleCollisionSpawn candidate;
    if (version != 1 || !detail::readVfxTables(candidate, in) || !candidate.valid()) return false;
    *this = candidate;
    return true;
  }
};

// --- Som repetido ----------------------------------------------------------
class EffectSound final : public ComponentValue {
public:
#include "scene/generated/vfx_behaviors_EffectSound_fields0.inc"
  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<EffectSound>(*this); }
  bool valid() const override { return detail::validVfxTables(*this); }
  void write(std::ostream &out) const override { detail::writeVfxTables(*this, out); }
  bool read(std::istream &in, u32 version) override {
    EffectSound candidate;
    if (version != 1 || !detail::readVfxTables(candidate, in) || !candidate.valid()) return false;
    *this = candidate;
    return true;
  }
};
// --- Girar -----------------------------------------------------------------
// Velocidade constante em graus por segundo, somada ao giro local do objeto a
// cada quadro do Play (o Rotator dos pacotes de VFX: Transform.Rotate em
// InvokeRepeating). Os filhos giram junto, como na Unity.
class Rotator final : public ComponentValue {
public:
#include "scene/generated/vfx_behaviors_Rotator_fields0.inc"
  float speed[3]{0, 0, 0};
  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<Rotator>(*this); }
  bool valid() const override { return detail::validVfxTables(*this); }
  void write(std::ostream &out) const override { detail::writeVfxTables(*this, out); }
  bool read(std::istream &in, u32 version) override {
    Rotator candidate;
    if (version != 1 || !detail::readVfxTables(candidate, in) || !candidate.valid()) return false;
    *this = candidate;
    return true;
  }
};

inline bool effectSoundRepeats(const ComponentValue &v) { return static_cast<const EffectSound &>(v).repeating; }
inline bool effectSoundRandom(const ComponentValue &v) { return static_cast<const EffectSound &>(v).randomVolume; }

#include "scene/generated/vfx_behaviors_lineNumbers.inc"
#include "scene/generated/vfx_behaviors_lineBooleans.inc"
#include "scene/generated/vfx_behaviors_laserNumbers.inc"
#include "scene/generated/vfx_behaviors_laserBooleans.inc"
#include "scene/generated/vfx_behaviors_laserEnums.inc"
#include "scene/generated/vfx_behaviors_projectileNumbers.inc"
#include "scene/generated/vfx_behaviors_projectileBooleans.inc"
#include "scene/generated/vfx_behaviors_projectileEnums.inc"
#include "scene/generated/vfx_behaviors_collisionSpawnNumbers.inc"
#include "scene/generated/vfx_behaviors_collisionSpawnBooleans.inc"
#include "scene/generated/vfx_behaviors_collisionSpawnEnums.inc"
#include "scene/generated/vfx_behaviors_effectSoundNumbers.inc"
#include "scene/generated/vfx_behaviors_effectSoundBooleans.inc"
#include "scene/generated/vfx_behaviors_rotatorNumbers.inc"
#include "scene/generated/vfx_behaviors_rotatorBooleans.inc"

inline constexpr std::array<ComponentTriple, 10> lineTriples{{
  {"point_0", "Ponto 1", {"point_0_x", "point_0_y", "point_0_z"}},
  {"point_1", "Ponto 2", {"point_1_x", "point_1_y", "point_1_z"}},
  {"point_2", "Ponto 3", {"point_2_x", "point_2_y", "point_2_z"}},
  {"point_3", "Ponto 4", {"point_3_x", "point_3_y", "point_3_z"}},
  {"point_4", "Ponto 5", {"point_4_x", "point_4_y", "point_4_z"}},
  {"point_5", "Ponto 6", {"point_5_x", "point_5_y", "point_5_z"}},
  {"point_6", "Ponto 7", {"point_6_x", "point_6_y", "point_6_z"}},
  {"point_7", "Ponto 8", {"point_7_x", "point_7_y", "point_7_z"}},
  {"color_start", "Cor inicial", {"color_start_r", "color_start_g", "color_start_b"}, ComponentTripleKind::LinearColor},
  {"color_end", "Cor final", {"color_end_r", "color_end_g", "color_end_b"}, ComponentTripleKind::LinearColor},
}};
inline constexpr std::array<ComponentResourceBinding, 1> lineResources{{
  {"material", "Material", resources::AssetType::Material, [](const ComponentValue &) -> u32 { return 1; },
   [](const ComponentValue &v, u32) { return lineRenderer(v).material; },
   [](ComponentValue &v, u32 slot, resources::AssetGuid value) {
     if (slot) return false;
     static_cast<LineRenderer &>(v).material = value;
     return true;
   },
   {"Aparência", "", "Material com shader Partícula: a imagem corre ao longo da linha", nullptr, nullptr, "render.lines",
    "runtime/scene_lines.cpp → platform/android/instanced_particles.inl"}}}};
inline const ComponentType LineRenderer::descriptor{
  "astra.render.line", 1, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<LineRenderer>(); },
  lineNumbers, lineBooleans, {}, nullptr, false, {}, lineTriples, lineResources};

inline constexpr std::array<ComponentObjectReference, 1> laserReferences{{
  {"hit_effect", "Efeito de impacto", "", ObjectReferenceScope::Other, "Nenhum",
   [](const ComponentValue &v) { return static_cast<const Laser &>(v).hitEffect; },
   [](ComponentValue &v, u64 id) { static_cast<Laser &>(v).hitEffect = id; },
   {"Impacto", "", "Objeto levado ao ponto atingido; as partículas dele tocam enquanto o raio acerta algo", nullptr, nullptr,
    "render.effects.laser", "runtime/scene_vfx.cpp"}},
}};
inline const ComponentType Laser::descriptor{
  "astra.effects.laser", 1, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<Laser>(); },
  laserNumbers, laserBooleans, laserEnums, nullptr, false, laserReferences};

inline constexpr std::array<ComponentTriple, 1> projectileTriples{{
  {"rotation_offset", "Giro do impacto", {"rotation_offset_x", "rotation_offset_y", "rotation_offset_z"}}}};
template <u32 N> constexpr ComponentObjectReference projectileDetach(const char *name) {
  return {N == 0 ? "detach_0" : N == 1 ? "detach_1" : N == 2 ? "detach_2" : "detach_3", name, "", ObjectReferenceScope::Descendant, "Nenhum",
          [](const ComponentValue &v) { return static_cast<const Projectile &>(v).detach[N]; },
          [](ComponentValue &v, u64 id) { static_cast<Projectile &>(v).detach[N] = id; },
          {"Ao acertar", "", "Filho que fica na cena quando o projétil some (rastro, fumaça)", nullptr, nullptr,
           "render.effects.projectile", "runtime/scene_vfx.cpp"}};
}
inline constexpr std::array<ComponentObjectReference, 4> projectileReferences{{
  projectileDetach<0>("Soltar 1"), projectileDetach<1>("Soltar 2"), projectileDetach<2>("Soltar 3"), projectileDetach<3>("Soltar 4")}};
inline constexpr std::array<ComponentResourceBinding, 2> projectileResources{{
  {"impact", "Impacto", resources::AssetType::Prefab, [](const ComponentValue &) -> u32 { return 1; },
   [](const ComponentValue &v, u32) { return static_cast<const Projectile &>(v).impact; },
   [](ComponentValue &v, u32 slot, resources::AssetGuid value) {
     if (slot) return false;
     static_cast<Projectile &>(v).impact = value;
     return true;
   },
   {"Ao acertar", "", "Prefab criado no ponto do acerto e destruído quando as partículas dele terminam", nullptr, nullptr,
    "render.effects.projectile", "runtime/scene_vfx.cpp"}},
  {"muzzle", "Disparo", resources::AssetType::Prefab, [](const ComponentValue &) -> u32 { return 1; },
   [](const ComponentValue &v, u32) { return static_cast<const Projectile &>(v).muzzle; },
   [](ComponentValue &v, u32 slot, resources::AssetGuid value) {
     if (slot) return false;
     static_cast<Projectile &>(v).muzzle = value;
     return true;
   },
   {"Projétil", "", "Prefab criado na saída (clarão do disparo) e destruído quando termina", nullptr, nullptr,
    "render.effects.projectile", "runtime/scene_vfx.cpp"}}}};
inline const ComponentType Projectile::descriptor{
  "astra.effects.projectile", 1, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<Projectile>(); },
  projectileNumbers, projectileBooleans, projectileEnums, nullptr, false, projectileReferences, projectileTriples, projectileResources};

inline constexpr std::array<ComponentTriple, 1> collisionSpawnTriples{{
  {"rotation_offset", "Giro fixo", {"rotation_offset_x", "rotation_offset_y", "rotation_offset_z"}}}};
inline constexpr std::array<ComponentResourceBinding, 1> collisionSpawnResources{{
  {"effects", "Efeitos", resources::AssetType::Prefab, [](const ComponentValue &) -> u32 { return ParticleCollisionSpawn::kEffectSlots; },
   [](const ComponentValue &v, u32 slot) { return static_cast<const ParticleCollisionSpawn &>(v).effects[slot]; },
   [](ComponentValue &v, u32 slot, resources::AssetGuid value) {
     if (slot >= ParticleCollisionSpawn::kEffectSlots) return false;
     static_cast<ParticleCollisionSpawn &>(v).effects[slot] = value;
     return true;
   },
   {"Ao colidir", "", "Prefabs criados em cada toque de partícula", nullptr, nullptr, "render.effects.collision_spawn",
    "runtime/scene_vfx.cpp"}}}};
inline const ComponentType ParticleCollisionSpawn::descriptor{
  "astra.effects.collision_spawn", 1, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<ParticleCollisionSpawn>(); },
  collisionSpawnNumbers, collisionSpawnBooleans, collisionSpawnEnums, nullptr, false, {}, collisionSpawnTriples, collisionSpawnResources};

inline const ComponentType EffectSound::descriptor{
  "astra.audio.effect_sound", 1, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<EffectSound>(); },
  effectSoundNumbers, effectSoundBooleans};

inline constexpr std::array<ComponentTriple, 1> rotatorTriples{{
  {"speed", "Velocidade", {"speed_x", "speed_y", "speed_z"}}}};
inline const ComponentType Rotator::descriptor{
  "astra.effects.rotator", 1, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<Rotator>(); },
  rotatorNumbers, rotatorBooleans, {}, nullptr, false, {}, rotatorTriples};

} // namespace ae::scene

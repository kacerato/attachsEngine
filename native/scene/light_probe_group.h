#pragma once
// PK-6: Light Probe Group (astra.render.light_probe_group v1), como na Unity 6.
//
// O autor cria Luzes › Grupo de sondas de luz (nasce com 8 sondas num cubo,
// como na Unity), liga "Editar sondas", seleciona, move, duplica e apaga
// pontos no viewport e no Inspector, e toca Assar sondas. O bake captura a
// cena em cada ponto (sol, céu e o rebate das superfícies iluminadas) e grava
// um SH L2 por ponto num recurso do projeto (.aelightprobes). Os Mesh
// Renderers com Light Probes = Misturar sondas interpolam pelo tetraedro que
// contém o centro dos limites.
//
// As posições são LOCAIS ao objeto do grupo; o recurso assado guarda a posição
// de mundo no momento do bake, como o LightingData da Unity.
//
// Referência: https://docs.unity3d.com/6000.0/Documentation/Manual/class-LightProbeGroup.html
#include "scene/components.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <vector>

namespace ae::scene {

struct LightProbePoint {
  u64 id = 0;
  float position[3]{};
  friend bool operator==(const LightProbePoint &a, const LightProbePoint &b) {
    return a.id == b.id && a.position[0] == b.position[0] && a.position[1] == b.position[1] && a.position[2] == b.position[2];
  }
};

class LightProbeGroup final : public ComponentValue {
public:
  static constexpr usize MaximumPoints = 512;
  static constexpr float MaximumExtent = 100000;
  bool enabled = true;
  // Remove Ringing: suaviza o SH para tirar lóbulos negativos atrás de luz forte.
  bool removeRinging = false;
  // Mostrar as arestas da tetraedralização no viewport (Show Wireframe).
  bool showWireframe = true;
  std::vector<LightProbePoint> points;
  u64 nextPointId = 1;
  resources::AssetGuid bakedData{};

  LightProbeGroup() { resetToCube(); }
  void resetToCube() {
    points.clear();
    nextPointId = 1;
    for (u32 i = 0; i < 8; ++i)
      points.push_back({nextPointId++, {(i & 1) ? .5f : -.5f, (i & 2) ? .5f : -.5f, (i & 4) ? .5f : -.5f}});
  }
  const LightProbePoint *point(u64 id) const {
    const auto at = std::find_if(points.begin(), points.end(), [&](const auto &p) { return p.id == id; });
    return at == points.end() ? nullptr : &*at;
  }
  LightProbePoint *point(u64 id) {
    const auto at = std::find_if(points.begin(), points.end(), [&](const auto &p) { return p.id == id; });
    return at == points.end() ? nullptr : &*at;
  }
  // Acrescenta um ponto e devolve a identidade nova (0 se cheio ou inválido).
  u64 addPoint(const float position[3]) {
    if (points.size() >= MaximumPoints || nextPointId == std::numeric_limits<u64>::max()) return 0;
    for (u32 k = 0; k < 3; ++k) if (!std::isfinite(position[k]) || std::abs(position[k]) > MaximumExtent) return 0;
    points.push_back({nextPointId, {position[0], position[1], position[2]}});
    return nextPointId++;
  }
  bool removePoint(u64 id) {
    const auto at = std::find_if(points.begin(), points.end(), [&](const auto &p) { return p.id == id; });
    if (at == points.end()) return false;
    points.erase(at);
    return true;
  }
  bool movePoint(u64 id, const float position[3]) {
    auto *p = point(id);
    if (!p) return false;
    for (u32 k = 0; k < 3; ++k) if (!std::isfinite(position[k]) || std::abs(position[k]) > MaximumExtent) return false;
    std::copy(position, position + 3, p->position);
    return true;
  }

  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<LightProbeGroup>(*this); }
  bool valid() const override {
    if (points.size() > MaximumPoints || !nextPointId) return false;
    for (usize i = 0; i < points.size(); ++i) {
      const auto &p = points[i];
      if (!p.id || p.id >= nextPointId) return false;
      for (float v : p.position) if (!std::isfinite(v) || std::abs(v) > MaximumExtent) return false;
      for (usize j = 0; j < i; ++j) if (points[j].id == p.id) return false;
    }
    return true;
  }
  void write(std::ostream &out) const override {
    const auto precision = out.precision();
    out << std::setprecision(std::numeric_limits<float>::max_digits10) << enabled << ' ' << removeRinging << ' ' << showWireframe
        << ' ' << bakedData.high << ' ' << bakedData.low << ' ' << nextPointId << ' ' << points.size();
    for (const auto &p : points) out << ' ' << p.id << ' ' << p.position[0] << ' ' << p.position[1] << ' ' << p.position[2];
    out.precision(precision);
  }
  bool read(std::istream &in, u32 version) override {
    if (version != 1) return false;
    LightProbeGroup value;
    usize count = 0;
    if (!(in >> value.enabled >> value.removeRinging >> value.showWireframe >> value.bakedData.high >> value.bakedData.low >>
          value.nextPointId >> count) || count > MaximumPoints) return false;
    value.points.resize(count);
    for (auto &p : value.points) if (!(in >> p.id >> p.position[0] >> p.position[1] >> p.position[2])) return false;
    if (!value.valid()) return false;
    // A base (identidade da instância) é do objeto lido, não do temporário.
    static_cast<ComponentValue &>(value) = static_cast<const ComponentValue &>(*this);
    *this = std::move(value);
    return true;
  }
};

namespace light_probe_detail {
inline const LightProbeGroup &group(const ComponentValue &v) { return static_cast<const LightProbeGroup &>(v); }
inline LightProbeGroup &group(ComponentValue &v) { return static_cast<LightProbeGroup &>(v); }
inline constexpr std::string_view capability = "render.probe.irradiance";
inline constexpr std::string_view consumer = "renderer/light_probes.cpp → SH L2 por desenho (tetraedro do centro)";
} // namespace light_probe_detail

inline constexpr std::array<ComponentBoolean, 3> lightProbeGroupBooleans{{
  {"enabled", "Ativo", [](const ComponentValue &v) { return light_probe_detail::group(v).enabled; },
   [](ComponentValue &v, bool next) { light_probe_detail::group(v).enabled = next; },
   {"Sondas", "", nullptr, nullptr, nullptr, light_probe_detail::capability, light_probe_detail::consumer, Invalidate::Probes}},
  {"remove_ringing", "Remover anéis", [](const ComponentValue &v) { return light_probe_detail::group(v).removeRinging; },
   [](ComponentValue &v, bool next) { light_probe_detail::group(v).removeRinging = next; },
   {"Sondas", "", "Tira os halos escuros atrás de uma luz forte; a luz das sondas fica mais suave", nullptr, nullptr,
    light_probe_detail::capability, light_probe_detail::consumer, Invalidate::Probes}},
  {"show_wireframe", "Mostrar arestas", [](const ComponentValue &v) { return light_probe_detail::group(v).showWireframe; },
   [](ComponentValue &v, bool next) { light_probe_detail::group(v).showWireframe = next; },
   {"Sondas", "", "Desenha no viewport os tetraedros que ligam as sondas", nullptr, nullptr, "", "editor/editor_component_visuals.h",
    0}}
}};
// Posição de cada sonda, por slot (o ponto na ordem do grupo).
inline const std::array<ComponentSlotNumber, 3> lightProbeGroupSlotNumbers = [] {
  std::array<ComponentSlotNumber, 3> a{};
  constexpr const char *ids[]{"probe_position_x", "probe_position_y", "probe_position_z"};
  constexpr const char *names[]{"Posição X", "Posição Y", "Posição Z"};
#define AE_PROBE_SLOT(N)                                                                                                    \
  a[N] = {ids[N], names[N], -LightProbeGroup::MaximumExtent, LightProbeGroup::MaximumExtent, .05f,                         \
          [](const ComponentValue &v) { return static_cast<u32>(light_probe_detail::group(v).points.size()); },             \
          [](const ComponentValue &v, u32 s) { return light_probe_detail::group(v).points[s].position[N]; },                \
          [](ComponentValue &v, u32 s, float n) {                                                                           \
            auto &g = light_probe_detail::group(v);                                                                         \
            if (s >= g.points.size() || !std::isfinite(n) || std::abs(n) > LightProbeGroup::MaximumExtent) return false;    \
            g.points[s].position[N] = n;                                                                                    \
            return true;                                                                                                    \
          },                                                                                                                \
          {"Sonda selecionada", "m", nullptr, nullptr, nullptr, light_probe_detail::capability, light_probe_detail::consumer, \
           Invalidate::Probes}};
  AE_PROBE_SLOT(0) AE_PROBE_SLOT(1) AE_PROBE_SLOT(2)
#undef AE_PROBE_SLOT
  return a;
}();
inline const std::array<ComponentCollection, 1> lightProbeGroupCollections{{
  {"probes", [](const ComponentValue &v) { return static_cast<u32>(light_probe_detail::group(v).points.size()); },
   [](const ComponentValue &v, u32 slot) { return light_probe_detail::group(v).points[slot].id; },
   [](const ComponentValue &v) { return light_probe_detail::group(v).nextPointId; },
   [](ComponentValue &v, u64 floor) {
     if (!floor) return false;
     auto &g = light_probe_detail::group(v);
     g.nextPointId = std::max(g.nextPointId, floor);
     return g.valid();
   }}
}};
inline constexpr std::array<ComponentResourceBinding, 1> lightProbeGroupResources{{
  {"baked_data", "Sondas assadas", resources::AssetType::LightProbes, [](const ComponentValue &) { return 1u; },
   [](const ComponentValue &v, u32) { return light_probe_detail::group(v).bakedData; },
   [](ComponentValue &v, u32, resources::AssetGuid value) { light_probe_detail::group(v).bakedData = value; return true; },
   {"Sondas", "", "Escrito pelo Assar sondas", nullptr, nullptr, light_probe_detail::capability, light_probe_detail::consumer,
    Invalidate::Probes}}
}};
// G7: LightProbeGroup.probePositions.Length pelo script (as posições já são
// lidas e escritas por slot).
inline constexpr std::array<ComponentMethod, 1> lightProbeGroupMethods{{
  {"probe_count", "Quantidade de sondas", "Quantos pontos o grupo tem (probePositions.Length da Unity)", {}, ComponentValueKind::Integer},
}};
inline const ComponentType LightProbeGroup::descriptor{
  "astra.render.light_probe_group", 1, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<LightProbeGroup>(); },
  {}, lightProbeGroupBooleans, {}, nullptr, false, {}, {}, lightProbeGroupResources, lightProbeGroupSlotNumbers, {},
  lightProbeGroupCollections, lightProbeGroupMethods};

} // namespace ae::scene

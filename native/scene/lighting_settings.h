#pragma once
#include "scene/components.h"
#include <array>

namespace ae::scene {
// Configurações de iluminação da CENA — o equivalente da janela Lighting da
// Unity 6 (Window › Rendering › Lighting › Environment), não de um Volume.
//
// Hoje guarda a Fonte do sol (Sun Source): a Luz direcional que é a luz
// principal da cena, a que projeta pelas cascatas e que o céu procedural
// desenha como disco. Vazio mantém a regra da Unity para "None": a direcional
// mais brilhante — com o sol de Hora do dia na frente, para que a noite não
// devolva o dia a outra direcional.
//
// Uma referência que deixou de servir (objeto apagado, luz desligada ou não
// direcional) também cai na regra automática, como no URP, que só usa
// RenderSettings.sun quando ele é uma direcional ativa.
//
// Referência: https://docs.unity3d.com/6000.0/Documentation/Manual/lighting-window.html
class LightingSettings final : public ComponentValue {
public:
  u64 sunSource = 0;

  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<LightingSettings>(*this); }
  bool valid() const override { return true; }
  void write(std::ostream &out) const override { out << sunSource; }
  bool read(std::istream &in, u32 version) override {
    return version == 1 && static_cast<bool>(in >> sunSource);
  }
};
inline constexpr std::array<ComponentObjectReference, 1> lightingSettingsReferences{{
  {"sun_source", "Fonte do sol", "astra.render.light", ObjectReferenceScope::Any, "Automática (a direcional mais brilhante)",
   [](const ComponentValue &v) { return static_cast<const LightingSettings &>(v).sunSource; },
   [](ComponentValue &v, u64 id) { static_cast<LightingSettings &>(v).sunSource = id; },
   {"Ambiente", "", "A luz direcional que é o sol da cena: projeta as cascatas e aparece no céu", nullptr, nullptr,
    "render.light.sun_source", "runtime/scene_lights.cpp → renderer/punctual_lights.h", Invalidate::LightCluster},
   nullptr, nullptr, true}
}};
inline const ComponentType LightingSettings::descriptor{
  "astra.render.lighting_settings", 1,
  []() -> std::unique_ptr<ComponentValue> { return std::make_unique<LightingSettings>(); },
  {}, {}, {}, nullptr, false, lightingSettingsReferences
};
} // namespace ae::scene

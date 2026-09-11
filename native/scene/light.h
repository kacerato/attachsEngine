#pragma once
#include "scene/components.h"
#include <array>
namespace ae::scene {
// Luz anexável a qualquer objeto: a pose vem da Transformação, como em qualquer
// outro componente. `+Z` local é a direção de emissão de direcional e spot — a
// mesma convenção de frente que a Câmera desta engine usa, para que girar um
// objeto de luz e um de câmera do mesmo jeito aponte os dois para o mesmo lado.
//
// Unidades, escritas aqui porque o shader depende delas:
//  - `color` é RGB linear, não sRGB. O inspetor mostra o mesmo número que o
//    shader multiplica.
//  - Direcional: `intensity` é irradiância, na mesma escala do sol do ambiente.
//  - Pontual e spot: a contribuição é `color * intensity / d²`, com janela suave
//    fechando em `range`. `intensity` é, portanto, o valor a um metro.
//  - `innerAngle`/`outerAngle` são meio-ângulos em graus. Entre eles a
//    atenuação é suave; fora de `outerAngle` é zero.
//
// Sombra NÃO é propriedade desta lista de propósito. O passe de sombra
// implementado é o de cascatas do sol, que só existe para a modalidade
// direcional; oferecer um interruptor de sombra em pontual ou spot seria um
// botão sem shader atrás. A matriz real de capacidades está em
// `renderer/punctual_lights.h`.
enum class LightKind : u32 { Directional = 0, Point = 1, Spot = 2 };
class Light final : public ComponentValue {
public:
  LightKind kind = LightKind::Point;
  bool enabled = true;
  float color[3]{1, 1, 1};
  float intensity = 8;
  float range = 10;
  float innerAngle = 20, outerAngle = 35;
  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<Light>(*this); }
  bool valid() const override {
    if (static_cast<u32>(kind) > 2) return false;
    for (const auto &p : descriptor.numbers) {
      const float v = p.read(*this);
      if (!std::isfinite(v) || v < p.minimum || v > p.maximum) return false;
    }
    // Um cone interno maior que o externo produziria uma janela invertida e,
    // no shader, uma borda dura no lugar errado. Recusa na autoria.
    return outerAngle >= innerAngle;
  }
  void write(std::ostream &out) const override {
    out << static_cast<u32>(kind) << ' ' << enabled;
    for (const auto &p : descriptor.numbers) out << ' ' << p.read(*this);
  }
  bool read(std::istream &in, u32 version) override {
    u32 mode = 0;
    if (version != 1 || !(in >> mode) || mode > 2 || !(in >> enabled)) return false;
    kind = static_cast<LightKind>(mode);
    for (const auto &p : descriptor.numbers) if (!(in >> *p.write(*this))) return false;
    return true;
  }
};
inline constexpr std::array<ComponentNumber, 7> lightNumbers{{
#define AE_LIGHT_NUMBER(id, label, field, lo, hi, step) {label, lo, hi, step, [](const ComponentValue &v) -> const float & {return static_cast<const Light &>(v).field;}, [](ComponentValue &v) -> float * {return &static_cast<Light &>(v).field;}, id}
  AE_LIGHT_NUMBER("color.r", "Cor R", color[0], 0, 1, .01f),
  AE_LIGHT_NUMBER("color.g", "Cor G", color[1], 0, 1, .01f),
  AE_LIGHT_NUMBER("color.b", "Cor B", color[2], 0, 1, .01f),
  AE_LIGHT_NUMBER("intensity", "Intensidade", intensity, 0, 10000, .1f),
  AE_LIGHT_NUMBER("range", "Alcance · m", range, .01f, 1000, .1f),
  AE_LIGHT_NUMBER("inner_angle", "Cone interno · graus", innerAngle, 0, 89, 1),
  AE_LIGHT_NUMBER("outer_angle", "Cone externo · graus", outerAngle, 0, 89, 1)
#undef AE_LIGHT_NUMBER
}};
inline constexpr std::array<ComponentEnumOption, 3> lightKindOptions{{
  {0, "Direcional"}, {1, "Pontual"}, {2, "Spot"}
}};
inline constexpr std::array<ComponentEnum, 1> lightEnums{{
  {"kind", "Modalidade", lightKindOptions,
    [](const ComponentValue &v) { return static_cast<u32>(static_cast<const Light &>(v).kind); },
    [](ComponentValue &v, u32 value) { static_cast<Light &>(v).kind = static_cast<LightKind>(value); }}
}};
inline constexpr std::array<ComponentBoolean, 1> lightBooleans{{
  {"enabled", "Acesa", [](const ComponentValue &v) { return static_cast<const Light &>(v).enabled; },
   [](ComponentValue &v, bool b) { static_cast<Light &>(v).enabled = b; }}
}};
inline const ComponentType Light::descriptor{
  "astra.render.light", 1, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<Light>(); },
  lightNumbers, lightBooleans, lightEnums
};
}

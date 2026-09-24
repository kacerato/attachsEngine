#pragma once
#include "resources/skeletal_animation.h"
#include "scene/components.h"

#include <array>
#include <cmath>

namespace ae::scene {
// Reprodutor de clipe por nós, no modelo do componente Animation (legado) da
// Unity: um clipe, Play Automatically, Wrap Mode e, por AnimationState, a
// velocidade e o tempo.
//
// Os clipes são dado imutável da fonte importada de que este objeto veio (o
// ImportLink do próprio objeto diz qual). Os canais endereçam nós da fonte por
// IDENTIDADE (o mapa de nós), resolvidos entre os objetos da mesma instância
// abaixo deste — não por caminho de nomes, como a Unity faz: renomear um osso
// na cena não quebra o clipe.
//
// `playing` e `time` são estado de EXECUÇÃO: nunca gravados, invisíveis no
// Inspector e escritos só pelo runtime e por scripts no Play. O Play parte
// sempre do tempo zero e, com Play Automatically, já tocando.
class Animation final : public ComponentValue {
public:
  static constexpr u32 MaximumClip = 1023;
  float clip = 0; // índice do clipe na fonte; inteiro
  bool playAutomatically = true;
  // glTF não declara modo de repetição; Loop deixa o clipe importado visível.
  resources::AnimationWrapMode wrapMode = resources::AnimationWrapMode::Loop;
  float speed = 1;
  bool playing = false;
  float time = 0;

  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<Animation>(*this); }
  u32 clipIndex() const noexcept { return static_cast<u32>(clip); }
  bool valid() const override {
    return std::isfinite(clip) && clip >= 0 && clip <= static_cast<float>(MaximumClip) && clip == std::floor(clip) &&
           static_cast<u32>(wrapMode) <= 3 && std::isfinite(speed) && speed >= -10 && speed <= 10 &&
           std::isfinite(time) && std::fabs(time) <= 1000000;
  }
  void write(std::ostream &out) const override {
    out << clipIndex() << ' ' << playAutomatically << ' ' << static_cast<u32>(wrapMode) << ' ' << speed;
  }
  bool read(std::istream &in, u32 version) override {
    u32 index = 0, mode = 0;
    if (version != 1 || !(in >> index >> playAutomatically >> mode >> speed) || index > MaximumClip || mode > 3)
      return false;
    clip = static_cast<float>(index);
    wrapMode = static_cast<resources::AnimationWrapMode>(mode);
    playing = false;
    time = 0;
    return valid();
  }
};

inline bool animationNever(const ComponentValue &) { return false; }
inline constexpr std::array<ComponentNumber, 3> animationNumbers{{
  {"Clipe", 0, static_cast<float>(Animation::MaximumClip), 1,
   [](const ComponentValue &v) -> const float & { return static_cast<const Animation &>(v).clip; },
   [](ComponentValue &v) -> float * { return &static_cast<Animation &>(v).clip; }, "clip",
   {"Clipe", "", "Índice do clipe na fonte importada deste objeto"}},
  {"Velocidade", -10, 10, .05f,
   [](const ComponentValue &v) -> const float & { return static_cast<const Animation &>(v).speed; },
   [](ComponentValue &v) -> float * { return &static_cast<Animation &>(v).speed; }, "speed",
   {"Reprodução", "x", "AnimationState.speed: negativo toca de trás para frente"}},
  // Só por script (AnimationState.time); fora do arquivo e do Inspector.
  {"Tempo", -1000000, 1000000, .01f,
   [](const ComponentValue &v) -> const float & { return static_cast<const Animation &>(v).time; },
   [](ComponentValue &v) -> float * { return &static_cast<Animation &>(v).time; }, "time",
   {"Execução", "s", "Estado de execução: tempo corrente do clipe", animationNever}}
}};
inline constexpr std::array<ComponentBoolean, 2> animationBooleans{{
  {"play_automatically", "Tocar ao iniciar",
   [](const ComponentValue &v) { return static_cast<const Animation &>(v).playAutomatically; },
   [](ComponentValue &v, bool value) { static_cast<Animation &>(v).playAutomatically = value; },
   {"Reprodução", "", "Play Automatically: começa a tocar quando o Play inicia"}},
  {"playing", "Tocando",
   [](const ComponentValue &v) { return static_cast<const Animation &>(v).playing; },
   [](ComponentValue &v, bool value) { static_cast<Animation &>(v).playing = value; },
   {"Execução", "", "Estado de execução: Play/Stop por script", animationNever}}
}};
inline constexpr std::array<ComponentEnumOption, 4> animationWrapOptions{{
  {0, "Uma vez"}, {1, "Repetir"}, {2, "Vai e volta"}, {3, "Segurar no fim"}}};
inline constexpr std::array<ComponentEnum, 1> animationEnums{{
  {"wrap_mode", "Repetição", animationWrapOptions,
   [](const ComponentValue &v) { return static_cast<u32>(static_cast<const Animation &>(v).wrapMode); },
   [](ComponentValue &v, u32 value) {
     static_cast<Animation &>(v).wrapMode = static_cast<resources::AnimationWrapMode>(value);
   },
   {"Reprodução", "", "Wrap Mode: o que acontece depois do fim do clipe"}}
}};
inline const ComponentType Animation::descriptor{
  "astra.animation", 1, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<Animation>(); },
  animationNumbers, animationBooleans, animationEnums
};
} // namespace ae::scene

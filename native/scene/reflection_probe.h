#pragma once
// PK-6: Reflection Probe (astra.render.reflection_probe v1), o componente da
// Unity 6 com o Inspector da URP, na mesma ordem:
//
//   Type            Baked | Realtime (Custom fica planejado: pede um cubemap do projeto)
//   Runtime settings: Importance, Intensity, Box Projection, Blend Distance,
//                   Box Size, Box Offset
//   Realtime:       Refresh Mode (On Awake, Every Frame, Via Scripting) e
//                   Time Slicing (All faces at once, Individual faces, No time slicing)
//   Cubemap capture settings: Resolution, HDR, Clear Flags (Skybox | Solid
//                   Color), Background, Culling Mask, Clipping Planes (Near/Far)
//
// O que o autor faz, como na Unity: cria Luzes › Sonda de reflexo, arrasta as
// alças da caixa até as paredes do cômodo, e na sonda Baked toca Assar (o
// cubemap vai para o projeto, em Iluminação/). Uma Realtime recaptura sozinha.
// Os Mesh Renderers misturam as duas sondas de maior importância.
//
// Shadow Distance e Occlusion Culling da captura não estão aqui: a captura usa
// as cascatas da câmera principal e não tem oclusão própria (ver capability
// render.probe.reflection). Nenhuma propriedade sem efeito.
//
// Referência: https://docs.unity3d.com/6000.0/Documentation/Manual/class-ReflectionProbe.html
#include "scene/components.h"
#include "scene/rendering_layers.h"

#include <array>
#include <cmath>

namespace ae::scene {

class ReflectionProbe final : public ComponentValue {
public:
  bool enabled = true;
  u32 probeType = 0;   // 0 Assada (Baked), 1 Tempo real (Realtime)
  u32 refreshMode = 0; // 0 Ao acordar, 1 Todo quadro, 2 Por script
  u32 timeSlicing = 0; // 0 Todas as faces de uma vez, 1 Uma face por quadro, 2 Sem fatiamento
  float importance = 1;
  float intensity = 1;
  bool boxProjection = false;
  float blendDistance = 1;
  float boxSize[3]{10, 10, 10};
  float boxOffset[3]{0, 0, 0};
  u32 resolution = 128;
  bool hdr = true;
  u32 clearFlags = 0; // 0 Céu (Skybox), 1 Cor sólida
  float background[3]{0.192f, 0.302f, 0.475f}; // sRGB, o padrão da Unity
  u32 cullingMask = kAllObjectLayers;
  float nearClip = 0.3f, farClip = 1000;
  // Recurso .aeprobe gravado pelo Assar; vazio numa sonda nunca assada.
  resources::AssetGuid bakedTexture{};

  bool realtime() const noexcept { return probeType == 1; }
  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<ReflectionProbe>(*this); }
  bool valid() const override;
  void write(std::ostream &out) const override;
  bool read(std::istream &in, u32 version) override;
};

inline constexpr std::array<ComponentEnumOption, 2> reflectionProbeTypeOptions{{{0, "Assada"}, {1, "Tempo real"}}};
inline constexpr std::array<ComponentEnumOption, 3> reflectionProbeRefreshOptions{{
  {0, "Ao acordar"}, {1, "Todo quadro"}, {2, "Por script"}}};
inline constexpr std::array<ComponentEnumOption, 3> reflectionProbeSlicingOptions{{
  {0, "Todas as faces de uma vez"}, {1, "Uma face por quadro"}, {2, "Sem fatiamento"}}};
// A Unity vai de 16 a 2048; o atlas móvel guarda até 256 por face.
inline constexpr std::array<ComponentEnumOption, 5> reflectionProbeResolutionOptions{{
  {16, "16"}, {32, "32"}, {64, "64"}, {128, "128"}, {256, "256"}}};
inline constexpr std::array<ComponentEnumOption, 2> reflectionProbeClearOptions{{{0, "Céu"}, {1, "Cor sólida"}}};

namespace reflection_probe_detail {
inline const ReflectionProbe &probe(const ComponentValue &v) { return static_cast<const ReflectionProbe &>(v); }
inline ReflectionProbe &probe(ComponentValue &v) { return static_cast<ReflectionProbe &>(v); }
inline bool isRealtime(const ComponentValue &v) { return probe(v).realtime(); }
inline bool solidBackground(const ComponentValue &v) { return probe(v).clearFlags == 1; }
inline constexpr std::string_view capability = "render.probe.reflection";
inline constexpr std::string_view consumer = "renderer/reflection_probes.cpp → atlas de sondas e mistura por desenho";
} // namespace reflection_probe_detail

#define AE_PROBE_NUMBER(id, label, field, lo, hi, step, group, unit, help, visible, inv)                                \
  {label, lo, hi, step, [](const ComponentValue &v) -> const float & { return reflection_probe_detail::probe(v).field; }, \
   [](ComponentValue &v) -> float * { return &reflection_probe_detail::probe(v).field; }, id,                           \
   {group, unit, help, visible, nullptr, reflection_probe_detail::capability, reflection_probe_detail::consumer, inv}}
inline constexpr std::array<ComponentNumber, 14> reflectionProbeNumbers{{
  AE_PROBE_NUMBER("importance", "Importância", importance, 0, 32767, 1, "Tempo de execução", "",
                  "Entre sondas sobrepostas, o objeto mistura as duas de maior importância", nullptr, Invalidate::Probes),
  AE_PROBE_NUMBER("intensity", "Intensidade", intensity, 0, 10, .01f, "Tempo de execução", "",
                  "Multiplica o reflexo desta sonda nos materiais", nullptr, Invalidate::Probes),
  AE_PROBE_NUMBER("blend_distance", "Distância de mistura", blendDistance, 0, 100, .05f, "Tempo de execução", "m",
                  "Faixa dentro da caixa em que o reflexo passa desta sonda para a vizinha ou para o céu", nullptr, Invalidate::Probes),
  AE_PROBE_NUMBER("box_size.x", "Tamanho X", boxSize[0], .01f, 10000, .1f, "Tempo de execução", "m", nullptr, nullptr, Invalidate::Probes),
  AE_PROBE_NUMBER("box_size.y", "Tamanho Y", boxSize[1], .01f, 10000, .1f, "Tempo de execução", "m", nullptr, nullptr, Invalidate::Probes),
  AE_PROBE_NUMBER("box_size.z", "Tamanho Z", boxSize[2], .01f, 10000, .1f, "Tempo de execução", "m", nullptr, nullptr, Invalidate::Probes),
  AE_PROBE_NUMBER("box_offset.x", "Deslocamento X", boxOffset[0], -10000, 10000, .1f, "Tempo de execução", "m", nullptr, nullptr, Invalidate::Probes),
  AE_PROBE_NUMBER("box_offset.y", "Deslocamento Y", boxOffset[1], -10000, 10000, .1f, "Tempo de execução", "m", nullptr, nullptr, Invalidate::Probes),
  AE_PROBE_NUMBER("box_offset.z", "Deslocamento Z", boxOffset[2], -10000, 10000, .1f, "Tempo de execução", "m", nullptr, nullptr, Invalidate::Probes),
  AE_PROBE_NUMBER("background.r", "Fundo R", background[0], 0, 1, .01f, "Captura", "", nullptr,
                  reflection_probe_detail::solidBackground, Invalidate::Probes),
  AE_PROBE_NUMBER("background.g", "Fundo G", background[1], 0, 1, .01f, "Captura", "", nullptr,
                  reflection_probe_detail::solidBackground, Invalidate::Probes),
  AE_PROBE_NUMBER("background.b", "Fundo B", background[2], 0, 1, .01f, "Captura", "", nullptr,
                  reflection_probe_detail::solidBackground, Invalidate::Probes),
  AE_PROBE_NUMBER("near_clip", "Plano próximo", nearClip, .01f, 1000, .01f, "Captura", "m",
                  "Nada mais perto da sonda que isto entra no reflexo", nullptr, Invalidate::Probes),
  AE_PROBE_NUMBER("far_clip", "Plano distante", farClip, .1f, 100000, 1, "Captura", "m",
                  "Nada mais longe que isto entra no reflexo; além dele aparece o fundo", nullptr, Invalidate::Probes)
}};
#undef AE_PROBE_NUMBER

inline constexpr std::array<ComponentBoolean, 3> reflectionProbeBooleans{{
  {"enabled", "Ativa", [](const ComponentValue &v) { return reflection_probe_detail::probe(v).enabled; },
   [](ComponentValue &v, bool next) { reflection_probe_detail::probe(v).enabled = next; },
   {"Tempo de execução", "", nullptr, nullptr, nullptr, reflection_probe_detail::capability, reflection_probe_detail::consumer,
    Invalidate::Probes}},
  {"box_projection", "Projeção na caixa", [](const ComponentValue &v) { return reflection_probe_detail::probe(v).boxProjection; },
   [](ComponentValue &v, bool next) { reflection_probe_detail::probe(v).boxProjection = next; },
   {"Tempo de execução", "", "Paredes a distância finita: o reflexo muda com a posição na sala, em vez de ficar no infinito",
    nullptr, nullptr, reflection_probe_detail::capability, reflection_probe_detail::consumer, Invalidate::Probes}},
  {"hdr", "HDR", [](const ComponentValue &v) { return reflection_probe_detail::probe(v).hdr; },
   [](ComponentValue &v, bool next) { reflection_probe_detail::probe(v).hdr = next; },
   {"Captura", "", "Guarda brilho acima de 1 (sol, lâmpadas); desligado, a captura é cortada em 0..1 e o recurso ocupa metade",
    nullptr, nullptr, reflection_probe_detail::capability, reflection_probe_detail::consumer, Invalidate::Probes}}
}};

inline constexpr std::array<ComponentEnum, 6> reflectionProbeEnums{{
  {"type", "Tipo", reflectionProbeTypeOptions,
   [](const ComponentValue &v) { return reflection_probe_detail::probe(v).probeType; },
   [](ComponentValue &v, u32 n) { reflection_probe_detail::probe(v).probeType = n; },
   {"Geral", "", "Assada é capturada no editor e guardada no projeto; Tempo real recaptura no aparelho", nullptr, nullptr,
    reflection_probe_detail::capability, reflection_probe_detail::consumer, Invalidate::Probes}},
  {"refresh_mode", "Atualização", reflectionProbeRefreshOptions,
   [](const ComponentValue &v) { return reflection_probe_detail::probe(v).refreshMode; },
   [](ComponentValue &v, u32 n) { reflection_probe_detail::probe(v).refreshMode = n; },
   {"Tempo real", "", "Ao acordar captura uma vez no início do Play; Todo quadro recaptura sem parar; Por script espera RenderProbe()",
    reflection_probe_detail::isRealtime, nullptr, reflection_probe_detail::capability, reflection_probe_detail::consumer,
    Invalidate::Probes}},
  {"time_slicing", "Fatiamento no tempo", reflectionProbeSlicingOptions,
   [](const ComponentValue &v) { return reflection_probe_detail::probe(v).timeSlicing; },
   [](ComponentValue &v, u32 n) { reflection_probe_detail::probe(v).timeSlicing = n; },
   {"Tempo real", "", "Espalha a captura e o filtro por vários quadros para não pesar num quadro só",
    reflection_probe_detail::isRealtime, nullptr, reflection_probe_detail::capability, reflection_probe_detail::consumer,
    Invalidate::Probes}},
  {"resolution", "Resolução", reflectionProbeResolutionOptions,
   [](const ComponentValue &v) { return reflection_probe_detail::probe(v).resolution; },
   [](ComponentValue &v, u32 n) { reflection_probe_detail::probe(v).resolution = n; },
   {"Captura", "px", "Lado de cada face capturada; mais nitidez em metal liso, mais custo de captura", nullptr, nullptr,
    reflection_probe_detail::capability, reflection_probe_detail::consumer, Invalidate::Probes}},
  {"clear_flags", "Fundo", reflectionProbeClearOptions,
   [](const ComponentValue &v) { return reflection_probe_detail::probe(v).clearFlags; },
   [](ComponentValue &v, u32 n) { reflection_probe_detail::probe(v).clearFlags = n; },
   {"Captura", "", "O que aparece onde não há objeto: o céu da cena ou uma cor", nullptr, nullptr,
    reflection_probe_detail::capability, reflection_probe_detail::consumer, Invalidate::Probes}},
  {"culling_mask", "Máscara de culling", objectLayerOptions,
   [](const ComponentValue &v) { return reflection_probe_detail::probe(v).cullingMask; },
   [](ComponentValue &v, u32 n) { reflection_probe_detail::probe(v).cullingMask = n; },
   {"Captura", "", "Só objetos destas camadas entram no reflexo", nullptr, nullptr,
    reflection_probe_detail::capability, reflection_probe_detail::consumer, Invalidate::Probes},
   false, true}
}};
inline constexpr std::array<ComponentTriple, 3> reflectionProbeTriples{{
  {"box_size", "Tamanho da caixa", {"box_size.x", "box_size.y", "box_size.z"}, ComponentTripleKind::Vector},
  {"box_offset", "Deslocamento da caixa", {"box_offset.x", "box_offset.y", "box_offset.z"}, ComponentTripleKind::Vector},
  {"background", "Cor de fundo", {"background.r", "background.g", "background.b"}, ComponentTripleKind::SrgbColor}
}};
inline constexpr std::array<ComponentResourceBinding, 1> reflectionProbeResources{{
  {"baked_texture", "Cubemap assado", resources::AssetType::ReflectionProbe,
   [](const ComponentValue &) { return 1u; },
   [](const ComponentValue &v, u32) { return reflection_probe_detail::probe(v).bakedTexture; },
   [](ComponentValue &v, u32, resources::AssetGuid value) { reflection_probe_detail::probe(v).bakedTexture = value; return true; },
   {"Captura", "", "Escrito pelo Assar; Tempo real não usa", nullptr, nullptr, reflection_probe_detail::capability,
    reflection_probe_detail::consumer, Invalidate::Probes}}
}};
// RenderProbe() da Unity: pede uma captura da sonda Tempo real em "Por script".
inline constexpr std::array<ComponentMethod, 1> reflectionProbeMethods{{
  {"render_probe", "Capturar agora", "Recaptura a sonda de tempo real no próximo quadro (RenderProbe da Unity)"}
}};

inline bool ReflectionProbe::valid() const {
  for (const auto &p : descriptor.numbers) {
    const float v = p.read(*this);
    if (!std::isfinite(v) || v < p.minimum || v > p.maximum) return false;
  }
  if (importance != std::floor(importance) || probeType > 1 || refreshMode > 2 || timeSlicing > 2 || clearFlags > 1) return false;
  if (resolution != 16 && resolution != 32 && resolution != 64 && resolution != 128 && resolution != 256) return false;
  return nearClip < farClip;
}
inline void ReflectionProbe::write(std::ostream &out) const {
  out << enabled << ' ' << probeType << ' ' << refreshMode << ' ' << timeSlicing << ' ' << boxProjection << ' ' << resolution << ' '
      << hdr << ' ' << clearFlags << ' ' << cullingMask << ' ' << bakedTexture.high << ' ' << bakedTexture.low;
  for (const auto &p : descriptor.numbers) out << ' ' << p.read(*this);
}
inline bool ReflectionProbe::read(std::istream &in, u32 version) {
  if (version != 1) return false;
  ReflectionProbe value;
  if (!(in >> value.enabled >> value.probeType >> value.refreshMode >> value.timeSlicing >> value.boxProjection >> value.resolution >>
        value.hdr >> value.clearFlags >> value.cullingMask >> value.bakedTexture.high >> value.bakedTexture.low)) return false;
  for (const auto &p : descriptor.numbers) if (!(in >> *p.write(value))) return false;
  if (!value.valid()) return false;
  // A base (identidade da instância) é do objeto lido, não do temporário.
  static_cast<ComponentValue &>(value) = static_cast<const ComponentValue &>(*this);
  *this = value;
  return true;
}
inline const ComponentType ReflectionProbe::descriptor{
  "astra.render.reflection_probe", 1, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<ReflectionProbe>(); },
  reflectionProbeNumbers, reflectionProbeBooleans, reflectionProbeEnums, nullptr, false, {}, reflectionProbeTriples,
  reflectionProbeResources, {}, {}, {}, reflectionProbeMethods};

} // namespace ae::scene

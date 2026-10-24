#pragma once

#include "renderer/scene_environment.h"
#include "renderer/volume_overrides.h"

#include <array>
#include <istream>
#include <ostream>
#include <span>
#include <string_view>

namespace ae::renderer {

// PK-4 (V-02): o Volume Profile da Unity 6 (URP/HDRP 17) é uma lista de
// Volume Overrides (Bloom, Vignette, Depth of Field...). Cada override tem um
// interruptor no cabeçalho (VolumeComponent.active) e cada parâmetro dele uma
// caixa própria (VolumeParameter.overrideState): marcada, o volume decide o
// valor; desmarcada, vale o que vem do volume de prioridade menor e, no fim,
// os padrões do mundo. ALL/NONE marcam ou desmarcam todas as caixas do
// override. Referências: docs.unity3d.com/6000.0/Documentation/Manual/urp/
// VolumeOverrides.html e Volumes.html; VolumeComponent.cs e VolumeParameter.cs
// do SRP Core 17 (Interp: discretos trocam quando t > 0).
//
// Aqui cada override é um VolumeGroup e cada parâmetro tem um índice estável
// nesta tabela, que é o endereço gravado em cena e perfil. A tabela só cresce
// no fim: reordenar mudaria o significado dos bits já gravados.
struct VolumeGroupInfo final {
  // Rótulo do override no Inspector; é também o grupo de apresentação das
  // propriedades do componente Ambiente que pertencem a ele.
  std::string_view name;
  // Booleano do componente que diz se o override está no perfil (Add Override).
  std::string_view addedProperty;
  // Categoria do menu "Adicionar override", como os submenus da Unity.
  std::string_view category;
  // Referência da Unity (manual 6000.0) que define o override.
  std::string_view reference;
};
std::span<const VolumeGroupInfo> volumeGroups() noexcept;
inline const VolumeGroupInfo &volumeGroupInfo(VolumeGroup group) noexcept {
  return volumeGroups()[static_cast<u32>(group)];
}
// Grupo de apresentação do Inspector -> override; Count quando não é override.
VolumeGroup volumeGroupForPresentation(std::string_view presentationGroup) noexcept;

enum class VolumeParameterKind : u8 { Float, Floats, Bool, Enum, Guid, Curve };
struct VolumeParameter final {
  std::string_view id;      // id da propriedade do componente (número, enum, bool, trio, recurso)
  VolumeGroup group;
  VolumeParameterKind kind;
  u8 count;                 // floats contíguos (Floats) ou índice da curva (Curve)
  void *(*field)(SceneEnvironment &);
  // Outro id de propriedade que endereça o mesmo parâmetro (a cor e o
  // deslocamento de um trackball, o menu de cinza alvo e o número dele).
  std::string_view alias{};
};
std::span<const VolumeParameter> volumeParameters() noexcept;
// Parâmetro de uma propriedade do componente; aceita canais de trio
// ("bloom_tint.r") e aliases. ~0u quando a propriedade não é de Volume
// (forma, peso, prioridade, perfil...).
u32 volumeParameterIndex(std::string_view propertyId) noexcept;
// As oito curvas de Color Curves: id do parâmetro, rótulo e o que o eixo X e o
// Y medem (como no editor de curvas da URP).
inline std::string_view colorCurveParameterId(u32 channel) noexcept {
  constexpr std::string_view ids[]{"curve_master","curve_red","curve_green","curve_blue","curve_hue_vs_hue",
                                   "curve_hue_vs_sat","curve_sat_vs_sat","curve_lum_vs_sat"};
  return channel < std::size(ids) ? ids[channel] : std::string_view{};
}
inline const char *colorCurveName(u32 channel) noexcept {
  constexpr const char *names[]{"Mestre","Vermelho","Verde","Azul","Matiz × matiz","Matiz × saturação",
                                "Saturação × saturação","Luminância × saturação"};
  return channel < std::size(names) ? names[channel] : "";
}
inline const char *colorCurveUnityName(u32 channel) noexcept {
  constexpr const char *names[]{"Master","Red","Green","Blue","Hue Vs Hue","Hue Vs Sat","Sat Vs Sat","Lum Vs Sat"};
  return channel < std::size(names) ? names[channel] : "";
}

// Migração das caixas por grupo (Ambiente até a v18, perfil até a v13): cada
// grupo marcado vira o override com todos os parâmetros marcados.
VolumeOverrides volumeOverridesFromLegacyGroups(u32 legacyGroups) noexcept;

// Texto: "<palavras> w0 .. wn <adicionados> <ativos>", em hexadecimal.
void writeVolumeOverrides(std::ostream &out, const VolumeOverrides &overrides);
bool readVolumeOverrides(std::istream &in, VolumeOverrides &out);

// Curvas de cor no texto: "<n> t v in out esq dir quebrada ..." por curva.
void writeColorCurves(std::ostream &out, const SceneEnvironment &values);
bool readColorCurves(std::istream &in, SceneEnvironment &values);

// Mistura de UM parâmetro de `target` em `result` com peso `amount`, como o
// VolumeParameter.Interp: números lineares, discretos quando amount > 0,
// curvas pela média amostrada (TextureCurve.Interp).
void blendVolumeParameter(const VolumeParameter &parameter, SceneEnvironment &result,
                          const SceneEnvironment &target, float amount) noexcept;
// Copia os parâmetros de um override (ou todos) de `source` para `target`.
void copyVolumeGroup(VolumeGroup group, const SceneEnvironment &source, SceneEnvironment &target) noexcept;
bool sameVolumeParameter(const VolumeParameter &parameter, const SceneEnvironment &a, const SceneEnvironment &b) noexcept;

} // namespace ae::renderer

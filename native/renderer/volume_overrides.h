#pragma once

#include "core/base.h"

#include <array>

namespace ae::renderer {

// PK-4: estado de override de um Volume Profile (ver renderer/volume_parameters.h,
// que define o que cada grupo e cada bit significam). Separado para que
// SceneEnvironmentVolume o carregue sem depender da tabela de parâmetros.
enum class VolumeGroup : u32 {
  Sky = 0, Celestial, PhysicalAtmosphere, Ambient, Fog, Clouds, Exposure, Tonemapping, Bloom, Vignette,
  ColorAdjustments, ColorGrading, ColorCurves, FilmGrain, AmbientOcclusion, DepthOfField, MotionBlur,
  ChromaticAberration, LensDistortion, Count
};
inline constexpr u32 VolumeGroupCount = static_cast<u32>(VolumeGroup::Count);
inline constexpr u32 VolumeGroupsAll = (1u << VolumeGroupCount) - 1u;

inline constexpr u32 VolumeParameterCapacity = 256;

// Estado de override de um perfil (ou de um Ambiente sem perfil).
struct VolumeOverrides final {
  std::array<u32, VolumeParameterCapacity / 32> parameters{};
  u32 added = 0;               // overrides presentes no perfil (Add Override)
  u32 active = VolumeGroupsAll; // interruptor do cabeçalho de cada override

  bool operator==(const VolumeOverrides &) const = default;
  bool overridden(u32 parameter) const noexcept {
    return parameter < VolumeParameterCapacity && (parameters[parameter >> 5] & (1u << (parameter & 31))) != 0;
  }
  void setOverridden(u32 parameter, bool value) noexcept {
    if (parameter >= VolumeParameterCapacity) return;
    if (value) parameters[parameter >> 5] |= 1u << (parameter & 31);
    else parameters[parameter >> 5] &= ~(1u << (parameter & 31));
  }
  bool hasGroup(VolumeGroup group) const noexcept { return (added & (1u << static_cast<u32>(group))) != 0; }
  bool groupActive(VolumeGroup group) const noexcept { return (active & (1u << static_cast<u32>(group))) != 0; }
  // O parâmetro entra na mistura: override no perfil, ligado, caixa marcada.
  bool contributes(u32 parameter) const noexcept;
  // ALL / NONE do cabeçalho do override.
  void setGroupParameters(VolumeGroup group, bool value) noexcept;
  // Adiciona (todas as caixas desmarcadas, como o Add Override da Unity) ou
  // remove o override do perfil.
  void addGroup(VolumeGroup group) noexcept;
  void removeGroup(VolumeGroup group) noexcept;
  u32 overriddenInGroup(VolumeGroup group) const noexcept;
  u32 parametersInGroup(VolumeGroup group) const noexcept;
  // Tudo adicionado e marcado: o comportamento de um Ambiente antes do PK-4.
  static VolumeOverrides everything() noexcept;
  // Nada no perfil: o Volume novo da Unity.
  static VolumeOverrides empty() noexcept { VolumeOverrides result;result.active=VolumeGroupsAll;return result; }
  bool valid() const noexcept;
};


} // namespace ae::renderer

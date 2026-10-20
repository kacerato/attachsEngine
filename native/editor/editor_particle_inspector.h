#pragma once
// Layout do inspetor do Sistema de partículas (G5 P1).
//
// O inspetor genérico agrupa por tipo de controle (interruptores, depois
// listas, depois números). Para partículas isso separa "Vida inicial · modo" da
// própria vida inicial e esconde a curva longe do modo que a liga. Aqui fica a
// ordem dos módulos como na Unity (Manual/PartSysMainModule.html): cada valor
// logo abaixo do seu modo, a curva ou o gradiente logo abaixo do valor, as
// rajadas dentro de Emissão. É só ordem e rótulo: os controles continuam os
// refletidos do componente, com a mesma validação, histórico e API.
#include "scene/particle_system.h"

#include <array>
#include <string_view>

namespace ae::editor {

inline constexpr std::string_view kParticlePreviewRow = "preview";

inline constexpr std::array<std::string_view, 150> particleInspectorOrder{{
  // Principal
  kParticlePreviewRow, "duration", "looping", "prewarm",
  "start_delay_mode", "start_delay", "start_delay_min",
  "start_lifetime_mode", "start_lifetime", "start_lifetime_min", "start_lifetime_multiplier", "start_lifetime.max", "start_lifetime.min",
  "start_speed_mode", "start_speed", "start_speed_min", "start_speed_multiplier", "start_speed.max", "start_speed.min",
  "start_size_mode", "start_size", "start_size_min", "start_size_multiplier", "start_size.max", "start_size.min",
  "start_rotation_mode", "start_rotation", "start_rotation_min", "start_rotation_multiplier", "start_rotation.max", "start_rotation.min",
  "start_color_mode", "start_color", "start_color_a", "start_color_min", "start_color_min_a", "start_color.max", "start_color.min",
  "gravity_modifier_mode", "gravity_modifier", "gravity_modifier_min", "gravity_modifier_multiplier", "gravity_modifier.max", "gravity_modifier.min",
  "simulation_space", "simulation_speed", "play_on_awake", "max_particles", "stop_action", "auto_random_seed", "random_seed",
  // Emissão
  "emission_enabled", "emission_rate_mode", "emission_rate", "emission_rate_min", "emission_rate_multiplier", "emission_rate.max", "emission_rate.min",
  "burst_count",
  "burst_0_time", "burst_0_count_min", "burst_0_count_max", "burst_0_cycles", "burst_0_interval", "burst_0_probability",
  "burst_1_time", "burst_1_count_min", "burst_1_count_max", "burst_1_cycles", "burst_1_interval", "burst_1_probability",
  "burst_2_time", "burst_2_count_min", "burst_2_count_max", "burst_2_cycles", "burst_2_interval", "burst_2_probability",
  "burst_3_time", "burst_3_count_min", "burst_3_count_max", "burst_3_cycles", "burst_3_interval", "burst_3_probability",
  // Forma
  "shape_enabled", "shape_type", "shape_radius", "shape_radius_thickness", "shape_angle", "shape_arc", "shape_cone_emit_from",
  "shape_length", "shape_box_emit_from", "shape_box", "shape_box_x", "shape_box_y", "shape_box_z",
  "shape_position", "shape_position_x", "shape_position_y", "shape_position_z",
  "shape_rotation", "shape_rotation_x", "shape_rotation_y", "shape_rotation_z", "shape_random_direction", "shape_spherize_direction",
  // Ao longo da vida
  "color_over_lifetime_enabled", "color_over_lifetime_mode", "color_over_lifetime.max", "color_over_lifetime.min",
  "size_over_lifetime_enabled", "size_over_lifetime_mode", "size_over_lifetime", "size_over_lifetime_min", "size_over_lifetime_multiplier",
  "size_over_lifetime.max", "size_over_lifetime.min",
  "rotation_over_lifetime_enabled", "rotation_over_lifetime_mode", "rotation_over_lifetime", "rotation_over_lifetime_min",
  "rotation_over_lifetime_multiplier", "rotation_over_lifetime.max", "rotation_over_lifetime.min",
  // Renderização
  "render_mode", "stretch_velocity_scale", "stretch_length_scale", "sort_mode", "sorting_fudge", "max_particle_size",
  // Material
  "material_blend", "material_texture", "material_tint", "material_tint_r", "material_tint_g", "material_tint_b", "material_emission",
  "material_scroll", "material_scroll_x", "material_scroll_y", "material_noise", "material_noise_scale",
  "material_noise_scroll_x", "material_noise_scroll_y", "material_noise_strength", "material_noise_distortion",
  "material_mask", "material_center_glow", "material_center_glow_power", "material_soft_distance",
}};

// Interruptor do módulo, desenhado no cabeçalho da seção (Unity: a caixa ao
// lado do nome do módulo). Vazio para seções sem interruptor.
inline std::string_view particleModuleToggle(std::string_view group) {
  if (group == "Emissão") return "emission_enabled";
  if (group == "Forma") return "shape_enabled";
  if (group == "Cor no tempo") return "color_over_lifetime_enabled";
  if (group == "Tamanho no tempo") return "size_over_lifetime_enabled";
  if (group == "Rotação no tempo") return "rotation_over_lifetime_enabled";
  return {};
}
// Rótulo curto do chip de modo Min/Max (cabe ao lado do valor).
inline const char *particleModeChip(u32 mode) {
  switch (mode) {
  case 0: return "Fixo";
  case 1: return "Curva";
  case 2: return "Entre";
  case 3: return "2 curvas";
  }
  return "?";
}

inline u32 particleInspectorRank(std::string_view id) {
  for (u32 i = 0; i < particleInspectorOrder.size(); ++i)
    if (particleInspectorOrder[i] == id) return i;
  return static_cast<u32>(particleInspectorOrder.size());
}

// Base da propriedade de uma curva/gradiente ("start_lifetime.max" → "start_lifetime").
inline std::string_view particleSlotBase(std::string_view slot) {
  const auto dot = slot.find('.');
  return dot == std::string_view::npos ? slot : slot.substr(0, dot);
}
// Módulo (grupo do inspetor) onde a linha da curva/gradiente aparece.
inline std::string_view particleSlotGroup(std::string_view slot) {
  const auto base = particleSlotBase(slot);
  if (base == "emission_rate") return "Emissão";
  if (base == "color_over_lifetime") return "Cor no tempo";
  if (base == "size_over_lifetime") return "Tamanho no tempo";
  if (base == "rotation_over_lifetime") return "Rotação no tempo";
  return "Principal";
}
inline const char *particleSlotName(std::string_view slot) {
  const auto base = particleSlotBase(slot);
  if (base == "start_delay") return "Atraso inicial";
  if (base == "start_lifetime") return "Vida inicial";
  if (base == "start_speed") return "Velocidade inicial";
  if (base == "start_size") return "Tamanho inicial";
  if (base == "start_rotation") return "Rotação inicial";
  if (base == "start_color") return "Cor inicial";
  if (base == "gravity_modifier") return "Gravidade";
  if (base == "emission_rate") return "Taxa por segundo";
  if (base == "color_over_lifetime") return "Cor no tempo";
  if (base == "size_over_lifetime") return "Tamanho no tempo";
  if (base == "rotation_over_lifetime") return "Velocidade angular";
  return "";
}

// Linha de curva visível: a curva máxima em Curva e Entre duas curvas; a
// mínima só em Entre duas curvas.
inline bool particleCurveRowVisible(const scene::ParticleSystem &system, const scene::ParticleSystem::CurveSlot &slot) {
  const auto &value = system.*slot.member;
  if (!value.usesCurves()) return false;
  return slot.maximum || value.mode == scene::ParticleCurveMode::TwoCurves;
}
inline bool particleGradientRowVisible(const scene::ParticleSystem &system, const scene::ParticleSystem::GradientSlot &slot) {
  const auto &value = system.*slot.member;
  using Mode = scene::ParticleGradientMode;
  if (slot.maximum) return value.mode == Mode::Gradient || value.mode == Mode::TwoGradients || value.mode == Mode::RandomColor;
  return value.mode == Mode::TwoGradients;
}
inline const char *particleCurveRowLabel(const scene::ParticleSystem &system, const scene::ParticleSystem::CurveSlot &slot) {
  if (!slot.maximum) return "Curva mínima";
  return (system.*slot.member).mode == scene::ParticleCurveMode::TwoCurves ? "Curva máxima" : "Curva";
}
inline const char *particleGradientRowLabel(const scene::ParticleSystem &system, const scene::ParticleSystem::GradientSlot &slot) {
  if (!slot.maximum) return "Gradiente mínimo";
  return (system.*slot.member).mode == scene::ParticleGradientMode::TwoGradients ? "Gradiente máximo" : "Gradiente";
}

} // namespace ae::editor

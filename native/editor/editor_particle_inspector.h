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

inline constexpr std::array<std::string_view, 383> particleInspectorOrder{{
  // Principal
  kParticlePreviewRow, "duration", "looping", "prewarm",
  "start_delay_mode", "start_delay", "start_delay_min",
  "start_lifetime_mode", "start_lifetime", "start_lifetime_min", "start_lifetime_multiplier", "start_lifetime.max", "start_lifetime.min",
  "start_speed_mode", "start_speed", "start_speed_min", "start_speed_multiplier", "start_speed.max", "start_speed.min",
  "start_size_mode", "start_size", "start_size_min", "start_size_multiplier", "start_size.max", "start_size.min",
  "size_3d", "start_size_y_mode", "start_size_y", "start_size_y_min", "start_size_y_multiplier", "start_size_y.max", "start_size_y.min",
  "start_size_z_mode", "start_size_z", "start_size_z_min", "start_size_z_multiplier", "start_size_z.max", "start_size_z.min",
  "start_rotation_mode", "start_rotation", "start_rotation_min", "start_rotation_multiplier", "start_rotation.max", "start_rotation.min",
  "rotation_3d", "start_rotation_x_mode", "start_rotation_x", "start_rotation_x_min", "start_rotation_x_multiplier", "start_rotation_x.max", "start_rotation_x.min",
  "start_rotation_y_mode", "start_rotation_y", "start_rotation_y_min", "start_rotation_y_multiplier", "start_rotation_y.max", "start_rotation_y.min",
  "start_color_mode", "start_color", "start_color_a", "start_color_min", "start_color_min_a", "start_color.max", "start_color.min",
  "gravity_modifier_mode", "gravity_modifier", "gravity_modifier_min", "gravity_modifier_multiplier", "gravity_modifier.max", "gravity_modifier.min",
  "simulation_space", "simulation_speed", "scaling_mode", "play_on_awake", "max_particles", "stop_action", "auto_random_seed", "random_seed",
  // Emissão
  "emission_enabled", "emission_rate_mode", "emission_rate", "emission_rate_min", "emission_rate_multiplier", "emission_rate.max", "emission_rate.min",
  "emission_rate_over_distance", "burst_count",
  "burst_0_time", "burst_0_count_min", "burst_0_count_max", "burst_0_cycles", "burst_0_interval", "burst_0_probability",
  "burst_1_time", "burst_1_count_min", "burst_1_count_max", "burst_1_cycles", "burst_1_interval", "burst_1_probability",
  "burst_2_time", "burst_2_count_min", "burst_2_count_max", "burst_2_cycles", "burst_2_interval", "burst_2_probability",
  "burst_3_time", "burst_3_count_min", "burst_3_count_max", "burst_3_cycles", "burst_3_interval", "burst_3_probability",
  // Forma
  "shape_enabled", "shape_type", "shape_radius", "shape_radius_thickness", "shape_angle", "shape_arc", "shape_donut_radius", "shape_cone_emit_from",
  "shape_length", "shape_box_emit_from", "shape_box", "shape_box_x", "shape_box_y", "shape_box_z",
  "shape_position", "shape_position_x", "shape_position_y", "shape_position_z",
  "shape_rotation", "shape_rotation_x", "shape_rotation_y", "shape_rotation_z", "shape_random_direction", "shape_spherize_direction",
  // Ao longo da vida
  "color_over_lifetime_enabled", "color_over_lifetime_mode", "color_over_lifetime.max", "color_over_lifetime.min",
  "size_over_lifetime_enabled", "size_over_lifetime_mode", "size_over_lifetime", "size_over_lifetime_min", "size_over_lifetime_multiplier",
  "size_over_lifetime.max", "size_over_lifetime.min",
  "size_separate_axes", "size_over_lifetime_y_mode", "size_over_lifetime_y", "size_over_lifetime_y_min", "size_over_lifetime_y_multiplier", "size_over_lifetime_y.max", "size_over_lifetime_y.min",
  "size_over_lifetime_z_mode", "size_over_lifetime_z", "size_over_lifetime_z_min", "size_over_lifetime_z_multiplier", "size_over_lifetime_z.max", "size_over_lifetime_z.min",
  "rotation_over_lifetime_enabled", "rotation_over_lifetime_mode", "rotation_over_lifetime", "rotation_over_lifetime_min",
  "rotation_over_lifetime_multiplier", "rotation_over_lifetime.max", "rotation_over_lifetime.min",
  "rotation_separate_axes", "rotation_over_lifetime_x_mode", "rotation_over_lifetime_x", "rotation_over_lifetime_x_min", "rotation_over_lifetime_x_multiplier", "rotation_over_lifetime_x.max", "rotation_over_lifetime_x.min",
  "rotation_over_lifetime_y_mode", "rotation_over_lifetime_y", "rotation_over_lifetime_y_min", "rotation_over_lifetime_y_multiplier", "rotation_over_lifetime_y.max", "rotation_over_lifetime_y.min",
  // Movimento (P2a): Velocity, Limit Velocity, Force over Lifetime e Noise
  "velocity_over_lifetime_enabled", "velocity_x_mode", "velocity_x", "velocity_x_min", "velocity_x_multiplier",
  "velocity_x.max", "velocity_x.min", "velocity_y_mode", "velocity_y", "velocity_y_min", "velocity_y_multiplier",
  "velocity_y.max", "velocity_y.min", "velocity_z_mode", "velocity_z", "velocity_z_min", "velocity_z_multiplier",
  "velocity_z.max", "velocity_z.min", "velocity_space", "speed_modifier_mode", "speed_modifier",
  "speed_modifier_min", "speed_modifier_multiplier", "speed_modifier.max", "speed_modifier.min",
  "velocity_orbital_x_mode", "velocity_orbital_x", "velocity_orbital_x_min", "velocity_orbital_x_multiplier", "velocity_orbital_x.max", "velocity_orbital_x.min",
  "velocity_orbital_y_mode", "velocity_orbital_y", "velocity_orbital_y_min", "velocity_orbital_y_multiplier", "velocity_orbital_y.max", "velocity_orbital_y.min",
  "velocity_orbital_z_mode", "velocity_orbital_z", "velocity_orbital_z_min", "velocity_orbital_z_multiplier", "velocity_orbital_z.max", "velocity_orbital_z.min",
  "velocity_orbital_offset_x", "velocity_orbital_offset_y", "velocity_orbital_offset_z",
  "velocity_radial_mode", "velocity_radial", "velocity_radial_min", "velocity_radial_multiplier", "velocity_radial.max", "velocity_radial.min",
  "limit_velocity_enabled", "limit_speed_mode", "limit_speed", "limit_speed_min", "limit_speed_multiplier",
  "limit_speed.max", "limit_speed.min", "limit_dampen", "limit_drag", "force_over_lifetime_enabled", "force_x_mode",
  "force_x", "force_x_min", "force_x_multiplier", "force_x.max", "force_x.min", "force_y_mode", "force_y",
  "force_y_min", "force_y_multiplier", "force_y.max", "force_y.min", "force_z_mode", "force_z", "force_z_min",
  "force_z_multiplier", "force_z.max", "force_z.min", "force_space", "noise_enabled", "noise_strength_mode",
  "noise_strength", "noise_strength_min", "noise_strength_multiplier", "noise_strength.max", "noise_strength.min",
  "noise_frequency", "noise_scroll_speed", "noise_octaves",
  // Herdar velocidade e "pela velocidade" (P2c)
  "inherit_velocity_enabled", "inherit_velocity_multiplier", "size_by_speed_enabled", "size_by_speed_mode",
  "size_by_speed", "size_by_speed_min", "size_by_speed_multiplier", "size_by_speed.max", "size_by_speed.min",
  "size_by_speed_range_min", "size_by_speed_range_max", "color_by_speed_enabled", "color_by_speed_mode",
  "color_by_speed.max", "color_by_speed.min", "color_by_speed_range_min", "color_by_speed_range_max",
  "rotation_by_speed_enabled", "rotation_by_speed_mode", "rotation_by_speed", "rotation_by_speed_min",
  "rotation_by_speed_multiplier", "rotation_by_speed.max", "rotation_by_speed.min", "rotation_by_speed_range_min",
  "rotation_by_speed_range_max",
  // Colisão e sub-emissores (P2b)
  "collision_enabled", "collision_type", "collision_plane_0", "collision_plane_1", "collision_dampen", "collision_bounce", "collision_lifetime_loss", "collision_min_kill_speed", "collision_radius_scale",
  "sub_emitters_enabled", "sub_emitter_birth", "sub_emitter_birth_count", "sub_emitter_collision", "sub_emitter_collision_count", "sub_emitter_death", "sub_emitter_death_count",
  "sub_emitter_extra_0", "sub_emitter_extra_0_type", "sub_emitter_extra_1", "sub_emitter_extra_1_type", "sub_emitter_extra_2", "sub_emitter_extra_2_type",
  // Folha de textura (PK-10a): grade, linha, quadro no tempo, início e ciclos
  "texture_sheet_enabled", "sheet_tiles_x", "sheet_tiles_y", "sheet_animation", "sheet_row_mode", "sheet_row",
  "sheet_frame_mode", "sheet_frame", "sheet_frame_min", "sheet_frame_multiplier", "sheet_frame.max", "sheet_frame.min",
  "sheet_start_frame_mode", "sheet_start_frame", "sheet_start_frame_min", "sheet_cycles",
  // Luzes (PK-10b)
  "lights_enabled", "light_template", "light_ratio", "light_max", "light_use_particle_color", "light_size_affects_range",
  "light_alpha_affects_intensity", "light_range_multiplier", "light_intensity_multiplier",
  // Trilhas (PK-10c)
  "trails_enabled", "trail_ratio", "trail_lifetime", "trail_min_vertex_distance", "trail_width", "trail_width_end",
  "trail_inherit_color", "trail_fade",
  // Renderização
  "render_mode", "render_alignment", "mesh_shape", "mesh", "stretch_velocity_scale", "stretch_length_scale", "sort_mode", "sorting_fudge", "max_particle_size",
  // Material: o recurso do projeto primeiro (como o Material do Renderer da Unity)
  "material", "material_blend", "material_texture", "material_tint", "material_tint_r", "material_tint_g", "material_tint_b", "material_emission",
  "material_scroll", "material_scroll_x", "material_scroll_y", "material_noise", "material_noise_scale",
  "material_noise_scroll_x", "material_noise_scroll_y", "material_noise_strength", "material_noise_distortion",
  "material_mask", "material_center_glow", "material_center_glow_power", "material_soft_distance", "material_distortion",
}};

// Interruptor do módulo, desenhado no cabeçalho da seção (Unity: a caixa ao
// lado do nome do módulo). Vazio para seções sem interruptor.
inline std::string_view particleModuleToggle(std::string_view group) {
  if (group == "Emissão") return "emission_enabled";
  if (group == "Forma") return "shape_enabled";
  if (group == "Cor no tempo") return "color_over_lifetime_enabled";
  if (group == "Tamanho no tempo") return "size_over_lifetime_enabled";
  if (group == "Rotação no tempo") return "rotation_over_lifetime_enabled";
  if (group == "Velocidade no tempo") return "velocity_over_lifetime_enabled";
  if (group == "Limite de velocidade") return "limit_velocity_enabled";
  if (group == "Força no tempo") return "force_over_lifetime_enabled";
  if (group == "Ruído") return "noise_enabled";
  if (group == "Colisão") return "collision_enabled";
  if (group == "Herdar velocidade") return "inherit_velocity_enabled";
  if (group == "Tamanho pela velocidade") return "size_by_speed_enabled";
  if (group == "Cor pela velocidade") return "color_by_speed_enabled";
  if (group == "Rotação pela velocidade") return "rotation_by_speed_enabled";
  if (group == "Sub-emissores") return "sub_emitters_enabled";
  if (group == "Folha de textura") return "texture_sheet_enabled";
  if (group == "Luzes") return "lights_enabled";
  if (group == "Trilhas") return "trails_enabled";
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
  if (base.starts_with("size_over_lifetime")) return "Tamanho no tempo";
  if (base.starts_with("rotation_over_lifetime")) return "Rotação no tempo";
  if (base.starts_with("velocity_") || base == "speed_modifier") return "Velocidade no tempo";
  if (base == "limit_speed") return "Limite de velocidade";
  if (base.starts_with("force_")) return "Força no tempo";
  if (base == "noise_strength") return "Ruído";
  if (base == "size_by_speed") return "Tamanho pela velocidade";
  if (base == "color_by_speed") return "Cor pela velocidade";
  if (base == "rotation_by_speed") return "Rotação pela velocidade";
  if (base == "sheet_frame") return "Folha de textura";
  return "Principal";
}
inline const char *particleSlotName(std::string_view slot) {
  const auto base = particleSlotBase(slot);
  if (base == "start_delay") return "Atraso inicial";
  if (base == "start_lifetime") return "Vida inicial";
  if (base == "start_speed") return "Velocidade inicial";
  if (base == "start_size") return "Tamanho inicial";
  if (base == "start_size_y") return "Tamanho inicial Y";
  if (base == "start_size_z") return "Tamanho inicial Z";
  if (base == "start_rotation_x") return "Rotação inicial X";
  if (base == "start_rotation_y") return "Rotação inicial Y";
  if (base == "size_over_lifetime_y") return "Tamanho no tempo Y";
  if (base == "size_over_lifetime_z") return "Tamanho no tempo Z";
  if (base == "rotation_over_lifetime_x") return "Velocidade angular X";
  if (base == "rotation_over_lifetime_y") return "Velocidade angular Y";
  if (base == "start_rotation") return "Rotação inicial";
  if (base == "start_color") return "Cor inicial";
  if (base == "gravity_modifier") return "Gravidade";
  if (base == "emission_rate") return "Taxa por segundo";
  if (base == "color_over_lifetime") return "Cor no tempo";
  if (base == "size_over_lifetime") return "Tamanho no tempo";
  if (base == "rotation_over_lifetime") return "Velocidade angular";
  if (base == "velocity_x") return "Velocidade X";
  if (base == "velocity_y") return "Velocidade Y";
  if (base == "velocity_z") return "Velocidade Z";
  if (base == "speed_modifier") return "Modificador de velocidade";
  if (base == "velocity_orbital_x") return "Órbita X";
  if (base == "velocity_orbital_y") return "Órbita Y";
  if (base == "velocity_orbital_z") return "Órbita Z";
  if (base == "velocity_radial") return "Radial";
  if (base == "limit_speed") return "Velocidade máxima";
  if (base == "force_x") return "Força X";
  if (base == "force_y") return "Força Y";
  if (base == "force_z") return "Força Z";
  if (base == "noise_strength") return "Força do ruído";
  if (base == "size_by_speed") return "Tamanho pela velocidade";
  if (base == "color_by_speed") return "Cor pela velocidade";
  if (base == "rotation_by_speed") return "Velocidade angular";
  if (base == "sheet_frame") return "Quadro no tempo";
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

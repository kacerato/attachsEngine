#pragma once

#include "resources/environment_profile.h"

#include <optional>
#include <span>
#include <string_view>

namespace ae::resources {

// G2 item 7: perfis de pós prontos. O autor escolhe um e ajusta; ninguém
// precisa saber montar pós do zero. Cada perfil é um EnvironmentProfile
// embutido (GUID estável derivado da chave), mas aplicá-lo num Ambiente troca
// só o grupo Pós — tonemap, brilho, vinheta, grão, contraste, saturação e a
// gradação de cor. Céu, neblina, exposição e oclusão continuam da cena: são do
// lugar, não do estilo. Referência de fluxo: Volume Profile da URP 17 (perfis
// reutilizáveis aplicados por override) e os presets de Environment da Godot.
enum class EnvironmentLook : u32 {
  Neutral = 0,
  Cinematic,
  WarmDay,
  Sunset,
  BlueNight,
  Storm,
  Fantasy,
  Count
};

struct EnvironmentLookPreset final {
  EnvironmentLook look;
  std::string_view key;  // estável: API C#, testes e GUID
  std::string_view name; // o que o autor lê no cartão
};

std::span<const EnvironmentLookPreset> environmentLookPresets() noexcept;
const EnvironmentLookPreset *findEnvironmentLook(std::string_view key) noexcept;

// Escreve no ambiente só os campos do grupo Pós que o perfil define (e liga o pós).
void applyEnvironmentLook(renderer::SceneEnvironment &target, EnvironmentLook look) noexcept;
// O perfil cujo grupo Pós é exatamente o do ambiente; vazio quando o autor já
// ajustou algo à mão (o cartão então mostra "Personalizado").
std::optional<EnvironmentLook> matchEnvironmentLook(const renderer::SceneEnvironment &environment) noexcept;
// Neutro é o padrão do contrato: cena antiga sem gradação casa com ele.
EnvironmentProfile environmentLookProfile(EnvironmentLook look);

} // namespace ae::resources

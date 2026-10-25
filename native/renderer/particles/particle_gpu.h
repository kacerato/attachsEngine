#pragma once
// Contrato CPU↔GPU do passe de partículas (rhi/shaders/particle.{vert,frag}).
// A ordem e o tamanho espelham o bloco `push_constant` dos dois shaders.
#include "core/base.h"
#include "renderer/particles/particle_simulation.h"
#include "renderer/particles/particle_textures.h"

#include <cstring>

namespace ae::renderer::particles {

inline constexpr u32 kParticleFlagStretched = 1u << 0;
inline constexpr u32 kParticleFlagAdditive = 1u << 1;
inline constexpr u32 kParticleFlagSoft = 1u << 2;
// PK-10a: a imagem vem do Material de projeto (slot bindless em stretch.w).
inline constexpr u32 kParticleFlagProjectTexture = 1u << 3;
// G6a: a instância traz orientação 3D (quatérnio) e altura própria; os bits 5-6
// dizem a base do alinhamento (scene::ParticleRenderAlignment); malha desenha a
// geometria do lote em vez do quadrado.
inline constexpr u32 kParticleFlagOriented = 1u << 4;
inline constexpr u32 kParticleAlignmentShift = 5;
inline constexpr u32 kParticleFlagMesh = 1u << 7;
inline constexpr u32 kParticleFlagLineUv = 1u << 8;

struct ParticlePushConstants {
  float tint[4]{};     // rgb = tinta × emissão; w = tempo em segundos
  float scroll[4]{};   // rolagem da principal (xy) e do ruído (zw), por segundo
  float noise[4]{};    // escala, recorte, distorção, distância suave (m)
  float glow[4]{};     // brilho central, concentração, tamanho máximo na tela, escala do comprimento
  float stretch[4]{};  // escala da velocidade, 1/largura e 1/altura do alvo, bits do slot da textura de projeto
  u32 layers[4]{};     // bloco da principal, do ruído (~0 nenhum), da máscara (~0 nenhuma), flags
  float sheet[4]{1, 1, 0, 0};  // colunas e linhas da folha de textura
};
static_assert(sizeof(ParticlePushConstants) == 112);

// O que o passe fez no último quadro: entra no perfil do editor e na bancada.
struct ParticleRenderStats {
  u32 systems = 0, alive = 0, budget = 0, drawn = 0, batches = 0;
  float simulationMs = 0, buildMs = 0, gpuMs = 0;
  bool softAvailable = false;
};

// `projectSlot`: slot bindless da textura do Material de projeto, ou ~0 quando
// o aparelho não tem bindless ou o lote usa a textura própria.
inline ParticlePushConstants particlePushConstants(const ParticleMaterialParameters &m, float time, float targetWidth,
                                                   float targetHeight, bool softAvailable, u32 projectSlot = ~0u) {
  ParticlePushConstants p;
  for (u32 k = 0; k < 3; ++k) p.tint[k] = m.tint[k];
  p.tint[3] = time;
  p.scroll[0] = m.scroll[0]; p.scroll[1] = m.scroll[1]; p.scroll[2] = m.noiseScroll[0]; p.scroll[3] = m.noiseScroll[1];
  p.noise[0] = m.noiseScale; p.noise[1] = m.noiseStrength; p.noise[2] = m.noiseDistortion; p.noise[3] = m.softDistance;
  p.glow[0] = m.centerGlow; p.glow[1] = m.centerGlowPower; p.glow[2] = m.maxScreenSize; p.glow[3] = m.stretchLengthScale;
  p.stretch[0] = m.stretchVelocityScale;
  p.stretch[1] = targetWidth > 0 ? 1.0f / targetWidth : 0;
  p.stretch[2] = targetHeight > 0 ? 1.0f / targetHeight : 0;
  p.layers[0] = particleTextureLayer(static_cast<scene::ParticleTexture>(m.texture));
  p.layers[1] = particleNoiseLayer(static_cast<scene::ParticleNoise>(m.noise));
  p.layers[2] = particleMaskLayer(static_cast<scene::ParticleMask>(m.mask));
  p.layers[3] = (m.renderMode == static_cast<u32>(scene::ParticleRenderMode::StretchedBillboard) ? kParticleFlagStretched : 0u) |
                (m.blend == static_cast<u32>(scene::ParticleBlend::Additive) ? kParticleFlagAdditive : 0u) |
                (softAvailable && m.softDistance > 0 ? kParticleFlagSoft : 0u) |
                (projectSlot != ~0u ? kParticleFlagProjectTexture : 0u) |
                (m.oriented ? kParticleFlagOriented : 0u) | ((m.alignment & 3u) << kParticleAlignmentShift) |
                (m.renderMode == static_cast<u32>(scene::ParticleRenderMode::Mesh) ? kParticleFlagMesh : 0u) |
                (m.lineUv ? kParticleFlagLineUv : 0u);
  if (projectSlot != ~0u) std::memcpy(&p.stretch[3], &projectSlot, sizeof projectSlot);
  p.sheet[0] = m.sheetTiles[0];
  p.sheet[1] = m.sheetTiles[1];
  return p;
}

} // namespace ae::renderer::particles

#pragma once
// Texturas de partícula próprias da engine (G5 P1).
//
// A licença da Asset Store não permite redistribuir as texturas do RPG VFX
// Bundle com a ferramenta (plano §11). Então a Astra gera as suas: oito formas
// (ponto suave, brilho, chama, fumaça, faísca, anel, estrela, quadrado), três
// ruídos que repetem nas bordas (nuvens, células, faixas) e três máscaras. Tudo
// sai de matemática determinística, sem arquivo: o mesmo binário produz os
// mesmos bytes em qualquer aparelho, e o teste de host confere isso.
//
// Disposição: um atlas RGBA8 de 4x4 blocos (o RHI ainda não cria textura em
// array). Cada bloco tem uma borda de `kParticleTileGutter` texels: nas formas
// ela é transparente; nos ruídos ela repete o conteúdo do lado oposto, então a
// rolagem de UV com `fract` no shader não mostra costura. Mips só até o nível
// em que a borda ainda tem um texel.
#include "core/base.h"
#include "scene/particle_system.h"

#include <vector>

namespace ae::renderer::particles {

inline constexpr u32 kParticleTileSize = 128;
inline constexpr u32 kParticleTileGutter = 4;
inline constexpr u32 kParticleAtlasTiles = 4;  // por lado
inline constexpr u32 kParticleAtlasSize = kParticleTileSize * kParticleAtlasTiles;
inline constexpr u32 kParticleAtlasLevels = 3;
inline constexpr u32 kParticleTextureLayers = scene::kParticleTextureCount + scene::kParticleNoiseCount + scene::kParticleMaskCount;
static_assert(kParticleTextureLayers <= kParticleAtlasTiles * kParticleAtlasTiles);

// Camada da forma principal, do ruído (Nenhum = sem camada) e da máscara.
constexpr u32 particleTextureLayer(scene::ParticleTexture texture) { return static_cast<u32>(texture); }
constexpr u32 particleNoiseLayer(scene::ParticleNoise noise) {
  return noise == scene::ParticleNoise::None ? ~0u : scene::kParticleTextureCount + static_cast<u32>(noise) - 1;
}
constexpr u32 particleMaskLayer(scene::ParticleMask mask) {
  return mask == scene::ParticleMask::None ? ~0u
                                           : scene::kParticleTextureCount + scene::kParticleNoiseCount + static_cast<u32>(mask) - 1;
}

struct ParticleTextureAtlas {
  u32 tileSize = 0, size = 0, levels = 0, layers = 0;
  // Níveis em sequência, cada um com o atlas inteiro em RGBA8.
  std::vector<u8> texels;
  std::vector<usize> levelOffsets;
  u32 levelSize(u32 level) const { return size >> level ? size >> level : 1u; }
  // Texel do nível 0 dentro da área útil (sem a borda) da camada; u,v em [0,1).
  const u8 *texel(u32 layer, float u, float v) const;
};

// Gera as camadas e os mips. Determinístico; ~1,4 MB com mips.
ParticleTextureAtlas generateParticleTextures(u32 tileSize = kParticleTileSize);

} // namespace ae::renderer::particles

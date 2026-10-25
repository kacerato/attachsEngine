#include "renderer/particles/particle_textures.h"

#include <algorithm>
#include <cmath>

namespace ae::renderer::particles {
namespace {
constexpr float kPi = 3.14159265358979323846f;

u32 hash2(u32 x, u32 y, u32 seed) {
  u32 h = x * 0x8da6b343u ^ y * 0xd8163841u ^ seed * 0xcb1ab31fu;
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  return h;
}
float lattice(i32 x, i32 y, u32 period, u32 seed) {
  const u32 px = static_cast<u32>((x % static_cast<i32>(period) + static_cast<i32>(period)) % static_cast<i32>(period));
  const u32 py = static_cast<u32>((y % static_cast<i32>(period) + static_cast<i32>(period)) % static_cast<i32>(period));
  return static_cast<float>(hash2(px, py, seed) >> 8) * (1.0f / 16777216.0f);
}
float smooth(float t) { return t * t * (3 - 2 * t); }
// Ruído de valor periódico: repete a cada `period` células, então a textura
// encaixa nas bordas e a rolagem de UV não mostra costura.
float valueNoise(float x, float y, u32 period, u32 seed) {
  const float fx = std::floor(x), fy = std::floor(y);
  const auto ix = static_cast<i32>(fx), iy = static_cast<i32>(fy);
  const float tx = smooth(x - fx), ty = smooth(y - fy);
  const float a = lattice(ix, iy, period, seed), b = lattice(ix + 1, iy, period, seed);
  const float c = lattice(ix, iy + 1, period, seed), d = lattice(ix + 1, iy + 1, period, seed);
  return (a + (b - a) * tx) * (1 - ty) + (c + (d - c) * tx) * ty;
}
float fbm(float u, float v, u32 basePeriod, u32 octaves, u32 seed, float stretchX = 1) {
  float sum = 0, amplitude = 0.5f, norm = 0;
  u32 period = basePeriod;
  for (u32 o = 0; o < octaves; ++o) {
    const float px = static_cast<float>(period) * stretchX;
    sum += amplitude * valueNoise(u * px, v * static_cast<float>(period), static_cast<u32>(px), seed + o * 7919u);
    norm += amplitude;
    amplitude *= 0.5f;
    period *= 2;
  }
  return sum / norm;
}
// Worley F1 periódico (distância à semente mais próxima numa grade toroidal).
float cells(float u, float v, u32 period, u32 seed) {
  const float x = u * static_cast<float>(period), y = v * static_cast<float>(period);
  const auto ix = static_cast<i32>(std::floor(x)), iy = static_cast<i32>(std::floor(y));
  float best = 4;
  for (i32 dy = -1; dy <= 1; ++dy)
    for (i32 dx = -1; dx <= 1; ++dx) {
      const i32 cx = ix + dx, cy = iy + dy;
      const float sx = static_cast<float>(cx) + lattice(cx, cy, period, seed);
      const float sy = static_cast<float>(cy) + lattice(cx, cy, period, seed ^ 0x51f2u);
      best = std::min(best, std::hypot(x - sx, y - sy));
    }
  return std::clamp(best, 0.0f, 1.0f);
}
u8 byte(float value) { return static_cast<u8>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)); }

// Valor de alfa de cada forma em (x, y) centrado, de -1 a 1; y para cima.
float shapeAlpha(scene::ParticleTexture texture, float x, float y, float u, float v) {
  const float r = std::sqrt(x * x + y * y);
  switch (texture) {
  case scene::ParticleTexture::SoftDot: {
    const float t = std::clamp(1 - r * r, 0.0f, 1.0f);
    return t * t;
  }
  case scene::ParticleTexture::Glow:
    return std::clamp(std::exp(-r * r * 9.0f) + 0.35f * std::exp(-r * 3.2f) * std::clamp(1 - r, 0.0f, 1.0f), 0.0f, 1.0f);
  case scene::ParticleTexture::Flame: {
    // Língua de fogo: larga embaixo, ponta em cima, borda recortada por ruído
    // que sobe; o miolo é denso para o brilho central encontrar.
    const float height = (y + 1) * 0.5f;  // 0 na base, 1 na ponta
    const float width = 0.62f * std::pow(std::max(0.0f, 1 - height), 0.75f) * (0.55f + 0.45f * std::sin(std::min(1.0f, height * 3.2f) * kPi * 0.5f));
    const float turbulence = fbm(u, v * 0.7f + 0.13f, 4, 4, 0x6f1au) - 0.5f;
    const float edge = std::abs(x + turbulence * 0.35f * height) / std::max(width, 1e-3f);
    const float body = std::clamp(1 - edge, 0.0f, 1.0f);
    const float base = std::clamp((y + 1) * 3.0f, 0.0f, 1.0f);
    return std::clamp(std::pow(body, 0.8f) * base * (0.75f + 0.5f * (turbulence + 0.5f)), 0.0f, 1.0f);
  }
  case scene::ParticleTexture::Smoke: {
    const float puff = fbm(u, v, 3, 5, 0x2c7du);
    const float falloff = std::clamp(1 - r, 0.0f, 1.0f);
    return std::clamp((puff * 1.6f - 0.35f) * falloff * falloff * 1.8f, 0.0f, 1.0f);
  }
  case scene::ParticleTexture::Spark: {
    // Traço vertical: o billboard esticado alonga o eixo Y na velocidade.
    const float core = std::exp(-x * x * 90.0f);
    const float halo = 0.35f * std::exp(-x * x * 14.0f);
    const float along = std::clamp(1 - std::pow(std::abs(y), 2.5f), 0.0f, 1.0f);
    return std::clamp((core + halo) * along, 0.0f, 1.0f);
  }
  case scene::ParticleTexture::Ring: {
    const float d = (r - 0.72f) / 0.09f;
    return std::clamp(std::exp(-d * d) + 0.15f * std::exp(-((r - 0.72f) / 0.3f) * ((r - 0.72f) / 0.3f)) * std::clamp(1 - r, 0.0f, 1.0f), 0.0f, 1.0f);
  }
  case scene::ParticleTexture::Star: {
    const float rays = std::exp(-std::abs(x) * 26.0f) * std::clamp(1 - std::abs(y), 0.0f, 1.0f) +
                       std::exp(-std::abs(y) * 26.0f) * std::clamp(1 - std::abs(x), 0.0f, 1.0f);
    return std::clamp(rays + std::exp(-r * r * 22.0f), 0.0f, 1.0f);
  }
  case scene::ParticleTexture::Square: {
    const float d = std::max(std::abs(x), std::abs(y));
    return std::clamp((1 - d) * 64.0f, 0.0f, 1.0f);
  }
  }
  return 0;
}
} // namespace

const u8 *ParticleTextureAtlas::texel(u32 layer, float u, float v) const {
  const u32 gutter = tileSize * kParticleTileGutter / kParticleTileSize, content = tileSize - 2 * gutter;
  const u32 tx = (layer % kParticleAtlasTiles) * tileSize + gutter + std::min(content - 1, static_cast<u32>(u * static_cast<float>(content)));
  const u32 ty = (layer / kParticleAtlasTiles) * tileSize + gutter + std::min(content - 1, static_cast<u32>(v * static_cast<float>(content)));
  return texels.data() + (static_cast<usize>(ty) * size + tx) * 4;
}

ParticleTextureAtlas generateParticleTextures(u32 tileSize) {
  ParticleTextureAtlas result;
  result.tileSize = std::max(16u, tileSize);
  result.size = result.tileSize * kParticleAtlasTiles;
  result.layers = kParticleTextureLayers;
  result.levels = kParticleAtlasLevels;
  usize total = 0;
  for (u32 level = 0; level < result.levels; ++level) {
    result.levelOffsets.push_back(total);
    total += static_cast<usize>(result.levelSize(level)) * result.levelSize(level) * 4;
  }
  result.texels.assign(total, 0);
  const u32 tile = result.tileSize, gutter = tile * kParticleTileGutter / kParticleTileSize;
  const float content = static_cast<float>(tile - 2 * gutter);
  for (u32 layer = 0; layer < result.layers; ++layer) {
    const u32 ox = (layer % kParticleAtlasTiles) * tile, oy = (layer / kParticleAtlasTiles) * tile;
    for (u32 y = 0; y < tile; ++y)
      for (u32 x = 0; x < tile; ++x) {
        // Coordenada da área útil; a borda cai fora de [0,1) e o ruído repete.
        const float u = (static_cast<float>(x) - static_cast<float>(gutter) + 0.5f) / content;
        const float v = (static_cast<float>(y) - static_cast<float>(gutter) + 0.5f) / content;
        const float wu = u - std::floor(u), wv = v - std::floor(v);
        const float cx = u * 2 - 1, cy = 1 - v * 2;  // linha 0 é o topo
        const float r = std::sqrt(cx * cx + cy * cy);
        u8 *p = result.texels.data() + (static_cast<usize>(oy + y) * result.size + ox + x) * 4;
        if (layer < scene::kParticleTextureCount) {
          p[0] = p[1] = p[2] = 255;
          const bool inside = u >= 0 && u < 1 && v >= 0 && v < 1;
          p[3] = inside ? byte(shapeAlpha(static_cast<scene::ParticleTexture>(layer), cx, cy, wu, wv)) : 0;
        } else if (layer < scene::kParticleTextureCount + scene::kParticleNoiseCount) {
          const u32 k = layer - scene::kParticleTextureCount;
          const float value = k == 0 ? fbm(wu, wv, 4, 5, 0x91e1u) : k == 1 ? 1 - cells(wu, wv, 6, 0x3a77u)
                                                                         : std::pow(fbm(wu, wv, 2, 4, 0x4b2du, 6.0f), 1.5f) * 1.3f;
          p[0] = p[1] = p[2] = p[3] = byte(value);
        } else {
          const u32 k = layer - scene::kParticleTextureCount - scene::kParticleNoiseCount;
          // Máscaras: radial, desvanece para cima (1 na base, 0 no topo), anel.
          const float ring = (r - 0.65f) / 0.2f;
          const float value = k == 0 ? std::clamp(1 - r, 0.0f, 1.0f) : k == 1 ? std::clamp((1 - cy) * 0.5f, 0.0f, 1.0f) : std::exp(-ring * ring);
          p[0] = p[1] = p[2] = p[3] = byte(value);
        }
      }
  }
  // Mips por caixa 2x2: blocos alinhados, então um texel nunca mistura dois blocos.
  for (u32 level = 1; level < result.levels; ++level) {
    const u32 dst = result.levelSize(level), src = result.levelSize(level - 1);
    const u8 *from = result.texels.data() + result.levelOffsets[level - 1];
    u8 *to = result.texels.data() + result.levelOffsets[level];
    for (u32 y = 0; y < dst; ++y)
      for (u32 x = 0; x < dst; ++x)
        for (u32 c = 0; c < 4; ++c) {
          u32 sum = 0;
          for (u32 dy = 0; dy < 2; ++dy)
            for (u32 dx = 0; dx < 2; ++dx) sum += from[(static_cast<usize>(y * 2 + dy) * src + x * 2 + dx) * 4 + c];
          to[(static_cast<usize>(y) * dst + x) * 4 + c] = static_cast<u8>((sum + 2) / 4);
        }
  }
  return result;
}

} // namespace ae::renderer::particles

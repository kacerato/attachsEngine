#include "renderer/cloud_noise.h"

#include <algorithm>
#include <cmath>

namespace ae::renderer {
namespace {
u32 hash3(u32 x, u32 y, u32 z, u32 seed) noexcept {
  u32 h = x * 0x8da6b343u ^ y * 0xd8163841u ^ z * 0xcb1ab31fu ^ seed * 0x9e3779b9u;
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  return h;
}
float unit(u32 h) noexcept { return static_cast<float>(h >> 8) * (1.0f / 16777216.0f); }
u32 wrap(int v, u32 period) noexcept {
  const int p = static_cast<int>(period);
  return static_cast<u32>(((v % p) + p) % p);
}
float fade(float t) noexcept { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
// Perlin 3D repetível com período `cells` em cada eixo; ~[-1,1].
float tileablePerlin(float x, float y, float z, u32 cells, u32 seed) noexcept {
  const float px = x * cells, py = y * cells, pz = z * cells;
  const int x0 = static_cast<int>(std::floor(px)), y0 = static_cast<int>(std::floor(py)), z0 = static_cast<int>(std::floor(pz));
  const float fx = px - x0, fy = py - y0, fz = pz - z0;
  float corner[8];
  for (int c = 0; c < 8; ++c) {
    const int dx = c & 1, dy = (c >> 1) & 1, dz = c >> 2;
    const u32 h = hash3(wrap(x0 + dx, cells), wrap(y0 + dy, cells), wrap(z0 + dz, cells), seed);
    // 12 gradientes das arestas do cubo, como o Perlin melhorado.
    static constexpr float g[12][3]{{1,1,0},{-1,1,0},{1,-1,0},{-1,-1,0},{1,0,1},{-1,0,1},
                                     {1,0,-1},{-1,0,-1},{0,1,1},{0,-1,1},{0,1,-1},{0,-1,-1}};
    const float *v = g[h % 12u];
    corner[c] = v[0] * (fx - dx) + v[1] * (fy - dy) + v[2] * (fz - dz);
  }
  const float u = fade(fx), v = fade(fy), w = fade(fz);
  const auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
  const float x00 = lerp(corner[0], corner[1], u), x10 = lerp(corner[2], corner[3], u);
  const float x01 = lerp(corner[4], corner[5], u), x11 = lerp(corner[6], corner[7], u);
  return lerp(lerp(x00, x10, v), lerp(x01, x11, v), w);
}
float perlinFbm(float x, float y, float z, u32 cells, u32 octaves, u32 seed) noexcept {
  float sum = 0.0f, amplitude = 1.0f, norm = 0.0f;
  for (u32 o = 0; o < octaves; ++o) {
    sum += amplitude * tileablePerlin(x, y, z, cells << o, seed + o * 131u);
    norm += amplitude; amplitude *= 0.5f;
  }
  return sum / norm;
}
float remap(float v, float lo, float hi, float newLo, float newHi) noexcept {
  return newLo + (v - lo) / std::max(hi - lo, 1e-6f) * (newHi - newLo);
}
u8 unorm(float v) noexcept { return static_cast<u8>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); }
} // namespace

float tileableWorley(float x, float y, float z, u32 cells, u32 seed) noexcept {
  const float px = x * cells, py = y * cells, pz = z * cells;
  const int cx = static_cast<int>(std::floor(px)), cy = static_cast<int>(std::floor(py)), cz = static_cast<int>(std::floor(pz));
  float best = 1e9f;
  for (int dz = -1; dz <= 1; ++dz) for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
    const int nx = cx + dx, ny = cy + dy, nz = cz + dz;
    const u32 h = hash3(wrap(nx, cells), wrap(ny, cells), wrap(nz, cells), seed);
    const float fx = nx + unit(h), fy = ny + unit(h * 747796405u + 1u), fz = nz + unit(h * 2891336453u + 7u);
    const float ex = fx - px, ey = fy - py, ez = fz - pz;
    best = std::min(best, ex * ex + ey * ey + ez * ez);
  }
  return std::clamp(1.0f - std::sqrt(best), 0.0f, 1.0f);
}

bool buildCloudShapeNoise(u32 size, u32 seed, std::vector<u8> &output) {
  if (size < 8 || size > 128 || (size & (size - 1))) return false;
  std::vector<u8> result(static_cast<usize>(size) * size * size * 4);
  std::vector<float> perlinWorley(static_cast<usize>(size) * size * size);
  const float inverse = 1.0f / static_cast<float>(size);
  for (u32 z = 0; z < size; ++z) for (u32 y = 0; y < size; ++y) for (u32 x = 0; x < size; ++x) {
    const float u = (x + 0.5f) * inverse, v = (y + 0.5f) * inverse, w = (z + 0.5f) * inverse;
    // Worley em 3 frequências (4, 8, 16 células) como fBm de forma.
    const float w1 = tileableWorley(u, v, w, 4, seed + 11u);
    const float w2 = tileableWorley(u, v, w, 8, seed + 23u);
    const float w3 = tileableWorley(u, v, w, 16, seed + 37u);
    const float worleyFbm = w1 * 0.625f + w2 * 0.25f + w3 * 0.125f;
    // Perlin-Worley (Schneider): o Perlin dilatado pelo Worley de baixa
    // frequência; o canal é esticado para 0..1 no fim, sobre o volume inteiro.
    const float perlin = std::clamp(perlinFbm(u, v, w, 4, 4, seed) * 0.5f + 0.5f, 0.0f, 1.0f);
    const usize index = ((static_cast<usize>(z) * size + y) * size + x) * 4;
    perlinWorley[index / 4] = remap(perlin, 0.0f, 1.0f, w1, 1.0f) - (1.0f - worleyFbm) * 0.5f;
    result[index + 1] = unorm(w1);
    result[index + 2] = unorm(w2);
    result[index + 3] = unorm(w3);
  }
  const auto [low, high] = std::minmax_element(perlinWorley.begin(), perlinWorley.end());
  const float lo = *low, span = std::max(*high - *low, 1e-6f);
  for (usize i = 0; i < perlinWorley.size(); ++i) result[i * 4] = unorm((perlinWorley[i] - lo) / span);
  output = std::move(result);
  return true;
}

bool buildCloudErosionNoise(u32 size, u32 seed, std::vector<u8> &output) {
  if (size < 8 || size > 128 || (size & (size - 1))) return false;
  std::vector<u8> result(static_cast<usize>(size) * size * size * 4);
  const float inverse = 1.0f / static_cast<float>(size);
  for (u32 z = 0; z < size; ++z) for (u32 y = 0; y < size; ++y) for (u32 x = 0; x < size; ++x) {
    const float u = (x + 0.5f) * inverse, v = (y + 0.5f) * inverse, w = (z + 0.5f) * inverse;
    const usize index = ((static_cast<usize>(z) * size + y) * size + x) * 4;
    result[index + 0] = unorm(tileableWorley(u, v, w, 2, seed + 5u));
    result[index + 1] = unorm(tileableWorley(u, v, w, 4, seed + 9u));
    result[index + 2] = unorm(tileableWorley(u, v, w, 8, seed + 17u));
    result[index + 3] = 255;
  }
  output = std::move(result);
  return true;
}

} // namespace ae::renderer

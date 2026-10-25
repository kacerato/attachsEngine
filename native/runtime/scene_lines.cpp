#include "runtime/scene_lines.h"

#include "runtime/transform_math.h"
#include "scene/vfx_behaviors.h"

#include <algorithm>
#include <cmath>

namespace ae::runtime {
namespace {
float lerp(float a, float b, float t) { return a + (b - a) * t; }
}

u32 appendLineRenderers(const SceneGraph &graph, const float (&camera)[3], renderer::particles::ParticleFrame &out,
                        const ParticleMaterialResolver *materials) {
  using namespace renderer::particles;
  std::vector<ObjectId> objects;
  graph.collectSubtree(graph.root(), objects);
  u32 lines = 0;
  for (const auto id : objects) {
    const auto *object = graph.find(id);
    if (!object || !graph.activeInHierarchy(id)) continue;
    const auto *value = object->components.find(scene::LineRenderer::descriptor);
    if (!value) continue;
    const auto &line = scene::lineRenderer(*value);
    if (!line.enabled || !line.valid()) continue;
    float world[16];
    if (!line.worldSpace && !worldMatrix(graph, id, world)) continue;
    // Pontos em mundo, na ordem; Fechar repete o primeiro no fim.
    const u32 count = line.usedPoints();
    std::vector<std::array<float, 3>> points;
    for (u32 i = 0; i < count + (line.loop ? 1u : 0u); ++i) {
      const float *p = line.points + (i % count) * 3;
      std::array<float, 3> w{p[0], p[1], p[2]};
      if (!line.worldSpace)
        for (u32 k = 0; k < 3; ++k) w[k] = world[12 + k] + world[k] * p[0] + world[4 + k] * p[1] + world[8 + k] * p[2];
      points.push_back(w);
    }
    // Comprimento acumulado: largura e cor variam pelo comprimento, como na Unity.
    std::vector<float> along(points.size(), 0.0f);
    for (usize i = 1; i < points.size(); ++i) {
      const float d[3]{points[i][0] - points[i - 1][0], points[i][1] - points[i - 1][1], points[i][2] - points[i - 1][2]};
      along[i] = along[i - 1] + std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    }
    const float total = along.back();
    if (!(total > 1e-5f)) continue;
    ParticleBatch batch;
    batch.firstInstance = static_cast<u32>(out.instances.size());
    u32 segments = 0;
    for (usize i = 1; i < points.size() && segments < kMaximumLineSegments; ++i) {
      const float length = along[i] - along[i - 1];
      if (!(length > 1e-6f)) continue;
      // Repetir: um trecho por repetição; o último fica com o que sobra.
      const u32 pieces = line.tileLength > 0 ? std::max(1u, static_cast<u32>(std::ceil(length / line.tileLength - 1e-4f))) : 1u;
      for (u32 piece = 0; piece < pieces && segments < kMaximumLineSegments; ++piece) {
        const float t0 = static_cast<float>(piece) / static_cast<float>(pieces);
        const float t1 = line.tileLength > 0 ? std::min(1.0f, (static_cast<float>(piece) + 1) * line.tileLength / length)
                                             : static_cast<float>(piece + 1) / static_cast<float>(pieces);
        const float a = line.tileLength > 0 ? std::min(1.0f, static_cast<float>(piece) * line.tileLength / length) : t0;
        float from[3], to[3];
        for (u32 k = 0; k < 3; ++k) {
          from[k] = lerp(points[i - 1][k], points[i][k], a);
          to[k] = lerp(points[i - 1][k], points[i][k], t1);
        }
        const float middle = (along[i - 1] + length * (a + t1) * .5f) / total;
        ParticleInstance instance;
        std::copy(to, to + 3, instance.position);
        for (u32 k = 0; k < 3; ++k) instance.velocity[k] = to[k] - from[k];
        instance.size = std::max(0.0f, lerp(line.widthStart, line.widthEnd, middle));
        instance.sizeYZ[0] = instance.sizeYZ[1] = instance.size;
        for (u32 k = 0; k < 4; ++k) instance.color[k] = floatToHalf(std::max(0.0f, lerp(line.colorStart[k], line.colorEnd[k], middle)));
        instance.random = static_cast<u32>(id) * 2654435761u + segments;
        if (instance.size > 0) { out.instances.push_back(instance); ++segments; }
      }
    }
    batch.instanceCount = static_cast<u32>(out.instances.size()) - batch.firstInstance;
    if (!batch.instanceCount) continue;
    // Material: a faixa esticada do comprimento exato do trecho, com a imagem
    // correndo ao longo da linha (U pelo comprimento, como a Unity).
    auto &m = batch.material;
    m.renderMode = static_cast<u32>(scene::ParticleRenderMode::StretchedBillboard);
    m.texture = static_cast<u32>(scene::ParticleTexture::Square);
    m.stretchVelocityScale = 1;
    m.stretchLengthScale = 0;
    m.maxScreenSize = 0;
    m.lineUv = true;
    m.scroll[0] = line.scrollX;
    m.scroll[1] = line.scrollY;
    if (line.material.valid() && materials && *materials) {
      ParticleMaterialLook look;
      if ((*materials)(line.material, look)) {
        m.projectTexture = look.texture;
        std::copy(look.color, look.color + 3, m.tint);
        m.blend = static_cast<u32>(look.additive ? scene::ParticleBlend::Additive : scene::ParticleBlend::AlphaBlend);
      }
    }
    for (auto &channel : m.tint) channel *= line.emission;
    const usize mid = points.size() / 2;
    const float d[3]{points[mid][0] - camera[0], points[mid][1] - camera[1], points[mid][2] - camera[2]};
    batch.sortDistance = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    out.batches.push_back(batch);
    out.alive += batch.instanceCount;
    ++lines;
  }
  if (lines)
    std::stable_sort(out.batches.begin(), out.batches.end(), [](const auto &a, const auto &b) { return a.sortDistance > b.sortDistance; });
  return lines;
}

} // namespace ae::runtime

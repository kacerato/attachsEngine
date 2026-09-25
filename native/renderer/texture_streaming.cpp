#include "renderer/texture_streaming.h"
#include "renderer/mesh_report.h"
#include <algorithm>
#include <cmath>
#include <queue>

namespace ae::renderer {
u64 textureStreamingChainBytes(const TextureStreamingTexture &texture, u32 baseMip) {
  if (!texture.width || !texture.height || baseMip >= texture.levels || !authoringTextureBlock(texture.format))
    return 0;
  u64 total = 0;
  u32 w = texture.width, h = texture.height;
  for (u32 level = 0; level < texture.levels; ++level) {
    if (level >= baseMip) total += authoringTextureLevelBytes(texture.format, w, h);
    w = w > 1 ? w / 2 : 1;
    h = h > 1 ? h / 2 : 1;
  }
  return total;
}

u32 textureStreamingFirstMip(const TextureStreamingTexture &texture, const TextureStreamingSettings &settings) {
  return texture.levels ? std::min(settings.minimumMip, texture.levels - 1) : 0;
}

u32 textureStreamingLastMip(const TextureStreamingTexture &texture, const TextureStreamingSettings &settings) {
  if (!texture.levels) return 0;
  const u32 first = textureStreamingFirstMip(texture, settings);
  if (!texture.streamable) return first;
  return static_cast<u32>(std::min<u64>(texture.levels - 1, u64(first) + settings.maxLevelReduction));
}

u32 textureStreamingScreenMip(const TextureStreamingTexture &texture, float metersPerUv, float distance,
                              const TextureStreamingView &view) {
  if (!texture.levels) return 0;
  // Sem métrica de UV não há como saber a densidade: detalhe máximo.
  if (!(metersPerUv > 0) || !std::isfinite(metersPerUv)) return 0;
  float pixelsPerMeter = view.orthographicPixelsPerMeter;
  if (!(pixelsPerMeter > 0)) {
    if (!(view.pixelsPerMeterAtUnitDistance > 0)) return 0;
    // Dentro da esfera o uso pode encostar na câmera: detalhe máximo.
    if (!(distance > 1e-3f)) return 0;
    pixelsPerMeter = view.pixelsPerMeterAtUnitDistance / distance;
  }
  const float texelsPerMeter = static_cast<float>(std::max(texture.width, texture.height)) / metersPerUv;
  const float texelsPerPixel = texelsPerMeter / pixelsPerMeter;
  if (!(texelsPerPixel > 1.0f) || !std::isfinite(texelsPerPixel)) return 0;
  const u32 mip = static_cast<u32>(std::floor(std::log2(texelsPerPixel)));
  return std::min(mip, texture.levels - 1);
}

bool planTextureStreaming(std::span<const TextureStreamingTexture> textures, std::span<const TextureStreamingUse> uses,
                          std::span<const TextureStreamingView> views, const TextureStreamingSettings &settings,
                          std::span<const u32> loadedMip, TextureStreamingPlan &out) {
  if (loadedMip.size() != textures.size()) return false;
  for (const auto &use : uses)
    if (use.texture >= textures.size()) return false;
  TextureStreamingPlan plan;
  const usize count = textures.size();
  plan.calculatedMip.assign(count, 0);
  plan.targetMip.assign(count, 0);
  // Pela tela: o uso mais exigente (menor mip) de todas as vistas. Sem uso a
  // textura não é vista e fica no fim da faixa, como na Unity.
  std::vector<u32> seen(count, ~0u);
  for (const auto &use : uses) {
    const auto &texture = textures[use.texture];
    for (const auto &view : views) {
      float distance = 0;
      for (u32 axis = 0; axis < 3; ++axis) {
        const float d = use.center[axis] - view.position[axis];
        distance += d * d;
      }
      distance = std::max(0.0f, std::sqrt(distance) - std::max(0.0f, use.radius));
      seen[use.texture] = std::min(seen[use.texture], textureStreamingScreenMip(texture, use.metersPerUv, distance, view));
    }
  }
  u64 streamingTarget = 0;
  for (usize t = 0; t < count; ++t) {
    const auto &texture = textures[t];
    const u32 first = textureStreamingFirstMip(texture, settings), last = textureStreamingLastMip(texture, settings);
    plan.totalBytes += textureStreamingChainBytes(texture, 0);
    if (loadedMip[t] != TextureStreamingNotLoaded) plan.currentBytes += textureStreamingChainBytes(texture, loadedMip[t]);
    u32 calculated = first;
    if (texture.streamable) {
      if (texture.requestedMip != TextureStreamingNoRequest)
        calculated = std::clamp(texture.requestedMip, first, texture.levels ? texture.levels - 1 : 0);
      else
        calculated = std::clamp(seen[t] == ~0u ? last : seen[t], first, last);
      ++plan.streamingTextures;
    }
    plan.calculatedMip[t] = plan.targetMip[t] = calculated;
    const u64 bytes = textureStreamingChainBytes(texture, calculated);
    plan.desiredBytes += bytes;
    if (texture.streamable) streamingTarget += bytes;
    else plan.nonStreamingBytes += bytes;
  }
  // Orçamento: tira um nível por vez de quem tem menor prioridade e, no empate,
  // a maior cadeia — até caber ou todas chegarem à redução máxima.
  const u64 available = settings.budgetBytes > plan.nonStreamingBytes ? settings.budgetBytes - plan.nonStreamingBytes : 0;
  struct Candidate {
    i32 priority;
    u64 bytes;
    usize texture;
  };
  const auto lessUrgent = [](const Candidate &a, const Candidate &b) {
    if (a.priority != b.priority) return a.priority > b.priority;
    if (a.bytes != b.bytes) return a.bytes < b.bytes;
    return a.texture > b.texture;
  };
  std::priority_queue<Candidate, std::vector<Candidate>, decltype(lessUrgent)> queue(lessUrgent);
  for (usize t = 0; t < count; ++t)
    if (textures[t].streamable && plan.targetMip[t] < textureStreamingLastMip(textures[t], settings))
      queue.push({textures[t].priority, textureStreamingChainBytes(textures[t], plan.targetMip[t]), t});
  std::vector<u8> reduced(count, 0);
  while (streamingTarget > available && !queue.empty()) {
    const auto top = queue.top();
    queue.pop();
    const auto &texture = textures[top.texture];
    u32 &target = plan.targetMip[top.texture];
    const u64 before = textureStreamingChainBytes(texture, target);
    ++target;
    const u64 after = textureStreamingChainBytes(texture, target);
    streamingTarget -= before - after;
    reduced[top.texture] = 1;
    if (target < textureStreamingLastMip(texture, settings)) queue.push({texture.priority, after, top.texture});
  }
  plan.overBudget = streamingTarget > available;
  for (usize t = 0; t < count; ++t) {
    plan.targetBytes += textureStreamingChainBytes(textures[t], plan.targetMip[t]);
    plan.budgetReducedTextures += reduced[t];
  }
  // Trocas deste quadro: primeiro as que descem (liberam memória), depois as
  // que sobem, por prioridade e pela distância até o alvo. O teto de bytes vale
  // a partir da segunda troca, para o streaming nunca ficar parado.
  std::vector<usize> drops, raises;
  for (usize t = 0; t < count; ++t) {
    if (loadedMip[t] == plan.targetMip[t]) continue;
    if (loadedMip[t] != TextureStreamingNotLoaded && plan.targetMip[t] > loadedMip[t]) drops.push_back(t);
    else raises.push_back(t);
  }
  std::stable_sort(raises.begin(), raises.end(), [&](usize a, usize b) {
    const bool newA = loadedMip[a] == TextureStreamingNotLoaded, newB = loadedMip[b] == TextureStreamingNotLoaded;
    if (newA != newB) return newA;
    if (textures[a].priority != textures[b].priority) return textures[a].priority > textures[b].priority;
    const u32 gapA = newA ? 0 : loadedMip[a] - plan.targetMip[a], gapB = newB ? 0 : loadedMip[b] - plan.targetMip[b];
    return gapA > gapB;
  });
  for (const auto *list : {&drops, &raises}) {
    for (const usize t : *list) {
      const u64 bytes = textureStreamingChainBytes(textures[t], plan.targetMip[t]);
      if (!plan.loads.empty() && plan.uploadBytes + bytes > settings.uploadBytesPerFrame) {
        ++plan.pendingLoads;
        continue;
      }
      plan.loads.push_back(static_cast<u32>(t));
      plan.uploadBytes += bytes;
    }
  }
  out = std::move(plan);
  return true;
}

float meshUvMetersPerUnit(std::span<const u8> vertices, std::span<const u32> indices, const MapDrawRecord &draw) {
  if (draw.indexCount < 3 || u64(draw.firstIndex) + draw.indexCount > indices.size()) return 0;
  const u64 vertexCount = vertices.size() / MapVertexStride;
  double surface = 0, uv = 0;
  for (u32 i = 0; i + 2 < draw.indexCount; i += 3) {
    MapVertexView corner[3];
    for (u32 k = 0; k < 3; ++k) {
      const u64 index = u64(draw.vertexOffset) + indices[draw.firstIndex + i + k];
      if (index >= vertexCount) return 0;
      corner[k] = readMapVertex(vertices, index);
    }
    double e1[3], e2[3];
    for (u32 a = 0; a < 3; ++a) {
      e1[a] = corner[1].position[a] - corner[0].position[a];
      e2[a] = corner[2].position[a] - corner[0].position[a];
    }
    const double cx = e1[1] * e2[2] - e1[2] * e2[1], cy = e1[2] * e2[0] - e1[0] * e2[2],
                 cz = e1[0] * e2[1] - e1[1] * e2[0];
    surface += .5 * std::sqrt(cx * cx + cy * cy + cz * cz);
    const double u1 = corner[1].uv0[0] - corner[0].uv0[0], v1 = corner[1].uv0[1] - corner[0].uv0[1];
    const double u2 = corner[2].uv0[0] - corner[0].uv0[0], v2 = corner[2].uv0[1] - corner[0].uv0[1];
    uv += .5 * std::abs(u1 * v2 - u2 * v1);
  }
  if (!(uv > 1e-12) || !(surface > 0)) return 0;
  const double metric = std::sqrt(surface / uv);
  return std::isfinite(metric) ? static_cast<float>(metric) : 0.0f;
}

float textureStreamingModelScale(const float model[16]) {
  const double det = double(model[0]) * (double(model[5]) * model[10] - double(model[9]) * model[6]) -
                     double(model[4]) * (double(model[1]) * model[10] - double(model[9]) * model[2]) +
                     double(model[8]) * (double(model[1]) * model[6] - double(model[5]) * model[2]);
  const double scale = std::cbrt(std::abs(det));
  return std::isfinite(scale) && scale > 0 ? static_cast<float>(scale) : 0.0f;
}
} // namespace ae::renderer

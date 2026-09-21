#include "renderer/shadow_atlas.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace ae::renderer {
namespace {

struct Vec3 {
  float x = 0, y = 0, z = 0;
};
Vec3 load(const float v[3]) { return {v[0], v[1], v[2]}; }
Vec3 sub(const Vec3 &a, const Vec3 &b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
float dot(const Vec3 &a, const Vec3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(const Vec3 &a, const Vec3 &b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
bool finite(const Vec3 &a) { return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z); }
bool normalize(Vec3 &a) {
  const float length = std::sqrt(dot(a, a));
  if (!std::isfinite(length) || length <= 1e-6f) return false;
  a = {a.x / length, a.y / length, a.z / length};
  return true;
}

u32 previousPowerOfTwo(u32 value) {
  u32 result = 1;
  while (result * 2u <= value) result *= 2u;
  return result;
}

// Direções das seis faces do cubo, na ordem +X, -X, +Y, -Y, +Z, -Z, com um
// "para cima" que não colapsa em nenhuma delas.
struct CubeFace {
  Vec3 forward, up;
};
constexpr CubeFace kCubeFaces[6]{
    {{1, 0, 0}, {0, 1, 0}},  {{-1, 0, 0}, {0, 1, 0}}, {{0, 1, 0}, {0, 0, -1}},
    {{0, -1, 0}, {0, 0, 1}}, {{0, 0, 1}, {0, 1, 0}},  {{0, 0, -1}, {0, 1, 0}}};

// Perspectiva com profundidade em [0,1] (convenção Vulkan), mão esquerda, já
// multiplicada pela vista da luz. Mesma convenção coluna-primeiro das cascatas.
void buildViewProjection(const Vec3 &eye, Vec3 forward, Vec3 up, float fovYRadians, float nearPlane,
                         float farPlane, float out[16]) {
  Vec3 right = cross(up, forward);
  if (!normalize(right)) {
    // Direção paralela ao "para cima" escolhido: troca a referência em vez de
    // devolver uma base degenerada.
    up = {0, 0, 1};
    right = cross(up, forward);
    normalize(right);
  }
  up = cross(forward, right);
  normalize(up);
  const float focal = 1.0f / std::tan(fovYRadians * .5f);
  const float depthScale = farPlane / (farPlane - nearPlane);
  const float depthOffset = -nearPlane * farPlane / (farPlane - nearPlane);
  const float translation[3]{-dot(right, eye), -dot(up, eye), -dot(forward, eye)};
  const float basis[3][3]{{right.x, right.y, right.z}, {up.x, up.y, up.z}, {forward.x, forward.y, forward.z}};
  for (u32 column = 0; column < 3; ++column) {
    out[column * 4 + 0] = basis[0][column] * focal;
    out[column * 4 + 1] = basis[1][column] * focal;
    out[column * 4 + 2] = basis[2][column] * depthScale;
    out[column * 4 + 3] = basis[2][column];
  }
  out[12] = translation[0] * focal;
  out[13] = translation[1] * focal;
  out[14] = translation[2] * depthScale + depthOffset;
  out[15] = translation[2];
}

// Alocador em quadtree: cada tile fica alinhado ao próprio tamanho, os maiores
// entram primeiro. Guarda os nós livres por nível, o que mantém a alocação em
// tempo constante sem varrer o atlas.
class BuddyAtlas final {
public:
  BuddyAtlas(u32 resolution, u32 reservedRows, u32 smallest) : smallest_(smallest) {
    // Os nós livres são a grade do maior bloco que cabe DEPOIS das linhas
    // reservadas, alinhado ao próprio tamanho — o sol e as luzes locais não
    // podem disputar o mesmo texel, e um bloco desalinhado quebraria a divisão
    // em quatro que o alocador faz depois.
    u32 block = resolution;
    while (block > smallest_ && reservedRows + block > resolution) block /= 2;
    if (block < smallest_ || reservedRows + block > resolution) return;
    const u32 start = ((reservedRows + block - 1) / block) * block;
    for (u32 y = start; y + block <= resolution; y += block)
      for (u32 x = 0; x + block <= resolution; x += block) free_[block].push_back({x, y});
  }

  bool allocate(u32 size, u32 &outX, u32 &outY) {
    if (size < smallest_) size = smallest_;
    if (split(size)) {
      auto &list = free_[size];
      outX = list.back().x;
      outY = list.back().y;
      list.pop_back();
      return true;
    }
    return false;
  }

private:
  struct Node {
    u32 x, y;
  };
  // Garante um nó livre do tamanho pedido, quebrando um maior se preciso.
  bool split(u32 size) {
    if (auto found = free_.find(size); found != free_.end() && !found->second.empty()) return true;
    if (size >= (1u << 30)) return false;
    if (!split(size * 2)) return false;
    auto &bigger = free_[size * 2];
    const Node node = bigger.back();
    bigger.pop_back();
    auto &list = free_[size];
    list.push_back({node.x, node.y});
    list.push_back({node.x + size, node.y});
    list.push_back({node.x, node.y + size});
    list.push_back({node.x + size, node.y + size});
    return true;
  }

  u32 smallest_;
  std::map<u32, std::vector<Node>> free_;
};

// Altura do alcance da luz na tela, em fração da altura do viewport. É a mesma
// grandeza que decide o nível de LOD, e pelo mesmo motivo: o que ocupa pouca
// tela não merece resolução.
float screenHeightFraction(const ShadowAtlasInput &input, const ShadowCaster &caster) {
  const Vec3 camera = load(input.cameraPosition);
  const Vec3 position = load(caster.position);
  const float distance = std::sqrt(dot(sub(position, camera), sub(position, camera)));
  const float halfHeight = std::tan(input.verticalFovRadians * .5f) * std::max(distance, 1e-3f);
  if (!(halfHeight > 0)) return 0;
  return std::min(1.0f, caster.range / halfHeight);
}

} // namespace

u32 shadowTileSizeFor(const ShadowAtlasInput &input, const ShadowCaster &caster) {
  const u32 floorSize = std::max(8u, previousPowerOfTwo(input.minimumTileSize));
  const u32 ceilingSize = std::max(floorSize, previousPowerOfTwo(input.maximumTileSize));
  switch (caster.resolution) {
  case ShadowResolution::Low: return std::clamp(256u, floorSize, ceilingSize);
  case ShadowResolution::Medium: return std::clamp(512u, floorSize, ceilingSize);
  case ShadowResolution::High: return std::clamp(1024u, floorSize, ceilingSize);
  case ShadowResolution::VeryHigh: return std::clamp(2048u, floorSize, ceilingSize);
  case ShadowResolution::FromQuality: break;
  }
  // Automática: a luz que cobre a tela inteira recebe o teto; cada metade de
  // tamanho na tela desce um nível de potência de dois.
  const float fraction = screenHeightFraction(input, caster);
  u32 size = ceilingSize;
  for (float threshold = .5f; threshold > .02f && size > floorSize; threshold *= .5f) {
    if (fraction >= threshold) break;
    size /= 2;
  }
  return std::clamp(size, floorSize, ceilingSize);
}

u32 buildShadowAtlas(const ShadowAtlasInput &input, std::span<const ShadowCaster> casters,
                     std::span<ShadowAtlasTile> out, ShadowAtlasReport &report) {
  report = {};
  if (out.empty() || casters.empty()) return 0;
  if (input.atlasResolution == 0 || previousPowerOfTwo(input.atlasResolution) != input.atlasResolution) return 0;
  if (input.reservedRows >= input.atlasResolution) return 0;
  if (!std::isfinite(input.verticalFovRadians) || input.verticalFovRadians <= 0) return 0;
  if (!std::isfinite(input.shadowDistance) || input.shadowDistance <= 0) return 0;
  if (!finite(load(input.cameraPosition))) return 0;

  // Ordem de atendimento: quem ocupa mais tela primeiro. Assim o orçamento
  // acaba na luz distante, não na que está na cara do jogador.
  struct Candidate {
    u32 index;
    float importance;
    u32 size;
  };
  std::vector<Candidate> queue;
  queue.reserve(casters.size());
  const Vec3 camera = load(input.cameraPosition);
  for (u32 index = 0; index < casters.size(); ++index) {
    const auto &caster = casters[index];
    if (!caster.enabled || caster.modality == LightModality::Directional) continue;
    if (!finite(load(caster.position)) || !std::isfinite(caster.range) || caster.range <= 0) continue;
    if (!std::isfinite(caster.nearPlane) || caster.nearPlane <= 0 || caster.nearPlane >= caster.range) continue;
    const Vec3 offset = sub(load(caster.position), camera);
    const float distance = std::sqrt(dot(offset, offset));
    // Além do alcance de sombra a luz continua iluminando e deixa de ocluir: é
    // o Max Distance das sombras, e o corte é pela superfície da esfera, não
    // pelo centro, senão uma luz de raio grande some de perto.
    if (distance - caster.range > input.shadowDistance) {
      ++report.droppedByDistance;
      continue;
    }
    queue.push_back({index, screenHeightFraction(input, caster), shadowTileSizeFor(input, caster)});
  }
  std::sort(queue.begin(), queue.end(), [](const Candidate &a, const Candidate &b) {
    if (a.importance != b.importance) return a.importance > b.importance;
    return a.index < b.index;
  });

  const u32 smallest = std::max(8u, previousPowerOfTwo(input.minimumTileSize));
  BuddyAtlas atlas(input.atlasResolution, input.reservedRows, smallest);
  u32 written = 0;
  for (const auto &candidate : queue) {
    const auto &caster = casters[candidate.index];
    const u32 faces = caster.modality == LightModality::Point ? 6u : 1u;
    if (written + faces > out.size() || report.tiles + faces > input.maximumTiles) {
      ++report.droppedByBudget;
      continue;
    }
    // Um cubo com metade das faces é pior do que cubo nenhum: a sombra
    // apareceria e sumiria conforme o objeto anda em volta da lâmpada.
    u32 size = candidate.size;
    u32 origin[6][2]{};
    bool placed = false;
    while (!placed && size >= smallest) {
      placed = true;
      u32 taken = 0;
      for (; taken < faces; ++taken)
        if (!atlas.allocate(size, origin[taken][0], origin[taken][1])) {
          placed = false;
          break;
        }
      if (!placed) {
        // Não devolve o que já pegou: a próxima tentativa é com tile menor, e
        // liberar aqui exigiria fundir nós — complexidade sem ganho, porque o
        // atlas é refeito a cada quadro.
        size /= 2;
      }
    }
    if (!placed) {
      ++report.droppedByBudget;
      continue;
    }

    const Vec3 position = load(caster.position);
    const float farPlane = caster.range;
    const float nearPlane = std::min(caster.nearPlane, farPlane * .5f);
    for (u32 face = 0; face < faces; ++face) {
      auto &tile = out[written++];
      tile = {};
      tile.caster = candidate.index;
      tile.face = face;
      tile.x = origin[face][0];
      tile.y = origin[face][1];
      tile.size = size;
      tile.nearPlane = nearPlane;
      tile.farPlane = farPlane;
      if (caster.modality == LightModality::Point) {
        buildViewProjection(position, kCubeFaces[face].forward, kCubeFaces[face].up, 1.57079633f,
                            nearPlane, farPlane, tile.viewProjection);
        tile.worldUnitsPerTexelAtRange = 2.0f * farPlane / static_cast<float>(size);
      } else {
        Vec3 forward = load(caster.direction);
        if (!normalize(forward)) forward = {0, -1, 0};
        Vec3 up{0, 1, 0};
        if (std::fabs(dot(forward, up)) > .99f) up = {0, 0, 1};
        // O cone do spot é meio-ângulo; o campo de visão do mapa é o ângulo
        // inteiro, com uma folga para a borda suave do cone não cair fora.
        const float halfAngle = std::clamp(caster.outerAngleDegrees, 1.0f, 89.0f) * 0.0174532925f;
        const float fov = std::min(2.0f * halfAngle * 1.05f, 3.0f);
        buildViewProjection(position, forward, up, fov, nearPlane, farPlane, tile.viewProjection);
        tile.worldUnitsPerTexelAtRange = 2.0f * std::tan(fov * .5f) * farPlane / static_cast<float>(size);
      }
      report.usedTexels += size * size;
    }
    report.tiles += faces;
    ++report.castersWithShadow;
  }
  const float area = static_cast<float>(input.atlasResolution) * static_cast<float>(input.atlasResolution);
  report.occupancy = area > 0 ? static_cast<float>(report.usedTexels) / area : 0;
  return written;
}

} // namespace ae::renderer

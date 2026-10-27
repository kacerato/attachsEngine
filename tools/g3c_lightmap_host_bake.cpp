// G3c: o mesmo Gerar iluminação do editor, no host, sobre uma cópia do projeto
// Sponza (cena + geometria da fonte, sem texturas). Serve para medir e depurar
// o baker com a malha real sem ocupar o aparelho: importa a fonte com o perfil
// do projeto (UV de lightmap incluída), monta a biblioteca como o editor, coleta
// a cena pelo mesmo EditorMapScene::collectLightmapScene e assa.
// Uso: g3c_lightmap_host_bake <projeto> <arquivo.gltf relativo> <guid-da-fonte> <saida.aelightmap>
#include "editor/editor_archive.h"
#include "editor/editor_map_scene.h"
#include "renderer/lightmap_baker.h"
#include "renderer/primitive_geometry.h"
#include "resources/gltf_folder_source.h"
#include "resources/gltf_import.h"
#include "resources/gltf_package.h"
#include "resources/import_node_map.h"
#include "resources/import_profile.h"
#include "resources/import_cache.h"
#include "resources/texture_compression.h"
#include "runtime/scene_lights.h"
#include "scene/lighting_settings.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>

using namespace ae;
using namespace ae::editor;

namespace {
[[noreturn]] void fail(const std::string &message) { std::cerr << message << '\n'; std::exit(20); }
std::string readText(const std::filesystem::path &p) { std::ifstream in(p, std::ios::binary); return {std::istreambuf_iterator<char>(in), {}}; }
} // namespace

int main(int argc, char **argv) {
  if (argc != 5) { std::cerr << "uso: g3c_lightmap_host_bake <projeto> <fonte.gltf> <guid> <saida>\n"; return 1; }
  const std::filesystem::path root = std::filesystem::absolute(argv[1]);
  const std::filesystem::path source = root / argv[2];
  const std::string guid = argv[3];
  EditorDocument doc;
  if (!loadEditorDocument((root / "scenes/editor.aescene").string().c_str(), 0, doc)) fail("cena recusada");
  const auto sourceDir = source.parent_path();
  const auto text = readText(source);
  resources::GltfFolder folder;
  folder.context = const_cast<std::filesystem::path *>(&sourceDir);
  folder.read = [](void *ctx, const std::string &relative, std::vector<u8> &out) {
    return resources::readGltfFolderFile(*static_cast<std::filesystem::path *>(ctx), relative, out);
  };
  resources::GltfPackage packed;
  std::string error;
  if (!resources::packGltfFolder({reinterpret_cast<const u8 *>(text.data()), text.size()}, folder, 512ull << 20, packed, error)) fail(error);
  resources::ImportProfile profile;
  if (!resources::parseImportProfile(readText(root / ".astra/imports" / (guid + ".profile")), profile)) fail("perfil recusado");
  resources::GltfImport model;
  resources::GltfImportProgress progress;
  // As imagens de uma fonte em pasta continuam por URI (gltf_package.h): sem
  // este leitor o host importava sem nenhuma textura e assava com a cor base
  // do material (albedo 0,94 na Sponza). Mesmo leitor da importação do editor.
  progress.context = const_cast<std::filesystem::path *>(&sourceDir);
  progress.externalFile = [](void *context, std::string_view uri, u64 prefix, std::vector<u8> &bytes) {
    std::string relative, refusal;
    return resources::gltfRelativeUri(uri, relative, refusal) &&
           resources::readGltfFolderFile(*static_cast<std::filesystem::path *>(context), relative, bytes,
                                         resources::GltfFolderMaximumFileBytes, prefix);
  };
  auto limits = resources::applyImportProfile(resources::GltfImportLimits{}, profile);
  const auto importStart = std::chrono::steady_clock::now();
  // A importação com UV2 é a parte lenta: guardada ao lado da saída e reaproveitada.
  const std::string cachePath = std::string(argv[4]) + ".aic", cacheKey = "g3c-host-texturas-" + resources::serializeImportProfile(profile);
  {
    const auto cached = readText(cachePath);
    if (cached.empty() || !resources::readImportCache({reinterpret_cast<const u8 *>(cached.data()), cached.size()}, cacheKey, model)) {
      if (!resources::importGlb(packed.glb, limits, progress, model)) fail(model.diagnostic);
      std::vector<u8> bytes;
      if (resources::writeImportCache(model, cacheKey, bytes))
        std::ofstream(cachePath, std::ios::binary).write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
  }
  std::cout << "importação: " << std::chrono::duration<float>(std::chrono::steady_clock::now() - importStart).count() << " s, "
            << model.draws.size() << " desenhos, " << model.vertices.size() / renderer::MapVertexStride << " vértices, UV2 em "
            << model.lightmapUvDraws << " desenhos (" << model.lightmapUvCharts << " ilhas)\n";
  resources::ImportNodeMap nodeMap;
  if (!resources::ImportNodeMap::deserialize(readText(root / ".astra/imports" / (guid + ".nodes")), nodeMap) ||
      nodeMap.nodes.size() != model.nodes.size()) fail("mapa de nós recusado");
  std::vector<resources::AssetGuid> identity;
  std::vector<u32> perNode(model.nodes.size());
  for (u32 i = 0; i < model.draws.size(); ++i) { const auto n = model.drawNodes[i]; identity.push_back(nodeMap.nodes[n].draws.at(perNode[n]++)); }
  std::vector<u8> vertices;
  std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  renderer::appendPrimitiveLibrary(renderer::MapVertexStride, vertices, indices, draws, materials);
  std::vector<resources::AssetGuid> identities(draws.size());
  std::vector<float> pivots(draws.size() * 3, 0);
  std::vector<std::string> names(draws.size());
  const auto vb = vertices.size() / renderer::MapVertexStride, ib = indices.size(), mb = materials.size();
  vertices.insert(vertices.end(), model.vertices.begin(), model.vertices.end());
  indices.insert(indices.end(), model.indices.begin(), model.indices.end());
  materials.insert(materials.end(), model.materials.begin(), model.materials.end());
  for (auto d : model.draws) { d.vertexOffset += static_cast<u32>(vb); d.firstIndex += static_cast<u32>(ib); d.materialIndex += static_cast<u32>(mb); draws.push_back(d); }
  identities.insert(identities.end(), identity.begin(), identity.end());
  pivots.resize(draws.size() * 3, 0);
  names.insert(names.end(), model.names.begin(), model.names.end());
  EditorMapScene library;
  if (!library.adoptPackage(doc, draws, materials, vertices, indices, identities, 0, pivots, names)) fail("biblioteca recusada");
  library.reconcileAssets(doc);
  renderer::LightmapBakeScene scene;
  std::vector<u32> textures;
  if (!library.collectLightmapScene(doc, 0, scene, textures, error)) fail(error);
  // A cor base das texturas, como o editor faz antes do bake (editor_lightmaps:
  // prévia RGBA8 de 64 px da textura publicada). Sem isto o host assava com a
  // cor base do material (albedo médio 0,94 na Sponza) e os rebotes saíam com
  // o dobro da energia do aparelho: a "paridade" de 2,29x vinha daí (27/10).
  {
    u32 filled = 0;
    scene.textures.resize(textures.size());
    for (usize i = 0; i < textures.size(); ++i) {
      const u32 published = textures[i];
      if (published >= model.textures.size() || !model.textures[published]) continue;
      renderer::AuthoringTexture preview;
      u32 first = 0;
      std::string diagnostic;
      if (!resources::previewTexture(*model.textures[published], 64, preview, first, diagnostic) ||
          preview.format != renderer::AuthoringTextureRgba8) continue;
      auto &texture = scene.textures[i];
      texture.width = preview.width;
      texture.height = preview.height;
      texture.srgb = model.textures[published]->srgb;
      const usize bytes = usize(preview.width) * preview.height * 4;
      if (preview.mipChain.size() < bytes) { texture = {}; continue; }
      texture.rgba.assign(preview.mipChain.begin(), preview.mipChain.begin() + bytes);
      ++filled;
    }
    std::cout << "texturas de cor base: " << filled << " de " << textures.size() << "\n";
  }
  u32 noUv = 0;
  for (const auto &instance : scene.instances) if (scene.meshes[instance.mesh].uv1Valid == false) ++noUv;
  {
    // Diagnóstico: as normais dos vértices concordam com o enrolamento?
    u64 agree = 0, disagree = 0, zero = 0;
    for (const auto &mesh : scene.meshes)
      for (usize t = 0; t + 2 < mesh.indices.size(); t += 3) {
        float n[3]{}, p[3][3];
        for (u32 k = 0; k < 3; ++k) {
          const u32 v = mesh.indices[t + k];
          for (u32 c = 0; c < 3; ++c) { n[c] += mesh.normals[v * 3 + c]; p[k][c] = mesh.positions[v * 3 + c]; }
        }
        const float e1[3]{p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2]}, e2[3]{p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2]};
        const float f[3]{e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
        const float d = n[0] * f[0] + n[1] * f[1] + n[2] * f[2];
        if (n[0] * n[0] + n[1] * n[1] + n[2] * n[2] < 1e-6f) ++zero; else if (d >= 0) ++agree; else ++disagree;
      }
    std::cout << "normais: concordam " << agree << ", contrárias " << disagree << ", nulas " << zero << '\n';
  }
  std::cout << "cena: " << scene.instances.size() << " instâncias, " << scene.meshes.size() << " malhas, " << noUv << " sem UV2 válida\n";
  {
    // Diagnóstico: visibilidade do céu em triângulos reais (amostra de cada
    // objeto pedido), com o que os raios acertam.
    renderer::LightmapTracer tracer(scene);
    // Pontos fixos do pátio (chão no centro, a 1 m e a 2 m de altura, olhando para cima).
    for (const float y : {0.02f, 1.0f, 2.0f, 8.0f}) {
      const float origin[3]{0.5f, y, 0.0f};
      u32 sky = 0;
      std::map<std::string, u32> hits;
      for (u32 r = 0; r < 512; ++r) {
        const float u = (r + .5f) / 512, v = std::fmod(r * 0.618034f, 1.0f);
        const float radius = std::sqrt(u), phi = 6.2831853f * v, z = std::sqrt(1 - u);
        const float d[3]{radius * std::cos(phi), z, radius * std::sin(phi)};
        const auto result = tracer.trace(origin, d, 1e30f);
        if (!result.hit) { ++sky; continue; }
        const auto *he = doc.find(static_cast<EditorEntityId>(scene.instances[result.instance].objectId));
        ++hits[(he ? std::string(he->name) : "?") + (result.back ? " (verso)" : "")];
      }
      std::cout << "  pátio y=" << y << ": céu " << sky / 5.12f << "%";
      std::vector<std::pair<u32, std::string>> top;
      for (const auto &[k, c] : hits) top.push_back({c, k});
      std::sort(top.rbegin(), top.rend());
      for (usize k = 0; k < top.size() && k < 4; ++k) std::cout << " | " << top[k].second << " " << top[k].first;
      std::cout << '\n';
    }
    const char *wanted[]{"columns_1stfloor_01", "stonewalls_2ndfloor_02", "arch_stones_04", "floor", "exterior"};
    for (const char *name : wanted) {
      for (u32 i = 0; i < scene.instances.size(); ++i) {
        const auto &instance = scene.instances[i];
        const auto *e = doc.find(static_cast<EditorEntityId>(instance.objectId));
        if (!e || std::string_view(e->name).rfind(name, 0) != 0) continue;
        const auto &mesh = scene.meshes[instance.mesh];
        const float *m = instance.model;
        const auto xf = [&](const float *p, float out[3], bool point) {
          for (u32 r = 0; r < 3; ++r) out[r] = m[r] * p[0] + m[4 + r] * p[1] + m[8 + r] * p[2] + (point ? m[12 + r] : 0.0f);
        };
        const usize triangles = mesh.indices.size() / 3;
        for (usize sample = 0; sample < 3 && triangles; ++sample) {
          const usize t = (triangles * (2 * sample + 1)) / 6;
          float p[3]{}, n[3]{};
          for (u32 k = 0; k < 3; ++k) {
            const u32 v = mesh.indices[t * 3 + k];
            float wp[3], wn[3];
            xf(&mesh.positions[v * 3], wp, true);
            xf(&mesh.normals[v * 3], wn, false);
            for (u32 c = 0; c < 3; ++c) { p[c] += wp[c] / 3; n[c] += wn[c]; }
          }
          const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
          for (float &c : n) c /= len;
          // Base em volta da normal e 512 raios em cosseno.
          float tx[3]{1, 0, 0};
          if (std::fabs(n[0]) > .9f) { tx[0] = 0; tx[1] = 1; }
          float b1[3]{n[1] * tx[2] - n[2] * tx[1], n[2] * tx[0] - n[0] * tx[2], n[0] * tx[1] - n[1] * tx[0]};
          const float bl = std::sqrt(b1[0] * b1[0] + b1[1] * b1[1] + b1[2] * b1[2]);
          for (float &c : b1) c /= bl;
          const float b2[3]{n[1] * b1[2] - n[2] * b1[1], n[2] * b1[0] - n[0] * b1[2], n[0] * b1[1] - n[1] * b1[0]};
          u32 sky = 0, front = 0, back = 0;
          std::map<std::string, u32> hits;
          float origin[3];
          for (u32 c = 0; c < 3; ++c) origin[c] = p[c] + n[c] * 0.002f;
          for (u32 r = 0; r < 512; ++r) {
            const float u = (r + .5f) / 512, v = std::fmod(r * 0.618034f, 1.0f);
            const float radius = std::sqrt(u), phi = 6.2831853f * v, z = std::sqrt(1 - u);
            float d[3];
            for (u32 c = 0; c < 3; ++c) d[c] = b1[c] * radius * std::cos(phi) + b2[c] * radius * std::sin(phi) + n[c] * z;
            const auto result = tracer.trace(origin, d, 1e30f);
            if (!result.hit) { ++sky; continue; }
            result.back ? ++back : ++front;
            const auto *he = doc.find(static_cast<EditorEntityId>(scene.instances[result.instance].objectId));
            ++hits[(he ? std::string(he->name) : "?") + (result.back ? " (verso)" : "")];
          }
          std::cout << "  " << e->name << " slot " << instance.slot << " p=(" << p[0] << "," << p[1] << "," << p[2] << ") n=(" << n[0] << ","
                    << n[1] << "," << n[2] << "): céu " << sky / 5.12f << "% frente " << front / 5.12f << "% verso " << back / 5.12f << "%";
          std::vector<std::pair<u32, std::string>> top;
          for (const auto &[k, c] : hits) top.push_back({c, k});
          std::sort(top.rbegin(), top.rend());
          for (usize k = 0; k < top.size() && k < 3; ++k) std::cout << " | " << top[k].second << " " << top[k].first;
          std::cout << '\n';
        }
        break;
      }
    }
  }
  std::vector<renderer::SceneLight> lights;
  runtime::collectSceneLights(doc, lights, 0.0);
  for (const auto &light : lights) {
    if (light.lightmapMode == 0) continue;
    renderer::LightmapBakeLight value;
    value.kind = light.modality == renderer::LightModality::Directional ? 0u : light.modality == renderer::LightModality::Point ? 1u : 2u;
    std::copy(light.position, light.position + 3, value.position);
    std::copy(light.direction, light.direction + 3, value.direction);
    for (u32 k = 0; k < 3; ++k) value.radiance[k] = light.color[k] * light.intensity;
    value.range = light.range;
    value.bakeDirect = light.lightmapMode == 2;
    value.shadowRadius = light.shadow.mode == 2 ? 0.1f : 0.0f;
    scene.lights.push_back(value);
  }
  renderer::LightmapBakeSettings settings;
  if (const auto *s = static_cast<const scene::LightingSettings *>(doc.find(doc.root())->components.find(scene::LightingSettings::descriptor))) {
    settings.texelsPerUnit = s->lightmapResolution; settings.maxSize = renderer::LightmapMaxSizeSteps[s->maxLightmapSize];
    settings.indirectSamples = static_cast<u32>(s->indirectSamples); settings.environmentSamples = static_cast<u32>(s->environmentSamples);
    settings.directSamples = static_cast<u32>(s->directSamples); settings.bounces = s->bounces;
  }
  renderer::LightmapBakeProgress bakeProgress;
  renderer::LightmapData data;
  renderer::LightmapBakeOptions options;
  options.lowPriority = false;
  if (!renderer::bakeLightmaps(scene, settings, options, bakeProgress, data, error)) fail(error);
  std::cout << "bake: " << data.bakeSeconds << " s, " << data.stats.receivers << " receptores, " << data.stats.texels << " texels ("
            << data.stats.invalidTexels << " descartados dentro de parede), " << data.stats.rays << " raios, " << data.pages.size()
            << " páginas\n";
  // Diagnóstico: visibilidade média do céu por objeto (piores primeiro).
  struct Row { float vis; u64 id; u32 slot; u32 material; float area; };
  std::vector<Row> rows;
  for (const auto &entry : data.entries) {
    const auto &page = data.pages[entry.page];
    double sum = 0; u32 n = 0;
    const u32 x0 = static_cast<u32>(entry.scaleOffset[2] * page.width), y0 = static_cast<u32>(entry.scaleOffset[3] * page.height);
    const u32 w = std::max(1u, static_cast<u32>(entry.scaleOffset[0] * page.width));
    for (u32 y = y0; y < y0 + w; ++y) for (u32 x = x0; x < x0 + w; ++x) { float rgb[3], v; renderer::decodeLightmapTexel(page, x, y, rgb, v); sum += v; ++n; }
    u32 material = 0;
    for (const auto &instance : scene.instances) if (instance.objectId == entry.objectId && instance.slot == entry.slot) material = instance.material;
    rows.push_back({static_cast<float>(sum / std::max(1u, n)), entry.objectId, entry.slot, material, static_cast<float>(w)});
  }
  std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.vis < b.vis; });
  for (usize i = 0; i < rows.size(); i += std::max<usize>(1, rows.size() / 40)) {
    const auto *e = doc.find(static_cast<EditorEntityId>(rows[i].id));
    const auto &m = scene.materials[rows[i].material];
    std::cout << "  V=" << rows[i].vis << " lado=" << rows[i].area << " " << (e ? std::string(e->name) : "?") << " slot " << rows[i].slot
              << (m.alphaTest ? " recorte" : "") << (m.doubleSided ? " 2faces" : "") << '\n';
  }
  // Paridade na vista "G3c arcada norte": por pixel, o que o tempo real soma
  // (direta difusa de todas as luzes locais, com e sem sombra, e o especular
  // GGX) contra o que o assado soma (lightmap decodificado + direta da Mista,
  // que continua em tempo real). Uma variável por vez, a mesma geometria.
  {
    renderer::LightmapTracer tracer(scene);
    auto shared = std::make_shared<renderer::LightmapData>(data);
    renderer::LightmapLookup lookup(shared);
    const float target[3]{1.0f, 1.1f, 4.4f}, distance = 10, yaw = 1.5708f, pitch = .08f;
    const float forward[3]{std::cos(pitch) * std::sin(yaw), -std::sin(pitch), std::cos(pitch) * std::cos(yaw)};
    const float eye[3]{target[0] - forward[0] * distance, target[1] - forward[1] * distance, target[2] - forward[2] * distance};
    const float right[3]{std::cos(yaw), 0, -std::sin(yaw)};
    const float up[3]{forward[1] * right[2] - forward[2] * right[1], forward[2] * right[0] - forward[0] * right[2], forward[0] * right[1] - forward[1] * right[0]};
    const float tanHalf = std::tan(30.0f * 3.14159265f / 180), aspect = 1330.0f / 1140.0f;
    const u32 W = 117, H = 100;
    double sum[6]{}, green[2]{}, disney[2]{};
    u32 hits = 0, greenHits = 0;
    const auto luma = [](const float c[3]) { return .2126 * c[0] + .7152 * c[1] + .0722 * c[2]; };
    for (u32 py = 0; py < H; ++py)
      for (u32 px = 0; px < W; ++px) {
        const float sx = (2 * (px + .5f) / W - 1) * tanHalf * aspect, sy = (1 - 2 * (py + .5f) / H) * tanHalf;
        float dir[3];
        for (u32 k = 0; k < 3; ++k) dir[k] = forward[k] + right[k] * sx + up[k] * sy;
        const float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
        for (float &d : dir) d /= len;
        const auto hit = tracer.trace(eye, dir, 100);
        if (!hit.hit || hit.back) continue;
        const auto &inst = scene.instances[hit.instance];
        const auto *entry = lookup.find(inst.objectId, inst.slot);
        if (!entry) continue;
        float p[3], E[3]{}, Eno[3]{}, Emixed[3]{}, spec[3]{};
        for (u32 k = 0; k < 3; ++k) p[k] = eye[k] + dir[k] * hit.distance;
        const float *n = hit.normal;
        for (const auto &light : lights) {
          if (light.modality == renderer::LightModality::Directional || light.celestialBody != renderer::CelestialLightBody::None) continue;
          float l[3]{light.position[0] - p[0], light.position[1] - p[1], light.position[2] - p[2]};
          const float d2 = std::max(l[0] * l[0] + l[1] * l[1] + l[2] * l[2], 1e-4f), d = std::sqrt(d2);
          for (float &c : l) c /= d;
          const float nl = n[0] * l[0] + n[1] * l[1] + n[2] * l[2];
          if (nl <= 0) continue;
          const float ratio = d2 / (light.range * light.range);
          float window = std::clamp(1 - ratio * ratio, 0.0f, 1.0f);
          window *= window;
          float origin[3];
          for (u32 k = 0; k < 3; ++k) origin[k] = p[k] + n[k] * .01f;
          const auto block = tracer.trace(origin, l, d - .02f);
          const float vis = (light.shadow.casts() && block.hit) ? 0.0f : 1.0f;
          // GGX (rugosidade 0,6; f0 0,04), mesma forma do directLight.
          const float v[3]{-dir[0], -dir[1], -dir[2]};
          float h[3]{v[0] + l[0], v[1] + l[1], v[2] + l[2]};
          const float hl = std::sqrt(h[0] * h[0] + h[1] * h[1] + h[2] * h[2]);
          for (float &c : h) c /= hl;
          const float nh = std::max(0.0f, n[0] * h[0] + n[1] * h[1] + n[2] * h[2]), nv = std::max(1e-4f, n[0] * v[0] + n[1] * v[1] + n[2] * v[2]);
          const float a2 = .36f * .36f, dd = nh * nh * (a2 - 1) + 1, D = a2 / (3.14159265f * dd * dd);
          const float k = .36f / 2, G = (nl / (nl * (1 - k) + k)) * (nv / (nv * (1 - k) + k)) / (4 * nl * nv);
          const float F = .04f + .96f * std::pow(1 - std::max(0.0f, v[0] * h[0] + v[1] * h[1] + v[2] * h[2]), 5.0f);
          // Difuso do directLight: Burley sem renormalização (rugosidade 0,8),
          // e a versão renormalizada do Frostbite (Lagarde 2014), contra Lambert.
          const float lh = std::max(0.0f, l[0] * h[0] + l[1] * h[1] + l[2] * h[2]), rough = .8f;
          const float fd90 = .5f + 2 * rough * lh * lh;
          const float burley = (1 + (fd90 - 1) * std::pow(1 - nl, 5.0f)) * (1 + (fd90 - 1) * std::pow(1 - nv, 5.0f));
          const float bias = .5f * rough, factor = 1 + (1 / 1.51f - 1) * rough, fd90r = bias + 2 * rough * lh * lh;
          const float frostbite = (1 + (fd90r - 1) * std::pow(1 - nl, 5.0f)) * (1 + (fd90r - 1) * std::pow(1 - nv, 5.0f)) * factor;
          disney[0] += burley * luma(hit.albedo) / 3.14159265f * (light.color[1] * light.intensity * window / d2) * nl * vis;
          disney[1] += frostbite * luma(hit.albedo) / 3.14159265f * (light.color[1] * light.intensity * window / d2) * nl * vis;
          for (u32 c = 0; c < 3; ++c) {
            const float radiance = light.color[c] * light.intensity * window / d2;
            Eno[c] += radiance * nl;
            E[c] += radiance * nl * vis;
            spec[c] += D * G * F * radiance * nl * vis;
            if (light.lightmapMode == 1) Emixed[c] += radiance * nl * vis;
          }
        }
        const auto &page = data.pages[entry->page];
        const u32 x = std::min(page.width - 1, static_cast<u32>((hit.uv1[0] * entry->scaleOffset[0] + entry->scaleOffset[2]) * page.width));
        const u32 y = std::min(page.height - 1, static_cast<u32>((hit.uv1[1] * entry->scaleOffset[1] + entry->scaleOffset[3]) * page.height));
        float lm[3], vis;
        renderer::decodeLightmapTexel(page, x, y, lm, vis);
        float rtS[3], rtN[3], sp[3], bk[3], bkDirectOnly[3];
        for (u32 c = 0; c < 3; ++c) {
          const float a = hit.albedo[c] / 3.14159265f;
          rtS[c] = a * E[c]; rtN[c] = a * Eno[c]; sp[c] = spec[c]; bk[c] = a * (lm[c] + Emixed[c]); bkDirectOnly[c] = a * Emixed[c];
        }
        sum[0] += luma(rtS); sum[1] += luma(rtN); sum[2] += luma(sp); sum[3] += luma(bk); sum[4] += luma(bkDirectOnly);
        float alb[3]{hit.albedo[0], hit.albedo[1], hit.albedo[2]}; sum[5] += luma(alb);
        ++hits;
        if (px < W / 3 && py > H / 4 && py < 3 * H / 4) { green[0] += rtS[1]; green[1] += bk[1]; ++greenHits; }
      }
    if (hits) {
      std::cout << "paridade na vista (" << hits << " pixels): tempo real difuso com sombra " << sum[0] / hits << " · sem sombra " << sum[1] / hits
                << " · especular GGX " << sum[2] / hits << " · assado (lightmap + Mista) " << sum[3] / hits << " · albedo médio " << sum[5] / hits << '\n';
      std::cout << "  razão assado/tempo real (com sombra) " << sum[3] / sum[0] << " · com especular " << sum[3] / (sum[0] + sum[2])
                << " · sem sombra " << sum[3] / sum[1] << '\n';
      std::cout << "  difuso direto: Lambert " << sum[0] / hits << " · Burley do shader " << disney[0] / hits << " (x" << disney[0] / sum[0]
                << ") · Burley renormalizado " << disney[1] / hits << " (x" << disney[1] / sum[0] << ")\n";
      if (greenHits) std::cout << "  parede esquerda, canal verde: tempo real " << green[0] / greenHits << " · assado " << green[1] / greenHits << '\n';
    }
  }
  data.guidHigh = 1; data.guidLow = 2;
  const auto bytes = data.serialize();
  std::ofstream(argv[4], std::ios::binary).write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return 0;
}

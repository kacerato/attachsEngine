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
  progress.externalFile = [](void *, std::string_view, u64, std::vector<u8> &) { return false; };
  auto limits = resources::applyImportProfile(resources::GltfImportLimits{}, profile);
  const auto importStart = std::chrono::steady_clock::now();
  // A importação com UV2 é a parte lenta: guardada ao lado da saída e reaproveitada.
  const std::string cachePath = std::string(argv[4]) + ".aic", cacheKey = "g3c-host-" + resources::serializeImportProfile(profile);
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
  data.guidHigh = 1; data.guidLow = 2;
  const auto bytes = data.serialize();
  std::ofstream(argv[4], std::ios::binary).write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return 0;
}

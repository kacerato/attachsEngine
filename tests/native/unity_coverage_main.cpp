// G6a/G6b: cobertura da importação de um pacote Unity extraído (pastas com os
// .meta), sem editor e sem GPU. Converte cada .prefab pelo mesmo conversor da
// Biblioteca (resources/unity_prefab_import + runtime/unity_prefab), com as
// dependências do próprio pacote, e classifica:
//   suportado — nada ficou de fora;
//   sem perda visível — só aproximações listadas;
//   parcial — perdas visíveis (módulo, malha ou script sem consumidor);
//   recusado — o prefab não abre.
// Também lê os Weather Type e Cloud Profile do UniStorm.
//
// Uso: aether_unity_coverage <pasta do pacote> <saida.json> [detalhe-local.txt]
// O JSON tem só contagens (pode ser publicado); o detalhe tem nomes de arquivo
// do pacote e fica na máquina de quem roda.
#include "core/sha256.h"
#include "resources/unistorm_import.h"
#include "resources/unity_material_import.h"
#include "resources/unity_prefab_import.h"
#include "resources/unity_yaml.h"
#include "runtime/unity_prefab.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

using namespace ae;
using namespace ae::resources;

namespace {
std::string readFile(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}
std::filesystem::path utf8Path(const char *text) {
  return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(text)));
}
std::string json(const std::string &text) {
  std::string out = "\"";
  for (const char c : text) {
    if (c == '"' || c == '\\') out.push_back('\\');
    out.push_back(c);
  }
  return out + "\"";
}
} // namespace

int main(int argc, char **argv) {
  if (argc < 3) {
    std::cerr << "uso: aether_unity_coverage <pasta do pacote> <saida.json> [detalhe.txt]\n";
    return 2;
  }
  const std::filesystem::path root = utf8Path(argv[1]);
  std::map<std::string, std::filesystem::path> byGuid;
  std::vector<std::filesystem::path> prefabs, assets;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
    if (!entry.is_regular_file()) continue;
    const auto path = entry.path();
    if (path.extension() == ".meta") {
      const auto text = readFile(path);
      auto at = text.find("\nguid: ");
      if (at != std::string::npos) ++at;
      else if (text.starts_with("guid: ")) at = 0;
      if (at != std::string::npos) byGuid[text.substr(at + 6, 32)] = path.parent_path() / path.stem();
    } else if (path.extension() == ".prefab") prefabs.push_back(path);
    else if (path.extension() == ".asset") assets.push_back(path);
  }
  std::sort(prefabs.begin(), prefabs.end());
  const auto pathOf = [&](const AssetGuid &guid) -> std::filesystem::path {
    const auto found = byGuid.find(guid.text());
    return found == byGuid.end() ? std::filesystem::path() : found->second;
  };
  UnityPrefabContext context;
  context.scriptName = [&](const AssetGuid &guid) {
    const auto path = pathOf(guid);
    return path.extension() == ".cs" ? path.stem().string() : std::string();
  };
  // Dependências do próprio pacote: existem e convertem (material pelo
  // conversor real), ou a perda entra nas notas do prefab.
  const runtime::UnityPrefabResolver resolve = [&](const UnityObjectReference &reference, AssetType type,
                                                   std::vector<AssetGuid> &ids, std::string &error) {
    const auto path = pathOf(reference.guid);
    if (path.empty()) { error = "dependência fora do pacote"; return false; }
    if (type == AssetType::Material) {
      UnityMaterialImport material;
      if (!readUnityMaterial(readFile(path), material) || !material.supported()) {
        error = material.diagnostic.empty() && !material.unsupported.empty() ? material.unsupported.front() : material.diagnostic;
        return false;
      }
    }
    if (type == AssetType::AudioClip && path.extension() != ".wav") { error = "áudio fora de WAV"; return false; }
    ids.push_back(assetGuidFromSeed("coverage:" + reference.guid.text() + ":" + std::to_string(reference.fileId)));
    return true;
  };
  runtime::UnityPrefabEffectServices effects;
  effects.particleLook = [&](const UnityObjectReference &reference, UnityParticleLook &look) {
    UnityMaterialImport material;
    if (!readUnityMaterial(readFile(pathOf(reference.guid)), material) || material.particle.family == UnityParticleFamily::None) return false;
    look = material.particle;
    return true;
  };
  effects.particleMesh = [&](const UnityObjectReference &reference, AssetType, std::vector<AssetGuid> &ids, std::string &error) {
    const auto path = pathOf(reference.guid);
    const auto ext = path.extension().string();
    if (ext != ".fbx" && ext != ".FBX" && ext != ".glb") { error = "malha fora de FBX/glTF"; return false; }
    ids.push_back(assetGuidFromSeed("coverage-mesh:" + reference.guid.text()));
    return true;
  };
  u32 opened = 0, supported = 0, invisible = 0, partial = 0, refused = 0;
  u32 systems = 0, lights = 0, audio = 0, lines = 0, scripts = 0;
  std::map<std::string, u32> lossCounts, approximationCounts;
  std::ostringstream detail;
  for (const auto &path : prefabs) {
    const auto relative = std::filesystem::relative(path, root).generic_string();
    UnityYamlDocument document;
    std::string error;
    UnityPrefabImport imported;
    runtime::Prefab prefab;
    runtime::UnityPrefabIdentityMap identities;
    std::vector<std::string> notes;
    effects.notes = &notes;
    const bool ok = readUnityYaml(readFile(path), document, error) && convertUnityPrefab(document, imported, error, &context) &&
                    runtime::buildUnityPrefab(imported, assetGuidFromSeed("coverage-prefab:" + relative), resolve, nullptr, prefab,
                                              identities, error, &effects);
    if (!ok) {
      ++refused;
      detail << "RECUSADO " << relative << " :: " << error << "\n";
      continue;
    }
    ++opened;
    systems += imported.coverage.particleSystems;
    lights += imported.coverage.lights;
    audio += imported.coverage.audioSources;
    lines += imported.coverage.lines;
    scripts += imported.coverage.scripts;
    auto losses = imported.coverage.losses;
    for (const auto &note : notes) losses.push_back(note);
    for (const auto &loss : losses) ++lossCounts[loss.substr(0, loss.find('('))];
    for (const auto &approx : imported.coverage.approximations) ++approximationCounts[approx];
    if (losses.empty() && imported.coverage.approximations.empty()) { ++supported; detail << "SUPORTADO " << relative << "\n"; }
    else if (losses.empty()) { ++invisible; detail << "SEM PERDA VISIVEL " << relative << "\n"; }
    else {
      ++partial;
      detail << "PARCIAL " << relative << "\n";
      for (const auto &loss : losses) detail << "    perda: " << loss << "\n";
    }
  }
  // G6b: perfis do UniStorm.
  u32 weathers = 0, clouds = 0, weatherRefused = 0;
  for (const auto &path : assets) {
    UnityYamlDocument document;
    std::string error, script;
    if (!readUnityYaml(readFile(path), document, error) || !uniStormScript(document, script)) continue;
    if (script == UniStormWeatherTypeScript) {
      UniStormWeather weather;
      if (readUniStormWeather(document, weather, error)) ++weathers;
      else ++weatherRefused;
    } else if (script == UniStormCloudProfileScript) {
      UniStormCloud cloud;
      if (readUniStormCloud(document, cloud, error)) ++clouds;
      else ++weatherRefused;
    }
  }
  const u32 total = static_cast<u32>(prefabs.size());
  std::ofstream out(utf8Path(argv[2]), std::ios::binary);
  out << "{\n  \"format\": 1,\n  \"prefabs\": " << total << ",\n  \"opened\": " << opened << ",\n  \"supported\": " << supported
      << ",\n  \"approximated_without_visible_loss\": " << invisible << ",\n  \"partial_with_visible_loss\": " << partial
      << ",\n  \"refused\": " << refused << ",\n  \"without_visible_loss_ratio\": "
      << (total ? static_cast<double>(supported + invisible) / total : 0.0) << ",\n  \"particle_systems\": " << systems
      << ",\n  \"lights\": " << lights << ",\n  \"audio_sources\": " << audio << ",\n  \"line_renderers\": " << lines
      << ",\n  \"scripts\": " << scripts << ",\n  \"unistorm_weather_types\": " << weathers << ",\n  \"unistorm_cloud_profiles\": " << clouds
      << ",\n  \"unistorm_refused\": " << weatherRefused << ",\n  \"losses\": {";
  bool first = true;
  for (const auto &[text, count] : lossCounts) { out << (first ? "\n    " : ",\n    ") << json(text) << ": " << count; first = false; }
  out << "\n  },\n  \"approximations\": {";
  first = true;
  for (const auto &[text, count] : approximationCounts) { out << (first ? "\n    " : ",\n    ") << json(text) << ": " << count; first = false; }
  out << "\n  }\n}\n";
  if (argc > 3) std::ofstream(utf8Path(argv[3]), std::ios::binary) << detail.str();
  std::printf("%u prefabs: %u abrem (%u suportados, %u sem perda visível, %u parciais), %u recusados; UniStorm %u climas, %u nuvens\n",
              total, opened, supported, invisible, partial, refused, weathers, clouds);
  return refused ? 1 : 0;
}

// G6b: perfis do UniStorm pela Biblioteca de pacotes. Abrir um Weather Type
// converte todos os do pacote em presets do Clima (com o som de clima, quando o
// tipo usa um WAV); abrir um Cloud Profile converte todos em perfis de
// Ambiente com só as Nuvens. Só dados: nenhum script ou shader do pacote.
#include "editor/editor_session.h"
#include "resources/unistorm_import.h"
#include "scene/audio.h"

#include <algorithm>
#include <filesystem>
#include <map>

namespace ae::editor {
namespace {
std::string lowerExtension(const std::string &path) {
  auto ext = std::filesystem::path(path).extension().string();
  for (auto &c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return ext;
}
} // namespace

bool EditorSession::uniStormAsset(const std::string &path, std::string &script) const {
  if (lowerExtension(path) != ".asset") return false;
  std::filesystem::path absolute;
  std::vector<u8> bytes;
  if (!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()), path, absolute) ||
      !EditorImportTransaction::read(absolute, bytes, 256u * 1024u))
    return false;
  resources::UnityYamlDocument document;
  std::string error;
  if (!resources::readUnityYaml({reinterpret_cast<const char *>(bytes.data()), bytes.size()}, document, error) ||
      !resources::uniStormScript(document, script))
    return false;
  return script == resources::UniStormWeatherTypeScript || script == resources::UniStormCloudProfileScript;
}

bool EditorSession::importUniStormProfiles(const std::string &path, std::string &diagnostic) {
  diagnostic.clear();
  if (isPlaying() || history_.isOpen()) { diagnostic = "Pare o Play e conclua a edição antes de importar perfis."; return false; }
  std::string script;
  if (!uniStormAsset(path, script)) { diagnostic = "Não é um perfil do UniStorm."; return false; }
  const resources::LibraryPackage *scope = nullptr;
  for (const auto &package : library_.packages())
    for (const auto &item : package.items)
      if (item.path == path) scope = &package;
  if (!scope) { diagnostic = "Importe o UniStorm pela Biblioteca de pacotes."; return false; }
  // Cópia dos caminhos: converter um WAV publica o índice e troca o pacote.
  std::vector<std::string> sources;
  std::map<std::string, std::string> byGuid;
  for (const auto &item : scope->items) {
    if (lowerExtension(item.path) == ".asset") sources.push_back(item.path);
    if (item.sourceIdentity.starts_with("unity:")) byGuid[item.sourceIdentity.substr(6)] = item.path;
  }
  const auto root = EditorImportTransaction::fromUtf8(files_.rootPath());
  const bool weather = script == resources::UniStormWeatherTypeScript;
  std::string loadError;
  if (weather && !componentPresets_.load(files_.rootPath(), loadError)) { diagnostic = loadError; return false; }
  u32 converted = 0, kept = 0, refused = 0, sounds = 0;
  std::vector<std::string> notes;
  for (const auto &source : sources) {
    std::filesystem::path absolute;
    std::vector<u8> bytes;
    if (!EditorImportTransaction::safePath(root, source, absolute) || !EditorImportTransaction::read(absolute, bytes, 256u * 1024u)) continue;
    resources::UnityYamlDocument document;
    std::string error, kind;
    if (!resources::readUnityYaml({reinterpret_cast<const char *>(bytes.data()), bytes.size()}, document, error) ||
        !resources::uniStormScript(document, kind) || kind != script)
      continue;
    if (weather) {
      resources::UniStormWeather parsed;
      if (!resources::readUniStormWeather(document, parsed, error)) { ++refused; notes.push_back(source + ": " + error); continue; }
      const auto name = "Clima · " + parsed.name;
      const bool exists = std::any_of(componentPresets_.entries.begin(), componentPresets_.entries.end(),
                                      [&](const auto &entry) { return entry.name == name; });
      if (exists) { ++kept; continue; }
      scene::Components parts;
      auto *value = static_cast<scene::Weather *>(parts.add(scene::Weather::descriptor));
      if (!value) { ++refused; continue; }
      // Os valores do tipo pelos descritores (a identidade da instância fica).
      value->kind = parsed.weather.kind;
      for (const auto &p : scene::Weather::descriptor.numbers) *p.write(*value) = p.read(parsed.weather);
      for (const auto &p : scene::Weather::descriptor.booleans) p.write(*value, p.read(parsed.weather));
      // Som de clima do tipo (WAV do pacote), em laço, como o do UniStorm.
      if (parsed.useSound) {
        const auto found = byGuid.find(parsed.sound.guid.text());
        resources::AssetGuid clip;
        if (found != byGuid.end() && lowerExtension(found->second) == ".wav") {
          const auto *known = assets_.findByPath(found->second);
          if (known && known->type == resources::AssetType::AudioClip) clip = known->guid;
          else if (std::string wave; !importWaveClip(found->second, clip, wave)) clip = {};
        }
        if (clip.valid()) {
          auto *audio = static_cast<scene::AudioSource *>(parts.add(scene::AudioSource::descriptor));
          if (audio) {
            audio->clip = clip;
            audio->loop = true;
            audio->volume = parsed.soundVolume;
            audio->playback = scene::AudioPlayback::Playing;
            audio->dimension = scene::AudioDimension::Flat;
            ++sounds;
          }
        } else {
          notes.push_back(parsed.name + ": som de clima fora de WAV (MP3/OGG não são lidos pela Astra).");
        }
      }
      if (!(parts.size() > 1 ? componentPresets_.captureRecipe(name, parts, error) : componentPresets_.capture(name, *value, error))) {
        ++refused;
        notes.push_back(parsed.name + ": " + error);
        continue;
      }
      ++converted;
    } else {
      resources::UniStormCloud parsed;
      if (!resources::readUniStormCloud(document, parsed, error)) { ++refused; notes.push_back(source + ": " + error); continue; }
      const auto name = "Nuvens UniStorm · " + parsed.name;
      const bool exists = std::any_of(environmentProfiles_.begin(), environmentProfiles_.end(),
                                      [&](const auto &profile) { return profile.name == name; });
      if (exists) { ++kept; continue; }
      renderer::SceneEnvironment values;
      values.clouds = parsed.clouds;
      auto overrides = renderer::VolumeOverrides::empty();
      overrides.addGroup(renderer::VolumeGroup::Clouds);
      overrides.setGroupParameters(renderer::VolumeGroup::Clouds, true);
      if (!writeNewEnvironmentProfile(name, values, overrides, error).valid()) {
        ++refused;
        notes.push_back(parsed.name + ": " + error);
        continue;
      }
      ++converted;
    }
  }
  diagnostic = weather ? std::to_string(converted) + " perfis de clima viraram presets do Clima (" + std::to_string(sounds) + " com som)"
                       : std::to_string(converted) + " perfis de nuvem viraram perfis de Ambiente";
  if (kept) diagnostic += " · " + std::to_string(kept) + " já existiam";
  if (refused) diagnostic += " · " + std::to_string(refused) + " recusados";
  for (const auto &note : notes) diagnostic += "\n" + note;
  state_.status = diagnostic;
  return converted + kept > 0;
}
} // namespace ae::editor

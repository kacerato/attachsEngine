#include "editor/editor_widgets.h"
// G3c: Lighting › Generate Lighting / Clear Baked Data no editor.
//
// Fluxo da Unity 6 reproduzido:
//  - Gerar iluminação junta os objetos Static com Contribute GI (malha, pose,
//    material e cor base), as luzes em modo Assado e Misto e as Configurações
//    de iluminação, e assa numa thread própria com jobs (prioridade baixa no
//    aparelho). A janela mostra etapa, passe, tempo e o que falta; Cancelar
//    interrompe sem tocar no recurso anterior.
//  - O resultado vai para Iluminação/<cena> Lightmap.aelightmap (o
//    LightingData da Unity) e as Configurações passam a apontar para ele, num
//    passo de desfazer. Assar de novo reescreve o mesmo recurso.
//  - Limpar desvincula o recurso; o arquivo continua no projeto.
#include "editor/editor_session.h"
#include "editor/editor_import_transaction.h"
#include "editor/editor_screen_internal.h"
#include "core/sha256.h"
#include "resources/texture_compression.h"
#include "scene/light.h"
#include "scene/lighting_settings.h"
#include "scene/mesh_renderer.h"
#include "runtime/scene_probes.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <thread>

namespace ae::editor {

struct EditorSession::LightmapBakeJob {
  renderer::LightmapBakeScene scene;
  renderer::LightmapBakeSettings settings;
  std::vector<renderer::SharedAuthoringTexture> sources; // uma por textura da cena
  renderer::LightmapBakeProgress progress;
  renderer::LightmapData result;
  std::string error;
  bool success = false;
  std::atomic<bool> done{false};
  std::thread thread;
  std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
  std::string projectRoot;
  ~LightmapBakeJob() {
    progress.cancel = true;
    if (thread.joinable()) thread.join();
  }
};

namespace {
const scene::LightingSettings *findLightingSettings(const runtime::SceneGraph &document, EditorEntityId *owner, u64 *instance) {
  std::vector<EditorEntityId> ids;
  document.collectSubtree(document.root(), ids);
  for (auto id : ids) {
    const auto *entity = document.find(id);
    if (!entity) continue;
    const auto *component = entity->components.find(scene::LightingSettings::descriptor);
    if (!component) continue;
    if (owner) *owner = id;
    if (instance) *instance = component->instanceId();
    return static_cast<const scene::LightingSettings *>(component);
  }
  return nullptr;
}

renderer::LightmapBakeSettings bakeSettingsFrom(const scene::LightingSettings &value) {
  renderer::LightmapBakeSettings settings;
  settings.texelsPerUnit = value.lightmapResolution;
  settings.padding = static_cast<u32>(std::lround(value.lightmapPadding));
  settings.maxSize = renderer::LightmapMaxSizeSteps[std::min<u32>(value.maxLightmapSize, 4)];
  settings.directSamples = static_cast<u32>(std::lround(value.directSamples));
  settings.indirectSamples = static_cast<u32>(std::lround(value.indirectSamples));
  settings.environmentSamples = static_cast<u32>(std::lround(value.environmentSamples));
  settings.bounces = value.bounces;
  settings.denoise = value.denoise;
  settings.compress = value.compressLightmaps;
  settings.indirectIntensity = value.indirectIntensity;
  settings.albedoBoost = value.albedoBoost;
  settings.directionalMode = value.directionalMode;
  return settings;
}

// Luzes que o bake considera: Misto (só o rebote) e Assado (direta e rebote).
// O astro da Hora do dia já chega aqui em Tempo real (runtime/scene_lights.cpp).
void bakeLightsFrom(std::span<const renderer::SceneLight> lights, std::vector<renderer::LightmapBakeLight> &out) {
  for (const auto &light : lights) {
    if (light.lightmapMode == 0 || light.celestialBody != renderer::CelestialLightBody::None) continue;
    renderer::LightmapBakeLight value;
    value.kind = light.modality == renderer::LightModality::Directional ? 0u : light.modality == renderer::LightModality::Point ? 1u : 2u;
    std::copy(light.position, light.position + 3, value.position);
    std::copy(light.direction, light.direction + 3, value.direction);
    for (u32 k = 0; k < 3; ++k) value.radiance[k] = light.color[k] * light.intensity;
    value.range = light.range;
    if (light.modality == renderer::LightModality::Spot) {
      // A mesma janela de cone do registro do shader (punctual_lights.h).
      const float toRadians = 0.0174532925199433f;
      const float outer = std::cos(std::clamp(light.outerAngle, 0.0f, 89.5f) * toRadians);
      const float inner = std::cos(std::clamp(light.innerAngle, 0.0f, 89.5f) * toRadians);
      const float span = std::max(inner - outer, 1e-4f);
      value.coneScale = 1.0f / span;
      value.coneOffset = -outer / span;
    }
    // Sombra suave do Light Inspector vira o raio da sombra assada (Baked
    // Shadow Radius/Angle da Unity): 10 cm na local, 0,5° na direcional.
    value.castsShadows = light.shadow.casts();
    value.shadowRadius = light.shadow.mode == 2 ? (value.kind == 0 ? 0.0087f : 0.1f) : 0.0f;
    value.bakeDirect = light.lightmapMode == 2;
    out.push_back(value);
  }
}
} // namespace

void EditorSession::loadLightmapAssets() {
  lightmapAssets_.clear();
  activeLightmap_.reset();
  activeLightmapGuid_ = {};
  mapScene_.setLightmapLookup(nullptr);
  ++lightmapRevision_;
  lightmapViewRevision_ = ~u64{0};
  if (files_.rootPath().empty()) return;
  for (const auto &record : assets_.records()) {
    if (record.type != resources::AssetType::Lightmap) continue;
    std::filesystem::path path;
    std::vector<u8> bytes;
    std::string error;
    auto data = std::make_shared<renderer::LightmapData>();
    if (!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()), record.path, path) ||
        !EditorImportTransaction::read(path, bytes, renderer::LightmapData::MaximumBytes) ||
        !renderer::LightmapData::deserialize(bytes, *data, &error) || data->guidHigh != record.guid.high ||
        data->guidLow != record.guid.low) {
      reportProblem(EditorConsoleSeverity::Warning, "Lightmap ausente ou inválido: " + record.path + (error.empty() ? "" : " · " + error));
      continue;
    }
    lightmapAssets_.push_back(std::move(data));
  }
}

std::shared_ptr<const renderer::LightmapData> EditorSession::lightmapAsset(const resources::AssetGuid &guid) const {
  if (!guid.valid()) return nullptr;
  for (const auto &data : lightmapAssets_)
    if (data && data->guidHigh == guid.high && data->guidLow == guid.low) return data;
  return nullptr;
}

bool EditorSession::lightmapBaking() const noexcept { return lightmapBake_ != nullptr; }

bool EditorSession::generateLighting() {
  if (lightmapBake_) { state_.status = "Já existe um bake de iluminação em andamento"; return false; }
  if (isPlaying() || playMirrorOpen_ || history_.isOpen()) { state_.status = "Pare o Play para gerar a iluminação"; return false; }
  if (files_.rootPath().empty()) { state_.status = "Abra um projeto para guardar os lightmaps"; return false; }
  // Sem Configurações de iluminação, a cena ganha as da Unity na raiz (como o
  // Lighting Settings asset novo) num passo de desfazer.
  EditorEntityId owner = kInvalidEntity;
  u64 instance = 0;
  const auto *settings = findLightingSettings(document_, &owner, &instance);
  if (!settings) {
    auto values = *document_.find(document_.root());
    auto *created = static_cast<scene::LightingSettings *>(values.components.add(scene::LightingSettings::descriptor));
    if (!created || !history_.applyValues(document_, document_.root(), values)) {
      state_.status = "A raiz não aceitou as Configurações de iluminação";
      return false;
    }
    settings = findLightingSettings(document_, &owner, &instance);
    if (!settings) return false;
  }
  if (!settings->bakedGlobalIllumination) { state_.status = "Ligue Iluminação global assada nas Configurações"; return false; }
  auto job = std::make_shared<LightmapBakeJob>();
  job->settings = bakeSettingsFrom(*settings);
  if (!job->settings.valid()) { state_.status = "Configurações de lightmap fora do intervalo"; return false; }
  std::vector<u32> textureSources;
  std::string error;
  if (!mapScene_.collectLightmapScene(evaluatedEditorScene(), bakeTextureBase_, job->scene, textureSources, error)) {
    state_.status = "Gerar iluminação: " + error;
    return false;
  }
  if (job->scene.instances.empty()) {
    state_.status = "Nenhum objeto Static com Contribuir para iluminação global";
    reportProblem(EditorConsoleSeverity::Warning, state_.status);
    return false;
  }
  std::vector<renderer::SceneLight> lights;
  if (extractLights(lights)) bakeLightsFrom(lights, job->scene.lights);
  for (u32 published : textureSources) {
    const u32 local = published >= bakeTextureBase_ ? published - bakeTextureBase_ : ~0u;
    job->sources.push_back(local < bakeTextures_.size() ? bakeTextures_[local] : nullptr);
  }
  auto *raw = job.get();
  job->thread = std::thread([raw] {
    // A cor base em baixa resolução (prévia RGBA8, decodificando ASTC) é a
    // mesma conta da prévia de textura do editor; feita aqui, fora do quadro.
    raw->scene.textures.resize(raw->sources.size());
    for (usize i = 0; i < raw->sources.size() && !raw->progress.cancel.load(); ++i) {
      if (!raw->sources[i]) continue;
      renderer::AuthoringTexture preview;
      u32 first = 0;
      std::string diagnostic;
      if (!resources::previewTexture(*raw->sources[i], 64, preview, first, diagnostic) || preview.format != renderer::AuthoringTextureRgba8)
        continue;
      auto &texture = raw->scene.textures[i];
      texture.width = preview.width;
      texture.height = preview.height;
      texture.srgb = raw->sources[i]->srgb;
      const usize bytes = usize(preview.width) * preview.height * 4;
      if (preview.mipChain.size() >= bytes) texture.rgba.assign(preview.mipChain.begin(), preview.mipChain.begin() + bytes);
      else texture = {};
    }
    renderer::LightmapBakeOptions options;
    options.lowPriority = true;
    raw->success = renderer::bakeLightmaps(raw->scene, raw->settings, options, raw->progress, raw->result, raw->error);
    raw->done = true;
  });
  job->projectRoot = files_.rootPath();
  lightmapBake_ = std::move(job);
  lightmapViewRevision_ = ~u64{0};
  char text[160];
  std::snprintf(text, sizeof text, "Gerando iluminação · %zu objetos, %zu luzes assadas…", lightmapBake_->scene.instances.size(),
                lightmapBake_->scene.lights.size());
  state_.status = text;
  return true;
}

void EditorSession::cancelLighting() {
  if (!lightmapBake_) return;
  lightmapBake_->progress.cancel = true;
  state_.status = "Cancelando o bake de iluminação…";
}

void EditorSession::pumpLightmapBake() {
  if (!lightmapBake_ || !lightmapBake_->done.load()) return;
  auto job = std::move(lightmapBake_);
  if (job->thread.joinable()) job->thread.join();
  lightmapViewRevision_ = ~u64{0};
  if (!job->success) {
    lightmapView_.lastResult = job->error.empty() ? "Bake recusado" : job->error;
    state_.status = job->progress.cancel.load() ? "Bake cancelado; os lightmaps anteriores foram preservados" : "Gerar iluminação: " + job->error;
    reportProblem(job->progress.cancel.load() ? EditorConsoleSeverity::Info : EditorConsoleSeverity::Error, state_.status);
    return;
  }
  if (isPlaying() || playMirrorOpen_ || history_.isOpen() || files_.rootPath().empty() || files_.rootPath() != job->projectRoot) {
    state_.status = "Bake concluído com o editor ocupado; gere de novo";
    return;
  }
  EditorEntityId owner = kInvalidEntity;
  u64 instance = 0;
  const auto *settings = findLightingSettings(document_, &owner, &instance);
  if (!settings) { state_.status = "As Configurações de iluminação foram removidas durante o bake"; return; }
  resources::AssetGuid guid{};
  std::string path;
  const auto sceneName = activeSceneName();
  probeResourcePath(resources::AssetType::Lightmap, settings->lightingData, (sceneName.empty() ? std::string("Cena") : sceneName) + " Lightmap",
                    ".aelightmap", guid, path);
  auto data = std::make_shared<renderer::LightmapData>(std::move(job->result));
  data->guidHigh = guid.high;
  data->guidLow = guid.low;
  const auto bytes = data->serialize();
  if (!commitProbeFile(resources::AssetType::Lightmap, guid, path, bytes, renderer::LightmapData::MaximumBytes)) return;
  const auto found = std::find_if(lightmapAssets_.begin(), lightmapAssets_.end(),
                                  [&](const auto &d) { return d && d->guidHigh == guid.high && d->guidLow == guid.low; });
  if (found != lightmapAssets_.end()) *found = data;
  else lightmapAssets_.push_back(data);
  activeLightmapGuid_ = {}; // força recarregar a consulta, mesmo com o mesmo GUID
  if (settings->lightingData != guid) {
    auto values = *document_.find(owner);
    if (auto *edit = values.components.editInstance(instance); edit && &edit->type() == &scene::LightingSettings::descriptor) {
      static_cast<scene::LightingSettings &>(*edit).lightingData = guid;
      history_.applyValues(document_, owner, values);
    }
  }
  char text[200];
  std::snprintf(text, sizeof text, "Iluminação gerada · %u objetos, %zu página(s) de %u px, %.1f MB · %.1f s", data->stats.receivers,
                data->pages.size(), data->pages.empty() ? 0u : data->pages.front().width,
                static_cast<double>(data->gpuBytes()) / (1024.0 * 1024.0), static_cast<double>(data->bakeSeconds));
  state_.status = text;
  lightmapView_.lastResult = text;
  reportProblem(EditorConsoleSeverity::Info, std::string(text) + " · " + path);
  // O Generate Lighting da Unity assa as sondas de luz junto: os objetos que se
  // movem recebem a luz assada (lanternas, rebote) pelas sondas do PK-6, que
  // agora capturam a cena com os lightmaps no lugar.
  std::vector<runtime::SceneLightProbeGroup> groups;
  if (runtime::collectSceneLightProbeGroups(evaluatedEditorScene(), groups) && !groups.empty() && !probeBaking()) {
    pendingProbeBakeAfterLightmap_ = true;
  }
}

bool EditorSession::clearBakedLighting() {
  if (lightmapBake_) { state_.status = "Cancele o bake antes de limpar"; return false; }
  if (isPlaying() || playMirrorOpen_ || history_.isOpen()) { state_.status = "Pare o Play para limpar a iluminação"; return false; }
  EditorEntityId owner = kInvalidEntity;
  u64 instance = 0;
  const auto *settings = findLightingSettings(document_, &owner, &instance);
  if (!settings || !settings->lightingData.valid()) { state_.status = "A cena não tem iluminação assada"; return false; }
  auto values = *document_.find(owner);
  auto *edit = values.components.editInstance(instance);
  if (!edit || &edit->type() != &scene::LightingSettings::descriptor) return false;
  static_cast<scene::LightingSettings &>(*edit).lightingData = {};
  if (!history_.applyValues(document_, owner, values)) return false;
  state_.status = "Iluminação assada desvinculada; o arquivo continua no projeto";
  return true;
}

bool EditorSession::handleLightmapWidget(u32 widget) {
  using namespace lightmap_widget;
  switch (code(widget)) {
  case TabEnvironment: {
    // Volta ao Ambiente da cena, como abrir a janela Iluminação.
    state_.lightingTab = 0;
    u64 environment = 0;
    const auto owner = sceneLightingEnvironment(document_, &environment);
    state_.selection = owner != kInvalidEntity ? owner : document_.root();
    state_.expandedNative = environment;
    state_.componentSelection = state_.selection;
    state_.componentGroup = "Céu";
    return true;
  }
  case TabLightmaps: {
    // As Configurações de iluminação ficam abertas logo abaixo do bake.
    state_.lightingTab = 1;
    EditorEntityId owner = kInvalidEntity;
    u64 instance = 0;
    findLightingSettings(document_, &owner, &instance);
    state_.selection = owner != kInvalidEntity ? owner : document_.root();
    state_.expandedNative = instance;
    state_.componentSelection = state_.selection;
    state_.componentGroup = "Lightmaps";
    lightmapViewRevision_ = ~u64{0};
    return true;
  }
  case Generate: generateLighting(); return true;
  case Cancel: cancelLighting(); return true;
  case Clear: clearBakedLighting(); return true;
  // Contador Sem UV2: seleciona esses objetos na cena (recebem pelas sondas).
  case SelectWithoutUv: {
    const auto &ids = lightmapView_.withoutUv;
    if (ids.empty()) return true;
    state_.workspace = EditorWorkspace::Scene;
    selectEntities(std::vector<EditorEntityId>(ids.begin(), ids.end()));
    return true;
  }
  case CreateSettings: {
    if (isPlaying() || history_.isOpen()) { state_.status = "Finalize a edição antes"; return true; }
    EditorEntityId owner = kInvalidEntity;
    u64 instance = 0;
    if (!findLightingSettings(document_, &owner, &instance)) {
      auto values = *document_.find(document_.root());
      if (!values.components.add(scene::LightingSettings::descriptor) || !history_.applyValues(document_, document_.root(), values)) {
        state_.status = "A raiz não aceitou as Configurações de iluminação";
        return true;
      }
      findLightingSettings(document_, &owner, &instance);
      state_.status = "Configurações de iluminação criadas na raiz";
    }
    state_.selection = owner != kInvalidEntity ? owner : document_.root();
    state_.expandedNative = instance;
    state_.componentSelection = state_.selection;
    state_.componentGroup = "Lightmaps";
    lightmapViewRevision_ = ~u64{0};
    return true;
  }
  // Generate Lightmap UVs do Model Importer: Hard Angle em passos de 5°,
  // Pack Margin de 1 px. Pedem nova preparação, como os outros campos.
  case ImportUvs: state_.importLightmapUvs = !state_.importLightmapUvs; return true;
  case ImportHardAngleDown: state_.importLightmapHardAngle = static_cast<u8>(std::max(5, state_.importLightmapHardAngle - 5)); return true;
  case ImportHardAngleUp: state_.importLightmapHardAngle = static_cast<u8>(std::min(180, state_.importLightmapHardAngle + 5)); return true;
  case ImportMarginDown: state_.importLightmapPackMargin = static_cast<u8>(std::max(1, state_.importLightmapPackMargin - 1)); return true;
  case ImportMarginUp: state_.importLightmapPackMargin = static_cast<u8>(std::min(64, state_.importLightmapPackMargin + 1)); return true;
  default: return false;
  }
}

void EditorSession::refreshLightmaps() {
  pumpLightmapBake();
  // Um quadro depois do commit: o renderer já recebeu o lightmap novo antes
  // de a primeira sonda ser capturada.
  if (pendingProbeBakeAfterLightmap_ && lightmapRevision_ != lightmapProbeRevision_) {
    lightmapProbeRevision_ = lightmapRevision_;
  } else if (pendingProbeBakeAfterLightmap_) {
    pendingProbeBakeAfterLightmap_ = false;
    if (bakeLightProbes()) state_.status = "Iluminação gerada · assando as sondas de luz com os lightmaps…";
  }
  // O recurso em vigor é o das Configurações da cena (no Play, da cópia).
  const auto &graph = isPlaying() && playScene_.active() ? playScene_.document() : document_;
  EditorEntityId owner = kInvalidEntity;
  u64 instance = 0;
  const auto *settings = findLightingSettings(graph, &owner, &instance);
  const resources::AssetGuid wanted = settings && settings->bakedGlobalIllumination ? settings->lightingData : resources::AssetGuid{};
  if (wanted != activeLightmapGuid_) {
    activeLightmapGuid_ = wanted;
    activeLightmap_ = lightmapAsset(wanted);
    mapScene_.setLightmapLookup(activeLightmap_ ? std::make_shared<renderer::LightmapLookup>(activeLightmap_) : nullptr);
    ++lightmapRevision_;
    appearanceChanged_ = true;
    lightmapViewRevision_ = ~u64{0};
  }
  auto &view = lightmapView_;
  view.baking = lightmapBake_ != nullptr;
  if (lightmapBake_) {
    auto &progress = lightmapBake_->progress;
    view.progress = progress.fraction.load();
    view.stage = renderer::lightmapBakeStageName(static_cast<renderer::LightmapBakeStage>(progress.stage.load()));
    view.pass = progress.pass.load();
    view.passes = progress.passes.load();
    view.elapsedSeconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - lightmapBake_->start).count();
    view.remainingSeconds = view.progress > .08f ? view.elapsedSeconds * (1.0f - view.progress) / view.progress : -1.0f;
  }
  view.settingsExist = settings != nullptr;
  view.bakedGi = settings ? settings->bakedGlobalIllumination : true;
  view.settingsEntity = owner;
  view.settingsInstance = instance;
  view.hasData = activeLightmap_ != nullptr;
  view.missing = wanted.valid() && !activeLightmap_;
  if (const auto *record = wanted.valid() ? assets_.find(wanted) : nullptr) view.path = record->path;
  else view.path.clear();
  if (activeLightmap_) {
    const auto &data = *activeLightmap_;
    view.pages = static_cast<u32>(data.pages.size());
    view.pageSize = data.pages.empty() ? 0 : data.pages.front().width;
    view.entries = static_cast<u32>(data.entries.size());
    view.gpuBytes = data.gpuBytes();
    view.texels = data.stats.texels;
    view.rays = data.stats.rays;
    view.bakeSeconds = data.bakeSeconds;
    view.occupancy = data.stats.occupancy;
    view.compressed = data.settings.compress;
  } else {
    view.pages = view.pageSize = view.entries = 0;
    view.gpuBytes = view.texels = view.rays = 0;
    view.bakeSeconds = view.occupancy = 0;
  }
  // Contagem do que o próximo bake vai encontrar e se o recurso ficou velho:
  // só com a janela Iluminação aberta e quando o documento mudou.
  if (state_.workspace != EditorWorkspace::Lighting || isPlaying()) return;
  const u64 revision = document_.revision() ^ (lightmapRevision_ * 0x9E3779B97F4A7C15ull);
  if (revision == lightmapViewRevision_) return;
  lightmapViewRevision_ = revision;
  renderer::LightmapBakeScene scene;
  std::vector<u32> textures;
  std::string error;
  view.contributors = view.receivers = view.receiversWithoutUv = 0;
  view.withoutUv.clear();
  view.bakedLights = view.mixedLights = view.realtimeLights = 0;
  if (mapScene_.collectLightmapScene(evaluatedEditorScene(), bakeTextureBase_, scene, textures, error)) {
    view.contributors = static_cast<u32>(scene.instances.size());
    for (const auto &instanceValue : scene.instances) {
      if (!instanceValue.receives) continue;
      ++view.receivers;
      if (instanceValue.mesh < scene.meshes.size() && !scene.meshes[instanceValue.mesh].uv1Valid) {
        ++view.receiversWithoutUv;
        if (std::find(view.withoutUv.begin(), view.withoutUv.end(), instanceValue.objectId) == view.withoutUv.end())
          view.withoutUv.push_back(instanceValue.objectId);
      }
    }
  }
  std::vector<renderer::SceneLight> lights;
  if (extractLights(lights))
    for (const auto &light : lights) {
      if (light.lightmapMode == 2) ++view.bakedLights;
      else if (light.lightmapMode == 1) ++view.mixedLights;
      else ++view.realtimeLights;
    }
  bakeLightsFrom(lights, scene.lights);
  view.stale = activeLightmap_ && settings && activeLightmap_->sourceHash != renderer::lightmapSceneHash(scene, bakeSettingsFrom(*settings));
}

} // namespace ae::editor

#include "editor/editor_widgets.h"
// PK-6: Reflection Probe e Light Probe Group no editor.
//
// Fluxo da Unity reproduzido:
//  - Sonda de reflexo Assada: "Assar" captura a cena do ponto da sonda (só
//    objetos Static, Culling Mask e Clipping Planes da sonda), filtra no
//    aparelho e grava Iluminação/<nome>.aeprobe; o componente passa a apontar
//    para o recurso (um passo de desfazer). Assar de novo reescreve o mesmo
//    recurso, como a Unity reescreve o EXR da sonda.
//  - Grupo de sondas de luz: "Assar sondas" captura cada ponto de TODOS os
//    grupos acesos (o Generate Lighting da Unity bake as sondas da cena
//    inteira), projeta em SH L2 e grava Iluminação/<grupo>.aelightprobes.
//  - "Editar sondas" liga a seleção de pontos; adicionar, duplicar, apagar e
//    mover são passos de desfazer como qualquer edição do Inspector.
#include "editor/editor_session.h"
#include "editor/editor_import_transaction.h"
#include "core/sha256.h"
#include "runtime/scene_probes.h"
#include "scene/light_probe_group.h"
#include "scene/reflection_probe.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>

namespace ae::editor {

struct EditorSession::ProbeBakeJob {
  u32 kind = 0; // 0 sonda de reflexo, 1 sondas de luz
  EditorEntityId entity = 0;
  u64 instance = 0;
  renderer::SceneReflectionProbe reflection{};
  // Sondas de luz: um pedido por ponto, na ordem dos grupos.
  struct Group {
    EditorEntityId entity = 0;
    u64 instance = 0;
    std::string name;
    std::vector<u64> ids;
    std::vector<std::array<float, 3>> positions;
    std::vector<renderer::ShL2> sh;
    std::vector<renderer::ProbeSkyVisibility> sky; // G3c: vazio na captura da GPU
    u64 sourceHash = 0;
  };
  std::vector<Group> groups;
  std::vector<std::pair<u32, u32>> pointOf; // pedido -> (grupo, ponto)
  u64 firstRequest = 0;
  u32 total = 0, done = 0;
  bool failed = false;
  std::string error;
  std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
};

namespace {
std::string probeFileStem(std::string name, const char *fallback) {
  for (auto &c : name)
    if (c == '/' || c == '\\' || c == ':' || c == '"' || c == '.' || c == '*' || c == '?' || c == '<' || c == '>' || c == '|') c = '_';
  return name.empty() ? std::string(fallback) : name;
}
template <class T> const T *componentOf(const EditorDocument &document, EditorEntityId id, u64 instance) {
  const auto *entity = document.find(id);
  if (!entity) return nullptr;
  const auto *component = instance ? entity->components.findInstance(instance) : entity->components.find(T::descriptor);
  return component && &component->type() == &T::descriptor ? static_cast<const T *>(component) : nullptr;
}
} // namespace

void EditorSession::loadProbeAssets() {
  reflectionProbeTextures_.clear();
  lightProbeBakes_.clear();
  ++reflectionProbeTexturesRevision_;
  ++lightProbeBakesRevision_;
  if (files_.rootPath().empty()) return;
  for (const auto &record : assets_.records()) {
    if (record.type != resources::AssetType::ReflectionProbe && record.type != resources::AssetType::LightProbes) continue;
    std::filesystem::path path;
    std::vector<u8> bytes;
    std::string error;
    const usize limit = record.type == resources::AssetType::ReflectionProbe ? renderer::ReflectionProbeTexture::MaximumBytes
                                                                              : renderer::LightProbeBakeData::MaximumBytes;
    if (!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()), record.path, path) ||
        !EditorImportTransaction::read(path, bytes, limit)) {
      reportProblem(EditorConsoleSeverity::Warning, "Sonda assada ausente: " + record.path);
      continue;
    }
    if (record.type == resources::AssetType::ReflectionProbe) {
      auto texture = std::make_shared<renderer::ReflectionProbeTexture>();
      if (!renderer::ReflectionProbeTexture::deserialize(bytes, *texture, &error) || texture->guid != record.guid) {
        reportProblem(EditorConsoleSeverity::Warning, "Sonda de reflexo inválida: " + record.path + (error.empty() ? "" : " · " + error));
        continue;
      }
      reflectionProbeTextures_.push_back(std::move(texture));
    } else {
      renderer::LightProbeBakeData data;
      if (!renderer::LightProbeBakeData::deserialize(bytes, data, &error) || data.guidHigh != record.guid.high ||
          data.guidLow != record.guid.low) {
        reportProblem(EditorConsoleSeverity::Warning, "Sondas de luz inválidas: " + record.path + (error.empty() ? "" : " · " + error));
        continue;
      }
      lightProbeBakes_.push_back(std::move(data));
    }
  }
}

bool EditorSession::extractReflectionProbes(std::vector<renderer::SceneReflectionProbe> &out) const {
  const bool play = isPlaying() && playScene_.active();
  if (!runtime::collectSceneReflectionProbes(play ? playScene_.document() : evaluatedEditorScene(), out)) return false;
  if (play) playScene_.reflectionProbeRequests().apply(out);
  return true;
}

std::shared_ptr<const renderer::LightProbeSet> EditorSession::lightProbeSet() const {
  const bool play = isPlaying() && playScene_.active();
  std::vector<runtime::SceneLightProbeGroup> groups;
  if (!runtime::collectSceneLightProbeGroups(play ? playScene_.document() : evaluatedEditorScene(), groups)) return lightProbeSet_;
  // O conjunto só muda quando muda o que ele contém: quais grupos, quais
  // recursos e o "Remover anéis". Mover um grupo não move as sondas assadas.
  u64 key = 1469598103934665603ull ^ lightProbeBakesRevision_;
  const auto mix = [&](u64 v) { key = (key ^ v) * 1099511628211ull; };
  for (const auto &group : groups) {
    mix(group.baked.high);
    mix(group.baked.low);
    mix(group.removeRinging);
  }
  if (lightProbeSet_ && key == lightProbeSetKey_) return lightProbeSet_;
  std::vector<runtime::LightProbeBakeLookup> lookups;
  for (const auto &bake : lightProbeBakes_) lookups.push_back({{bake.guidHigh, bake.guidLow}, &bake});
  auto set = std::make_shared<renderer::LightProbeSet>();
  runtime::buildSceneLightProbeSet(groups, lookups, *set);
  lightProbeSet_ = std::move(set);
  lightProbeSetKey_ = key;
  return lightProbeSet_;
}

void EditorSession::takeProbeCaptureRequests(std::vector<EditorProbeCaptureRequest> &out) {
  out.assign(probeRequests_.begin(), probeRequests_.end());
  probeRequests_.clear();
}

bool EditorSession::probeBaking() const noexcept { return probeBake_ != nullptr; }

bool EditorSession::bakeReflectionProbe(EditorEntityId entity, u64 instance) {
  if (probeBake_) { state_.status = "Já existe um bake de sondas em andamento"; return false; }
  if (isPlaying() || playMirrorOpen_ || history_.isOpen()) { state_.status = "Pare o Play para assar a sonda"; return false; }
  if (files_.rootPath().empty()) { state_.status = "Abra um projeto para guardar a sonda assada"; return false; }
  const auto *probe = componentOf<scene::ReflectionProbe>(document_, entity, instance);
  if (!probe || !probe->valid()) { state_.status = "Selecione uma Sonda de reflexo"; return false; }
  std::vector<renderer::SceneReflectionProbe> probes;
  if (!runtime::collectSceneReflectionProbes(evaluatedEditorScene(), probes)) return false;
  const auto found = std::find_if(probes.begin(), probes.end(), [&](const auto &p) { return p.objectId == entity; });
  if (found == probes.end()) { state_.status = "Ative a sonda e o objeto dela para assar"; return false; }
  auto job = std::make_shared<ProbeBakeJob>();
  job->kind = 0;
  job->entity = entity;
  job->instance = probe->instanceId();
  job->reflection = *found;
  EditorProbeCaptureRequest request;
  request.id = nextProbeRequestId_++;
  request.kind = 0;
  request.objectId = entity;
  request.instance = probe->instanceId();
  std::copy(found->position, found->position + 3, request.position);
  request.resolution = found->resolution;
  request.nearClip = found->nearClip;
  request.farClip = found->farClip;
  request.cullingMask = found->cullingMask;
  request.staticOnly = true;
  request.solidBackground = found->solidBackground;
  std::copy(found->background, found->background + 3, request.background);
  request.hdr = found->hdr;
  job->firstRequest = request.id;
  job->total = 1;
  probeRequests_.push_back(request);
  probeBake_ = std::move(job);
  state_.status = "Assando sonda de reflexo…";
  return true;
}

bool EditorSession::bakeLightProbes() {
  if (probeBake_) { state_.status = "Já existe um bake de sondas em andamento"; return false; }
  if (isPlaying() || playMirrorOpen_ || history_.isOpen()) { state_.status = "Pare o Play para assar as sondas"; return false; }
  if (files_.rootPath().empty()) { state_.status = "Abra um projeto para guardar as sondas"; return false; }
  std::vector<runtime::SceneLightProbeGroup> groups;
  if (!runtime::collectSceneLightProbeGroups(evaluatedEditorScene(), groups) || groups.empty()) {
    state_.status = "Nenhum Grupo de sondas de luz ativo na cena";
    return false;
  }
  auto job = std::make_shared<ProbeBakeJob>();
  job->kind = 1;
  job->firstRequest = nextProbeRequestId_;
  usize points = 0;
  for (const auto &group : groups) points += group.positions.size();
  if (points == 0) { state_.status = "Os grupos não têm sondas"; return false; }
  if (points > renderer::MaximumLightProbes) { state_.status = "Mais de 2048 sondas de luz na cena"; return false; }
  for (u32 g = 0; g < groups.size(); ++g) {
    const auto &source = groups[g];
    ProbeBakeJob::Group group;
    group.entity = static_cast<EditorEntityId>(source.objectId);
    group.instance = source.instance;
    const auto *entity = document_.find(group.entity);
    group.name = entity ? std::string(entity->name) : std::string("Sondas de luz");
    group.ids = source.pointIds;
    group.positions = source.positions;
    group.sh.assign(source.positions.size(), renderer::ShL2{});
    group.sourceHash = source.positionsHash();
    for (u32 p = 0; p < source.positions.size(); ++p) {
      EditorProbeCaptureRequest request;
      request.id = nextProbeRequestId_++;
      request.kind = 1;
      std::copy(source.positions[p].begin(), source.positions[p].end(), request.position);
      request.resolution = LightProbeCaptureResolution;
      request.nearClip = 0.05f;
      request.farClip = 1000;
      request.staticOnly = true;
      request.hdr = true;
      probeRequests_.push_back(request);
      job->pointOf.push_back({g, p});
    }
    job->groups.push_back(std::move(group));
  }
  job->total = static_cast<u32>(job->pointOf.size());
  probeBake_ = std::move(job);
  char text[96];
  std::snprintf(text, sizeof text, "Assando %u sondas de luz…", probeBake_->total);
  state_.status = text;
  return true;
}

void EditorSession::deliverProbeCaptureResult(EditorProbeCaptureResult result) {
  if (!probeBake_) return;
  auto &job = *probeBake_;
  if (result.id < job.firstRequest || result.id >= job.firstRequest + job.total) return;
  ++job.done;
  if (!result.success) {
    job.failed = true;
    if (job.error.empty()) job.error = result.error.empty() ? "captura recusada pelo renderer" : result.error;
  } else if (job.kind == 1) {
    const auto [g, p] = job.pointOf[static_cast<usize>(result.id - job.firstRequest)];
    renderer::ShL2 sh;
    if (renderer::projectCubeToIrradiance(result.faces, sh)) job.groups[g].sh[p] = sh;
    else { job.failed = true; job.error = "faces da captura inválidas"; }
  }
  if (job.kind == 0 && result.success) {
    auto finished = std::move(probeBake_);
    commitReflectionBake(*finished, std::move(result.texture));
    return;
  }
  if (job.done < job.total) return;
  auto finished = std::move(probeBake_);
  if (finished->failed) {
    state_.status = "Bake de sondas: " + finished->error;
    reportProblem(EditorConsoleSeverity::Error, state_.status);
    return;
  }
  if (finished->kind == 1) commitLightProbeBake(*finished);
}

void EditorSession::probeResourcePath(resources::AssetType type, const resources::AssetGuid &current, std::string_view name,
                                     std::string_view extension, resources::AssetGuid &guid, std::string &path) {
  const auto *existing = current.valid() ? assets_.find(current) : nullptr;
  if (existing && existing->type != type) existing = nullptr;
  if (existing) {
    path = existing->path;
    guid = existing->guid;
  } else {
    const auto stem = probeFileStem(std::string(name), type == resources::AssetType::ReflectionProbe ? "Sonda" : "Sondas de luz");
    path = "Iluminação/" + stem + std::string(extension);
    for (u32 n = 2; assets_.findByPath(path) || files_.exists(path); ++n)
      path = "Iluminação/" + stem + " " + std::to_string(n) + std::string(extension);
    guid = resources::assetGuidFromSeed(std::string(resources::assetTypeName(type)) + ":" + path + ":" +
                                        std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ":" +
                                        std::to_string(++importInstanceCounter_));
  }
}

bool EditorSession::commitProbeFile(resources::AssetType type, const resources::AssetGuid &guid, const std::string &path,
                                    const std::vector<u8> &bytes, usize maximum) {
  std::filesystem::path absolute;
  if (!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()), path, absolute)) {
    state_.status = "Destino da sonda inválido";
    return false;
  }
  std::string expected;
  {
    std::vector<u8> previous;
    std::error_code e;
    if (std::filesystem::exists(absolute, e) && EditorImportTransaction::read(absolute, previous, maximum)) expected = Sha256::hex(previous);
  }
  const auto *existing = assets_.find(guid);
  resources::AssetRecord record = existing ? *existing : resources::AssetRecord{};
  record.guid = guid;
  record.type = type;
  record.path = path;
  record.contentHash = Sha256::hex({bytes.data(), bytes.size()});
  auto next = assets_;
  if (existing) next.remove(guid);
  if (!next.add(record)) { state_.status = "O registro recusou a sonda assada"; return false; }
  std::string diagnostic;
  EditorImportTransaction transaction(files_.rootPath());
  if (!transaction.begin(path, expected, diagnostic)) { state_.status = diagnostic; return false; }
  if (!transaction.commit({bytes.data(), bytes.size()}, next.serialize())) {
    state_.status = transaction.rollback() ? "Gravação recusada; projeto preservado" : "Recuperação pendente no journal";
    return false;
  }
  assets_ = std::move(next);
  assetRegistryDirty_ = true;
  files_.rebuildTree();
  return true;
}

bool EditorSession::commitReflectionBake(ProbeBakeJob &job, renderer::ReflectionProbeTexture texture) {
  const auto *entity = document_.find(job.entity);
  const auto *current = componentOf<scene::ReflectionProbe>(document_, job.entity, job.instance);
  if (!entity || !current) { state_.status = "A sonda foi removida durante o bake; resultado descartado"; return false; }
  if (isPlaying() || playMirrorOpen_ || history_.isOpen() || files_.rootPath().empty()) {
    state_.status = "Bake concluído com o editor ocupado; asse de novo";
    return false;
  }
  resources::AssetGuid guid{};
  std::string path;
  probeResourcePath(resources::AssetType::ReflectionProbe, current->bakedTexture, entity->name, ".aeprobe", guid, path);
  texture.guid = guid;
  texture.sourceHash = job.reflection.captureKey;
  texture.bakeSeconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - job.start).count();
  const auto bytes = texture.serialize();
  if (!commitProbeFile(resources::AssetType::ReflectionProbe, guid, path, bytes, renderer::ReflectionProbeTexture::MaximumBytes))
    return false;
  auto shared = std::make_shared<renderer::ReflectionProbeTexture>(std::move(texture));
  const auto found = std::find_if(reflectionProbeTextures_.begin(), reflectionProbeTextures_.end(),
                                  [&](const auto &t) { return t && t->guid == guid; });
  if (found != reflectionProbeTextures_.end()) *found = shared;
  else reflectionProbeTextures_.push_back(shared);
  ++reflectionProbeTexturesRevision_;
  if (current->bakedTexture != guid) {
    auto values = *entity;
    auto *edit = values.components.editInstance(job.instance);
    if (edit && &edit->type() == &scene::ReflectionProbe::descriptor) {
      static_cast<scene::ReflectionProbe &>(*edit).bakedTexture = guid;
      history_.applyValues(document_, entity->id, values);
    }
  }
  char text[160];
  std::snprintf(text, sizeof text, "Sonda assada · %u px · %s · %.1f s", job.reflection.resolution,
                job.reflection.hdr ? "HDR" : "LDR", static_cast<double>(shared->bakeSeconds));
  state_.status = text;
  reportProblem(EditorConsoleSeverity::Info, std::string(text) + " · " + path);
  return true;
}

// G3c: as sondas assadas no Gerar iluminação (CPU, mesmas luzes do lightmap)
// vão para o mesmo recurso de cada grupo que o bake por captura usaria.
bool EditorSession::commitLightProbesFromLightmap(const std::vector<runtime::SceneLightProbeGroup> &groups,
                                                  const renderer::LightmapData &data) {
  ProbeBakeJob job;
  job.kind = 1;
  job.start = std::chrono::steady_clock::now() - std::chrono::milliseconds(static_cast<i64>(data.bakeSeconds * 1000));
  usize at = 0;
  for (const auto &source : groups) {
    ProbeBakeJob::Group group;
    group.entity = static_cast<EditorEntityId>(source.objectId);
    group.instance = source.instance;
    const auto *entity = document_.find(group.entity);
    group.name = entity ? std::string(entity->name) : std::string("Sondas de luz");
    group.ids = source.pointIds;
    group.positions = source.positions;
    group.sourceHash = source.positionsHash();
    for (usize p = 0; p < source.positions.size(); ++p, ++at) {
      if (at >= data.probeSh.size() || at >= data.probeSky.size()) return false;
      group.sh.push_back(data.probeSh[at]);
      group.sky.push_back(data.probeSky[at]);
    }
    job.groups.push_back(std::move(group));
  }
  return commitLightProbeBake(job);
}

bool EditorSession::commitLightProbeBake(ProbeBakeJob &job) {
  if (isPlaying() || playMirrorOpen_ || history_.isOpen() || files_.rootPath().empty()) {
    state_.status = "Bake concluído com o editor ocupado; asse de novo";
    return false;
  }
  const float seconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - job.start).count();
  u32 written = 0, points = 0;
  for (auto &group : job.groups) {
    const auto *entity = document_.find(group.entity);
    const auto *current = componentOf<scene::LightProbeGroup>(document_, group.entity, group.instance);
    if (!entity || !current) continue;
    resources::AssetGuid guid{};
    std::string path;
    probeResourcePath(resources::AssetType::LightProbes, current->bakedData, group.name, ".aelightprobes", guid, path);
    renderer::LightProbeBakeData data;
    data.guidHigh = guid.high;
    data.guidLow = guid.low;
    data.sourceHash = group.sourceHash;
    data.bakeSeconds = seconds;
    data.pointIds = group.ids;
    data.positions = group.positions;
    data.sh = group.sh;
    data.skyVisibility = group.sky;
    const auto bytes = data.serialize();
    if (!commitProbeFile(resources::AssetType::LightProbes, guid, path, bytes, renderer::LightProbeBakeData::MaximumBytes)) return false;
    const auto found = std::find_if(lightProbeBakes_.begin(), lightProbeBakes_.end(),
                                    [&](const auto &b) { return b.guidHigh == guid.high && b.guidLow == guid.low; });
    if (found != lightProbeBakes_.end()) *found = std::move(data);
    else lightProbeBakes_.push_back(std::move(data));
    if (current->bakedData != guid) {
      auto values = *entity;
      auto *edit = values.components.editInstance(group.instance);
      if (edit && &edit->type() == &scene::LightProbeGroup::descriptor) {
        static_cast<scene::LightProbeGroup &>(*edit).bakedData = guid;
        history_.applyValues(document_, entity->id, values);
      }
    }
    ++written;
    points += static_cast<u32>(group.positions.size());
  }
  ++lightProbeBakesRevision_;
  char text[160];
  std::snprintf(text, sizeof text, "Sondas de luz assadas · %u sondas em %u grupos · %.1f s", points, written, static_cast<double>(seconds));
  state_.status = text;
  reportProblem(EditorConsoleSeverity::Info, text);
  return true;
}

bool EditorSession::clearProbeBake(EditorEntityId entity, u64 instance) {
  if (isPlaying() || playMirrorOpen_ || history_.isOpen()) return false;
  const auto *object = document_.find(entity);
  const auto *component = object ? object->components.findInstance(instance) : nullptr;
  if (!component) return false;
  auto values = *object;
  auto *edit = values.components.editInstance(instance);
  if (!edit) return false;
  if (&edit->type() == &scene::ReflectionProbe::descriptor) static_cast<scene::ReflectionProbe &>(*edit).bakedTexture = {};
  else if (&edit->type() == &scene::LightProbeGroup::descriptor) static_cast<scene::LightProbeGroup &>(*edit).bakedData = {};
  else return false;
  if (!history_.applyValues(document_, entity, values)) return false;
  state_.status = "Bake desvinculado; o arquivo continua no projeto";
  return true;
}

bool EditorSession::editLightProbePoints(EditorEntityId entity, u64 instance, u32 operation) {
  if (isPlaying() || playMirrorOpen_ || history_.isOpen()) { state_.status = "Saia do Play para editar as sondas"; return false; }
  const auto *object = document_.find(entity);
  if (!object) return false;
  auto values = *object;
  auto *edit = values.components.editInstance(instance);
  if (!edit || &edit->type() != &scene::LightProbeGroup::descriptor) return false;
  auto &group = static_cast<scene::LightProbeGroup &>(*edit);
  auto &view = probeView_;
  const auto *selected = group.point(view.selectedPoint);
  float position[3]{0, 0, 0};
  if (selected) std::copy(selected->position, selected->position + 3, position);
  u64 created = 0;
  switch (operation) {
  case 0: { // Adicionar: um metro ao lado da selecionada, ou na origem do grupo
    if (selected) position[0] += 1;
    created = group.addPoint(position);
    if (!created) { state_.status = "O grupo já tem 512 sondas"; return false; }
    view.selectedPoint = created;
    break;
  }
  case 1: { // Duplicar a selecionada, meio metro deslocada
    if (!selected) return false;
    position[0] += .5f;
    position[2] += .5f;
    created = group.addPoint(position);
    if (!created) { state_.status = "O grupo já tem 512 sondas"; return false; }
    view.selectedPoint = created;
    break;
  }
  case 2: { // Apagar a selecionada
    if (!selected) return false;
    const auto at = std::find_if(group.points.begin(), group.points.end(), [&](const auto &p) { return p.id == view.selectedPoint; });
    const usize index = static_cast<usize>(at - group.points.begin());
    group.removePoint(view.selectedPoint);
    view.selectedPoint = group.points.empty() ? 0 : group.points[std::min(index, group.points.size() - 1)].id;
    break;
  }
  default: return false;
  }
  return history_.applyValues(document_, entity, values);
}

void EditorSession::refreshProbeView() {
  auto &view = probeView_;
  const auto &document = *state_.document;
  const auto *entity = document.find(state_.selection);
  const scene::ComponentValue *component = entity ? entity->components.findInstance(state_.expandedNative) : nullptr;
  if (component && &component->type() != &scene::ReflectionProbe::descriptor && &component->type() != &scene::LightProbeGroup::descriptor)
    component = nullptr;
  if (!component && entity) {
    component = entity->components.find(scene::ReflectionProbe::descriptor);
    if (!component) component = entity->components.find(scene::LightProbeGroup::descriptor);
  }
  const bool sameTarget = component && view.entity == (entity ? entity->id : 0) && view.instance == component->instanceId();
  const bool editing = sameTarget && view.editing;
  const u64 selected = sameTarget ? view.selectedPoint : 0;
  view = {};
  view.stats = probeRendererStats_;
  if (!component) return;
  view.entity = entity->id;
  view.instance = component->instanceId();
  view.baking = probeBake_ != nullptr;
  if (probeBake_) view.progress = probeBake_->total ? static_cast<float>(probeBake_->done) / static_cast<float>(probeBake_->total) : 0;
  char text[200];
  if (&component->type() == &scene::ReflectionProbe::descriptor) {
    const auto &probe = static_cast<const scene::ReflectionProbe &>(*component);
    view.reflection = true;
    if (probe.realtime()) {
      static constexpr const char *refresh[]{"captura ao acordar", "recaptura todo quadro", "captura por script"};
      std::snprintf(text, sizeof text, "Tempo real · %u px · %s", probe.resolution, refresh[std::min<u32>(probe.refreshMode, 2)]);
      view.title = text;
      view.hasBake = true;
      view.detail = isPlaying() ? "Atualiza no aparelho conforme o modo" : "No editor recaptura quando a sonda muda";
    } else {
      const auto found = std::find_if(reflectionProbeTextures_.begin(), reflectionProbeTextures_.end(),
                                      [&](const auto &t) { return t && t->guid == probe.bakedTexture; });
      view.hasBake = probe.bakedTexture.valid() && found != reflectionProbeTextures_.end();
      view.missing = probe.bakedTexture.valid() && found == reflectionProbeTextures_.end();
      if (view.hasBake) {
        std::vector<renderer::SceneReflectionProbe> probes;
        if (runtime::collectSceneReflectionProbes(document, probes))
          for (const auto &p : probes) if (p.objectId == entity->id) view.stale = p.captureKey != (*found)->sourceHash;
        view.bakeSeconds = (*found)->bakeSeconds;
        std::snprintf(text, sizeof text, "Assada · %u px · %s", probe.resolution, (*found)->hdr ? "HDR" : "LDR");
        view.title = text;
        view.detail = view.stale ? "Desatualizada: a sonda mudou desde o bake" : "Os objetos dentro da caixa refletem esta captura";
      } else {
        view.title = view.missing ? "Recurso .aeprobe ausente" : "Sem bake";
        view.detail = "Assar captura os objetos Static vistos desta sonda";
      }
    }
  } else {
    const auto &group = static_cast<const scene::LightProbeGroup &>(*component);
    view.lightGroup = true;
    view.editing = editing;
    view.pointCount = static_cast<u32>(group.points.size());
    view.selectedPoint = group.point(selected) ? selected : (group.points.empty() ? 0 : group.points.front().id);
    for (u32 i = 0; i < group.points.size(); ++i) if (group.points[i].id == view.selectedPoint) view.selectedIndex = i;
    const auto found = std::find_if(lightProbeBakes_.begin(), lightProbeBakes_.end(), [&](const auto &b) {
      return b.guidHigh == group.bakedData.high && b.guidLow == group.bakedData.low;
    });
    view.hasBake = group.bakedData.valid() && found != lightProbeBakes_.end();
    view.missing = group.bakedData.valid() && found == lightProbeBakes_.end();
    if (view.hasBake) {
      std::vector<runtime::SceneLightProbeGroup> groups;
      if (runtime::collectSceneLightProbeGroups(document, groups))
        for (const auto &g : groups) if (g.objectId == entity->id) view.stale = g.positionsHash() != found->sourceHash;
      view.bakeSeconds = found->bakeSeconds;
      std::snprintf(text, sizeof text, "%u sondas assadas · %u tetraedros na cena", static_cast<u32>(found->positions.size()),
                    probeRendererStats_.tetrahedra);
      view.title = text;
      view.detail = view.stale ? "Desatualizado: os pontos mudaram desde o bake" : "Objetos com Sondas de luz = Misturar usam esta luz";
    } else {
      std::snprintf(text, sizeof text, "%u sondas · sem bake", view.pointCount);
      view.title = text;
      view.detail = view.missing ? "Recurso .aelightprobes ausente; asse de novo" : "Assar captura sol, céu e rebate em cada ponto";
    }
  }
  if (view.baking) {
    std::snprintf(text, sizeof text, "Assando · %u de %u", probeBake_->done, probeBake_->total);
    view.title = text;
  }
}

bool EditorSession::handleProbeWidget(u32 widget) {
  namespace w = probe_widget;
  auto &view = probeView_;
  switch (w::code(widget)) {
  case w::BakeReflection: return bakeReflectionProbe(static_cast<EditorEntityId>(view.entity), view.instance);
  case w::BakeLight: return bakeLightProbes();
  case w::Clear: return clearProbeBake(static_cast<EditorEntityId>(view.entity), view.instance);
  case w::EditPoints: view.editing = !view.editing; return true;
  case w::AddPoint: return editLightProbePoints(static_cast<EditorEntityId>(view.entity), view.instance, 0);
  case w::DuplicatePoint: return editLightProbePoints(static_cast<EditorEntityId>(view.entity), view.instance, 1);
  case w::RemovePoint: return editLightProbePoints(static_cast<EditorEntityId>(view.entity), view.instance, 2);
  case w::PreviousPoint:
  case w::NextPoint: {
    const auto *group = componentOf<scene::LightProbeGroup>(document_, static_cast<EditorEntityId>(view.entity), view.instance);
    if (!group || group->points.empty()) return true;
    const i64 next = static_cast<i64>(view.selectedIndex) + (w::code(widget) == w::NextPoint ? 1 : -1);
    const usize index = static_cast<usize>(std::clamp<i64>(next, 0, static_cast<i64>(group->points.size()) - 1));
    view.selectedPoint = group->points[index].id;
    view.selectedIndex = static_cast<u32>(index);
    return true;
  }
  default: return false;
  }
}

} // namespace ae::editor

namespace ae::editor {
bool EditorSession::pickLightProbePoint(ui::UiPoint at) {
  auto &view = probeView_;
  if (!view.editing || state_.workspace != EditorWorkspace::Scene || isPlaying()) return false;
  const auto *group = componentOf<scene::LightProbeGroup>(document_, static_cast<EditorEntityId>(view.entity), view.instance);
  float world[16];
  if (!group || !editorWorldMatrix(document_, static_cast<EditorEntityId>(view.entity), world)) return false;
  u64 best = 0;
  float bestDistance = 28.0f * 28.0f;
  for (const auto &p : group->points) {
    float centre[3];
    for (u32 k = 0; k < 3; ++k) centre[k] = world[k] * p.position[0] + world[4 + k] * p.position[1] + world[8 + k] * p.position[2] + world[12 + k];
    const auto projected = projectWorldToScreen(view_, centre);
    if (!projected.valid) continue;
    const float dx = projected.screen.x - at.x, dy = projected.screen.y - at.y;
    if (dx * dx + dy * dy < bestDistance) { bestDistance = dx * dx + dy * dy; best = p.id; }
  }
  if (!best) return false;
  view.selectedPoint = best;
  state_.status = "Sonda selecionada; arraste as alças para mover";
  return true;
}
} // namespace ae::editor

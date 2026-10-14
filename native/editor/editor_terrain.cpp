#include "core/sha256.h"
#include "editor/editor_import_transaction.h"
#include "editor/editor_screen.h"
#include "editor/editor_session.h"
#include "resources/terrain_textures.generated.h"
#include "scene/collider.h"
#include "scene/import_link.h"
#include "scene/physics_body.h"
#include <atomic>
#include <cmath>
#include <map>
#include <thread>

namespace ae::editor {
namespace {
std::array<std::vector<u8>, 4> textures() {
  using namespace resources;
  return {{{std::begin(terrainTexture0), std::end(terrainTexture0)},
           {std::begin(terrainTexture1), std::end(terrainTexture1)},
           {std::begin(terrainTexture2), std::end(terrainTexture2)},
           {std::begin(terrainTexture3), std::end(terrainTexture3)}}};
}
double number(const EditorScreenState &s, u32 i) {
  const auto &r = s.terrainDraft;
  const auto &b = s.terrainBrush;
  const double values[]{double(r.seed),
                        r.size,
                        double(r.cells),
                        r.height,
                        r.base,
                        r.frequency,
                        double(r.octaves),
                        double(r.verticalCells),
                        r.textureScale,
                        b.radius,
                        b.strength,
                        b.x,
                        b.y,
                        b.z};
  return i < 14 ? values[i] : 0;
}
} // namespace
struct EditorSession::TerrainJob {
  resources::TerrainRecipe recipe;
  resources::TerrainMesh mesh;
  resources::GltfImport model;
  std::vector<u8> bytes;
  std::string error, project;
  u64 epoch = 0, revision = 0;
  EditorEntityId target = 0;
  std::atomic<bool> cancel{false}, ready{false};
  std::atomic<float> progress{0};
};
bool EditorSession::openTerrain(EditorEntityId target) {
  if (isPlaying() || state_.terrainBusy)
    return false;
  if (target) {
    auto id = target;
    const scene::ImportLink *link = nullptr;
    while (const auto *e = document_.find(id)) {
      link = scene::importLink(e->components);
      if (link && link->root)
        break;
      id = e->parent;
    }
    const auto *record = link ? assets_.find(link->source) : nullptr;
    std::vector<u8> bytes;
    std::string error;
    std::filesystem::path absolute;
    if (!record ||
        !EditorImportTransaction::safePath(
            EditorImportTransaction::fromUtf8(files_.rootPath()), record->path,
            absolute) ||
        !EditorImportTransaction::read(absolute, bytes, 32u << 20) ||
        !resources::terrainRecipeFromGlb(bytes, state_.terrainDraft, error)) {
      state_.status = error.empty()
                          ? "Selecione a raiz ou um chunk do terreno Astra"
                          : error;
      return false;
    }
    state_.terrainTarget = id;
    terrainSource_ = record->guid;
    state_.terrainMessage = "Receita carregada · " +
                            std::to_string(state_.terrainDraft.strokes.size()) +
                            " operações";
  } else {
    state_.terrainDraft = {};
    state_.terrainTarget = 0;
    terrainSource_ = {};
    state_.terrainMessage = "Novo terreno · configure e gere";
  }
  state_.terrainOpen = true;
  state_.terrainBrushArmed = false;
  state_.creationMenu = false;
  state_.workspace = EditorWorkspace::Scene;
  state_.terrainTab = 0;
  state_.terrainPage = 0;
  return true;
}
bool EditorSession::setTerrainNumber(u32 field, double value) {
  if (!state_.terrainOpen || state_.terrainBusy || !std::isfinite(value) ||
      field >= 14)
    return false;
  if ((field == 0 || field == 2 || field == 6 || field == 7) &&
      (value < 0 || value > 4294967295.0 || std::floor(value) != value))
    return false;
  auto r = state_.terrainDraft;
  auto b = state_.terrainBrush;
  const float f = float(value);
  switch (field) {
  case 0:
    r.seed = u32(value);
    break;
  case 1:
    r.size = f;
    break;
  case 2:
    r.cells = u32(value);
    break;
  case 3:
    r.height = f;
    break;
  case 4:
    r.base = f;
    break;
  case 5:
    r.frequency = f;
    break;
  case 6:
    r.octaves = u32(value);
    break;
  case 7:
    r.verticalCells = u32(value);
    break;
  case 8:
    r.textureScale = f;
    break;
  case 9:
    b.radius = f;
    break;
  case 10:
    b.strength = f;
    break;
  case 11:
    b.x = f;
    break;
  case 12:
    b.y = f;
    break;
  case 13:
    b.z = f;
    break;
  default:
    return false;
  }
  std::string error;
  if (!resources::validTerrainRecipe(r, error) || b.radius < .1f ||
      b.radius > 2048 || b.strength < 0 || b.strength > 128 ||
      std::abs(b.x) > 4096 || std::abs(b.z) > 4096 || std::abs(b.y) > 2048)
    return false;
  state_.terrainDraft = std::move(r);
  state_.terrainBrush = b;
  state_.terrainMessage = "Rascunho alterado · Gerar e aplicar para atualizar";
  return true;
}
bool EditorSession::publishTerrain(const resources::TerrainRecipe &r,
                                   const resources::TerrainMesh &mesh,
                                   const std::vector<u8> &bytes,
                                   const resources::GltfImport &model,
                                   std::string &error) {
  if (isPlaying() || history_.isOpen() || files_.rootPath().empty()) {
    error = "Abra um projeto e finalize a edição antes de gerar.";
    return false;
  }
  if (state_.terrainTarget && !document_.find(state_.terrainTarget)) {
    error = "O terreno foi removido; abra um novo rascunho.";
    return false;
  }
  // Immutable, content-addressed revisions make scene Undo restore exactly the
  // previous geometry and recipe. A failed publisher cannot alter the old
  // source.
  const auto hash = Sha256::hex(bytes);
  const auto path = "Terrenos/terrain-" + hash.substr(0, 24) + ".glb";
  ModelImportReport report;
  const auto *known = assets_.findByPath(path);
  if (!known) {
    if (!commitModelImport(bytes, model, path, "", report)) {
      error = report.diagnostic;
      return false;
    }
  }
  const auto *record = assets_.findByPath(path);
  if (!record) {
    error = "Fonte não registrada.";
    return false;
  }
  usize before = 0, total = 0;
  const ImportedSource *source = nullptr;
  for (const auto &entry : importedSources_) {
    if (entry.guid == record->guid)
      source = &entry;
    else if (!source)
      before += entry.draws.size();
    total += entry.draws.size();
  }
  if (!source || total > mapScene_.assetCount() ||
      source->draws.size() != mesh.parts.size()) {
    error = "Biblioteca de terreno incompleta.";
    return false;
  }
  const auto offset = mapScene_.assetCount() - u32(total) + u32(before);
  if (document_.entityCount() + mesh.parts.size() + 1 >
      EditorDocument::kMaximumEntities) {
    error = "Limite de objetos da cena atingido.";
    return false;
  }
  history_.begin("Gerar / editar terreno");
  auto root = state_.terrainTarget;
  if (!root)
    root = history_.createEntity(document_, document_.root(),
                                 EditorEntityKind::Folder, "Terreno");
  const auto fail = [&](const char *message) {
    history_.cancel(document_);
    error = message;
    return false;
  };
  if (!root)
    return fail("Falha ao criar terreno.");
  auto rootValue = *document_.find(root);
  const auto *oldLink = scene::importLink(rootValue.components);
  const auto oldSource = oldLink ? oldLink->source : resources::AssetGuid{};
  std::vector<EditorEntityId> children;
  document_.collectSubtree(root, children);
  std::map<std::string, EditorEntityId> existing;
  for (auto id : children) {
    const auto *e = document_.find(id);
    const auto *link = e ? scene::importLink(e->components) : nullptr;
    if (id != root && link && link->source == oldSource) {
      if (!existing.emplace(link->baseName, id).second)
        return fail(
            "Chunk duplicado: remova a cópia do grupo antes de regenerar.");
    }
  }
  auto *link = scene::editImportLink(rootValue.components);
  if (!link)
    return fail("Vínculo de terreno indisponível.");
  link->source = record->guid;
  link->instance = resources::assetGuidFromSeed(
      "terrain-instance:" + std::to_string(root) + ":" + hash);
  link->root = true;
  link->unlinked = true;
  link->revision = 1;
  link->baseName = rootValue.name;
  if (!history_.applyValues(document_, root, rootValue))
    return fail("Raiz do terreno inválida.");
  for (u32 i = 0; i < mesh.parts.size(); ++i) {
    const auto &part = mesh.parts[i];
    const float origin[3]{};
    const auto name = "Chunk " + std::to_string(part.x) + ":" +
                      std::to_string(part.y) + ":" + std::to_string(part.z) +
                      " / " + std::to_string(part.layer);
    EditorAssetInstantiation options;
    options.name = name;
    const auto found = existing.find(name);
    const bool reused = found != existing.end();
    const auto id = reused ? found->second
                           : instantiateAssetInTransaction(offset + i, root,
                                                           origin, &options);
    if (reused)
      existing.erase(found);
    if (!id)
      return fail("Falha ao criar chunk.");
    auto value = *document_.find(id);
    if (!reused)
      value.transform = {};
    value.isStatic = true;
    auto *render = editMeshRenderer(value);
    if (!render)
      return fail("Chunk sem MeshRenderer.");
    render->mesh = offset + i + 1;
    render->asset = mapScene_.assetGuid(offset + i);
    // Instance material overrides and user attachments survive regeneration.
    auto *childLink = scene::editImportLink(value.components);
    if (!childLink)
      return fail("Vínculo de chunk indisponível.");
    childLink->source = record->guid;
    childLink->instance = link->instance;
    childLink->unlinked = true;
    childLink->node =
        resources::assetGuidFromSeed(hash + ":" + std::to_string(i));
    childLink->baseName = name;
    if (r.collision) {
      auto *body = static_cast<scene::PhysicsBody *>(
          value.components.edit(scene::PhysicsBody::descriptor));
      auto *collider = static_cast<scene::Collider *>(
          value.components.edit(scene::Collider::descriptor));
      if (!body || !collider)
        return fail("Colisor indisponível.");
      body->motion = scene::BodyMotion::Static;
      collider->shape = scene::ColliderShape::Mesh;
      collider->convex = false;
    } else {
      value.components.remove(scene::Collider::descriptor);
      value.components.remove(scene::PhysicsBody::descriptor);
    }
    if (!history_.applyValues(document_, id, value))
      return fail("Chunk inválido.");
  }
  for (const auto &[name, id] : existing) {
    (void)name;
    std::vector<EditorEntityId> descendants;
    document_.collectSubtree(id, descendants);
    for (auto attached : descendants)
      if (attached != id) {
        const auto *child = document_.find(attached);
        if (child && child->parent == id &&
            !history_.reparentKeepingWorld(document_, attached, root))
          return fail("Anexo do chunk não pôde ser preservado.");
      }
    if (!history_.destroyEntity(document_, id))
      return fail("Não foi possível remover chunk vazio.");
  }
  history_.end();
  state_.terrainTarget = root;
  terrainSource_ = record->guid;
  state_.terrainDraft = r;
  setSelection(root);
  if (!oldSource.valid())
    frameSelection();
  state_.terrainMessage = std::to_string(mesh.chunks) + " chunks · " +
                          std::to_string(mesh.triangles) + " tris · " +
                          std::to_string(r.strokes.size()) + " operações";
  state_.status = "Terreno aplicado · salvar a cena para preservar os objetos";
  error.clear();
  return true;
}
bool EditorSession::applyTerrain(const resources::TerrainRecipe &r,
                                 std::string &error) {
  resources::TerrainMesh mesh;
  std::vector<u8> bytes;
  resources::GltfImport model;
  if (!resources::generateTerrain(r, mesh, error) ||
      !resources::writeTerrainGlb(r, mesh, textures(), bytes, error))
    return false;
  if (!resources::importGlb(bytes, {}, {}, model)) {
    error = model.diagnostic;
    return false;
  }
  return publishTerrain(r, mesh, bytes, model, error);
}
void EditorSession::startTerrainJob() {
  if (state_.terrainBusy || isPlaying() || history_.isOpen())
    return;
  std::string error;
  if (!resources::validTerrainRecipe(state_.terrainDraft, error)) {
    state_.terrainMessage = error;
    return;
  }
  auto job = std::make_shared<TerrainJob>();
  job->recipe = state_.terrainDraft;
  job->target = state_.terrainTarget;
  job->epoch = sceneEpoch_;
  job->revision = document_.revision();
  job->project = files_.rootPath();
  terrainJob_ = job;
  state_.terrainBusy = true;
  state_.terrainProgress = 0;
  state_.terrainMessage = "Gerando malha…";
  std::thread([job] {
    const auto progress = [job](float p) {
      job->progress.store(p * .8f);
      return !job->cancel.load();
    };
    if (resources::generateTerrain(job->recipe, job->mesh, job->error,
                                   progress) &&
        !job->cancel &&
        resources::writeTerrainGlb(job->recipe, job->mesh, textures(),
                                   job->bytes, job->error)) {
      if (!resources::importGlb(job->bytes, {}, {}, job->model))
        job->error = job->model.diagnostic;
    }
    job->ready.store(true, std::memory_order_release);
  }).detach();
}
void EditorSession::refreshTerrainJob() {
  if (!terrainJob_) {
    if (state_.terrainOpen && state_.terrainTarget) {
      const auto *entity = document_.find(state_.terrainTarget);
      const auto *link =
          entity ? scene::importLink(entity->components) : nullptr;
      if (!link) {
        state_.terrainTarget = 0;
        terrainSource_ = {};
        state_.terrainBrushArmed = false;
      } else if (link->source != terrainSource_)
        openTerrain(state_.terrainTarget);
    }
    return;
  }
  auto job = terrainJob_;
  state_.terrainProgress = job->progress.load();
  if (!job->ready.load(std::memory_order_acquire))
    return;
  state_.terrainBusy = false;
  terrainJob_.reset();
  if (job->cancel) {
    state_.terrainMessage = "Geração cancelada · cena preservada";
    return;
  }
  if (job->epoch != sceneEpoch_ || job->revision != document_.revision() ||
      job->project != files_.rootPath() ||
      job->target != state_.terrainTarget || isPlaying()) {
    state_.terrainMessage =
        "Cena mudou durante a geração; resultado descartado";
    return;
  }
  std::string error = job->error;
  if (error.empty())
    publishTerrain(job->recipe, job->mesh, job->bytes, job->model, error);
  if (!error.empty()) {
    state_.terrainMessage = error;
    state_.status = error;
  }
}
bool EditorSession::handleTerrainInput(const ui::UiPointerEvent &event,
                                       const ui::UiPointerRouting &routing) {
  const auto id = routing.widgetId, base = widgetId(EditorWidget::TerrainOpen);
  if (routing.tapped && id == base) {
    openTerrain();
    return true;
  }
  if (!state_.terrainOpen)
    return false;
  if (state_.terrainBrushArmed &&
      routing.target == ui::UiPointerTarget::Viewport) {
    if (state_.terrainBusy)
      return true;
    if (routing.tapped) {
      const auto ray = screenPointToRay(view_, event.position);
      buildPickCandidates(nullptr, false, state_.terrainTarget);
      const auto hit = pickNearest(candidates_, ray, 0, true);
      float world[16];
      if (hit.hit &&
          editorWorldMatrix(document_, state_.terrainTarget, world)) {
        // Invert affine transform by reciprocal basis. Reject shear explicitly.
        float p[3];
        for (u32 a = 0; a < 3; ++a)
          p[a] =
              ray.origin[a] + ray.direction[a] * hit.distance - world[12 + a];
        float local[3]{};
        bool valid = true;
        for (u32 a = 0; a < 3; ++a) {
          float length = 0;
          for (u32 k = 0; k < 3; ++k) {
            length += world[a * 4 + k] * world[a * 4 + k];
            local[a] += p[k] * world[a * 4 + k];
          }
          if (length < 1e-8f)
            valid = false;
          else
            local[a] /= length;
          for (u32 b = a + 1; b < 3; ++b) {
            float d = 0;
            for (u32 k = 0; k < 3; ++k)
              d += world[a * 4 + k] * world[b * 4 + k];
            if (std::abs(d) > 1e-4f)
              valid = false;
          }
        }
        if (valid) {
          state_.terrainBrush.x = local[0];
          state_.terrainBrush.y = local[1];
          state_.terrainBrush.z = local[2];
          if (state_.terrainDraft.strokes.size() < 256) {
            state_.terrainDraft.strokes.push_back(state_.terrainBrush);
            startTerrainJob();
          }
        } else
          state_.terrainMessage =
              "Transformação com shear: ajuste o pai antes de pintar";
      } else
        state_.terrainMessage = "Toque na superfície do terreno";
    }
    return true;
  }
  if (id < base || id >= base + 0x200)
    return false;
  if (!routing.tapped)
    return true;
  if (id == widgetId(EditorWidget::TerrainCancel)) {
    if (terrainJob_)
      terrainJob_->cancel = true;
    return true;
  }
  if (id == widgetId(EditorWidget::TerrainClose)) {
    if (terrainJob_)
      terrainJob_->cancel = true;
    state_.terrainOpen = false;
    state_.terrainBrushArmed = false;
    return true;
  }
  if (state_.terrainBusy)
    return true;
  if (id >= widgetId(EditorWidget::TerrainNumberBase) &&
      id < widgetId(EditorWidget::TerrainNumberBase) + 14) {
    state_.numericField = id;
    state_.numericEntity = state_.terrainTarget;
    state_.numericInstance = 0;
    state_.numericProperty.clear();
    state_.numericCurrent =
        number(state_, id - widgetId(EditorWidget::TerrainNumberBase));
    std::snprintf(state_.numericText, sizeof(state_.numericText), "%.9g",
                  state_.numericCurrent);
    state_.numericReplace = true;
    state_.numericError = false;
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainTabBase) &&
      id < widgetId(EditorWidget::TerrainTabBase) + 4) {
    state_.terrainTab = id - widgetId(EditorWidget::TerrainTabBase);
    state_.terrainPage = 0;
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainToolBase) &&
      id < widgetId(EditorWidget::TerrainToolBase) + 8) {
    state_.terrainBrush.tool =
        resources::TerrainTool(id - widgetId(EditorWidget::TerrainToolBase));
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainLayerBase) &&
      id < widgetId(EditorWidget::TerrainLayerBase) + 4) {
    state_.terrainBrush.layer = id - widgetId(EditorWidget::TerrainLayerBase);
    state_.terrainBrush.tool = resources::TerrainTool::Paint;
    return true;
  }
  switch (EditorWidget(id)) {
  case EditorWidget::TerrainGenerate:
    startTerrainJob();
    break;
  case EditorWidget::TerrainNew:
    openTerrain();
    break;
  case EditorWidget::TerrainLoad:
    openTerrain(state_.selection);
    break;
  case EditorWidget::TerrainMode:
    if (state_.terrainDraft.strokes.empty())
      state_.terrainDraft.mode =
          resources::TerrainMode((u32(state_.terrainDraft.mode) + 1) % 3);
    else
      state_.terrainMessage =
          "Crie outro terreno para mudar a representação após esculpir";
    break;
  case EditorWidget::TerrainIsland:
    state_.terrainDraft.island = !state_.terrainDraft.island;
    break;
  case EditorWidget::TerrainCollision:
    state_.terrainDraft.collision = !state_.terrainDraft.collision;
    break;
  case EditorWidget::TerrainBrushArm:
    state_.terrainBrushArmed =
        state_.terrainTarget && !state_.terrainBrushArmed;
    break;
  case EditorWidget::TerrainStamp:
    if (state_.terrainDraft.strokes.size() < 256) {
      state_.terrainDraft.strokes.push_back(state_.terrainBrush);
      startTerrainJob();
    }
    break;
  case EditorWidget::TerrainRemoveStroke:
    if (!state_.terrainDraft.strokes.empty()) {
      state_.terrainDraft.strokes.pop_back();
      startTerrainJob();
    }
    break;
  case EditorWidget::TerrainPrevious:
    if (state_.terrainPage)
      --state_.terrainPage;
    break;
  case EditorWidget::TerrainNext:
    ++state_.terrainPage;
    break;
  case EditorWidget::TerrainFrame:
    if (state_.terrainTarget) {
      setSelection(state_.terrainTarget);
      frameSelection();
    }
    break;
  default:
    break;
  }
  return true;
}
} // namespace ae::editor

#include "core/sha256.h"
#include "editor/editor_import_transaction.h"
#include "editor/editor_screen.h"
#include "editor/editor_session.h"
#include "resources/terrain_textures.generated.h"
#include "scene/collider.h"
#include "scene/import_link.h"
#include "scene/physics_body.h"
#include "renderer/authoring_texture.h"
#include <atomic>
#include <cmath>
#include <map>
#include <limits>
#include <type_traits>
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
                        b.z, r.baseDepth, b.hardness, b.rotation, b.aspect, s.terrainSpacing, r.macroVariation};
  return i < 20 ? values[i] : 0;
}
u64 terrainCandidateBytes(const resources::GltfImport &model) {
  // Terrain GLBs are static. Count the retained geometry, decoded textures and
  // conservative metadata capacity before stageSource copies the whole library.
  // This bounds growth; it is not a process/GPU peak-memory guarantee.
  if (!model.skins.empty() || !model.animations.empty() || !model.morphs.empty())
    return std::numeric_limits<u64>::max();
  const auto storage = [](const auto &values) {
    return u64(values.capacity()) * sizeof(typename std::decay_t<decltype(values)>::value_type);
  };
  u64 bytes = 1024 + storage(model.vertices) + storage(model.indices) + storage(model.draws) +
              storage(model.materials) + storage(model.names) + storage(model.nodes) +
              storage(model.drawNodes) + storage(model.materialNames) + storage(model.textures) +
              storage(model.textureImages) + storage(model.meshLods) + storage(model.drawSkins) +
              storage(model.drawMorphs) + storage(model.skinInfluences);
  bytes += u64(model.nodes.size()) * 2 * sizeof(resources::ImportNodeRecord);
  bytes += u64(model.draws.size()) * 4 * sizeof(resources::AssetGuid);
  for (const auto &node : model.nodes)
    bytes += 2 * (node.name.capacity()+node.authoredId.capacity()) + storage(node.morphWeights);
  for (const auto *names : {&model.names, &model.materialNames, &model.textureImages})
    for (const auto &name : *names) bytes += name.capacity();
  for (const auto &texture : model.textures) if (texture) {
    bytes += sizeof(*texture) + texture->mipChain.capacity();
    if (texture->file) bytes += sizeof(*texture->file) + texture->file->path.capacity();
  }
  return bytes;
}
bool terrainSourceCurrent(const EditorDocument &document, EditorEntityId target,
                          resources::AssetGuid source) {
  if (!target) return true;
  const auto *entity = document.find(target);
  const auto *link = entity ? scene::importLink(entity->components) : nullptr;
  return link && link->source == source;
}
void normalizeTerrainBrush(EditorScreenState &state) {
  const auto tool=state.terrainBrush.tool;
  const bool volume=state.terrainDraft.mode!=resources::TerrainMode::Heightfield &&
      (tool==resources::TerrainTool::Dig || tool==resources::TerrainTool::Add ||
       tool==resources::TerrainTool::Smooth || tool==resources::TerrainTool::Sharpen ||
       tool==resources::TerrainTool::Restore || tool==resources::TerrainTool::Paint);
  if(!volume &&
      u32(state.terrainBrush.shape)>1)state.terrainBrush.shape=resources::TerrainBrushShape::Circle;
  if(state.terrainDraft.mode != resources::TerrainMode::Heightfield &&
      state.terrainBrush.tool==resources::TerrainTool::Hole)state.terrainBrush.tool=resources::TerrainTool::Dig;
  if (state.terrainDraft.mode == resources::TerrainMode::Heightfield &&
      (state.terrainBrush.tool == resources::TerrainTool::Dig ||
       state.terrainBrush.tool == resources::TerrainTool::Add))
    state.terrainBrush.tool = resources::TerrainTool::Raise;
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
struct EditorSession::TerrainPreviewJob {
  resources::TerrainRecipe recipe;
  resources::TerrainMesh mesh;
  std::string error;
  u64 epoch=0, revision=0;
  EditorEntityId target=0;
  usize samples=0;
  std::atomic<bool> ready{false},cancel{false};
};
void EditorSession::clearTerrainPreview(bool restore) {
  if(terrainPreviewJob_)terrainPreviewJob_->cancel=true;
  terrainPreviewJob_.reset();
  const bool published=terrainPreviewCount_!=0;
  terrainPreviewCount_=terrainPreviewFirst_=0;
  terrainPreviewSamples_=0;
  state_.terrainLivePreview=false;
  if(published && restore && terrainPreviewEpoch_==sceneEpoch_) {
    std::string error;
    if(!publishAndAdopt(flattenSources(importedSources_),error,nullptr,true))
      state_.terrainMessage="Falha ao restaurar prévia: "+error;
    appearanceChanged_=true;
  }
}
void EditorSession::queueTerrainPreview() {
  if(terrainPreviewJob_ || !terrainGestureActive_ || terrainGestureStrokes_.empty() ||
      terrainPreviewSamples_==terrainGestureStrokes_.size())return;
  auto job=std::make_shared<TerrainPreviewJob>();
  job->recipe=state_.terrainDraft;
  job->recipe.strokes.insert(job->recipe.strokes.end(),terrainGestureStrokes_.begin(),terrainGestureStrokes_.end());
  job->epoch=sceneEpoch_;job->revision=document_.revision();job->target=state_.terrainTarget;
  job->samples=terrainGestureStrokes_.size();terrainPreviewJob_=job;
  std::thread([job] {
    resources::generateTerrain(job->recipe,job->mesh,job->error,[job](float){return !job->cancel.load();});
    job->ready.store(true,std::memory_order_release);
  }).detach();
}
bool EditorSession::publishTerrainPreview(const resources::TerrainMesh &mesh,std::string &error) {
  const auto source=std::find_if(importedSources_.begin(),importedSources_.end(),
      [&](const auto &s){return s.guid==terrainSource_;});
  if(source==importedSources_.end())return false;
  ImportedSource preview;
  preview.guid=resources::assetGuidFromSeed("terrain-transient-preview");
  preview.materials=source->materials;preview.textures=source->textures;
  // The draft may switch a legacy terrain to blended layers before Apply.
  // Rebind the same decoded albedos without a GLB encode/decode round trip.
  if(preview.materials.size()>=4) {
    u32 layers[4];
    const bool oldBlend=(source->materials[0].flags&renderer::MapMaterialTerrain)!=0;
    for(u32 i=0;i<4;++i)layers[i]=oldBlend?source->materials[0].textureIndices[i]:source->materials[i].textureIndices[0];
    for(u32 i=0;i<4;++i) {
      auto &material=preview.materials[i];
      material.flags&=~(renderer::MapMaterialTerrain|renderer::MapMaterialNormalMap|
        renderer::MapMaterialMetallicRoughnessMap|renderer::MapMaterialEmissiveMap);
      for(u32 slot=0;slot<4;++slot)material.textureIndices[slot]=state_.terrainDraft.blendedSurface?layers[slot]:
        slot==0?layers[i]:renderer::InvalidMapTexture;
      if(state_.terrainDraft.blendedSurface)material.flags|=renderer::MapMaterialTerrain;
    }
  }
  // No file encoding, decoding, asset registry, entity or history mutation.
  struct Vertex {float p[3];i16 n[4],t[4];float uv[2],uv1[2];u8 color[4];};
  static_assert(sizeof(Vertex)==renderer::MapVertexStride);
  for(const auto &part:mesh.parts) {
    renderer::MapDrawRecord draw{};
    draw.firstIndex=u32(preview.indices.size());draw.indexCount=u32(part.vertices.size());
    draw.vertexOffset=u32(preview.vertices.size()/sizeof(Vertex));draw.materialIndex=part.layer;
    draw.model[0]=draw.model[5]=draw.model[10]=draw.model[15]=1;
    float lo[3]{INFINITY,INFINITY,INFINITY},hi[3]{-INFINITY,-INFINITY,-INFINITY};
    for(u32 i=0;i<part.vertices.size();++i) {
      const auto &v=part.vertices[i];Vertex packed{};
      for(u32 a=0;a<3;++a) {
        packed.p[a]=v.position[a];packed.n[a]=i16(std::clamp(v.normal[a],-1.f,1.f)*32767);
        lo[a]=std::min(lo[a],v.position[a]);hi[a]=std::max(hi[a],v.position[a]);
      }
      packed.t[0]=packed.t[3]=32767;
      std::copy_n(v.uv,2,packed.uv);std::copy_n(v.uv1,2,packed.uv1);
      for(u32 a=0;a<4;++a)packed.color[a]=u8(std::lround(std::clamp(v.weights[a],0.f,1.f)*255));
      auto offset=preview.vertices.size();preview.vertices.resize(offset+sizeof(packed));
      std::memcpy(preview.vertices.data()+offset,&packed,sizeof(packed));preview.indices.push_back(i);
    }
    float radius2=0;
    for(u32 a=0;a<3;++a){draw.boundsCenter[a]=(lo[a]+hi[a])*.5f;radius2+=(hi[a]-lo[a])*(hi[a]-lo[a])*.25f;}
    draw.boundsRadius=std::sqrt(radius2);draw.lodGroupId=u32(preview.draws.size());
    preview.identities.push_back(resources::assetGuidFromSeed("terrain-preview:"+std::to_string(preview.draws.size())));
    preview.names.push_back("Prévia de terreno");preview.draws.push_back(draw);
  }
  auto sources=importedSources_;
  usize previous=0;for(const auto &s:sources)previous+=s.draws.size();
  sources.push_back(std::move(preview));
  usize primitives=0;
  if(!publishAndAdopt(flattenSources(sources),error,&primitives,true))return false;
  terrainPreviewFirst_=u32(primitives+previous);terrainPreviewCount_=u32(mesh.parts.size());
  terrainPreviewEpoch_=sceneEpoch_;state_.terrainLivePreview=true;++state_.terrainPreviewUpdates;
  appearanceChanged_=true;
  return true;
}
void EditorSession::refreshTerrainPreview() {
  if(!terrainGestureActive_ && !state_.terrainBusy) {
    if(terrainPreviewCount_ || terrainPreviewJob_)clearTerrainPreview();
    return;
  }
  if(!terrainPreviewJob_) {queueTerrainPreview();return;}
  auto job=terrainPreviewJob_;
  if(job->epoch!=sceneEpoch_ || job->revision!=document_.revision() ||
      job->target!=state_.terrainTarget || !state_.terrainOpen || isPlaying()) {
    terrainGestureActive_=false;terrainGestureStrokes_.clear();clearTerrainPreview();return;
  }
  if(!job->ready.load(std::memory_order_acquire))return;
  terrainPreviewJob_.reset();
  terrainPreviewSamples_=job->samples;
  if(!job->error.empty())state_.terrainMessage="Prévia: "+job->error;
  if(!job->cancel && job->error.empty() && terrainGestureActive_) {
    std::string error;
    if(!publishTerrainPreview(job->mesh,error))state_.terrainMessage="Prévia: "+error;
  }
  queueTerrainPreview();
}
void EditorSession::refreshTerrainBudget() {
  state_.terrainRetainedBytes = state_.terrainRetainedDraws = 0;
  state_.terrainRetainedBytesKnown = true;
  for (const auto &source : importedSources_) {
    state_.terrainRetainedDraws += source.draws.size();
    const auto bytes = retainedModelBytes(source);
    if (bytes == std::numeric_limits<u64>::max() ||
        bytes > std::numeric_limits<u64>::max() - state_.terrainRetainedBytes) {
      state_.terrainRetainedBytesKnown = false;
      state_.terrainRetainedBytes = EditorScreenState::TerrainRetainedByteLimit + 1;
    } else if (state_.terrainRetainedBytesKnown)
      state_.terrainRetainedBytes += bytes;
  }
}
EditorEntityId EditorSession::terrainRoot(EditorEntityId target) {
  if (terrainAccessProject_ != files_.rootPath()) {
    terrainAccessProject_ = files_.rootPath();
    terrainAccess_.clear();
  }
  // Resolve authoring through ancestors, including attachments or nested imported
  // models. The recipe identifies a terrain; renaming its group or GLB does not.
  for (usize depth = 0; target && depth < document_.entityCount(); ++depth) {
    const auto *entity = document_.find(target);
    if (!entity)
      break;
    const auto *link = scene::importLink(entity->components);
    const auto *record = link && link->root ? assets_.find(link->source) : nullptr;
    if (record) {
      auto found = std::find_if(terrainAccess_.begin(), terrainAccess_.end(),
                               [&](const auto &entry) { return entry.source == record->guid; });
      if (found == terrainAccess_.end()) {
        terrainAccess_.push_back({record->guid, {}, {}, false});
        found = std::prev(terrainAccess_.end());
      }
      if (found->path != record->path || found->hash != record->contentHash) {
        found->path = record->path;
        found->hash = record->contentHash;
        resources::TerrainRecipe recipe;
        std::filesystem::path absolute;
        std::vector<u8> bytes;
        std::string error;
        found->editable = EditorImportTransaction::safePath(
                              EditorImportTransaction::fromUtf8(files_.rootPath()),
                              record->path, absolute) &&
                          EditorImportTransaction::read(absolute, bytes, 64u << 20) &&
                          resources::terrainRecipeFromGlb(bytes, recipe, error);
      }
      if (found->editable)
        return entity->id;
    }
    target = entity->parent;
  }
  return 0;
}
bool EditorSession::openTerrain(EditorEntityId target) {
  if (isPlaying() || state_.terrainBusy)
    return false;
  clearTerrainPreview();
  refreshTerrainBudget();
  if (target) {
    // Explicit reopening retries a restored file even if its registry hash did
    // not change. Automatic refresh checks this per-session negative cache first.
    for (auto ancestor = target, depth = EditorEntityId{0};
         ancestor && depth < document_.entityCount(); ++depth) {
      const auto *entity = document_.find(ancestor);
      if (!entity) break;
      const auto *link = scene::importLink(entity->components);
      if (link && link->root)
        terrainAccess_.erase(std::remove_if(terrainAccess_.begin(), terrainAccess_.end(),
            [&](const auto &entry) { return entry.source == link->source && !entry.editable; }),
            terrainAccess_.end());
      ancestor = entity->parent;
    }
    const auto id = terrainRoot(target);
    const auto *entity = document_.find(id);
    const auto *link = entity ? scene::importLink(entity->components) : nullptr;
    const auto *record = link ? assets_.find(link->source) : nullptr;
    std::vector<u8> bytes;
    std::string error;
    std::filesystem::path absolute;
    resources::TerrainRecipe recipe;
    if (!record ||
        !EditorImportTransaction::safePath(
            EditorImportTransaction::fromUtf8(files_.rootPath()), record->path,
            absolute) ||
        !EditorImportTransaction::read(absolute, bytes, 64u << 20) ||
        !resources::terrainRecipeFromGlb(bytes, recipe, error)) {
      if (record)
        for (auto &entry : terrainAccess_)
          if (entry.source == record->guid) entry.editable = false;
      state_.status = error.empty()
                          ? "Selecione a raiz ou um chunk do terreno Astra"
                          : error;
      state_.terrainMessage = state_.status;
      return false;
    }
    state_.terrainDraft = std::move(recipe);
    state_.terrainTarget = id;
    terrainSource_ = record->guid;
    state_.terrainMessage = "Receita carregada · " +
                            std::to_string(state_.terrainDraft.strokes.size()) +
                            " operações";
  } else {
    state_.terrainDraft = {};
    state_.terrainDraft.solidBase = true;
    state_.terrainTarget = 0;
    terrainSource_ = {};
    state_.terrainMessage = "Novo terreno · configure e gere";
  }
  normalizeTerrainBrush(state_);
  state_.terrainOpen = true;
  state_.terrainBrushArmed = false;
  terrainGestureActive_ = false;
  terrainGestureStrokes_.clear();
  state_.creationMenu = false;
  state_.contextMenu = false;
  state_.contextMenuCreating = false;
  state_.inspectorMenu = false;
  state_.workspace = EditorWorkspace::Scene;
  state_.terrainTab = target ? 1u : 0u;
  state_.terrainPage = 0;
  state_.terrainScroll = 0;
  state_.terrainSection = 0;
  state_.terrainAppliedRecipe = target ? resources::serializeTerrainRecipe(state_.terrainDraft) : std::string{};
  return true;
}
bool EditorSession::setTerrainNumber(u32 field, double value) {
  if (!state_.terrainOpen || state_.terrainBusy || !std::isfinite(value) ||
      field >= 20)
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
  case 14:
    r.baseDepth = f;
    break;
  case 15: b.hardness=f;break;
  case 16: b.rotation=f;break;
  case 17: b.aspect=f;break;
  case 18:
    if(f<.05f || f>1)return false;
    state_.terrainSpacing=f;return true;
  case 19: r.macroVariation=f;break;
  default:
    return false;
  }
  std::string error;
  if (!resources::validTerrainRecipe(r, error) || b.radius < .1f ||
      b.radius > 2048 || b.hardness<0 || b.hardness>1 || std::abs(b.rotation)>360 || b.aspect<.1f || b.aspect>10 || b.strength < 0 || b.strength > 128 ||
      std::abs(b.x) > 4096 || std::abs(b.z) > 4096 || std::abs(b.y) > 2048) {
    state_.terrainMessage = error.empty() ? "Valor fora dos limites do pincel" : error;
    return false;
  }
  state_.terrainDraft = std::move(r);
  state_.terrainBrush = b;
  state_.terrainMessage = (field >= 9 && field <= 13) || (field>=15 && field<=18)
      ? "Pincel ajustado · use o terreno ou Aplicar na posição XYZ"
      : "Rascunho alterado · toque Aplicar alterações para atualizar";
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
  auto path = "Terrenos/terrain-" + hash.substr(0, 24) + ".glb";
  ModelImportReport report;
  const auto *known = assets_.findByPath(path);
  if (!known) for (const auto &entry : importedSources_) {
    const auto *record = assets_.find(entry.guid);
    if (record && record->contentHash == hash) {
      known = record;
      path = record->path;
      break;
    }
  }
  refreshTerrainBudget();
  if (!known) {
    const auto candidateBytes = terrainCandidateBytes(model);
    const auto byteLimit = EditorScreenState::TerrainRetainedByteLimit;
    const auto drawLimit = EditorScreenState::TerrainRetainedDrawLimit;
    if (!state_.terrainRetainedBytesKnown || candidateBytes == std::numeric_limits<u64>::max()) {
      error = "Não foi possível contabilizar a biblioteca carregada; nova revisão recusada. "
              "Terreno, fontes e histórico preservados.";
      return false;
    }
    if (candidateBytes > byteLimit ||
        state_.terrainRetainedBytes > byteLimit-candidateBytes ||
        model.draws.size() > drawLimit || state_.terrainRetainedDraws > drawLimit-model.draws.size()) {
      error = "Nova revisão excede o orçamento da biblioteca: 128 MiB CPU contabilizados ou 4096 desenhos. "
              "Terreno, fontes e histórico preservados; não há limpeza automática.";
      return false;
    }
    if (!commitModelImport(bytes, model, path, "", report)) {
      error = report.diagnostic;
      return false;
    }
    refreshTerrainBudget();
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
  state_.terrainAppliedRecipe = resources::serializeTerrainRecipe(r);
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
  if (!terrainSourceCurrent(document_, state_.terrainTarget, terrainSource_)) {
    state_.terrainBrushArmed = false;
    state_.terrainMessage = "Fonte do terreno mudou · reabra o terreno antes de aplicar";
    return;
  }
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
  refreshTerrainPreview();
  state_.terrainSelectionRoot = isPlaying() ? 0 : terrainRoot(state_.selection);
  const auto inspected = state_.inspectorTarget ? state_.inspectorTarget :
                         state_.inspectorLocked ? state_.inspectorLocked : state_.selection;
  state_.terrainInspectorRoot = isPlaying() ? 0 : terrainRoot(inspected);
  if (!terrainJob_) {
    if (state_.terrainOpen && state_.terrainTarget) {
      const auto *entity = document_.find(state_.terrainTarget);
      const auto *link =
          entity ? scene::importLink(entity->components) : nullptr;
      if (!link) {
        state_.terrainTarget = 0;
        terrainSource_ = {};
        state_.terrainBrushArmed = false;
      } else if (link->source != terrainSource_ && !isPlaying()) {
        state_.terrainBrushArmed = false;
        terrainGestureActive_ = false;
        terrainGestureStrokes_.clear();
        const auto source = link->source;
        const auto *record = assets_.find(source);
        const auto access = std::find_if(terrainAccess_.begin(), terrainAccess_.end(),
            [&](const auto &entry) { return entry.source == source; });
        const bool failed = access != terrainAccess_.end() && !access->editable &&
            (record ? access->path == record->path && access->hash == record->contentHash
                    : access->path.empty() && access->hash.empty());
        if (!failed) {
          // Undo/Redo replaces the recipe, not the user's working context. Keep
          // controls under the same finger; explicit Inspector opening still
          // selects the brush tab through openTerrain's normal defaults.
          const auto tab = state_.terrainTab, section = state_.terrainSection,
                     page = state_.terrainPage;
          const auto scroll = state_.terrainScroll;
          const auto brush = state_.terrainBrush;
          const bool loaded = openTerrain(state_.terrainTarget);
          state_.terrainTab = tab;
          state_.terrainSection = section;
          state_.terrainPage = page;
          state_.terrainScroll = scroll;
          state_.terrainBrush = brush;
          if (loaded) normalizeTerrainBrush(state_);
          else {
            auto cached = std::find_if(terrainAccess_.begin(), terrainAccess_.end(),
                [&](const auto &entry) { return entry.source == source; });
            if (cached == terrainAccess_.end()) {
              terrainAccess_.push_back({source, {}, {}, false});
              cached = std::prev(terrainAccess_.end());
            }
            cached->path = record ? record->path : std::string{};
            cached->hash = record ? record->contentHash : std::string{};
            cached->editable = false;
          }
        }
        if (link->source != terrainSource_)
          state_.terrainMessage = "Fonte do terreno indisponível · rascunho preservado; reabra para tentar novamente";
      }
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
  clearTerrainPreview(false);
  if (error.empty())
    publishTerrain(job->recipe, job->mesh, job->bytes, job->model, error);
  if(!error.empty()) {std::string ignored;publishAndAdopt(flattenSources(importedSources_),ignored,nullptr,true);}
  if (!error.empty()) {
    state_.terrainMessage = error;
    state_.status = error;
  }
}
bool EditorSession::handleTerrainInput(const ui::UiPointerEvent &event,
                                       const ui::UiPointerRouting &routing) {
  const auto id = routing.widgetId, base = widgetId(EditorWidget::TerrainOpen);
  if (routing.tapped && id == base) {
    auto target = terrainRoot(state_.selection);
    if (!target && document_.find(state_.terrainTarget))
      target = state_.terrainTarget;
    openTerrain(target);
    return true;
  }
  if (routing.tapped && id == widgetId(EditorWidget::TerrainEditSelection)) {
    const auto target = state_.inspectorTarget ? state_.inspectorTarget :
                        state_.inspectorLocked ? state_.inspectorLocked : state_.selection;
    openTerrain(target);
    return true;
  }
  if (!state_.terrainOpen)
    return false;
  if(routing.tapped && (id==widgetId(EditorWidget::NavigationOrbit) ||
      id==widgetId(EditorWidget::NavigationPan) || id==widgetId(EditorWidget::NavigationZoom))) {
    state_.terrainBrushArmed=false;state_.terrainCursorVisible=false;clearTerrainPreview();
    return false;
  }
  if(id==widgetId(EditorWidget::ToolMove) || id==widgetId(EditorWidget::ToolRotate) ||
      id==widgetId(EditorWidget::ToolScale) || id==widgetId(EditorWidget::ToolSelect))return true;
  if (!terrainSourceCurrent(document_, state_.terrainTarget, terrainSource_) &&
      ((routing.tapped && (id == widgetId(EditorWidget::TerrainGenerate) ||
                          id == widgetId(EditorWidget::TerrainBrushArm) ||
                          id == widgetId(EditorWidget::TerrainSculpt) ||
                          id == widgetId(EditorWidget::TerrainStamp))) ||
       (routing.target == ui::UiPointerTarget::Viewport && state_.terrainBrushArmed) ||
       terrainGestureActive_)) {
    state_.terrainBrushArmed = false;
    terrainGestureActive_ = false;
    terrainGestureStrokes_.clear();
    state_.terrainMessage = "Fonte do terreno mudou · reabra o terreno antes de aplicar";
    return true;
  }
  if (terrainGestureActive_ && event.phase == ui::UiPointerPhase::Down &&
      routing.target != ui::UiPointerTarget::Viewport) {
    terrainGestureActive_ = false;
    terrainGestureStrokes_.clear();
    state_.terrainMessage = "Traço cancelado ao usar os controles · cena preservada";
  }
  if ((state_.terrainBrushArmed && routing.target == ui::UiPointerTarget::Viewport) ||
      (terrainGestureActive_ && terrainGesturePointer_ == event.pointerId)) {
    if (state_.terrainBusy)
      return true;
    if (event.phase == ui::UiPointerPhase::Down) {
      if (terrainGestureActive_)
        return true; // One brush owns the gesture; a second finger never paints.
      terrainGestureActive_ = true;
      terrainGesturePointer_ = event.pointerId;
      terrainGestureTarget_ = state_.terrainTarget;
      terrainGestureRevision_ = document_.revision();
      clearTerrainPreview();
      terrainGestureBrush_ = state_.terrainBrush;
      terrainGestureBrush_.gesture=state_.terrainDraft.strokes.empty()?1:state_.terrainDraft.strokes.back().gesture+1;
      terrainGestureRecipe_ = resources::serializeTerrainRecipe(state_.terrainDraft);
      terrainGestureStrokes_.clear();
      terrainGestureOverflow_ = terrainGestureConnected_ = false;
    }
    if (!terrainGestureActive_ || terrainGesturePointer_ != event.pointerId)
      return true;
    if (event.phase == ui::UiPointerPhase::Cancel ||
        terrainGestureTarget_ != state_.terrainTarget || terrainGestureRevision_ != document_.revision() ||
        terrainGestureRecipe_ != resources::serializeTerrainRecipe(state_.terrainDraft)) {
      terrainGestureActive_ = false;
      terrainGestureStrokes_.clear();
      state_.terrainCursorVisible=false;
      clearTerrainPreview();
      state_.terrainMessage = "Traço cancelado · cena e receita preservadas";
      return true;
    }
    const auto append = [&](const resources::TerrainStroke &stroke) {
      if (state_.terrainDraft.strokes.size() + terrainGestureStrokes_.size() >= 4096) {
        terrainGestureOverflow_ = true;
        terrainGestureStrokes_.clear();
        state_.terrainMessage = "Limite de 4096 amostras · traço descartado; use raio maior ou um traço menor";
        return false;
      }
      terrainGestureStrokes_.push_back(stroke);
      return true;
    };
    // The pointer keeps capture outside the viewport, but cannot paint through
    // a panel. Missing the surface suspends brush resampling until the next hit.
    if (!terrainGestureOverflow_ && view_.rect.contains(event.position) &&
        router_.hitTest(event.position).target == ui::UiPointerTarget::Viewport) {
      auto pointer=event.position;
      state_.terrainPointer=pointer;
      if(state_.terrainTouchOffset)pointer.y-=44.f;
      const auto ray = screenPointToRay(view_, pointer);
      buildPickCandidates(nullptr, false, state_.terrainTarget);
      const auto *root = document_.find(state_.terrainTarget);
      const auto *rootLink = root ? scene::importLink(root->components) : nullptr;
      std::erase_if(candidates_, [&](const auto &candidate) {
        const auto *object = document_.find(candidate.id);
        const auto *link = object ? scene::importLink(object->components) : nullptr;
        return !rootLink || !link || link->root || link->source != rootLink->source;
      });
      float world[16];
      if(terrainPreviewCount_ && editorWorldMatrix(document_,state_.terrainTarget,world)) {
        candidates_.clear();
        for(u32 part=0;part<terrainPreviewCount_;++part) {
          EditorPickCandidate c;c.id=state_.terrainTarget;
          if(mapScene_.pickResourceGeometry(terrainPreviewFirst_+part,world,c))candidates_.push_back(std::move(c));
        }
      }
      const auto hit = pickNearest(candidates_, ray, 0, true);
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
          state_.terrainCursorVisible=true;
          for(u32 a=0;a<3;++a)state_.terrainCursor[a]=p[a]+world[12+a];
          state_.terrainCursorLines.clear();
          const auto &brush=terrainGestureBrush_;
          const float angle=brush.rotation*.01745329252f;
          const bool volume=state_.terrainDraft.mode!=resources::TerrainMode::Heightfield &&
            (brush.tool==resources::TerrainTool::Dig || brush.tool==resources::TerrainTool::Add ||
             brush.tool==resources::TerrainTool::Smooth || brush.tool==resources::TerrainTool::Sharpen ||
             brush.tool==resources::TerrainTool::Restore || brush.tool==resources::TerrainTool::Paint);
          for(u32 ring=0;ring<(volume?3u:2u);++ring) {
            float previous[3]{};
            for(u32 segment=0;segment<=48;++segment) {
              const float theta=segment*6.28318530718f/48;
              float x=std::cos(theta),z=std::sin(theta),y=0;
              if(brush.shape==resources::TerrainBrushShape::Box) {const float m=std::max(std::abs(x),std::abs(z));x/=m;z/=m;}
              const float scale=brush.radius*(!volume && ring==1?(.5f+.5f*brush.hardness):1.f);
              if(volume && ring==1){y=z;z=0;}else if(volume && ring==2){y=x;x=0;}
              if(volume && brush.shape==resources::TerrainBrushShape::Hemisphere)y=std::abs(y);
              if(volume && brush.shape==resources::TerrainBrushShape::Cone && ring>0) {
                // Each vertical ring is the exact triangular cone silhouette.
                const float phase=segment*3.f/48.f;
                float radial=0;
                if(phase<1){radial=1-phase;y=2*phase;}
                else if(phase<2){radial=1-phase;y=2*(2-phase);}
                else {radial=2*phase-5;y=0;}
                x=ring==1?radial:0;z=ring==2?radial:0;
              }
              x*=brush.aspect;
              const float q[]{local[0]+scale*(x*std::cos(angle)-z*std::sin(angle)),local[1]+scale*y,
                              local[2]+scale*(x*std::sin(angle)+z*std::cos(angle))};
              float at[3]{};
              for(u32 a=0;a<3;++a)at[a]=world[12+a]+world[a]*q[0]+world[4+a]*q[1]+world[8+a]*q[2];
              if(!volume) {
                EditorRay probe;probe.valid=true;
                float len=std::sqrt(world[4]*world[4]+world[5]*world[5]+world[6]*world[6]);
                for(u32 a=0;a<3;++a){probe.direction[a]=-world[4+a]/len;probe.origin[a]=at[a]-probe.direction[a]*4096;}
                auto surface=pickNearest(candidates_,probe,0,true);
                if(surface.hit)for(u32 a=0;a<3;++a)at[a]=probe.origin[a]+probe.direction[a]*(surface.distance-.04f);
              }
              if(segment)state_.terrainCursorLines.push_back({previous[0],previous[1],previous[2],at[0],at[1],at[2]});
              std::copy_n(at,3,previous);
            }
          }
          const bool fixedLevel =
              terrainGestureBrush_.tool == resources::TerrainTool::Flatten ||
              terrainGestureBrush_.tool == resources::TerrainTool::Path;
          state_.terrainBrush.x = local[0];
          if (!fixedLevel)
            state_.terrainBrush.y = local[1];
          state_.terrainBrush.z = local[2];
          auto sample = terrainGestureBrush_;
          sample.x = local[0];
          if (!fixedLevel)
            sample.y = local[1];
          sample.z = local[2];
          if (!terrainGestureConnected_ || terrainGestureStrokes_.empty()) {
            append(sample);
          } else {
            const auto previous = terrainGestureStrokes_.back();
            const float delta[]{sample.x-previous.x,sample.y-previous.y,sample.z-previous.z};
            const float distance = std::sqrt(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]);
            const float spacing = std::max(.1f, sample.radius * state_.terrainSpacing);
            const u32 steps = static_cast<u32>(std::min(4097.f, std::floor(distance / spacing)));
            for (u32 step = 1; step <= steps; ++step) {
              auto between = sample;
              const float t = std::min(1.f, step * spacing / distance);
              between.x = previous.x + delta[0]*t;
              between.y = previous.y + delta[1]*t;
              between.z = previous.z + delta[2]*t;
              if (!append(between)) break;
            }
            if (!terrainGestureOverflow_ && event.phase == ui::UiPointerPhase::Up &&
                distance - steps * spacing > spacing * .1f)
              append(sample);
          }
          terrainGestureConnected_ = true;
          queueTerrainPreview();
          if (!terrainGestureOverflow_)
            state_.terrainMessage = "Traço · " + std::to_string(terrainGestureStrokes_.size()) +
                                    " amostras · prévia durante o gesto";
        } else {
          terrainGestureActive_ = false;
          terrainGestureStrokes_.clear();
          state_.terrainMessage =
              "Transformação com shear: ajuste o pai antes de pintar";
        }
      } else {
        terrainGestureConnected_ = false;
        state_.terrainCursorVisible=false;
        if (terrainGestureStrokes_.empty()) state_.terrainMessage = "Toque e arraste na superfície do terreno";
      }
    } else {
      terrainGestureConnected_ = false;
    }
    if (event.phase == ui::UiPointerPhase::Up && terrainGestureActive_) {
      terrainGestureActive_ = false;
      if (!terrainGestureOverflow_ && !terrainGestureStrokes_.empty()) {
        state_.terrainDraft.strokes.insert(state_.terrainDraft.strokes.end(),
                                          terrainGestureStrokes_.begin(), terrainGestureStrokes_.end());
        terrainGestureStrokes_.clear();
        startTerrainJob();
      }
    }
    return true;
  }
  const bool terrainControl = id >= base && id < base + 0x200;
  if(id>=widgetId(EditorWidget::TerrainSliderBase) && id<widgetId(EditorWidget::TerrainSliderBase)+20 && !state_.terrainBusy) {
    const auto field=id-widgetId(EditorWidget::TerrainSliderBase);
    if(routing.dragging && std::abs(routing.totalDelta.y)>std::abs(routing.totalDelta.x)) {
      if(event.phase==ui::UiPointerPhase::Move)
        state_.terrainScroll=std::clamp(state_.terrainScroll-routing.stepDelta.y,0.f,terrainScrollLimit(state_,terrainPanelRect(state_)));
    } else if(routing.tapped || (routing.dragging && event.phase==ui::UiPointerPhase::Move)) {
      auto area=terrainContentRect(state_,terrainPanelRect(state_));
      float t=std::clamp((event.position.x-area.x-8)/std::max(1.f,area.width-96),0.f,1.f);
      const float low=field==9?.1f:field==10?.05f:0;
      const float high=field==9?std::max(8.f,state_.terrainDraft.size*.25f):field==10?8.f:1.f;
      if(field==9 || field==10 || field==15 || field==19)setTerrainNumber(field,std::lerp(low,high,t));
    }
    return true;
  }
  const bool historyControl = id == widgetId(EditorWidget::Undo) || id == widgetId(EditorWidget::Redo);
  if ((terrainControl || historyControl) && !state_.terrainBusy && !state_.numericField && routing.dragging &&
      event.phase == ui::UiPointerPhase::Move &&
      terrainContentRect(state_, terrainPanelRect(state_)).contains(routing.start)) {
    state_.terrainScroll = std::clamp(state_.terrainScroll - routing.stepDelta.y,
                                     0.f, terrainScrollLimit(state_, terrainPanelRect(state_)));
    return true;
  }
  if (!terrainControl)
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
    clearTerrainPreview();
    state_.terrainCursorVisible=false;
    state_.terrainOpen = false;
    state_.terrainBrushArmed = false;
    return true;
  }
  if (state_.terrainBusy)
    return true;
  if (id >= widgetId(EditorWidget::TerrainNumberBase) &&
      id < widgetId(EditorWidget::TerrainNumberBase) + 20) {
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
  const auto minus = widgetId(EditorWidget::TerrainMinusBase), plus = widgetId(EditorWidget::TerrainPlusBase);
  if ((id >= minus && id < minus + 20) || (id >= plus && id < plus + 20)) {
    const bool increase = id >= plus;
    const u32 field = id - (increase ? plus : minus);
    const double steps[]{1,8,8,1,1,.25,1,8,.5,.5,.25,1,1,1,1,.1,15,.25,.05,.1};
    setTerrainNumber(field, number(state_,field) + steps[field] * (increase ? 1 : -1));
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainSectionBase) && id < widgetId(EditorWidget::TerrainSectionBase) + 3) {
    state_.terrainSection = id - widgetId(EditorWidget::TerrainSectionBase);
    state_.terrainScroll = 0;
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainModeBase) && id < widgetId(EditorWidget::TerrainModeBase) + 3) {
    if (!state_.terrainDraft.strokes.empty()) {
      state_.terrainMessage = "Crie outro terreno para trocar o tipo após esculpir";
      return true;
    }
    auto recipe = state_.terrainDraft;
    recipe.mode = resources::TerrainMode(id - widgetId(EditorWidget::TerrainModeBase));
    std::string error;
    if (resources::validTerrainRecipe(recipe,error)) {
      state_.terrainDraft = std::move(recipe);
      state_.terrainBrush.tool = resources::TerrainTool::Raise;
      normalizeTerrainBrush(state_);
      state_.terrainMessage = "Tipo alterado · aplique para atualizar";
    } else state_.terrainMessage = error;
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainPresetBase) && id < widgetId(EditorWidget::TerrainPresetBase) + 4) {
    const auto index = id - widgetId(EditorWidget::TerrainPresetBase);
    auto &r = state_.terrainDraft;
    const float amplitude[]{0,8,24,16}, frequency[]{2,2,4,3};
    r.height=amplitude[index];r.frequency=frequency[index];r.base=4;r.octaves=4;r.island=index==3;
    state_.terrainMessage="Relevo alterado · traços preservados · aplique";
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainTabBase) &&
      id < widgetId(EditorWidget::TerrainTabBase) + 4) {
    state_.terrainTab = id - widgetId(EditorWidget::TerrainTabBase);
    state_.terrainPage = 0;
    state_.terrainSection = 0;
    state_.terrainScroll = 0;
    if (state_.terrainTab == 2) state_.terrainBrush.tool = resources::TerrainTool::Paint;
    normalizeTerrainBrush(state_);
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainToolBase) &&
      id < widgetId(EditorWidget::TerrainToolBase) + 13) {
    state_.terrainBrush.tool =
        resources::TerrainTool(id - widgetId(EditorWidget::TerrainToolBase));
    normalizeTerrainBrush(state_);
    state_.terrainBrushArmed=state_.terrainTarget!=0;
    state_.terrainCursorVisible=false;
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainLayerBase) &&
      id < widgetId(EditorWidget::TerrainLayerBase) + 4) {
    state_.terrainBrush.layer = id - widgetId(EditorWidget::TerrainLayerBase);
    state_.terrainBrush.tool = resources::TerrainTool::Paint;
    state_.terrainBrushArmed=state_.terrainTarget!=0;
    return true;
  }
  if(id>=widgetId(EditorWidget::TerrainShapeBase) && id<widgetId(EditorWidget::TerrainShapeBase)+4) {
    state_.terrainBrush.shape=resources::TerrainBrushShape(id-widgetId(EditorWidget::TerrainShapeBase));
    normalizeTerrainBrush(state_);return true;
  }
  switch (EditorWidget(id)) {
  case EditorWidget::TerrainNavigate:
    state_.terrainBrushArmed=false;state_.terrainCursorVisible=false;clearTerrainPreview();break;
  case EditorWidget::TerrainSculpt:
    state_.terrainBrushArmed=state_.terrainTarget!=0;break;
  case EditorWidget::TerrainTouchOffset:
    state_.terrainTouchOffset=!state_.terrainTouchOffset;break;
  case EditorWidget::TerrainBlend:
    state_.terrainDraft.blendedSurface=!state_.terrainDraft.blendedSurface;break;
  case EditorWidget::TerrainGenerate:
    startTerrainJob();
    break;
  case EditorWidget::TerrainNew:
    openTerrain();
    break;
  case EditorWidget::TerrainLoad:
    if (state_.selection) openTerrain(state_.selection);
    else state_.terrainMessage="Selecione um terreno ou chunk para retomar";
    break;
  case EditorWidget::TerrainSurface:
  case EditorWidget::TerrainSolidBase:
    state_.terrainDraft.solidBase = EditorWidget(id) == EditorWidget::TerrainSolidBase;
    state_.terrainMessage="Estrutura alterada · aplique para atualizar";
    break;
  case EditorWidget::TerrainRevert:
    if (state_.terrainTarget) openTerrain(state_.terrainTarget);
    break;
  case EditorWidget::TerrainSave:
    if (state_.terrainTarget && resources::serializeTerrainRecipe(state_.terrainDraft) == state_.terrainAppliedRecipe) {
      state_.saveRequested = true;
      state_.terrainMessage="Salvamento da cena solicitado";
    } else state_.terrainMessage="Aplique ou reverta as alterações antes de salvar";
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
    if (state_.terrainTab == 2) state_.terrainBrush.tool = resources::TerrainTool::Paint;
    state_.terrainBrushArmed =
        state_.terrainTarget && !state_.terrainBrushArmed;
    break;
  case EditorWidget::TerrainStamp:
    if (!state_.terrainTarget) {
      state_.terrainMessage="Crie ou abra um terreno antes de aplicar o pincel";
    } else if (state_.terrainDraft.strokes.size() < 4096) {
      state_.terrainDraft.strokes.push_back(state_.terrainBrush);
      startTerrainJob();
    } else state_.terrainMessage="Limite de 4096 amostras · remova uma amostra antes de aplicar";
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

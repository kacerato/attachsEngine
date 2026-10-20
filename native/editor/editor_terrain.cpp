#include "resources/terrain_surface_texture_refs.h"
#include "core/sha256.h"
#include "editor/editor_import_transaction.h"
#include "editor/editor_screen.h"
#include "editor/editor_session.h"
#include "editor/editor_terrain_recovery.h"
#include "editor/terrain_controls.h"
#include "editor/editor_terrain_noise_controls.h"
#include "editor/editor_terrain_catalog_search.h"
#include "renderer/authoring_texture.h"
#include "resources/terrain_textures.generated.h"
#include "resources/terrain_vertex_index.h"
#include "scene/collider.h"
#include "scene/import_link.h"
#include "scene/physics_body.h"
#include <atomic>
#include <bit>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <thread>
#include <type_traits>
#include <unordered_set>

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
  if(terrainSnowNumberAvailable(s,i))return *terrainSnowNumber(r.surface,b.layer,s.terrainMaterialView,i);
  if(i>=128 && i<=154 && (terrainNoiseAvailable(r.surface,b.layer,s.terrainMaterialView) || (i>=149 && i<=151 && s.terrainMaterialView==30 && b.layer<r.surface.layers.size()) || (i>=152 && s.terrainMaterialView==50 && b.layer<r.surface.layers.size())))
    return *terrainNoiseNumber(r.surface,b.layer,s.terrainMaterialView,i);
  if(terrainClusterNumberAvailable(r.surface,b.layer,s.terrainMaterialView,i))
    return *terrainClusterNumber(r.surface,b.layer,s.terrainMaterialView,i,s.terrainMapRole);
  if(terrainModuleNumberAvailable(s,i)) {
    if(i==255) return s.terrainProfilePoint+1;
    return *terrainModuleNumber(r.surface,b.layer,s.terrainMaterialView,s.terrainMapRole,s.terrainProfilePoint,i);
  }
  if (i == 60)
    return r.controlCells;
  if (!r.surface.layers.empty() && b.layer < r.surface.layers.size()) {
    const auto &layer = r.surface.layers[b.layer];
    if (i >= 64 && i <= 76) {
      const double values[] = {
          layer.scaleX,         layer.scaleY,         layer.offsetX,
          layer.offsetY,        layer.rotation,       layer.tint[0],
          layer.tint[1],        layer.tint[2],        layer.saturation,
          layer.axisContrast,   layer.heightContrast, layer.heightWidth,
          layer.heightInfluence};
      return values[i - 64];
    }
    if (i >= 96 && i <= 110) {
      const auto &map = layer.maps[std::min(s.terrainMapRole, 6u)];
      if (i == 96)
        return map.factor;
      if (i == 97)
        return map.remapMin;
      if (i == 98)
        return map.remapMax;
      if (i <= 102)
        return map.colorFactor[i - 99];
      if (i <= 106)
        return map.colorRemapMin[i - 103];
      return map.colorRemapMax[i - 107];
    }
  }
  if (i >= 32 && i <= 51 && s.terrainPath < r.paths.size()) {
    const auto &p = r.paths[s.terrainPath];
    const auto &curve = s.terrainSection == 2 ? p.textureProfile : p.profile;
    const auto &node =
        curve[std::min(s.terrainProfilePoint, u32(curve.size() - 1))];
    if (i == 50)
      return node.x;
    if (i == 51)
      return node.y;
    const resources::TerrainPathPoint point =
        s.terrainPathPoint < p.points.size() ? p.points[s.terrainPathPoint]
                                             : resources::TerrainPathPoint{};
    const double pathValues[]{
        p.width,
        p.shoulderWidth,
        p.elevation,
        p.slopeLimit,
        p.uniformRamp,
        p.definedHeight,
        p.textureOpacity,
        p.shoulderOpacity,
        p.textureTransition,
        p.vegetationTransition,
        double(p.surfaceLayer),
        double(p.shoulderLayer),
        point.x,
        point.y,
        point.z,
        p.profile.size() > 2 ? p.profile[1].y : .5f,
        p.textureProfile.size() > 2 ? p.textureProfile[1].y : .5f,
        p.vegetationProfile.size() > 2 ? p.vegetationProfile[1].y : .5f,
        0};
    return pathValues[i - 32];
  }
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
                        b.z,
                        r.baseDepth,
                        b.hardness,
                        b.rotation,
                        b.aspect,
                        s.terrainSpacing,
                        r.macroVariation,
                        b.verticalAspect,
                        b.depthAspect,
                        b.pitch,
                        b.roll,
                        b.roundness,
                        b.opacity,
                        double(b.filterRadius),
                        b.surfaceDepth,
                        double(r.lodLevels),
                        double(r.lodRelativeError) * 100};
  return i < 30 ? values[i] : 0;
}
u64 textureRetainedBytes(const renderer::AuthoringTexture &texture) {
  return sizeof(texture) + texture.mipChain.capacity() +
         (texture.file ? sizeof(*texture.file) + texture.file->path.capacity()
                       : 0);
}
u64 terrainCandidateBytes(
    const resources::GltfImport &model,
    std::unordered_set<const renderer::AuthoringTexture *> resident,
    std::span<const resources::TerrainSurfaceImage> originals) {
  // Terrain GLBs are static. Count the retained geometry, decoded textures and
  // conservative metadata capacity before stageSource copies the whole library.
  // This bounds growth; it is not a process/GPU peak-memory guarantee.
  if (!model.skins.empty() || !model.animations.empty() ||
      !model.morphs.empty())
    return std::numeric_limits<u64>::max();
  const auto storage = [](const auto &values) {
    return u64(values.capacity()) *
           sizeof(typename std::decay_t<decltype(values)>::value_type);
  };
  u64 bytes = 1024 + storage(model.vertices) + storage(model.indices) +
              storage(model.draws) + storage(model.materials) +
              storage(model.terrainControls) + storage(model.names) +
              storage(model.nodes) + storage(model.drawNodes) +
              storage(model.materialNames) + storage(model.textures) +
              storage(model.textureImages) + storage(model.meshLods) +
              storage(model.drawSkins) + storage(model.drawMorphs) +
              storage(model.skinInfluences);
  bytes+=storage(model.terrainSurfaces)+storage(model.terrainSurfaceImages);
  for(const auto &binding:model.terrainSurfaces) {
    bytes+=storage(binding.textures)+storage(binding.surface.layers)+storage(binding.surface.antiTileArray.entries);
    for(const auto &layer:binding.surface.layers)bytes+=layer.name.capacity()+layer.sourceCatalogId.capacity()+
      layer.sourcePropertiesJson.capacity()+layer.sourcePackingJson.capacity()+layer.sourceResolvedMapsJson.capacity()+
      storage(layer.cluster.variants);
  }
  for(const auto &image:model.terrainSurfaceImages)if(image.encoded) {
    const bool shared=std::any_of(originals.begin(),originals.end(),[&](const auto &prior){
      return prior.identity==image.identity && prior.bytes().size()==image.bytes().size() &&
        std::equal(image.bytes().begin(),image.bytes().end(),prior.bytes().begin());
    });
    if(!shared)bytes+=sizeof(*image.encoded)+image.encoded->capacity();
  }
  bytes += u64(model.nodes.size()) * 2 * sizeof(resources::ImportNodeRecord);
  bytes += u64(model.draws.size()) * 4 * sizeof(resources::AssetGuid);
  for (const auto &node : model.nodes)
    bytes += 2 * (node.name.capacity() + node.authoredId.capacity()) +
             storage(node.morphWeights);
  for (const auto *names :
       {&model.names, &model.materialNames, &model.textureImages})
    for (const auto &name : *names)
      bytes += name.capacity();
  for (const auto &texture : model.textures)
    if (texture && resident.insert(texture.get()).second)
      bytes += textureRetainedBytes(*texture);
  return bytes;
}
bool terrainSourceCurrent(const EditorDocument &document, EditorEntityId target,
                          resources::AssetGuid source) {
  if (!target)
    return true;
  const auto *entity = document.find(target);
  const auto *link = entity ? scene::importLink(entity->components) : nullptr;
  return link && link->source == source;
}
void normalizeTerrainBrush(EditorScreenState &state) {
  const auto tool = state.terrainBrush.tool;
  const bool volume =
      state.terrainDraft.mode != resources::TerrainMode::Heightfield &&
      (tool == resources::TerrainTool::Dig ||
       tool == resources::TerrainTool::Add ||
       tool == resources::TerrainTool::Smooth ||
       tool == resources::TerrainTool::Sharpen ||
       tool == resources::TerrainTool::Restore ||
       tool == resources::TerrainTool::Paint);
  if (!volume &&
      (state.terrainBrush.shape == resources::TerrainBrushShape::Hemisphere ||
       state.terrainBrush.shape == resources::TerrainBrushShape::Cone))
    state.terrainBrush.shape = resources::TerrainBrushShape::Circle;
  if (state.terrainDraft.mode != resources::TerrainMode::Heightfield &&
      state.terrainBrush.tool == resources::TerrainTool::Hole)
    state.terrainBrush.tool = resources::TerrainTool::Dig;
  if (state.terrainDraft.mode == resources::TerrainMode::Heightfield &&
      (state.terrainBrush.tool == resources::TerrainTool::Dig ||
       state.terrainBrush.tool == resources::TerrainTool::Add))
    state.terrainBrush.tool = resources::TerrainTool::Raise;
}
} // namespace
namespace {
TerrainLibraryReader defaultTerrainLibraryReader() {
  std::error_code ec;
  auto root = std::filesystem::current_path(ec);
  if (ec)
    return {};
  for (unsigned i = 0; i < 8 && !root.empty(); ++i) {
    auto library = root / "assets/terrain/library";
    if (std::filesystem::is_regular_file(library / "catalog.json", ec)) {
      return [library](std::string_view relative, std::vector<u8> &out) {
        std::filesystem::path path;
        return EditorImportTransaction::safePath(library, std::string(relative),
                                                 path) &&
               EditorImportTransaction::read(path, out, 64u << 20);
      };
    }
    ec.clear();
    auto parent = root.parent_path();
    if (parent == root)
      break;
    root = std::move(parent);
  }
  return {};
}
resources::AssetGuid legacyTerrainTexture(u32 i) {
  return resources::assetGuidFromSeed("astra.terrain.legacy.texture:" +
                                      std::to_string(i));
}
void initializeTerrainSurface(resources::TerrainRecipe &recipe) {
  if (!recipe.surface.layers.empty())
    return;
  const char *names[] = {"Grama", "Rocha", "Terra", "Caminho"};
  for (u32 i = 0; i < 4; ++i) {
    resources::TerrainSurfaceLayer layer;
    layer.identity = resources::assetGuidFromSeed(
        "astra.terrain.legacy.layer:" + std::to_string(i));
    layer.name = names[i];
    layer.scaleX = layer.scaleY = recipe.textureScale;
    layer.maps[0].texture = legacyTerrainTexture(i);
    layer.maps[0].linear = false;
    layer.maps[0].channel = resources::TerrainSurfaceChannel::RGB;
    layer.maps[1].channel = resources::TerrainSurfaceChannel::NormalRGB;
    layer.maps[2].factor = 0;
    layer.maps[4].factor = .95f;
    layer.maps[5].factor = 0;
    layer.maps[6].factor = 0;
    layer.maps[6].channel = resources::TerrainSurfaceChannel::RGB;
    recipe.surface.layers.push_back(std::move(layer));
  }
}
} // namespace
std::shared_ptr<const TerrainMaterialCache> prepareTerrainMaterial(
    const resources::TerrainRecipe &recipe,
    const std::shared_ptr<const resources::TerrainSurfaceCatalog> &catalog,
    const TerrainLibraryReader &reader,
    const std::shared_ptr<const TerrainMaterialCache> &previous,
    std::string &error) {
  if (recipe.surface.layers.empty())
    return {};
  std::string key;
  if (!resources::serializeTerrainSurface(recipe.surface, key, error))
    return {};
  key +=
      "|metersPerUv=" + std::to_string(std::bit_cast<u32>(recipe.textureScale));
  if (previous && previous->key == key)
    return previous;
  auto result = std::make_shared<TerrainMaterialCache>();
  result->key = std::move(key);
  if(previous) {
    const auto currentRefs=resources::terrainSurfaceTextureRefs(recipe.surface);
    const auto previousRefs=resources::terrainSurfaceTextureRefs(previous->binding.surface);
    const bool same=currentRefs.size()==previousRefs.size() && std::equal(
      currentRefs.begin(),currentRefs.end(),previousRefs.begin(),[](const auto &a,const auto &b){
        return a.identity==b.identity && a.linear==b.linear && a.normal==b.normal && a.normalRG==b.normalRG &&
               a.normalChannel==b.normalChannel && a.residencyCap==b.residencyCap && a.clampSampling==b.clampSampling;
      });
    if(same) {
      result->images=previous->images;result->textures=previous->textures;result->binding=previous->binding;
      result->binding.surface=recipe.surface;result->binding.localMetersPerUv=recipe.textureScale;
      result->binding.terrainDomain={-recipe.size*.5f,-recipe.size*.5f,recipe.size,recipe.size};
      error.clear();return result;
    }
  }
  std::vector<resources::AssetGuid> references;
  std::vector<resources::TerrainSurfaceDiagnostic> diagnostics;
  if (!resources::resolveTerrainSurface(
          recipe.surface,
          [&](resources::AssetGuid id) {
            if (std::find(references.begin(), references.end(), id) ==
                references.end())
              references.push_back(id);
            return true;
          },
          diagnostics, error))
    return {};
  u64 total = 0;
  for (auto id : references) {
    resources::TerrainSurfaceImage image;
    image.identity = id;
    std::vector<u8> encoded;
    bool found = false;
    if (previous)
      for (const auto &old : previous->images)
        if (old.identity == id) {
          image = old;
          found = true;
          break;
        }
    if (!found)
      for (u32 i = 0; i < 4; ++i)
        if (legacyTerrainTexture(i) == id) {
          image = resources::TerrainSurfaceImage(id, textures()[i]);
          found = true;
          break;
        }
    if (!found && catalog && reader)
      for (const auto &entry : catalog->textures)
        if (entry.identity == id) {
          if (!reader(entry.runtimePath, encoded) ||
              Sha256::hex(encoded) != entry.runtimeHash) {
            error = "Textura original do catálogo ausente ou alterada: " +
                    entry.runtimePath;
            return {};
          }
          image = resources::TerrainSurfaceImage(id, std::move(encoded));
          found = true;
          break;
        }
    if (!found) {
      error = "Textura da camada indisponível: " + id.text();
      return {};
    }
    if (image.bytes().empty() || image.bytes().size() > (128u << 20) - total) {
      error =
          "Imagens do terreno excedem 128 MiB; qualidade original preservada.";
      return {};
    }
    total += image.bytes().size();
    result->images.push_back(std::move(image));
  }
  if (!resources::importTerrainSurface(
          recipe.surface, result->images, recipe.textureScale, 128u << 20,
          result->textures, result->binding, error,
          {-recipe.size*.5f,-recipe.size*.5f,recipe.size,recipe.size}))
    return {};
  return result;
}
void EditorSession::setTerrainLibraryReader(TerrainLibraryReader reader) {
  terrainLibraryReader_ = std::move(reader);
  state_.terrainCatalog.reset();
  terrainMaterialCache_.reset();
  clearTerrainPreview();
}
bool EditorSession::loadTerrainCatalog() {
  if (state_.terrainCatalog)
    return true;
  if (!terrainLibraryReader_)
    terrainLibraryReader_ = defaultTerrainLibraryReader();
  std::vector<u8> bytes;
  if (!terrainLibraryReader_ || !terrainLibraryReader_("catalog.json", bytes) ||
      bytes.size() > (8u << 20)) {
    state_.terrainMessage = "Biblioteca de materiais indisponível.";
    return false;
  }
  auto catalog = std::make_shared<resources::TerrainSurfaceCatalog>();
  std::string error;
  if (!resources::loadTerrainSurfaceCatalog(
          std::string_view(reinterpret_cast<const char *>(bytes.data()),
                           bytes.size()),
          *catalog, error)) {
    state_.terrainMessage = error;
    return false;
  }
  state_.terrainCatalog = std::move(catalog);
  return true;
}
bool EditorSession::selectTerrainCatalogMaterial(u32 index) {
  if (!state_.terrainOpen || state_.terrainBusy || !loadTerrainCatalog() ||
      index >= state_.terrainCatalog->materials.size() ||
      state_.terrainBrush.layer >= 4)
    return false;
  auto recipe = state_.terrainDraft;
  initializeTerrainSurface(recipe);
  if (recipe.surface.layers.size() != 4) {
    state_.terrainMessage =
        "Este editor usa quatro canais de pintura; superfície preservada.";
    return false;
  }
  const auto &material = state_.terrainCatalog->materials[index];
  recipe.surface.layers[state_.terrainBrush.layer] = material.layer;
  // Slot identity belongs to the authored control channel, not its current
  // catalog material.
  recipe.surface.layers[state_.terrainBrush.layer].identity =
      state_.terrainDraft.surface.layers.empty()
          ? resources::assetGuidFromSeed(
                "astra.terrain.legacy.layer:" +
                std::to_string(state_.terrainBrush.layer))
          : state_.terrainDraft.surface.layers[state_.terrainBrush.layer]
                .identity;
  recipe.blendedSurface = true;
  std::string error;
  if (!resources::validTerrainRecipe(recipe, error)) {
    state_.terrainMessage = error;
    return false;
  }
  state_.terrainDraft = std::move(recipe);
  state_.terrainMaterialView = 2;
  state_.terrainScroll = 0;
  state_.terrainMessage = "Material selecionado · prévia em qualidade original";
  return true;
}
struct EditorSession::TerrainJob {
  resources::TerrainRecipe recipe;
  std::shared_ptr<const resources::TerrainBuild> previous, build;
  resources::GltfImport model;
  std::shared_ptr<const TerrainMaterialCache> material, previousMaterial;
  std::shared_ptr<const resources::TerrainSurfaceCatalog> catalog;
  TerrainLibraryReader reader;
  std::vector<u8> bytes;
  std::string error, project;
  u64 epoch = 0, revision = 0;
  EditorEntityId target = 0;
  std::atomic<bool> cancel{false}, ready{false};
  std::atomic<float> progress{0};
};
struct EditorSession::TerrainPreviewJob {
  resources::TerrainRecipe recipe;
  std::shared_ptr<const resources::TerrainBuild> previous;
  std::shared_ptr<resources::TerrainBuild> build =
      std::make_shared<resources::TerrainBuild>();
  std::string error, key;
  std::shared_ptr<const TerrainMaterialCache> material, previousMaterial;
  std::shared_ptr<const resources::TerrainSurfaceCatalog> catalog;
  TerrainLibraryReader reader;
  u64 epoch = 0, revision = 0;
  EditorEntityId target = 0;
  usize samples = 0;
  bool authoringOnly = false;
  std::atomic<bool> ready{false}, cancel{false};
};
void EditorSession::clearTerrainPreview(bool restore) {
  if (terrainPreviewJob_)
    terrainPreviewJob_->cancel = true;
  terrainPreviewJob_.reset();
  if (!state_.terrainOpen || !terrainPreviewCache_ ||
      resources::serializeTerrainRecipe(terrainPreviewCache_->recipe) !=
          state_.terrainAppliedRecipe) {
    terrainPreviewCache_.reset();
    state_.terrainPreviewBuild.reset();
  }
  const bool published = terrainPreviewCount_ != 0;
  terrainPreviewCount_ = terrainPreviewFirst_ = 0;
  terrainPreviewSamples_ = 0;
  terrainPreviewRecipe_.clear();
  state_.terrainLivePreview = false;
  if (published && restore && terrainPreviewEpoch_ == sceneEpoch_) {
    std::string error;
    if (!publishAndAdopt(flattenSources(importedSources_), error, nullptr,
                         true))
      state_.terrainMessage = "Falha ao restaurar prévia: " + error;
    appearanceChanged_ = true;
  }
}
void EditorSession::queueTerrainPreview() {
  if (terrainPreviewJob_ || !state_.terrainOpen || state_.terrainBusy ||
      !state_.terrainTarget)
    return;
  auto recipe = state_.terrainDraft;
  if (terrainGestureActive_)
    recipe.strokes.insert(recipe.strokes.end(), terrainGestureStrokes_.begin(),
                          terrainGestureStrokes_.end());
  const auto key = resources::serializeTerrainRecipe(recipe);
  const bool authoringOnly = key == state_.terrainAppliedRecipe;
  if (key == terrainPreviewRecipe_ ||
      (authoringOnly && (state_.terrainPreviewBuild || state_.terrainTab != 4 ||
                         recipe.paths.empty())))
    return;
  auto job = std::make_shared<TerrainPreviewJob>();
  job->recipe = std::move(recipe);
  job->key = key;
  job->epoch = sceneEpoch_;
  job->revision = document_.revision();
  job->target = state_.terrainTarget;
  job->samples = terrainGestureStrokes_.size();
  job->authoringOnly = authoringOnly;
  if (terrainPreviewCache_ && !terrainPreviewCache_->mesh.parts.empty())
    job->previous = terrainPreviewCache_;
  if (!job->recipe.surface.layers.empty() && !terrainMaterialCache_ &&
      !loadTerrainCatalog())
    return;
  job->catalog = state_.terrainCatalog;
  job->reader = terrainLibraryReader_;
  job->previousMaterial = terrainMaterialCache_;
  terrainPreviewJob_ = job;
  std::thread([job] {
    if (!job->recipe.surface.layers.empty()) {
      job->material =
          prepareTerrainMaterial(job->recipe, job->catalog, job->reader,
                                 job->previousMaterial, job->error);
      if (!job->material) {
        job->ready.store(true, std::memory_order_release);
        return;
      }
    }
    if (job->authoringOnly) {
      job->build->recipe = job->recipe;
      resources::generateTerrainField(
          job->recipe, job->build->field, job->error,
          [job](float) { return !job->cancel.load(); });
    } else
      resources::updateTerrain(job->recipe, job->previous.get(), *job->build,
                               job->error,
                               [job](float) { return !job->cancel.load(); });
    job->ready.store(true, std::memory_order_release);
  }).detach();
}
bool EditorSession::publishTerrainPreview(const resources::TerrainBuild &build,
                                          std::string &error) {
  const auto &mesh = build.mesh;
  const auto source =
      std::find_if(importedSources_.begin(), importedSources_.end(),
                   [&](const auto &s) { return s.guid == terrainSource_; });
  if (source == importedSources_.end())
    return false;
  ImportedSource preview;
  preview.guid = resources::assetGuidFromSeed("terrain-transient-preview");
  preview.materials = source->materials;
  preview.textures = source->textures;
  preview.terrainControls.resize(preview.materials.size());
  // The draft may switch a legacy terrain to blended layers before Apply.
  // Rebind the same decoded albedos without a GLB encode/decode round trip.
  if (build.recipe.surface.layers.empty() && preview.materials.size() >= 4) {
    u32 layers[4];
    const bool oldBlend =
        (source->materials[0].flags & renderer::MapMaterialTerrain) != 0;
    for (u32 i = 0; i < 4; ++i)
      layers[i] = oldBlend ? source->materials[0].textureIndices[i]
                           : source->materials[i].textureIndices[0];
    for (u32 i = 0; i < 4; ++i) {
      auto &material = preview.materials[i];
      material.flags &=
          ~(renderer::MapMaterialTerrain | renderer::MapMaterialNormalMap |
            renderer::MapMaterialMetallicRoughnessMap |
            renderer::MapMaterialEmissiveMap);
      for (u32 slot = 0; slot < 4; ++slot)
        material.textureIndices[slot] =
            state_.terrainDraft.blendedSurface ? layers[slot]
            : slot == 0                        ? layers[i]
                                               : renderer::InvalidMapTexture;
      if (state_.terrainDraft.blendedSurface)
        material.flags |= renderer::MapMaterialTerrain;
    }
  }
  if (!build.recipe.surface.layers.empty()) {
    if (!terrainMaterialCache_) {
      error = "Materiais da prévia ainda não preparados.";
      return false;
    }
    preview.textures = terrainMaterialCache_->textures;
    preview.terrainSurfaces.resize(preview.materials.size(),
                                   terrainMaterialCache_->binding);
    for (auto &material : preview.materials) {
      material.flags &= ~(renderer::MapMaterialNormalMap |
                          renderer::MapMaterialMetallicRoughnessMap |
                          renderer::MapMaterialEmissiveMap);
      material.flags |= renderer::MapMaterialTerrain;
      std::fill_n(material.textureIndices, 4, renderer::InvalidMapTexture);
      std::fill_n(material.baseColorFactor, 4, 1.f);
    }
  }
  if (build.recipe.blendedSurface && build.recipe.controlCells &&
      build.field.height == 1 && build.field.controlWidth) {
    renderer::SharedAuthoringTexture control;
    if (!resources::makeTerrainControlTexture(build.field, control, error))
      return false;
    const auto binding = resources::terrainControlBinding(
        build.recipe, build.field, u32(preview.textures.size()));
    preview.textures.push_back(std::move(control));
    for (usize m = 0; m < preview.materials.size(); ++m)
      if (preview.materials[m].flags & renderer::MapMaterialTerrain)
        preview.terrainControls[m] = binding;
  }
  // No file encoding, decoding, asset registry, entity or history mutation.
  struct Vertex {
    float p[3];
    i16 n[4], t[4];
    float uv[2], uv1[2];
    u8 color[4];
  };
  static_assert(sizeof(Vertex) == renderer::MapVertexStride);
  for (const auto &part : mesh.parts) {
    renderer::MapDrawRecord draw{};
    draw.firstIndex = u32(preview.indices.size());
    draw.indexCount = u32(part.vertices.size());
    draw.vertexOffset = u32(preview.vertices.size() / sizeof(Vertex));
    draw.materialIndex = part.layer;
    draw.model[0] = draw.model[5] = draw.model[10] = draw.model[15] = 1;
    float lo[3]{INFINITY, INFINITY, INFINITY},
        hi[3]{-INFINITY, -INFINITY, -INFINITY};
    std::vector<resources::TerrainVertex> unique;
    std::vector<u32> indices;
    resources::indexTerrainVertices(part.vertices, unique, indices);
    for (const auto &v : unique) {
      Vertex packed{};
      for (u32 a = 0; a < 3; ++a) {
        packed.p[a] = v.position[a];
        packed.n[a] = i16(std::clamp(v.normal[a], -1.f, 1.f) * 32767);
        lo[a] = std::min(lo[a], v.position[a]);
        hi[a] = std::max(hi[a], v.position[a]);
      }
      packed.t[0] = packed.t[3] = 32767;
      std::copy_n(v.uv, 2, packed.uv);
      std::copy_n(v.uv1, 2, packed.uv1);
      for (u32 a = 0; a < 4; ++a)
        packed.color[a] =
            u8(std::lround(std::clamp(v.weights[a], 0.f, 1.f) * 255));
      auto offset = preview.vertices.size();
      preview.vertices.resize(offset + sizeof(packed));
      std::memcpy(preview.vertices.data() + offset, &packed, sizeof(packed));
    }
    preview.indices.insert(preview.indices.end(), indices.begin(),
                           indices.end());
    float radius2 = 0;
    for (u32 a = 0; a < 3; ++a) {
      draw.boundsCenter[a] = (lo[a] + hi[a]) * .5f;
      radius2 += (hi[a] - lo[a]) * (hi[a] - lo[a]) * .25f;
    }
    draw.boundsRadius = std::sqrt(radius2);
    draw.lodGroupId = u32(preview.draws.size());
    preview.identities.push_back(resources::assetGuidFromSeed(
        "terrain-preview:" + std::to_string(preview.draws.size())));
    preview.names.push_back("Prévia de terreno");
    preview.draws.push_back(draw);
  }
  auto sources = importedSources_;
  usize previous = 0, textureBase = 0;
  for (const auto &s : sources) {
    previous += s.draws.size();
    textureBase += s.textures.size();
  }
  sources.push_back(std::move(preview));
  auto library = flattenSources(sources);
  // Share immutable albedos under the exact same streaming policy; only the
  // generated control image changes during a stroke. The publication interner
  // remaps both kinds of binding in one transaction, including the new map.
  for (usize i = textureBase; i < library.textureSources.size(); ++i)
    library.textureSources[i] = terrainSource_;
  usize primitives = 0;
  if (!publishAndAdopt(library, error, &primitives, true))
    return false;
  terrainPreviewFirst_ = u32(primitives + previous);
  terrainPreviewCount_ = u32(mesh.parts.size());
  terrainPreviewEpoch_ = sceneEpoch_;
  state_.terrainLivePreview = true;
  ++state_.terrainPreviewUpdates;
  appearanceChanged_ = true;
  return true;
}
void EditorSession::refreshTerrainPreview() {
  if (!terrainGestureActive_ && !state_.terrainBusy &&
      (!state_.terrainOpen ||
       resources::serializeTerrainRecipe(state_.terrainDraft) ==
           state_.terrainAppliedRecipe)) {
    if (terrainPreviewCount_ ||
        (terrainPreviewJob_ && !terrainPreviewJob_->authoringOnly) ||
        !state_.terrainOpen)
      clearTerrainPreview();
    if (!state_.terrainOpen || state_.terrainPreviewBuild ||
        state_.terrainTab != 4 || state_.terrainDraft.paths.empty())
      return;
  }
  if (!terrainPreviewJob_) {
    queueTerrainPreview();
    return;
  }
  auto job = terrainPreviewJob_;
  if (job->epoch != sceneEpoch_ || job->revision != document_.revision() ||
      job->target != state_.terrainTarget || !state_.terrainOpen ||
      isPlaying()) {
    terrainGestureActive_ = false;
    terrainGestureStrokes_.clear();
    clearTerrainPreview();
    return;
  }
  if (!job->ready.load(std::memory_order_acquire))
    return;
  terrainPreviewJob_.reset();
  terrainPreviewSamples_ = job->samples;
  terrainPreviewRecipe_ = job->key;
  if (!job->error.empty())
    state_.terrainMessage = "Prévia: " + job->error;
  auto current = state_.terrainDraft;
  if (terrainGestureActive_)
    current.strokes.insert(current.strokes.end(),
                           terrainGestureStrokes_.begin(),
                           terrainGestureStrokes_.end());
  if (!job->cancel && job->error.empty() &&
      job->key == resources::serializeTerrainRecipe(current)) {
    std::string error;
    terrainMaterialCache_ = job->material;
    terrainPreviewCache_ = job->build;
    state_.terrainPreviewBuild = job->build;
    if (!job->authoringOnly && !publishTerrainPreview(*job->build, error))
      state_.terrainMessage = "Prévia: " + error;
  }
  queueTerrainPreview();
}
void EditorSession::refreshTerrainBudget() {
  state_.terrainRetainedBytes = state_.terrainRetainedDraws = 0;
  state_.terrainRetainedBytesKnown = true;
  std::unordered_set<const renderer::AuthoringTexture *> resident;
  std::unordered_set<const std::vector<u8> *> originalImages;
  for (const auto &source : importedSources_) {
    state_.terrainRetainedDraws += source.draws.size();
    auto bytes = retainedModelBytes(source);
    if (bytes != std::numeric_limits<u64>::max())
      for (const auto &texture : source.textures)
        if (texture && !resident.insert(texture.get()).second)
          bytes -= textureRetainedBytes(*texture);
    if(bytes!=std::numeric_limits<u64>::max())
      for(const auto &image:source.terrainSurfaceImages)
        if(image.encoded && !originalImages.insert(image.encoded.get()).second)
          bytes-=sizeof(*image.encoded)+image.encoded->capacity();
    if (bytes == std::numeric_limits<u64>::max() ||
        bytes > std::numeric_limits<u64>::max() - state_.terrainRetainedBytes) {
      state_.terrainRetainedBytesKnown = false;
      state_.terrainRetainedBytes =
          EditorScreenState::TerrainRetainedByteLimit + 1;
    } else if (state_.terrainRetainedBytesKnown)
      state_.terrainRetainedBytes += bytes;
  }
}
EditorEntityId EditorSession::terrainRoot(EditorEntityId target) {
  if (terrainAccessProject_ != files_.rootPath()) {
    terrainAccessProject_ = files_.rootPath();
    terrainAccess_.clear();
  }
  // Resolve authoring through ancestors, including attachments or nested
  // imported models. The recipe identifies a terrain; renaming its group or GLB
  // does not.
  for (usize depth = 0; target && depth < document_.entityCount(); ++depth) {
    const auto *entity = document_.find(target);
    if (!entity)
      break;
    const auto *link = scene::importLink(entity->components);
    const auto *record =
        link && link->root ? assets_.find(link->source) : nullptr;
    if (record) {
      auto found = std::find_if(
          terrainAccess_.begin(), terrainAccess_.end(),
          [&](const auto &entry) { return entry.source == record->guid; });
      if (found == terrainAccess_.end()) {
        terrainAccess_.push_back({record->guid, {}, {}, false});
        found = std::prev(terrainAccess_.end());
      }
      if (found->path != record->path || found->hash != record->contentHash) {
        found->path = record->path;
        found->hash = record->contentHash;
        resources::TerrainRecipe recipe;
        std::string error;
        bool recovered = false;
        found->editable =
            readTerrainAuthoring(files_.rootPath(), record->path,
                                 record->contentHash, recipe, error, recovered);
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
  bool restoredFromHistory = false;
  if (target) {
    // Explicit reopening retries a restored file even if its registry hash did
    // not change. Automatic refresh checks this per-session negative cache
    // first.
    for (auto ancestor = target, depth = EditorEntityId{0};
         ancestor && depth < document_.entityCount(); ++depth) {
      const auto *entity = document_.find(ancestor);
      if (!entity)
        break;
      const auto *link = scene::importLink(entity->components);
      if (link && link->root)
        terrainAccess_.erase(
            std::remove_if(terrainAccess_.begin(), terrainAccess_.end(),
                           [&](const auto &entry) {
                             return entry.source == link->source &&
                                    !entry.editable;
                           }),
            terrainAccess_.end());
      ancestor = entity->parent;
    }
    const auto id = terrainRoot(target);
    const auto *entity = document_.find(id);
    const auto *link = entity ? scene::importLink(entity->components) : nullptr;
    const auto *record = link ? assets_.find(link->source) : nullptr;
    std::string error;
    resources::TerrainRecipe recipe;
    if (!record || !readTerrainAuthoring(files_.rootPath(), record->path,
                                         record->contentHash, recipe, error,
                                         restoredFromHistory)) {
      if (record)
        for (auto &entry : terrainAccess_)
          if (entry.source == record->guid)
            entry.editable = false;
      state_.status = error.empty()
                          ? "Selecione a raiz ou um chunk do terreno Astra"
                          : error;
      state_.terrainMessage = state_.status;
      return false;
    }
    state_.terrainDraft = std::move(recipe);
    state_.terrainTarget = id;
    terrainSource_ = record->guid;
    state_.terrainMessage =
        std::string(restoredFromHistory ? "Receita recuperada do histórico · "
                                          "aplique para reconstruir · "
                                        : "Receita carregada · ") +
        std::to_string(state_.terrainDraft.strokes.size()) + " operações";
  } else {
    state_.terrainDraft = {};
    state_.terrainDraft.controlCells = 128;
    // A blank canvas with enough samples across the initial soft brush.
    // Saved recipes retain their own amplitude and resolution on reopening.
    state_.terrainDraft.height = 0;
    state_.terrainDraft.cells = 64;
    state_.terrainDraft.solidBase = true;
    state_.terrainBrush = {};
    state_.terrainBrush.radius = 8;
    state_.terrainBrush.strength = .25f;
    state_.terrainSpacing = .2f;
    state_.terrainTarget = 0;
    terrainSource_ = {};
    state_.terrainMessage = "Novo terreno · configure e gere";
  }
  terrainMaterialCache_.reset();
  if (!state_.terrainDraft.surface.layers.empty()) {
    const auto source = std::find_if(
        importedSources_.begin(), importedSources_.end(),
        [&](const auto &entry) { return entry.guid == terrainSource_; });
    if (source != importedSources_.end() &&
        !source->terrainSurfaceImages.empty()) {
      auto material = std::make_shared<TerrainMaterialCache>();
      material->images = source->terrainSurfaceImages;
      terrainMaterialCache_ = std::move(material);
    }
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
  state_.terrainAppliedRecipe =
      target && !restoredFromHistory
          ? resources::serializeTerrainRecipe(state_.terrainDraft)
          : std::string{};
  if (terrainPreviewCache_ &&
      resources::serializeTerrainRecipe(terrainPreviewCache_->recipe) !=
          state_.terrainAppliedRecipe) {
    terrainPreviewCache_.reset();
    state_.terrainPreviewBuild.reset();
  }
  return true;
}
bool EditorSession::setTerrainNumber(u32 field, double value) {
  if (!state_.terrainOpen || state_.terrainBusy || !std::isfinite(value) ||
      (field >= 52 && field != 60 && !(field >= 64 && field <= 76) &&
       !(field >= 96 && field <= 110) && !(field >= 128 && field <= 154) && !terrainClusterField(field) && !terrainModuleField(field) && !terrainSnowField(field)))
    return false;
  if ((field == 0 || field == 2 || field == 6 || field == 7 || field == 28 ||
       field == 60) &&
      (value < 0 || value > 4294967295.0 || std::floor(value) != value))
    return false;
  auto r = state_.terrainDraft;
  auto b = state_.terrainBrush;
  if(terrainSnowField(field)) {
    if(!terrainSnowNumberAvailable(state_,field))return false;
    auto *target=terrainSnowNumber(r.surface,b.layer,state_.terrainMaterialView,field);
    if(!target)return false;*target=float(value);
    std::string error;if(!resources::validTerrainRecipe(r,error)){state_.terrainMessage=error;return false;}
    state_.terrainDraft=std::move(r);state_.terrainMessage="Neve ajustada · prévia antes de aplicar";return true;
  }
  if(terrainModuleField(field)) {
    if(!terrainModuleNumberAvailable(state_,field)) return false;
    if(field==255) {
      const auto count=r.surface.geo.curve.keys.size();
      if(value<1 || value>count || std::floor(value)!=value) return false;
      state_.terrainProfilePoint=u32(value)-1;return true;
    }
    *terrainModuleNumber(r.surface,b.layer,state_.terrainMaterialView,state_.terrainMapRole,state_.terrainProfilePoint,field)=float(value);
    if(field==208 || field==209) {
      auto &key=r.surface.geo.curve.keys[std::min<usize>(state_.terrainProfilePoint,r.surface.geo.curve.keys.size()-1)];
      if(!key.broken) {if(field==208) key.outSlope=key.inSlope;else key.inSlope=key.outSlope;}
    }
    std::string error;
    if(!resources::validTerrainRecipe(r,error)) {state_.terrainMessage=error;return false;}
    state_.terrainDraft=std::move(r);state_.terrainMessage="Estratos e mapas ajustados · prévia antes de aplicar";return true;
  }
  if(terrainClusterField(field)) {
    if(!terrainClusterNumberAvailable(r.surface,b.layer,state_.terrainMaterialView,field)) return false;
    if(field>=176 && state_.terrainMapRole==1){const auto &map=r.surface.layers[b.layer].cluster.variants[terrainClusterVariant(state_.terrainMaterialView)].maps[1];if(!terrainNormalMapNumberAvailable(map.channel,field,176,177,179))return false;}
    *terrainClusterNumber(r.surface,b.layer,state_.terrainMaterialView,field,state_.terrainMapRole)=float(value);
    std::string error;
    if(!resources::validTerrainRecipe(r,error)) {state_.terrainMessage=error;return false;}
    state_.terrainDraft=std::move(r);state_.terrainMessage="Variantes ajustadas · prévia antes de aplicar";return true;
  }
  if(field>=128 && field<=154) {
    if(!(terrainNoiseAvailable(r.surface,b.layer,state_.terrainMaterialView) || (field>=149 && field<=151 && state_.terrainMaterialView==30 && b.layer<r.surface.layers.size()) || (field>=152 && state_.terrainMaterialView==50 && b.layer<r.surface.layers.size()))) return false;
    if(field>=133 && field<=147 && terrainNoiseAvailable(r.surface,b.layer,state_.terrainMaterialView) && terrainNoiseNormal(state_.terrainMaterialView)){
      const auto &map=terrainSelectedNoise(r.surface,b.layer,state_.terrainMaterialView).map;if(!terrainNormalMapNumberAvailable(map.channel,field,133,134,136))return false;
    }
    *terrainNoiseNumber(r.surface,b.layer,state_.terrainMaterialView,field)=float(value);
    std::string error;
    if(!resources::validTerrainRecipe(r,error)) { state_.terrainMessage=error; return false; }
    state_.terrainDraft=std::move(r);
    state_.terrainMessage="Ruído ajustado · prévia antes de aplicar";
    return true;
  }
  if ((field >= 64 && field <= 76) || (field >= 96 && field <= 110)) {
    if (b.layer >= r.surface.layers.size())
      return false;
    auto &layer = r.surface.layers[b.layer];
    const float v = float(value);
    if (field >= 64 && field <= 76) {
      float *values[] = {
          &layer.scaleX,         &layer.scaleY,         &layer.offsetX,
          &layer.offsetY,        &layer.rotation,       &layer.tint[0],
          &layer.tint[1],        &layer.tint[2],        &layer.saturation,
          &layer.axisContrast,   &layer.heightContrast, &layer.heightWidth,
          &layer.heightInfluence};
      *values[field - 64] = v;
    } else {
      auto &map = layer.maps[std::min(state_.terrainMapRole, 6u)];
      if(state_.terrainMapRole==1 && !terrainNormalMapNumberAvailable(map.channel,field,96,97,99))return false;
      if (field == 96)
        map.factor = v;
      else if (field == 97)
        map.remapMin = v;
      else if (field == 98)
        map.remapMax = v;
      else if (field <= 102)
        map.colorFactor[field - 99] = v;
      else if (field <= 106)
        map.colorRemapMin[field - 103] = v;
      else
        map.colorRemapMax[field - 107] = v;
    }
    std::string error;
    if (!resources::validTerrainRecipe(r, error)) {
      state_.terrainMessage = error;
      return false;
    }
    state_.terrainDraft = std::move(r);
    state_.terrainMessage = "Material ajustado · prévia antes de aplicar";
    return true;
  }
  if (field >= 32 && field <= 51) {
    if (state_.terrainPath >= r.paths.size())
      return false;
    auto &p = r.paths[state_.terrainPath];
    const float v = float(value);
    switch (field) {
    case 32:
      if (p.keepWidthRatio && p.width > 0)
        p.shoulderWidth *= v / p.width;
      p.width = v;
      break;
    case 33:
      p.shoulderWidth = v;
      break;
    case 34:
      p.elevation = v;
      break;
    case 35:
      p.slopeLimit = v;
      break;
    case 36:
      p.uniformRamp = v;
      break;
    case 37:
      p.definedHeight = v;
      break;
    case 38:
      p.textureOpacity = v;
      break;
    case 39:
      p.shoulderOpacity = v;
      break;
    case 40:
      p.textureTransition = v;
      break;
    case 41:
      p.vegetationTransition = v;
      break;
    case 42:
    case 43:
      if (v < 0 || v > 3 || std::floor(v) != v)
        return false;
      (field == 42 ? p.surfaceLayer : p.shoulderLayer) = u32(v);
      break;
    case 44:
    case 45:
    case 46:
      if (state_.terrainPathPoint >= p.points.size())
        return false;
      if (field == 44)
        p.points[state_.terrainPathPoint].x = v;
      else if (field == 45)
        p.points[state_.terrainPathPoint].y = v;
      else
        p.points[state_.terrainPathPoint].z = v;
      break;
    case 47:
      p.profile = {{0, 1}, {.5f, v}, {1, 0}};
      break;
    case 48:
      p.textureProfile = {{0, 1}, {.5f, v}, {1, 0}};
      break;
    case 49:
      p.vegetationProfile = {{0, 1}, {.5f, v}, {1, 0}};
      break;
    case 50:
    case 51: {
      auto &curve = state_.terrainSection == 2 ? p.textureProfile : p.profile;
      const auto index =
          std::min(state_.terrainProfilePoint, u32(curve.size() - 1));
      if (field == 50)
        curve[index].x = v;
      else
        curve[index].y = v;
    } break;
    }
    std::string error;
    if (!resources::validTerrainRecipe(r, error)) {
      state_.terrainMessage = error;
      return false;
    }
    state_.terrainDraft = std::move(r);
    state_.terrainMessage = "Caminho ajustado · prévia antes de aplicar";
    return true;
  }
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
  case 15:
    b.hardness = f;
    break;
  case 16:
    b.rotation = f;
    break;
  case 17:
    b.aspect = f;
    break;
  case 18:
    if (f < .05f || f > 1)
      return false;
    state_.terrainSpacing = f;
    return true;
  case 19:
    r.macroVariation = f;
    break;
  case 20:
    b.verticalAspect = f;
    break;
  case 21:
    b.depthAspect = f;
    break;
  case 22:
    b.pitch = f;
    break;
  case 23:
    b.roll = f;
    break;
  case 24:
    b.roundness = f;
    break;
  case 25:
    b.opacity = f;
    break;
  case 26:
    if (f < 1 || f > 4 || std::floor(f) != f)
      return false;
    b.filterRadius = u32(f);
    break;
  case 27:
    b.surfaceDepth = f;
    break;
  case 28:
    r.lodLevels = u32(value);
    break;
  case 29:
    r.lodRelativeError = f * .01f;
    break;
  case 60:
    if (r.mode != resources::TerrainMode::Heightfield)
      return false;
    r.controlCells = u32(value);
    break;
  default:
    return false;
  }
  std::string error;
  if (b.dimensionsLinked && (field == 17 || field == 20 || field == 21))
    b.aspect = b.verticalAspect = b.depthAspect = f;
  if (!resources::validTerrainRecipe(r, error) ||
      !resources::validTerrainStroke(b, error)) {
    state_.terrainMessage =
        error.empty() ? "Valor fora dos limites do pincel" : error;
    return false;
  }
  state_.terrainDraft = std::move(r);
  state_.terrainBrush = b;
  state_.terrainMessage =
      (field >= 9 && field <= 13) || (field >= 15 && field <= 18) ||
              (field >= 20 && field <= 27)
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
  // The GLB remains the scene/Undo resource. A separate recipe chain can
  // reconstruct authoring when its source file is lost or truncated.
  std::string previousHash;
  if (const auto *old = assets_.find(terrainSource_)) {
    resources::TerrainRecipe previousRecipe;
    bool recovered = false;
    if (readTerrainAuthoring(files_.rootPath(), old->path, old->contentHash,
                             previousRecipe, error, recovered)) {
      previousHash = old->contentHash;
      if (!writeTerrainRecovery(files_.rootPath(), previousHash, previousRecipe,
                                {}, error))
        return false;
    } else {
      error = "Receita anterior indisponível; revisão preservada.";
      return false;
    }
  }
  if (!writeTerrainRecovery(files_.rootPath(), hash, r, previousHash, error))
    return false;
  auto path = "Terrenos/terrain-" + hash + ".glb";
  ModelImportReport report;
  const auto *known = assets_.findByPath(path);
  if (!known)
    for (const auto &entry : importedSources_) {
      const auto *record = assets_.find(entry.guid);
      if (record && record->contentHash == hash) {
        known = record;
        path = record->path;
        break;
      }
    }
  if (known && known->contentHash != hash) {
    error = "O caminho da revisão pertence a outra fonte; terreno preservado.";
    return false;
  }
  refreshTerrainBudget();
  if (!known) {
    std::unordered_set<const renderer::AuthoringTexture *> resident;
    for (const auto &source : importedSources_)
      for (const auto &texture : source.textures)
        if (texture)
          resident.insert(texture.get());
    std::vector<resources::TerrainSurfaceImage> originals;
    for(const auto &source:importedSources_)originals.insert(originals.end(),source.terrainSurfaceImages.begin(),source.terrainSurfaceImages.end());
    const auto candidateBytes =
        terrainCandidateBytes(model, std::move(resident), originals);
    const auto byteLimit = EditorScreenState::TerrainRetainedByteLimit;
    const auto drawLimit = EditorScreenState::TerrainRetainedDrawLimit;
    if (!state_.terrainRetainedBytesKnown ||
        candidateBytes == std::numeric_limits<u64>::max()) {
      error = "Não foi possível contabilizar a biblioteca carregada; nova "
              "revisão recusada. "
              "Terreno, fontes e histórico preservados.";
      return false;
    }
    if (candidateBytes > byteLimit ||
        state_.terrainRetainedBytes > byteLimit - candidateBytes ||
        model.draws.size() > drawLimit ||
        state_.terrainRetainedDraws > drawLimit - model.draws.size()) {
      error =
          "Nova revisão excede o orçamento da biblioteca: 128 MiB CPU "
          "contabilizados ou 4096 desenhos. "
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
  if (known) {
    std::filesystem::path absolute;
    std::vector<u8> current;
    if (!EditorImportTransaction::safePath(
            EditorImportTransaction::fromUtf8(files_.rootPath()), path,
            absolute)) {
      error = "Caminho da revisão inválido.";
      return false;
    }
    if (!EditorImportTransaction::read(absolute, current, 64u << 20) ||
        Sha256::hex(current) != hash) {
      std::filesystem::path temporary;
      if (!EditorImportTransaction::safePath(
              EditorImportTransaction::fromUtf8(files_.rootPath()),
              path + ".aether-tmp", temporary)) {
        error = "Temporário da revisão inválido.";
        return false;
      }
      std::error_code ec;
      std::filesystem::create_directories(absolute.parent_path(), ec);
      if (ec || !EditorImportTransaction::write(absolute, bytes, &error))
        return false;
    }
  }
  // A reused source may follow a transient preview or history restoration.
  // Rebuild the canonical library before deriving chunk slots from its size.
  if(known && !publishAndAdopt(flattenSources(importedSources_),error))return false;
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
  std::shared_ptr<const resources::TerrainBuild> build = terrainPreviewCache_;
  if (!build || build->mesh.parts.empty() ||
      resources::serializeTerrainRecipe(build->recipe) !=
          resources::serializeTerrainRecipe(r)) {
    auto updated = std::make_shared<resources::TerrainBuild>();
    if (!resources::updateTerrain(
            r, build && !build->mesh.parts.empty() ? build.get() : nullptr,
            *updated, error))
      return false;
    build = std::move(updated);
  }
  std::vector<u8> bytes;
  resources::GltfImport model;
  if (!r.surface.layers.empty()) {
    if (!terrainMaterialCache_ && !loadTerrainCatalog()) {
      error = state_.terrainMessage;
      return false;
    }
    auto material =
        prepareTerrainMaterial(r, state_.terrainCatalog, terrainLibraryReader_,
                               terrainMaterialCache_, error);
    if (!material)
      return false;
    terrainMaterialCache_ = std::move(material);
  }
  const std::span<const resources::TerrainSurfaceImage> images =
      r.surface.layers.empty()
          ? std::span<const resources::TerrainSurfaceImage>{}
          : terrainMaterialCache_->images;
  if (!resources::writeTerrainGlb(*build, textures(), images, bytes, error))
    return false;
  if (!resources::importGlb(bytes, {}, {}, model)) {
    error = model.diagnostic;
    return false;
  }
  clearTerrainPreview(false);
  if (!publishTerrain(r, build->mesh, bytes, model, error)) {
    const auto reason=error;std::string restore;
    if(!publishAndAdopt(flattenSources(importedSources_),restore))error=reason+" Restauração pendente: "+restore;
    else error=reason;
    return false;
  }
  terrainPreviewCache_ = build;
  state_.terrainPreviewBuild = std::move(build);
  return true;
}
void EditorSession::startTerrainJob() {
  if (state_.terrainBusy || isPlaying() || history_.isOpen())
    return;
  if (!terrainSourceCurrent(document_, state_.terrainTarget, terrainSource_)) {
    state_.terrainBrushArmed = false;
    state_.terrainMessage =
        "Fonte do terreno mudou · reabra o terreno antes de aplicar";
    return;
  }
  std::string error;
  if (!resources::validTerrainRecipe(state_.terrainDraft, error)) {
    state_.terrainMessage = error;
    return;
  }
  auto job = std::make_shared<TerrainJob>();
  job->recipe = state_.terrainDraft;
  if (terrainPreviewCache_ && !terrainPreviewCache_->mesh.parts.empty())
    job->previous = terrainPreviewCache_;
  job->target = state_.terrainTarget;
  job->epoch = sceneEpoch_;
  job->revision = document_.revision();
  job->project = files_.rootPath();
  if (!job->recipe.surface.layers.empty() && !terrainMaterialCache_ &&
      !loadTerrainCatalog())
    return;
  job->catalog = state_.terrainCatalog;
  job->reader = terrainLibraryReader_;
  job->previousMaterial = terrainMaterialCache_;
  terrainJob_ = job;
  state_.terrainBusy = true;
  state_.terrainProgress = 0;
  state_.terrainMessage = "Gerando malha…";
  std::thread([job] {
    if (!job->recipe.surface.layers.empty()) {
      job->material =
          prepareTerrainMaterial(job->recipe, job->catalog, job->reader,
                                 job->previousMaterial, job->error);
      if (!job->material) {
        job->ready.store(true, std::memory_order_release);
        return;
      }
    }
    const auto progress = [job](float p) {
      job->progress.store(p * .8f);
      return !job->cancel.load();
    };
    // Immutable snapshots remain valid even if the UI cancels or replaces its
    // preview.
    if (job->previous &&
        resources::serializeTerrainRecipe(job->previous->recipe) ==
            resources::serializeTerrainRecipe(job->recipe)) {
      job->build = job->previous;
      progress(1.f);
    } else {
      auto updated = std::make_shared<resources::TerrainBuild>();
      if (resources::updateTerrain(job->recipe, job->previous.get(), *updated,
                                   job->error, progress))
        job->build = std::move(updated);
      else if (job->error.empty())
        job->error = "Não foi possível gerar o terreno";
    }
    if (job->build && !job->cancel &&
        resources::writeTerrainGlb(
            *job->build, textures(),
            job->material ? std::span<const resources::TerrainSurfaceImage>(
                                job->material->images)
                          : std::span<const resources::TerrainSurfaceImage>{},
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
  const auto inspected = state_.inspectorTarget   ? state_.inspectorTarget
                         : state_.inspectorLocked ? state_.inspectorLocked
                                                  : state_.selection;
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
        const auto access = std::find_if(
            terrainAccess_.begin(), terrainAccess_.end(),
            [&](const auto &entry) { return entry.source == source; });
        const bool failed =
            access != terrainAccess_.end() && !access->editable &&
            (record ? access->path == record->path &&
                          access->hash == record->contentHash
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
          if (loaded)
            normalizeTerrainBrush(state_);
          else {
            auto cached = std::find_if(
                terrainAccess_.begin(), terrainAccess_.end(),
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
          state_.terrainMessage = "Fonte do terreno indisponível · rascunho "
                                  "preservado; reabra para tentar novamente";
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
  if (error.empty() && publishTerrain(job->recipe, job->build->mesh, job->bytes,
                                      job->model, error)) {
    terrainMaterialCache_ = job->material;
    terrainPreviewCache_ = job->build;
    state_.terrainPreviewBuild = job->build;
  }
  if (!error.empty()) {
    std::string ignored;
    publishAndAdopt(flattenSources(importedSources_), ignored, nullptr, true);
  }
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
    const auto target = state_.inspectorTarget   ? state_.inspectorTarget
                        : state_.inspectorLocked ? state_.inspectorLocked
                                                 : state_.selection;
    openTerrain(target);
    return true;
  }
  if (!state_.terrainOpen)
    return false;
  TerrainCatalogContextGuard catalogContext(state_);
  if (routing.tapped && (id == widgetId(EditorWidget::NavigationOrbit) ||
                         id == widgetId(EditorWidget::NavigationPan) ||
                         id == widgetId(EditorWidget::NavigationZoom))) {
    state_.terrainBrushArmed = false;
    state_.terrainCursorVisible = false;
    clearTerrainPreview();
    return false;
  }
  if (id == widgetId(EditorWidget::ToolMove) ||
      id == widgetId(EditorWidget::ToolRotate) ||
      id == widgetId(EditorWidget::ToolScale) ||
      id == widgetId(EditorWidget::ToolSelect))
    return true;
  if (!terrainSourceCurrent(document_, state_.terrainTarget, terrainSource_) &&
      ((routing.tapped && (id == widgetId(EditorWidget::TerrainGenerate) ||
                           id == widgetId(EditorWidget::TerrainBrushArm) ||
                           id == widgetId(EditorWidget::TerrainSculpt) ||
                           id == widgetId(EditorWidget::TerrainStamp))) ||
       (routing.target == ui::UiPointerTarget::Viewport &&
        state_.terrainBrushArmed) ||
       terrainGestureActive_)) {
    state_.terrainBrushArmed = false;
    terrainGestureActive_ = false;
    terrainGestureStrokes_.clear();
    state_.terrainMessage =
        "Fonte do terreno mudou · reabra o terreno antes de aplicar";
    return true;
  }
  if (terrainGestureActive_ && event.phase == ui::UiPointerPhase::Down &&
      routing.target != ui::UiPointerTarget::Viewport) {
    terrainGestureActive_ = false;
    terrainGestureStrokes_.clear();
    state_.terrainMessage =
        "Traço cancelado ao usar os controles · cena preservada";
  }
  if ((state_.terrainBrushArmed &&
       routing.target == ui::UiPointerTarget::Viewport) ||
      (terrainGestureActive_ && terrainGesturePointer_ == event.pointerId)) {
    if (state_.terrainBusy)
      return true;
    if (event.phase == ui::UiPointerPhase::Down) {
      if (terrainGestureActive_)
        return true; // One brush owns the gesture; a second finger never
                     // paints.
      terrainGestureActive_ = true;
      terrainGesturePointer_ = event.pointerId;
      terrainGestureTarget_ = state_.terrainTarget;
      terrainGestureRevision_ = document_.revision();
      clearTerrainPreview();
      terrainGestureBrush_ = state_.terrainBrush;
      terrainGestureBrush_.gesture =
          state_.terrainDraft.strokes.empty()
              ? 1
              : state_.terrainDraft.strokes.back().gesture + 1;
      terrainGestureRecipe_ =
          resources::serializeTerrainRecipe(state_.terrainDraft);
      terrainGestureStrokes_.clear();
      terrainGestureOverflow_ = false;
      terrainGestureSampler_.reset(terrainGestureBrush_.radius *
                                   state_.terrainSpacing);
    }
    if (!terrainGestureActive_ || terrainGesturePointer_ != event.pointerId)
      return true;
    if (event.phase == ui::UiPointerPhase::Cancel ||
        terrainGestureTarget_ != state_.terrainTarget ||
        terrainGestureRevision_ != document_.revision() ||
        terrainGestureRecipe_ !=
            resources::serializeTerrainRecipe(state_.terrainDraft)) {
      terrainGestureActive_ = false;
      terrainGestureStrokes_.clear();
      state_.terrainCursorVisible = false;
      clearTerrainPreview();
      state_.terrainMessage = "Traço cancelado · cena e receita preservadas";
      return true;
    }
    const auto append = [&](const resources::TerrainStroke &stroke) {
      if (state_.terrainDraft.strokes.size() + terrainGestureStrokes_.size() >=
          4096) {
        terrainGestureOverflow_ = true;
        terrainGestureStrokes_.clear();
        state_.terrainMessage = "Limite de 4096 amostras · traço descartado; "
                                "use raio maior ou um traço menor";
        return false;
      }
      terrainGestureStrokes_.push_back(stroke);
      return true;
    };
    // The pointer keeps capture outside the viewport, but cannot paint through
    // a panel. Missing the surface suspends brush resampling until the next
    // hit.
    if (!terrainGestureOverflow_ && view_.rect.contains(event.position) &&
        router_.hitTest(event.position).target ==
            ui::UiPointerTarget::Viewport) {
      auto pointer = event.position;
      state_.terrainPointer = pointer;
      if (state_.terrainTouchOffset)
        pointer.y -= 44.f;
      const auto ray = screenPointToRay(view_, pointer);
      buildPickCandidates(nullptr, false, state_.terrainTarget);
      const auto *root = document_.find(state_.terrainTarget);
      const auto *rootLink =
          root ? scene::importLink(root->components) : nullptr;
      std::erase_if(candidates_, [&](const auto &candidate) {
        const auto *object = document_.find(candidate.id);
        const auto *link =
            object ? scene::importLink(object->components) : nullptr;
        return !rootLink || !link || link->root ||
               link->source != rootLink->source;
      });
      // Anchor a gesture to its committed surface. Picking the deforming
      // preview feeds its own displacement back into the next sample, so a
      // stationary finger can grow spikes and stroke strength depends on GPU
      // update timing. The preview remains the source for the visible brush
      // outline below.
      const auto hit = pickNearest(candidates_, ray, 0, true);
      float world[16];
      if (terrainPreviewCount_ &&
          editorWorldMatrix(document_, state_.terrainTarget, world)) {
        candidates_.clear();
        for (u32 part = 0; part < terrainPreviewCount_; ++part) {
          EditorPickCandidate c;
          c.id = state_.terrainTarget;
          if (mapScene_.pickResourceGeometry(terrainPreviewFirst_ + part, world,
                                             c))
            candidates_.push_back(std::move(c));
        }
      }
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
          const bool levelTool =
              terrainGestureBrush_.tool == resources::TerrainTool::Flatten ||
              terrainGestureBrush_.tool == resources::TerrainTool::Path;
          if (!levelTool) {
            local[1] -= terrainGestureBrush_.surfaceDepth;
            if (terrainGestureBrush_.lockHeight &&
                !terrainGestureStrokes_.empty())
              local[1] = terrainGestureStrokes_.front().y;
          }
          state_.terrainCursorVisible = true;
          for (u32 a = 0; a < 3; ++a)
            state_.terrainCursor[a] = world[12 + a] + world[a] * local[0] +
                                      world[4 + a] * local[1] +
                                      world[8 + a] * local[2];
          state_.terrainCursorLines.clear();
          const auto &brush = terrainGestureBrush_;
          const bool volume =
              state_.terrainDraft.mode != resources::TerrainMode::Heightfield &&
              (brush.tool == resources::TerrainTool::Dig ||
               brush.tool == resources::TerrainTool::Add ||
               brush.tool == resources::TerrainTool::Smooth ||
               brush.tool == resources::TerrainTool::Sharpen ||
               brush.tool == resources::TerrainTool::Restore ||
               brush.tool == resources::TerrainTool::Paint);
          for (u32 ring = 0; ring < (volume ? 3u : 2u); ++ring) {
            float previous[3]{};
            for (u32 segment = 0; segment <= 48; ++segment) {
              auto outlineBrush = brush;
              if (!volume) {
                outlineBrush.pitch = outlineBrush.roll = 0;
                if (ring == 1)
                  outlineBrush.radius *= .5f + .5f * brush.hardness;
              }
              const auto offset = resources::terrainBrushOutline(
                  outlineBrush, volume ? ring : 0, segment / 48.f);
              const float q[]{local[0] + offset[0], local[1] + offset[1],
                              local[2] + offset[2]};
              float at[3]{};
              for (u32 a = 0; a < 3; ++a)
                at[a] = world[12 + a] + world[a] * q[0] + world[4 + a] * q[1] +
                        world[8 + a] * q[2];
              if (!volume) {
                EditorRay probe;
                probe.valid = true;
                float len =
                    std::sqrt(world[4] * world[4] + world[5] * world[5] +
                              world[6] * world[6]);
                for (u32 a = 0; a < 3; ++a) {
                  probe.direction[a] = -world[4 + a] / len;
                  probe.origin[a] = at[a] - probe.direction[a] * 4096;
                }
                auto surface = pickNearest(candidates_, probe, 0, true);
                if (surface.hit)
                  for (u32 a = 0; a < 3; ++a)
                    at[a] = probe.origin[a] +
                            probe.direction[a] * (surface.distance - .04f);
              }
              if (segment)
                state_.terrainCursorLines.push_back({previous[0], previous[1],
                                                     previous[2], at[0], at[1],
                                                     at[2]});
              std::copy_n(at, 3, previous);
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
          if (!terrainGestureSampler_.sample(
                  {sample.x, sample.y, sample.z}, volume,
                  event.phase == ui::UiPointerPhase::Up,
                  [&](const auto &point) {
                    auto between = sample;
                    between.x = point[0];
                    between.y = point[1];
                    between.z = point[2];
                    return append(between);
                  })) {
            if (!terrainGestureOverflow_)
              state_.terrainMessage =
                  "Posição inválida · traço descartado; cena preservada";
            terrainGestureOverflow_ = true;
            terrainGestureStrokes_.clear();
            clearTerrainPreview();
          }
          if (!terrainGestureOverflow_)
            queueTerrainPreview();
          if (!terrainGestureOverflow_)
            state_.terrainMessage =
                "Traço · " + std::to_string(terrainGestureStrokes_.size()) +
                " amostras · prévia durante o gesto";
        } else {
          terrainGestureActive_ = false;
          terrainGestureStrokes_.clear();
          state_.terrainMessage =
              "Transformação com shear: ajuste o pai antes de pintar";
        }
      } else {
        terrainGestureSampler_.disconnect();
        state_.terrainCursorVisible = false;
        if (terrainGestureStrokes_.empty())
          state_.terrainMessage = "Toque e arraste na superfície do terreno";
      }
    } else {
      terrainGestureSampler_.disconnect();
    }
    if (event.phase == ui::UiPointerPhase::Up && terrainGestureActive_) {
      terrainGestureActive_ = false;
      if (!terrainGestureOverflow_ && !terrainGestureStrokes_.empty()) {
        state_.terrainDraft.strokes.insert(state_.terrainDraft.strokes.end(),
                                           terrainGestureStrokes_.begin(),
                                           terrainGestureStrokes_.end());
        terrainGestureStrokes_.clear();
        startTerrainJob();
      }
    }
    return true;
  }
  const u32 profileBase = widgetId(EditorWidget::TerrainPathNew) + 0x500;
  if (state_.terrainTab == 4 && state_.terrainSection > 0 &&
      id >= profileBase && id < profileBase + 32 &&
      state_.terrainPath < state_.terrainDraft.paths.size()) {
    auto &path = state_.terrainDraft.paths[state_.terrainPath];
    auto &curve =
        state_.terrainSection == 2 ? path.textureProfile : path.profile;
    const u32 index = id - profileBase;
    if (index >= curve.size() || state_.terrainBusy)
      return true;
    state_.terrainProfilePoint = index;
    if (event.phase == ui::UiPointerPhase::Down) {
      terrainGestureRecipe_ =
          resources::serializeTerrainRecipe(state_.terrainDraft);
      terrainGestureBrush_.x = curve[index].x;
      terrainGestureBrush_.y = curve[index].y;
    } else if (event.phase == ui::UiPointerPhase::Cancel) {
      resources::TerrainRecipe original;
      std::string error;
      if (resources::deserializeTerrainRecipe(terrainGestureRecipe_, original,
                                              error))
        state_.terrainDraft = std::move(original);
    } else if (routing.dragging && event.phase == ui::UiPointerPhase::Move) {
      const auto panel = terrainPanelRect(state_),
                 content = terrainContentRect(state_, panel);
      if (index > 0 && index + 1 < curve.size())
        curve[index].x = std::clamp(
            terrainGestureBrush_.x +
                routing.totalDelta.x / std::max(1.f, content.width - 32),
            curve[index - 1].x + .00001f, curve[index + 1].x - .00001f);
      const bool geometry = state_.terrainSection == 1;
      curve[index].y =
          std::clamp(terrainGestureBrush_.y -
                         routing.totalDelta.y * (geometry ? 3.f : 1.f) /
                             (panel.height < 400 ? 32.f : 56.f),
                     geometry ? -1.f : 0.f, geometry ? 2.f : 1.f);
    }
    state_.terrainBrushArmed = false;
    return true;
  }
  const u32 pointBase = widgetId(EditorWidget::TerrainPathNew) + 0x100;
  if (state_.terrainTab == 4 && id >= pointBase && id < pointBase + 1024 &&
      state_.terrainPath < state_.terrainDraft.paths.size()) {
    auto &path = state_.terrainDraft.paths[state_.terrainPath];
    const u32 index = id - pointBase;
    if (index >= path.points.size() || state_.terrainBusy)
      return true;
    auto &point = path.points[index];
    state_.terrainPathPoint = index;
    if (event.phase == ui::UiPointerPhase::Down) {
      terrainGestureBrush_.x = point.x;
      terrainGestureBrush_.y = point.y;
      terrainGestureBrush_.z = point.z;
      // Hold the same preview-height plane for the entire gesture. Rebuilding
      // the path must not feed a moving plane back into this pointer drag.
      terrainGestureBrush_.surfaceDepth =
          state_.terrainPreviewBuild
              ? resources::terrainFieldHeight(state_.terrainPreviewBuild->field,
                                              point.x, point.z) +
                    .05f
              : point.y;
    }
    if (event.phase == ui::UiPointerPhase::Cancel) {
      point.x = terrainGestureBrush_.x;
      point.y = terrainGestureBrush_.y;
      point.z = terrainGestureBrush_.z;
      return true;
    }
    if (routing.dragging && event.phase == ui::UiPointerPhase::Move) {
      float world[16];
      const auto ray = screenPointToRay(view_, event.position);
      if (ray.valid &&
          editorWorldMatrix(document_, state_.terrainTarget, world)) {
        float at[3], normal[3]{world[4], world[5], world[6]};
        float numerator = 0, denominator = 0;
        for (u32 k = 0; k < 3; ++k) {
          at[k] = world[12 + k] + world[k] * terrainGestureBrush_.x +
                  world[4 + k] * terrainGestureBrush_.surfaceDepth +
                  world[8 + k] * terrainGestureBrush_.z;
          numerator += (at[k] - ray.origin[k]) * normal[k];
          denominator += ray.direction[k] * normal[k];
        }
        if (std::abs(denominator) > 1e-6f && numerator / denominator > 0) {
          float local[3]{};
          for (u32 axis = 0; axis < 3; ++axis) {
            float length = 0;
            for (u32 k = 0; k < 3; ++k) {
              length += world[axis * 4 + k] * world[axis * 4 + k];
              local[axis] +=
                  (ray.origin[k] + ray.direction[k] * numerator / denominator -
                   world[12 + k]) *
                  world[axis * 4 + k];
            }
            if (length > 1e-8f)
              local[axis] /= length;
          }
          if (std::abs(local[0]) <= 4096 && std::abs(local[2]) <= 4096) {
            point.x = local[0];
            point.z = local[2];
          }
        }
      }
    }
    state_.terrainBrushArmed = false;
    state_.terrainMessage = "Ponto " + std::to_string(index + 1) +
                            " selecionado · arraste ou ajuste XYZ";
    return true;
  }
  const bool terrainControl = id >= base && id < base + 0x2000;
  if(id>=terrainSnowSliderBase && id<terrainSnowSliderBase+0x300 && !state_.terrainBusy) {
    const auto encoded=id-terrainSnowSliderBase,field=256+encoded%0x100,column=encoded/0x100;
    const auto range=terrainControlRange(state_,field);if(!range.enabled)return true;
    if(routing.dragging && std::abs(routing.totalDelta.y)>std::abs(routing.totalDelta.x)) {
      if(event.phase==ui::UiPointerPhase::Move)state_.terrainScroll=std::clamp(state_.terrainScroll-routing.stepDelta.y,0.f,terrainScrollLimit(state_,terrainPanelRect(state_)));
    } else if(routing.tapped || (routing.dragging && event.phase==ui::UiPointerPhase::Move)) {
      auto area=terrainContentRect(state_,terrainPanelRect(state_));if(column){area.width*=.5f;if(column==2)area.x+=area.width;}
      setTerrainNumber(field,range.value((event.position.x-area.x-10)/std::max(1.f,area.width-20)));
    }
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainSliderBase) &&
      id < widgetId(EditorWidget::TerrainSliderBase) + 0x300 &&
      !state_.terrainBusy) {
    const auto encoded = id - widgetId(EditorWidget::TerrainSliderBase);
    const auto field = encoded % 0x100, column = encoded / 0x100;
    const auto range = terrainControlRange(state_, field);
    if (!range.enabled)
      return true;
    if (routing.dragging &&
        std::abs(routing.totalDelta.y) > std::abs(routing.totalDelta.x)) {
      if (event.phase == ui::UiPointerPhase::Move)
        state_.terrainScroll =
            std::clamp(state_.terrainScroll - routing.stepDelta.y, 0.f,
                       terrainScrollLimit(state_, terrainPanelRect(state_)));
    } else if (routing.tapped ||
               (routing.dragging && event.phase == ui::UiPointerPhase::Move)) {
      auto area = terrainContentRect(state_, terrainPanelRect(state_));
      if (column) {
        area.width *= .5f;
        if (column == 2)
          area.x += area.width;
      }
      const float t =
          (event.position.x - area.x - 10) / std::max(1.f, area.width - 20);
      setTerrainNumber(field, range.value(t));
    }
    return true;
  }
  const bool historyControl =
      id == widgetId(EditorWidget::Undo) || id == widgetId(EditorWidget::Redo);
  if ((terrainControl || historyControl) && !state_.terrainBusy &&
      !state_.numericField && routing.dragging &&
      event.phase == ui::UiPointerPhase::Move &&
      terrainContentRect(state_, terrainPanelRect(state_))
          .contains(routing.start)) {
    state_.terrainScroll =
        std::clamp(state_.terrainScroll - routing.stepDelta.y, 0.f,
                   terrainScrollLimit(state_, terrainPanelRect(state_)));
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
    state_.terrainCursorVisible = false;
    state_.terrainOpen = false;
    state_.terrainBrushArmed = false;
    return true;
  }
  if (state_.terrainBusy)
    return true;
  if(id>=terrainSnowActionBase && id<terrainSnowActionBase+0x100) {
    const auto action=id-terrainSnowActionBase;
    const auto open=[&](u32 view){state_.terrainMaterialView=view;state_.terrainScroll=0;};
    if(action==0 || action==100) {
      if(!loadTerrainCatalog())return true;initializeTerrainSurface(state_.terrainDraft);state_.terrainDraft.blendedSurface=true;
      open(action==0?200:240);return true;
    }
    auto recipe=state_.terrainDraft;auto &snow=recipe.surface.snow;
    const auto view=state_.terrainMaterialView;
    if(action>=20 && action<=29){open(200+action-20);return true;}
    if(action==30 || action==31){open(action==30?245:246);return true;}
    if(action>=40 && action<=47){open(210+action-40);return true;}
    if(action==48 && terrainSnowMapView(view)){open(view+10);state_.terrainCatalogPage=0;return true;}
    if(action==49 && terrainSnowPicker(view)){open(view-10);return true;}
    if((action==50 || action==51) && terrainSnowMapView(view)){open(210+(terrainSnowMapRole(view)+(action==50?1:7))%8);return true;}
    const auto findSource=[&](const char *text,resources::AssetGuid &guid){
      if(!resources::AssetGuid::parse(text,guid)||!state_.terrainCatalog)return false;
      return std::any_of(state_.terrainCatalog->textures.begin(),state_.terrainCatalog->textures.end(),[&](const auto &t){return t.identity==guid;});
    };
    const auto seed=[&](bool replace){
      resources::AssetGuid diffuse,normal,noise,sparkle;
      if(!findSource("b822881b9ef3441f29c88bb6e0ef60cd",diffuse)||!findSource("cc22f06eaf78d43f782ee617776c1640",normal))return false;
      for(u32 role=0;role<5;++role){auto &map=terrainSnowMap(snow,role);if(replace || !map.texture.valid()){
        map.texture=role<2?diffuse:normal;
        map.channel=role==0?resources::TerrainSurfaceChannel::RGB:role==1?resources::TerrainSurfaceChannel::A:role==2?resources::TerrainSurfaceChannel::NormalGA:role==3?resources::TerrainSurfaceChannel::R:resources::TerrainSurfaceChannel::B;
        map.linear=role!=0;
      }}
      if(findSource("3364d9aca19e4401dbdb0a9bcfcd70bc",noise) && (replace || !snow.normalNoiseMap.texture.valid())){
        snow.normalNoiseMap.texture=noise;snow.normalNoiseMap.channel=resources::TerrainSurfaceChannel::NormalRGB;snow.normalNoiseMap.linear=true;}
      if(findSource("fb84fc5f3e7e14ae7be5bd45efc3d3c4",sparkle) && (replace || !snow.sparkleNoiseMap.texture.valid())){
        snow.sparkleNoiseMap.texture=sparkle;snow.sparkleNoiseMap.channel=resources::TerrainSurfaceChannel::RGB;snow.sparkleNoiseMap.linear=true;}
      return true;
    };
    if(action>=1 && action<=3){
      if(action>1 && (!snow.diffuse.texture.valid() || !snow.height.texture.valid() || !snow.normal.texture.valid() || !snow.smoothness.texture.valid() || !snow.ao.texture.valid()) && !seed(false)){
        state_.terrainMessage="Fontes originais de neve indisponíveis · selecione cinco mapas reais";open(201);return true;}
      snow.mode=resources::TerrainSnowMode(action-1);
    } else if(action==32){if(!seed(true)){state_.terrainMessage="Fontes originais de neve não estão no catálogo";return true;}}
    else if(action>=52 && action<=54 && terrainSnowMapView(view)){
      const auto role=terrainSnowMapRole(view);auto &map=terrainSnowMap(snow,role);
      if(action==52){if(role==2 || role==5)map.channel=terrainNextNormalChannel(map.channel);else map.channel=resources::TerrainSurfaceChannel((u32(map.channel)+1)%((role==1 || role==3 || role==4)?4:5));}
      if(action==53)map.invert=!map.invert;if(action==54 && role!=2 && role!=5)map.linear=!map.linear;
    } else if(action>=60 && action<=71){
      if(action==60){if(!snow.normalNoiseMap.texture.valid()){state_.terrainMessage="Selecione uma fonte de ruído normal";return true;}snow.normalNoiseEnabled=!snow.normalNoiseEnabled;}
      if(action==61)snow.distanceResampleEnabled=!snow.distanceResampleEnabled;
      if(action==62){if(!snow.mask.texture.valid()){state_.terrainMessage="Selecione máscara real: R máximo, G mínimo";return true;}snow.maskEnabled=!snow.maskEnabled;}
      if(action==63)snow.worldSpaceUV=!snow.worldSpaceUV;if(action==64)snow.stochasticEnabled=!snow.stochasticEnabled;
      if(action==65)snow.triplanarEnabled=!snow.triplanarEnabled;
      if(action==66){if(snow.mode!=resources::TerrainSnowMode::Rich){state_.terrainMessage="Participação por camada usa o modo Rich";return true;}snow.perLayerEnabled=!snow.perLayerEnabled;}
      if(action==67)snow.sssEnabled=!snow.sssEnabled;if(action==68)snow.rimEnabled=!snow.rimEnabled;
      if(action==69){if(!snow.sparkleNoiseMap.texture.valid()){state_.terrainMessage="Selecione fonte real do brilho";return true;}snow.sparkleEnabled=!snow.sparkleEnabled;}
      if(action==70)snow.useGlobalLevel=false;if(action==71)snow.useGlobalHeight=false;
    } else if(action==72){
      float minimum=INFINITY,maximum=-INFINITY;
      if(terrainSnowFlatDraft(state_))minimum=maximum=recipe.base;
      else if(terrainSnowMeasuredPreview(state_)){
        float world[16]{};world[0]=world[5]=world[10]=world[15]=1;
        if(state_.terrainTarget && !editorWorldMatrix(document_,state_.terrainTarget,world)){state_.terrainMessage="Transformação mundial indisponível";return true;}
        for(const auto &part:state_.terrainPreviewBuild->mesh.parts)for(const auto &v:part.vertices){
          const float y=world[1]*v.position[0]+world[5]*v.position[1]+world[9]*v.position[2]+world[13];
          if(std::isfinite(y)){minimum=std::min(minimum,y);maximum=std::max(maximum,y);}
        }
      }
      if(!std::isfinite(minimum)||!std::isfinite(maximum)){state_.terrainMessage="Gere uma prévia correspondente para medir alturas reais";return true;}
      snow.heightAngleRange[0]=minimum-1;snow.heightAngleRange[1]=maximum+1;
    } else if(action>=80 && action<=85 && terrainSnowPicker(view)){
      const auto source=terrainCatalogResolve(state_,action-80,false);
      if(!state_.terrainCatalog || source>=state_.terrainCatalog->textures.size()){state_.terrainMessage="Resultado mudou · selecione novamente";return true;}
      const auto role=terrainSnowMapRole(view);auto &map=terrainSnowMap(snow,role);map.texture=state_.terrainCatalog->textures[source].identity;
      if(role==2 || role==5)map.linear=true;
    } else if(action>=101 && action<=115){
      if(action==101)recipe.surface.textureResidency={resources::TerrainTextureResidencyMode::Original,0};
      else if(action==102)recipe.surface.textureResidency=terrainResidencyNextCap(recipe.surface.textureResidency);
      else if(action>=106 && action<=110){constexpr u32 caps[]{4096,2048,1024,512,256};recipe.surface.textureResidency={resources::TerrainTextureResidencyMode::Cap,caps[action-106]};}
      else {auto *map=terrainCurrentAuthoredMap(recipe.surface,state_.terrainBrush.layer,view,state_.terrainMapRole);if(!map)return true;
        if(action>=111){constexpr u32 caps[]{4096,2048,1024,512,256};map->textureResidency={resources::TerrainTextureResidencyMode::Cap,caps[action-111]};}
        else map->textureResidency=action==103?resources::TerrainTextureResidency{resources::TerrainTextureResidencyMode::InheritTerrain,0}:action==104?resources::TerrainTextureResidency{resources::TerrainTextureResidencyMode::Original,0}:terrainResidencyNextCap(map->textureResidency);
      }
    } else return true;
    std::string error;if(resources::validTerrainRecipe(recipe,error)){
      state_.terrainDraft=std::move(recipe);if(action>=80 && action<=85)open(view-10);
      state_.terrainMessage=action==72?"Faixa da prévia medida · margem explícita de 1 m":"Neve e qualidade ajustadas · prévia antes de aplicar";
    } else state_.terrainMessage=error;
    return true;
  }
  if(id>=terrainSnowNumberBase && id<terrainSnowNumberBase+64) {
    const auto field=256+id-terrainSnowNumberBase;
    if(!terrainSnowNumberAvailable(state_,field))return true;
    state_.numericField=id;state_.numericEntity=state_.terrainTarget;state_.numericInstance=0;state_.numericProperty.clear();
    state_.numericCurrent=number(state_,field);std::snprintf(state_.numericText,sizeof(state_.numericText),"%.9g",state_.numericCurrent);
    state_.numericReplace=true;state_.numericError=false;return true;
  }
  if(id>=terrainCatalogActionBase && id<terrainCatalogActionBase+0x100) {
    const auto action=id-terrainCatalogActionBase;
    if(action==4) {
      if(!loadTerrainCatalog())return true;
      state_.terrainMaterialView=190;state_.terrainCatalogPage=0;state_.terrainScroll=0;return true;
    }
    if(action==5) {state_.terrainMaterialView=3;state_.terrainScroll=0;return true;}
    if(!terrainCatalogSearchView(state_.terrainMaterialView))return true;
    if(action==0) {
      state_.editingTerrainCatalogSearch=true;
      std::snprintf(state_.renameText,sizeof(state_.renameText),"%s",state_.terrainCatalogQuery.c_str());return true;
    }
    if(action==1)state_.terrainCatalogQuery.clear();
    else if(action==2)state_.terrainCatalogRoleFilter=(state_.terrainCatalogRoleFilter+1)%8;
    else if(action==3) {
      const auto packages=terrainCatalogPackages(state_);
      const auto found=std::find(packages.begin(),packages.end(),state_.terrainCatalogPackageFilter);
      state_.terrainCatalogPackageFilter=found==packages.end()? (packages.empty()?std::string{}:packages.front()):
        std::next(found)==packages.end()?std::string{}:*std::next(found);
    } else if(action==6 || action==7) {
      const auto count=terrainCatalogCount(state_,false);
      if(action==6 && state_.terrainCatalogPage)--state_.terrainCatalogPage;
      if(action==7 && (state_.terrainCatalogPage+1)*6<count)++state_.terrainCatalogPage;
      state_.terrainScroll=0;return true;
    } else if(action>=8 && action<14 && state_.terrainMaterialView==190) {
      const auto source=terrainCatalogResolve(state_,action-8,false);
      if(!state_.terrainCatalog || source>=state_.terrainCatalog->textures.size()) {
        state_.terrainMessage="Resultado mudou · selecione novamente";return true;
      }
      auto recipe=state_.terrainDraft;
      if(state_.terrainBrush.layer>=recipe.surface.layers.size())return true;
      recipe.surface.layers[state_.terrainBrush.layer].maps[std::min(state_.terrainMapRole,6u)].texture=state_.terrainCatalog->textures[source].identity;
      std::string error;
      if(resources::validTerrainRecipe(recipe,error)) {state_.terrainDraft=std::move(recipe);state_.terrainMaterialView=3;state_.terrainScroll=0;}
      else state_.terrainMessage=error;
      return true;
    } else return true;
    state_.terrainCatalogPage=0;state_.terrainScroll=0;return true;
  }
  if(id>=terrainModuleActionBase && id<terrainModuleActionBase+0x100) {
    const auto action=id-terrainModuleActionBase;
    const auto open=[&](u32 view) {state_.terrainMaterialView=view;state_.terrainScroll=0;};
    if(action==0 || action==13) {
      if(!loadTerrainCatalog()) return true;
      initializeTerrainSurface(state_.terrainDraft);
      state_.terrainDraft.blendedSurface=true;
      open(action==0?120:(terrainPanelRect(state_).height<400?160:159));return true;
    }
    if(action==61) {state_.terrainTab=2;open(0);return true;}
    if(state_.terrainBrush.layer>=state_.terrainDraft.surface.layers.size()) return true;
    if(action==60) {open(121);return true;}
    if(action==62 || action==63) {
      const auto count=std::min<u32>(4,u32(state_.terrainDraft.surface.layers.size()));
      state_.terrainBrush.layer=(state_.terrainBrush.layer+(action==62?count-1:1))%count;
      state_.terrainScroll=0;return true;
    }
    if(action==5 || action==7 || action==10 || action==11 || action==12 || action==49) {
      open(action==5?124:action==7?130:action==10?140:action==11?141:action==12?123:120);
      if(action==7) state_.terrainProfilePoint=0;
      return true;
    }
    if(action==57 || action==58 || action==59) {open(action==57?131:action==58?132:130);return true;}
    if(action>=14 && action<=17) {open(160+action-14);return true;}
    auto recipe=state_.terrainDraft;
    auto &geo=recipe.surface.geo;
    auto &layer=recipe.surface.layers[state_.terrainBrush.layer];
    const auto view=state_.terrainMaterialView;
    if(action==21) {state_.terrainMapRole=0;open(170+terrainModuleGlobal(view));return true;}
    if(action==22) {open(view<159?121:160+terrainModuleGlobal(view));return true;}
    if(action==23 || action==24) {open(action==23?view+10:view-10);state_.terrainCatalogPage=0;return true;}
    if(action==28 || action==29) {state_.terrainMapRole=(state_.terrainMapRole+(action==29?1:3))%4;state_.terrainScroll=0;return true;}
    if(action==30 || action==31) {
      if(action==30 && state_.terrainCatalogPage) --state_.terrainCatalogPage;
      if(action==31 && state_.terrainCatalog && (state_.terrainCatalogPage+1)*6<terrainCatalogCount(state_,false)) ++state_.terrainCatalogPage;
      state_.terrainScroll=0;return true;
    }
    if((action>=40 && action<=48) || action==54) {
      auto &curve=geo.curve;
      auto index=curve.keys.empty()?usize(0):std::min<usize>(state_.terrainProfilePoint,curve.keys.size()-1);
      if(action==40 || action==41) {
        if(!curve.keys.empty()) state_.terrainProfilePoint=u32(action==40?(index?index-1:0):std::min(index+1,curve.keys.size()-1));
        return true;
      }
      if(action==54) {
        if(curve.keys.empty()) return true;
        const auto area=terrainContentRect(state_,terrainPanelRect(state_));
        const auto domain=terrainGeoCurveDomain(curve);
        const float fraction=std::clamp((event.position.x-area.x-10)/std::max(1.f,area.width-20),0.f,1.f);
        const float time=std::lerp(domain[0],domain[1],fraction);
        const auto found=std::lower_bound(curve.keys.begin(),curve.keys.end(),time,[](const auto &key,float t){return key.time<t;});
        auto selected=usize(found-curve.keys.begin());
        if(selected && (selected==curve.keys.size() || time-curve.keys[selected-1].time<curve.keys[selected].time-time)) --selected;
        state_.terrainProfilePoint=u32(selected);return true;
      }
      if(action==42) {
        u64 next=1,result=0;
        for(const auto &old:curve.keys) {
          if(old.id==std::numeric_limits<u64>::max()) {state_.terrainMessage="Limite de identidade de chave alcançado";return true;}
          next=std::max(next,old.id+1);
        }
        bool inserted=false;
        if(curve.keys.size()>=2) {
          const auto left=index+1<curve.keys.size()?index:index-1;
          const auto time=std::midpoint(curve.keys[left].time,curve.keys[left+1].time);
          if(time>curve.keys[left].time && time<curve.keys[left+1].time)
            inserted=resources::splitAnimationCurveDomain(curve,time,next,result,resources::AnimationCurveDomain::Signed);
        }
        if(!inserted) {
          resources::AnimationCurveKey added;
          if(!curve.keys.empty()) {
            added.value=curve.keys.back().value;added.time=curve.keys.back().time+1;
            if(!std::isfinite(added.time) || added.time<=curve.keys.back().time)
              added.time=std::nextafter(curve.keys.back().time,std::numeric_limits<float>::infinity());
            if(!std::isfinite(added.time)) {
              added.value=curve.keys.front().value;
              added.time=std::nextafter(curve.keys.front().time,-std::numeric_limits<float>::infinity());
            }
          }
          inserted=resources::putAnimationCurveKeyDomain(curve,added,next,result,resources::AnimationCurveDomain::Signed);
        }
        if(!inserted) {state_.terrainMessage="Não foi possível inserir uma chave neste domínio";return true;}
        for(usize i=0;i<curve.keys.size();++i) if(curve.keys[i].id==result) {index=i;break;}
      } else if(action==43) {
        if(curve.keys.empty()) return true;
        const u64 removed=curve.keys[index].id;
        if(!resources::eraseAnimationCurveKeysDomain(curve,std::span<const u64>(&removed,1),resources::AnimationCurveDomain::Signed)) {
          state_.terrainMessage="Não foi possível remover a chave selecionada";return true;
        }
        index=curve.keys.empty()?0:std::min(index,curve.keys.size()-1);
      } else if(curve.keys.empty()) return true;
      else if(action==44) {
        auto &key=curve.keys[index];
        key.broken=!key.broken;
        if(!key.broken) {key.outgoing=key.incoming;key.outSlope=key.inSlope;}
      } else if(action==45 || action==46) {
        auto &key=curve.keys[index];
        auto &mode=action==45?key.incoming:key.outgoing;
        for(u32 attempt=0;attempt<7;++attempt) {
          mode=resources::AnimationTangentMode((unsigned(mode)+1)%7);
          if(!key.broken) {key.incoming=key.outgoing=mode;key.outSlope=key.inSlope;}
          if(resources::validAnimationCurveDomain(curve,resources::AnimationCurveDomain::Signed)) break;
        }
      } else if(action==47) curve.keys[index].weightedIn=!curve.keys[index].weightedIn;
      else if(action==48) curve.keys[index].weightedOut=!curve.keys[index].weightedOut;
      state_.terrainProfilePoint=u32(index);
    } else if(action>=1 && action<=3) geo.mode=resources::TerrainGeoMode(action-1);
    else if(action==4) geo.blend=geo.blend==resources::TerrainColorBlend::Multiply2x?resources::TerrainColorBlend::LighterColor:resources::TerrainColorBlend::Multiply2x;
    else if(action==6) geo.rangeEnabled=!geo.rangeEnabled;
    else if(action==8) geo.curveEnabled=!geo.curveEnabled;
    else if(action==9) {
      resources::AssetGuid albedo,normal;
      resources::AssetGuid::parse("863f19b183b2443859f959a96f7dce32",albedo);
      resources::AssetGuid::parse("192aaea3237044f608d663c9884e0c70",normal);
      bool a=false,n=false;
      if(state_.terrainCatalog) for(const auto &source:state_.terrainCatalog->textures) {a=a||source.identity==albedo;n=n||source.identity==normal;}
      if(!a || !n) {state_.terrainMessage="Par Geo original indisponível no catálogo";return true;}
      geo.albedo=resources::neutralTerrainSurfaceMaps()[0];geo.normal=resources::neutralTerrainSurfaceMaps()[1];
      geo.albedo.texture=albedo;geo.normal.texture=normal;geo.mode=resources::TerrainGeoMode::AlbedoNormal;
    } else if(action==18 || action==19 || action==20) {
      auto &global=recipe.surface.globalMaps[terrainModuleGlobal(view)];
      if(action==18) global.enabled=!global.enabled;
      if(action==19) global.blend=resources::TerrainColorBlend((u32(global.blend)+1)%4);
      if(action==20) global.normalBlend=resources::TerrainNormalBlend((u32(global.normalBlend)+1)%2);
    } else if(action==25 || action==26 || action==27 || (action>=32 && action<=37)) {
      auto &map=terrainModuleMap(recipe.surface,view,state_.terrainMapRole);
      if(action==25) map.channel=terrainModuleNormal(view)?
        terrainNextNormalChannel(map.channel):
        resources::TerrainSurfaceChannel((u32(map.channel)+1)%((view==172 || view==182)?4:5));
      if(action==26) map.invert=!map.invert;
      if(action==27 && !terrainModuleNormal(view)) map.linear=!map.linear;
      if(action>=32 && action<=37) {
        const auto source=terrainCatalogResolve(state_,action-32,false);
        if(!state_.terrainCatalog || source>=state_.terrainCatalog->textures.size()) {state_.terrainMessage="Resultado mudou · selecione novamente";return true;}
        map.texture=state_.terrainCatalog->textures[source].identity;
        if(terrainModuleNormal(view)) {map.linear=true;if(u32(map.channel)<5) map.channel=resources::TerrainSurfaceChannel::NormalRGB;}
      }
    } else if(action==55) layer.geoHeightFilterEnabled=!layer.geoHeightFilterEnabled;
    else if(action==56) {
      auto &g=recipe.surface.globalMaps[2];
      if(!g.smoothness.texture.valid()) {state_.terrainMessage="Selecione primeiro a fonte de smoothness";return true;}
      g.ao.texture=g.metallic.texture=g.mask.texture=g.smoothness.texture;
    } else return true;
    std::string error;
    if(resources::validTerrainRecipe(recipe,error)) {
      state_.terrainDraft=std::move(recipe);
      if(action>=32 && action<=37) open(view-10);
      state_.terrainMessage="Estratos e mapas ajustados · prévia antes de aplicar";
    } else state_.terrainMessage=error;
    return true;
  }
  if(id>=terrainClusterActionBase && id<terrainClusterActionBase+0x80) {
    const auto action=id-terrainClusterActionBase;
    if(state_.terrainBrush.layer>=state_.terrainDraft.surface.layers.size()) return true;
    if(action==0) {
      if(!loadTerrainCatalog()) return true;
      state_.terrainMaterialView=60;state_.terrainScroll=0;return true;
    }
    auto recipe=state_.terrainDraft;
    auto &cluster=recipe.surface.layers[state_.terrainBrush.layer].cluster;
    const auto index=terrainClusterVariant(state_.terrainMaterialView);
    if(action==4) {state_.terrainMaterialView=62;state_.terrainScroll=0;return true;}
    if(action==5) {
      if(cluster.variants.size()>=3) return true;
      state_.terrainMaterialView=80+u32(cluster.variants.size());state_.terrainCatalogPage=0;state_.terrainScroll=0;return true;
    }
    if(action>=40 && action<=42) {
      if(action-40>=cluster.variants.size()) return true;
      state_.terrainMaterialView=70+action-40;state_.terrainScroll=0;return true;
    }
    if(action==7 || action==8 || action==9 || action==10) {
      if(index>=cluster.variants.size()) return true;
      state_.terrainMaterialView=(action==7?100:action==8?80:action==9?90:70)+index;
      state_.terrainCatalogPage=0;state_.terrainScroll=0;return true;
    }
    if(action==16 || action==17) {
      const bool material=state_.terrainMaterialView>=80 && state_.terrainMaterialView<=82;
      const auto size=terrainCatalogCount(state_,material);
      if(action==16 && state_.terrainCatalogPage) --state_.terrainCatalogPage;
      if(action==17 && (state_.terrainCatalogPage+1)*6<size) ++state_.terrainCatalogPage;
      state_.terrainScroll=0;return true;
    }
    u32 nextView=state_.terrainMaterialView;
    if(action==1) {
      if(cluster.variants.size()<2) {state_.terrainMessage="Clusters exigem duas variantes com albedos distintos";return true;}
      cluster.enabled=!cluster.enabled;
    } else if(action==2) cluster.stochastic=!cluster.stochastic;
    else if(action==3) cluster.quality=resources::TerrainSurfaceQuality((u32(cluster.quality)+1)%3);
    else if(action==6) {
      if(index>=cluster.variants.size()) return true;
      cluster.variants.erase(cluster.variants.begin()+index);
      if(cluster.variants.size()<2) cluster.enabled=false;
      nextView=60;
    } else if(action>=20 && action<=25 && state_.terrainCatalog) {
      const auto source=terrainCatalogResolve(state_,action-20,true);
      if(source>=state_.terrainCatalog->materials.size()) {state_.terrainMessage="Resultado mudou · selecione novamente";return true;}
      if(index>cluster.variants.size() || index>=3)return true;
      const auto &material=state_.terrainCatalog->materials[source];
      for(u32 i=0;i<cluster.variants.size();++i)
        if(i!=index && (cluster.variants[i].identity==material.layer.identity || cluster.variants[i].maps[0].texture==material.layer.maps[0].texture)) {
          state_.terrainMessage="Variante duplicada: escolha outro material/albedo";return true;
        }
      resources::TerrainSurfaceVariant variant;
      variant.identity=material.layer.identity;variant.maps=material.layer.maps;
      variant.scale=material.layer.scaleX;variant.offsetX=material.layer.offsetX;variant.offsetY=material.layer.offsetY;variant.rotation=material.layer.rotation;
      if(index==cluster.variants.size()) cluster.variants.push_back(std::move(variant));else cluster.variants[index]=std::move(variant);
      nextView=70+index;
    } else if(action>=32 && action<=37 && state_.terrainCatalog) {
      const auto source=terrainCatalogResolve(state_,action-32,false);
      if(source>=state_.terrainCatalog->textures.size()) {state_.terrainMessage="Resultado mudou · selecione novamente";return true;}
      if(index>=cluster.variants.size())return true;
      auto &map=cluster.variants[index].maps[std::min(state_.terrainMapRole,6u)];
      map.texture=state_.terrainCatalog->textures[source].identity;
      nextView=100+index;
    } else if(index<cluster.variants.size()) {
      auto &map=cluster.variants[index].maps[std::min(state_.terrainMapRole,6u)];
      if(action==11 || action==12) {state_.terrainMapRole=(state_.terrainMapRole+(action==12?1:6))%7;state_.terrainScroll=0;return true;}
      if(action==13) map.channel=state_.terrainMapRole==1?
        terrainNextNormalChannel(map.channel):resources::TerrainSurfaceChannel((u32(map.channel)+1)%5);
      else if(action==14) map.invert=!map.invert;
      else if(action==15 && state_.terrainMapRole!=1) map.linear=!map.linear;
      else return true;
    } else return true;
    std::string error;
    if(resources::validTerrainRecipe(recipe,error)) {
      state_.terrainDraft=std::move(recipe);state_.terrainMaterialView=nextView;
      if(action==6 || (action>=20 && action<=37)) state_.terrainScroll=0;
      state_.terrainMessage="Variantes ajustadas · prévia antes de aplicar";
    } else state_.terrainMessage=error;
    return true;
  }
  if(id>=terrainNoiseActionBase && id<terrainNoiseActionBase+0x40) {
    const u32 action=id-terrainNoiseActionBase;
    if(action<=4) {
      if(!loadTerrainCatalog()) return true;
      initializeTerrainSurface(state_.terrainDraft);
      state_.terrainDraft.blendedSurface=true;
      state_.terrainMaterialView=action==0 ? 4 : 3+action;
      state_.terrainScroll=0; return true;
    }
    const auto view=state_.terrainMaterialView;
    if(state_.terrainBrush.layer>=state_.terrainDraft.surface.layers.size()) return true;
    if(action==44 || action==45) {
      const auto selection=terrainNoiseSelection(view);
      const bool local=action==45;
      const auto next=selection<4?(selection%2)+(local?2:0):selection<10?4+(selection-4)%3+(local?3:0):(local?2:0);
      state_.terrainMaterialView=terrainNoiseViewFor(next);state_.terrainScroll=0;return true;
    }
    if(action>=35 && action<=43) {
      if(action==35 || action==43) {state_.terrainMaterialView=50;state_.terrainScroll=0;return true;}
      if(action==42) {state_.terrainMaterialView=51;state_.terrainScroll=0;return true;}
      auto recipe=state_.terrainDraft;
      auto &layer=recipe.surface.layers[state_.terrainBrush.layer];
      if(action==36) layer.resampleAlbedo=!layer.resampleAlbedo;
      if(action==37) layer.resampleNormal=!layer.resampleNormal;
      if(action==38) layer.resampleHeightBlend=!layer.resampleHeightBlend;
      if(action>=39 && action<=41) layer.resampleBlend=resources::TerrainDistanceBlend(action-39);
      if(layer.resampleBlend==resources::TerrainDistanceBlend::Noise) {
        if(!layer.resampleNoise.map.texture.valid()) {
          layer.resampleNoise.map=layer.maps[0];
          if(!layer.resampleNoise.map.texture.valid() && state_.terrainCatalog)
            for(const auto &material:state_.terrainCatalog->materials)
              if(material.layer.maps[0].texture.valid()) {layer.resampleNoise.map=material.layer.maps[0];break;}
          layer.resampleNoise.map.channel=resources::TerrainSurfaceChannel::R;
        }
        if(!layer.resampleNoise.map.texture.valid()) {state_.terrainMessage="Selecione uma textura antes do modo Noise";return true;}
        layer.resampleNoise.enabled=true;
      }
      std::string error;
      if(resources::validTerrainRecipe(recipe,error)) {state_.terrainDraft=std::move(recipe);state_.terrainMessage="Reamostragem ajustada · prévia antes de aplicar";}
      else state_.terrainMessage=error;
      return true;
    }
    if(action>=22 && action<=25) {
      const auto selection=terrainNoiseSelection(view);
      const auto role=action==22?(selection-4)%3:action-23;
      const bool local=action==22?!terrainNoiseLocal(view):terrainNoiseLocal(view);
      state_.terrainMaterialView=terrainNoiseViewFor((local?7:4)+role);
      state_.terrainScroll=0;return true;
    }
    if(action>=26 && action<=34) {
      if(action==26) {state_.terrainMaterialView=30;state_.terrainScroll=0;return true;}
      auto recipe=state_.terrainDraft;
      auto &array=recipe.surface.antiTileArray;
      auto &layer=recipe.surface.layers[state_.terrainBrush.layer];
      if(action==27) array.enabled=!array.enabled;
      else if(action==28) {
        if(array.entries.size()>=32) return true;
        resources::TerrainAntiTileEntry entry;
        u32 nonce=0;
        do {
          entry.identity=resources::assetGuidFromSeed("astra.terrain.antitile:"+layer.identity.text()+":"+std::to_string(nonce++));
        } while(std::any_of(array.entries.begin(),array.entries.end(),[&](const auto &old){return old.identity==entry.identity;}));
        entry.normal.map=layer.maps[1];
        if(!entry.normal.map.texture.valid() && state_.terrainCatalog)
          for(const auto &material:state_.terrainCatalog->materials)
            if(material.layer.maps[1].texture.valid()) {entry.normal.map=material.layer.maps[1];break;}
        if(!entry.normal.map.texture.valid()) {state_.terrainMessage="Selecione um material com normal antes de criar a entrada";return true;}
        if(u32(entry.normal.map.channel)<5)
          entry.normal.map.channel=resources::TerrainSurfaceChannel::NormalRGB;
        entry.normal.map.linear=true;entry.normal.enabled=true;
        array.entries.push_back(entry);layer.antiTileEntry=entry.identity;
      } else if(action==29) {
        if(array.entries.empty()) return true;
        auto found=std::find_if(array.entries.begin(),array.entries.end(),[&](const auto &entry){return entry.identity==layer.antiTileEntry;});
        layer.antiTileEntry=(found==array.entries.end() || ++found==array.entries.end())?array.entries.front().identity:found->identity;
      } else if(action==30) layer.antiTileEntry={};
      else if(action<=33) {
        if(!terrainBoundAntiTile(recipe.surface,state_.terrainBrush.layer)) return true;
        state_.terrainMaterialView=31+action-31;state_.terrainScroll=0;return true;
      } else return true;
      std::string error;
      if(resources::validTerrainRecipe(recipe,error)) {state_.terrainDraft=std::move(recipe);state_.terrainMessage="Anti-tile ajustado · prévia antes de aplicar";}
      else state_.terrainMessage=error;
      return true;
    }
    if(!terrainNoiseAvailable(state_.terrainDraft.surface,state_.terrainBrush.layer,view)) return true;
    if(action==10 || action==11) {
      state_.terrainMaterialView=terrainNoiseViewFor(terrainNoiseSelection(view),action==10);
      state_.terrainCatalogPage=0; state_.terrainScroll=0; return true;
    }
    if(action==12 || action==13) {
      if(action==12 && state_.terrainCatalogPage) --state_.terrainCatalogPage;
      if(action==13 && state_.terrainCatalog && (state_.terrainCatalogPage+1)*6<terrainCatalogCount(state_,false)) ++state_.terrainCatalogPage;
      state_.terrainScroll=0; return true;
    }
    auto recipe=state_.terrainDraft;
    auto &noise=terrainSelectedNoise(recipe.surface,state_.terrainBrush.layer,view);
    if(action==5) noise.enabled=!noise.enabled;
    else if(action==6) noise.projection=resources::TerrainProjection((u32(noise.projection)+1)%3);
    else if(action==7) noise.map.channel=terrainNoiseNormal(view)?
      terrainNextNormalChannel(noise.map.channel):
      resources::TerrainSurfaceChannel((u32(noise.map.channel)+1)%5);
    else if(action==8) noise.map.linear=!noise.map.linear;
    else if(action==9) noise.map.invert=!noise.map.invert;
    else if(action>=16 && action<22 && state_.terrainCatalog) {
      const auto index=terrainCatalogResolve(state_,action-16,false);
      if(index>=state_.terrainCatalog->textures.size()) {state_.terrainMessage="Resultado mudou · selecione novamente";return true;}
      noise.map.texture=state_.terrainCatalog->textures[index].identity;
      if(terrainNoiseNormal(view)) {
        if(u32(noise.map.channel)<5) noise.map.channel=resources::TerrainSurfaceChannel::NormalRGB;
        noise.map.linear=true;
      }
      state_.terrainMaterialView=terrainNoiseViewFor(terrainNoiseSelection(view)); state_.terrainScroll=0;
    } else return true;
    std::string error;
    if(resources::validTerrainRecipe(recipe,error)) {
      state_.terrainDraft=std::move(recipe);
      state_.terrainMessage="Ruído ajustado · prévia antes de aplicar";
    } else state_.terrainMessage=error;
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainCatalogItemBase) &&
      id < widgetId(EditorWidget::TerrainCatalogItemBase) + 6) {
    const auto source=terrainCatalogResolve(state_,id-widgetId(EditorWidget::TerrainCatalogItemBase),true);
    if(state_.terrainCatalog && source<state_.terrainCatalog->materials.size())selectTerrainCatalogMaterial(source);
    else state_.terrainMessage="Resultado mudou · selecione novamente";
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainMaterialLibrary) &&
      id <= widgetId(EditorWidget::TerrainMaterialPaint)) {
    if (id == widgetId(EditorWidget::TerrainMaterialLibrary) &&
        !loadTerrainCatalog())
      return true;
    if(id==widgetId(EditorWidget::TerrainMaterialLibrary) && state_.terrainMaterialView!=1)
      state_.terrainCatalogPage=0;
    if(id!=widgetId(EditorWidget::TerrainMaterialPaint)) {
      initializeTerrainSurface(state_.terrainDraft);state_.terrainDraft.blendedSurface=true;
    }
    state_.terrainMaterialView =
        id == widgetId(EditorWidget::TerrainMaterialLibrary)      ? 1
        : id == widgetId(EditorWidget::TerrainMaterialProperties) ? 2
        : id == widgetId(EditorWidget::TerrainMaterialMaps)       ? 3
                                                                  : 0;
    state_.terrainScroll = 0;
    return true;
  }
  if (id == widgetId(EditorWidget::TerrainCatalogPrevious) ||
      id == widgetId(EditorWidget::TerrainCatalogNext)) {
    if (id == widgetId(EditorWidget::TerrainCatalogPrevious)) {
      if (state_.terrainCatalogPage)
        --state_.terrainCatalogPage;
    } else if (state_.terrainCatalog &&
               (state_.terrainCatalogPage + 1) * 6 < terrainCatalogCount(state_,true))
      ++state_.terrainCatalogPage;
    state_.terrainScroll = 0;
    return true;
  }
  if (id == widgetId(EditorWidget::TerrainMapPrevious) ||
      id == widgetId(EditorWidget::TerrainMapNext)) {
    state_.terrainMapRole =
        (state_.terrainMapRole +
         (id == widgetId(EditorWidget::TerrainMapNext) ? 1 : 6)) %
        7;
    state_.terrainScroll = 0;
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainLayerActive) &&
      id <= widgetId(EditorWidget::TerrainMapColorSpace)) {
    auto recipe = state_.terrainDraft;
    if (state_.terrainBrush.layer >= recipe.surface.layers.size())
      return true;
    auto &layer = recipe.surface.layers[state_.terrainBrush.layer];
    auto &map = layer.maps[std::min(state_.terrainMapRole, 6u)];
    switch (EditorWidget(id)) {
    case EditorWidget::TerrainLayerActive:
      layer.active = !layer.active;
      break;
    case EditorWidget::TerrainLayerProjection:
      layer.projection =
          resources::TerrainProjection((u32(layer.projection) + 1) % 3);
      break;
    case EditorWidget::TerrainLayerFaceNormals:
      layer.faceNormals = !layer.faceNormals;
      break;
    case EditorWidget::TerrainLayerHeightBlend:
      layer.heightBlend = !layer.heightBlend;
      break;
    case EditorWidget::TerrainLayerHeightQuality:
      layer.heightQuality =
          resources::TerrainSurfaceQuality((u32(layer.heightQuality) + 1) % 3);
      break;
    case EditorWidget::TerrainMapChannel:
      map.channel =
          state_.terrainMapRole == 1
              ? terrainNextNormalChannel(map.channel)
              : resources::TerrainSurfaceChannel((u32(map.channel) + 1) % 5);
      break;
    case EditorWidget::TerrainMapInvert:
      map.invert = !map.invert;
      break;
    case EditorWidget::TerrainMapColorSpace:
      map.linear = !map.linear;
      break;
    default:
      return true;
    }
    std::string error;
    if (resources::validTerrainRecipe(recipe, error)) {
      state_.terrainDraft = std::move(recipe);
      state_.terrainMessage = "Material ajustado · prévia antes de aplicar";
    } else
      state_.terrainMessage = error;
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainNumberBase) &&
      (id < widgetId(EditorWidget::TerrainNumberBase) + 52 ||
       id == widgetId(EditorWidget::TerrainNumberBase) + 60 ||
       (id >= widgetId(EditorWidget::TerrainNumberBase) + 64 &&
        id <= widgetId(EditorWidget::TerrainNumberBase) + 76) ||
       (id >= widgetId(EditorWidget::TerrainNumberBase) + 96 &&
        id <= widgetId(EditorWidget::TerrainNumberBase) + 110) ||
       (id >= widgetId(EditorWidget::TerrainNumberBase) + 128 && id <= widgetId(EditorWidget::TerrainNumberBase) + 154) ||
       terrainClusterField(id-widgetId(EditorWidget::TerrainNumberBase)) || terrainModuleField(id-widgetId(EditorWidget::TerrainNumberBase)))) {
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
  const auto minus = widgetId(EditorWidget::TerrainMinusBase),
             plus = widgetId(EditorWidget::TerrainPlusBase);
  if ((id >= minus && id < minus + 111) || (id >= plus && id < plus + 111)) {
    const bool increase = id >= plus;
    const u32 field = id - (increase ? plus : minus);
    const double steps[]{1,   8,   8,  1,  1,  .25, 1,  8,   .5,  .5,
                         .25, 1,   1,  1,  1,  .1,  15, .25, .05, .1,
                         .25, .25, 15, 15, .1, .1,  1,  .25};
    setTerrainNumber(field, number(state_, field) +
                                (field < 28    ? steps[field]
                                 : field == 35 ? 5
                                 : field == 29 || field == 36 || field == 38 ||
                                         field == 39 || field >= 47
                                     ? .1
                                     : 1) *
                                    (increase ? 1 : -1));
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainSectionBase) &&
      id < widgetId(EditorWidget::TerrainSectionBase) + 3) {
    state_.terrainSection = id - widgetId(EditorWidget::TerrainSectionBase);
    state_.terrainScroll = 0;
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainModeBase) &&
      id < widgetId(EditorWidget::TerrainModeBase) + 3) {
    if (!state_.terrainDraft.strokes.empty()) {
      state_.terrainMessage =
          "Crie outro terreno para trocar o tipo após esculpir";
      return true;
    }
    auto recipe = state_.terrainDraft;
    recipe.mode =
        resources::TerrainMode(id - widgetId(EditorWidget::TerrainModeBase));
    std::string error;
    if (resources::validTerrainRecipe(recipe, error)) {
      state_.terrainDraft = std::move(recipe);
      state_.terrainBrush.tool = resources::TerrainTool::Raise;
      normalizeTerrainBrush(state_);
      state_.terrainMessage = "Tipo alterado · aplique para atualizar";
    } else
      state_.terrainMessage = error;
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainPresetBase) &&
      id < widgetId(EditorWidget::TerrainPresetBase) + 4) {
    const auto index = id - widgetId(EditorWidget::TerrainPresetBase);
    auto &r = state_.terrainDraft;
    const float amplitude[]{0, 8, 24, 16}, frequency[]{2, 2, 4, 3};
    r.height = amplitude[index];
    r.frequency = frequency[index];
    r.base = 4;
    r.octaves = 4;
    r.island = index == 3;
    state_.terrainMessage = "Relevo alterado · traços preservados · aplique";
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainTabBase) &&
      id < widgetId(EditorWidget::TerrainTabBase) + 5) {
    state_.terrainTab = id - widgetId(EditorWidget::TerrainTabBase);
    state_.terrainPage = 0;
    state_.terrainSection = 0;
    state_.terrainScroll = 0;
    if (state_.terrainTab == 4) {
      state_.terrainBrushArmed = false;
      state_.terrainCursorVisible = false;
    }
    if (state_.terrainTab == 2)
      state_.terrainBrush.tool = resources::TerrainTool::Paint;
    normalizeTerrainBrush(state_);
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainToolBase) &&
      id < widgetId(EditorWidget::TerrainToolBase) + 13) {
    state_.terrainBrush.tool =
        resources::TerrainTool(id - widgetId(EditorWidget::TerrainToolBase));
    normalizeTerrainBrush(state_);
    state_.terrainBrushArmed = state_.terrainTarget != 0;
    state_.terrainCursorVisible = false;
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainLayerBase) &&
      id < widgetId(EditorWidget::TerrainLayerBase) + 4) {
    state_.terrainBrush.layer = id - widgetId(EditorWidget::TerrainLayerBase);
    state_.terrainBrush.tool = resources::TerrainTool::Paint;
    state_.terrainBrushArmed = state_.terrainTarget != 0;
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainShapeBase) &&
      id < widgetId(EditorWidget::TerrainShapeBase) + 5) {
    state_.terrainBrush.shape = resources::TerrainBrushShape(
        id - widgetId(EditorWidget::TerrainShapeBase));
    normalizeTerrainBrush(state_);
    return true;
  }
  if (id >= widgetId(EditorWidget::TerrainPathNew) &&
      id <= widgetId(EditorWidget::TerrainPathEnabled)) {
    auto &paths = state_.terrainDraft.paths;
    auto selected =
        std::min(state_.terrainPath, u32(paths.empty() ? 0 : paths.size() - 1));
    u32 nextId = 1;
    for (const auto &path : paths)
      nextId = std::max(nextId, path.id + 1);
    if (id == widgetId(EditorWidget::TerrainPathNew)) {
      if (paths.size() >= 64) {
        state_.terrainMessage = "Limite de 64 caminhos";
        return true;
      }
      resources::TerrainPath path;
      path.id = nextId;
      path.name = "Caminho " + std::to_string(nextId);
      const auto &r = state_.terrainDraft;
      path.points = {{-r.size * .25f, r.base, 0, false},
                     {r.size * .25f, r.base, 0, false}};
      paths.push_back(std::move(path));
      state_.terrainPath = u32(paths.size() - 1);
      state_.terrainPathPoint = 0;
    } else if (!paths.empty()) {
      auto &path = paths[selected];
      auto &point = state_.terrainPathPoint;
      switch (EditorWidget(id)) {
      case EditorWidget::TerrainPathDelete:
        paths.erase(paths.begin() + selected);
        state_.terrainPath = selected ? selected - 1 : 0;
        point = 0;
        break;
      case EditorWidget::TerrainPathDuplicate:
        if (paths.size() < 64) {
          auto copy = path;
          copy.id = nextId;
          copy.name += " cópia";
          paths.push_back(std::move(copy));
          state_.terrainPath = u32(paths.size() - 1);
        }
        break;
      case EditorWidget::TerrainPathPrevious:
        state_.terrainPath = selected ? selected - 1 : u32(paths.size() - 1);
        point = 0;
        break;
      case EditorWidget::TerrainPathNext:
        state_.terrainPath = (selected + 1) % u32(paths.size());
        point = 0;
        break;
      case EditorWidget::TerrainPointPrevious:
        if (point)
          --point;
        break;
      case EditorWidget::TerrainPointNext:
        if (point + 1 < path.points.size())
          ++point;
        break;
      case EditorWidget::TerrainPointInsert:
        if (path.points.size() < 1024 && point < path.points.size()) {
          auto p = path.points[point];
          if (point + 1 < path.points.size()) {
            const auto &q = path.points[point + 1];
            p.x = (p.x + q.x) * .5f;
            p.y = (p.y + q.y) * .5f;
            p.z = (p.z + q.z) * .5f;
          } else
            p.x += 2;
          path.points.insert(path.points.begin() + point + 1, p);
          ++point;
        }
        break;
      case EditorWidget::TerrainPointRemove:
        if (path.points.size() > 2 && point < path.points.size()) {
          path.points.erase(path.points.begin() + point);
          point = std::min(point, u32(path.points.size() - 1));
        }
        break;
      case EditorWidget::TerrainPointCurve:
        if (point < path.points.size())
          path.points[point].curved = !path.points[point].curved;
        break;
      case EditorWidget::TerrainPathShape:
        path.shape = !path.shape;
        break;
      case EditorWidget::TerrainPathTexture:
        path.texture = !path.texture;
        break;
      case EditorWidget::TerrainPathRatio:
        path.keepWidthRatio = !path.keepWidthRatio;
        break;
      case EditorWidget::TerrainPathAutoRamp:
        path.autoRamp = !path.autoRamp;
        break;
      case EditorWidget::TerrainPathFollowTerrain:
        path.followTerrain = !path.followTerrain;
        break;
      case EditorWidget::TerrainPathDefinedHeight:
        path.useDefinedHeight = !path.useDefinedHeight;
        break;
      case EditorWidget::TerrainPathReverse:
        path.reverseRamp = !path.reverseRamp;
        break;
      case EditorWidget::TerrainPathSmart:
        path.smartPaint = !path.smartPaint;
        break;
      case EditorWidget::TerrainPathCap:
        path.cap = resources::TerrainPathCap((u32(path.cap) + 1) % 3);
        break;
      case EditorWidget::TerrainPathEnabled:
        path.enabled = !path.enabled;
        break;
      case EditorWidget::TerrainProfilePrevious:
      case EditorWidget::TerrainProfileNext:
      case EditorWidget::TerrainProfileInsert:
      case EditorWidget::TerrainProfileRemove: {
        auto &curve =
            state_.terrainSection == 2 ? path.textureProfile : path.profile;
        auto &index = state_.terrainProfilePoint;
        index = std::min(index, u32(curve.size() - 1));
        if (id == widgetId(EditorWidget::TerrainProfilePrevious))
          index = index ? index - 1 : u32(curve.size() - 1);
        else if (id == widgetId(EditorWidget::TerrainProfileNext))
          index = (index + 1) % u32(curve.size());
        else if (id == widgetId(EditorWidget::TerrainProfileInsert) &&
                 curve.size() < 32) {
          const auto left = std::min(index, u32(curve.size() - 2));
          const auto a = curve[left], b = curve[left + 1];
          if (b.x - a.x > 1e-5f) {
            curve.insert(curve.begin() + left + 1,
                         {(a.x + b.x) * .5f, (a.y + b.y) * .5f});
            index = left + 1;
          }
        } else if (id == widgetId(EditorWidget::TerrainProfileRemove) &&
                   index > 0 && index + 1 < curve.size()) {
          curve.erase(curve.begin() + index);
          index = std::min(index, u32(curve.size() - 1));
        }
      } break;
      default:
        break;
      }
    }
    state_.terrainBrushArmed = false;
    state_.terrainMessage = "Caminho editável · confira a prévia e aplique";
    return true;
  }
  switch (EditorWidget(id)) {
  case EditorWidget::TerrainNavigate:
    state_.terrainBrushArmed = false;
    state_.terrainCursorVisible = false;
    clearTerrainPreview();
    break;
  case EditorWidget::TerrainSculpt:
    state_.terrainBrushArmed = state_.terrainTarget != 0;
    break;
  case EditorWidget::TerrainDimensionsLinked:
    state_.terrainBrush.dimensionsLinked =
        !state_.terrainBrush.dimensionsLinked;
    break;
  case EditorWidget::TerrainBrushInvert:
    state_.terrainBrush.inverted = !state_.terrainBrush.inverted;
    break;
  case EditorWidget::TerrainHeightLock:
    state_.terrainBrush.lockHeight = !state_.terrainBrush.lockHeight;
    break;
  case EditorWidget::TerrainOpacityTarget:
    state_.terrainBrush.application =
        state_.terrainBrush.application ==
                resources::TerrainBrushApplication::Target
            ? resources::TerrainBrushApplication::Accumulate
            : resources::TerrainBrushApplication::Target;
    break;
  case EditorWidget::TerrainRestoreGeometry:
    state_.terrainBrush.restoreGeometry = !state_.terrainBrush.restoreGeometry;
    break;
  case EditorWidget::TerrainRestoreWeights:
    state_.terrainBrush.restoreWeights = !state_.terrainBrush.restoreWeights;
    break;
  case EditorWidget::TerrainTouchOffset:
    state_.terrainTouchOffset = !state_.terrainTouchOffset;
    break;
  case EditorWidget::TerrainBlend:
    state_.terrainDraft.blendedSurface = !state_.terrainDraft.blendedSurface;
    break;
  case EditorWidget::TerrainGenerate:
    startTerrainJob();
    break;
  case EditorWidget::TerrainNew:
    openTerrain();
    break;
  case EditorWidget::TerrainLoad:
    if (state_.selection)
      openTerrain(state_.selection);
    else
      state_.terrainMessage = "Selecione um terreno ou chunk para retomar";
    break;
  case EditorWidget::TerrainSurface:
  case EditorWidget::TerrainSolidBase:
    state_.terrainDraft.solidBase =
        EditorWidget(id) == EditorWidget::TerrainSolidBase;
    state_.terrainMessage = "Estrutura alterada · aplique para atualizar";
    break;
  case EditorWidget::TerrainRevert:
    if (state_.terrainTarget)
      openTerrain(state_.terrainTarget);
    break;
  case EditorWidget::TerrainSave:
    if (state_.terrainTarget &&
        resources::serializeTerrainRecipe(state_.terrainDraft) ==
            state_.terrainAppliedRecipe) {
      state_.saveRequested = true;
      state_.terrainMessage = "Salvamento da cena solicitado";
    } else
      state_.terrainMessage =
          "Aplique ou reverta as alterações antes de salvar";
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
    if (state_.terrainTab == 2)
      state_.terrainBrush.tool = resources::TerrainTool::Paint;
    state_.terrainBrushArmed =
        state_.terrainTarget && !state_.terrainBrushArmed;
    break;
  case EditorWidget::TerrainStamp:
    if (!state_.terrainTarget) {
      state_.terrainMessage =
          "Crie ou abra um terreno antes de aplicar o pincel";
    } else if (state_.terrainDraft.strokes.size() < 4096) {
      state_.terrainBrush.gesture =
          state_.terrainDraft.strokes.empty()
              ? 1
              : state_.terrainDraft.strokes.back().gesture + 1;
      state_.terrainDraft.strokes.push_back(state_.terrainBrush);
      startTerrainJob();
    } else
      state_.terrainMessage =
          "Limite de 4096 amostras · remova uma amostra antes de aplicar";
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

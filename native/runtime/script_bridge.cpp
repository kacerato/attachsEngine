#include "runtime/script_bridge.h"
#include "runtime/transform_math.h"
#include "scene/script_behavior.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace ae::runtime {
namespace {

void jsonString(std::ostream &out, std::string_view text) {
  out << '"';
  constexpr char hex[] = "0123456789abcdef";
  for (unsigned char c : text) {
    if (c == '"' || c == '\\') out << '\\' << static_cast<char>(c);
    else if (c < 32) out << "\\u00" << hex[c >> 4] << hex[c & 15];
    else out << static_cast<char>(c);
  }
  out << '"';
}

bool transformFromAbi(const float *v, Transform &transform, const float parent[16]) {
  for (u32 i = 0; i < 10; ++i) if (!std::isfinite(v[i])) return false;
  const float norm = std::sqrt(v[3] * v[3] + v[4] * v[4] + v[5] * v[5] + v[6] * v[6]);
  if (norm < 1e-8f) return false;
  const float x = v[3] / norm, y = v[4] / norm, z = v[5] / norm, w = v[6] / norm;
  float world[16]{(1 - 2 * (y * y + z * z)) * v[7], 2 * (x * y + w * z) * v[7], 2 * (x * z - w * y) * v[7], 0,
    2 * (x * y - w * z) * v[8], (1 - 2 * (x * x + z * z)) * v[8], 2 * (y * z + w * x) * v[8], 0,
    2 * (x * z + w * y) * v[9], 2 * (y * z - w * x) * v[9], (1 - 2 * (x * x + y * y)) * v[9], 0, v[0], v[1], v[2], 1};
  return localTransformForWorld(world, parent, transform);
}

void transformToAbi(const Transform &t, float *out) {
  std::copy(t.position, t.position + 3, out);
  std::copy(t.scale, t.scale + 3, out + 7);
  transformRotationQuaternion(t, out + 3);
}

std::string_view viewOf(const u8 *text, int length) {
  if (!text || length <= 0 || length > 1024) return {};
  return {reinterpret_cast<const char *>(text), static_cast<usize>(length)};
}

scene::ScriptRenderingSettings toAbi(const renderer::ProjectRenderingSettings &s) {
  scene::ScriptRenderingSettings o;
  o.schemaVersion=s.schemaVersion;o.preset=(u32)s.preset;o.shadows=(u32)s.shadows;o.ambient=(u32)s.ambient;o.post=(u32)s.post;o.textures=(u32)s.textures;o.waterMesh=(u32)s.waterMesh;
  o.resolutionScale=s.resolutionScale;o.maximumRenderHz=s.maximumRenderHz;o.shadowCascadeCount=s.shadowCascadeCount;o.shadowCascadeResolution=s.shadowCascadeResolution;o.shadowFilterTaps=s.shadowFilterTaps;o.shadowFarFilterTaps=s.shadowFarFilterTaps;
  o.shadowMaximumDistance=s.shadowMaximumDistance;o.shadowDepthBiasConstant=s.shadowDepthBiasConstant;o.shadowDepthBiasSlope=s.shadowDepthBiasSlope;o.shadowNormalOffsetTexels=s.shadowNormalOffsetTexels;o.staticShadowCache=(u32)s.staticShadowCache;o.shadowCacheGuardBandRatio=s.shadowCacheGuardBandRatio;o.shadowCascadeBlendRatio=s.shadowCascadeBlendRatio;o.shadowDistanceFadeRatio=s.shadowDistanceFadeRatio;
  o.lodPixelErrorBudget=s.lodPixelErrorBudget;o.coverageLodPixelErrorBudget=s.coverageLodPixelErrorBudget;o.lodHysteresisBandRatio=s.lodHysteresisBandRatio;o.lodSelection=(u32)s.lodSelection;o.materialShaderVariants=(u32)s.materialShaderVariants;o.environmentSplitSumBrdf=(u32)s.environmentSplitSumBrdf;
  o.normalMapMaximumDistance=s.normalMapMaximumDistance;o.specularProbeMaximumDistance=s.specularProbeMaximumDistance;o.metallicRoughnessMaximumDistance=s.metallicRoughnessMaximumDistance;o.emissiveMaximumDistance=s.emissiveMaximumDistance;o.materialDetailFadeBandRatio=s.materialDetailFadeBandRatio;o.thermalDistanceScaling=(u32)s.thermalDistanceScaling;
  o.antiAliasing=(u32)s.antiAliasing;o.upscalingFilter=(u32)s.upscalingFilter;o.postFxaa=(u32)s.postFxaa;o.postVignette=(u32)s.postVignette;o.bloomThreshold=s.bloomThreshold;o.bloomIntensity=s.bloomIntensity;o.postContrast=s.postContrast;o.postSaturation=s.postSaturation;o.postSharpen=s.postSharpen;o.temporalHistoryWeight=s.temporalHistoryWeight;
  o.dynamicResolution=(u32)s.dynamicResolution;o.dynamicResolutionMinimumScale=s.dynamicResolutionMinimumScale;o.dynamicResolutionDecreaseStep=s.dynamicResolutionDecreaseStep;o.dynamicResolutionIncreaseStep=s.dynamicResolutionIncreaseStep;o.dynamicResolutionRecoveryHeadroomRatio=s.dynamicResolutionRecoveryHeadroomRatio;o.dynamicResolutionOverloadFrames=s.dynamicResolutionOverloadFrames;o.dynamicResolutionRecoveryFrames=s.dynamicResolutionRecoveryFrames;
  return o;
}

renderer::ProjectRenderingSettings fromAbi(const scene::ScriptRenderingSettings &s) {
  renderer::ProjectRenderingSettings o;
  o.schemaVersion=s.schemaVersion;o.preset=(renderer::QualityPreset)s.preset;o.shadows=(renderer::ShadowQuality)s.shadows;o.ambient=(renderer::AmbientQuality)s.ambient;o.post=(renderer::PostQuality)s.post;o.textures=(renderer::TextureQuality)s.textures;o.waterMesh=(renderer::WaterMeshQuality)s.waterMesh;
  o.resolutionScale=s.resolutionScale;o.maximumRenderHz=s.maximumRenderHz;o.shadowCascadeCount=s.shadowCascadeCount;o.shadowCascadeResolution=s.shadowCascadeResolution;o.shadowFilterTaps=s.shadowFilterTaps;o.shadowFarFilterTaps=s.shadowFarFilterTaps;
  o.shadowMaximumDistance=s.shadowMaximumDistance;o.shadowDepthBiasConstant=s.shadowDepthBiasConstant;o.shadowDepthBiasSlope=s.shadowDepthBiasSlope;o.shadowNormalOffsetTexels=s.shadowNormalOffsetTexels;o.staticShadowCache=(renderer::FeatureOverride)s.staticShadowCache;o.shadowCacheGuardBandRatio=s.shadowCacheGuardBandRatio;o.shadowCascadeBlendRatio=s.shadowCascadeBlendRatio;o.shadowDistanceFadeRatio=s.shadowDistanceFadeRatio;
  o.lodPixelErrorBudget=s.lodPixelErrorBudget;o.coverageLodPixelErrorBudget=s.coverageLodPixelErrorBudget;o.lodHysteresisBandRatio=s.lodHysteresisBandRatio;o.lodSelection=(renderer::FeatureOverride)s.lodSelection;o.materialShaderVariants=(renderer::FeatureOverride)s.materialShaderVariants;o.environmentSplitSumBrdf=(renderer::FeatureOverride)s.environmentSplitSumBrdf;
  o.normalMapMaximumDistance=s.normalMapMaximumDistance;o.specularProbeMaximumDistance=s.specularProbeMaximumDistance;o.metallicRoughnessMaximumDistance=s.metallicRoughnessMaximumDistance;o.emissiveMaximumDistance=s.emissiveMaximumDistance;o.materialDetailFadeBandRatio=s.materialDetailFadeBandRatio;o.thermalDistanceScaling=(renderer::FeatureOverride)s.thermalDistanceScaling;
  o.antiAliasing=(renderer::AntiAliasingMode)s.antiAliasing;o.upscalingFilter=(renderer::UpscalingFilter)s.upscalingFilter;o.postFxaa=(renderer::FeatureOverride)s.postFxaa;o.postVignette=(renderer::FeatureOverride)s.postVignette;o.bloomThreshold=s.bloomThreshold;o.bloomIntensity=s.bloomIntensity;o.postContrast=s.postContrast;o.postSaturation=s.postSaturation;o.postSharpen=s.postSharpen;o.temporalHistoryWeight=s.temporalHistoryWeight;
  o.dynamicResolution=(renderer::FeatureOverride)s.dynamicResolution;o.dynamicResolutionMinimumScale=s.dynamicResolutionMinimumScale;o.dynamicResolutionDecreaseStep=s.dynamicResolutionDecreaseStep;o.dynamicResolutionIncreaseStep=s.dynamicResolutionIncreaseStep;o.dynamicResolutionRecoveryHeadroomRatio=s.dynamicResolutionRecoveryHeadroomRatio;o.dynamicResolutionOverloadFrames=s.dynamicResolutionOverloadFrames;o.dynamicResolutionRecoveryFrames=s.dynamicResolutionRecoveryFrames;
  return o;
}

scene::ScriptResolvedRenderingPolicy toAbi(const renderer::ResolvedRenderingPolicy &p) {
  scene::ScriptResolvedRenderingPolicy o;
  o.renderHz=p.frame.renderHz;o.simulationHz=p.frame.simulationHz;o.frameIntervalMs=p.frame.frameIntervalMs;o.cpuLaneBudgetMs=p.frame.cpuLaneBudgetMs;o.gpuLaneBudgetMs=p.frame.gpuLaneBudgetMs;o.compositorReserveMs=p.frame.compositorReserveMs;
  o.hzbMinimumCandidateDraws=p.visibility.hzbMinimumCandidateDraws;o.hzbHysteresisFrames=p.visibility.hzbHysteresisFrames;o.hzbNormalizedDepthBias=p.visibility.hzbNormalizedDepthBias;o.lodPixelErrorBudget=p.visibility.lodPixelErrorBudget;o.coverageLodPixelErrorBudget=p.visibility.coverageLodPixelErrorBudget;o.lodHysteresisBandRatio=p.visibility.lodHysteresisBandRatio;
  o.shadowsEnabled=p.shadows.enabled;o.shadowCascadeCount=p.shadows.cascadeCount;o.shadowCascadeResolution=p.shadows.cascadeResolution;o.shadowFilterTaps=p.shadows.filterTaps;o.shadowFarFilterTaps=p.shadows.farFilterTaps;o.shadowMaximumDistance=p.shadows.maximumDistance;o.shadowDepthBiasConstant=p.shadows.depthBiasConstant;o.shadowDepthBiasSlope=p.shadows.depthBiasSlope;o.shadowNormalOffsetTexels=p.shadows.normalOffsetTexels;o.shadowStabilizeTexelSnap=p.shadows.stabilizeTexelSnap;o.staticShadowCache=p.shadows.staticCasterCache;o.shadowCacheGuardBandRatio=p.shadows.cacheGuardBandRatio;o.shadowCascadeBlendRatio=p.shadows.cascadeBlendRatio;o.shadowDistanceFadeRatio=p.shadows.distanceFadeRatio;
  o.ambientHemispheric=p.ambient.hemispheric;o.ambientSpecularProbe=p.ambient.specularProbe;o.ambientSplitSumBrdf=p.ambient.splitSumBrdf;o.postDedicatedPass=p.post.dedicatedPass;o.postBloom=p.post.bloom;o.antiAliasing=(u32)p.post.antiAliasing;o.upscalingFilter=(u32)p.post.upscalingFilter;o.postVignette=p.post.vignette;o.bloomThreshold=p.post.bloomThreshold;o.bloomIntensity=p.post.bloomIntensity;o.postContrast=p.post.contrast;o.postSaturation=p.post.saturation;o.postSharpen=p.post.sharpen;o.vignetteIntensity=p.post.vignetteIntensity;o.temporalHistoryWeight=p.post.temporalHistoryWeight;
  o.lodSelection=p.geometry.lodSelection;o.materialShaderVariants=p.geometry.materialShaderVariants;o.waterMesh=(u32)p.geometry.waterMesh;o.textureResidencyMipBias=p.textures.residencyMipBias;o.samplerAnisotropy=p.textures.samplerAnisotropy;o.normalMapMaximumDistance=p.materialDistance.normalMapMaximumDistance;o.specularProbeMaximumDistance=p.materialDistance.specularProbeMaximumDistance;o.metallicRoughnessMaximumDistance=p.materialDistance.metallicRoughnessMaximumDistance;o.emissiveMaximumDistance=p.materialDistance.emissiveMaximumDistance;o.materialDetailFadeBandRatio=p.materialDistance.fadeBandRatio;
  o.dynamicResolutionEnabled=p.dynamicResolution.enabled;o.dynamicResolutionMinimumScale=p.dynamicResolution.minimumScale;o.dynamicResolutionMaximumScale=p.dynamicResolution.maximumScale;o.dynamicResolutionDecreaseStep=p.dynamicResolution.decreaseStep;o.dynamicResolutionIncreaseStep=p.dynamicResolution.increaseStep;o.dynamicResolutionRecoveryHeadroomRatio=p.dynamicResolution.recoveryHeadroomRatio;o.dynamicResolutionOverloadFrames=p.dynamicResolution.overloadFrames;o.dynamicResolutionRecoveryFrames=p.dynamicResolution.recoveryFrames;o.resolutionScale=p.resolutionScale;o.effectiveProfile=(u32)p.effectiveProfile;o.clampCount=p.clampCount;
  return o;
}

} // namespace

bool ScriptBridge::hasScripts(const SceneGraph &graph) {
  std::vector<ObjectId> ids;
  graph.collectSubtree(graph.root(), ids);
  for (auto id : ids) for (usize i = 0; i < graph.find(id)->components.size(); ++i)
    if (scene::scriptBehavior(graph.find(id)->components.at(i))) return true;
  return false;
}

std::string ScriptBridge::attachments(const SceneGraph &graph) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(std::numeric_limits<float>::max_digits10) << '[';
  std::vector<ObjectId> ids;
  graph.collectSubtree(graph.root(), ids);
  bool first = true;
  for (auto id : ids) {
    const auto &object = *graph.find(id);
    if (!graph.activeInHierarchy(id)) continue;
    for (usize i = 0; i < object.components.size(); ++i) if (const auto *script = scene::scriptBehavior(object.components.at(i))) {
      if (!first) out << ',';
      first = false;
      out << "{\"ObjectId\":" << id << ",\"InstanceId\":" << script->instanceId() << ",\"TypeId\":";
      jsonString(out, script->scriptType);
      out << ",\"Enabled\":" << (script->enabled ? "true" : "false") << ",\"Properties\":{";
      bool fieldFirst = true;
      for (const auto &p : script->properties) {
        if (!fieldFirst) out << ',';
        fieldFirst = false;
        jsonString(out, p.id);
        out << ':';
        if (p.valueType == "string") jsonString(out, p.value);
        else if (p.valueType == "asset") { out << "{\"AssetId\":"; jsonString(out, p.value); out << '}'; }
        else if (p.valueType == "object") { std::istringstream in(p.value); u64 v = 0; in >> v; out << "{\"ObjectId\":" << v << '}'; }
        else if (p.valueType == "vector3") {
          std::istringstream in(p.value);
          in.imbue(std::locale::classic());
          float x = 0, y = 0, z = 0;
          in >> x >> y >> z;
          out << "{\"X\":" << x << ",\"Y\":" << y << ",\"Z\":" << z << '}';
        } else if (p.valueType == "float") { std::istringstream in(p.value); in.imbue(std::locale::classic()); float v = 0; in >> v; out << v; }
        else if (p.valueType == "int32" || p.valueType == "enum") { std::istringstream in(p.value); i32 v = 0; in >> v; out << v; }
        else out << p.value;
      }
      out << "},\"PropertyTypes\":{";
      fieldFirst = true;
      for (const auto &p : script->properties) {
        if (!fieldFirst) out << ',';
        fieldFirst = false;
        jsonString(out, p.id);
        out << ':';
        jsonString(out, p.valueType);
      }
      out << "}}";
    }
  }
  out << ']';
  return out.str();
}

// O adaptador inteiro vive aqui: cada lambda recebe o contexto, traduz o id em
// handle do mundo e guarda o motivo da recusa em `lastStatus_`, para que o lado
// C# possa dizer "referência vencida" em vez de "false".
void ScriptBridge::installAccess() {
  access_ = scene::ScriptSceneAccess{};
  access_.context = this;
  access_.exists = [](void *c, u64 id) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    return id <= std::numeric_limits<ObjectId>::max() && s.world_->graph().exists(static_cast<ObjectId>(id));
  };
  access_.worldId = [](void *c) -> u32 { return static_cast<ScriptBridge *>(c)->world_->worldId(); };
  access_.lastStatus = [](void *c) -> u32 { return static_cast<u32>(static_cast<ScriptBridge *>(c)->lastStatus_); };
  access_.generation = [](void *c, u64 id) -> u32 {
    auto &s = *static_cast<ScriptBridge *>(c);
    if (id > std::numeric_limits<ObjectId>::max()) return 0;
    return s.world_->handle(static_cast<ObjectId>(id)).generation;
  };
  access_.getTransform = [](void *c, u64 id, float *out) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    if (!out) return 0;
    Transform local;
    s.lastStatus_ = s.world_->localTransform(s.world_->handle(static_cast<ObjectId>(id)), local);
    if (s.lastStatus_ != WorldStatus::Ok) return 0;
    transformToAbi(local, out);
    return 1;
  };
  access_.setTransform = [](void *c, u64 id, const float *value) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    if (!value) return 0;
    const auto handle = s.world_->handle(static_cast<ObjectId>(id));
    s.lastStatus_ = s.world_->validate(handle);
    if (s.lastStatus_ != WorldStatus::Ok) return 0;
    // `getTransform`/`setTransform` falam espaço LOCAL — a decomposição contra a
    // identidade apenas normaliza o quatérnio e recusa shear. Espaço de mundo
    // tem par próprio (`getWorldTransform`/`setWorldTransform`).
    float identity[16]{};
    identity[0] = identity[5] = identity[10] = identity[15] = 1;
    Transform local;
    if (!transformFromAbi(value, local, identity)) { s.lastStatus_ = WorldStatus::InvalidArgument; return 0; }
    s.lastStatus_ = s.world_->setLocalTransform(handle, local);
    return s.lastStatus_ == WorldStatus::Ok;
  };
  access_.getWorldTransform = [](void *c, u64 id, float *out) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    if (!out) return 0;
    Transform world;
    s.lastStatus_ = s.world_->worldTransform(s.world_->handle(static_cast<ObjectId>(id)), world);
    if (s.lastStatus_ != WorldStatus::Ok) return 0;
    transformToAbi(world, out);
    return 1;
  };
  access_.setWorldTransform = [](void *c, u64 id, const float *value) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    if (!value) return 0;
    float identity[16]{};
    identity[0] = identity[5] = identity[10] = identity[15] = 1;
    Transform world;
    if (!transformFromAbi(value, world, identity)) { s.lastStatus_ = WorldStatus::InvalidArgument; return 0; }
    s.lastStatus_ = s.world_->setWorldTransform(s.world_->handle(static_cast<ObjectId>(id)), world);
    return s.lastStatus_ == WorldStatus::Ok;
  };
  access_.setVelocity = [](void *c, u64 id, const float *v) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    return v && s.access_.exists(c, id) && s.physics_->setBodyVelocity(static_cast<ObjectId>(id), v);
  };
  access_.moveKinematic = [](void *c, u64 id, const float *v) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    return v && s.access_.exists(c, id) && s.physics_->moveKinematic(static_cast<ObjectId>(id), v);
  };
  access_.bodyForce = [](void *c, u64 id, const float *v, u32 kind) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    return v && s.access_.exists(c, id) && s.physics_->applyBodyForce(static_cast<ObjectId>(id), v, kind);
  };
  access_.getVelocity = [](void *c, u64 id, float *v) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    return v && s.access_.exists(c, id) && s.physics_->getBodyVelocity(static_cast<ObjectId>(id), v);
  };
  access_.log = [](void *c, u64 id, const u8 *text, int length) {
    auto &s = *static_cast<ScriptBridge *>(c);
    if (text && length > 0 && length <= 32768) {
      const std::string_view message(reinterpret_cast<const char *>(text), static_cast<usize>(length));
      if (s.diagnostics_.size() > 65536) s.diagnostics_.clear();
      s.diagnostics_ += std::to_string(id) + ": " + std::string(message) + "\n";
      if (s.logSink_) s.logSink_(id, message);
    }
  };

  // --- hierarquia ---------------------------------------------------------
  access_.parentOf = [](void *c, u64 id) -> u64 {
    auto &s = *static_cast<ScriptBridge *>(c);
    return s.world_->parentOf(s.world_->handle(static_cast<ObjectId>(id))).id;
  };
  access_.childCount = [](void *c, u64 id) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    const auto handle = s.world_->handle(static_cast<ObjectId>(id));
    s.lastStatus_ = s.world_->validate(handle);
    return s.lastStatus_ == WorldStatus::Ok ? static_cast<int>(s.world_->childCount(handle)) : -1;
  };
  access_.childAt = [](void *c, u64 id, u32 index) -> u64 {
    auto &s = *static_cast<ScriptBridge *>(c);
    return s.world_->childAt(s.world_->handle(static_cast<ObjectId>(id)), index).id;
  };
  access_.findChild = [](void *c, u64 id, const u8 *name, int length, int recursive) -> u64 {
    auto &s = *static_cast<ScriptBridge *>(c);
    return s.world_->findChildByName(s.world_->handle(static_cast<ObjectId>(id)), viewOf(name, length), recursive != 0).id;
  };
  access_.getName = [](void *c, u64 id, u8 *out, int capacity) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    const auto name = s.world_->nameOf(s.world_->handle(static_cast<ObjectId>(id)));
    if (name.empty()) return -1;
    const int size = static_cast<int>(name.size());
    if (!out || capacity < size) return size;
    std::memcpy(out, name.data(), name.size());
    return size;
  };
  access_.setName = [](void *c, u64 id, const u8 *name, int length) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    s.lastStatus_ = s.world_->setName(s.world_->handle(static_cast<ObjectId>(id)), viewOf(name, length));
    return s.lastStatus_ == WorldStatus::Ok;
  };
  access_.getActive = [](void *c, u64 id) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    const auto handle = s.world_->handle(static_cast<ObjectId>(id));
    s.lastStatus_ = s.world_->validate(handle);
    if (s.lastStatus_ != WorldStatus::Ok) return -1;
    return s.world_->activeInHierarchy(handle) ? 1 : 0;
  };
  access_.setActive = [](void *c, u64 id, int active) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    s.lastStatus_ = s.world_->setActive(s.world_->handle(static_cast<ObjectId>(id)), active != 0);
    return s.lastStatus_ == WorldStatus::Ok;
  };

  // --- ciclo de vida ------------------------------------------------------
  access_.createObject = [](void *c, u64 parent, const u8 *name, int length) -> u64 {
    auto &s = *static_cast<ScriptBridge *>(c);
    const auto created = s.world_->createObject(s.world_->handle(static_cast<ObjectId>(parent)), viewOf(name, length), s.lastStatus_);
    return created.id;
  };
  access_.destroyObject = [](void *c, u64 id) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    s.lastStatus_ = s.world_->destroyObject(s.world_->handle(static_cast<ObjectId>(id)));
    return s.lastStatus_ == WorldStatus::Ok;
  };
  access_.setParent = [](void *c, u64 id, u64 parent, u32 index) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    s.lastStatus_ = s.world_->setParent(s.world_->handle(static_cast<ObjectId>(id)),
                                        s.world_->handle(static_cast<ObjectId>(parent)), index);
    return s.lastStatus_ == WorldStatus::Ok;
  };

  // --- componentes --------------------------------------------------------
  access_.componentCount = [](void *c, u64 id) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    const auto handle = s.world_->handle(static_cast<ObjectId>(id));
    s.lastStatus_ = s.world_->validate(handle);
    return s.lastStatus_ == WorldStatus::Ok ? static_cast<int>(s.world_->componentCount(handle)) : -1;
  };
  access_.componentAt = [](void *c, u64 id, u32 index, u8 *typeId, int capacity) -> u64 {
    auto &s = *static_cast<ScriptBridge *>(c);
    const auto component = s.world_->componentAt(s.world_->handle(static_cast<ObjectId>(id)), index);
    if (!component.valid()) return 0;
    const auto name = s.world_->componentTypeId(component);
    // Terminado em zero: o lado gerenciado lê até o terminador em vez de
    // precisar de uma segunda chamada só para descobrir o tamanho.
    if (typeId) {
      if (capacity <= static_cast<int>(name.size())) return 0;
      std::memcpy(typeId, name.data(), name.size());
      typeId[name.size()] = 0;
    }
    return component.instance;
  };
  access_.findComponent = [](void *c, u64 id, const u8 *typeId, int length, u32 ordinal) -> u64 {
    auto &s = *static_cast<ScriptBridge *>(c);
    return s.world_->findComponent(s.world_->handle(static_cast<ObjectId>(id)), viewOf(typeId, length), ordinal).instance;
  };
  access_.addComponent = [](void *c, u64 id, const u8 *typeId, int length) -> u64 {
    auto &s = *static_cast<ScriptBridge *>(c);
    return s.world_->addComponent(s.world_->handle(static_cast<ObjectId>(id)), viewOf(typeId, length), s.lastStatus_).instance;
  };
  access_.removeComponent = [](void *c, u64 id, u64 instance) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    s.lastStatus_ = s.world_->removeComponent({s.world_->handle(static_cast<ObjectId>(id)), instance});
    return s.lastStatus_ == WorldStatus::Ok;
  };
  // --- consultas fisicas --------------------------------------------------
  access_.rayCast = [](void *c, const float *origin, const float *direction,
                       const scene::ScriptQueryFilter *filter, scene::ScriptQueryHit *out, int capacity) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    if (!origin || !direction || !filter || filter->size != sizeof(*filter) || capacity < 0 || capacity > 4096) return -1;
    std::vector<QueryHit> hits(static_cast<usize>(capacity));
    const u32 total = s.physics_->rayCastAll(origin, direction, s.queryFilter(*filter),
                                             capacity ? hits.data() : nullptr, static_cast<u32>(capacity));
    s.copyHits(hits, total, out, capacity);
    return static_cast<int>(total);
  };
  access_.shapeCast = [](void *c, const scene::ScriptShapeQuery *shape, const float *origin,
                         const float *direction, const scene::ScriptQueryFilter *filter,
                         scene::ScriptQueryHit *out) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    if (!shape || !origin || !direction || !filter || filter->size != sizeof(*filter) || !out) return -1;
    QueryHit hit;
    if (!s.physics_->shapeCast(ScriptBridge::queryShape(*shape), origin, direction, s.queryFilter(*filter), hit)) return 0;
    s.copyHits({hit}, 1, out, 1);
    return 1;
  };
  access_.overlap = [](void *c, const scene::ScriptShapeQuery *shape, const float *origin,
                       const scene::ScriptQueryFilter *filter, scene::ScriptQueryHit *out, int capacity) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    if (!shape || !origin || !filter || filter->size != sizeof(*filter) || capacity < 0 || capacity > 4096) return -1;
    std::vector<QueryHit> hits(static_cast<usize>(capacity));
    const u32 total = s.physics_->overlap(ScriptBridge::queryShape(*shape), origin, s.queryFilter(*filter),
                                          capacity ? hits.data() : nullptr, static_cast<u32>(capacity));
    s.copyHits(hits, total, out, capacity);
    return static_cast<int>(total);
  };
  access_.layerByName = [](void *c, const u8 *name, int length) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    const auto text = viewOf(name, length);
    if (text.empty()) return -1;
    const auto &layers = s.world_->graph().layers();
    for (u32 layer = 0; layer < GameplayLayers::kCount; ++layer)
      if (layers.name(layer) == text) return static_cast<int>(layer);
    return -1;
  };
  access_.layerName = [](void *c, u32 layer, u8 *out, int capacity) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    const auto name = s.world_->graph().layers().name(layer);
    if (name.empty()) return -1;
    const int size = static_cast<int>(name.size());
    if (!out || capacity < size) return size;
    std::memcpy(out, name.data(), name.size());
    return size;
  };

  // --- entrada por acoes ---------------------------------------------------
  access_.inputAxis = [](void *c, const u8 *action, int length, float *out) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    const auto name = viewOf(action, length);
    if (name.empty() || !out) return 0;
    if (!s.input_->map().find(name)) { s.lastStatus_ = WorldStatus::InvalidArgument; return 0; }
    s.input_->axis2(name, out);
    return 1;
  };
  access_.inputButton = [](void *c, const u8 *action, int length, u32 query) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    const auto name = viewOf(action, length);
    if (name.empty() || !s.input_->map().find(name)) { s.lastStatus_ = WorldStatus::InvalidArgument; return -1; }
    switch (query) {
      case 0: return s.input_->pressed(name) ? 1 : 0;
      case 1: return s.input_->justPressed(name) ? 1 : 0;
      case 2: return s.input_->justReleased(name) ? 1 : 0;
      default: return -1;
    }
  };
  access_.inputContext = [](void *c, const u8 *context, int length, int enabled) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    const auto name = viewOf(context, length);
    if (name.empty()) return 0;
    if (enabled >= 0) s.input_->setContextEnabled(name, enabled != 0);
    return s.input_->contextEnabled(name) ? 1 : 0;
  };
  access_.inputRole = [](void *c, u32 role, u8 *out, int capacity) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    const auto &map = s.input_->map();
    const std::string &name = role == 0 ? map.moveAction() : role == 1 ? map.lookAction() : map.jumpAction();
    if (role > 2 || name.empty()) return -1;
    const int size = static_cast<int>(name.size());
    if (!out || capacity < size) return size;
    std::memcpy(out, name.data(), name.size());
    return size;
  };

  access_.getProperty = [](void *c, u64 id, u64 instance, const u8 *propertyId, int length, u32 *kind, u64 *bits) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    if (!kind || !bits) return 0;
    scene::ComponentPropertyValue value;
    s.lastStatus_ = s.world_->getProperty({s.world_->handle(static_cast<ObjectId>(id)), instance}, viewOf(propertyId, length), value);
    if (s.lastStatus_ != WorldStatus::Ok) return 0;
    if (const auto *number = std::get_if<float>(&value)) {
      u32 pattern = 0;
      std::memcpy(&pattern, number, sizeof(pattern));
      *kind = 0;
      *bits = pattern;
    } else if (const auto *boolean = std::get_if<bool>(&value)) { *kind = 1; *bits = *boolean ? 1 : 0; }
    else if (const auto *enumeration = std::get_if<u32>(&value)) { *kind = 2; *bits = *enumeration; }
    else if (const auto *reference = std::get_if<scene::ObjectReference>(&value)) { *kind = 3; *bits = reference->id; }
    else return 0;
    return 1;
  };
  access_.setProperty = [](void *c, u64 id, u64 instance, const u8 *propertyId, int length, u32 kind, u64 bits) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    scene::ComponentPropertyValue value;
    switch (kind) {
      case 0: {
        float number = 0;
        const u32 pattern = static_cast<u32>(bits);
        std::memcpy(&number, &pattern, sizeof(number));
        value = number;
        break;
      }
      case 1: value = bits != 0; break;
      case 2: value = static_cast<u32>(bits); break;
      case 3: value = scene::ObjectReference{bits}; break;
      default: s.lastStatus_ = WorldStatus::InvalidArgument; return 0;
    }
    s.lastStatus_ = s.world_->setProperty({s.world_->handle(static_cast<ObjectId>(id)), instance}, viewOf(propertyId, length), value);
    return s.lastStatus_ == WorldStatus::Ok;
  };

  access_.getRenderingState = [](void *c, u32 expectedWorld, scene::ScriptRenderingState *out) -> int {
    auto &s=*static_cast<ScriptBridge *>(c);
    if(!out||out->size!=sizeof(*out)||!s.rendering_.active(expectedWorld)) {s.lastStatus_=WorldStatus::ForeignWorld;return 0;}
    out->world=expectedWorld;out->pending=s.rendering_.pending();out->pendingRequestId=s.rendering_.pendingId();
    out->lastRequestSucceeded=s.rendering_.lastRequestSucceeded();out->effectiveAvailable=s.rendering_.effectiveAvailable();
    out->requested=toAbi(s.rendering_.requested());
    out->effective=toAbi(s.rendering_.effective());
    const auto &cap=s.rendering_.capabilities();out->capabilities.profile=(u32)cap.profile;
    out->capabilities.recommendedProfile=(u32)cap.qualityRecommendation.profile;
    out->capabilities.recommendationSource=(u32)cap.qualityRecommendation.evidence;
    out->capabilities.maximumImage2DSize=cap.maximumImage2DSize;out->capabilities.maximumImageArrayLayers=cap.maximumImageArrayLayers;
    out->capabilities.supportsDepthSampling=cap.supportsDepthSampling;out->capabilities.maximumSamplerAnisotropy=cap.maximumSamplerAnisotropy;out->capabilities.displayHz=cap.displayHz;
    s.lastStatus_=WorldStatus::Ok;return 1;
  };
  access_.setRenderingSettings = [](void *c,u32 expectedWorld,const scene::ScriptRenderingSettings *value,u64 *request)->int {
    auto &s=*static_cast<ScriptBridge *>(c);
    if(!value||value->size!=sizeof(*value)||!request) {s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    if(!s.rendering_.active(expectedWorld)) {s.lastStatus_=WorldStatus::ForeignWorld;return 0;}
    if(!s.rendering_.request(expectedWorld,fromAbi(*value),*request)) {s.lastStatus_=WorldStatus::Rejected;return 0;}
    s.lastStatus_=WorldStatus::Ok;return 1;
  };
  access_.copyRenderingDiagnostics = [](void *c,u32 expectedWorld,u8 *out,int capacity)->int {
    auto &s=*static_cast<ScriptBridge *>(c);
    if(!s.rendering_.active(expectedWorld)||capacity<0) return -1;
    std::string text;
    const auto &p=s.rendering_.effective();
    for(u32 i=0;i<p.clampCount;++i) {if(i) text+='\n';text+=p.clamps[i].axis;text+='=';text+=renderer::policyClampName(p.clamps[i].reason);}
    const int size=(int)text.size();if(!out||capacity<size)return size;
    std::memcpy(out,text.data(),text.size());return size;
  };
  access_.getComponentResource = [](void *c,u64 id,u64 instance,const u8 *property,int length,u32 slot,scene::ScriptAssetGuid *out)->int {
    auto &s=*static_cast<ScriptBridge *>(c);if(!out||id>std::numeric_limits<ObjectId>::max()){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    resources::AssetGuid value;s.lastStatus_=s.world_->getResource({s.world_->handle((ObjectId)id),instance},viewOf(property,length),slot,value);
    if(s.lastStatus_!=WorldStatus::Ok) return 0;
    out->high=value.high;out->low=value.low;return 1;
  };
  access_.setComponentResource = [](void *c,u64 id,u64 instance,const u8 *property,int length,u32 slot,scene::ScriptAssetGuid value)->int {
    auto &s=*static_cast<ScriptBridge *>(c);if(!s.assets_||id>std::numeric_limits<ObjectId>::max()){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    const std::span<const resources::EnvironmentProfile> profiles=s.environmentProfiles_?
        std::span<const resources::EnvironmentProfile>(*s.environmentProfiles_):std::span<const resources::EnvironmentProfile>{};
    s.lastStatus_=s.world_->setResource({s.world_->handle((ObjectId)id),instance},viewOf(property,length),slot,
                                        {value.high,value.low},*s.assets_,profiles,s.resourceAvailable_);
    return s.lastStatus_==WorldStatus::Ok;
  };
  access_.getComponentSlotProperty=[](void *c,u64 id,u64 instance,const u8 *property,int length,u32 slot,
                                      u32 *kind,u64 *bits)->int {
    auto &s=*static_cast<ScriptBridge*>(c);if(!kind||!bits||id>std::numeric_limits<ObjectId>::max()){
      s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    scene::ComponentPropertyValue value;
    s.lastStatus_=s.world_->getSlotProperty({s.world_->handle((ObjectId)id),instance},viewOf(property,length),slot,value);
    if(s.lastStatus_!=WorldStatus::Ok)return 0;
    if(const auto *number=std::get_if<float>(&value)){u32 raw;std::memcpy(&raw,number,sizeof(raw));*kind=0;*bits=raw;return 1;}
    if(const auto *enumeration=std::get_if<u32>(&value)){*kind=2;*bits=*enumeration;return 1;}
    s.lastStatus_=WorldStatus::InvalidArgument;return 0;
  };
  access_.setComponentSlotProperty=[](void *c,u64 id,u64 instance,const u8 *property,int length,u32 slot,
                                      u32 kind,u64 bits)->int {
    auto &s=*static_cast<ScriptBridge*>(c);if(id>std::numeric_limits<ObjectId>::max()){
      s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    scene::ComponentPropertyValue value;
    if(kind==0){
      if(bits>std::numeric_limits<u32>::max()){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
      const u32 raw=(u32)bits;float number;std::memcpy(&number,&raw,sizeof(number));value=number;
    }
    else if(kind==2){
      if(bits>std::numeric_limits<u32>::max()){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
      value=(u32)bits;
    }
    else{s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    s.lastStatus_=s.world_->setSlotProperty({s.world_->handle((ObjectId)id),instance},viewOf(property,length),slot,value,
                                            s.resourceAvailable_);
    return s.lastStatus_==WorldStatus::Ok;
  };
  access_.characterMove=[](void *c,u64 id,const float *move)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(!move||id>std::numeric_limits<ObjectId>::max()) {s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    const auto handle=s.world_->handle((ObjectId)id);
    s.lastStatus_=s.world_->validate(handle);
    if(s.lastStatus_!=WorldStatus::Ok) return 0;
    const auto *object=s.world_->find(handle);
    if(!object->components.find(scene::Character::descriptor)) {s.lastStatus_=WorldStatus::ComponentMissing;return 0;}
    if(!std::isfinite(move[0])||!std::isfinite(move[1])||!std::isfinite(move[2])||
       move[0]<-1||move[0]>1||move[1]<-1||move[1]>1) {s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    s.lastStatus_=s.physics_->setCharacterScriptMove((ObjectId)id,move[0],move[1],move[2])?
        WorldStatus::Ok:WorldStatus::Rejected;
    return s.lastStatus_==WorldStatus::Ok;
  };
  access_.characterJump=[](void *c,u64 id)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(id>std::numeric_limits<ObjectId>::max()) {s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    const auto handle=s.world_->handle((ObjectId)id);
    s.lastStatus_=s.world_->validate(handle);
    if(s.lastStatus_!=WorldStatus::Ok) return 0;
    const auto *object=s.world_->find(handle);
    if(!object->components.find(scene::Character::descriptor)) {s.lastStatus_=WorldStatus::ComponentMissing;return 0;}
    s.lastStatus_=s.physics_->jumpCharacter((ObjectId)id)?WorldStatus::Ok:WorldStatus::Rejected;
    return s.lastStatus_==WorldStatus::Ok;
  };
  access_.cameraLook=[](void *c,u64 id,const float *delta)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(!delta||id>std::numeric_limits<ObjectId>::max()) {s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    s.lastStatus_=s.world_->applyCameraLook(s.world_->handle((ObjectId)id),delta[0],delta[1]);
    return s.lastStatus_==WorldStatus::Ok;
  };
}

bool ScriptBridge::start(GameWorld &world, ScenePhysics &physics, InputService &input) {
  stop();
  diagnostics_.clear();
  if (!hasScripts(world.graph())) return true;
  if (!api_.available()) { diagnostics_ = "Runtime C# indisponível; aplique o código antes de Play"; return false; }
  world_ = &world;
  physics_ = &physics;
  input_ = &input;
  lastStatus_ = WorldStatus::Ok;
  if (!rendering_.begin(world.worldId(), initialEffective_)) {
    diagnostics_ = "Política gráfica de execução não configurada"; world_=nullptr;physics_=nullptr;input_=nullptr;return false;
  }
  installAccess();
  const auto data = attachments(world.graph());
  if (api_.start(reinterpret_cast<const u8 *>(root_.data()), static_cast<int>(root_.size()),
      reinterpret_cast<const u8 *>(data.data()), static_cast<int>(data.size()), &access_) != 0) {
    collectDiagnostics();
    world_ = nullptr;
    physics_ = nullptr;
    rendering_.end();
    return false;
  }
  running_ = true;
  collectDiagnostics();
  return true;
}

void ScriptBridge::collectDiagnostics() {
  if (!api_.copyDiagnostics) return;
  const int size = api_.copyDiagnostics(nullptr, 0);
  if (size <= 0 || size > 4 * 1024 * 1024) return;
  std::string text(static_cast<usize>(size), '\0');
  if (api_.copyDiagnostics(reinterpret_cast<u8 *>(text.data()), size) == size) diagnostics_ = std::move(text);
}

bool ScriptBridge::update(float elapsed) {
  if (!running_) return true;
  physics_->beginScriptInputFrame();
  const bool ok = api_.update(elapsed) == 0;
  collectDiagnostics();
  return ok;
}

bool ScriptBridge::fixedUpdate(float elapsed) {
  if (!running_) return true;
  const bool ok = api_.fixedUpdate(elapsed) == 0;
  collectDiagnostics();
  return ok;
}

QueryFilter ScriptBridge::queryFilter(const scene::ScriptQueryFilter &filter) const {
  QueryFilter value;
  value.gameplayLayerMask = filter.gameplayLayerMask;
  value.includeStatic = (filter.flags & 1u) != 0;
  value.includeDynamic = (filter.flags & 2u) != 0;
  value.includeSensors = (filter.flags & 4u) != 0;
  value.ignore = filter.ignore <= std::numeric_limits<ObjectId>::max()
                     ? static_cast<ObjectId>(filter.ignore) : kInvalidObject;
  return value;
}

QueryShapeDesc ScriptBridge::queryShape(const scene::ScriptShapeQuery &shape) {
  QueryShapeDesc value;
  value.kind = static_cast<QueryShapeKind>(shape.kind > 2 ? 1u : shape.kind);
  std::copy(shape.halfExtent, shape.halfExtent + 3, value.halfExtent);
  value.radius = shape.radius;
  value.halfHeight = shape.halfHeight;
  std::copy(shape.rotation, shape.rotation + 4, value.rotation);
  return value;
}

void ScriptBridge::copyHits(const std::vector<QueryHit> &hits, u32 total, scene::ScriptQueryHit *out, int capacity) {
  if (!out || capacity <= 0) return;
  const u32 copied = std::min<u32>(static_cast<u32>(capacity), std::min<u32>(total, static_cast<u32>(hits.size())));
  for (u32 i = 0; i < copied; ++i) {
    const auto &hit = hits[i];
    scene::ScriptQueryHit value{};
    value.object = hit.object;
    value.collider = hit.colliderInstance;
    std::copy(hit.point, hit.point + 3, value.point);
    std::copy(hit.normal, hit.normal + 3, value.normal);
    value.distance = hit.distance;
    value.fraction = hit.fraction;
    value.flags = (hit.hasNormal ? 1u : 0u) | (hit.isSensor ? 2u : 0u);
    out[i] = value;
  }
}

bool ScriptBridge::contact(const ContactEvent &event) {
  if (!running_ || !api_.contact) return true;
  const float *normal = event.hasNormal ? event.normal : nullptr;
  // Os dois lados recebem o evento, cada um com o outro objeto: um contato não
  // tem "dono", e obrigar o projeto a saber qual corpo o backend listou
  // primeiro seria uma regra invisível.
  const bool ok = api_.contact(event.first, event.second, event.phase, normal) == 0 &&
                  api_.contact(event.second, event.first, event.phase, normal) == 0;
  collectDiagnostics();
  return ok;
}

bool ScriptBridge::trigger(ObjectId sensor, ObjectId other, u32 phase) {
  if (!running_) return true;
  const bool ok = api_.trigger(sensor, other, phase) == 0;
  collectDiagnostics();
  return ok;
}

void ScriptBridge::stop() {
  if (running_) api_.stop();
  rendering_.end();
  running_ = false;
  world_ = nullptr;
  physics_ = nullptr;
  input_ = nullptr;
  access_ = scene::ScriptSceneAccess{};
}

} // namespace ae::runtime

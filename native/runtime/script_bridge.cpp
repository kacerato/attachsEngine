#include "runtime/script_bridge.h"
#include "runtime/transform_math.h"
#include "scene/script_behavior.h"
#include "scene/animation.h"
#include "scene/character.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include "runtime/scene_animator_graph.h"
#include "scene/animator.h"
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace ae::runtime {
namespace {
bool validQueryFilter(const scene::ScriptQueryFilter *filter) {
  return filter&&filter->size==sizeof(*filter)&&filter->reserved==0&&filter->flags<=7&&
         filter->ignore<=std::numeric_limits<ObjectId>::max();
}

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
  o.temporalUpscalerQuality=(u32)s.temporalUpscalerQuality;
  o.textureStreaming=(u32)s.textureStreaming;o.textureStreamingBudgetMegabytes=s.textureStreamingBudgetMegabytes;
  o.textureStreamingMaxLevelReduction=s.textureStreamingMaxLevelReduction;
  o.textureStreamingUploadKilobytesPerFrame=s.textureStreamingUploadKilobytesPerFrame;
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
  o.temporalUpscalerQuality=(renderer::TemporalUpscalerQuality)s.temporalUpscalerQuality;
  o.textureStreaming=(renderer::FeatureOverride)s.textureStreaming;o.textureStreamingBudgetMegabytes=s.textureStreamingBudgetMegabytes;
  o.textureStreamingMaxLevelReduction=s.textureStreamingMaxLevelReduction;
  o.textureStreamingUploadKilobytesPerFrame=s.textureStreamingUploadKilobytesPerFrame;
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
  o.temporalUpscalerQuality=(u32)p.post.temporalUpscalerQuality;
  o.textureStreaming=p.textures.streaming;o.textureStreamingBudgetMegabytes=static_cast<u32>(p.textures.streamingBudgetBytes>>20);
  o.textureStreamingMaxLevelReduction=p.textures.streamingMaxLevelReduction;
  o.textureStreamingUploadKilobytesPerFrame=p.textures.streamingUploadBytesPerFrame>>10;
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

namespace {
// Um valor autoral de campo no JSON que o BehaviorWorld converte para o tipo do
// membro C#. Compartilhado pelos anexos do Start e pela edição ao vivo.
void writeScriptValue(std::ostream &out, const scene::ScriptPropertyValue &p) {
  if (const auto element = scene::scriptArrayElementType(p.valueType); !element.empty()) {
    std::vector<std::string> items;
    scene::parseScriptArray(p.value, items);
    out << '[';
    for (usize i = 0; i < items.size(); ++i) {
      if (i) out << ',';
      writeScriptValue(out, {p.id, std::string(element), items[i]});
    }
    out << ']';
    return;
  }
  if (p.valueType == "string") jsonString(out, p.value);
  else if (p.valueType == "asset") { out << "{\"AssetId\":"; jsonString(out, p.value); out << '}'; }
  else if (p.valueType == "object") { std::istringstream in(p.value); u64 v = 0; in >> v; out << "{\"ObjectId\":" << v << '}'; }
  else if (scene::scriptCurveType(p.valueType)) {
    scene::ScriptCurve curve;
    scene::parseScriptCurve(p.value, curve);
    out << "{\"PreWrapMode\":" << static_cast<u32>(curve.pre) << ",\"PostWrapMode\":" << static_cast<u32>(curve.post) << ",\"Keys\":[";
    for (usize i = 0; i < curve.keys.size(); ++i) {
      const auto &k = curve.keys[i];
      out << (i ? "," : "") << "{\"Time\":" << k.time << ",\"Value\":" << k.value << ",\"InTangent\":" << k.in
          << ",\"OutTangent\":" << k.out << ",\"LeftMode\":" << static_cast<u32>(k.left) << ",\"RightMode\":"
          << static_cast<u32>(k.right) << ",\"Broken\":" << (k.broken ? "true" : "false") << '}';
    }
    out << "]}";
  } else if (scene::scriptGradientType(p.valueType)) {
    scene::ScriptGradient gradient;
    scene::parseScriptGradient(p.value, gradient);
    out << "{\"Mode\":" << static_cast<u32>(gradient.mode) << ",\"ColorKeys\":[";
    for (usize i = 0; i < gradient.colors.size(); ++i) {
      const auto &key = gradient.colors[i];
      out << (i ? "," : "") << "{\"Time\":" << key.time << ",\"Color\":{\"R\":" << key.rgb[0] << ",\"G\":" << key.rgb[1]
          << ",\"B\":" << key.rgb[2] << ",\"A\":1}}";
    }
    out << "],\"AlphaKeys\":[";
    for (usize i = 0; i < gradient.alphas.size(); ++i)
      out << (i ? "," : "") << "{\"Time\":" << gradient.alphas[i].time << ",\"Alpha\":" << gradient.alphas[i].alpha << '}';
    out << "]}";
  } else if (scene::scriptColorType(p.valueType)) {
    float rgba[4]{1, 1, 1, 1};
    scene::parseScriptColor(p.value, rgba);
    out << "{\"R\":" << rgba[0] << ",\"G\":" << rgba[1] << ",\"B\":" << rgba[2] << ",\"A\":" << rgba[3] << '}';
  } else if (!scene::scriptComponentTypeId(p.valueType).empty()) {
    u64 object = 0, instance = 0;
    scene::parseScriptComponentValue(p.value, object, instance);
    out << "{\"ObjectId\":" << object << ",\"InstanceId\":" << instance << '}';
  } else if (p.valueType == "vector3") {
    std::istringstream in(p.value);
    in.imbue(std::locale::classic());
    float x = 0, y = 0, z = 0;
    in >> x >> y >> z;
    out << "{\"X\":" << x << ",\"Y\":" << y << ",\"Z\":" << z << '}';
  } else if (p.valueType == "float") { std::istringstream in(p.value); in.imbue(std::locale::classic()); float v = 0; in >> v; out << v; }
  else if (p.valueType == "int32" || p.valueType == "enum") { std::istringstream in(p.value); i32 v = 0; in >> v; out << v; }
  else out << p.value;
}
}

std::string ScriptBridge::attachments(const SceneGraph &graph,ObjectId root) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(std::numeric_limits<float>::max_digits10) << '[';
  std::vector<ObjectId> ids;
  graph.collectSubtree(root?root:graph.root(), ids);
  bool first = true;
  for (auto id : ids) {
    const auto &object = *graph.find(id);
    // Instâncias inativas também pertencem à sessão: o runtime adia o Awake
    // até a primeira ativação, sem perder seus campos nem recriar a instância.
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
        writeScriptValue(out, p);
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
void ScriptBridge::installExtensions() {
  componentOperations_ = scene::ScriptComponentOperations{};
  componentOperations_.invoke=[](void *context,u64 object,u32 worldId,u32 generation,u64 instance,const u8 *method,int methodLength,
                                 const scene::ComponentOperationValue *arguments,int count,scene::ComponentOperationValue *result)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_){self.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(object>std::numeric_limits<ObjectId>::max()||!method||methodLength<=0||methodLength>256||count<0||
       count>static_cast<int>(scene::kComponentEventPayloadLimit)||(count&&!arguments)||!result) {
      self.lastStatus_=WorldStatus::InvalidArgument;return 0;
    }
    const ComponentHandle handle{{worldId,static_cast<ObjectId>(object),generation},instance};
    self.lastStatus_=invokeComponentMethod(self.operationServices(),handle,viewOf(method,methodLength),
                                           std::span(arguments,static_cast<usize>(count)),*result);
    return self.lastStatus_==WorldStatus::Ok;
  };
  componentOperations_.pollEvents=[](void *context,scene::ScriptComponentEvent *events,int capacity)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_){self.lastStatus_=WorldStatus::NotRunning;return -1;}
    if(capacity<0||(capacity&&!events)){self.lastStatus_=WorldStatus::InvalidArgument;return -1;}
    if(!self.events_||!capacity) return 0;
    int written=0;
    self.events_->consume(ComponentEventQueue::Consumer::Scripts,[&](const ComponentEventRecord &record,u64 lost) {
      auto &out=events[written++];
      out=scene::ScriptComponentEvent{};
      out.object=record.object.id;out.instance=record.instance;out.world=record.object.world;out.generation=record.object.generation;
      out.type=static_cast<u32>(scene::componentSchemas.size());
      for(usize i=0;i<scene::componentSchemas.size();++i) if(scene::componentSchemas[i].type==record.type) {out.type=static_cast<u32>(i);break;}
      out.event=record.event;out.count=record.count;out.lost=static_cast<u32>(std::min<u64>(lost,std::numeric_limits<u32>::max()));
      for(u32 i=0;i<record.count;++i) out.values[i]=record.values[i];
    },static_cast<usize>(capacity));
    return written;
  };
  componentOperations_.eventName=[](void *,u32 type,u32 event,u8 *buffer,int capacity)->int {
    if(type>=scene::componentSchemas.size()) return -1;
    const auto &descriptor=*scene::componentSchemas[type].type;
    if(event>=descriptor.events.size()) return -1;
    const std::string name=std::string(descriptor.id)+"/"+std::string(descriptor.events[event].id);
    if(buffer && capacity>0) std::copy_n(name.data(),std::min<usize>(name.size(),static_cast<usize>(capacity)),buffer);
    return static_cast<int>(name.size());
  };
  componentOperations_.declaresEvent=[](void *,const u8 *name,int length)->int {
    const auto text=viewOf(name,length);const auto slash=text.find('/');
    if(slash==std::string_view::npos) return 0;
    const auto *schema=scene::findComponentSchema(text.substr(0,slash));
    return schema && scene::findComponentEvent(*schema->type,text.substr(slash+1))?1:0;
  };
  viewOperations_ = scene::ScriptViewOperations{};
  viewOperations_.state=[](void *context,scene::ScriptViewState *out)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_||!self.gameView_){self.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(!out||out->size!=sizeof(*out)){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    const auto &view=*self.gameView_;
    *out=scene::ScriptViewState{};
    out->flags=(view.valid?scene::kScriptViewValid:0u)|(view.safeAreaReported?scene::kScriptViewSafeArea:0u)|(view.camera?scene::kScriptViewAuthoredCamera:0u);
    out->width=view.width;out->height=view.height;out->dpi=view.dpi;
    // Sem recorte informado, a área segura é a vista inteira, e a flag diz isso.
    out->safeX=view.safeAreaReported?view.safeX:0;out->safeY=view.safeAreaReported?view.safeY:0;
    out->safeWidth=view.safeAreaReported?view.safeWidth:view.width;out->safeHeight=view.safeAreaReported?view.safeHeight:view.height;
    out->platform=static_cast<u32>(view.platform);out->camera=view.camera;
    return 1;
  };
  viewOperations_.screenRay=[](void *context,float x,float y,float *origin,float *direction)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_||!self.gameView_){self.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(!origin||!direction||!gameViewScreenRay(*self.gameView_,x,y,origin,direction)){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    return 1;
  };
  viewOperations_.worldToScreen=[](void *context,const float *world,float *screen)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_||!self.gameView_){self.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(!world||!screen||!gameViewWorldToScreen(*self.gameView_,world,screen)){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    return 1;
  };
  debugOperations_ = scene::ScriptDebugOperations{};
  debugOperations_.drawLine=[](void *context,const float *from,const float *to,u32 rgba,float seconds)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_||!self.debugLines_){self.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(!from||!to||!self.debugLines_->add(from,to,rgba,seconds)){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    return 1;
  };
  hierarchyOperations_ = scene::ScriptHierarchyOperations{};
  hierarchyOperations_.pollChanges=[](void *context,scene::ScriptHierarchyChange *changes,int capacity)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_){self.lastStatus_=WorldStatus::NotRunning;return -1;}
    if(capacity<0||(capacity&&!changes)){self.lastStatus_=WorldStatus::InvalidArgument;return -1;}
    auto &pending=self.pendingHierarchy_;
    if(pending.empty()) {
      auto taken=self.world_->takeHierarchyChanges();
      pending.assign(taken.begin(),taken.end());
    }
    int written=0;
    while(written<capacity && !pending.empty()) {
      const auto change=pending.front();pending.pop_front();
      changes[written++]=scene::ScriptHierarchyChange{change.object.id,change.object.generation,static_cast<u32>(change.kind)};
    }
    return written;
  };
  sceneOperations_ = scene::ScriptSceneOperations{};
  sceneOperations_.active=[](void *context,u8 *buffer,int capacity)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_){self.lastStatus_=WorldStatus::NotRunning;return -1;}
    if(capacity<0||(capacity&&!buffer)){self.lastStatus_=WorldStatus::InvalidArgument;return -1;}
    if(buffer) std::copy_n(self.activeScene_.data(),std::min<usize>(self.activeScene_.size(),static_cast<usize>(capacity)),buffer);
    return static_cast<int>(self.activeScene_.size());
  };
  sceneOperations_.count=[](void *context)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_||!self.sceneCatalog_){self.lastStatus_=WorldStatus::NotRunning;return -1;}
    return static_cast<int>(std::min<usize>(self.sceneCatalog_().size(),4096));
  };
  sceneOperations_.nameAt=[](void *context,u32 index,u8 *buffer,int capacity)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_||!self.sceneCatalog_){self.lastStatus_=WorldStatus::NotRunning;return -1;}
    const auto names=self.sceneCatalog_();
    if(index>=names.size()||capacity<0||(capacity&&!buffer)){self.lastStatus_=WorldStatus::InvalidArgument;return -1;}
    if(buffer) std::copy_n(names[index].data(),std::min<usize>(names[index].size(),static_cast<usize>(capacity)),buffer);
    return static_cast<int>(names[index].size());
  };
  sceneOperations_.loadAdditive=[](void *context,u64 parent,const u8 *name,int length)->u64 {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_||!self.sceneLoader_){self.lastStatus_=WorldStatus::NotRunning;return 0;}
    const auto request=viewOf(name,length);
    if(parent>std::numeric_limits<ObjectId>::max()||request.empty()){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    const auto destination=self.world_->handle(static_cast<ObjectId>(parent));
    self.lastStatus_=self.world_->validate(destination);if(self.lastStatus_!=WorldStatus::Ok) return 0;
    SceneGraph graph;std::string canonical,error;
    if(!self.sceneLoader_(request,graph,canonical,error)) {
      self.lastStatus_=WorldStatus::UnknownResource;
      if(self.logSink_) self.logSink_(parent,"Cena recusada: "+error);
      return 0;
    }
    ObjectCloneMap mapping;
    const auto created=self.world_->instantiateScene(graph,destination,canonical,mapping,self.lastStatus_);
    return created.valid()?created.id:0;
  };
  sceneOperations_.requestSingle=[](void *context,const u8 *name,int length)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_||!self.sceneLoader_){self.lastStatus_=WorldStatus::NotRunning;return 0;}
    const auto request=viewOf(name,length);
    if(request.empty()){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    // Um pedido por quadro: dois LoadScene no mesmo quadro seriam uma corrida
    // sem vencedor definido; o segundo é recusado de forma explícita.
    if(self.sceneRequest_.pending){self.lastStatus_=WorldStatus::LimitReached;return 0;}
    SceneRequest pending;std::string error;
    // Carregar já agora recusa nome ou arquivo inválido no mesmo callback.
    if(!self.sceneLoader_(request,pending.graph,pending.name,error)) {
      self.lastStatus_=WorldStatus::UnknownResource;
      if(self.logSink_) self.logSink_(0,"Cena recusada: "+error);
      return 0;
    }
    pending.pending=true;self.sceneRequest_=std::move(pending);
    self.lastStatus_=WorldStatus::Ok;return 1;
  };
  hapticsOperations_ = scene::ScriptHapticsOperations{};
  hapticsOperations_.vibrate=[](void *context,u32 milliseconds,float amplitude)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_||!self.haptics_){self.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(milliseconds<1||milliseconds>5000||!std::isfinite(amplitude)||amplitude<0||amplitude>1){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    if(!self.haptics_(milliseconds,amplitude)){self.lastStatus_=WorldStatus::Rejected;return 0;}
    return 1;
  };
  animatorOperations_={};
  // Animator: o componente é validado pelo identificador completo (mundo,
  // objeto, geração, instância); o nome chega como UTF-8 com comprimento.
  const auto animatorHandle=[](ScriptBridge &s,u64 id,u32 worldId,u32 generation,u64 instance,ComponentHandle &handle){
    if(!s.world_||!s.world_->running()||!s.animators_){s.lastStatus_=WorldStatus::NotRunning;return false;}
    if(id>std::numeric_limits<ObjectId>::max()){s.lastStatus_=WorldStatus::InvalidArgument;return false;}
    handle={{worldId,ObjectId(id),generation},instance};
    s.lastStatus_=s.world_->validate(handle.object);if(s.lastStatus_!=WorldStatus::Ok)return false;
    if(s.world_->componentTypeId(handle)!=scene::Animator::descriptor.id){s.lastStatus_=WorldStatus::ComponentMissing;return false;}
    return true;
  };
  static decltype(animatorHandle) handleOf=animatorHandle;
  const auto animatorStatus=[](SceneAnimatorGraphs::Status status){
    switch(status){
      case SceneAnimatorGraphs::Status::Ok: return WorldStatus::Ok;
      case SceneAnimatorGraphs::Status::UnknownComponent: return WorldStatus::ComponentUnavailable;
      case SceneAnimatorGraphs::Status::MissingController: return WorldStatus::UnknownResource;
      case SceneAnimatorGraphs::Status::BoundParameter: return WorldStatus::Rejected;
      case SceneAnimatorGraphs::Status::UnknownParameter: case SceneAnimatorGraphs::Status::UnknownState: case SceneAnimatorGraphs::Status::UnknownLayer: return WorldStatus::UnknownResource;
      default: return WorldStatus::InvalidArgument;
    }
  };
  static decltype(animatorStatus) statusOf=animatorStatus;
  animatorOperations_.parameter=[](void *context,u64 id,u32 worldId,u32 generation,u64 instance,u32 operation,const u8 *name,int length,float value,float *result)->int {
    auto &s=*static_cast<ScriptBridge*>(context);ComponentHandle handle;
    if(!handleOf(s,id,worldId,generation,instance,handle))return 0;
    if(operation>5||!name||length<=0||length>int(scene::Animator::MaximumName)||!result){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    s.lastStatus_=statusOf(s.animators_->parameter(*s.world_,handle.object.id,instance,std::string_view(reinterpret_cast<const char*>(name),usize(length)),
                                                    SceneAnimatorGraphs::ParameterOperation(operation),value,*result));
    return s.lastStatus_==WorldStatus::Ok;
  };
  animatorOperations_.play=[](void *context,u64 id,u32 worldId,u32 generation,u64 instance,u32 layer,const u8 *state,int length,float crossFade)->int {
    auto &s=*static_cast<ScriptBridge*>(context);ComponentHandle handle;
    if(!handleOf(s,id,worldId,generation,instance,handle))return 0;
    constexpr usize maximumPath=(scene::Animator::MaximumDepth+1)*(scene::Animator::MaximumName+1)-1;
    if(!state||length<=0||length>int(maximumPath)){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    s.lastStatus_=statusOf(s.animators_->play(*s.world_,handle.object.id,instance,layer,std::string_view(reinterpret_cast<const char*>(state),usize(length)),crossFade));
    return s.lastStatus_==WorldStatus::Ok;
  };
  animatorOperations_.state=[](void *context,u64 id,u32 worldId,u32 generation,u64 instance,u32 layer,scene::ScriptAnimatorStateInfo *out)->int {
    auto &s=*static_cast<ScriptBridge*>(context);ComponentHandle handle;
    if(!handleOf(s,id,worldId,generation,instance,handle))return 0;
    if(!out||out->size!=sizeof(*out)){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    SceneAnimatorGraphs::Info info;s.lastStatus_=statusOf(s.animators_->info(*s.world_,handle.object.id,instance,layer,info));
    if(s.lastStatus_!=WorldStatus::Ok)return 0;
    out->flags=info.transitioning?1u:0u;out->state=info.state;out->next=info.next;out->normalizedTime=info.normalizedTime;out->progress=info.progress;
    return 1;
  };
  animatorOperations_.stateName=[](void *context,u64 id,u32 worldId,u32 generation,u64 instance,u32 layer,u64 state,u8 *buffer,int capacity)->int {
    auto &s=*static_cast<ScriptBridge*>(context);ComponentHandle handle;
    if(!handleOf(s,id,worldId,generation,instance,handle))return -1;
    const auto *a=s.animators_->configuration(*s.world_,handle.object.id,instance);
    if(!a||layer>=a->layers.size()){s.lastStatus_=WorldStatus::UnknownResource;return -1;}
    const auto *found=a->layers[layer].state(state);
    if(!found){s.lastStatus_=WorldStatus::UnknownResource;return -1;}
    const auto path=a->layers[layer].path(found->id);const int length=int(path.size());
    if(buffer&&capacity>=length)std::memcpy(buffer,path.data(),usize(length));
    s.lastStatus_=WorldStatus::Ok;return length;
  };
  motorControlOperations_={};
  motorControlOperations_.command=[](void *context,u64 id,u32 worldId,u32 generation,u64 instance,u32 operation,u32 source,const float *move,u32 jump,scene::ScriptMotorControlState *out)->int {
    auto &s=*static_cast<ScriptBridge*>(context);
    if(!s.world_||!s.world_->running()||!s.physics_){s.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(id>std::numeric_limits<ObjectId>::max()||operation>2||!out||out->size!=sizeof(*out)||jump>1||
       (operation<2&&(source<4||source>5))||(operation==0&&!move)){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    if(operation==0&&(!std::isfinite(move[0])||!std::isfinite(move[1])||!std::isfinite(move[2])||std::abs(move[0])>1||std::abs(move[1])>1)){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    const ComponentHandle handle{{worldId,ObjectId(id),generation},instance};
    s.lastStatus_=s.world_->validate(handle.object);if(s.lastStatus_!=WorldStatus::Ok)return 0;
    const auto type=s.world_->componentTypeId(handle);
    if(type!=scene::Character::descriptor.id&&type!=scene::DynamicBodyMotor::descriptor.id){s.lastStatus_=WorldStatus::ComponentMissing;return 0;}
    if(operation<2&&!s.world_->activeInHierarchy(handle.object)){s.lastStatus_=WorldStatus::Rejected;return 0;}
    const auto *entity=s.world_->find(handle.object);
    if(const auto *motor=entity->components.find(scene::DynamicBodyMotor::descriptor);motor&&!static_cast<const scene::DynamicBodyMotor&>(*motor).enabled&&operation<2){s.lastStatus_=WorldStatus::ComponentUnavailable;return 0;}
    if(operation==0&&!s.physics_->submitMotorControl(ObjectId(id),MotorControlSource(source),move[0],move[1],move[2],jump!=0)){s.lastStatus_=WorldStatus::Rejected;return 0;}
    if(operation==1&&!s.physics_->releaseMotorControl(ObjectId(id),MotorControlSource(source))){s.lastStatus_=WorldStatus::Rejected;return 0;}
    MotorControlSnapshot state;if(!s.physics_->motorControlState(ObjectId(id),state)){s.lastStatus_=WorldStatus::ComponentUnavailable;return 0;}
    const auto *dynamic=entity->components.find(scene::DynamicBodyMotor::descriptor);
    if(!s.world_->activeInHierarchy(handle.object)||(dynamic&&!static_cast<const scene::DynamicBodyMotor&>(*dynamic).enabled)){state={};state.focused=false;}
    out->source=u32(state.source);out->candidates=state.candidates;out->flags=(state.focused?1u:0u)|(state.measured?2u:0u)|(state.jump?4u:0u);
    out->right=state.right;out->forward=state.forward;out->yaw=state.yaw;out->priority=state.priority;
    s.lastStatus_=WorldStatus::Ok;return 1;
  };
  motorMotionOperations_={};
  motorMotionOperations_.state=[](void *context,u64 id,u32 worldId,u32 generation,u64 instance,scene::ScriptMotorMotionState *out)->int {
    auto &s=*static_cast<ScriptBridge*>(context);
    if(!s.world_||!s.world_->running()||!s.physics_){s.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(id>std::numeric_limits<ObjectId>::max()||!out||out->size!=sizeof(*out)){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    const ComponentHandle h{{worldId,ObjectId(id),generation},instance};
    s.lastStatus_=s.world_->validate(h.object);if(s.lastStatus_!=WorldStatus::Ok)return 0;
    const auto *v=s.world_->readComponent(h);
    if(!v){s.lastStatus_=WorldStatus::ComponentMissing;return 0;}
    if(!s.world_->activeInHierarchy(h.object)){s.lastStatus_=WorldStatus::ComponentUnavailable;return 0;}
    *out={};
    if(&v->type()==&scene::DynamicBodyMotor::descriptor) {
      if(!static_cast<const scene::DynamicBodyMotor&>(*v).enabled){s.lastStatus_=WorldStatus::ComponentUnavailable;return 0;}
      ScenePhysics::DynamicMotorState motion;
      if(!s.physics_->dynamicMotorState(ObjectId(id),motion)||!s.physics_->getBodyVelocity(ObjectId(id),out->velocity)){s.lastStatus_=WorldStatus::ComponentUnavailable;return 0;}
      out->flags=(motion.hasMeasuredStep?1u:0u)|(motion.grounded?2u:0u);out->support=motion.support;
      std::copy(motion.supportVelocity,motion.supportVelocity+3,out->groundVelocity);
      std::copy(motion.normal,motion.normal+3,out->normal);std::copy(motion.point,motion.point+3,out->point);
    } else if(&v->type()==&scene::Character::descriptor) {
      physics::CharacterMotor::RuntimeState motion;s.lastStatus_=s.physics_->characterState(*s.world_,ObjectId(id),motion);
      if(s.lastStatus_!=WorldStatus::Ok)return 0;
      out->flags=(motion.hasMeasuredStep?1u:0u)|(motion.groundState==AetherCharacterGroundState::OnGround?2u:0u);
      const auto copy=[](AetherVec3 a,float *b){b[0]=a.x;b[1]=a.y;b[2]=a.z;};
      copy(motion.velocity,out->velocity);copy(motion.groundVelocity,out->groundVelocity);copy(motion.groundNormal,out->normal);copy(motion.groundPoint,out->point);
    } else {s.lastStatus_=WorldStatus::ComponentMissing;return 0;}
    s.lastStatus_=WorldStatus::Ok;return 1;
  };
  access_.extension=[](void *context,const u8 *name,int length,u32 *version,u32 *size)->const void * {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!name||length<=0||length>256) return nullptr;
    const auto requested=viewOf(name,length);
    const auto publish=[&](const auto &table)->const void * {
      if(version) *version=table.version;
      if(size) *size=table.size;
      return &table;
    };
    if(requested==scene::kScriptComponentOperations) return publish(self.componentOperations_);
    if(requested==scene::kScriptView) return publish(self.viewOperations_);
    if(requested==scene::kScriptDebug) return publish(self.debugOperations_);
    if(requested==scene::kScriptHierarchy) return publish(self.hierarchyOperations_);
    if(requested==scene::kScriptScenes) return publish(self.sceneOperations_);
    if(requested==scene::kScriptMotorControl&&self.physics_)return publish(self.motorControlOperations_);
    if(requested==scene::kScriptAnimator&&self.animators_)return publish(self.animatorOperations_);
    if(requested==scene::kScriptMotorMotion&&self.physics_)return publish(self.motorMotionOperations_);
    // Família ausente de verdade: sem vibrador, não há tabela a oferecer.
    if(requested==scene::kScriptHaptics && self.haptics_) return publish(self.hapticsOperations_);
    return nullptr;
  };
}

void ScriptBridge::installAccess() {
  access_ = scene::ScriptSceneAccess{};
  access_.context = this;
  installExtensions();
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
    if (id > std::numeric_limits<ObjectId>::max()) { s.lastStatus_ = WorldStatus::InvalidArgument; return -1; }
    const auto handle = s.world_->handle(static_cast<ObjectId>(id));
    s.lastStatus_ = s.world_->validate(handle);
    if (s.lastStatus_ != WorldStatus::Ok) return -1;
    return s.world_->activeInHierarchy(handle) ? 1 : 0;
  };
  access_.getActiveSelf = [](void *c, u64 id) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    if (id > std::numeric_limits<ObjectId>::max()) { s.lastStatus_ = WorldStatus::InvalidArgument; return -1; }
    const auto handle = s.world_->handle(static_cast<ObjectId>(id));
    s.lastStatus_ = s.world_->validate(handle);
    if (s.lastStatus_ != WorldStatus::Ok) return -1;
    return s.world_->activeSelf(handle) ? 1 : 0;
  };
  access_.getTag=[](void *c,u64 id,u8 *out,int capacity)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(id>std::numeric_limits<ObjectId>::max() || capacity<0) {s.lastStatus_=WorldStatus::InvalidArgument;return -1;}
    const auto h=s.world_->handle(static_cast<ObjectId>(id));s.lastStatus_=s.world_->validate(h);
    if(s.lastStatus_!=WorldStatus::Ok) return -1;
    const auto tag=s.world_->tagOf(h);
    if(out && capacity>=static_cast<int>(tag.size())) std::memcpy(out,tag.data(),tag.size());
    return static_cast<int>(tag.size());
  };
  access_.setTag=[](void *c,u64 id,const u8 *text,int length)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(id>std::numeric_limits<ObjectId>::max() || !text || length<=0 || length>static_cast<int>(ObjectTags::MaximumNameBytes)) {
      s.lastStatus_=WorldStatus::InvalidArgument;return 0;
    }
    s.lastStatus_=s.world_->setTag(s.world_->handle(static_cast<ObjectId>(id)),viewOf(text,length));
    return s.lastStatus_==WorldStatus::Ok;
  };
  access_.compareTag=[](void *c,u64 id,const u8 *text,int length)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(id>std::numeric_limits<ObjectId>::max() || !text || length<=0 || length>static_cast<int>(ObjectTags::MaximumNameBytes)) {
      s.lastStatus_=WorldStatus::InvalidArgument;return -1;
    }
    bool matches=false;s.lastStatus_=s.world_->compareTag(s.world_->handle(static_cast<ObjectId>(id)),viewOf(text,length),matches);
    return s.lastStatus_==WorldStatus::Ok?(matches?1:0):-1;
  };
  access_.findTagged=[](void *c,const u8 *text,int length,u64 *out,int capacity,int firstOnly)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(!text || length<=0 || length>static_cast<int>(ObjectTags::MaximumNameBytes) || capacity<0 ||
       capacity>static_cast<int>(SceneGraph::kMaximumObjects) || (!out && capacity) || (firstOnly!=0 && firstOnly!=1)) {
      s.lastStatus_=WorldStatus::InvalidArgument;return -1;
    }
    u32 count=0;s.lastStatus_=s.world_->findTagged(viewOf(text,length),{out,static_cast<usize>(capacity)},count,firstOnly!=0);
    return s.lastStatus_==WorldStatus::Ok?static_cast<int>(count):-1;
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
  access_.addBehavior = [](void *c,u64 id,const u8 *type,int typeLength,const u8 *source,int sourceLength)->u64 {
    auto &s=*static_cast<ScriptBridge *>(c);
    if(id>std::numeric_limits<ObjectId>::max() || !type || typeLength<=0 || typeLength>256 ||
       !source || sourceLength<=0 || sourceLength>1024) {s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    return s.world_->addBehavior(s.world_->handle(static_cast<ObjectId>(id)),viewOf(type,typeLength),
      viewOf(source,sourceLength),s.lastStatus_).instance;
  };
  access_.destroyAfter = [](void *c,u64 id,double seconds)->int {
    auto &s=*static_cast<ScriptBridge *>(c);
    if(id>std::numeric_limits<ObjectId>::max()) {s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    s.lastStatus_=s.world_->destroyAfter(s.world_->handle(static_cast<ObjectId>(id)),seconds);
    return s.lastStatus_==WorldStatus::Ok;
  };
  access_.createPrimitive=[](void *c,u64 parent,u32 kind)->u64 {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(!s.world_) {s.lastStatus_=WorldStatus::NotRunning;return 0;}
    const auto type=static_cast<scene::PrimitiveType>(kind);
    if(!scene::validPrimitive(type) || parent>std::numeric_limits<ObjectId>::max()) {s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    return s.world_->createPrimitive(s.world_->handle(static_cast<ObjectId>(parent)),type,s.primitives_[kind],s.lastStatus_).id;
  };
  access_.instantiatePrefab=[](void *c,u64 parent,scene::ScriptAssetGuid asset)->u64 {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(!s.world_ || !s.prefabLoader_) {s.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(parent>std::numeric_limits<ObjectId>::max() || !(asset.high || asset.low)) {s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    const auto destination=s.world_->handle(static_cast<ObjectId>(parent));
    s.lastStatus_=s.world_->validate(destination);if(s.lastStatus_!=WorldStatus::Ok) return 0;
    Prefab prefab;std::string error;
    if(!s.prefabLoader_({asset.high,asset.low},prefab,error)) {
      s.lastStatus_=WorldStatus::UnknownResource;
      if(s.logSink_) s.logSink_(parent,"Prefab recusado: "+error);
      return 0;
    }
    ObjectCloneMap mapping;
    const auto created=s.world_->instantiate(prefab,destination,mapping,s.lastStatus_,error);
    if(!created.valid() && s.logSink_) s.logSink_(parent,"Prefab recusado: "+error);
    return created.id;
  };
  access_.instantiationAttachments=[](void *c,u64 root,u8 *out,int capacity)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(!s.world_ || root>std::numeric_limits<ObjectId>::max() || capacity<0 || !s.world_->handle(static_cast<ObjectId>(root)).valid()) {
      s.lastStatus_=WorldStatus::InvalidArgument;return -1;
    }
    const auto text=attachments(s.world_->graph(),static_cast<ObjectId>(root));
    if(text.size()>32*1024*1024) {s.lastStatus_=WorldStatus::LimitReached;return -1;}
    s.lastStatus_=WorldStatus::Ok;
    if(out && static_cast<usize>(capacity)>=text.size()) std::memcpy(out,text.data(),text.size());
    return static_cast<int>(text.size());
  };
  access_.instantiate=[](void *c,u64 source,u64 parent,u64 *pairs,int capacity)->int {
    auto &s=*static_cast<ScriptBridge *>(c);
    if(source>std::numeric_limits<ObjectId>::max() || parent>std::numeric_limits<ObjectId>::max() || capacity<0) {
      s.lastStatus_=WorldStatus::InvalidArgument;return -1;
    }
    const auto original=s.world_->handle(static_cast<ObjectId>(source));
    const auto destination=s.world_->handle(static_cast<ObjectId>(parent));
    if(!s.world_->alive(original) || !s.world_->alive(destination) || source==s.world_->graph().root()) {
      s.lastStatus_=WorldStatus::InvalidArgument;return -1;
    }
    std::vector<ObjectId> ids;s.world_->graph().collectSubtree(original.id,ids);
    std::erase_if(ids,[&](ObjectId id) {return !s.world_->handle(id).valid();});
    if(!pairs || static_cast<usize>(capacity)<ids.size()) {s.lastStatus_=WorldStatus::Ok;return static_cast<int>(ids.size());}
    ObjectCloneMap mapping;
    if(!s.world_->instantiate(original,destination,mapping,s.lastStatus_).valid()) return -1;
    for(usize i=0;i<ids.size();++i) {pairs[2*i]=ids[i];pairs[2*i+1]=mapping.at(ids[i]);}
    return static_cast<int>(ids.size());
  };
  access_.finishInstantiation=[](void *c,u64 root,int commit)->int {
    auto &s=*static_cast<ScriptBridge *>(c);
    if(root>std::numeric_limits<ObjectId>::max() || (commit!=0 && commit!=1)) {s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    s.lastStatus_=s.world_->finishInstantiation(s.world_->handle(static_cast<ObjectId>(root)),commit!=0);
    return s.lastStatus_==WorldStatus::Ok;
  };
  // --- consultas fisicas --------------------------------------------------
  access_.rayCast = [](void *c, const float *origin, const float *direction,
                       const scene::ScriptQueryFilter *filter, scene::ScriptQueryHit *out, int capacity) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    if(!s.world_||!s.physics_) {s.lastStatus_=WorldStatus::NotRunning;return -1;}
    if(!validPhysicsQueryRay(origin,direction)||!validQueryFilter(filter)||capacity<0||capacity>4096||(capacity&&!out)) {s.lastStatus_=WorldStatus::InvalidArgument;return -1;}
    std::vector<QueryHit> hits(static_cast<usize>(capacity));
    const u32 total = s.physics_->rayCastAll(origin, direction, s.queryFilter(*filter),
                                             capacity ? hits.data() : nullptr, static_cast<u32>(capacity));
    s.copyHits(hits, total, out, capacity);
    s.lastStatus_=WorldStatus::Ok;
    return static_cast<int>(total);
  };
  access_.shapeCast = [](void *c, const scene::ScriptShapeQuery *shape, const float *origin,
                         const float *direction, const scene::ScriptQueryFilter *filter,
                         scene::ScriptQueryHit *out) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    if(!s.world_||!s.physics_) {s.lastStatus_=WorldStatus::NotRunning;return -1;}
    if(!shape||!validPhysicsQueryRay(origin,direction)||!validQueryFilter(filter)||!out||!validPhysicsQueryShape(ScriptBridge::queryShape(*shape))) {s.lastStatus_=WorldStatus::InvalidArgument;return -1;}
    QueryHit hit;
    s.lastStatus_=WorldStatus::Ok;
    if (!s.physics_->shapeCast(ScriptBridge::queryShape(*shape), origin, direction, s.queryFilter(*filter), hit)) return 0;
    s.copyHits({hit}, 1, out, 1);
    return 1;
  };
  access_.overlap = [](void *c, const scene::ScriptShapeQuery *shape, const float *origin,
                       const scene::ScriptQueryFilter *filter, scene::ScriptQueryHit *out, int capacity) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    if(!s.world_||!s.physics_) {s.lastStatus_=WorldStatus::NotRunning;return -1;}
    if(!shape||!validPhysicsQueryVector(origin)||!validQueryFilter(filter)||capacity<0||capacity>4096||(capacity&&!out)||!validPhysicsQueryShape(ScriptBridge::queryShape(*shape))) {s.lastStatus_=WorldStatus::InvalidArgument;return -1;}
    std::vector<QueryHit> hits(static_cast<usize>(capacity));
    const u32 total = s.physics_->overlap(ScriptBridge::queryShape(*shape), origin, s.queryFilter(*filter),
                                          capacity ? hits.data() : nullptr, static_cast<u32>(capacity));
    s.copyHits(hits, total, out, capacity);
    s.lastStatus_=WorldStatus::Ok;
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
    if(!s.input_||!s.world_){s.lastStatus_=WorldStatus::NotRunning;return 0;}
    const auto name = viewOf(action, length);
    if (name.empty() || !out) return 0;
    if (!s.input_->map().find(name)) { s.lastStatus_ = WorldStatus::InvalidArgument; return 0; }
    s.input_->axis2(name, out);
    return 1;
  };
  access_.inputActionCommand=[](void*c,const u8*action,int length,u32 operation,scene::ScriptInputActionState*out)->int {
    auto&s=*static_cast<ScriptBridge*>(c);
    if(!s.input_||!s.world_){s.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(!out||out->size!=sizeof(*out)||operation>4){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    const auto name=viewOf(action,length);const auto*authored=s.input_->authoredMap().find(name);
    if(operation<3 && !authored){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    if((operation==1 && ((out->flags&~1u)||!s.input_->setActionEnabled(name,(out->flags&1)!=0))) ||
       (operation==2 && !s.input_->restoreActionEnabled(name)) ||
       (operation==4 && !s.input_->setDeviceGroups(out->deviceGroups))){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    scene::ScriptInputActionState result;
    if(operation>=3)result.deviceGroups=s.input_->deviceGroups();
    else {
      result.flags=(s.input_->actionEnabled(name)?1u:0u)|(authored->enabled?2u:0u);
      result.interaction=static_cast<u32>(authored->interaction);result.duration=authored->duration;
      result.deviceGroups=authored->deviceGroups;result.phase=static_cast<u32>(s.input_->phase(name));
      result.progress=s.input_->progress(name);result.elapsed=s.input_->elapsed(name);
    }
    *out=result;s.lastStatus_=WorldStatus::Ok;return 1;
  };
  access_.inputButton = [](void *c, const u8 *action, int length, u32 query) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    if(!s.input_||!s.world_){s.lastStatus_=WorldStatus::NotRunning;return -1;}
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
    if(!s.input_||!s.world_){s.lastStatus_=WorldStatus::NotRunning;return 0;}
    const auto name = viewOf(context, length);
    if (name.empty()) return 0;
    if (enabled >= 0) s.input_->setContextEnabled(name, enabled != 0);
    return s.input_->contextEnabled(name) ? 1 : 0;
  };
  access_.inputBindingCommand=[](void *c,const u8 *action,int length,u32 index,u32 operation,scene::ScriptInputBinding *value)->int {
    auto &s=*static_cast<ScriptBridge*>(c);if(!s.input_||!s.world_){s.lastStatus_=WorldStatus::NotRunning;return 0;}const auto name=viewOf(action,length);bool ok=false;
    if(operation==3){s.input_->removeAllOverrides();ok=true;}
    else if(operation==2)ok=s.input_->removeOverride(name,index);
    else if(value && (operation==0||operation==4)) {
      InputBinding binding;if(s.input_->binding(name,index,binding,operation==4)) {
        *value={static_cast<u32>(binding.source),binding.code,binding.negativeCode,binding.axis,binding.scale,binding.invert?1u:0u};ok=true;
      }
    } else if(value && operation==1 && value->invert<=1) {
      const InputBinding binding{static_cast<InputSource>(value->source),value->code,value->negativeCode,value->axis,value->scale,value->invert!=0};
      ok=s.input_->overrideBinding(name,index,binding);
    }
    s.lastStatus_=ok?WorldStatus::Ok:WorldStatus::InvalidArgument;return ok?1:0;
  };
  access_.inputProfile=[](void *c,u32 operation,const u8 *data,int length,u8 *out,int capacity)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(!s.input_||!s.world_){s.lastStatus_=WorldStatus::NotRunning;return -1;}
    if(operation==1) {
      const bool ok=data&&length>0&&length<=262144&&s.input_->importProfile({reinterpret_cast<const char*>(data),static_cast<usize>(length)});
      s.lastStatus_=ok?WorldStatus::Ok:WorldStatus::InvalidArgument;return ok?1:-1;
    }
    if(operation!=0 || capacity<0){s.lastStatus_=WorldStatus::InvalidArgument;return -1;}
    const auto profile=s.input_->exportProfile();const int size=static_cast<int>(profile.size());
    if(size>262144){s.lastStatus_=WorldStatus::Rejected;return -1;}
    if(out && capacity>=size)std::memcpy(out,profile.data(),profile.size());
    s.lastStatus_=WorldStatus::Ok;return size;
  };
  access_.numberTweenCreate=[](void*context,u64 object,u64 instance,const u8*property,int length,const scene::ScriptNumberTweenParameters*parameters,u64*out)->int {
    auto&s=*static_cast<ScriptBridge*>(context);
    if(!s.world_||!s.numberTweens_){s.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(object>std::numeric_limits<u32>::max()||!property||length<=0||length>127||!parameters||parameters->size!=sizeof(*parameters)||parameters->reserved||(parameters->flags&~1u)||!out){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    s.lastStatus_=s.numberTweens_->create(*s.world_,{s.world_->handle(static_cast<ObjectId>(object)),instance},std::string_view(reinterpret_cast<const char*>(property),static_cast<usize>(length)),parameters->destination,parameters->duration,parameters->easing,(parameters->flags&1)!=0,*out);
    return s.lastStatus_==WorldStatus::Ok?1:0;
  };
  access_.numberTweenCommand=[](void*context,u64 id,u32 operation,scene::ScriptNumberTweenState*output)->int {
    auto&s=*static_cast<ScriptBridge*>(context);
    if(!s.world_||!s.numberTweens_){s.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(!output||output->size!=sizeof(*output)){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    SceneNumberTweens::State state;s.lastStatus_=s.numberTweens_->command(*s.world_,id,operation,state);if(s.lastStatus_!=WorldStatus::Ok)return 0;
    output->status=static_cast<u32>(state.status);output->failure=static_cast<u32>(state.failure);output->elapsed=state.elapsed;output->value=state.value;output->duration=state.duration;
    output->flags=(state.paused?1u:0u)|(state.active?2u:0u)|(state.enabled?4u:0u);return 1;
  };
  access_.tweenCommand=[](void*context,u64 object,u64 instance,u32 operation,scene::ScriptTweenState*output)->int {
    auto&s=*static_cast<ScriptBridge*>(context);
    if(!s.world_||!s.tweens_){s.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(object>std::numeric_limits<u32>::max()||!output||output->size!=sizeof(*output)||output->reserved){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    const auto h=s.world_->handle(static_cast<ObjectId>(object));SceneTweens::State state;
    s.lastStatus_=s.tweens_->command(*s.world_,{h,instance},operation,state);if(s.lastStatus_!=WorldStatus::Ok)return 0;
    const auto*v=s.world_->readComponent({h,instance});const bool enabled=v&&static_cast<const scene::TransformTween*>(v)->enabled;
    output->status=static_cast<u32>(state.status);output->elapsed=state.elapsed;output->flags=(state.paused?1u:0u)|(enabled?2u:0u)|(s.world_->activeInHierarchy(h)?4u:0u);return 1;
  };
  access_.timerCommand=[](void *context,u64 object,u64 instance,u32 operation,float seconds,scene::ScriptTimerState *output)->int {
    auto &s=*static_cast<ScriptBridge*>(context);
    if(!s.world_||!s.timers_){s.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(object>std::numeric_limits<u32>::max()||!output||output->size!=sizeof(*output)){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    const auto handle=s.world_->handle(static_cast<ObjectId>(object));SceneTimers::State state;
    s.lastStatus_=s.timers_->command(*s.world_,{handle,instance},operation,seconds,state);
    if(s.lastStatus_!=WorldStatus::Ok)return 0;
    const auto *value=s.world_->readComponent({handle,instance});
    const bool enabled=value&&static_cast<const scene::Timer*>(value)->enabled;
    output->remaining=state.running?std::max(0.0,state.remaining):0;
    output->flags=(state.running?1u:0u)|(state.paused?2u:0u)|(state.completed?4u:0u)|(enabled?8u:0u)|(s.world_->activeInHierarchy(handle)?16u:0u);
    return 1;
  };
  access_.inputCaptureCommand=[](void *c,u32 operation,const u8 *name,int length,u32 index,u32 source,u32 flags,u32 cancelKey)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(!s.input_||!s.world_){s.lastStatus_=WorldStatus::NotRunning;return -1;}
    if(operation==1) {
      const bool ok=flags<=1&&s.input_->beginBindingCapture(viewOf(name,length),index,static_cast<InputSource>(source),flags!=0,cancelKey);
      s.lastStatus_=ok?WorldStatus::Ok:WorldStatus::InvalidArgument;if(!ok)return -1;
    } else if(operation==2)s.input_->cancelBindingCapture();
    else if(operation!=0){s.lastStatus_=WorldStatus::InvalidArgument;return -1;}
    s.lastStatus_=WorldStatus::Ok;return static_cast<int>(s.input_->captureStatus());
  };
  access_.inputRole = [](void *c, u32 role, u8 *out, int capacity) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    if(!s.input_||!s.world_){s.lastStatus_=WorldStatus::NotRunning;return -1;}
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

  access_.setTriple = [](void *c,u64 id,u64 instance,const u8 *propertyId,int length,const float *values) -> int {
    auto &s=*static_cast<ScriptBridge *>(c);
    if(!values || id>std::numeric_limits<ObjectId>::max() || !propertyId || length<=0) {
      s.lastStatus_=WorldStatus::InvalidArgument;return 0;
    }
    const float tuple[3]{values[0],values[1],values[2]};
    s.lastStatus_=s.world_->setTriple({s.world_->handle(static_cast<ObjectId>(id)),instance},viewOf(propertyId,length),tuple);
    return s.lastStatus_==WorldStatus::Ok;
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
    out->capabilities.armAsr=(u32)cap.armAsr;out->capabilities.fsr2=(u32)cap.fsr2;
    out->executedUpscaler=(u32)s.rendering_.executedUpscaler();out->executedStatus=(u32)s.rendering_.executedStatus();
    const auto &streaming=s.rendering_.textureStreaming();auto &ts=out->textureStreaming;
    ts.budgetBytes=streaming.budgetBytes;ts.totalBytes=streaming.totalBytes;ts.desiredBytes=streaming.desiredBytes;
    ts.targetBytes=streaming.targetBytes;ts.currentBytes=streaming.currentBytes;ts.nonStreamingBytes=streaming.nonStreamingBytes;
    ts.uploadedBytesLastFrame=streaming.uploadedBytesLastFrame;ts.active=streaming.active;ts.overBudget=streaming.overBudget;
    ts.streamingTextures=streaming.streamingTextures;ts.pendingLoads=streaming.pendingLoads;
    ts.budgetReducedTextures=streaming.budgetReducedTextures;ts.uploadsLastFrame=streaming.uploadsLastFrame;
    ts.failedUploads=streaming.failedUploads;
    const auto &frame=s.rendering_.sceneStatistics();auto &fs=out->frame;
    fs.frameIntervalMs=frame.frameIntervalMs;fs.gpuFrameMs=frame.gpuFrameMs;fs.renderWidth=frame.renderWidth;
    fs.renderHeight=frame.renderHeight;fs.drawCalls=frame.drawCalls;fs.lodDraws=frame.lodDraws;
    fs.lodReducedDraws=frame.lodReducedDraws;fs.triangles=frame.triangles;fs.lodBaseTriangles=frame.lodBaseTriangles;
    fs.lodSelectedTriangles=frame.lodSelectedTriangles;
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
    // A refused temporal upscaler names the device reason next to the clamp.
    const auto &cap=s.rendering_.capabilities();
    const auto requested=s.rendering_.requested().upscalingFilter;
    if(renderer::isTemporalUpscaler(requested)) {
      if(!text.empty()) text+='\n';
      text+=std::string("upscaler.")+renderer::upscalingFilterName(requested)+'='+
            renderer::temporalUpscalerAvailabilityName(renderer::temporalUpscalerAvailability(cap,requested));
    }
    if(s.rendering_.executedStatus()!=renderer::TemporalUpscalerAvailability::Available) {
      if(!text.empty()) text+='\n';
      text+=std::string("upscaler.executed=")+renderer::temporalUpscalerAvailabilityName(s.rendering_.executedStatus());
    }
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
  access_.objectLayer=[](void *c,u64 id,u32 world,u32 generation,int layer)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(!s.world_||!s.world_->running()){s.lastStatus_=WorldStatus::NotRunning;return -1;}
    if(id>std::numeric_limits<ObjectId>::max()||layer < -1||layer>=int(GameplayLayers::kCount)){
      s.lastStatus_=WorldStatus::InvalidArgument;return -1;
    }
    const ObjectHandle owner{world,(ObjectId)id,generation};
    s.lastStatus_=s.world_->validate(owner);if(s.lastStatus_!=WorldStatus::Ok)return -1;
    if(layer>=0){s.lastStatus_=s.world_->setLayer(owner,u32(layer));if(s.lastStatus_!=WorldStatus::Ok)return -1;}
    return int(s.world_->find(owner)->layer);
  };
  access_.fieldQuery=[](void *c,u64 id,u32 world,u32 generation,u64 instance,u32 op,const float *point,u32 layer,scene::ScriptFieldState *out)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(!s.world_||!s.physics_||!s.world_->running()){s.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(id>std::numeric_limits<ObjectId>::max()||!out||out->size!=sizeof(*out)||out->reserved){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    const ObjectHandle owner{world,(ObjectId)id,generation};s.lastStatus_=s.world_->validate(owner);if(s.lastStatus_!=WorldStatus::Ok)return 0;
    const auto *component=s.world_->find(owner)->components.findInstance(instance);
    const bool field2D=component&&scene::physicsField2DKind(*component)>=0;
    PhysicsFieldSample sample;s.lastStatus_=field2D?
      (s.physics2D_?s.physics2D_->fieldQuery(*s.world_,owner,instance,op,point,layer,sample):WorldStatus::NotRunning):
      s.physics_->fieldQuery(*s.world_,owner,instance,op,point,layer,sample);
    if(s.lastStatus_!=WorldStatus::Ok)return 0;
    static_assert(sizeof(sample)==sizeof(*out));*out={};out->flags=sample.flags;out->weight=sample.weight;std::copy_n(sample.acceleration,3,out->acceleration);std::copy_n(sample.windVelocity,3,out->windVelocity);out->windDrag=sample.windDrag;out->linearDrag=sample.linearDrag;out->angularDrag=sample.angularDrag;out->overrideWeight=sample.overrideWeight;out->affectedBodies=sample.affectedBodies;out->affectedMass=sample.affectedMass;return 1;
  };
  access_.bodyCommand=[](void *c,u64 id,u32 world,u32 generation,u64 instance,u32 op,const float *value,const float *point,scene::ScriptBodyState *out)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(!s.world_||!s.physics_||!s.world_->running()){s.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(id>std::numeric_limits<ObjectId>::max()||!out||out->size!=sizeof(*out)||out->reserved||!value||!point||(op>9&&(op<100||op>103))){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    for(u32 a=0;a<3;++a)if(!std::isfinite(value[a])||!std::isfinite(point[a])){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    AetherBodyStateV1 state;
    s.lastStatus_=s.physics_->bodyCommand(*s.world_,{world,(ObjectId)id,generation},instance,op,{value[0],value[1],value[2]},{point[0],point[1],point[2]},state);
    if(s.lastStatus_!=WorldStatus::Ok)return 0;
    *out={};out->flags=state.flags;auto copy=[](float *to,AetherVec3 from){to[0]=from.x;to[1]=from.y;to[2]=from.z;};copy(out->linear,state.linear);copy(out->angular,state.angular);copy(out->centerOfMass,state.centerOfMass);return 1;
  };
  access_.characterSnapshot=[](void *c,u64 id,scene::ScriptCharacterState *out)->int {
    auto&s=*static_cast<ScriptBridge*>(c);if(!s.world_||!s.physics_||!s.world_->running()){s.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(id>std::numeric_limits<ObjectId>::max()||!out||out->size!=sizeof(*out)||out->reserved||out->tailReserved){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    physics::CharacterMotor::RuntimeState state;s.lastStatus_=s.physics_->characterState(*s.world_,(ObjectId)id,state);if(s.lastStatus_!=WorldStatus::Ok)return 0;
    *out={};out->groundState=static_cast<u32>(state.groundState);out->flags=state.hasMeasuredStep?1:0;
    auto copy=[](float *to,AetherVec3 from){to[0]=from.x;to[1]=from.y;to[2]=from.z;};
    copy(out->position,state.position);copy(out->velocity,state.velocity);copy(out->motorVelocity,state.motorVelocity);copy(out->groundVelocity,state.groundVelocity);copy(out->groundNormal,state.groundNormal);return 1;
  };
  access_.characterJump=[](void *c,u64 id)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(id>std::numeric_limits<ObjectId>::max()) {s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    const auto handle=s.world_->handle((ObjectId)id);
    s.lastStatus_=s.world_->validate(handle);
    if(s.lastStatus_!=WorldStatus::Ok) return 0;
    const auto *object=s.world_->find(handle);
    if(!object->components.find(scene::Character::descriptor)) {s.lastStatus_=WorldStatus::ComponentMissing;return 0;}
    s.lastStatus_=s.physics_->jumpCharacter((ObjectId)id,s.world_)?WorldStatus::Ok:WorldStatus::Rejected;
    return s.lastStatus_==WorldStatus::Ok;
  };
  access_.cameraLook=[](void *c,u64 id,const float *delta)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(!delta||id>std::numeric_limits<ObjectId>::max()) {s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    s.lastStatus_=s.world_->applyCameraLook(s.world_->handle((ObjectId)id),delta[0],delta[1]);
    return s.lastStatus_==WorldStatus::Ok;
  };
  // --- v9: animação ---------------------------------------------------------
  // Objeto vivo, componente Animation dele e o avaliador ligado; senão o
  // motivo fica em lastStatus.
  static const auto animationTarget=[](ScriptBridge &s,u64 id,u64 instance)->bool {
    if(!s.animator_) {s.lastStatus_=WorldStatus::NotRunning;return false;}
    if(id>std::numeric_limits<ObjectId>::max()) {s.lastStatus_=WorldStatus::InvalidArgument;return false;}
    const auto handle=s.world_->handle((ObjectId)id);
    s.lastStatus_=s.world_->validate(handle);
    if(s.lastStatus_!=WorldStatus::Ok) return false;
    const auto *value=s.world_->find(handle)->components.findInstance(instance);
    if(!value||&value->type()!=&scene::Animation::descriptor) {s.lastStatus_=WorldStatus::ComponentMissing;return false;}
    return true;
  };
  static const auto animationStatus=[](AnimationCommandStatus status) {
    switch(status) {
    case AnimationCommandStatus::Ok: return WorldStatus::Ok;
    case AnimationCommandStatus::UnknownComponent: return WorldStatus::ComponentMissing;
    case AnimationCommandStatus::UnknownClip: return WorldStatus::UnknownResource;
    case AnimationCommandStatus::ClipNotInComponent: return WorldStatus::ClipNotInComponent;
    case AnimationCommandStatus::InvalidArgument: return WorldStatus::InvalidArgument;
    }
    return WorldStatus::Rejected;
  };
  access_.animationCommand=[](void *c,u64 id,u64 instance,const scene::ScriptAnimationCommand *command)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(!command||command->size!=sizeof(scene::ScriptAnimationCommand)||command->playMode>1) {
      s.lastStatus_=WorldStatus::InvalidArgument;return 0;
    }
    if(!animationTarget(s,id,instance)) return 0;
    const resources::AssetGuid clip{command->clip.high,command->clip.low};
    const auto mode=static_cast<AnimationPlayMode>(command->playMode);
    AnimationCommandStatus status=AnimationCommandStatus::InvalidArgument;
    switch(command->op) {
    case 0: status=s.animator_->play((ObjectId)id,instance,clip,mode);break;
    case 1: status=s.animator_->crossFade((ObjectId)id,instance,clip,command->seconds,mode);break;
    case 2: status=s.animator_->blend((ObjectId)id,instance,clip,command->targetWeight,command->seconds);break;
    case 3: status=s.animator_->stop((ObjectId)id,instance,clip);break;
    case 4: status=s.animator_->rewind((ObjectId)id,instance,clip);break;
    default: break;
    }
    s.lastStatus_=animationStatus(status);
    return s.lastStatus_==WorldStatus::Ok;
  };
  access_.getAnimationState=[](void *c,u64 id,u64 instance,scene::ScriptAssetGuid clip,scene::ScriptAnimationState *out)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(!out||out->size!=sizeof(scene::ScriptAnimationState)) {s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    if(!animationTarget(s,id,instance)) return 0;
    AnimationStateView view;
    s.lastStatus_=animationStatus(s.animator_->state((ObjectId)id,instance,{clip.high,clip.low},view));
    if(s.lastStatus_!=WorldStatus::Ok) return 0;
    out->enabled=view.enabled;out->clip={view.clip.high,view.clip.low};
    out->time=view.time;out->speed=view.speed;out->weight=view.weight;out->length=view.length;
    out->layer=view.layer;out->wrapMode=static_cast<u32>(view.wrapMode);
    return 1;
  };
  access_.setAnimationState=[](void *c,u64 id,u64 instance,const scene::ScriptAnimationState *value)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(!value||value->size!=sizeof(scene::ScriptAnimationState)||value->wrapMode>3) {
      s.lastStatus_=WorldStatus::InvalidArgument;return 0;
    }
    if(!animationTarget(s,id,instance)) return 0;
    AnimationStateView view;
    view.clip={value->clip.high,value->clip.low};view.enabled=value->enabled!=0;
    view.time=value->time;view.speed=value->speed;view.weight=value->weight;
    view.layer=value->layer;view.wrapMode=static_cast<resources::AnimationWrapMode>(value->wrapMode);
    s.lastStatus_=animationStatus(s.animator_->setState((ObjectId)id,instance,view));
    return s.lastStatus_==WorldStatus::Ok;
  };
  access_.animationClipAt=[](void *c,u64 id,u64 instance,u32 index,scene::ScriptAssetGuid *clip,u8 *name,int capacity)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(!animationTarget(s,id,instance)) return -1;
    const u32 count=s.animator_->clipCount((ObjectId)id,instance);
    if(index<count) {
      resources::AssetGuid guid;std::string text;
      if(!s.animator_->clipAt((ObjectId)id,instance,index,guid,text)) {s.lastStatus_=WorldStatus::Rejected;return -1;}
      if(clip) *clip={guid.high,guid.low};
      if(name&&capacity>0) {
        const auto length=std::min<usize>(text.size(),static_cast<usize>(capacity-1));
        std::memcpy(name,text.data(),length);name[length]=0;
      }
    }
    s.lastStatus_=WorldStatus::Ok;
    return static_cast<int>(count);
  };
  access_.resourceElementId=[](void *c,u64 id,u64 instance,const u8 *property,int length,u32 slot,u64 *out)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(!out||id>std::numeric_limits<ObjectId>::max()) {s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    s.lastStatus_=s.world_->resourceElementId({s.world_->handle((ObjectId)id),instance},viewOf(property,length),slot,*out);
    return s.lastStatus_==WorldStatus::Ok;
  };
  access_.getResourceByElementId=[](void *c,u64 id,u64 instance,const u8 *property,int length,u64 element,
                                     scene::ScriptAssetGuid *out)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(!out||id>std::numeric_limits<ObjectId>::max()) {s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    resources::AssetGuid value;
    s.lastStatus_=s.world_->getResourceByElementId({s.world_->handle((ObjectId)id),instance},viewOf(property,length),element,value);
    if(s.lastStatus_!=WorldStatus::Ok) return 0;
    *out={value.high,value.low};return 1;
  };
  access_.setResourceByElementId=[](void *c,u64 id,u64 instance,const u8 *property,int length,u64 element,
                                     scene::ScriptAssetGuid value)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(!s.assets_||id>std::numeric_limits<ObjectId>::max()) {s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    const std::span<const resources::EnvironmentProfile> profiles=s.environmentProfiles_?
        std::span<const resources::EnvironmentProfile>(*s.environmentProfiles_):std::span<const resources::EnvironmentProfile>{};
    s.lastStatus_=s.world_->setResourceByElementId({s.world_->handle((ObjectId)id),instance},viewOf(property,length),element,
        {value.high,value.low},*s.assets_,profiles,s.resourceAvailable_);
    return s.lastStatus_==WorldStatus::Ok;
  };
  access_.setParentWithPolicy = [](void *c, u64 id, u64 parent, u32 index, u32 policy) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    if (id > std::numeric_limits<ObjectId>::max() || parent > std::numeric_limits<ObjectId>::max() || policy > 1) {
      s.lastStatus_ = WorldStatus::InvalidArgument;
      return 0;
    }
    s.lastStatus_ = s.world_->setParent(s.world_->handle(static_cast<ObjectId>(id)),
                                        s.world_->handle(static_cast<ObjectId>(parent)), index,
                                        static_cast<ReparentPosePolicy>(policy));
    return s.lastStatus_ == WorldStatus::Ok;
  };
  access_.queueStructuralOperation = [](void *c, u32 kind, u64 id, u64 other,
                                         u32 index, u32 policy, u64 *operationId) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    if (!operationId || id > std::numeric_limits<ObjectId>::max() ||
        (kind == 1 && other > std::numeric_limits<ObjectId>::max()) || kind > 2 || policy > 1) {
      s.lastStatus_ = WorldStatus::InvalidArgument;
      return 0;
    }
    *operationId = 0;
    const auto object = s.world_->handle(static_cast<ObjectId>(id));
    if (kind == 0) s.lastStatus_ = s.world_->destroyObject(object, operationId);
    else if (kind == 1) s.lastStatus_ = s.world_->setParent(
        object, s.world_->handle(static_cast<ObjectId>(other)), index,
        static_cast<ReparentPosePolicy>(policy), operationId);
    else s.lastStatus_ = s.world_->removeComponent({object, other}, operationId);
    return s.lastStatus_ == WorldStatus::Ok;
  };
  access_.queryOperation = [](void *c, u32 world, u64 operationId, u32 *state, u32 *result) -> int {
    auto &s = *static_cast<ScriptBridge *>(c);
    if (!state || !result) { s.lastStatus_ = WorldStatus::InvalidArgument; return 0; }
    WorldOperationState operationState{};
    WorldStatus operationResult{};
    s.lastStatus_ = s.world_->operationResult(world, operationId, operationState, operationResult);
    if (s.lastStatus_ != WorldStatus::Ok) return 0;
    *state = static_cast<u32>(operationState);
    *result = static_cast<u32>(operationResult);
    return 1;
  };
  access_.appendAnimationClip=[](void *c,u64 id,u64 instance,scene::ScriptAssetGuid clip,u64 *element)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(!element||id>std::numeric_limits<ObjectId>::max()) {s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    s.lastStatus_=s.world_->appendAnimationClip({s.world_->handle((ObjectId)id),instance},
        {clip.high,clip.low},*element,s.resourceAvailable_);
    return s.lastStatus_==WorldStatus::Ok;
  };
  access_.removeAnimationClip=[](void *c,u64 id,u64 instance,u64 element)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(id>std::numeric_limits<ObjectId>::max()) {s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    s.lastStatus_=s.world_->removeAnimationClip({s.world_->handle((ObjectId)id),instance},element);
    return s.lastStatus_==WorldStatus::Ok;
  };
  access_.moveAnimationClip=[](void *c,u64 id,u64 instance,u64 element,u32 index)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(id>std::numeric_limits<ObjectId>::max()) {s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    s.lastStatus_=s.world_->moveAnimationClip({s.world_->handle((ObjectId)id),instance},element,index);
    return s.lastStatus_==WorldStatus::Ok;
  };
  access_.body2DCommand=[](void*c,u64 id,u32 world,u32 generation,u32 op,const float*input,float*out)->int {
    auto&s=*static_cast<ScriptBridge*>(c);
    if(!s.world_||!s.physics2D_){s.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(id>std::numeric_limits<ObjectId>::max()||op>6){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    s.lastStatus_=s.world_->validate({world,static_cast<ObjectId>(id),generation});
    if(s.lastStatus_!=WorldStatus::Ok)return 0;
    if(op==0&&!out){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    if(op!=0&&(!input||!std::isfinite(input[0])||!std::isfinite(input[1])||!std::isfinite(input[2]))){s.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    const auto object=static_cast<ObjectId>(id);bool ok=false;
    switch(op){case 0:ok=s.physics2D_->velocity(object,out,out[2]);break;
      case 1:ok=s.physics2D_->setVelocity(object,input,input[2]);break;
      case 2:ok=s.physics2D_->addForce(object,input);break;
      case 3:ok=s.physics2D_->addImpulse(object,input);break;
      case 4:ok=s.physics2D_->addTorque(object,input[0]);break;
      case 5:ok=s.physics2D_->addAngularImpulse(object,input[0]);break;
      case 6:ok=s.physics2D_->moveKinematic(object,input,input[2]);break;}
    s.lastStatus_=ok?WorldStatus::Ok:WorldStatus::InvalidArgument;return ok;
  };
  access_.pathPointCommand=[](void *context,u64 id,u32 worldId,u32 generation,u64 instance,u32 operation,u64 element,u32 index,const float *input,float *output,u64 *identity)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_){self.lastStatus_=WorldStatus::NotRunning;return -1;}
    if(id>std::numeric_limits<ObjectId>::max()||operation>10){self.lastStatus_=WorldStatus::InvalidArgument;return -1;}
    const ComponentHandle handle{{worldId,static_cast<ObjectId>(id),generation},instance};
    self.lastStatus_=self.world_->validate(handle.object);
    if(self.lastStatus_!=WorldStatus::Ok) return -1;
    const auto *value=self.world_->readComponent(handle);
    if(!value||value->type().id!=scene::Path::descriptor.id){self.lastStatus_=WorldStatus::ComponentMissing;return -1;}
    const auto &path=static_cast<const scene::Path &>(*value);
    if(operation==0){self.lastStatus_=WorldStatus::Ok;return static_cast<int>(path.curve.points.size());}
    if(operation==1||operation==2||operation==7||operation==8) {
      const auto *point=(operation==1||operation==7)?(index<path.curve.points.size()?&path.curve.points[index]:nullptr):path.point(element);
      if(!point){self.lastStatus_=WorldStatus::UnknownElement;return -1;}
      if(!output||!identity){self.lastStatus_=WorldStatus::InvalidArgument;return -1;}
      std::copy(point->position.begin(),point->position.end(),output);
      std::copy(point->in.begin(),point->in.end(),output+3);
      std::copy(point->out.begin(),point->out.end(),output+6);
      if(operation==7||operation==8)output[9]=point->rollDegrees;
      *identity=point->id;self.lastStatus_=WorldStatus::Ok;return 1;
    }
    if((operation==3||operation==9)&&!identity){self.lastStatus_=WorldStatus::InvalidArgument;return -1;}
    u64 allocated=0;
    self.lastStatus_=self.world_->editPathPoint(handle,operation,element,index,input,allocated);
    if(self.lastStatus_!=WorldStatus::Ok) return -1;
    if(identity)*identity=allocated;
    return 1;
  };
  access_.pathRuntimeCommand=[](void *context,u64 id,u32 worldId,u32 generation,u64 instance,u32 operation,double distance,u32 wrap,float *output,double *scalar)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_||!self.paths_){self.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(id>std::numeric_limits<ObjectId>::max()||operation>5||wrap>1||!std::isfinite(distance)){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    const ComponentHandle handle{{worldId,static_cast<ObjectId>(id),generation},instance};
    self.lastStatus_=self.world_->validate(handle.object);
    if(self.lastStatus_!=WorldStatus::Ok) return 0;
    const auto *value=self.world_->readComponent(handle);
    const auto expected=(operation==0||operation==5)?scene::Path::descriptor.id:scene::PathFollow::descriptor.id;
    if(!value||value->type().id!=expected){self.lastStatus_=WorldStatus::ComponentMissing;return 0;}
    bool ok=false;
    if(operation==0) {
      if(!output||!scalar){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
      std::array<float,3> position{},tangent{};
      ok=self.paths_->sample(*self.world_,handle.object.id,distance,position,tangent,wrap!=0)&&self.paths_->length(*self.world_,handle.object.id,*scalar);
      if(ok){std::copy(position.begin(),position.end(),output);std::copy(tangent.begin(),tangent.end(),output+3);}
    } else if(operation==5){
      if(!output||!scalar){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
      resources::BakedCurve3D::Frame frame;
      ok=self.paths_->sampleFrame(*self.world_,handle.object.id,distance,frame,wrap!=0)&&self.paths_->length(*self.world_,handle.object.id,*scalar);
      if(ok){std::copy(frame.position.begin(),frame.position.end(),output);std::copy(frame.tangent.begin(),frame.tangent.end(),output+3);std::copy(frame.up.begin(),frame.up.end(),output+6);output[9]=frame.rollDegrees;}
    } else if(operation==1) ok=self.paths_->restart(*self.world_,handle.object.id);
    else if(operation==2) ok=self.paths_->stop(*self.world_,handle.object.id);
    else {
      if(!scalar){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
      if(operation==3) ok=self.paths_->progress(*self.world_,handle.object.id,*scalar);
      else {bool playing=false;ok=self.paths_->playing(*self.world_,handle.object.id,playing);if(ok)*scalar=playing?1:0;}
    }
    self.lastStatus_=ok?WorldStatus::Ok:WorldStatus::Rejected;
    return ok;
  };
  access_.audioCommand=[](void *context,u64 id,u32 worldId,u32 generation,u64 instance,u32 operation,double seconds)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_||!self.audio_){self.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(id>std::numeric_limits<ObjectId>::max()){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    const ComponentHandle handle{{worldId,static_cast<ObjectId>(id),generation},instance};
    self.lastStatus_=self.audio_->command(*self.world_,handle,static_cast<SceneAudio::Command>(operation),seconds);
    return self.lastStatus_==WorldStatus::Ok;
  };
  access_.guiCommand=[](void *context,u32 worldId,u32 id,u32 op,const u8 *text,int length,float value,scene::ScriptGuiState *out)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_ || !self.gui_){self.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(!out || op>14 || length<0 || length>4096 || (length && !text) || !std::isfinite(value)) {
      self.lastStatus_=WorldStatus::InvalidArgument;return 0;
    }
    if(op!=0 && worldId!=self.world_->worldId()){self.lastStatus_=WorldStatus::ForeignWorld;return 0;}
    const std::string_view string=length?std::string_view(reinterpret_cast<const char*>(text),static_cast<usize>(length)):std::string_view{};
    if(string.find('\0')!=std::string_view::npos){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    ui::GuiEvent event{};
    if(op==0) id=self.gui_->document().findByName(string);
    if(op==7) {
      if(out->kind>=ui::kGuiKindCount) {self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
      auto &document=self.gui_->document();const auto before=document;
      id=document.create(static_cast<ui::GuiKind>(out->kind),id);
      if(!id){self.lastStatus_=WorldStatus::Rejected;return 0;}
      if(!string.empty()) {
        auto created=*document.find(id);created.name=string;std::string error;
        if(!document.update(created,error)){document=before;self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
      }
    }
    if(op==6) {
      if(!self.gui_->poll(event)){self.lastStatus_=WorldStatus::Ok;*out={};return 0;}
      id=event.node;
    }
    const auto *node=self.gui_->document().find(id);
    if(!node){self.lastStatus_=WorldStatus::UnknownElement;return 0;}
    if(op==8) {self.gui_->document().remove(id);*out={};self.lastStatus_=WorldStatus::Ok;return 1;}
    bool ok=true;
    if(op==2) ok=self.gui_->setText(id,std::string(string));
    if(op==3) ok=self.gui_->setValue(id,value);
    if(op==4) ok=self.gui_->setVisible(id,value!=0);
    if(op==5) ok=self.gui_->setEnabled(id,value!=0);
    if(op==10) {auto edit=*node;edit.image=string;std::string error;ok=node->kind==ui::GuiKind::Image && self.gui_->document().update(edit,error);}
    if(op==11 || op==12)ok=self.gui_->document().reorder(id,op==11?-1:1);
    if(op==13)ok=self.gui_->playAnimation(id);
    if(op==14)ok=self.gui_->stopAnimation(id);
    if(op==9) {auto edit=*node;edit.name=string;std::string error;ok=self.gui_->document().update(edit,error);}
    if(!ok){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    node=self.gui_->document().find(id);
    *out={self.world_->worldId(),id,static_cast<u32>(node->kind),node->visible?1u:0u,node->enabled?1u:0u,
          node->value,node->minimum,node->maximum,op==6?static_cast<u32>(event.kind)+1:0};
    if(op==6) out->value=event.value;
    self.lastStatus_=WorldStatus::Ok;return 1;
  };
  access_.guiProperties=[](void *context,u32 world,u32 id,u32 operation,scene::ScriptGuiProperties *out)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_ || !self.gui_) {self.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(world!=self.world_->worldId()) {self.lastStatus_=WorldStatus::ForeignWorld;return 0;}
    if(!out || operation>1) {self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    const auto *node=self.gui_->document().find(id);
    if(!node) {self.lastStatus_=WorldStatus::UnknownElement;return 0;}
    if(operation==1) {
      auto edit=*node;
      edit.anchorMin={out->anchors[0],out->anchors[1]};edit.anchorMax={out->anchors[2],out->anchors[3]};
      edit.offsets={out->offsets[0],out->offsets[1],out->offsets[2],out->offsets[3]};
      edit.background=out->background;edit.foreground=out->foreground;edit.accent=out->accent;
      edit.fontSize=out->fontSize;edit.radius=out->radius;edit.clipChildren=out->clipChildren!=0;
      std::string error;
      if(out->clipChildren>1 || !self.gui_->document().update(edit,error)) {self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
      node=self.gui_->document().find(id);
    }
    *out={{node->anchorMin.x,node->anchorMin.y,node->anchorMax.x,node->anchorMax.y},
          {node->offsets.x,node->offsets.y,node->offsets.width,node->offsets.height},
          node->background,node->foreground,node->accent,node->clipChildren?1u:0u,node->fontSize,node->radius};
    self.lastStatus_=WorldStatus::Ok;return 1;
  };
  access_.guiText=[](void *context,u32 world,u32 id,u32 field,u8 *buffer,int capacity)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_ || !self.gui_) {self.lastStatus_=WorldStatus::NotRunning;return -1;}
    if(world!=self.world_->worldId()) {self.lastStatus_=WorldStatus::ForeignWorld;return -1;}
    if(field>3 || capacity<0 || (capacity && !buffer)) {self.lastStatus_=WorldStatus::InvalidArgument;return -1;}
    if(field==3) {
      const auto &text=self.gui_->diagnostic();
      if(capacity>=static_cast<int>(text.size()) && !text.empty())std::memcpy(buffer,text.data(),text.size());
      self.lastStatus_=WorldStatus::Ok;return static_cast<int>(text.size());
    }
    const auto *node=self.gui_->document().find(id);
    if(!node) {self.lastStatus_=WorldStatus::UnknownElement;return -1;}
    const auto &text=field==0?node->text:field==1?node->name:node->image;
    if(capacity>=static_cast<int>(text.size()) && !text.empty()) std::memcpy(buffer,text.data(),text.size());
    self.lastStatus_=WorldStatus::Ok;return static_cast<int>(text.size());
  };
  access_.guiSizing=[](void *context,u32 world,u32 id,u32 op,scene::ScriptGuiSizing *out)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_ || !self.gui_){self.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(world!=self.world_->worldId()){self.lastStatus_=WorldStatus::ForeignWorld;return 0;}
    if(!out || op>1){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    const auto *node=self.gui_->document().find(id);if(!node){self.lastStatus_=WorldStatus::UnknownElement;return 0;}
    if(op==1) {
      auto edit=*node;auto &z=edit.sizing;
      z.minimum={out->minimum[0],out->minimum[1]};z.preferred={out->preferred[0],out->preferred[1]};z.flexible={out->flexible[0],out->flexible[1]};
      z.padding={out->padding[0],out->padding[1],out->padding[2],out->padding[3]};z.spacing={out->spacing[0],out->spacing[1]};
      z.alignment=static_cast<ui::GuiAlignment>(out->alignment);z.columns=out->columns;z.ignore=out->ignore!=0;edit.imageFit=static_cast<ui::GuiImageFit>(out->imageFit);edit.imageTint=out->imageTint;
      std::string error;if(out->alignment>3 || out->imageFit>2 || out->ignore>1 || !self.gui_->document().update(edit,error)){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}node=self.gui_->document().find(id);
    }
    const auto &z=node->sizing;
    *out={{z.minimum.x,z.minimum.y},{z.preferred.x,z.preferred.y},{z.flexible.x,z.flexible.y},{z.padding.left,z.padding.top,z.padding.right,z.padding.bottom},{z.spacing.x,z.spacing.y},static_cast<u32>(z.alignment),z.columns,z.ignore?1u:0u,static_cast<u32>(node->imageFit),node->imageTint};
    self.lastStatus_=WorldStatus::Ok;return 1;
  };
  access_.guiBehavior=[](void *context,u32 world,u32 id,u32 op,scene::ScriptGuiBehavior *out)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_ || !self.gui_){self.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(world!=self.world_->worldId()){self.lastStatus_=WorldStatus::ForeignWorld;return 0;}
    if(!out || op>1){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    const auto *node=self.gui_->document().find(id);if(!node){self.lastStatus_=WorldStatus::UnknownElement;return 0;}
    if(op==1) {
      if(out->clickable>1 || out->action>5 || out->enabled>1 || out->autoPlay>1 || out->loop>1 || out->pingPong>1 || out->easing>3){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
      auto edit=*node;edit.interaction={out->clickable!=0,static_cast<ui::GuiClickAction>(out->action),out->target,out->value};
      edit.motion={out->enabled!=0,out->autoPlay!=0,out->loop!=0,out->pingPong!=0,static_cast<ui::GuiEasing>(out->easing),out->duration,out->delay,
                   {out->from[0],out->from[1],out->from[2],out->from[3]},{out->to[0],out->to[1],out->to[2],out->to[3]}};
      std::string error;if(!self.gui_->document().update(edit,error)){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
      node=self.gui_->document().find(id);
    }
    const auto &a=node->interaction;const auto &m=node->motion;
    *out={a.clickable?1u:0u,static_cast<u32>(a.action),a.target,a.value,m.enabled?1u:0u,m.autoPlay?1u:0u,m.loop?1u:0u,m.pingPong?1u:0u,static_cast<u32>(m.easing),m.duration,m.delay,
          {m.from.x,m.from.y,m.from.scale,m.from.opacity},{m.to.x,m.to.y,m.to.scale,m.to.opacity}};
    self.lastStatus_=WorldStatus::Ok;return 1;
  };
  access_.guiAction=[](void *context,u32 world,u32 id,u32 op,u32 index,scene::ScriptGuiAction *out)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_ || !self.gui_){self.lastStatus_=WorldStatus::NotRunning;return -1;}
    if(world!=self.world_->worldId()){self.lastStatus_=WorldStatus::ForeignWorld;return -1;}
    const auto *node=self.gui_->document().find(id);if(!node){self.lastStatus_=WorldStatus::UnknownElement;return -1;}
    if(op>5 || (op && !out) || (op!=0 && op!=2 && index>=node->actions.size()) || (op==2 && node->actions.size()>=ui::GuiDocument::kMaximumActions)) {self.lastStatus_=WorldStatus::InvalidArgument;return -1;}
    if(op==0){self.lastStatus_=WorldStatus::Ok;return static_cast<int>(node->actions.size());}
    if(op==1){const auto &a=node->actions[index];*out={static_cast<u32>(a.event)+1,static_cast<u32>(a.action),a.target,a.value};self.lastStatus_=WorldStatus::Ok;return 1;}
    auto edit=*node;
    if(op==2 || op==3) {
      if(out->event<1 || out->event>2 || out->action>5 || !std::isfinite(out->value)){self.lastStatus_=WorldStatus::InvalidArgument;return -1;}
      ui::GuiActionBinding a{static_cast<ui::GuiEventKind>(out->event-1),static_cast<ui::GuiClickAction>(out->action),out->target,out->value};
      if(op==2)edit.actions.push_back(a);else edit.actions[index]=a;
    } else if(op==4)edit.actions.erase(edit.actions.begin()+index);
    else {
      if(out->target>=edit.actions.size()){self.lastStatus_=WorldStatus::InvalidArgument;return -1;}
      const auto a=edit.actions[index];edit.actions.erase(edit.actions.begin()+index);edit.actions.insert(edit.actions.begin()+out->target,a);
    }
    std::string error;if(!self.gui_->document().update(edit,error)){self.lastStatus_=WorldStatus::InvalidArgument;return -1;}
    self.lastStatus_=WorldStatus::Ok;return 1;
  };
  access_.guiTransitions=[](void *context,u32 world,u32 id,u32 op,scene::ScriptGuiTransitions *out)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_ || !self.gui_){self.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(world!=self.world_->worldId()){self.lastStatus_=WorldStatus::ForeignWorld;return 0;}
    const auto *node=self.gui_->document().find(id);if(!node){self.lastStatus_=WorldStatus::UnknownElement;return 0;}
    if(!out || op>1 || (op && (out->enabled>1 || out->easing>3))){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    if(op) {
      auto edit=*node;auto &t=edit.transitions;t.enabled=out->enabled!=0;t.easing=static_cast<ui::GuiEasing>(out->easing);t.duration=out->duration;
      u32 i=0;for(auto *v:{&t.normal,&t.pressed,&t.disabled}){const auto *p=&out->poses[4*i];v->pose={p[0],p[1],p[2],p[3]};v->tint=out->tints[i++];}
      std::string error;if(!self.gui_->document().update(edit,error)){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}node=self.gui_->document().find(id);
    }
    const auto &t=node->transitions;out->enabled=t.enabled?1u:0u;out->easing=static_cast<u32>(t.easing);out->duration=t.duration;
    u32 i=0;for(const auto &v:{t.normal,t.pressed,t.disabled}){auto *p=&out->poses[4*i];p[0]=v.pose.x;p[1]=v.pose.y;p[2]=v.pose.scale;p[3]=v.pose.opacity;out->tints[i++]=v.tint;}
    self.lastStatus_=WorldStatus::Ok;return 1;
  };
  access_.guiCanvas=[](void *context,u32 world,u32 op,scene::ScriptGuiCanvas *out)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_ || !self.gui_){self.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(world!=self.world_->worldId()){self.lastStatus_=WorldStatus::ForeignWorld;return 0;}
    if(!out || op>1){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    if(op==1) {
      ui::GuiCanvas c;c.mode=static_cast<ui::GuiCanvasMode>(out->mode);c.resolution={out->resolution[0],out->resolution[1]};std::copy_n(out->position,3,c.position);std::copy_n(out->rotation,3,c.rotation);c.unitsPerPixel=out->unitsPerPixel;c.occlusion=out->occlusion!=0;
      std::string error;if(out->mode>1 || out->occlusion>1 || !self.gui_->document().setCanvas(c,error)){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
      self.gui_->cancelPointers();
    }
    const auto &c=self.gui_->document().canvas();
    *out={static_cast<u32>(c.mode),{c.resolution.x,c.resolution.y},{c.position[0],c.position[1],c.position[2]},{c.rotation[0],c.rotation[1],c.rotation[2]},c.unitsPerPixel,c.occlusion?1u:0u};self.lastStatus_=WorldStatus::Ok;return 1;
  };
  access_.guiInstance=[](void *context,u32 world,u64 object,u64 component)->u64 {
    auto &self=*static_cast<ScriptBridge*>(context);
    if(!self.world_||!self.sceneGui_){self.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(world!=self.world_->worldId()){self.lastStatus_=WorldStatus::ForeignWorld;return 0;}
    if(object>std::numeric_limits<ObjectId>::max()){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    self.lastStatus_=self.world_->validate(self.world_->handle(static_cast<ObjectId>(object)));
    if(self.lastStatus_!=WorldStatus::Ok)return 0;
    const auto id=self.sceneGui_->instanceFor(*self.world_,static_cast<ObjectId>(object),component);
    self.lastStatus_=id?WorldStatus::Ok:WorldStatus::UnknownElement;return id;
  };
  access_.guiInstanceRequest=[](void *context,u32 world,u64 instance,u32 request,u32 node,u32 operation,u32 index,void *payload,u32 size,u8 *text,int length,float value)->int {
    auto &self=*static_cast<ScriptBridge*>(context);
    const int failure=request==2||request==6||request==9?-1:0;
    if(!self.world_||(!self.sceneGui_&&instance)){self.lastStatus_=WorldStatus::NotRunning;return failure;}
    if(world!=self.world_->worldId()){self.lastStatus_=WorldStatus::ForeignWorld;return failure;}
    auto *runtime=self.gui_;
    if(instance){self.sceneGui_->reconcile(*self.world_);auto *entry=self.sceneGui_->find(*self.world_,instance);runtime=entry?&entry->runtime:nullptr;}
    if(!runtime){self.lastStatus_=WorldStatus::StaleHandle;return failure;}
    const u32 sizes[]{sizeof(scene::ScriptGuiState),sizeof(scene::ScriptGuiProperties),0,sizeof(scene::ScriptGuiSizing),sizeof(scene::ScriptGuiCanvas),sizeof(scene::ScriptGuiBehavior),sizeof(scene::ScriptGuiAction),sizeof(scene::ScriptGuiTransitions),sizeof(scene::ScriptGuiControl),0,sizeof(float)*2,sizeof(u32),sizeof(scene::ScriptInputActionState)};
    if(request>=std::size(sizes)||size!=sizes[request]||(size&&!payload)||self.guiRequestActive_){self.lastStatus_=WorldStatus::InvalidArgument;return failure;}
    // Single-owner-thread synchronous dispatch. Reuse the exact validators and
    // operations of the legacy ABI; the selected runtime is restored on all exits.
    // These UI operations do not invoke managed callbacks or mutate the scene.
    struct Scope {ScriptBridge &s;ui::GuiRuntime *before;Scope(ScriptBridge &b,ui::GuiRuntime *r):s(b),before(b.gui_){s.gui_=r;s.guiRequestActive_=true;}~Scope(){s.gui_=before;s.guiRequestActive_=false;}} scope(self,runtime);
    if(request>=10) {
      if(length<=0||length>48||!text||!self.input_){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
      const auto name=viewOf(text,length);const InputService *input=self.input_;std::unique_ptr<InputService> neutral;
      if(instance) {
        const auto *entry=self.sceneGui_->find(*self.world_,instance);const auto receiver=entry->config.inputReceiver;
        if(receiver) {
          if(receiver>std::numeric_limits<ObjectId>::max()){self.lastStatus_=WorldStatus::StaleHandle;return 0;}
          const auto handle=self.world_->handle(static_cast<ObjectId>(receiver));const auto *entity=self.world_->find(handle);
          if(!entity||!self.world_->activeInHierarchy(handle)||!scene::uiInputReceiverAccepts(entity->components)){self.lastStatus_=WorldStatus::StaleHandle;return 0;}
          input=self.sceneGui_->inputFor(handle.id);if(!input){neutral=std::make_unique<InputService>();neutral->copyPolicyFrom(*self.input_);input=neutral.get();}
        }
      }
      const auto *action=input->map().find(name);if(!action){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
      if(request==10){if(operation){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}input->axis2(name,static_cast<float*>(payload));}
      else if(request==11){if(operation>2||action->kind!=ActionKind::Button){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}*static_cast<u32*>(payload)=operation==0?input->pressed(name):operation==1?input->justPressed(name):input->justReleased(name);}
      else {
        auto &out=*static_cast<scene::ScriptInputActionState*>(payload);if(operation||out.size!=sizeof(out)){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
        out.flags=(input->actionEnabled(name)?1u:0u)|(action->enabled?2u:0u);out.interaction=static_cast<u32>(action->interaction);out.deviceGroups=action->deviceGroups;out.phase=static_cast<u32>(input->phase(name));out.duration=action->duration;out.progress=input->progress(name);out.elapsed=input->elapsed(name);
      }
      self.lastStatus_=WorldStatus::Ok;return 1;
    }
    if(request==8||request==9) {
      const auto *stored=runtime->document().find(node);
      if(!stored||!ui::guiInputKind(stored->kind)){self.lastStatus_=WorldStatus::InvalidArgument;return failure;}
      auto edit=*stored;auto &c=edit.control;std::string error;
      if(operation>1){self.lastStatus_=WorldStatus::InvalidArgument;return failure;}
      if(request==9) {
        if(index>2||length<0||length>1024||(length&&!text)){self.lastStatus_=WorldStatus::InvalidArgument;return failure;}
        auto &field=index==0?c.action:index==1?c.baseImage:c.knobImage;
        if(operation==1){field.assign(reinterpret_cast<const char*>(text?text:reinterpret_cast<const u8*>("")),static_cast<usize>(length));if(!runtime->document().update(edit,error)){self.lastStatus_=WorldStatus::InvalidArgument;return -1;}}
        else if(text){if(length<static_cast<int>(field.size())){self.lastStatus_=WorldStatus::InvalidArgument;return -1;}std::memcpy(text,field.data(),field.size());}
        self.lastStatus_=WorldStatus::Ok;return static_cast<int>(field.size());
      }
      auto &out=*static_cast<scene::ScriptGuiControl*>(payload);
      if(operation==1) {
        if(out.mode>2||out.axis>2||out.gate>1||out.showBase>1||out.showKnob>1){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
        c.mode=static_cast<ui::GuiStickMode>(out.mode);c.axis=static_cast<ui::GuiStickAxis>(out.axis);c.gate=static_cast<ui::GuiStickGate>(out.gate);c.showBase=out.showBase!=0;c.showKnob=out.showKnob!=0;
        c.inputRadius=out.inputRadius;c.baseRadius=out.baseRadius;c.knobRadius=out.knobRadius;c.deadzone=out.deadzone;c.outerDeadzone=out.outerDeadzone;c.exponent=out.exponent;c.sensitivity=out.sensitivity;c.returnSeconds=out.returnSeconds;
        if(!runtime->document().update(edit,error)){self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
      }
      runtime->layout(runtime->viewport());
      out={static_cast<u32>(c.mode),static_cast<u32>(c.axis),static_cast<u32>(c.gate),c.showBase?1u:0u,c.showKnob?1u:0u,0,0,0,c.inputRadius,c.baseRadius,c.knobRadius,c.deadzone,c.outerDeadzone,c.exponent,c.sensitivity,c.returnSeconds,0,0};
      if(const auto *state=runtime->controlState(node)){out.pressed=state->down;out.pointer=state->pointer;out.device=static_cast<u32>(state->device);out.x=state->value.x;out.y=state->value.y;}
      self.lastStatus_=WorldStatus::Ok;return 1;
    }
    switch(request) {
      case 0:return self.access_.guiCommand(context,world,node,operation,text,length,value,static_cast<scene::ScriptGuiState*>(payload));
      case 1:return self.access_.guiProperties(context,world,node,operation,static_cast<scene::ScriptGuiProperties*>(payload));
      case 2:return self.access_.guiText(context,world,node,operation,text,length);
      case 3:return self.access_.guiSizing(context,world,node,operation,static_cast<scene::ScriptGuiSizing*>(payload));
      case 4:return self.access_.guiCanvas(context,world,operation,static_cast<scene::ScriptGuiCanvas*>(payload));
      case 5:return self.access_.guiBehavior(context,world,node,operation,static_cast<scene::ScriptGuiBehavior*>(payload));
      case 6:return self.access_.guiAction(context,world,node,operation,index,static_cast<scene::ScriptGuiAction*>(payload));
      case 7:return self.access_.guiTransitions(context,world,node,operation,static_cast<scene::ScriptGuiTransitions*>(payload));
    }
    return failure;
  };
  access_.audioSnapshot=[](void *context,u64 id,u32 worldId,u32 generation,u64 instance,scene::ScriptAudioSnapshot *output)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_||!self.audio_){self.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(!output||output->size!=sizeof(*output)||output->reserved||id>std::numeric_limits<ObjectId>::max()) {
      self.lastStatus_=WorldStatus::InvalidArgument;return 0;
    }
    const ComponentHandle handle{{worldId,static_cast<ObjectId>(id),generation},instance};
    self.lastStatus_=self.world_->validate(handle.object);
    if(self.lastStatus_!=WorldStatus::Ok) return 0;
    if(self.world_->componentTypeId(handle)!="astra.audio.source") {
      self.lastStatus_=WorldStatus::ComponentMissing;return 0;
    }
    const auto *observed=self.audio_->diagnostic(handle.object.id,instance);
    if(!observed){self.lastStatus_=WorldStatus::NotRunning;return 0;}
    output->state=static_cast<u32>(observed->state);
    output->cursor=observed->cursor;
    output->outputRunning=self.audio_->deviceRunning()?1:0;
    self.lastStatus_=WorldStatus::Ok;
    return 1;
  };
  access_.groupMembership=[](void *c,u64 id,const u8 *text,int length,int operation)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(id>std::numeric_limits<ObjectId>::max() || !text || length<=0 ||
       length>static_cast<int>(ObjectTags::MaximumNameBytes) || operation<-1 || operation>1) {
      s.lastStatus_=WorldStatus::InvalidArgument;return -1;
    }
    const auto handle=s.world_->handle(static_cast<ObjectId>(id));
    if(operation==-1) {
      bool member=false;s.lastStatus_=s.world_->isInGroup(handle,viewOf(text,length),member);
      return s.lastStatus_==WorldStatus::Ok?static_cast<int>(member):-1;
    }
    s.lastStatus_=s.world_->setGroupMembership(handle,viewOf(text,length),operation==1);
    return s.lastStatus_==WorldStatus::Ok?1:-1;
  };
  access_.findGroup=[](void *c,const u8 *text,int length,u64 *out,int capacity,int includeInactive)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(!text || length<=0 || length>static_cast<int>(ObjectTags::MaximumNameBytes) || capacity<0 ||
       capacity>static_cast<int>(SceneGraph::kMaximumObjects) || (!out && capacity) || (includeInactive!=0 && includeInactive!=1)) {
      s.lastStatus_=WorldStatus::InvalidArgument;return -1;
    }
    u32 count=0;s.lastStatus_=s.world_->findGroup(viewOf(text,length),{out,static_cast<usize>(capacity)},count,includeInactive!=0);
    return s.lastStatus_==WorldStatus::Ok?static_cast<int>(count):-1;
  };
  access_.groupAt=[](void *c,u64 id,u32 slot,u8 *out,int capacity)->int {
    auto &s=*static_cast<ScriptBridge*>(c);
    if(id>std::numeric_limits<ObjectId>::max() || capacity<0 || (!out && capacity)) {
      s.lastStatus_=WorldStatus::InvalidArgument;return -1;
    }
    const auto handle=s.world_->handle(static_cast<ObjectId>(id));s.lastStatus_=s.world_->validate(handle);
    if(s.lastStatus_!=WorldStatus::Ok) return -1;
    const auto &names=s.world_->find(handle)->groups.names();
    if(slot==std::numeric_limits<u32>::max() && !out && !capacity) return static_cast<int>(names.size());
    if(slot>=names.size()) {s.lastStatus_=WorldStatus::InvalidArgument;return -1;}
    const auto &name=names[slot];
    if(out && capacity>=static_cast<int>(name.size())) std::memcpy(out,name.data(),name.size());
    return static_cast<int>(name.size());
  };
  access_.timeSnapshot=[](void *context,u32 worldId,scene::ScriptTimeState *output)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_ || !self.world_->running()) {self.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(worldId!=self.world_->worldId()) {self.lastStatus_=WorldStatus::ForeignWorld;return 0;}
    if(!output || output->size!=sizeof(*output) || output->reserved) {self.lastStatus_=WorldStatus::InvalidArgument;return 0;}
    const auto &clock=self.world_->clock();
    output->frameCount=clock.frameCount();output->simulationTime=self.world_->elapsedSeconds();
    output->unscaledTime=clock.unscaledTime();output->delta=clock.delta();output->unscaledDelta=clock.unscaledDelta();
    output->timeScale=clock.scale();output->frameScale=clock.frameScale();
    self.lastStatus_=WorldStatus::Ok;return 1;
  };
  access_.setTimeScale=[](void *context,u32 worldId,float value)->int {
    auto &self=*static_cast<ScriptBridge *>(context);
    if(!self.world_ || !self.world_->running()) {self.lastStatus_=WorldStatus::NotRunning;return 0;}
    if(worldId!=self.world_->worldId()) {self.lastStatus_=WorldStatus::ForeignWorld;return 0;}
    self.lastStatus_=self.world_->setTimeScale(value);return self.lastStatus_==WorldStatus::Ok;
  };
  access_.query2D=[](void*c,u32 world,u32 kind,const float*origin,const float*translation,float radius,const scene::ScriptQueryFilter*filter,scene::ScriptQueryHit*out,int capacity)->int {
    auto&s=*static_cast<ScriptBridge*>(c);
    if(!s.world_||!s.physics2D_){s.lastStatus_=WorldStatus::NotRunning;return -1;}
    if(world!=s.world_->worldId()){s.lastStatus_=WorldStatus::ForeignWorld;return -1;}
    if(kind>1||!origin||!filter||filter->size!=sizeof(*filter)||filter->reserved||filter->flags>7||capacity<0||capacity>4096||(capacity&&!out)||filter->ignore>std::numeric_limits<ObjectId>::max()||!std::isfinite(origin[0])||!std::isfinite(origin[1])||!std::isfinite(radius)||radius<0||(kind==1&&radius<=0)||(kind==0&&(!translation||!std::isfinite(translation[0])||!std::isfinite(translation[1])))){s.lastStatus_=WorldStatus::InvalidArgument;return -1;}
    Physics2DFilter f;f.layerMask=filter->gameplayLayerMask;f.includeStatic=filter->flags&1;f.includeDynamic=filter->flags&2;f.includeSensors=filter->flags&4;f.ignore=static_cast<ObjectId>(filter->ignore);
    std::vector<Physics2DHit>hits(static_cast<usize>(capacity));
    const auto count=kind==0?s.physics2D_->rayCastAll(origin,translation,f,hits.data(),static_cast<u32>(capacity)):s.physics2D_->overlapCircle(origin,radius,f,hits.data(),static_cast<u32>(capacity));
    for(u32 i=0;i<std::min(count,static_cast<u32>(capacity));++i){scene::ScriptQueryHit hit{};const auto&raw=hits[i];hit.object=raw.object;hit.colliderObject=raw.object;hit.collider=raw.colliderInstance;hit.point[0]=raw.point[0];hit.point[1]=raw.point[1];hit.normal[0]=raw.normal[0];hit.normal[1]=raw.normal[1];hit.fraction=raw.fraction;hit.distance=kind==0?std::hypot(translation[0],translation[1])*raw.fraction:0;hit.flags=(kind==0?1u:0u)|(raw.sensor?2u:0u);out[i]=hit;}
    s.lastStatus_=WorldStatus::Ok;return static_cast<int>(count);
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
  if(events_) events_->attach(ComponentEventQueue::Consumer::Scripts,true);
  const auto data = attachments(world.graph());
  if (api_.start(reinterpret_cast<const u8 *>(root_.data()), static_cast<int>(root_.size()),
      reinterpret_cast<const u8 *>(data.data()), static_cast<int>(data.size()), &access_) != 0) {
    collectDiagnostics();
    if(events_) events_->attach(ComponentEventQueue::Consumer::Scripts,false);
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

bool ScriptBridge::lateUpdate(float elapsed) {
  if (!running_) return true;
  const bool ok = api_.lateUpdate(elapsed) == 0;
  collectDiagnostics();
  return ok;
}

bool ScriptBridge::lifecycle(scene::ScriptLifecycleEvent event, bool value) {
  if (!running_) return true;
  const bool ok = api_.lifecycle(static_cast<u32>(event), value ? 1u : 0u) == 0;
  collectDiagnostics();
  return ok;
}

bool ScriptBridge::editBehavior(ObjectId object, u64 instance, bool enabled,
                                std::span<const scene::ScriptPropertyValue> changed) {
  if (!running_) { diagnostics_ = "Play parado: não há instância para editar"; return false; }
  if (!api_.edit) { diagnostics_ = "O runtime C# carregado não aceita edição de campos em Play"; return false; }
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(std::numeric_limits<float>::max_digits10)
      << "{\"Enabled\":" << (enabled ? "true" : "false") << ",\"Properties\":{";
  bool first = true;
  for (const auto &p : changed) {
    if (!first) out << ',';
    first = false;
    jsonString(out, p.id);
    out << ':';
    writeScriptValue(out, p);
  }
  out << "}}";
  const std::string json = out.str();
  const bool ok = api_.edit(object, instance, reinterpret_cast<const u8 *>(json.data()), static_cast<int>(json.size())) == 0;
  collectDiagnostics();
  return ok;
}

bool ScriptBridge::inspectFields(ObjectId object,std::vector<ScriptFieldIssue> &issues) {
  issues.clear();
  if(!running_ || !world_->alive(world_->handle(object))) return false;
  if(!api_.inspectFields) {diagnostics_="Runtime sem leitura de campos em Play";return false;}
  const int size=api_.inspectFields(object,nullptr,0);
  if(size<=0 || size>1024*1024) {collectDiagnostics();return false;}
  std::string text(static_cast<usize>(size),'\0');
  if(api_.inspectFields(object,reinterpret_cast<u8*>(text.data()),size)!=size || text.find('\0')!=std::string::npos) return false;
  // User getters may remove their object. Validate again before touching storage.
  if(!world_->alive(world_->handle(object))) return false;
  std::istringstream input(text);input.imbue(std::locale::classic());
  std::string magic;u32 version=0,count=0;
  if(!(input>>magic>>version>>count) || magic!="ASTRA_FIELDS" || version!=1 || count>4096) return false;
  auto components=world_->graph().find(object)->components;
  usize expected=0;
  for(usize i=0;i<components.size();++i) expected+=scene::scriptBehavior(components.at(i))?1:0;
  if(count!=expected) return false;
  std::vector<ScriptFieldIssue> preparedIssues;
  std::vector<u64> seen;
  for(u32 i=0;i<count;++i) {
    u64 instance=0;u32 fields=0;
    if(!(input>>instance>>fields) || fields>1024 || std::find(seen.begin(),seen.end(),instance)!=seen.end()) return false;
    seen.push_back(instance);
    const auto *source=scene::scriptBehavior(components.findInstance(instance));
    if(!source) return false;
    auto value=*source;std::vector<std::string> ids;
    for(u32 f=0;f<fields;++f) {
      scene::ScriptPropertyValue property;u32 state=0;
      if(!(input>>std::quoted(property.id)>>std::quoted(property.valueType)>>state>>std::quoted(property.value)) || state>2 ||
         property.id.empty() || property.id.size()>256 || property.valueType.size()>512 ||
         std::find(ids.begin(),ids.end(),property.id)!=ids.end()) return false;
      ids.push_back(property.id);
      std::erase_if(value.properties,[&](const auto &p){return p.id==property.id;});
      if(state==0 && !scene::validScriptPropertyValue(property.valueType,property.value)) {
        state=2;property.value="Valor fora dos limites do Inspector";
      }
      if(state) preparedIssues.push_back({instance,property.id,property.value,state==1});
      else if(!value.setProperty(property.id,property.valueType,property.value)) return false;
    }
    if(!components.replaceInstance(instance,value)) return false;
  }
  input>>std::ws;if(!input.eof()) return false;
  auto *destination=world_->poseGraph().editComponents(object);
  if(!destination) return false;
  *destination=std::move(components);
  issues=std::move(preparedIssues);
  return true;
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
  value.kind = static_cast<QueryShapeKind>(shape.kind);
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
    value.colliderObject = hit.colliderObject;
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

bool ScriptBridge::timer(ObjectId object,u64 instance,u32 count) {
  if(!running_) return true;
  if(!api_.timer || !count) return false;
  const bool ok=api_.timer(object,instance,count)==0;
  collectDiagnostics();
  return ok;
}

void ScriptBridge::stop() {
  if (running_) api_.stop();
  if (events_) events_->attach(ComponentEventQueue::Consumer::Scripts,false);
  pendingHierarchy_.clear();
  sceneRequest_ = {};
  rendering_.end();
  running_ = false;
  world_ = nullptr;
  physics_ = nullptr;
  input_ = nullptr;
  access_ = scene::ScriptSceneAccess{};
  // A tabela da família continua instalada: uma cópia retida pelo runtime
  // gerenciado recusa por `world_` nulo (NotRunning) em vez de saltar para nulo.
}

} // namespace ae::runtime

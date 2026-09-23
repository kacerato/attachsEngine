#include "renderer/rendering_settings_file.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace ae::renderer {
namespace {
std::string number(float value) { char text[32]; std::snprintf(text,sizeof(text),"%.9g",static_cast<double>(value)); return text; }
bool parseFloat(std::string_view text,float &out) {
  const std::string copy(text); char *end=nullptr; const float value=std::strtof(copy.c_str(),&end);
  if(end==copy.c_str()||*end!='\0'||!std::isfinite(value)) return false;
  out=value;
  return true;
}
bool parseUnsigned(std::string_view text,u32 &out) {
  if(text.empty()||text.front()=='-') return false;
  const std::string copy(text); char *end=nullptr; const unsigned long value=std::strtoul(copy.c_str(),&end,10);
  if(end==copy.c_str()||*end!='\0'||value>std::numeric_limits<u32>::max()) return false;
  out=static_cast<u32>(value);
  return true;
}
const char *overrideName(FeatureOverride value) {
  return value==FeatureOverride::Enabled?"on":value==FeatureOverride::Disabled?"off":"inherit";
}
bool parseOverride(std::string_view text,FeatureOverride &out) {
  if(text=="on") out=FeatureOverride::Enabled;
  else if(text=="off") out=FeatureOverride::Disabled;
  else if(text=="inherit") out=FeatureOverride::Inherit;
  else return false;
  return true;
}
template<class T,class Parse,class Name> bool parseNamed(std::string_view text,T &out,Parse parse,Name name) {
  const std::string copy(text); out=parse(copy.c_str()); return std::string_view(name(out))==text;
}
bool parseRangedFloat(std::string_view text,float &out,float minimum,float maximum,float inherited) {
  float value=0; if(!parseFloat(text,value)) return false;
  if(value==inherited||(value>=minimum&&value<=maximum)) {out=value;return true;} return false;
}
bool parseRangedUnsigned(std::string_view text,u32 &out,u32 minimum,u32 maximum) {
  u32 value=0; if(!parseUnsigned(text,value)) return false;
  if(value==0||(value>=minimum&&value<=maximum)) {out=value;return true;} return false;
}
void append(std::string &text,const char *key,const char *value) {text+=key;text+='=';text+=value;text+='\n';}
void append(std::string &text,const char *key,float value) {append(text,key,number(value).c_str());}
void append(std::string &text,const char *key,u32 value) {append(text,key,std::to_string(value).c_str());}
} // namespace

std::string writeRenderingSettings(const ProjectRenderingSettings &s) {
  std::string text="astra_rendering "+std::to_string(RenderingSettingsFileVersion)+"\n";
  append(text,"quality",qualityPresetName(s.preset));
  append(text,"shadows",shadowQualityName(s.shadows));
  append(text,"ambient",ambientQualityName(s.ambient));
  append(text,"post",postQualityName(s.post));
  append(text,"textures",textureQualityName(s.textures));
  append(text,"water_mesh",waterMeshQualityName(s.waterMesh));
  append(text,"resolution_scale",s.resolutionScale);
  append(text,"maximum_render_hz",s.maximumRenderHz);
  append(text,"upscaling_filter",upscalingFilterName(s.upscalingFilter));
  append(text,"shadow_cascade_count",s.shadowCascadeCount);
  append(text,"shadow_cascade_resolution",s.shadowCascadeResolution);
  append(text,"shadow_filter_taps",s.shadowFilterTaps);
  append(text,"shadow_far_filter_taps",s.shadowFarFilterTaps);
  append(text,"shadow_maximum_distance",s.shadowMaximumDistance);
  append(text,"shadow_depth_bias_constant",s.shadowDepthBiasConstant);
  append(text,"shadow_depth_bias_slope",s.shadowDepthBiasSlope);
  append(text,"shadow_normal_offset_texels",s.shadowNormalOffsetTexels);
  append(text,"static_shadow_cache",overrideName(s.staticShadowCache));
  append(text,"shadow_cache_guard_band_ratio",s.shadowCacheGuardBandRatio);
  append(text,"shadow_cascade_blend_ratio",s.shadowCascadeBlendRatio);
  append(text,"shadow_distance_fade_ratio",s.shadowDistanceFadeRatio);
  append(text,"lod_pixel_error_budget",s.lodPixelErrorBudget);
  append(text,"coverage_lod_pixel_error_budget",s.coverageLodPixelErrorBudget);
  append(text,"lod_hysteresis_band_ratio",s.lodHysteresisBandRatio);
  append(text,"lod_selection",overrideName(s.lodSelection));
  append(text,"material_shader_variants",overrideName(s.materialShaderVariants));
  append(text,"environment_split_sum_brdf",overrideName(s.environmentSplitSumBrdf));
  append(text,"normal_map_maximum_distance",s.normalMapMaximumDistance);
  append(text,"specular_probe_maximum_distance",s.specularProbeMaximumDistance);
  append(text,"metallic_roughness_maximum_distance",s.metallicRoughnessMaximumDistance);
  append(text,"emissive_maximum_distance",s.emissiveMaximumDistance);
  append(text,"material_detail_fade_band_ratio",s.materialDetailFadeBandRatio);
  append(text,"thermal_distance_scaling",overrideName(s.thermalDistanceScaling));
  append(text,"anti_aliasing",antiAliasingModeName(s.antiAliasing));
  append(text,"post_fxaa",overrideName(s.postFxaa));
  append(text,"vignette",overrideName(s.postVignette));
  append(text,"bloom_threshold",s.bloomThreshold);
  append(text,"bloom_intensity",s.bloomIntensity);
  append(text,"contrast",s.postContrast);
  append(text,"saturation",s.postSaturation);
  append(text,"sharpen",s.postSharpen);
  append(text,"temporal_history_weight",s.temporalHistoryWeight);
  append(text,"dynamic_resolution",overrideName(s.dynamicResolution));
  append(text,"dynamic_resolution_minimum_scale",s.dynamicResolutionMinimumScale);
  append(text,"dynamic_resolution_decrease_step",s.dynamicResolutionDecreaseStep);
  append(text,"dynamic_resolution_increase_step",s.dynamicResolutionIncreaseStep);
  append(text,"dynamic_resolution_recovery_headroom_ratio",s.dynamicResolutionRecoveryHeadroomRatio);
  append(text,"dynamic_resolution_overload_frames",s.dynamicResolutionOverloadFrames);
  append(text,"dynamic_resolution_recovery_frames",s.dynamicResolutionRecoveryFrames);
  return text;
}

bool readRenderingSettings(std::string_view text,ProjectRenderingSettings &out) {
  ProjectRenderingSettings result=out; bool header=false; u32 fileVersion=0;
  while(!text.empty()) {
    const auto end=text.find('\n'); std::string_view line=text.substr(0,end);
    text=end==std::string_view::npos?std::string_view{}:text.substr(end+1);
    while(!line.empty()&&(line.back()=='\r'||line.back()==' ')) line.remove_suffix(1);
    if(line.empty()) continue;
    if(!header) {
      if(!line.starts_with("astra_rendering ")||!parseUnsigned(line.substr(16),fileVersion)||fileVersion<1||
         fileVersion>RenderingSettingsFileVersion) return false;
      header=true; continue;
    }
    const auto equals=line.find('='); if(equals==std::string_view::npos) return false;
    const auto key=line.substr(0,equals),value=line.substr(equals+1);
    if(key=="quality") {if(!parseNamed(value,result.preset,parseQualityPreset,qualityPresetName)) return false;}
    else if(key=="shadows") {if(!parseNamed(value,result.shadows,parseShadowQuality,shadowQualityName)) return false;}
    else if(key=="ambient") {if(!parseNamed(value,result.ambient,parseAmbientQuality,ambientQualityName)) return false;}
    else if(key=="post") {if(!parseNamed(value,result.post,parsePostQuality,postQualityName)) return false;}
    else if(key=="textures") {if(!parseNamed(value,result.textures,parseTextureQuality,textureQualityName)) return false;}
    else if(key=="water_mesh") {if(!parseNamed(value,result.waterMesh,parseWaterMeshQuality,waterMeshQualityName)) return false;}
    else if(key=="resolution_scale") {if(!parseRangedFloat(value,result.resolutionScale,.5f,1.0f,0.0f)) return false;}
    else if(key=="maximum_render_hz") {if(!parseRangedUnsigned(value,result.maximumRenderHz,24,240)) return false;}
    else if(key=="upscaling_filter") {if(!parseNamed(value,result.upscalingFilter,parseUpscalingFilter,upscalingFilterName)) return false;}
    else if(key=="shadow_cascade_count") {if(!parseRangedUnsigned(value,result.shadowCascadeCount,1,4)) return false;}
    else if(key=="shadow_cascade_resolution") {if(!parseRangedUnsigned(value,result.shadowCascadeResolution,256,4096)) return false;}
    else if(key=="shadow_filter_taps"||key=="shadow_far_filter_taps") {
      u32 parsed=0;if(!parseUnsigned(value,parsed)||!(parsed==0||parsed==1||parsed==9||parsed==25)) return false;
      (key=="shadow_filter_taps"?result.shadowFilterTaps:result.shadowFarFilterTaps)=parsed;
    } else if(key=="shadow_maximum_distance") {if(!parseRangedFloat(value,result.shadowMaximumDistance,.01f,100000.0f,0.0f)) return false;}
    else if(key=="shadow_depth_bias_constant") {if(!parseRangedFloat(value,result.shadowDepthBiasConstant,0.0f,100.0f,-1.0f)) return false;}
    else if(key=="shadow_depth_bias_slope") {if(!parseRangedFloat(value,result.shadowDepthBiasSlope,0.0f,100.0f,-1.0f)) return false;}
    else if(key=="shadow_normal_offset_texels") {if(!parseRangedFloat(value,result.shadowNormalOffsetTexels,0.0f,100.0f,-1.0f)) return false;}
    else if(key=="static_shadow_cache") {if(!parseOverride(value,result.staticShadowCache)) return false;}
    else if(key=="shadow_cache_guard_band_ratio") {if(!parseRangedFloat(value,result.shadowCacheGuardBandRatio,1.0f,1.25f,0.0f)) return false;}
    else if(key=="shadow_cascade_blend_ratio") {if(!parseRangedFloat(value,result.shadowCascadeBlendRatio,0.0f,.30f,-1.0f)) return false;}
    else if(key=="shadow_distance_fade_ratio") {if(!parseRangedFloat(value,result.shadowDistanceFadeRatio,0.0f,.50f,-1.0f)) return false;}
    else if(key=="lod_pixel_error_budget") {if(!parseRangedFloat(value,result.lodPixelErrorBudget,.25f,16.0f,0.0f)) return false;}
    else if(key=="coverage_lod_pixel_error_budget") {if(!parseRangedFloat(value,result.coverageLodPixelErrorBudget,.25f,128.0f,0.0f)) return false;}
    else if(key=="lod_hysteresis_band_ratio") {if(!parseRangedFloat(value,result.lodHysteresisBandRatio,.10f,.99f,0.0f)) return false;}
    else if(key=="lod_selection") {if(!parseOverride(value,result.lodSelection)) return false;}
    else if(key=="material_shader_variants") {if(!parseOverride(value,result.materialShaderVariants)) return false;}
    else if(key=="environment_split_sum_brdf") {if(!parseOverride(value,result.environmentSplitSumBrdf)) return false;}
    else if(key=="normal_map_maximum_distance"||key=="specular_probe_maximum_distance"||key=="metallic_roughness_maximum_distance"||key=="emissive_maximum_distance") {
      float parsed=0;if(!parseRangedFloat(value,parsed,0.0f,100000.0f,-1.0f)) return false;
      if(key=="normal_map_maximum_distance") result.normalMapMaximumDistance=parsed;
      else if(key=="specular_probe_maximum_distance") result.specularProbeMaximumDistance=parsed;
      else if(key=="metallic_roughness_maximum_distance") result.metallicRoughnessMaximumDistance=parsed;
      else result.emissiveMaximumDistance=parsed;
    } else if(key=="material_detail_fade_band_ratio") {if(!parseRangedFloat(value,result.materialDetailFadeBandRatio,0.0f,.50f,-1.0f)) return false;}
    else if(key=="thermal_distance_scaling") {if(!parseOverride(value,result.thermalDistanceScaling)) return false;}
    else if(key=="anti_aliasing") {if(!parseNamed(value,result.antiAliasing,parseAntiAliasingMode,antiAliasingModeName)) return false;}
    else if(key=="post_fxaa") {if(!parseOverride(value,result.postFxaa)) return false;}
    else if(key=="vignette") {if(!parseOverride(value,result.postVignette)) return false;}
    else if(key=="bloom_threshold") {if(!parseRangedFloat(value,result.bloomThreshold,0.0f,8.0f,-1.0f)) return false;}
    else if(key=="bloom_intensity") {if(!parseRangedFloat(value,result.bloomIntensity,0.0f,2.0f,-1.0f)) return false;}
    else if(key=="contrast") {if(!parseRangedFloat(value,result.postContrast,.5f,2.0f,-1.0f)) return false;}
    else if(key=="saturation") {if(!parseRangedFloat(value,result.postSaturation,0.0f,2.0f,-1.0f)) return false;}
    else if(key=="sharpen") {if(!parseRangedFloat(value,result.postSharpen,0.0f,1.0f,-1.0f)) return false;}
    else if(key=="temporal_history_weight") {if(!parseRangedFloat(value,result.temporalHistoryWeight,0.0f,.97f,-1.0f)) return false;}
    else if(key=="dynamic_resolution") {if(!parseOverride(value,result.dynamicResolution)) return false;}
    else if(key=="dynamic_resolution_minimum_scale") {if(!parseRangedFloat(value,result.dynamicResolutionMinimumScale,.5f,1.0f,0.0f)) return false;}
    else if(key=="dynamic_resolution_decrease_step") {if(!parseRangedFloat(value,result.dynamicResolutionDecreaseStep,.01f,.25f,0.0f)) return false;}
    else if(key=="dynamic_resolution_increase_step") {if(!parseRangedFloat(value,result.dynamicResolutionIncreaseStep,.005f,.25f,0.0f)) return false;}
    else if(key=="dynamic_resolution_recovery_headroom_ratio") {if(!parseRangedFloat(value,result.dynamicResolutionRecoveryHeadroomRatio,.5f,.95f,0.0f)) return false;}
    else if(key=="dynamic_resolution_overload_frames") {if(!parseRangedUnsigned(value,result.dynamicResolutionOverloadFrames,1,240)) return false;}
    else if(key=="dynamic_resolution_recovery_frames") {if(!parseRangedUnsigned(value,result.dynamicResolutionRecoveryFrames,1,1200)) return false;}
  }
  if(!header) return false;
  // v1 had only six keys. Missing v2 axes keep the caller/default value.
  result.schemaVersion=RenderingSettingsSchemaVersion; out=result; return true;
}

ProjectRenderingSettings withEditorDefaults(ProjectRenderingSettings settings) {
  if(settings.dynamicResolution==FeatureOverride::Inherit) settings.dynamicResolution=FeatureOverride::Disabled;
  if(settings.maximumRenderHz==0) settings.maximumRenderHz=60;
  return settings;
}
const char *qualityLevelLabel(QualityPreset v) {switch(v){case QualityPreset::C:return "Baixo";case QualityPreset::B:return "Médio";case QualityPreset::A:return "Alto";case QualityPreset::S:return "Ultra";case QualityPreset::Custom:return "Personalizado";default:return "Automático";}}
const char *antiAliasingLabel(AntiAliasingMode v) {switch(v){case AntiAliasingMode::Off:return "Desligado";case AntiAliasingMode::Fxaa:return "FXAA";case AntiAliasingMode::Temporal:return "TAA (câmera)";default:return "Do nível";}}
const char *upscalingFilterLabel(UpscalingFilter v) {switch(v){case UpscalingFilter::Bilinear:return "Bilinear";case UpscalingFilter::CatmullRom:return "Bicúbica nítida";case UpscalingFilter::Fsr1:return "AMD FSR 1";default:return "Do nível";}}
const char *shadowQualityLabel(ShadowQuality v) {switch(v){case ShadowQuality::Off:return "Desligadas";case ShadowQuality::Hard:return "Duras";case ShadowQuality::Soft:return "Suaves";case ShadowQuality::UltraSoft:return "Ultra suaves";default:return "Do nível";}}
const char *ambientQualityLabel(AmbientQuality v) {switch(v){case AmbientQuality::Constant:return "Constante";case AmbientQuality::Hemispheric:return "Hemisférica";case AmbientQuality::HemisphericSpecular:return "Hemisférica + reflexos";default:return "Do nível";}}
const char *postQualityLabel(PostQuality v) {switch(v){case PostQuality::None:return "Desligado";case PostQuality::Tonemap:return "Tonemap";case PostQuality::Bloom:return "Tonemap + bloom";default:return "Do nível";}}
const char *textureQualityLabel(TextureQuality v) {switch(v){case TextureQuality::Half:return "Meia resolução · 2×";case TextureQuality::Full:return "Completa · 8×";default:return "Do nível";}}
const char *featureOverrideLabel(FeatureOverride v) {return v==FeatureOverride::Enabled?"Ligado":v==FeatureOverride::Disabled?"Desligado":"Do nível";}
} // namespace ae::renderer

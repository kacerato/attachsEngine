#include "runtime/runtime_rendering_state.h"

namespace ae::runtime {

bool RuntimeRenderingState::validate(const renderer::ProjectRenderingSettings &settings) {
  const auto feature=[](renderer::FeatureOverride value) {return (u32)value<=(u32)renderer::FeatureOverride::Enabled;};
  if(settings.schemaVersion!=renderer::RenderingSettingsSchemaVersion ||
     (u32)settings.preset>(u32)renderer::QualityPreset::Custom ||
     (u32)settings.shadows>(u32)renderer::ShadowQuality::UltraSoft ||
     (u32)settings.ambient>(u32)renderer::AmbientQuality::HemisphericSpecular ||
     (u32)settings.post>(u32)renderer::PostQuality::Bloom ||
     (u32)settings.textures>(u32)renderer::TextureQuality::Full ||
     (u32)settings.waterMesh>=(u32)renderer::WaterMeshQuality::Count ||
     (u32)settings.antiAliasing>(u32)renderer::AntiAliasingMode::Temporal ||
     (u32)settings.upscalingFilter>(u32)renderer::UpscalingFilter::Fsr1 ||
     !feature(settings.staticShadowCache)||!feature(settings.lodSelection)||
     !feature(settings.materialShaderVariants)||!feature(settings.environmentSplitSumBrdf)||
     !feature(settings.thermalDistanceScaling)||!feature(settings.postFxaa)||
     !feature(settings.postVignette)||!feature(settings.dynamicResolution)) return false;
  renderer::ProjectRenderingSettings parsed{};
  return renderer::readRenderingSettings(renderer::writeRenderingSettings(settings), parsed);
}

bool RuntimeRenderingState::begin(u32 world, const renderer::ResolvedRenderingPolicy &effective) {
  if (!world || world_) return false;
  requested_ = authored_;
  effective_ = effective;
  world_ = world;
  pendingId_ = 0;
  changed_ = false;
  lastRequestSucceeded_ = true;
  effectiveAvailable_ = true;
  return true;
}

bool RuntimeRenderingState::request(u32 expectedWorld,
                                    const renderer::ProjectRenderingSettings &settings,
                                    u64 &requestId) {
  requestId = 0;
  if (!active(expectedWorld) || pendingId_ || !sink_ || !validate(settings)) return false;
  pendingSettings_ = settings;
  pendingSettings_.schemaVersion = renderer::RenderingSettingsSchemaVersion;
  requested_ = pendingSettings_; // Requested é a intenção; Effective só muda no acknowledge.
  pendingId_ = nextRequestId_++;
  if (!nextRequestId_) nextRequestId_ = 1;
  requestId = pendingId_;
  lastRequestSucceeded_ = false;
  if (sink_(pendingId_, pendingSettings_,false)) return true;
  pendingId_ = 0;
  return false;
}

bool RuntimeRenderingState::complete(u64 requestId, bool success,
                                     const renderer::ResolvedRenderingPolicy &effective,
                                     bool effectiveAvailable) {
  if (!world_ || !requestId || requestId != pendingId_) return false;
  pendingId_ = 0;
  lastRequestSucceeded_ = success;
  // O host descreve o estado que realmente restou. Em uma recusa anterior à
  // destruição ele pode repetir a política ativa; após um rebuild destrutivo
  // falhar, passa a política vazia para não expor o renderer morto como atual.
  effective_ = effective;
  effectiveAvailable_ = effectiveAvailable;
  if (success) changed_ = true;
  return true;
}

void RuntimeRenderingState::end() {
  if (world_ && (changed_ || pendingId_ || !effectiveAvailable_) && sink_) {
    const u64 restore = nextRequestId_++;
    sink_(restore, authored_,true); // restauração autoral substitui pedido ainda não consumido.
  }
  world_ = 0;
  pendingId_ = 0;
  changed_ = false;
  effectiveAvailable_ = false;
}

} // namespace ae::runtime

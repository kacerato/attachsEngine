#include "renderer/rendering_settings_file.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace ae::renderer {
namespace {

std::string number(float value) {
  char text[32];
  std::snprintf(text, sizeof(text), "%.4g", static_cast<double>(value));
  return text;
}

bool parseFloat(std::string_view text, float &out) {
  const std::string copy(text);
  char *end = nullptr;
  const float value = std::strtof(copy.c_str(), &end);
  if (end == copy.c_str() || *end != '\0' || !std::isfinite(value)) return false;
  out = value;
  return true;
}

bool parseUnsigned(std::string_view text, u32 &out) {
  const std::string copy(text);
  char *end = nullptr;
  const unsigned long value = std::strtoul(copy.c_str(), &end, 10);
  if (end == copy.c_str() || *end != '\0' || value > 1000) return false;
  out = static_cast<u32>(value);
  return true;
}

const char *overrideName(FeatureOverride value) {
  return value == FeatureOverride::Enabled ? "on" : value == FeatureOverride::Disabled ? "off" : "inherit";
}

bool parseOverride(std::string_view text, FeatureOverride &out) {
  if (text == "on") out = FeatureOverride::Enabled;
  else if (text == "off") out = FeatureOverride::Disabled;
  else if (text == "inherit") out = FeatureOverride::Inherit;
  else return false;
  return true;
}

} // namespace

std::string writeRenderingSettings(const ProjectRenderingSettings &settings) {
  std::string text = "astra_rendering " + std::to_string(RenderingSettingsFileVersion) + "\n";
  text += std::string("quality=") + qualityPresetName(settings.preset) + "\n";
  text += "resolution_scale=" + number(settings.resolutionScale) + "\n";
  text += std::string("dynamic_resolution=") + overrideName(settings.dynamicResolution) + "\n";
  text += std::string("anti_aliasing=") + antiAliasingModeName(settings.antiAliasing) + "\n";
  text += "sharpen=" + number(settings.postSharpen) + "\n";
  text += "maximum_render_hz=" + std::to_string(settings.maximumRenderHz) + "\n";
  return text;
}

bool readRenderingSettings(std::string_view text, ProjectRenderingSettings &out) {
  ProjectRenderingSettings result = out;
  bool header = false;
  while (!text.empty()) {
    const auto end = text.find('\n');
    std::string_view line = text.substr(0, end);
    text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.remove_suffix(1);
    if (line.empty()) continue;
    if (!header) {
      if (!line.starts_with("astra_rendering ")) return false;
      u32 version = 0;
      if (!parseUnsigned(line.substr(16), version) || version < 1) return false;
      header = true;
      continue;
    }
    const auto equals = line.find('=');
    if (equals == std::string_view::npos) return false;
    const auto key = line.substr(0, equals), value = line.substr(equals + 1);
    const std::string copy(value);
    if (key == "quality") {
      result.preset = parseQualityPreset(copy.c_str());
      if (std::string_view(qualityPresetName(result.preset)) != value) return false;
    } else if (key == "resolution_scale") {
      float scale = 0;
      if (!parseFloat(value, scale) || !(scale == 0 || (scale >= .5f && scale <= 1))) return false;
      result.resolutionScale = scale;
    } else if (key == "dynamic_resolution") {
      if (!parseOverride(value, result.dynamicResolution)) return false;
    } else if (key == "anti_aliasing") {
      result.antiAliasing = parseAntiAliasingMode(copy.c_str());
      if (std::string_view(antiAliasingModeName(result.antiAliasing)) != value) return false;
    } else if (key == "sharpen") {
      float sharpen = 0;
      if (!parseFloat(value, sharpen) || !(sharpen < 0 || sharpen <= 1)) return false;
      result.postSharpen = sharpen;
    } else if (key == "maximum_render_hz") {
      u32 hz = 0;
      if (!parseUnsigned(value, hz) || !(hz == 0 || (hz >= 24 && hz <= 240))) return false;
      result.maximumRenderHz = hz;
    }
  }
  if (!header) return false;
  out = result;
  return true;
}

ProjectRenderingSettings withEditorDefaults(ProjectRenderingSettings settings) {
  if (settings.dynamicResolution == FeatureOverride::Inherit) settings.dynamicResolution = FeatureOverride::Disabled;
  if (settings.maximumRenderHz == 0) settings.maximumRenderHz = 60;
  return settings;
}

const char *qualityLevelLabel(QualityPreset preset) {
  switch (preset) {
  case QualityPreset::C: return "Baixo";
  case QualityPreset::B: return "Médio";
  case QualityPreset::A: return "Alto";
  case QualityPreset::S: return "Ultra";
  case QualityPreset::Custom: return "Personalizado";
  case QualityPreset::Auto: break;
  }
  return "Automático";
}

const char *antiAliasingLabel(AntiAliasingMode mode) {
  switch (mode) {
  case AntiAliasingMode::Off: return "Desligado";
  case AntiAliasingMode::Fxaa: return "FXAA";
  case AntiAliasingMode::Temporal: return "Temporal (TAA)";
  case AntiAliasingMode::Inherit: break;
  }
  return "Do nível";
}

} // namespace ae::renderer

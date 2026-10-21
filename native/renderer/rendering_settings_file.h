#pragma once
// Project GAME rendering settings, persisted and included in exported packages.
// Unity 6000.0: Manual/class-QualitySettings.html and Manual/GameView.html.
// Authoring previews have an independent local budget in view_rendering_settings.h;
// saving game quality does not overwrite the current authoring viewport policy.
// The versioned file retains every supported per-axis override on roundtrip.
#include "renderer/rendering_policy.h"

#include <string>
#include <string_view>

namespace ae::renderer {

// v3: temporal upscalers (arm-asr/fsr2) and `temporal_upscaler_quality`.
// Older engines reject the file instead of misreading the new filter names.
inline constexpr u32 RenderingSettingsFileVersion = 3;

// Texto `chave=valor`, uma por linha, com a versão na primeira. Chave
// desconhecida é ignorada nas versões suportadas; versão futura ou valor
// ilegível numa chave conhecida recusa o arquivo inteiro, sem alterar a saída.
std::string writeRenderingSettings(const ProjectRenderingSettings &settings);
bool readRenderingSettings(std::string_view text, ProjectRenderingSettings &out);

// Legacy compatibility helper for consumers that explicitly request native-scale
// 60 Hz defaults. Current Android authoring uses settingsForRenderView instead;
// never apply this helper to the persisted game/export authority.
ProjectRenderingSettings withEditorDefaults(ProjectRenderingSettings settings);

// "Ampliação temporal" do painel: uma escolha só sobre dois eixos da política
// (anti-aliasing temporal nativo ou um ampliador temporal no filtro de
// ampliação). Desligar preserva o filtro espacial e o AA não temporal.
enum class TemporalReconstruction : u32 { Off = 0, NativeTaa, ArmAsr, Fsr2 };
TemporalReconstruction temporalReconstruction(const ProjectRenderingSettings &settings);
void setTemporalReconstruction(ProjectRenderingSettings &settings, TemporalReconstruction mode);
const char *temporalReconstructionLabel(TemporalReconstruction mode);

// Nomes do painel, no vocabulário dos níveis de qualidade da Unity.
const char *qualityLevelLabel(QualityPreset preset);
const char *antiAliasingLabel(AntiAliasingMode mode);
const char *upscalingFilterLabel(UpscalingFilter filter);
const char *temporalUpscalerQualityLabel(TemporalUpscalerQuality quality);
// Motivo legível da indisponibilidade, no vocabulário do painel.
const char *temporalUpscalerAvailabilityLabel(TemporalUpscalerAvailability availability);
const char *shadowQualityLabel(ShadowQuality quality);
const char *ambientQualityLabel(AmbientQuality quality);
const char *postQualityLabel(PostQuality quality);
const char *textureQualityLabel(TextureQuality quality);
const char *featureOverrideLabel(FeatureOverride value);

} // namespace ae::renderer

// Configurações de renderização do projeto (painel Qualidade).
//
// Protegido aqui: ida e volta pelo arquivo; versão futura ou valor ilegível
// recusa o arquivo inteiro; chave desconhecida é ignorada; e o editor usa
// resolução nativa sem escala dinâmica quando o projeto não decidiu.
#include "harness.h"
#include "renderer/rendering_settings_file.h"

using namespace ae;
using namespace ae::renderer;

AE_TEST(rendering_settings_survive_the_project_file) {
  ProjectRenderingSettings settings;
  settings.preset = QualityPreset::A;
  settings.resolutionScale = .85f;
  settings.dynamicResolution = FeatureOverride::Disabled;
  settings.antiAliasing = AntiAliasingMode::Temporal;
  settings.upscalingFilter = UpscalingFilter::Fsr1;
  settings.postSharpen = .35f;
  settings.postContrast = .123456791f + .5f;
  settings.maximumRenderHz = 60;
  settings.shadows = ShadowQuality::UltraSoft;
  settings.shadowCascadeCount = 4;
  settings.shadowCascadeResolution = 2048;
  settings.shadowMaximumDistance = 320.0f;
  settings.shadowDepthBiasConstant = 1.2f;
  settings.shadowDepthBiasSlope = 1.8f;
  settings.shadowNormalOffsetTexels = 1.5f;
  settings.ambient = AmbientQuality::HemisphericSpecular;
  settings.environmentSplitSumBrdf = FeatureOverride::Enabled;
  settings.post = PostQuality::Bloom;
  settings.bloomThreshold = 1.1f;
  settings.bloomIntensity = .4f;
  settings.temporalHistoryWeight = .91f;
  settings.textures = TextureQuality::Full;
  settings.lodSelection = FeatureOverride::Enabled;
  settings.lodPixelErrorBudget = .75f;
  settings.lodHysteresisBandRatio = .8f;
  settings.dynamicResolutionMinimumScale = .65f;
  settings.dynamicResolutionDecreaseStep = .05f;
  ProjectRenderingSettings restored;
  AE_EXPECT_TRUE(readRenderingSettings(writeRenderingSettings(settings), restored), "o arquivo volta");
  AE_EXPECT_TRUE(restored.preset == QualityPreset::A, "nível Alto");
  AE_EXPECT_TRUE(restored.resolutionScale == .85f, "escala 85%");
  AE_EXPECT_TRUE(restored.dynamicResolution == FeatureOverride::Disabled, "escala dinâmica desligada");
  AE_EXPECT_TRUE(restored.antiAliasing == AntiAliasingMode::Temporal, "TAA");
  AE_EXPECT_TRUE(restored.upscalingFilter == UpscalingFilter::Fsr1, "ampliação AMD FSR 1");
  AE_EXPECT_TRUE(restored.postSharpen == .35f && restored.maximumRenderHz == 60, "nitidez e taxa alvo");
  AE_EXPECT_TRUE(restored.postContrast == settings.postContrast, "float mantém round-trip exato");
  AE_EXPECT_TRUE(restored.shadows == ShadowQuality::UltraSoft && restored.shadowCascadeCount == 4 &&
                 restored.shadowCascadeResolution == 2048 && restored.shadowMaximumDistance == 320.0f,
                 "qualidade, cascatas, resolução e alcance das sombras");
  AE_EXPECT_TRUE(restored.shadowDepthBiasConstant == 1.2f && restored.shadowDepthBiasSlope == 1.8f &&
                 restored.shadowNormalOffsetTexels == 1.5f, "bias de sombra");
  AE_EXPECT_TRUE(restored.ambient == AmbientQuality::HemisphericSpecular &&
                 restored.environmentSplitSumBrdf == FeatureOverride::Enabled,
                 "ambiente especular");
  AE_EXPECT_TRUE(restored.post == PostQuality::Bloom && restored.bloomThreshold == 1.1f &&
                 restored.bloomIntensity == .4f && restored.temporalHistoryWeight == .91f,
                 "bloom e histórico temporal");
  AE_EXPECT_TRUE(restored.textures == TextureQuality::Full && restored.lodSelection == FeatureOverride::Enabled &&
                 restored.lodPixelErrorBudget == .75f && restored.lodHysteresisBandRatio == .8f,
                 "texturas e LOD");
  AE_EXPECT_TRUE(restored.dynamicResolutionMinimumScale == .65f && restored.dynamicResolutionDecreaseStep == .05f,
                 "parâmetros da resolução dinâmica");
}

AE_TEST(a_bad_rendering_settings_file_changes_nothing) {
  ProjectRenderingSettings settings;
  settings.preset = QualityPreset::B;
  AE_EXPECT_TRUE(!readRenderingSettings("astra_rendering 1\nresolution_scale=0.2\n", settings),
                 "escala abaixo de 50% é recusada");
  AE_EXPECT_TRUE(!readRenderingSettings("astra_rendering 1\nquality=ultra-mega\n", settings),
                 "nível desconhecido é recusado");
  AE_EXPECT_TRUE(!readRenderingSettings("quality=a\n", settings), "sem cabeçalho não é arquivo de configurações");
  AE_EXPECT_TRUE(settings.preset == QualityPreset::B, "recusa não muda nada");
  settings.shadowCascadeCount = 3;
  AE_EXPECT_TRUE(!readRenderingSettings("astra_rendering 2\nquality=a\nshadow_cascade_count=9\n", settings),
                 "override fora da faixa recusa o arquivo inteiro");
  AE_EXPECT_TRUE(settings.preset == QualityPreset::B && settings.shadowCascadeCount == 3,
                 "falha tardia também é atômica");
  // Chave que esta versão não conhece é de um arquivo mais novo: abre com o resto.
  AE_EXPECT_TRUE(readRenderingSettings("astra_rendering 2\nquality=s\nupscaling=fsr\n", settings),
                 "chave futura é ignorada");
  AE_EXPECT_TRUE(settings.preset == QualityPreset::S, "o que se entende vale");
  AE_EXPECT_TRUE(!readRenderingSettings("astra_rendering 4\nquality=a\n", settings),
                 "versão futura requer migração conhecida");
}

AE_TEST(rendering_settings_v1_migrates_without_inventing_new_overrides) {
  ProjectRenderingSettings settings;
  settings.upscalingFilter = UpscalingFilter::Bilinear;
  settings.shadowCascadeCount = 3;
  AE_EXPECT_TRUE(readRenderingSettings(
    "astra_rendering 1\nquality=a\nresolution_scale=0.8\ndynamic_resolution=off\n"
    "anti_aliasing=fxaa\nsharpen=0.2\nmaximum_render_hz=60\n", settings), "arquivo v1 abre");
  AE_EXPECT_TRUE(settings.preset == QualityPreset::A && settings.resolutionScale == .8f &&
                 settings.antiAliasing == AntiAliasingMode::Fxaa, "campos v1 migram");
  AE_EXPECT_TRUE(settings.upscalingFilter == UpscalingFilter::Bilinear && settings.shadowCascadeCount == 3,
                 "eixos ausentes preservam o valor do chamador");
}

AE_TEST(legacy_fxaa_override_survives_the_project_file) {
  ProjectRenderingSettings settings;
  settings.antiAliasing = AntiAliasingMode::Inherit;
  settings.postFxaa = FeatureOverride::Disabled;
  ProjectRenderingSettings restored;
  AE_EXPECT_TRUE(readRenderingSettings(writeRenderingSettings(settings), restored), "arquivo com override legado abre");
  AE_EXPECT_TRUE(restored.antiAliasing == AntiAliasingMode::Inherit &&
                 restored.postFxaa == FeatureOverride::Disabled, "fallback FXAA não muda de semântica");
}

AE_TEST(the_editor_renders_native_unless_the_project_asks_otherwise) {
  const auto editor = withEditorDefaults({});
  AE_EXPECT_TRUE(editor.dynamicResolution == FeatureOverride::Disabled, "editor sem escala dinâmica por padrão");
  AE_EXPECT_EQ(editor.maximumRenderHz, 60u, "editor mira 60 Hz, não 120");
  ProjectRenderingSettings chosen;
  chosen.dynamicResolution = FeatureOverride::Enabled;
  chosen.maximumRenderHz = 120;
  const auto kept = withEditorDefaults(chosen);
  AE_EXPECT_TRUE(kept.dynamicResolution == FeatureOverride::Enabled && kept.maximumRenderHz == 120,
                 "a escolha explícita do projeto manda");
  // Com a escala dinâmica desligada, a política resolve a resolução pedida e
  // não um intervalo que o controlador vá derrubar.
  RenderingCapabilities device;
  device.profile = rhi::DeviceProfile::B;
  device.displayHz = 120;
  const auto policy = resolveRenderingPolicy(editor, device, ThermalPressure::None);
  AE_EXPECT_TRUE(!policy.dynamicResolution.enabled, "sem controlador de escala no editor");
  AE_EXPECT_TRUE(policy.resolutionScale >= .999f, "resolução nativa");
}

AE_TEST(rendering_settings_persist_texture_streaming_and_reject_out_of_range) {
  ProjectRenderingSettings settings;
  settings.textureStreaming = FeatureOverride::Enabled;
  settings.textureStreamingBudgetMegabytes = 768;
  settings.textureStreamingMaxLevelReduction = 3;
  settings.textureStreamingUploadKilobytesPerFrame = 2048;
  ProjectRenderingSettings restored;
  AE_EXPECT_TRUE(readRenderingSettings(writeRenderingSettings(settings), restored), "arquivo volta");
  AE_EXPECT_TRUE(restored.textureStreaming == FeatureOverride::Enabled && restored.textureStreamingBudgetMegabytes == 768 &&
                 restored.textureStreamingMaxLevelReduction == 3 && restored.textureStreamingUploadKilobytesPerFrame == 2048,
                 "streaming, orçamento, redução e envio sobrevivem");
  ProjectRenderingSettings untouched;
  AE_EXPECT_TRUE(!readRenderingSettings("astra_rendering 3\ntexture_streaming_max_level_reduction=9\n", untouched) &&
                 untouched.textureStreamingMaxLevelReduction == 0, "redução fora de 1..7 recusa o arquivo");
  AE_EXPECT_TRUE(!readRenderingSettings("astra_rendering 3\ntexture_streaming_budget_mb=8\n", untouched),
                 "orçamento abaixo de 32 MB recusado");
  AE_EXPECT_TRUE(readRenderingSettings("astra_rendering 3\nquality=b\n", untouched) &&
                 untouched.textureStreaming == FeatureOverride::Inherit, "arquivo anterior segue sem streaming");
}

AE_TEST(rendering_settings_v3_persist_temporal_upscaler_and_reject_future_names) {
  ProjectRenderingSettings settings;
  settings.upscalingFilter = UpscalingFilter::ArmAsr;
  settings.temporalUpscalerQuality = TemporalUpscalerQuality::Balanced;
  settings.resolutionScale = .67f;
  const auto text = writeRenderingSettings(settings);
  AE_EXPECT_TRUE(text.rfind("astra_rendering 3\n", 0) == 0, "arquivo v3");
  ProjectRenderingSettings restored;
  AE_EXPECT_TRUE(readRenderingSettings(text, restored), "v3 volta");
  AE_EXPECT_TRUE(restored.upscalingFilter == UpscalingFilter::ArmAsr &&
                 restored.temporalUpscalerQuality == TemporalUpscalerQuality::Balanced &&
                 restored.resolutionScale == .67f, "Arm ASR, preset e escala sobrevivem");
  settings.upscalingFilter = UpscalingFilter::Fsr2;
  AE_EXPECT_TRUE(readRenderingSettings(writeRenderingSettings(settings), restored) &&
                 restored.upscalingFilter == UpscalingFilter::Fsr2, "AMD FSR 2 sobrevive");
  ProjectRenderingSettings untouched;
  AE_EXPECT_TRUE(!readRenderingSettings("astra_rendering 3\ntemporal_upscaler_quality=extreme\n", untouched) &&
                 untouched.temporalUpscalerQuality == TemporalUpscalerQuality::Inherit,
                 "preset ilegível recusa o arquivo sem alterar a saída");
  AE_EXPECT_TRUE(readRenderingSettings("astra_rendering 2\nupscaling_filter=fsr1\n", untouched) &&
                 untouched.upscalingFilter == UpscalingFilter::Fsr1, "v2 continua legível");
}

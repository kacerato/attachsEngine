// Configurações de renderização do projeto (painel Qualidade).
//
// Protegido aqui: ida e volta pelo arquivo; arquivo de versão futura abre com o
// que entende; valor ilegível recusa o arquivo inteiro; e o editor usa
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
  settings.postSharpen = .35f;
  settings.maximumRenderHz = 60;
  ProjectRenderingSettings restored;
  AE_EXPECT_TRUE(readRenderingSettings(writeRenderingSettings(settings), restored), "o arquivo volta");
  AE_EXPECT_TRUE(restored.preset == QualityPreset::A, "nível Alto");
  AE_EXPECT_TRUE(restored.resolutionScale == .85f, "escala 85%");
  AE_EXPECT_TRUE(restored.dynamicResolution == FeatureOverride::Disabled, "escala dinâmica desligada");
  AE_EXPECT_TRUE(restored.antiAliasing == AntiAliasingMode::Temporal, "TAA");
  AE_EXPECT_TRUE(restored.postSharpen == .35f && restored.maximumRenderHz == 60, "nitidez e taxa alvo");
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
  // Chave que esta versão não conhece é de um arquivo mais novo: abre com o resto.
  AE_EXPECT_TRUE(readRenderingSettings("astra_rendering 2\nquality=s\nupscaling=fsr\n", settings),
                 "chave futura é ignorada");
  AE_EXPECT_TRUE(settings.preset == QualityPreset::S, "o que se entende vale");
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

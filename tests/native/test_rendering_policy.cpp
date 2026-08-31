// Política global de renderização (ADR-014).
//
// O que estes testes protegem não é uma tabela de números — é a forma da política:
// preset é um ponto, eixos são independentes, capability só reduz, térmica é
// transitória, e toda degradação fica registrada com motivo.
#include "harness.h"
#include "renderer/rendering_policy.h"

using namespace ae;
using namespace ae::test;
using namespace ae::renderer;

namespace {

RenderingCapabilities strongDevice() {
  RenderingCapabilities capabilities{};
  capabilities.profile = rhi::DeviceProfile::S;
  capabilities.maximumImage2DSize = 16384;
  capabilities.maximumImageArrayLayers = 2048;
  capabilities.supportsDepthSampling = true;
  capabilities.maximumSamplerAnisotropy = 16.0f;
  capabilities.displayHz = 120.0f;
  return capabilities;
}

bool hasClamp(const ResolvedRenderingPolicy &policy, const char *axis, PolicyClamp reason) {
  for (u32 index = 0; index < policy.clampCount; ++index) {
    const auto &record = policy.clamps[index];
    if (record.reason != reason) continue;
    const char *a = record.axis;
    const char *b = axis;
    while (*a != '\0' && *a == *b) { ++a; ++b; }
    if (*a == '\0' && *b == '\0') return true;
  }
  return false;
}

} // namespace

AE_TEST(policy_auto_deriva_do_perfil_detectado) {
  auto capabilities = strongDevice();
  const auto s = resolveRenderingPolicy({}, capabilities, ThermalPressure::None);
  AE_EXPECT_TRUE(s.shadows.enabled, "perfil S liga sombras");
  AE_EXPECT_EQ(s.shadows.cascadeCount, 4u, "perfil S usa quatro cascatas");
  AE_EXPECT_TRUE(s.post.bloom, "perfil S liga bloom");
  AE_EXPECT_TRUE(s.ambient.specularProbe, "perfil S liga sonda especular");

  capabilities.profile = rhi::DeviceProfile::C;
  const auto c = resolveRenderingPolicy({}, capabilities, ThermalPressure::None);
  // O perfil base perde filtro e alcance, mas não perde a sombra: sem oclusão o
  // sol volta a iluminar tudo igualmente, que é o defeito que ela existe para
  // corrigir.
  AE_EXPECT_TRUE(c.shadows.enabled, "perfil C mantem sombra direcional");
  AE_EXPECT_EQ(c.shadows.cascadeCount, 1u, "perfil C usa uma cascata");
  AE_EXPECT_EQ(c.shadows.filterTaps, 1u, "perfil C nao filtra");
  AE_EXPECT_TRUE(!c.post.dedicatedPass, "perfil C nao paga passe de pos");
}

AE_TEST(policy_eixo_sobrescrito_nao_arrasta_os_outros) {
  // A prova de que são muitas configurações e não três nomes: mexer num eixo não
  // pode mover nenhum outro.
  ProjectRenderingSettings settings{};
  settings.preset = QualityPreset::C;
  settings.shadows = ShadowQuality::UltraSoft;
  const auto policy = resolveRenderingPolicy(settings, strongDevice(), ThermalPressure::None);
  AE_EXPECT_EQ(policy.shadows.cascadeCount, 4u, "override de sombra vale sobre o preset");
  AE_EXPECT_TRUE(!policy.post.dedicatedPass, "override de sombra nao liga pos");
  AE_EXPECT_EQ(policy.textures.residencyMipBias, 1u, "override de sombra nao muda textura");
}

AE_TEST(policy_preset_explicito_ignora_o_perfil_detectado) {
  ProjectRenderingSettings settings{};
  settings.preset = QualityPreset::S;
  auto capabilities = strongDevice();
  capabilities.profile = rhi::DeviceProfile::C;
  const auto policy = resolveRenderingPolicy(settings, capabilities, ThermalPressure::None);
  AE_EXPECT_EQ(policy.effectiveProfile, rhi::DeviceProfile::S, "preset explicito manda");
  AE_EXPECT_EQ(policy.shadows.cascadeCount, 4u, "preset explicito eleva a qualidade");
}

AE_TEST(policy_capability_reduz_mas_nunca_eleva) {
  ProjectRenderingSettings settings{};
  settings.preset = QualityPreset::S;
  auto capabilities = strongDevice();
  capabilities.maximumImage2DSize = 1024;
  const auto policy = resolveRenderingPolicy(settings, capabilities, ThermalPressure::None);
  AE_EXPECT_EQ(policy.shadows.cascadeResolution, 1024u, "resolucao de cascata limitada");
  AE_EXPECT_TRUE(hasClamp(policy, "shadows.cascadeResolution", PolicyClamp::Capability),
                 "reducao por capability precisa ficar registrada");
}

AE_TEST(policy_sem_amostragem_de_profundidade_desliga_sombra_em_vez_de_falsificar) {
  ProjectRenderingSettings settings{};
  settings.preset = QualityPreset::S;
  auto capabilities = strongDevice();
  capabilities.supportsDepthSampling = false;
  const auto policy = resolveRenderingPolicy(settings, capabilities, ThermalPressure::None);
  AE_EXPECT_TRUE(!policy.shadows.enabled, "sem depth sampling nao ha mapa de sombra legivel");
  AE_EXPECT_EQ(policy.shadows.cascadeCount, 0u, "sombra desligada nao reserva cascata");
  AE_EXPECT_TRUE(hasClamp(policy, "shadows", PolicyClamp::Capability), "motivo registrado");
}

AE_TEST(policy_termica_piora_um_degrau_e_registra_o_motivo) {
  const auto none = resolveRenderingPolicy({}, strongDevice(), ThermalPressure::None);
  const auto light = resolveRenderingPolicy({}, strongDevice(), ThermalPressure::Light);
  AE_EXPECT_EQ(none.shadows.cascadeCount, 4u, "sem pressao, qualidade cheia");
  AE_EXPECT_EQ(light.shadows.cascadeCount, 3u, "pressao leve cede um degrau de sombra");
  AE_EXPECT_TRUE(!light.post.bloom, "pressao leve cede bloom antes de ceder ambiente");
  AE_EXPECT_TRUE(light.ambient.hemispheric, "ambiente resiste a pressao leve");
  AE_EXPECT_TRUE(hasClamp(light, "shadows", PolicyClamp::Thermal), "motivo termico registrado");
}

AE_TEST(policy_termica_severa_cede_ambiente_e_resolucao) {
  const auto severe = resolveRenderingPolicy({}, strongDevice(), ThermalPressure::Severe);
  AE_EXPECT_TRUE(!severe.ambient.specularProbe, "pressao severa cede a sonda especular");
  AE_EXPECT_TRUE(severe.resolutionScale <= 0.75f, "pressao severa reduz resolucao");
  AE_EXPECT_TRUE(hasClamp(severe, "resolutionScale", PolicyClamp::Thermal), "motivo registrado");
}

AE_TEST(policy_termica_nao_altera_a_escolha_do_projeto) {
  // A pressão é transitória: as mesmas configurações de projeto, sem pressão,
  // precisam devolver exatamente a qualidade original. Degradação que "gruda"
  // seria uma alteração silenciosa do projeto do autor.
  ProjectRenderingSettings settings{};
  settings.preset = QualityPreset::A;
  const auto before = resolveRenderingPolicy(settings, strongDevice(), ThermalPressure::None);
  resolveRenderingPolicy(settings, strongDevice(), ThermalPressure::Severe);
  const auto after = resolveRenderingPolicy(settings, strongDevice(), ThermalPressure::None);
  AE_EXPECT_EQ(after.shadows.cascadeCount, before.shadows.cascadeCount, "sem efeito residual");
  AE_EXPECT_EQ(after.post.bloom, before.post.bloom, "sem efeito residual no pos");
}

AE_TEST(policy_e_deterministica_para_as_mesmas_entradas) {
  // O que permite reproduzir uma captura a partir das quatro entradas registradas.
  ProjectRenderingSettings settings{};
  settings.preset = QualityPreset::B;
  settings.post = PostQuality::Bloom;
  const auto first = resolveRenderingPolicy(settings, strongDevice(), ThermalPressure::Light);
  const auto second = resolveRenderingPolicy(settings, strongDevice(), ThermalPressure::Light);
  AE_EXPECT_EQ(first.shadows.cascadeCount, second.shadows.cascadeCount, "determinismo");
  AE_EXPECT_EQ(first.clampCount, second.clampCount, "mesmos clamps registrados");
  AE_EXPECT_EQ(first.resolutionScale, second.resolutionScale, "mesma resolucao");
}

AE_TEST(policy_registra_no_maximo_a_capacidade_sem_estourar) {
  // Muitas degradações simultâneas não podem corromper memória; o excedente é
  // descartado e o relatório continua legível.
  ProjectRenderingSettings settings{};
  settings.preset = QualityPreset::S;
  auto capabilities = strongDevice();
  capabilities.maximumImage2DSize = 512;
  capabilities.maximumSamplerAnisotropy = 1.0f;
  const auto policy = resolveRenderingPolicy(settings, capabilities, ThermalPressure::Severe);
  AE_EXPECT_TRUE(policy.clampCount <= MaximumPolicyClamps, "contagem dentro da capacidade");
  AE_EXPECT_TRUE(policy.clamped(), "degradacao precisa ser visivel");
}

AE_TEST(policy_anisotropia_respeita_o_limite_do_sampler) {
  ProjectRenderingSettings settings{};
  settings.preset = QualityPreset::S;
  auto capabilities = strongDevice();
  capabilities.maximumSamplerAnisotropy = 4.0f;
  const auto policy = resolveRenderingPolicy(settings, capabilities, ThermalPressure::None);
  AE_EXPECT_EQ(policy.textures.samplerAnisotropy, 4.0f, "nunca pede acima do limite");
  AE_EXPECT_TRUE(hasClamp(policy, "textures.samplerAnisotropy", PolicyClamp::Capability),
                 "motivo registrado");
}

AE_TEST(policy_parsing_round_trip_de_todos_os_eixos) {
  // Nome e enum precisam fechar nos dois sentidos: a configuração do projeto, a
  // opção de lançamento e o relatório de captura usam o mesmo vocabulário, e uma
  // divergência aqui faria uma captura registrar um preset que não foi o usado.
  const ShadowQuality shadows[] = {ShadowQuality::Off, ShadowQuality::Hard,
                                   ShadowQuality::Soft, ShadowQuality::UltraSoft};
  for (auto value : shadows) {
    AE_EXPECT_EQ(parseShadowQuality(shadowQualityName(value)), value, "round-trip de sombra");
  }
  const PostQuality posts[] = {PostQuality::None, PostQuality::Tonemap, PostQuality::Bloom};
  for (auto value : posts) {
    AE_EXPECT_EQ(parsePostQuality(postQualityName(value)), value, "round-trip de pos");
  }
  const AmbientQuality ambients[] = {AmbientQuality::Constant, AmbientQuality::Hemispheric,
                                     AmbientQuality::HemisphericSpecular};
  for (auto value : ambients) {
    AE_EXPECT_EQ(parseAmbientQuality(ambientQualityName(value)), value, "round-trip de ambiente");
  }
  const QualityPreset presets[] = {QualityPreset::Auto, QualityPreset::C, QualityPreset::B,
                                   QualityPreset::A, QualityPreset::S, QualityPreset::Custom};
  for (auto value : presets) {
    AE_EXPECT_EQ(parseQualityPreset(qualityPresetName(value)), value, "round-trip de preset");
  }
}

AE_TEST(policy_nome_invalido_herda_em_vez_de_derrubar) {
  // Configuração é conteúdo de usuário: um nome errado precisa cair no padrão,
  // nunca abortar a engine nem escolher um caminho arbitrário.
  AE_EXPECT_EQ(parseShadowQuality("inventado"), ShadowQuality::Inherit, "sombra desconhecida");
  AE_EXPECT_EQ(parseShadowQuality(nullptr), ShadowQuality::Inherit, "ponteiro nulo");
  AE_EXPECT_EQ(parsePostQuality(""), PostQuality::Inherit, "string vazia");
  AE_EXPECT_EQ(parseQualityPreset("ultra"), QualityPreset::Auto, "preset desconhecido vira auto");
}

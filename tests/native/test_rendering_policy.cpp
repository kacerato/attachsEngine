// Política global de renderização (ADR-014).
//
// O que estes testes protegem não é uma tabela de números — é a forma da política:
// preset é um ponto, eixos são independentes, capability só reduz, térmica é
// transitória, e toda degradação fica registrada com motivo.
#include "harness.h"
#include "renderer/rendering_policy.h"
#include "renderer/material_distance.h"

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
  AE_EXPECT_TRUE(c.post.dedicatedPass, "escala reduzida do perfil C exige passe de upscale");
  AE_EXPECT_TRUE(!c.post.bloom && !c.post.fxaa,
                 "perfil C nao paga filtros adicionais no passe de upscale");
}

AE_TEST(policy_eixo_sobrescrito_nao_arrasta_os_outros) {
  // A prova de que são muitas configurações e não três nomes: mexer num eixo não
  // pode mover nenhum outro.
  ProjectRenderingSettings settings{};
  settings.preset = QualityPreset::C;
  settings.shadows = ShadowQuality::UltraSoft;
  const auto policy = resolveRenderingPolicy(settings, strongDevice(), ThermalPressure::None);
  AE_EXPECT_EQ(policy.shadows.cascadeCount, 4u, "override de sombra vale sobre o preset");
  AE_EXPECT_TRUE(policy.post.dedicatedPass, "escala do preset C preserva seu upscale");
  AE_EXPECT_TRUE(!policy.post.bloom, "override de sombra nao liga bloom");
  AE_EXPECT_EQ(policy.textures.residencyMipBias, 1u, "override de sombra nao muda textura");
}

AE_TEST(policy_shader_variants_sao_capacidade_configuravel_e_nao_suposta_otimizacao) {
  ProjectRenderingSettings settings{};
  RenderingCapabilities capabilities{};
  capabilities.profile = rhi::DeviceProfile::B;
  auto policy = resolveRenderingPolicy(settings, capabilities, ThermalPressure::None);
  AE_EXPECT_TRUE(!policy.geometry.materialShaderVariants,
                 "perfil B evita variantes que regrediram no Adreno medido");
  capabilities.profile = rhi::DeviceProfile::A;
  policy = resolveRenderingPolicy(settings, capabilities, ThermalPressure::None);
  AE_EXPECT_TRUE(policy.geometry.materialShaderVariants, "perfil A pode especializar materiais");
  settings.materialShaderVariants = FeatureOverride::Disabled;
  policy = resolveRenderingPolicy(settings, capabilities, ThermalPressure::None);
  AE_EXPECT_TRUE(!policy.geometry.materialShaderVariants, "projeto controla o eixo independentemente");
}

AE_TEST(policy_split_sum_e_independente_mas_exige_sonda_especular) {
  ProjectRenderingSettings settings{};
  RenderingCapabilities capabilities{};
  capabilities.profile = rhi::DeviceProfile::A;
  auto policy = resolveRenderingPolicy(settings, capabilities, ThermalPressure::None);
  AE_EXPECT_TRUE(policy.ambient.specularProbe && policy.ambient.splitSumBrdf,
                 "ambiente avançado herda integração exata");
  settings.environmentSplitSumBrdf = FeatureOverride::Disabled;
  policy = resolveRenderingPolicy(settings, capabilities, ThermalPressure::None);
  AE_EXPECT_TRUE(policy.ambient.specularProbe && !policy.ambient.splitSumBrdf,
                 "radiância continua disponível sem LUT");
  settings.ambient = AmbientQuality::Hemispheric;
  settings.environmentSplitSumBrdf = FeatureOverride::Enabled;
  policy = resolveRenderingPolicy(settings, capabilities, ThermalPressure::None);
  AE_EXPECT_TRUE(!policy.ambient.specularProbe && !policy.ambient.splitSumBrdf,
                 "LUT isolada sem radiância não cria caminho inválido");
}

AE_TEST(policy_ajustes_finos_nao_dependem_do_nome_do_preset) {
  ProjectRenderingSettings settings{};
  settings.preset = QualityPreset::C;
  settings.shadowCascadeCount = 3;
  settings.shadowCascadeResolution = 1792;
  settings.shadowFilterTaps = 25;
  settings.shadowFarFilterTaps = 9;
  settings.lodPixelErrorBudget = 1.25f;
  settings.coverageLodPixelErrorBudget = 37.0f;
  settings.lodSelection = FeatureOverride::Enabled;
  settings.normalMapMaximumDistance = 180.0f;
  settings.metallicRoughnessMaximumDistance = 260.0f;
  settings.postFxaa = FeatureOverride::Enabled;
  settings.postContrast = 1.08f;
  settings.dynamicResolution = FeatureOverride::Enabled;
  settings.dynamicResolutionMinimumScale = 0.72f;
  const auto policy = resolveRenderingPolicy(settings, strongDevice(), ThermalPressure::None);
  AE_EXPECT_EQ(policy.shadows.cascadeCount, 3u, "cascatas sao um eixo numerico");
  AE_EXPECT_EQ(policy.shadows.cascadeResolution, 1792u, "resolucao nao fica presa ao preset");
  AE_EXPECT_EQ(policy.shadows.filterTaps, 25u, "kernel de sombra e independente");
  AE_EXPECT_EQ(policy.shadows.farFilterTaps, 9u, "kernel distante e independente");
  AE_EXPECT_EQ(policy.visibility.lodPixelErrorBudget, 1.25f, "erro de LOD e global");
  AE_EXPECT_EQ(policy.visibility.coverageLodPixelErrorBudget, 37.0f,
               "vegetacao possui budget de LOD independente");
  AE_EXPECT_EQ(policy.materialDistance.normalMapMaximumDistance, 180.0f,
               "detalhe material preserva alcance autoral configurado");
  AE_EXPECT_EQ(policy.materialDistance.metallicRoughnessMaximumDistance, 260.0f,
               "MR distante possui budget proprio");
  AE_EXPECT_TRUE(policy.post.fxaa && policy.post.dedicatedPass,
                 "filtro isolado ativa o passe sem trocar preset");
  AE_EXPECT_TRUE(policy.dynamicResolution.enabled, "resolucao dinamica e eixo global");
  AE_EXPECT_EQ(policy.dynamicResolution.minimumScale, 0.72f, "piso autoral preservado");
}

AE_TEST(policy_detalhe_material_tem_alcance_e_faixa_de_fade_independentes) {
  ProjectRenderingSettings settings{};
  settings.preset = QualityPreset::B;
  const auto inherited = resolveRenderingPolicy(settings, strongDevice(), ThermalPressure::None);
  AE_EXPECT_EQ(inherited.materialDistance.normalMapMaximumDistance, 60.0f,
               "perfil B preserva normal perto e remove trabalho distante");
  AE_EXPECT_EQ(inherited.materialDistance.specularProbeMaximumDistance, 120.0f,
               "IBL tem alcance independente");
  AE_EXPECT_EQ(inherited.materialDistance.fadeBandRatio, 0.20f,
               "detalhe desaparece gradualmente");

  settings.normalMapMaximumDistance = 180.0f;
  settings.specularProbeMaximumDistance = 320.0f;
  settings.materialDetailFadeBandRatio = 0.35f;
  const auto custom = resolveRenderingPolicy(settings, strongDevice(), ThermalPressure::None);
  AE_EXPECT_EQ(custom.materialDistance.normalMapMaximumDistance, 180.0f,
               "autor controla alcance de normal");
  AE_EXPECT_EQ(custom.materialDistance.specularProbeMaximumDistance, 320.0f,
               "autor controla alcance de IBL");
  AE_EXPECT_EQ(custom.materialDistance.fadeBandRatio, 0.35f,
               "autor controla a faixa de transicao");
}

AE_TEST(policy_lod_de_coverage_herda_por_perfil_e_tem_limite_seguro) {
  ProjectRenderingSettings settings{};
  settings.preset = QualityPreset::A;
  const auto inherited = resolveRenderingPolicy(settings, strongDevice(), ThermalPressure::None);
  AE_EXPECT_EQ(inherited.visibility.coverageLodPixelErrorBudget, 32.0f,
               "perfil A herda budget de coverage sem afetar malha solida");
  AE_EXPECT_EQ(inherited.visibility.lodPixelErrorBudget, 1.5f,
               "budget solido permanece independente");

  settings.coverageLodPixelErrorBudget = 1000.0f;
  const auto clamped = resolveRenderingPolicy(settings, strongDevice(), ThermalPressure::None);
  AE_EXPECT_EQ(clamped.visibility.coverageLodPixelErrorBudget, 128.0f,
               "override extremo e limitado pela API global");
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

AE_TEST(policy_perfil_b_tem_budget_de_sombra_mobile_sem_limitar_overrides) {
  auto capabilities = strongDevice();
  capabilities.profile = rhi::DeviceProfile::B;
  const auto automatic = resolveRenderingPolicy({}, capabilities, ThermalPressure::None);
  AE_EXPECT_EQ(automatic.shadows.cascadeCount, 2u, "perfil B financia duas cascatas");
  AE_EXPECT_EQ(automatic.shadows.cascadeResolution, 1024u, "perfil B usa atlas mobile");
  AE_EXPECT_EQ(automatic.shadows.filterTaps, 9u, "perfil B preserva PCF 3x3");
  AE_EXPECT_EQ(automatic.shadows.farFilterTaps, 1u,
               "perfil B nao paga PCF onde o kernel nao e perceptivel");
  AE_EXPECT_TRUE(automatic.shadows.staticCasterCache, "perfil B cacheia casters estaticos");
  AE_EXPECT_EQ(automatic.shadows.cacheGuardBandRatio, 1.08f,
               "margem do cache e um budget independente");
  // `strongDevice()` declara painel de 120 Hz, e o ponto do preset B foi
  // autorado para 60. A cadencia pedida e que decide: em 60 Hz o perfil B
  // preserva a resolucao nativa e o piso do preset; em 120 Hz nao cabe, e a
  // regra de cadencia alta assume (ver os testes policy_120hz_* no fim do
  // arquivo). Fixar o caso de 60 Hz aqui mantem a asercao original honesta.
  auto sixtyHz = capabilities;
  sixtyHz.displayHz = 60.0f;
  const auto atSixty = resolveRenderingPolicy({}, sixtyHz, ThermalPressure::None);
  AE_EXPECT_TRUE(!atSixty.dynamicResolution.enabled,
                 "perfil B preserva resolucao nativa por default em 60 Hz");
  AE_EXPECT_EQ(atSixty.dynamicResolution.minimumScale, 0.58f,
               "piso dinamico continua configuravel");
  AE_EXPECT_TRUE(automatic.dynamicResolution.enabled,
                 "em 120 Hz o mesmo perfil precisa da escala interna variavel");
  AE_EXPECT_EQ(automatic.post.sharpen, 0.0f,
               "resolucao dinamica nao liga sharpen sem decisao autoral");

  ProjectRenderingSettings custom{};
  custom.preset = QualityPreset::B;
  custom.shadowCascadeCount = 4;
  custom.shadowCascadeResolution = 2048;
  custom.staticShadowCache = FeatureOverride::Disabled;
  custom.shadowCacheGuardBandRatio = 1.20f;
  const auto overridden = resolveRenderingPolicy(custom, capabilities, ThermalPressure::None);
  AE_EXPECT_EQ(overridden.shadows.cascadeCount, 4u, "autor pode elevar cascatas independentemente");
  AE_EXPECT_EQ(overridden.shadows.cascadeResolution, 2048u,
               "autor pode elevar resolucao independentemente");
  AE_EXPECT_TRUE(!overridden.shadows.staticCasterCache, "autor pode desligar cache independentemente");
  AE_EXPECT_EQ(overridden.shadows.cacheGuardBandRatio, 1.20f,
               "autor controla a margem do cache sem novo preset");
}

AE_TEST(policy_capability_reduz_mas_nunca_eleva) {
  ProjectRenderingSettings settings{};
  settings.preset = QualityPreset::S;
  auto capabilities = strongDevice();
  capabilities.maximumImage2DSize = 1024;
  const auto policy = resolveRenderingPolicy(settings, capabilities, ThermalPressure::None);
  AE_EXPECT_EQ(policy.shadows.cascadeResolution, 512u,
               "atlas 2x2 limita cada cascata ao tamanho suportado");
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

AE_TEST(material_distance_so_reduz_quando_bounds_inteiro_esta_distante) {
  const float camera[3]{0.0f, 0.0f, 0.0f};
  const float centerNearBoundary[3]{65.0f, 0.0f, 0.0f};
  const float centerPastBoundary[3]{71.0f, 0.0f, 0.0f};
  AE_EXPECT_TRUE(!boundsEntirelyPastDistance(camera, centerNearBoundary, 10.0f, 60.0f),
                 "esfera que ainda toca a faixa conserva qualidade completa");
  AE_EXPECT_TRUE(boundsEntirelyPastDistance(camera, centerPastBoundary, 10.0f, 60.0f),
                 "esfera totalmente distante aceita variante reduzida");
  AE_EXPECT_TRUE(!boundsEntirelyPastDistance(camera, centerPastBoundary, 10.0f, 0.0f),
                 "distancia desligada nunca reduz material");
}

AE_TEST(material_distance_invalida_falha_para_qualidade_completa) {
  const float camera[3]{0.0f, 0.0f, 0.0f};
  const float center[3]{100.0f, 0.0f, 0.0f};
  const float invalidCenter[3]{NAN, 0.0f, 0.0f};
  AE_EXPECT_TRUE(!boundsEntirelyPastDistance(nullptr, center, 1.0f, 60.0f),
                 "camera nula falha conservadoramente");
  AE_EXPECT_TRUE(!boundsEntirelyPastDistance(camera, invalidCenter, 1.0f, 60.0f),
                 "bounds nao finito falha conservadoramente");
  AE_EXPECT_TRUE(!boundsEntirelyPastDistance(camera, center, -1.0f, 60.0f),
                 "raio invalido falha conservadoramente");
}

// --- cadência alta -----------------------------------------------------------
//
// Os pontos de preset descrevem o que cada perfil sustenta em 60 Hz. Pedir
// 120 Hz corta o orçamento pela metade sem mudar o preset, e no perfil B isso
// media 84,7 FPS em escala nativa contra 111,1 FPS com o piso da escala interna
// em 0,50 (docs/ORCAMENTO-120HZ.md §9). Estes testes protegem a regra que
// converte o pedido de cadência em configuração, e o limite dela.

namespace {
RenderingCapabilities midDevice(float displayHz) {
  RenderingCapabilities capabilities{};
  capabilities.profile = rhi::DeviceProfile::B;
  capabilities.maximumImage2DSize = 8192;
  capabilities.maximumImageArrayLayers = 256;
  capabilities.supportsDepthSampling = true;
  capabilities.maximumSamplerAnisotropy = 16.0f;
  capabilities.displayHz = displayHz;
  return capabilities;
}
}

AE_TEST(policy_120hz_liga_resolucao_dinamica_que_o_preset_de_60hz_deixava_desligada) {
  const auto sessenta = resolveRenderingPolicy({}, midDevice(60.0f), ThermalPressure::None);
  AE_EXPECT_EQ(sessenta.frame.renderHz, 60u, "60 Hz de painel resolve 60 Hz");
  AE_EXPECT_TRUE(!sessenta.dynamicResolution.enabled,
                 "em 60 Hz o perfil B mantem a escala nativa, como o preset pede");

  const auto centoEVinte = resolveRenderingPolicy({}, midDevice(120.0f), ThermalPressure::None);
  AE_EXPECT_EQ(centoEVinte.frame.renderHz, 120u, "120 Hz de painel resolve 120 Hz");
  AE_EXPECT_TRUE(centoEVinte.dynamicResolution.enabled,
                 "120 Hz nao cabe no preset de 60 Hz sem resolucao dinamica");
  AE_EXPECT_TRUE(hasClamp(centoEVinte, "dynamicResolution.enabled", PolicyClamp::Budget),
                 "a mudanca aparece no relatorio de perfil, com motivo");
  AE_EXPECT_TRUE(centoEVinte.post.dedicatedPass,
                 "escala interna variavel exige o passe de upscale");
}

AE_TEST(policy_piso_da_escala_interna_acompanha_a_cadencia_ate_o_limite_do_controlador) {
  const auto noventa = resolveRenderingPolicy({}, midDevice(90.0f), ThermalPressure::None);
  const auto centoEVinte = resolveRenderingPolicy({}, midDevice(120.0f), ThermalPressure::None);
  AE_EXPECT_EQ(noventa.frame.renderHz, 90u, "90 Hz resolve 90 Hz");
  // Preset B tem piso 0,58 em 60 Hz; 90 Hz fica na metade do caminho ate 0,50.
  AE_EXPECT_TRUE(noventa.dynamicResolution.minimumScale > 0.53f &&
                     noventa.dynamicResolution.minimumScale < 0.55f,
                 "90 Hz interpola entre o piso do preset e o limite do controlador");
  AE_EXPECT_TRUE(centoEVinte.dynamicResolution.minimumScale <= DynamicResolutionFloor + 1.0e-4f,
                 "120 Hz chega ao limite do controlador");
  AE_EXPECT_TRUE(centoEVinte.dynamicResolution.minimumScale >= DynamicResolutionFloor,
                 "e nunca passa dele: abaixo de 0,50 o upscale vira borrao");
  AE_EXPECT_TRUE(hasClamp(centoEVinte, "dynamicResolution.minimumScale", PolicyClamp::Budget),
                 "baixar o piso e degradacao registrada, nao efeito colateral");
}

AE_TEST(policy_cadencia_alta_nao_atropela_a_escolha_explicita_do_autor) {
  ProjectRenderingSettings settings{};
  settings.dynamicResolution = FeatureOverride::Disabled;
  settings.dynamicResolutionMinimumScale = 0.9f;
  const auto policy = resolveRenderingPolicy(settings, midDevice(120.0f), ThermalPressure::None);
  AE_EXPECT_TRUE(!policy.dynamicResolution.enabled,
                 "quem desliga explicitamente continua desligado em 120 Hz");
  AE_EXPECT_TRUE(policy.dynamicResolution.minimumScale > 0.89f &&
                     policy.dynamicResolution.minimumScale < 0.91f,
                 "piso explicito do autor nao e reduzido pela cadencia");
  AE_EXPECT_TRUE(!hasClamp(policy, "dynamicResolution.minimumScale", PolicyClamp::Budget),
                 "sem mudanca, sem registro de degradacao");
}

AE_TEST(policy_perfil_que_ja_pede_resolucao_dinamica_nao_registra_clamp_por_cadencia) {
  // O perfil C ja liga resolucao dinamica no proprio preset: em 120 Hz o piso
  // desce, mas ligar nao e uma degradacao nova e nao pode aparecer como tal.
  auto capabilities = midDevice(120.0f);
  capabilities.profile = rhi::DeviceProfile::C;
  const auto policy = resolveRenderingPolicy({}, capabilities, ThermalPressure::None);
  AE_EXPECT_TRUE(policy.dynamicResolution.enabled, "perfil C ja usa resolucao dinamica");
  AE_EXPECT_TRUE(!hasClamp(policy, "dynamicResolution.enabled", PolicyClamp::Budget),
                 "nao registra como degradacao o que o preset ja pedia");
}

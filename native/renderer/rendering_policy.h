// Política global de renderização — o núcleo de qualidade previsto na ADR-014.
//
// A ADR decidiu que "o frame inteiro consumirá uma única `ResolvedRenderingPolicy`,
// imutável durante uma época de configuração", resolvida a partir de quatro entradas
// separadas, e que os presets definem "budgets de cadência, visibilidade, LOD por erro
// projetado, sombras, GI/AO, pós, streaming, memória, uploads e trabalho assíncrono".
// Até aqui só a fatia de cadência existia. Este módulo entrega o resto.
//
// Três regras estruturais que este arquivo existe para tornar impossíveis de violar:
//
// 1. **O renderer nunca lê o nome do preset.** Ele lê eixos resolvidos. Um preset é um
//    ponto no espaço de configuração, não um caminho de código: `if (preset == Alta)`
//    dentro do renderer é o mesmo defeito que `if (cena == dirt-road)`, que a seção 8
//    do plano de otimização proíbe. Por isso `QualityPreset` não aparece em nenhum
//    campo de `ResolvedRenderingPolicy`.
//
// 2. **Todo eixo é independente e sobrescrevível.** O autor escolhe um preset e pode
//    divergir em qualquer eixo isolado. São muitas configurações, não três nomes.
//
// 3. **Toda degradação é auditável.** Quando o valor resolvido difere do pedido, a
//    política registra o eixo e o motivo (capability, budget ou térmica). Sem isso, um
//    aparelho que silenciosamente desliga sombras vira "bug de iluminação" para o
//    usuário e para quem lê o relatório de perfil.
//
// Lógica pura: sem Vulkan, sem Android, sem I/O. Testável integralmente no host.
#pragma once

#include "core/base.h"
#include "core/frame_policy.h"
#include "rhi/device_profile.h"

namespace ae::renderer {

// ---------------------------------------------------------------------------
// Eixos de qualidade — o vocabulário que o autor escolhe.
// ---------------------------------------------------------------------------
//
// `Inherit` é sempre o valor 0 e significa "use o que o preset disser". Existe como
// sentinela dentro do próprio enum, e não como um `optional` ao lado, porque estes
// valores são serializados no projeto: um sentinela sobrevive ao round-trip binário e
// textual sem um campo de presença paralelo que pode dessincronizar.

enum class ShadowQuality : u32 {
  Inherit = 0,
  // Sem mapa de sombra. O sol continua iluminando por N·L — o que desaparece é a
  // oclusão entre objetos, não a direcionalidade.
  Off,
  // Uma cascata, sem filtro: silhueta dura e serrilhada, custo mínimo.
  Hard,
  // Cascatas múltiplas com PCF 3×3. O alvo do perfil médio.
  Soft,
  // Cascatas múltiplas em alta resolução com PCF 5×5.
  UltraSoft,
};

enum class AmbientQuality : u32 {
  Inherit = 0,
  // Irradiância constante — a cena inteira recebe o mesmo ambiente.
  Constant,
  // Hemisfério céu/chão parametrizado por AEEN. É o que a engine faz hoje.
  Hemispheric,
  // Hemisfério + sonda especular pré-filtrada para reflexo direcional.
  HemisphericSpecular,
};

enum class PostQuality : u32 {
  Inherit = 0,
  // Sem passe de pós: tonemap acontece inline no shading, como hoje.
  None,
  // Passe dedicado com exposição e tonemap.
  Tonemap,
  // Tonemap + bloom.
  Bloom,
};

enum class TextureQuality : u32 {
  Inherit = 0,
  Half,   // um nível de mip a menos de residência
  Full,   // resolução autoral completa
};

// Preset é um ponto nomeado no espaço acima. `Auto` deriva do perfil de dispositivo
// detectado; os demais fixam o ponto independentemente do hardware, e a capability
// ainda pode reduzi-los (nunca elevá-los).
enum class QualityPreset : u32 { Auto = 0, C, B, A, S, Custom };

// ---------------------------------------------------------------------------
// Entrada 3 da ADR — escolha global serializada do projeto.
// ---------------------------------------------------------------------------
struct ProjectRenderingSettings final {
  QualityPreset preset = QualityPreset::Auto;
  // Overrides por eixo. `Inherit` em todos = o preset decide tudo.
  ShadowQuality shadows = ShadowQuality::Inherit;
  AmbientQuality ambient = AmbientQuality::Inherit;
  PostQuality post = PostQuality::Inherit;
  TextureQuality textures = TextureQuality::Inherit;
  // 0 = herda do preset. Fora disso, fração da resolução nativa em [0.5, 1.0].
  float resolutionScale = 0.0f;
  // Teto de cadência preferido pelo autor; 0 = herda a política de display.
  u32 maximumRenderHz = 0;
};

// ---------------------------------------------------------------------------
// Entrada 1 da ADR — fatos do dispositivo. Nunca inferência de velocidade.
// ---------------------------------------------------------------------------
struct RenderingCapabilities final {
  rhi::DeviceProfile profile = rhi::DeviceProfile::C;
  // Maior dimensão de imagem 2D suportada; limita a resolução de cascata.
  u32 maximumImage2DSize = 4096;
  // Camadas de array suportadas; cascatas moram num array de mapas de sombra.
  u32 maximumImageArrayLayers = 256;
  // `false` desliga sombras por completo em vez de produzir um mapa inválido.
  bool supportsDepthSampling = true;
  // Anisotropia máxima do sampler (1.0 = sem suporte).
  float maximumSamplerAnisotropy = 1.0f;
  float displayHz = 60.0f;
};

// ---------------------------------------------------------------------------
// Entrada 4 da ADR — pressão transitória. Piora imediata, recuperação com
// histerese, e jamais gravada no projeto.
// ---------------------------------------------------------------------------
enum class ThermalPressure : u32 {
  None = 0,   // Android THERMAL_STATUS_NONE
  Light,      // LIGHT / MODERATE
  Severe,     // SEVERE em diante
};

// ---------------------------------------------------------------------------
// Saída — eixos derivados, prontos para o renderer consumir.
// ---------------------------------------------------------------------------
struct ShadowSettings final {
  bool enabled = false;
  // Cascatas de um único mapa direcional. 0 quando desligado.
  u32 cascadeCount = 0;
  // Resolução por cascata, quadrada.
  u32 cascadeResolution = 0;
  // Amostras do filtro percentage-closer: 1 = duro, 9 = 3×3, 25 = 5×5.
  u32 filterTaps = 1;
  // Distância além da qual o sol deixa de projetar sombra, em unidades de mundo.
  float maximumDistance = 0.0f;
  // Bias em unidades de profundidade e em texels da cascata. O normal-offset é o
  // que remove acne sem produzir peter-panning, então acompanha o filtro.
  float depthBiasConstant = 0.0f;
  float depthBiasSlope = 0.0f;
  float normalOffsetTexels = 0.0f;
  // Ancorar a matriz da cascata a texels inteiros elimina o "nado" da borda de
  // sombra quando a câmera se move. Custa uma quantização por cascata por frame.
  bool stabilizeTexelSnap = true;
};

struct AmbientSettings final {
  bool hemispheric = false;
  bool specularProbe = false;
};

struct PostSettings final {
  // Um passe dedicado de pós, em vez do tonemap inline no shading.
  bool dedicatedPass = false;
  bool bloom = false;
  float bloomThreshold = 1.0f;
  float bloomIntensity = 0.0f;
};

struct TextureSettings final {
  // Deslocamento de mip aplicado na residência: 0 = resolução autoral.
  u32 residencyMipBias = 0;
  float samplerAnisotropy = 1.0f;
};

// Por que um eixo resolvido difere do pedido. A ADR exige "motivo registrado".
enum class PolicyClamp : u32 {
  None = 0,
  Capability, // o dispositivo não suporta o que foi pedido
  Budget,     // o orçamento de frame não comporta
  Thermal,    // pressão térmica transitória
};

struct PolicyClampRecord final {
  // Literal estável, propriedade do módulo; nunca alocado.
  const char *axis = "";
  PolicyClamp reason = PolicyClamp::None;
};

// Capacidade fixa: o número de eixos é conhecido em tempo de compilação e a
// resolução acontece no caminho quente de reconfiguração, sem heap.
constexpr u32 MaximumPolicyClamps = 8;

struct ResolvedRenderingPolicy final {
  FrameBudget frame{};
  VisibilityBudget visibility{};
  ShadowSettings shadows{};
  AmbientSettings ambient{};
  PostSettings post{};
  TextureSettings textures{};
  float resolutionScale = 1.0f;

  // Perfil efetivamente usado para derivar os padrões. Existe para o relatório de
  // perfil e para o Inspector; o renderer não deve ramificar nele.
  rhi::DeviceProfile effectiveProfile = rhi::DeviceProfile::C;

  PolicyClampRecord clamps[MaximumPolicyClamps]{};
  u32 clampCount = 0;

  bool clamped() const { return clampCount != 0; }
};

// --------------------------------------------------------------------------
// Parsing dos nomes usados por configuração serializada e por opção de
// lançamento. Fica aqui, e não no shell Android, porque o vocabulário do preset
// pertence à política: o editor, o projeto e a ferramenta de captura precisam
// concordar sobre o que "a" ou "soft" significam.
//
// Nome desconhecido devolve `Inherit`/`Auto` em vez de abortar: configuração é
// conteúdo de usuário, e conteúdo inválido nunca pode derrubar a engine.
// --------------------------------------------------------------------------
QualityPreset parseQualityPreset(const char *name);
ShadowQuality parseShadowQuality(const char *name);
AmbientQuality parseAmbientQuality(const char *name);
PostQuality parsePostQuality(const char *name);
TextureQuality parseTextureQuality(const char *name);

const char *shadowQualityName(ShadowQuality quality);
const char *ambientQualityName(AmbientQuality quality);
const char *postQualityName(PostQuality quality);
const char *textureQualityName(TextureQuality quality);

// Resolve a política. Determinística e sem estado: as mesmas quatro entradas
// produzem sempre a mesma saída, que é o que permite reproduzir uma captura.
ResolvedRenderingPolicy resolveRenderingPolicy(const ProjectRenderingSettings &settings,
                                               const RenderingCapabilities &capabilities,
                                               ThermalPressure thermal);

// Nome estável do preset para relatório/serialização. Nunca usado para ramificar
// comportamento de renderização.
const char *qualityPresetName(QualityPreset preset);
const char *policyClampName(PolicyClamp clamp);

} // namespace ae::renderer

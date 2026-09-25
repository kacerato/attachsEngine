#pragma once
#include "core/base.h"
#include "renderer/authoring_texture.h"
#include "renderer/map_package.h"
#include <span>
#include <vector>

namespace ae::renderer {
// Streaming de mipmaps (S2) — o Texture Mipmap Streaming da Unity 6.0: cada
// textura fica residente só a partir do mip que o maior uso na tela pede, o
// conjunto cabe num orçamento de memória com prioridade, e as trocas por quadro
// têm teto de bytes enviados.
//
// Vocabulário da Unity mantido de propósito: `calculatedMip` é o
// `desiredMipmapLevel`, `targetMip` o nível depois do orçamento, o `loaded` do
// chamador o `loadedMipmapLevel`, `requestedMip` o `requestedMipmapLevel`;
// `maxLevelReduction` e `budgetBytes` são os campos homônimos do Quality.
//
// Adaptação explícita: a Unity lê os mips do disco; aqui a fonte é a cadeia já
// preparada na memória (`AuthoringTexture`), e o que o streaming economiza é a
// residência na GPU. O plano não depende de Vulkan e é o mesmo no host.
inline constexpr u32 TextureStreamingNoRequest = ~0u;
inline constexpr u32 TextureStreamingNotLoaded = ~0u;

struct TextureStreamingTexture {
  u32 width = 0, height = 0, levels = 0, format = AuthoringTextureRgba8;
  // "Stream Mipmap Levels" do importador. Desligada, a textura fica inteira
  // (a partir do mip mínimo da qualidade) e conta como memória sem streaming.
  bool streamable = true;
  // Mesma faixa da Unity (-128..127): maior prioridade perde níveis por último.
  i32 priority = 0;
  // Nível forçado por script; `TextureStreamingNoRequest` segue a câmera.
  u32 requestedMip = TextureStreamingNoRequest;
};

// Escolhas autorais por textura publicada (Texture Importer da Unity): vêm do
// perfil da textura do projeto ou do perfil de importação da fonte.
struct TextureStreamingParameters {
  bool streamable = true;
  i32 priority = 0;
  u32 requestedMip = TextureStreamingNoRequest;
};

// Um desenho que amostra a textura: esfera no mundo e a escala da UV nele.
struct TextureStreamingUse {
  u32 texture = 0;
  float center[3]{};
  float radius = 0;
  // Metros de superfície por unidade de UV, já com a escala do objeto. Zero é
  // "desconhecido" (sem UV útil) e pede o detalhe máximo — nunca adivinhado.
  float metersPerUv = 0;
};

struct TextureStreamingView {
  float position[3]{};
  // Perspectiva: altura do alvo em pixels / (2·tan(fov/2)), ou seja pixels por
  // metro a um metro da câmera.
  float pixelsPerMeterAtUnitDistance = 0;
  // Ortográfica: pixels por metro, constantes com a distância. Tem precedência.
  float orthographicPixelsPerMeter = 0;
};

struct TextureStreamingSettings {
  u64 budgetBytes = 512ull << 20;
  u32 maxLevelReduction = 2;
  u64 uploadBytesPerFrame = 4ull << 20;
  // Limite global de mip da qualidade (Meia resolução = 1). O streaming conta
  // a redução a partir dele.
  u32 minimumMip = 0;
};

struct TextureStreamingPlan {
  std::vector<u32> calculatedMip; // pela tela (ou pedido), antes do orçamento
  std::vector<u32> targetMip;     // depois do orçamento
  std::vector<u32> loads;         // texturas a trocar neste quadro, em ordem
  u64 totalBytes = 0;             // cadeias completas (totalTextureMemory)
  u64 desiredBytes = 0;           // em `calculatedMip`
  u64 targetBytes = 0;            // em `targetMip`
  u64 currentBytes = 0;           // no nível carregado
  u64 nonStreamingBytes = 0;
  u64 uploadBytes = 0;            // enviados pelas trocas deste quadro
  u32 streamingTextures = 0, pendingLoads = 0, budgetReducedTextures = 0;
  // Nem a redução máxima coube: dito, não escondido.
  bool overBudget = false;
};

// O que o renderer relata a cada quadro: painel Qualidade, Inspector de
// textura e `Astra.Graphics` leem a mesma coisa (Texture.*TextureMemory da Unity).
struct TextureStreamingStats {
  bool active = false, overBudget = false;
  u64 budgetBytes = 0, totalBytes = 0, desiredBytes = 0, targetBytes = 0, currentBytes = 0, nonStreamingBytes = 0;
  u32 streamingTextures = 0, pendingLoads = 0, budgetReducedTextures = 0;
  u32 uploadsLastFrame = 0, failedUploads = 0;
  u64 uploadedBytesLastFrame = 0;
};

// Bytes da cauda da cadeia a partir de `baseMip` (0 se inválido).
u64 textureStreamingChainBytes(const TextureStreamingTexture &texture, u32 baseMip);
// Faixa permitida: [mínimo da qualidade, mínimo + redução máxima].
u32 textureStreamingFirstMip(const TextureStreamingTexture &texture, const TextureStreamingSettings &settings);
u32 textureStreamingLastMip(const TextureStreamingTexture &texture, const TextureStreamingSettings &settings);
// Mip pedido por um uso: log2 dos texels por pixel na tela, a `distance` metros.
u32 textureStreamingScreenMip(const TextureStreamingTexture &texture, float metersPerUv, float distance,
                              const TextureStreamingView &view);
// `loadedMip` tem uma entrada por textura (`TextureStreamingNotLoaded` quando
// não está na GPU). Falha fechada: tamanhos incoerentes ou uso fora da lista.
bool planTextureStreaming(std::span<const TextureStreamingTexture> textures,
                          std::span<const TextureStreamingUse> uses,
                          std::span<const TextureStreamingView> views,
                          const TextureStreamingSettings &settings, std::span<const u32> loadedMip,
                          TextureStreamingPlan &out);
// Métrica de distribuição de UV (Mesh.GetUVDistributionMetric da Unity, em
// raiz): metros de superfície por unidade de UV0 no espaço da malha. Zero sem
// UV com área.
float meshUvMetersPerUnit(std::span<const u8> vertices, std::span<const u32> indices, const MapDrawRecord &draw);
// Escala linear média de uma matriz coluna principal (raiz cúbica do determinante).
float textureStreamingModelScale(const float model[16]);
} // namespace ae::renderer

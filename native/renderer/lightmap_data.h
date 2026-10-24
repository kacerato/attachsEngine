#pragma once
// G3c: dados assados de lightmap (Lighting › Generate Lighting da Unity 6).
//
// Na Unity o Generate Lighting grava as texturas de lightmap e um LightingData
// por cena, e cada Mesh Renderer Static guarda `lightmapIndex` e
// `lightmapScaleOffset` (uvLightmap = uv2 * scale.xy + offset.zw). Aqui o recurso
// `.aelightmap` carrega as duas coisas juntas: as páginas e, por (objeto, slot)
// do Mesh Renderer, a página e a escala/deslocamento. A cena só guarda a
// referência ao recurso nas Configurações de iluminação; assar de novo troca o
// conteúdo sem reescrever cada objeto.
//
// O que cada texel guarda (compatível com o dia e noite, plano G3 item 8):
//  - RGB: irradiância E (RGB linear, cosseno já integrado) das luzes LOCAIS
//    assadas: a luz direta das luzes em modo Assado e os rebotes das luzes em
//    modo Assado e Misto, mais os emissivos. O sol da Hora do dia NUNCA entra:
//    ele continua direto e com sombra em tempo real.
//  - A: visibilidade do céu V = E_céu / π para um céu branco de radiância 1,
//    com os rebotes do próprio céu. O fragmento multiplica o ambiente do céu
//    DA HORA ATUAL por V; o lightmap não congela o meio-dia.
//
// Formatos de página: RGBA16F (sem compressão) ou RGBA8 com raiz quadrada e
// faixa por página (Compress Lightmaps): rgb = sqrt(E / faixa), a = sqrt(V).
//
// Referências: https://docs.unity3d.com/6000.0/Documentation/Manual/progressive-lightmapper.html
//              https://docs.unity3d.com/6000.0/Documentation/Manual/class-LightingSettings.html
//              https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Renderer-lightmapScaleOffset.html
//              https://docs.godotengine.org/en/4.5/tutorials/3d/global_illumination/using_lightmap_gi.html
#include "core/base.h"
#include "renderer/reflection_probes.h" // floatToHalf/halfToFloat do PK-6

#include <array>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace ae::renderer {

// Lightmapping Settings do Lighting Settings da Unity, na ordem da janela.
struct LightmapBakeSettings {
  // Lightmap Resolution: texels por unidade de mundo (Unity: 40).
  float texelsPerUnit = 40.0f;
  // Lightmap Padding: texels entre objetos na página (Unity: 2).
  u32 padding = 2;
  // Max Lightmap Size: lado máximo da página (Unity: 1024).
  u32 maxSize = 1024;
  // Direct/Indirect/Environment Samples do Progressive (Unity: 32/512/256).
  u32 directSamples = 32;
  u32 indirectSamples = 512;
  u32 environmentSamples = 256;
  // Bounces (Unity: 2). 0 deixa só a luz direta assada e o céu direto.
  u32 bounces = 2;
  // Filtering › Denoiser simples (à-trous guiado por normal e posição).
  bool denoise = true;
  // Compress Lightmaps: RGBA8 com raiz quadrada por página.
  bool compress = true;
  // Indirect Intensity e Albedo Boost (Unity: 1 e 1).
  float indirectIntensity = 1.0f;
  float albedoBoost = 1.0f;
  // Directional Mode: só Non-Directional (0) existe por enquanto.
  u32 directionalMode = 0;
  bool valid() const noexcept;
};
inline constexpr std::array<u32, 5> LightmapMaxSizeSteps{256, 512, 1024, 2048, 4096};
inline constexpr u32 MaximumLightmapPages = 64;

// Onde um slot de Mesh Renderer lê o lightmap.
struct LightmapRendererEntry {
  u64 objectId = 0;
  u32 slot = 0;
  u32 page = 0;
  float scaleOffset[4]{1, 1, 0, 0};
};

struct LightmapPage {
  static constexpr u32 FormatHalf = 0;      // RGBA16F
  static constexpr u32 FormatSqrtRgba8 = 1; // RGBA8, rgb = sqrt(E/faixa), a = sqrt(V)
  u32 width = 0, height = 0;
  u32 format = FormatHalf;
  float range = 1.0f; // só no formato comprimido
  std::vector<u8> texels;
  u64 expectedBytes() const noexcept { return u64(width) * height * (format == FormatHalf ? 8u : 4u); }
};

struct LightmapStats {
  u32 receivers = 0, contributors = 0, triangles = 0, lights = 0;
  u64 texels = 0, rays = 0;
  u32 rejectedReceivers = 0; // sem UV2 válida
  float occupancy = 0;       // fração das páginas coberta por objetos
  // Só do bake em memória (não vai ao arquivo): texels descartados por mais
  // da metade dos raios bater no verso de uma face.
  u32 invalidTexels = 0;
};

// O recurso .aelightmap.
struct LightmapData {
  static constexpr u32 Version = 1;
  static constexpr usize MaximumBytes = 512ull << 20;
  u64 guidHigh = 0, guidLow = 0;
  u64 sourceHash = 0;
  float bakeSeconds = 0;
  LightmapBakeSettings settings{};
  LightmapStats stats{};
  std::vector<LightmapPage> pages;
  std::vector<LightmapRendererEntry> entries;
  bool valid() const noexcept;
  std::vector<u8> serialize() const;
  static bool deserialize(std::span<const u8> bytes, LightmapData &out, std::string *error = nullptr);
  // Memória que as páginas ocupam na GPU (mesma forma que o upload).
  u64 gpuBytes() const noexcept;
};

// Consulta rápida (objeto, slot) -> entrada, montada uma vez por recurso.
class LightmapLookup {
public:
  explicit LightmapLookup(std::shared_ptr<const LightmapData> data);
  const LightmapRendererEntry *find(u64 objectId, u32 slot) const noexcept;
  const std::shared_ptr<const LightmapData> &data() const noexcept { return data_; }
private:
  std::shared_ptr<const LightmapData> data_;
  std::unordered_map<u64, u32> index_;
};

// Decodifica um texel de página (para testes, prévia e leitura da CPU).
void decodeLightmapTexel(const LightmapPage &page, u32 x, u32 y, float rgb[3], float &visibility) noexcept;

} // namespace ae::renderer

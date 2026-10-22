#pragma once
// PK-6: Reflection Probe como na Unity 6 (URP).
//
// Na Unity o autor cria GameObject › Light › Reflection Probe, ajusta a caixa
// de influência (Box Size/Offset) às paredes, escolhe Baked ou Realtime e
// clica Bake (ou deixa a sonda Realtime atualizar). Cada Mesh Renderer, pela
// opção Reflection Probes, mistura as duas sondas de maior importância que
// contêm o centro dos seus limites e cai no reflexo do céu fora delas. Box
// Projection corrige o reflexo para paredes a distância finita.
//
// Este arquivo é a parte de CPU, independente do backend: o registro que a
// extração produz, a escolha das sondas do quadro, a mistura por objeto, a
// geometria da box projection (o espelho em C++ do shader) e o recurso assado.
//
// Referências: https://docs.unity3d.com/6000.0/Documentation/Manual/class-ReflectionProbe.html
//              https://docs.unity3d.com/6000.0/Documentation/Manual/AdvancedRefProbe.html
//              https://docs.unity3d.com/6000.0/Documentation/Manual/RefProbeTypes.html
#include "core/base.h"
#include "resources/asset_registry.h"

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ae::renderer {

enum class ReflectionProbeType : u8 { Baked = 0, Realtime = 1 };
enum class ReflectionProbeRefresh : u8 { OnAwake = 0, EveryFrame = 1, ViaScripting = 2 };
enum class ReflectionProbeSlicing : u8 { AllFacesAtOnce = 0, IndividualFaces = 1, NoTimeSlicing = 2 };
// Mesh Renderer › Probes › Reflection Probes (os valores do enum da Unity).
enum class ReflectionProbeUsage : u8 { Off = 0, BlendProbes = 1, BlendProbesAndSkybox = 2, Simple = 3 };

// Uma sonda acesa, com pose de mundo. A caixa da Unity é alinhada aos eixos do
// mundo: a rotação do objeto não gira a caixa; a escala do objeto também não
// a escala (Box Size é em metros).
struct SceneReflectionProbe {
  u64 objectId = 0;
  u64 instance = 0;
  ReflectionProbeType type = ReflectionProbeType::Baked;
  ReflectionProbeRefresh refresh = ReflectionProbeRefresh::OnAwake;
  ReflectionProbeSlicing slicing = ReflectionProbeSlicing::AllFacesAtOnce;
  float position[3]{};   // ponto de captura (o pivô do objeto)
  float boxMin[3]{}, boxMax[3]{};
  float blendDistance = 1;
  i32 importance = 1;
  float intensity = 1;
  bool boxProjection = false;
  u32 resolution = 128;  // lado da face capturada
  bool hdr = true;
  float nearClip = .3f, farClip = 1000;
  u32 cullingMask = ~0u; // camadas de objeto que a captura desenha
  bool solidBackground = false;
  float background[3]{0.192f, 0.302f, 0.475f}; // linear
  resources::AssetGuid baked{};
  // Quantas vezes RenderProbe() foi pedido em Play; o renderer captura quando muda.
  u32 renderRequests = 0;
  // Hash da autoria que muda a imagem: posição, recorte, fundo, resolução.
  u64 captureKey = 0;
  bool contains(const float p[3]) const noexcept {
    for (u32 k = 0; k < 3; ++k) if (p[k] < boxMin[k] || p[k] > boxMax[k]) return false;
    return true;
  }
  float volume() const noexcept {
    return (boxMax[0] - boxMin[0]) * (boxMax[1] - boxMin[1]) * (boxMax[2] - boxMin[2]);
  }
};
u64 reflectionProbeCaptureKey(const SceneReflectionProbe &probe) noexcept;

// Orçamento: quantas sondas o fragmento conhece por quadro e quantas camadas
// tem o atlas. Oito cobre um interior com cômodos e a sala vizinha.
inline constexpr u32 MaximumFrameReflectionProbes = 8;
inline constexpr u32 ReflectionProbeAtlasSize = 256;   // lado do octaedro no mip 0
inline constexpr u32 ReflectionProbeAtlasMips = 6;     // 256..8
inline constexpr u32 ReflectionProbeNone = 15;         // "céu" na palavra do desenho

// Lado do octaedro e mip base que uma resolução de face ocupa no atlas: a face
// R vira um octaedro de 2R (o octaedro tem a mesma densidade angular no
// equador), limitado ao lado do atlas. Resolução menor começa num mip mais alto.
u32 reflectionProbeBaseMip(u32 resolution) noexcept;

// V-1: reflexo do ambiente pelo céu, como "Lighting › Environment Reflections ›
// Source: Skybox" da Unity e o reflexo do céu do HDRP. Sem HDRI, o reflexo que
// cada material lê (e que as sondas usam fora das caixas) é o próprio céu da
// cena capturado sem objetos, na resolução padrão da Unity, filtrado em GGX no
// octaedro do atlas. É refeito quando algo que muda a imagem do céu muda além
// da tolerância (um milésimo, relativo acima de 1): sol, cores, atmosfera,
// astros, nuvens, exposição. O vento das nuvens fica fora da chave.
// Referência: https://docs.unity3d.com/6000.0/Documentation/Manual/lighting-window.html
inline constexpr u32 SkyReflectionResolution = 128;
inline constexpr u32 SkyReflectionKeySize = 64;
bool skyReflectionNeedsCapture(std::span<const float> current, std::span<const float> captured,
                               bool captureValid) noexcept;

// Escolhe as sondas do quadro: primeiro as que contêm a câmera, depois por
// importância e distância ao centro da caixa, até `capacity`.
u32 selectFrameReflectionProbes(std::span<const SceneReflectionProbe> probes, const float camera[3],
                                std::span<u32> outIndices) noexcept;

// Mistura de um objeto, já resolvida: duas camadas do quadro e os pesos. O
// resto até 1 é o céu. A Unity faz isso por objeto, pelo centro dos limites.
struct ReflectionProbeBlend {
  u32 first = ReflectionProbeNone, second = ReflectionProbeNone;
  float firstWeight = 0, secondWeight = 0;
  float skyWeight() const noexcept { return 1.0f - firstWeight - secondWeight; }
};
// Peso de uma sonda num ponto: 1 dentro da caixa a mais de Blend Distance da
// borda, caindo a 0 na face da caixa. Fora da caixa, 0.
float reflectionProbeInfluence(const SceneReflectionProbe &probe, const float point[3]) noexcept;
// `frame` são as sondas aceitas do quadro (índice = camada da palavra).
ReflectionProbeBlend blendReflectionProbes(std::span<const SceneReflectionProbe> frame, const float center[3],
                                           ReflectionProbeUsage usage) noexcept;
// Palavra por desenho: bits 0..3 primeira camada, 4..7 segunda, 8..15 peso da
// primeira e 16..23 da segunda em 1/255. Zero de pesos é só o céu.
u32 encodeReflectionProbeBlend(const ReflectionProbeBlend &blend) noexcept;
ReflectionProbeBlend decodeReflectionProbeBlend(u32 word) noexcept;

// Box projection: direção de amostragem corrigida para um ponto `position` que
// reflete na direção `direction` dentro da caixa capturada em `capture`. O
// shader faz exatamente esta conta; fora da caixa (ou sem box projection) a
// direção volta intacta, que é o reflexo no infinito da Unity.
void boxProjectReflection(const float position[3], const float direction[3], const float boxMin[3],
                          const float boxMax[3], const float capture[3], float out[3]) noexcept;

// Projeção octaédrica do atlas (a mesma do mapa especular do ambiente).
void octahedralEncode(const float direction[3], float uv[2]) noexcept;
void octahedralDecode(const float uv[2], float direction[3]) noexcept;

// Recurso assado (.aeprobe): a cadeia de mips octaédrica de uma sonda Baked,
// gravada no projeto como a Unity grava o cubemap EXR ao lado da cena.
// Formato binário: "AERP", versão, lado, mip base, mips, HDR, depois os texels
// de cada mip a partir do mip base — RGBA16F se HDR, RGBA8 linear se não.
struct ReflectionProbeTexture {
  static constexpr u32 Version = 1;
  static constexpr usize MaximumBytes = 4u << 20;
  resources::AssetGuid guid{};
  // reflectionProbeCaptureKey da sonda no bake: muda quando posição, recorte ou
  // fundo mudam, e o Inspector avisa que o bake ficou velho.
  u64 sourceHash = 0;
  float bakeSeconds = 0;
  u32 size = ReflectionProbeAtlasSize; // lado do mip 0 do atlas
  u32 baseMip = 0;
  u32 mips = ReflectionProbeAtlasMips;
  bool hdr = true;
  // Meia precisão RGBA por texel, do mip base ao último, como o atlas guarda.
  std::vector<u16> texels;
  u32 mipSide(u32 mip) const noexcept { return std::max<u32>(size >> mip, 1u); }
  usize expectedTexels() const noexcept;
  bool valid(std::string *error = nullptr) const;
  std::vector<u8> serialize() const;
  static bool deserialize(std::span<const u8> bytes, ReflectionProbeTexture &out, std::string *error = nullptr);
};
u16 floatToHalf(float value) noexcept;
float halfToFloat(u16 value) noexcept;

} // namespace ae::renderer

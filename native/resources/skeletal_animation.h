#pragma once
// Skin e animação por nós (G6-B), no contrato do glTF 2.0.
//
// Dados que a importação extrai e que o editor, o runtime e o renderer
// consomem sem conhecer JSON. Nada aqui conhece cena nem Vulkan:
//
// * `AnimationClip`: canais por nó (translação, rotação, escala), cada um com
//   os tempos e os valores do sampler e a interpolação STEP, LINEAR (slerp na
//   rotação) ou CUBICSPLINE (Hermite com tangentes de entrada/saída, §3.11 da
//   especificação). Pesos de morph não entram: são contados como não suportados.
// * `SkinDefinition`: juntas (nós emitidos pela importação), matrizes de bind
//   inversas e, por junta, a esfera dos vértices que ela influencia no espaço
//   da própria junta -- os limites do corpo deformado saem daí sem varrer
//   vértices por quadro.
// * Influências por vértice: 4 juntas u16 + 4 pesos unorm16 (16 bytes),
//   paralelas aos vértices do pacote. Mais de 4 influências no arquivo viram
//   as 4 maiores, renormalizadas (o mesmo que a Unity faz com "Skin Weights: 4").
// * `MorphTargetSet`: blend shapes de um desenho (morph targets do glTF). Por
//   alvo e por vértice, o deslocamento de posição, normal e tangente; pesos
//   padrão da malha e nomes (`extras.targetNames`). O canal `weights` dos clipes
//   anima os pesos; a deformação aplica morph ANTES do skin, como a especificação.
//
// Paleta: para um desenho cujo modelo é M (mundo do nó da malha), a junta j
// contribui com inversa(M) * mundo(j) * bindInversa(j). O vértice deformado,
// ainda no espaço do desenho, é soma(peso * paleta * posição); a matriz de
// modelo do desenho continua sendo a do nó, e o resultado no mundo é o que o
// glTF manda (a pose do nó da malha é ignorada pelo skin).
#include "core/base.h"
#include "resources/asset_registry.h"
#include "resources/animation_curve.h"

#include <span>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace ae::resources {

inline constexpr u32 SkinInfluenceStride = 16;
inline constexpr u32 MaximumSkinJoints = 256;
// Posição, normal e tangente (xyz) por alvo e por vértice, em float.
inline constexpr u32 MorphDeltaStride = 9;
inline constexpr u32 MaximumMorphTargets = 64;

enum class AnimationPath : u8 { Translation = 0, Rotation = 1, Scale = 2, Weights = 3 };
enum class AnimationInterpolation : u8 { Step = 0, Linear = 1, CubicSpline = 2 };
enum class AnimationRotationMode : u8 { Quaternion, Euler, ProgressiveQuaternion };
inline constexpr u32 rotationCurveComponents(AnimationRotationMode mode) {
  return mode==AnimationRotationMode::Euler?3u:mode==AnimationRotationMode::ProgressiveQuaternion?5u:4u;
}

struct AnimationChannel {
  u32 node = 0; // índice em GltfImport::nodes
  AnimationPath path = AnimationPath::Translation;
  AnimationInterpolation interpolation = AnimationInterpolation::Linear;
  std::vector<float> times;
  // 3, 4 ou `weightCount` componentes por chave; CUBICSPLINE guarda (tangente de
  // entrada, valor, tangente de saída) por chave, na ordem do glTF.
  std::vector<float> values;
  // Só para `Weights`: quantos alvos de morph a malha do nó tem.
  u32 weightCount = 0;
  // Editable clip channels use independently timed component curves. Imported
  // glTF channels keep their original samplers. Exactly one representation is
  // present; authored metadata cannot be silently written to a glTF cache.
  std::vector<AnimationCurve> curves;
  AnimationRotationMode rotationMode=AnimationRotationMode::Quaternion;
  // Compiled authored composition: one flat source per participating layer,
  // base first. Imported channels never contain this representation.
  std::vector<AnimationChannel> layerSources;
  float layerWeight=1,layerReferenceTime=-1;
  bool layerAdditive=false;
  u32 curveComponents() const noexcept {return path==AnimationPath::Rotation?rotationCurveComponents(rotationMode):components();}
  u32 components() const noexcept {
    return path == AnimationPath::Rotation ? 4u : path == AnimationPath::Weights ? weightCount : 3u;
  }
};

enum class AnimationCueKind : u32 { Marker, Event };
struct AnimationCue {
  u64 id=0;
  float time=0;
  AnimationCueKind kind=AnimationCueKind::Marker;
  std::string name;
  u32 tag=0;
  double value=0;
  // Local direction, including the returning leg of ping-pong.
  bool forward=true,reverse=true,enabled=true;
};
inline constexpr usize MaximumAnimationCues=4096;
bool validAnimationCues(std::span<const AnimationCue> cues,float duration);
struct AnimationClip {
  std::string name;
  float duration = 0;
  std::vector<AnimationChannel> channels;
  std::vector<AnimationCue> cues;
};

// Blend shapes de um desenho. `deltas` tem `vertexCount * targetCount *
// MorphDeltaStride` floats, VÉRTICE a vértice (cada vértice traz todos os alvos:
// é a ordem que o compute lê e a que cresce quando a importação duplica um
// vértice numa aresta dura); `maximumDisplacement` é o maior
// deslocamento de posição de cada alvo (limites conservadores sem varrer
// vértices por quadro).
struct MorphTargetSet {
  u32 targetCount = 0, vertexCount = 0;
  std::vector<float> deltas;
  std::vector<float> defaultWeights;
  std::vector<std::string> names;
  std::vector<float> maximumDisplacement;
};
bool validMorphTargetSet(const MorphTargetSet &set);

struct SkinDefinition {
  std::string name;
  std::vector<u32> joints;         // índices em GltfImport::nodes
  std::vector<float> inverseBind;  // 16 por junta, coluna principal
  std::vector<float> jointSpheres; // 4 por junta: centro no espaço da junta e raio; raio < 0 = sem vértices
  i32 skeleton = -1;
};

// Como o tempo do clipe corre depois do fim (WrapMode da Unity, Animation legado).
enum class AnimationWrapMode : u32 { Once = 0, Loop = 1, PingPong = 2, ClampForever = 3 };
// Half-open traversal: (from,to] forward, [to,from) backward. Seeking is not
// traversal. Bounded merge of periodic occurrences; returns suppressed count.
u64 visitAnimationCues(const AnimationClip &clip,double from,double to,AnimationWrapMode mode,
                      const std::function<void(const AnimationCue &)> &deliver,u32 budget=256,bool validated=false);

// Identidade persistente de um clipe: fonte + nome + ordem entre clipes de mesmo
// nome. Renomear o clipe no DCC é trocar de clipe; reordenar não.
AssetGuid animationClipGuid(const AssetGuid &source, std::string_view name, u32 ordinal);
// As identidades de todos os clipes de uma fonte, na ordem do arquivo.
std::vector<AssetGuid> animationClipGuids(const AssetGuid &source, std::span<const AnimationClip> clips);
// Nome de exibição de um clipe sem nome no arquivo.
std::string animationClipDisplayName(const AnimationClip &clip, u32 index);

// Tempo local amostrado e se a reprodução terminou (só Once).
float wrapAnimationTime(float time, float duration, AnimationWrapMode mode, bool &finished);

// Valor do canal no tempo `time` (segundos do clipe). Rotação sai normalizada.
// Falso quando o canal é inválido; `out` recebe 3 ou 4 componentes.
bool sampleAnimationChannel(const AnimationChannel &channel, float time, float out[4]);
// Forma geral (pesos de morph têm N componentes): `out` precisa de
// `channel.components()` floats.
bool sampleAnimationChannel(const AnimationChannel &channel, float time, std::span<float> out);
// Immutable channels validated when published. Avoid scanning every key on
// every frame; arbitrary API input must use the checked entry point above.
bool sampleValidatedAnimationChannel(const AnimationChannel &channel,float time,std::span<float> out);

// Consistência estrutural (tempos crescentes e finitos, valores do tamanho da
// interpolação). A importação recusa canais que falham aqui.
bool validAnimationChannel(const AnimationChannel &channel);

// Matriz local TRS (coluna principal) a partir de translação, quaternion (x,y,z,w) e escala.
void composeTransform(const float translation[3], const float rotation[4], const float scale[3], float out[16]);

// Paleta de uma instância: `jointWorlds` tem 16 floats por junta, na ordem do
// skin; `drawModel` é o modelo do desenho. Falso se alguma matriz for singular
// ou não finita (a instância mantém a paleta anterior).
bool computeSkinPalette(const SkinDefinition &skin, const float drawModel[16],
                        const std::vector<float> &jointWorlds, std::vector<float> &palette);

// Esfera que contém todos os vértices deformados, no espaço do desenho, a
// partir das esferas por junta e da paleta. Falso sem nenhuma junta com vértices.
bool skinnedLocalBounds(const SkinDefinition &skin, const std::vector<float> &palette,
                        float center[3], float &radius);

// Quanto os pesos atuais podem afastar um vértice da forma base: soma de
// |peso| x maior deslocamento de cada alvo. Soma-se ao raio dos limites.
float morphBoundsExpansion(const MorphTargetSet &set, std::span<const float> weights);

// Deformação na CPU, para seleção, enquadramento e colisor: os mesmos passos do
// compute (morph e depois skin, pesos renormalizados, `influenceLimit` 1/2/4).
// `positions` entra com a forma base (xyz por vértice) e sai deformada, no
// espaço do desenho. `influences` pode ser vazio (sem skin) e `palette` também.
bool deformPositions(std::span<float> positions, std::span<const u8> influences, std::span<const float> palette,
                     u32 influenceLimit, const MorphTargetSet *morph, std::span<const float> weights);

} // namespace ae::resources

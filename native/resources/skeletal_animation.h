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
//
// Paleta: para um desenho cujo modelo é M (mundo do nó da malha), a junta j
// contribui com inversa(M) * mundo(j) * bindInversa(j). O vértice deformado,
// ainda no espaço do desenho, é soma(peso * paleta * posição); a matriz de
// modelo do desenho continua sendo a do nó, e o resultado no mundo é o que o
// glTF manda (a pose do nó da malha é ignorada pelo skin).
#include "core/base.h"

#include <string>
#include <vector>

namespace ae::resources {

inline constexpr u32 SkinInfluenceStride = 16;
inline constexpr u32 MaximumSkinJoints = 256;

enum class AnimationPath : u8 { Translation = 0, Rotation = 1, Scale = 2 };
enum class AnimationInterpolation : u8 { Step = 0, Linear = 1, CubicSpline = 2 };

struct AnimationChannel {
  u32 node = 0; // índice em GltfImport::nodes
  AnimationPath path = AnimationPath::Translation;
  AnimationInterpolation interpolation = AnimationInterpolation::Linear;
  std::vector<float> times;
  // 3 ou 4 componentes por chave; CUBICSPLINE guarda (tangente de entrada,
  // valor, tangente de saída) por chave, na ordem do glTF.
  std::vector<float> values;
  u32 components() const noexcept { return path == AnimationPath::Rotation ? 4u : 3u; }
};

struct AnimationClip {
  std::string name;
  float duration = 0;
  std::vector<AnimationChannel> channels;
};

struct SkinDefinition {
  std::string name;
  std::vector<u32> joints;         // índices em GltfImport::nodes
  std::vector<float> inverseBind;  // 16 por junta, coluna principal
  std::vector<float> jointSpheres; // 4 por junta: centro no espaço da junta e raio; raio < 0 = sem vértices
  i32 skeleton = -1;
};

// Como o tempo do clipe corre depois do fim (WrapMode da Unity, Animation legado).
enum class AnimationWrapMode : u32 { Once = 0, Loop = 1, PingPong = 2, ClampForever = 3 };

// Tempo local amostrado e se a reprodução terminou (só Once).
float wrapAnimationTime(float time, float duration, AnimationWrapMode mode, bool &finished);

// Valor do canal no tempo `time` (segundos do clipe). Rotação sai normalizada.
// Falso quando o canal é inválido; `out` recebe 3 ou 4 componentes.
bool sampleAnimationChannel(const AnimationChannel &channel, float time, float out[4]);

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

} // namespace ae::resources

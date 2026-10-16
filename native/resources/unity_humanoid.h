#pragma once
// Clipes Humanoid da Unity (Mecanim) convertidos em rotações de osso do
// personagem.
//
// A Unity guarda o Humanoid em "espaço de músculos": cada osso humano tem até
// três músculos normalizados (torção e dois balanços, de -1 a 1 dentro do
// intervalo padrão) e o corpo vem de RootT/RootQ (centro de massa e orientação
// em escala humana). Não há API pública da conversão; ela segue a reconstrução
// do HumanPoseHandler do ShaderMotion (lox9973) e do Unidot Importer (Lyuma),
// Apache-2.0 (native/third_party/unity_humanoid/): pré/pós-rotações, sinais e
// limites num esqueleto humanoide de referência, com as poses de descanso do
// perfil humanoide da Godot 4.3 (MIT). O resultado é redirecionado para o
// esqueleto do personagem pela pose em T dele.
//
// Referências: Unity 6000.0 Manual, Humanoid Avatars / Muscle & Settings
// (https://docs.unity3d.com/6000.0/Documentation/Manual/MuscleDefinitions.html),
// Unidot humanoid/transform_util.gd (https://github.com/V-Sekai/unidot_importer),
// Godot SkeletonProfileHumanoid (scene/resources/skeleton_profile.cpp).
#include "core/base.h"
#include "resources/animation_clip_asset.h"
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace ae::resources {

inline constexpr u32 UnityHumanBoneCount=55,UnityHumanMuscleCount=95;

// Nome do osso humano como o .meta da Unity escreve em humanName (Hips,
// LeftUpperLeg, "Left Thumb Proximal", UpperChest...) e o índice dele.
std::string_view unityHumanBoneName(u32 bone);
i32 unityHumanBoneIndex(std::string_view humanName);

// O que um atributo de curva Humanoid (classID 95) endereça.
struct UnityHumanoidAttribute {
  enum class Kind : u8 {None,Muscle,RootT,RootQ,Ignored} kind=Kind::None;
  u32 bone=0,axis=0;
};
UnityHumanoidAttribute unityHumanoidAttribute(std::string_view attribute);

// Curvas Humanoid lidas do clipe: músculos por osso (eixo 0 torção, 1 e 2
// balanços) e o corpo. Curva ausente vale zero (a pose neutra da Unity).
struct UnityHumanoidCurves {
  std::array<std::array<AnimationCurve,3>,UnityHumanBoneCount> muscles{};
  std::array<std::array<bool,3>,UnityHumanBoneCount> present{};
  std::array<AnimationCurve,3> rootT{};
  std::array<AnimationCurve,4> rootQ{};
  bool hasRootT=false,hasRootQ=false;
  u32 muscleCurves=0;
  // Root Transform Rotation › Offset (m_OrientationOffsetY, graus em volta de Y
  // da Unity): gira o corpo inteiro, como a Unity gira a raiz ao reproduzir.
  double orientationOffsetY=0;
};

// O personagem, pais antes dos filhos; o nó 0 é o dono do clipe (caminho
// vazio). `human` é o osso humano do nó, ou -1. `translation/rotation/scale`
// são o descanso real (o bind, que a reprodução usa nos ossos não animados);
// `tpose*` é a pose em T do Avatar da Unity, a referência do redirecionamento
// (a Unity pode "forçar a pose em T" e ela difere do bind, como nos braços do
// Kyle). Sem Avatar, a pose em T é o próprio descanso.
struct UnityHumanoidTarget {
  struct Node {
    std::string path;
    i32 parent=-1,human=-1;
    float translation[3]{0,0,0},rotation[4]{0,0,0,1},scale[3]{1,1,1};
    bool hasTpose=false;
    float tposeTranslation[3]{0,0,0},tposeRotation[4]{0,0,0,1};
  };
  std::vector<Node> nodes;
};

struct UnityHumanoidReport {
  u32 bones=0,samples=0;
  std::vector<std::string> notes;
};

// Acrescenta ao clipe uma trilha de rotação por osso humano mapeado no alvo e
// a posição do quadril (corpo da Unity), amostradas em `sampleRate` de 0 até
// `duration`. Trilhas de Transform do mesmo nó já presentes no clipe ficam de
// fora, como a Unity ignora curvas genéricas de ossos humanos.
bool appendUnityHumanoidTracks(const UnityHumanoidCurves &curves,const UnityHumanoidTarget &target,
    float duration,u32 sampleRate,AnimationClipAsset &clip,UnityHumanoidReport &report,std::string &error);

// Pose de um quadro, para testes e diagnóstico: rotações locais do esqueleto de
// referência (Unidot/Godot) a partir dos músculos, e o quadril pelo corpo.
struct UnityHumanoidPose {
  std::array<std::array<double,4>,UnityHumanBoneCount> local{};  // x,y,z,w
  std::array<std::array<double,4>,UnityHumanBoneCount> global{};
  std::array<double,3> hips{0,1,0};
};
void unityHumanoidReferencePose(const std::array<std::array<float,3>,UnityHumanBoneCount> &muscles,
    const float rootT[3],const float rootQ[4],UnityHumanoidPose &out);
// O esqueleto de referência como alvo (pose de descanso do perfil humanoide,
// quadril a 1 m): redirecionar para ele devolve as rotações da referência.
UnityHumanoidTarget unityHumanoidReferenceSkeleton();

} // namespace ae::resources

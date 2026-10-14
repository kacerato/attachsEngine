#pragma once
#include "resources/animation_clip_asset.h"
#include "resources/unity_humanoid.h"
#include "resources/unity_yaml.h"

namespace ae::resources {
// The mesh's real, ordered morph names are required. Unity stores blendShape
// curves by name, while the native renderer consumes weight array indices.
struct UnityAnimationMorphBinding {
  std::string path;
  std::vector<std::string> names;
  std::vector<float> defaults;
};
struct UnityAnimationImport {
  AnimationClipAsset clip;
  AssetGuid unityGuid{};
  std::string sourceHash;
  i64 fileId=0;
  bool loop=false;
  std::vector<std::string> transformations;
};
// Generic Transform and named blendShape curves, plus Humanoid muscle curves
// when `humanoid` describes the target character (Avatar mapping and T-pose);
// without it a Humanoid clip fails with the reason. Compressed clips,
// object-reference curves, callbacks and Euler rotation orders without an
// exact native representation fail before publishing.
// Unity (mão esquerda) para a Astra (mão direita) espelha um eixo, e qual
// depende de onde o alvo veio: prefabs e cenas convertidos espelham Z; modelos
// importados de FBX chegam como a Unity os vê com X espelhado (a Unity espelha X
// ao importar o FBX), então curvas de Transform sobre eles espelham X.
enum class UnityHandedness : u8 {MirrorZ,MirrorX};
bool convertUnityAnimation(const UnityYamlDocument &document,AssetGuid nativeGuid,AssetGuid unityGuid,
    std::string_view sourceHash,std::span<const UnityAnimationMorphBinding> morphs,
    const UnityHumanoidTarget *humanoid,UnityAnimationImport &out,std::string &diagnostic,
    UnityHandedness handedness=UnityHandedness::MirrorZ);
} // namespace ae::resources

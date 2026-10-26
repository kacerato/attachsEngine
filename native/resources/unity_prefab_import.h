#pragma once
#include "resources/unity_particle_import.h"
#include "resources/unity_yaml.h"
#include <array>
#include <functional>
#include <optional>

namespace ae::resources {
// G6a: componentes da Unity com consumidor na Astra, lidos do prefab. Cada um
// guarda só o que a Astra executa; o resto vai para as perdas do prefab.
struct UnityPrefabLight {
  u32 type = 2;  // Unity LightType: 0 Spot, 1 Directional, 2 Point, 3 Area
  float color[3]{1, 1, 1};  // sRGB, como o seletor da Unity e a Luz da Astra
  float intensity = 1, range = 10, spotAngle = 30;
  u32 shadows = 0;  // 0 nenhuma, 1 dura, 2 suave
  bool enabled = true;
};
struct UnityPrefabAudio {
  UnityObjectReference clip;
  bool enabled = true, playOnAwake = true, loop = false, mute = false;
  float volume = 1, pitch = 1, minDistance = 1, maxDistance = 500, spatialBlend = 0, priority = 128;
  u32 rolloff = 0;  // Unity: 0 logarítmico, 1 linear, 2 personalizado
};
struct UnityPrefabBody {
  bool twoD = false, kinematic = false, gravity = true;
  float mass = 1, drag = 0, angularDrag = .05f, gravityScale = 1;
};
struct UnityPrefabCollider {
  enum class Shape : u8 { Box, Sphere, Capsule } shape = Shape::Box;
  bool twoD = false, trigger = false, enabled = true;
  float center[3]{}, size[3]{1, 1, 1}, radius = .5f, height = 2;
  u32 direction = 1;  // eixo da cápsula: 0 X, 1 Y, 2 Z
};
struct UnityPrefabLine {
  bool enabled = true, worldSpace = true, loop = false;
  std::vector<std::array<float, 3>> points;
  float widthStart = 1, widthEnd = 1;
  float colorStart[4]{1, 1, 1, 1}, colorEnd[4]{1, 1, 1, 1};
  bool tile = false;
  UnityObjectReference material;
};
// Scripts comuns de pacotes de VFX, reconhecidos pelo nome do .cs.
struct UnityPrefabProjectile {
  bool twoD = false, faceShooter = false;
  float speed = 15, hitOffset = 0, rotationOffset[3]{};
  UnityObjectReference hit, flash;  // GameObject de outro prefab
  std::vector<i64> detached;        // GameObjects locais
};
struct UnityPrefabCollisionSpawn {
  std::vector<UnityObjectReference> effects;
  float destroyDelay = 5, offset = 0, rotationOffset[3]{};
  bool worldSpace = false, rotationOffsetOnly = true, faceEmitter = false, destroyEmitter = false;
};
struct UnityPrefabEffectSound {
  bool repeating = true, randomVolume = false;
  float interval = 2, startDelay = 0, volumeMin = .4f, volumeMax = 1;
};
struct UnityPrefabLaser {
  i64 hitEffect = 0;  // GameObject local
  float hitOffset = 0, maxLength = 20, textureLength = 1;
  bool alignHit = false;
};

struct UnityPrefabObject {
  i64 fileId=0,transformId=0,parentTransform=0;
  std::string name,tag;
  std::array<float,3> position{},scale{1,1,1};
  std::array<float,4> rotation{0,0,0,1};
  bool active=true,renderer=false,rendererEnabled=true,receiveShadow=true;
  // ShadowCastingMode da Unity: 0 Off, 1 On, 2 TwoSided, 3 ShadowsOnly — os
  // mesmos valores do Projetar sombras do Mesh Renderer da Astra.
  u32 castShadows=1;
  // PK-6: Probes do Mesh Renderer (LightProbeUsage, ReflectionProbeUsage da Unity).
  u32 lightProbes=1,reflectionProbes=1;
  u32 layer=0;
  UnityObjectReference mesh;
  std::vector<UnityObjectReference> materials;
  // G6a. As referências locais do sistema (planos, sub-emissores, luz) já
  // estão traduzidas para fileID de GameObject.
  std::optional<UnityParticleImport> particle;
  std::optional<UnityPrefabLight> light;
  std::optional<UnityPrefabAudio> audio;
  std::optional<UnityPrefabBody> body;
  std::vector<UnityPrefabCollider> colliders;
  std::optional<UnityPrefabLine> line;
  std::optional<UnityPrefabProjectile> projectile;
  std::optional<UnityPrefabCollisionSpawn> collisionSpawn;
  std::optional<UnityPrefabEffectSound> effectSound;
  std::optional<UnityPrefabLaser> laser;
  // Girar: graus por segundo, já refletidos para a Astra.
  std::optional<std::array<float, 3>> rotator;
};
// Cobertura de um prefab (G6a, relatório por prefab). Suportado: nada ficou
// de fora. Parcial: abre, com perdas visíveis ou aproximações listadas.
struct UnityPrefabCoverage {
  u32 particleSystems = 0, lights = 0, audioSources = 0, lines = 0, scripts = 0, bodies = 0, colliders = 0;
  std::vector<std::string> losses, approximations;
};
struct UnityPrefabImport {
  i64 rootFileId=0;
  std::vector<UnityPrefabObject> objects;
  std::vector<std::string> transformations;
  UnityPrefabCoverage coverage;
};
// Quem chama conhece o pacote: o nome de um script pelo GUID do .cs.
struct UnityPrefabContext {
  std::function<std::string(const AssetGuid &)> scriptName;
};
// Prefab autônomo: GameObject/Transform, MeshFilter/MeshRenderer e, desde o
// G6a, Particle System + Renderer, Light, Audio Source, Rigidbody(2D),
// colisores primitivos, Line Renderer e os scripts de VFX reconhecidos.
// Variante/instância com patches continua recusada como um todo; componente
// sem consumidor vira perda listada (o prefab abre sem ele).
bool convertUnityPrefab(const UnityYamlDocument &,UnityPrefabImport &,std::string &diagnostic,const UnityPrefabContext *context=nullptr);
} // namespace ae::resources

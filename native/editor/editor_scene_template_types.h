#pragma once
// Tipos dos modelos de cena (ver editor_scene_template.h).
#include "core/base.h"

#include <span>

namespace ae::editor {

// 0 nenhuma, e os três tipos do componente Luz na ordem do contrato da cena.
enum class SceneTemplateLight : u8 { None = 0, Directional = 1, Point = 2, Spot = 3 };

struct SceneTemplateNode final {
  const char *name;
  // Índice do nó pai DENTRO do modelo; -1 pendura na raiz do modelo.
  i32 parent;
  float position[3];
  float rotationDegrees[3];
  float scale[3];
  // Instancia o cubo autoral da biblioteca. Um modelo de referência precisa
  // funcionar em projeto vazio, e o cubo é a única geometria que sempre existe.
  bool mesh;
  SceneTemplateLight light;
  // Lux para direcional, lúmen para pontual/spot — as unidades fotométricas do
  // componente Luz.
  float lightIntensity;
  float lightRange;
  float color[3];
  // Sombra suave nas luzes locais do modelo: o cenário de referência existe
  // para mostrar sombra de contato no interior, e luz sem sombra atravessa a mesa.
  bool shadows;
};

// Enquadramento que nasce com o modelo, para comparar as mesmas vistas depois
// de cada mudança visual.
struct SceneTemplateView final {
  const char *name;
  float target[3];
  float distance, yaw, pitch;
};

// Volume de Ambiente que nasce com o modelo. Luz fotométrica exige exposição
// coerente: o sol de 100.000 lux e a lâmpada de 1.200 lúmen diferem uns 12 EV,
// como na vida real, e uma exposição só estoura um lado ou apaga o outro. É o
// que o Volume da Unity (HDRP: Exposure por volume) resolve, e é o mesmo
// componente Ambiente, com caixa e mistura, que resolve aqui.
struct SceneTemplateVolume final {
  const char *name;
  i32 parent; // índice do nó pai no modelo; -1 pendura na raiz
  float position[3];
  bool box;   // falso = global
  float size[3];
  float exposureEv;
  float priority;
  float blendDistance;
  // Difuso e reflexo indiretos (Indirect Lighting Controller). Dentro de uma
  // sala o céu não chega inteiro; sem sondas de luz, é este número que diz isso.
  float indirectDiffuse;
  float indirectSpecular;
};

// Componentes que o modelo liga entre nos (indices locais ao modelo). Cada
// tipo e um caminho real de autoria; nenhum e decorativo.
enum class SceneTemplateComponentKind : u8 {
  StaticCollider,    // corpo estatico + colisor caixa do tamanho do cubo
  Character,         // Personagem com aceleracao/frenagem de demonstracao
  FollowCamera,      // Camera + Acompanhar alvo (a = alvo)
  TwoBoneIk,         // a = alvo
  ChainIk,           // a = alvo, value = ossos na cadeia
  LookAt,            // a = alvo, value = ossos na cadeia
  FootGrounding,
  Grounder,          // a = pelve
  Inertia,
  RotationCone,      // value = cone em graus
  InteractionObject, // pega ao alcancar
  InteractionSystem, // a = mao, b = objeto: comeca sozinho no Play
};
struct SceneTemplateComponent final {
  i32 node;
  SceneTemplateComponentKind kind;
  i32 a=-1,b=-1;
  float value=0;
};

struct SceneTemplate final {
  const char *id, *name, *summary;
  std::span<const SceneTemplateNode> nodes;
  std::span<const SceneTemplateView> views;
  std::span<const SceneTemplateVolume> volumes;
  std::span<const SceneTemplateComponent> components{};
  // Depois dos nos: o no 0 (humanoide) recebe a montagem de carregar caixa.
  bool carry=false;
};

} // namespace ae::editor

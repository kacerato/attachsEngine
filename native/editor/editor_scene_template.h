#pragma once
// Modelos de cena: uma cena de partida montada pelos MESMOS comandos de autoria
// que o usuário tem à mão.
//
// A Unity 6 tem Scene Templates (File > New Scene mostra os modelos do projeto,
// e Assets > Create > Scene Template cria outro a partir de uma cena). A ideia
// é a mesma; a diferença é que aqui o modelo entra na cena ABERTA em vez de
// abrir outra — no aparelho, trocar de cena no meio de uma comparação é perder
// o que estava sendo comparado, e o objetivo do modelo de referência é
// justamente servir de base de comparação.
//
// O modelo é DADO: uma lista de nós com pose, e as vistas salvas que vêm com
// ele. Nada aqui conhece Vulkan, arquivo ou aparelho, então o mesmo modelo vale
// em qualquer projeto e é conferível em teste.
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

struct SceneTemplate final {
  const char *id, *name, *summary;
  std::span<const SceneTemplateNode> nodes;
  std::span<const SceneTemplateView> views;
  std::span<const SceneTemplateVolume> volumes;
};

namespace detail {

// Cena de referência (G6-A): um interior fechado e um exterior aberto lado a
// lado, com a MESMA escala humana nos dois. Comparar iluminação, material ou
// pós só diz alguma coisa quando os dois casos estão na mesma cena, com o
// mesmo enquadramento — medir o interior hoje e o exterior amanhã compara
// também a diferença entre as duas montagens.
//
// Metros de verdade: o corpo tem 1,80 m, a porta 2,10 m, o pé-direito 2,60 m.
// É isso que faz um modelo importado parecer grande ou pequeno demais no ato.
inline constexpr SceneTemplateNode kReferenceNodes[]{
    // 0 raiz do exterior
    {"Exterior", -1, {0, 0, 0}, {0, 0, 0}, {1, 1, 1}, false, SceneTemplateLight::None, 0, 0, {1, 1, 1}, false},
    {"Terreno", 0, {0, -.1f, 0}, {0, 0, 0}, {60, .2f, 60}, true, SceneTemplateLight::None, 0, 0, {.62f, .60f, .56f}, false},
    // Sol: 100.000 lux é céu aberto de meio-dia; a inclinação dá sombra longa o
    // bastante para o relevo aparecer.
    {"Sol", 0, {0, 8, 0}, {50, -35, 0}, {1, 1, 1}, false, SceneTemplateLight::Directional, 100000, 0, {1, .96f, .90f}, false},
    // Referência humana: 1,80 m de pé. Todo julgamento de escala passa por ela.
    {"Pessoa 1,80 m", 0, {2.5f, .9f, 1.5f}, {0, 0, 0}, {.45f, 1.8f, .28f}, true, SceneTemplateLight::None, 0, 0, {.80f, .45f, .35f}, false},
    {"Bloco 1 m", 0, {-1.5f, .5f, 2.5f}, {0, 0, 0}, {1, 1, 1}, true, SceneTemplateLight::None, 0, 0, {.78f, .78f, .78f}, false},
    {"Bloco 2 m", 0, {-3.5f, 1, 2.5f}, {0, 0, 0}, {2, 2, 2}, true, SceneTemplateLight::None, 0, 0, {.55f, .58f, .62f}, false},
    {"Bloco 4 m", 0, {-7, 2, 2.5f}, {0, 0, 0}, {4, 4, 4}, true, SceneTemplateLight::None, 0, 0, {.35f, .37f, .40f}, false},
    // Pedestal do modelo em avaliação: 1 m de altura, para o objeto importado
    // ficar na altura dos olhos e ao lado da referência humana.
    {"Pedestal", 0, {0, .5f, 2.5f}, {0, 0, 0}, {1, 1, 1}, true, SceneTemplateLight::None, 0, 0, {.70f, .70f, .72f}, false},
    {"Modelo em avaliação", 0, {0, 1, 2.5f}, {0, 0, 0}, {1, 1, 1}, false, SceneTemplateLight::None, 0, 0, {1, 1, 1}, false},

    // 9 raiz do interior, 20 m ao lado: perto o bastante para um enquadramento
    // pegar os dois, longe o bastante para a luz de um não invadir o outro.
    {"Interior", -1, {-20, 0, 0}, {0, 0, 0}, {1, 1, 1}, false, SceneTemplateLight::None, 0, 0, {1, 1, 1}, false},
    {"Piso", 9, {0, .05f, 0}, {0, 0, 0}, {8, .1f, 6}, true, SceneTemplateLight::None, 0, 0, {.55f, .52f, .48f}, false},
    {"Teto", 9, {0, 2.65f, 0}, {0, 0, 0}, {8, .2f, 6}, true, SceneTemplateLight::None, 0, 0, {.88f, .88f, .86f}, false},
    {"Parede norte", 9, {0, 1.3f, -3}, {0, 0, 0}, {8, 2.6f, .2f}, true, SceneTemplateLight::None, 0, 0, {.82f, .80f, .76f}, false},
    {"Parede sul", 9, {0, 1.3f, 3}, {0, 0, 0}, {8, 2.6f, .2f}, true, SceneTemplateLight::None, 0, 0, {.82f, .80f, .76f}, false},
    {"Parede oeste", 9, {-4, 1.3f, 0}, {0, 0, 0}, {.2f, 2.6f, 6}, true, SceneTemplateLight::None, 0, 0, {.82f, .80f, .76f}, false},
    // A parede leste tem um vão de 2,10 m por 1,10 m: é a abertura que deixa a
    // luz do exterior entrar e, com ela, a diferença entre interior e exterior
    // ser visível na MESMA cena.
    {"Parede leste (baixa)", 9, {4, 1.3f, -1.95f}, {0, 0, 0}, {.2f, 2.6f, 2.1f}, true, SceneTemplateLight::None, 0, 0, {.82f, .80f, .76f}, false},
    {"Parede leste (alta)", 9, {4, 1.3f, 1.95f}, {0, 0, 0}, {.2f, 2.6f, 2.1f}, true, SceneTemplateLight::None, 0, 0, {.82f, .80f, .76f}, false},
    {"Verga da porta", 9, {4, 2.35f, 0}, {0, 0, 0}, {.2f, .5f, 1.8f}, true, SceneTemplateLight::None, 0, 0, {.82f, .80f, .76f}, false},
    // Luz de teto: 1200 lúmen é uma lâmpada doméstica; o alcance cobre a sala.
    {"Luz de teto", 9, {0, 2.4f, 0}, {0, 0, 0}, {1, 1, 1}, false, SceneTemplateLight::Point, 1200, 8, {1, .93f, .84f}, true},
    // Luz de recorte apontada para a parede oeste: é nela que sombra de contato
    // e mapa normal aparecem.
    {"Foco de parede", 9, {-2.4f, 2.4f, 1.6f}, {35, 140, 0}, {1, 1, 1}, false, SceneTemplateLight::Spot, 900, 9, {1, .97f, .92f}, true},
    {"Mesa", 9, {-1, .75f, 0}, {0, 0, 0}, {1.6f, .08f, .9f}, true, SceneTemplateLight::None, 0, 0, {.45f, .32f, .22f}, false},
    {"Pé da mesa", 9, {-1, .36f, 0}, {0, 0, 0}, {.12f, .72f, .12f}, true, SceneTemplateLight::None, 0, 0, {.30f, .22f, .16f}, false},
    {"Pessoa 1,80 m (interior)", 9, {1.8f, .9f, 1}, {0, 0, 0}, {.45f, 1.8f, .28f}, true, SceneTemplateLight::None, 0, 0, {.80f, .45f, .35f}, false},
    {"Modelo em avaliação (interior)", 9, {-1, .84f, 0}, {0, 0, 0}, {1, 1, 1}, false, SceneTemplateLight::None, 0, 0, {1, 1, 1}, false},
};

// Calibrado pela referência do próprio motor: o sol padrão do ambiente vale 2
// unidades de shader; 100.000 lux valem 146 (lux / 683), então o exterior pede
// log2(2/146) = -6,2 EV (-6,8 deixa o chão claro sem estourar). A lâmpada de 1.200 lm ilumina a mesa a 1,6 m com 0,055
// unidades, o que pede +5 EV; +4,5 deixa a luz que entra pela porta legível.
inline constexpr SceneTemplateVolume kReferenceVolumes[]{
    {"Exposição do dia", 0, {0, 0, 0}, false, {1, 1, 1}, -6.8f, 0, 0, 1, 1},
    // Numa sala fechada com uma porta, o céu chega como fração de por cento:
    // com o sol a 146 unidades, 0,1% do céu fica na ordem de um terço da
    // lâmpada sobre a mesa (0,055), que é o que deixa a lâmpada dominar e a luz
    // da porta ler como luz de fora. Medido no aparelho: 2% ainda estourava.
    {"Exposição do interior", 9, {0, 1.4f, 0}, true, {8.4f, 3.0f, 6.4f}, 4.5f, 10, 1, .001f, .001f},
};

// As vistas nascem junto: é o enquadramento repetido que torna duas medições
// comparáveis (runtime/scene_views.h).
// A vista do interior fica DENTRO da sala: o volume de exposição resolve pela
// posição da câmera, e uma câmera do lado de fora mediria o dia.
inline constexpr SceneTemplateView kReferenceViews[]{
    {"Exterior", {0, 1, 2.5f}, 9, .8f, .30f},
    {"Interior", {-20, 1.3f, 0}, 2.6f, 2.5f, .12f},
    {"Interior · porta", {-16.5f, 1.2f, 0}, 6, 1.5f, .10f},
    {"Comparação", {-10, 1.5f, 1}, 34, 1.2f, .42f},
};

inline constexpr SceneTemplate kTemplates[]{
    {"referencia", "Cenário de referência",
     "Interior fechado e exterior aberto na mesma cena, com escala humana, aberturas e vistas salvas.",
     kReferenceNodes, kReferenceViews, kReferenceVolumes},
};

} // namespace detail

inline std::span<const SceneTemplate> sceneTemplates() { return detail::kTemplates; }

} // namespace ae::editor

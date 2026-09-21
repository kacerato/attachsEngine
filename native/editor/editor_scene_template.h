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
};

// Enquadramento que nasce com o modelo, para comparar as mesmas vistas depois
// de cada mudança visual.
struct SceneTemplateView final {
  const char *name;
  float target[3];
  float distance, yaw, pitch;
};

struct SceneTemplate final {
  const char *id, *name, *summary;
  std::span<const SceneTemplateNode> nodes;
  std::span<const SceneTemplateView> views;
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
    {"Exterior", -1, {0, 0, 0}, {0, 0, 0}, {1, 1, 1}, false, SceneTemplateLight::None, 0, 0, {1, 1, 1}},
    {"Terreno", 0, {0, -.1f, 0}, {0, 0, 0}, {60, .2f, 60}, true, SceneTemplateLight::None, 0, 0, {.62f, .60f, .56f}},
    // Sol: 100.000 lux é céu aberto de meio-dia; a inclinação dá sombra longa o
    // bastante para o relevo aparecer.
    {"Sol", 0, {0, 8, 0}, {50, -35, 0}, {1, 1, 1}, false, SceneTemplateLight::Directional, 100000, 0, {1, .96f, .90f}},
    // Referência humana: 1,80 m de pé. Todo julgamento de escala passa por ela.
    {"Pessoa 1,80 m", 0, {2.5f, .9f, 1.5f}, {0, 0, 0}, {.45f, 1.8f, .28f}, true, SceneTemplateLight::None, 0, 0, {.80f, .45f, .35f}},
    {"Bloco 1 m", 0, {-1.5f, .5f, 2.5f}, {0, 0, 0}, {1, 1, 1}, true, SceneTemplateLight::None, 0, 0, {.78f, .78f, .78f}},
    {"Bloco 2 m", 0, {-3.5f, 1, 2.5f}, {0, 0, 0}, {2, 2, 2}, true, SceneTemplateLight::None, 0, 0, {.55f, .58f, .62f}},
    {"Bloco 4 m", 0, {-7, 2, 2.5f}, {0, 0, 0}, {4, 4, 4}, true, SceneTemplateLight::None, 0, 0, {.35f, .37f, .40f}},
    // Pedestal do modelo em avaliação: 1 m de altura, para o objeto importado
    // ficar na altura dos olhos e ao lado da referência humana.
    {"Pedestal", 0, {0, .5f, 2.5f}, {0, 0, 0}, {1, 1, 1}, true, SceneTemplateLight::None, 0, 0, {.70f, .70f, .72f}},
    {"Modelo em avaliação", 0, {0, 1, 2.5f}, {0, 0, 0}, {1, 1, 1}, false, SceneTemplateLight::None, 0, 0, {1, 1, 1}},

    // 9 raiz do interior, 20 m ao lado: perto o bastante para um enquadramento
    // pegar os dois, longe o bastante para a luz de um não invadir o outro.
    {"Interior", -1, {-20, 0, 0}, {0, 0, 0}, {1, 1, 1}, false, SceneTemplateLight::None, 0, 0, {1, 1, 1}},
    {"Piso", 9, {0, .05f, 0}, {0, 0, 0}, {8, .1f, 6}, true, SceneTemplateLight::None, 0, 0, {.55f, .52f, .48f}},
    {"Teto", 9, {0, 2.65f, 0}, {0, 0, 0}, {8, .1f, 6}, true, SceneTemplateLight::None, 0, 0, {.88f, .88f, .86f}},
    {"Parede norte", 9, {0, 1.3f, -3}, {0, 0, 0}, {8, 2.6f, .1f}, true, SceneTemplateLight::None, 0, 0, {.82f, .80f, .76f}},
    {"Parede sul", 9, {0, 1.3f, 3}, {0, 0, 0}, {8, 2.6f, .1f}, true, SceneTemplateLight::None, 0, 0, {.82f, .80f, .76f}},
    {"Parede oeste", 9, {-4, 1.3f, 0}, {0, 0, 0}, {.1f, 2.6f, 6}, true, SceneTemplateLight::None, 0, 0, {.82f, .80f, .76f}},
    // A parede leste tem um vão de 2,10 m por 1,10 m: é a abertura que deixa a
    // luz do exterior entrar e, com ela, a diferença entre interior e exterior
    // ser visível na MESMA cena.
    {"Parede leste (baixa)", 9, {4, 1.3f, -1.95f}, {0, 0, 0}, {.1f, 2.6f, 2.1f}, true, SceneTemplateLight::None, 0, 0, {.82f, .80f, .76f}},
    {"Parede leste (alta)", 9, {4, 1.3f, 1.95f}, {0, 0, 0}, {.1f, 2.6f, 2.1f}, true, SceneTemplateLight::None, 0, 0, {.82f, .80f, .76f}},
    {"Verga da porta", 9, {4, 2.35f, 0}, {0, 0, 0}, {.1f, .5f, 1.8f}, true, SceneTemplateLight::None, 0, 0, {.82f, .80f, .76f}},
    // Luz de teto: 1200 lúmen é uma lâmpada doméstica; o alcance cobre a sala.
    {"Luz de teto", 9, {0, 2.4f, 0}, {0, 0, 0}, {1, 1, 1}, false, SceneTemplateLight::Point, 1200, 8, {1, .93f, .84f}},
    // Luz de recorte apontada para a parede oeste: é nela que sombra de contato
    // e mapa normal aparecem.
    {"Foco de parede", 9, {-2.4f, 2.4f, 1.6f}, {35, 140, 0}, {1, 1, 1}, false, SceneTemplateLight::Spot, 900, 9, {1, .97f, .92f}},
    {"Mesa", 9, {-1, .75f, 0}, {0, 0, 0}, {1.6f, .08f, .9f}, true, SceneTemplateLight::None, 0, 0, {.45f, .32f, .22f}},
    {"Pé da mesa", 9, {-1, .36f, 0}, {0, 0, 0}, {.12f, .72f, .12f}, true, SceneTemplateLight::None, 0, 0, {.30f, .22f, .16f}},
    {"Pessoa 1,80 m (interior)", 9, {1.8f, .9f, 1}, {0, 0, 0}, {.45f, 1.8f, .28f}, true, SceneTemplateLight::None, 0, 0, {.80f, .45f, .35f}},
    {"Modelo em avaliação (interior)", 9, {-1, .84f, 0}, {0, 0, 0}, {1, 1, 1}, false, SceneTemplateLight::None, 0, 0, {1, 1, 1}},
};

// As vistas nascem junto: é o enquadramento repetido que torna duas medições
// comparáveis (runtime/scene_views.h).
inline constexpr SceneTemplateView kReferenceViews[]{
    {"Exterior", {0, 1, 2.5f}, 9, .8f, .30f},
    {"Interior", {-20, 1.3f, 0}, 7, 2.5f, .18f},
    {"Interior · porta", {-16.5f, 1.2f, 0}, 6, 1.5f, .10f},
    {"Comparação", {-10, 1.5f, 1}, 34, 1.2f, .42f},
};

inline constexpr SceneTemplate kTemplates[]{
    {"referencia", "Cenário de referência",
     "Interior fechado e exterior aberto na mesma cena, com escala humana, aberturas e vistas salvas.",
     kReferenceNodes, kReferenceViews},
};

} // namespace detail

inline std::span<const SceneTemplate> sceneTemplates() { return detail::kTemplates; }

} // namespace ae::editor

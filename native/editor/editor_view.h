// A ponte entre o que está na tela e o que está na cena: projetar um ponto do
// mundo para pixels, disparar um raio a partir de um toque e descobrir o que
// ele acerta.
//
// **A convenção de câmera não é inventada aqui.** Ela é a mesma de
// `renderer::PerspectiveFrustum` e de `projectBoundsToHzbScreenRect`, que por
// sua vez espelham o vertex shader: espaço de vista com X à direita, Y para
// cima e Z para frente, e clip com Y invertido. Um editor que projetasse por
// conta própria selecionaria um objeto e desenharia o contorno em outro assim
// que a tela girasse — por isso a transformação de superfície entra aqui do
// mesmo jeito que entra na oclusão, e não como um caso especial.
//
// Sem Vulkan, sem Android, sem I/O: testável integralmente no host.
#pragma once

#include "core/base.h"
#include "renderer/camera_ray.h"
#include "renderer/frustum_visibility.h"
#include "renderer/hzb_visibility.h"
#include "ui/ui_geometry.h"

#include <span>
#include <memory>
#include "editor/editor_pick_mesh.h"

namespace ae::editor {

struct EditorViewport final {
  renderer::PerspectiveFrustum frustum{};
  // Pré-rotação do display. Identidade em retrato/paisagem nativos.
  renderer::HzbScreenTransform surfaceTransform{};
  // Onde a vista 3D vive na superfície, em pixels lógicos. NÃO é a tela
  // inteira: no editor ela é a área entre a barra superior e a dock, e usar a
  // tela toda deslocaria toda seleção pela altura da barra.
  ui::UiRect rect{};
};

bool isViewportValid(const EditorViewport &viewport) noexcept;

struct EditorProjectedPoint final {
  ui::UiPoint screen{};
  // Profundidade em espaço de vista, em unidades de mundo. Positiva à frente da
  // câmera. Quem ordena sobreposições usa isto, não a profundidade normalizada.
  float viewDepth = 0.0f;
  bool valid = false;
};

// Projeta um ponto do mundo. `valid` é falso quando o ponto está atrás do plano
// próximo, onde a divisão perspectiva não tem resposta utilizável — devolver um
// pixel espelhado seria pior do que dizer que não dá.
EditorProjectedPoint projectWorldToScreen(const EditorViewport &viewport,
                                          const float world[3]) noexcept;

struct EditorRay final {
  float origin[3]{};
  // Normalizada.
  float direction[3]{};
  bool valid = false;
  // Picking interval along the normalized ray; gizmo intersections are free
  // to extend beyond this interval without changing their mathematical origin.
  float minimumDistance = 0;
  float maximumDistance = 3.402823466e+38F;
};

// Raio que sai da câmera e passa pelo pixel dado. Fora do retângulo da vista o
// raio ainda é calculado: um arraste que começou dentro continua válido depois
// que o dedo sai, e recusar aqui truncaria o gesto na borda.
EditorRay screenPointToRay(const EditorViewport &viewport, ui::UiPoint screen) noexcept;

// Sphere is broadphase only when resource triangles are available. Legacy
// resources without CPU geometry retain an explicitly approximate fallback.
struct EditorPickCandidate final {
  u32 id = 0;
  float center[3]{};
  float radius = 0.0f;
  // Candidato invisível ou travado não é selecionável pelo toque. O olho e o
  // cadeado da hierarquia precisam significar algo no viewport também.
  bool selectable = true;
  std::shared_ptr<const EditorPickMesh> mesh{};
  float model[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
};

struct EditorPickResult final {
  u32 id = 0;
  float distance = 0.0f;
  bool hit = false;
};

// Primeiro acerto ao longo do raio. Empates de distância ficam com o candidato
// registrado antes, o que torna a seleção determinística entre frames.
EditorPickResult pickNearest(std::span<const EditorPickCandidate> candidates,
                             const EditorRay &ray) noexcept;

// Distância da câmera até o ponto, em unidades de mundo. É o que o gizmo usa
// para manter tamanho constante na tela.
float distanceToCamera(const EditorViewport &viewport, const float world[3]) noexcept;

// Projeta um SEGMENTO, recortando-o no plano próximo antes de dividir.
//
// Sem o recorte, uma linha da grade que passa por baixo da câmera teria uma
// ponta atrás dela, e a divisão perspectiva jogaria essa ponta para o lado
// oposto da tela — uma linha atravessando o viewport na diagonal, do nada.
// Falso quando o segmento inteiro está atrás do plano próximo.
bool projectSegmentToScreen(const EditorViewport &viewport, const float from[3],
                            const float to[3], ui::UiPoint &outFrom,
                            ui::UiPoint &outTo) noexcept;

} // namespace ae::editor

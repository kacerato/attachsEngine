// Gizmo de transformação, do lado da matemática.
//
// O gizmo do mockup são três setas que saem do objeto e mantêm o mesmo tamanho
// na tela a qualquer distância. Esta camada resolve as três perguntas que isso
// implica — onde desenhar, o que o dedo pegou e quanto o objeto andou — e não
// desenha nada nem toca no documento.
//
// **O quadro é congelado no toque, não recalculado por frame.** Se os eixos
// fossem reprojetados a cada quadro a partir da posição já atualizada, o objeto
// perseguiria o dedo com ganho crescente: mover para a direita afastaria a
// ponta do eixo, que aumentaria a escala de mundo por pixel, que moveria mais no
// quadro seguinte. Congelar o quadro e aplicar sempre o deslocamento TOTAL desde
// o toque (ver UiPointerRouting::totalDelta) elimina a realimentação e o acúmulo
// de arredondamento de uma vez.
//
// Sem Vulkan, sem Android, sem I/O: testável integralmente no host.
#pragma once

#include "core/base.h"
#include "editor/editor_document.h"
#include "editor/editor_view.h"
#include "ui/ui_geometry.h"

namespace ae::editor {

enum class EditorGizmoMode : u8 { Select, Translate, Rotate, Scale };

enum class EditorGizmoHandle : u8 { None, AxisX, AxisY, AxisZ, PlaneYZ, PlaneZX, PlaneXY };

struct EditorGizmoSettings final {
  // Comprimento do eixo em pixels lógicos. Constante na tela: é o que faz o
  // gizmo continuar utilizável num objeto a duzentos metros.
  float screenLengthPixels = 120.0f;
  // Meia-largura do alvo de toque ao longo do eixo. Generosa de propósito — o
  // dedo cobre a própria seta, e errar o eixo num editor 3D custa um undo.
  float pickThresholdPixels = 26.0f;
  // Abaixo disto o eixo está apontado para a câmera: a projeção dele degenera
  // num ponto e qualquer arraste viraria um deslocamento enorme. Ele deixa de
  // ser pegável em vez de responder de forma imprevisível.
  float minimumAxisPixels = 24.0f;
  // Passo de encaixe em unidades de mundo. Zero desliga.
  float snapStep = 0.0f;
};

bool isGizmoSettingsValid(const EditorGizmoSettings &settings) noexcept;

struct EditorGizmoFrame final {
  float origin[3]{};
  ui::UiPoint originScreen{};
  ui::UiPoint axisEndScreen[3]{};
  // Quantas unidades de mundo o eixo desenhado representa. É a constante que
  // converte pixels arrastados em metros.
  float axisWorldLength = 0.0f;
  bool axisUsable[3]{};
  bool valid = false;
};

// Constrói o quadro para uma origem no mundo. Inválido quando o objeto está
// atrás do plano próximo — não há onde desenhar o gizmo nesse caso.
EditorGizmoFrame buildGizmoFrame(const EditorViewport &viewport, const float origin[3],
                                 const EditorGizmoSettings &settings) noexcept;

// Qual eixo o toque pegou. `None` quando nenhum está dentro do limiar; nesse
// caso o toque pertence à seleção ou à câmera, e é o chamador que decide.
EditorGizmoHandle pickGizmoHandle(const EditorGizmoFrame &frame, ui::UiPoint screen,
                                  const EditorGizmoSettings &settings) noexcept;

// Estado de um arraste em curso. Guarda o quadro e o transform do instante do
// toque; nada aqui é atualizado enquanto o dedo se move.
struct EditorGizmoDrag final {
  EditorGizmoFrame frame{};
  EditorGizmoHandle handle = EditorGizmoHandle::None;
  EditorTransform initial{};
  EditorGizmoSettings settings{};
  bool active = false;
};

bool beginGizmoDrag(const EditorGizmoFrame &frame, EditorGizmoHandle handle,
                    const EditorTransform &initial, const EditorGizmoSettings &settings,
                    EditorGizmoDrag &outDrag) noexcept;

// Transform resultante para um deslocamento total de tela desde o toque.
// Sempre parte de `drag.initial`: chamar duas vezes com o mesmo deslocamento dá
// o mesmo resultado, que é o que torna o arraste reprodutível e o undo exato.
bool resolveGizmoTranslation(const EditorGizmoDrag &drag, ui::UiPoint totalScreenDelta,
                             EditorTransform &outTransform) noexcept;
bool resolveGizmoTransform(const EditorGizmoDrag &drag, EditorGizmoMode mode,
                           ui::UiPoint delta, EditorTransform &outTransform) noexcept;

// Intersect a world-axis plane through origin. Parallel/behind rays are rejected.
bool gizmoPlanePoint(const EditorViewport &view, const float origin[3], u32 normalAxis,
                     ui::UiPoint point, float out[3]) noexcept;

// Ring geometry and ray/plane angle use the same world-axis convention.
void gizmoRingPoint(const float origin[3], u32 axis, float radius, float angle, float out[3]) noexcept;
bool gizmoRingAngle(const EditorViewport &view, const float origin[3], u32 axis,
                    ui::UiPoint point, float &angle) noexcept;

// Distância em pixels de um ponto ao segmento do eixo. Exposta porque o desenho
// destaca o eixo sob o dedo com o mesmo critério que a seleção usa.
float distanceToGizmoAxis(const EditorGizmoFrame &frame, EditorGizmoHandle handle,
                          ui::UiPoint screen) noexcept;

} // namespace ae::editor

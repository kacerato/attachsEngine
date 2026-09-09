// A câmera do editor: orbita em torno de um alvo, e não voa como a do jogo.
//
// É a diferença entre "a cena está rodando" e "estou editando a cena". Uma
// câmera livre em primeira pessoa é ótima para percorrer um mundo e péssima para
// examinar um objeto: para olhar uma peça pelo outro lado é preciso andar em
// volta dela sem perdê-la de vista, o que numa tela de toque é quase impossível.
// A câmera de órbita inverte isso — o alvo fica parado e a câmera é que anda.
//
// Os três gestos são os de qualquer ferramenta 3D, e a escolha de qual é qual
// não é arbitrária: **um dedo orbita, dois dedos deslocam e afastam**. Orbitar é
// o gesto mais frequente, então fica no mais barato; deslocar move o alvo e
// muda o que está sendo examinado, então custa dois dedos de propósito.
//
// Sem Vulkan, sem Android, sem I/O: testável integralmente no host.
#pragma once

#include "core/base.h"
#include "editor/editor_view.h"
#include "renderer/frustum_visibility.h"
#include "renderer/hzb_visibility.h"
#include "ui/ui_geometry.h"

namespace ae::editor {

struct EditorCamera final {
  // Ponto orbitado, em coordenadas de mundo.
  float target[3]{0.0f, 0.0f, 0.0f};
  float distance = 12.0f;
  float yaw = 0.6f;
  // Positivo olha de cima para baixo. Preso longe dos polos: exatamente neles a
  // órbita perde o eixo de referência e a câmera gira sozinha em torno de si.
  float pitch = 0.45f;
};

struct EditorCameraLimits final {
  float minimumDistance = 0.01f;
  float maximumDistance = 1000000.0f;
  float maximumPitch = 1.45f;  // ~83 graus
  // Radianos por pixel arrastado. Uma volta completa em cerca de meia tela.
  float orbitRadiansPerPixel = 0.010f;
};

bool isEditorCameraValid(const EditorCamera &camera) noexcept;

// Posição da câmera derivada do alvo, da distância e dos ângulos. Ela nunca é
// guardada: guardar posição E alvo permitiria os dois divergirem, e a órbita
// passaria a girar em torno de um ponto que não é o que está sendo olhado.
void editorCameraPosition(const EditorCamera &camera, float outPosition[3]) noexcept;

// Monta a vista para um retângulo de tela. `rect` é a área da cena JÁ descontados
// os painéis: é o que faz a projeção continuar certa quando um divisor se move.
EditorViewport buildEditorViewport(const EditorCamera &camera, const ui::UiRect &rect,
                                   const renderer::HzbScreenTransform &surfaceTransform,
                                   const renderer::PerspectiveVisibilitySettings &settings = {});

void orbitEditorCamera(EditorCamera &camera, ui::UiPoint delta,
                       const EditorCameraLimits &limits = {}) noexcept;

// Desloca o ALVO no plano da tela. A conversão usa a distância, então o mundo
// acompanha o dedo na mesma proporção perto e longe — sem isso, deslocar de
// longe atravessaria o mapa inteiro.
void panEditorCamera(EditorCamera &camera, ui::UiPoint delta, const ui::UiRect &rect,
                     const renderer::PerspectiveVisibilitySettings &settings = {},
                     const EditorCameraLimits &limits = {}) noexcept;

// `factor` maior que 1 afasta. Multiplicativo e não aditivo: a mesma pinça
// aproxima na mesma proporção a dois metros e a duzentos.
void zoomEditorCamera(EditorCamera &camera, float factor,
                      const EditorCameraLimits &limits = {}) noexcept;

// Enquadra uma esfera: alvo no centro, distância suficiente para o raio caber.
void frameEditorCamera(EditorCamera &camera, const float center[3], float radius,
                       const renderer::PerspectiveVisibilitySettings &settings = {},
                       const EditorCameraLimits &limits = {}) noexcept;

} // namespace ae::editor

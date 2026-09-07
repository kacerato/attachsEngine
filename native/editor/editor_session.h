// A sessão de edição: o objeto que a plataforma segura.
//
// Ela existe para que a camada Android não precise saber que existem documento,
// histórico, câmera, roteador, lista de desenho e construtor de instâncias.
// `android_main` alimenta toques e pede instâncias; tudo entre uma coisa e outra
// é decisão do editor, e portanto testável sem aparelho.
//
// **O que o dedo faz aqui é o que ele faria num editor de verdade:**
//   • um dedo no vazio da cena orbita a câmera;
//   • dois dedos deslocam o alvo e afastam/aproximam;
//   • um toque curto na cena seleciona o que está sob ele;
//   • arrastar uma alça do gizmo move o objeto, num único passo de desfazer;
//   • tudo que muda o documento passa pelo histórico.
//
// Sem Vulkan, sem Android, sem I/O: testável integralmente no host.
#pragma once

#include "core/base.h"
#include "editor/editor_camera.h"
#include "editor/editor_document.h"
#include "editor/editor_history.h"
#include "editor/editor_screen.h"
#include "ui/ui_font.h"
#include "ui/ui_icon_atlas.h"
#include "ui/ui_instance_builder.h"

#include <span>
#include <vector>

namespace ae::editor {

class EditorSession final {
public:
  static constexpr u32 kMaximumInstances = 16384;

  // Os atlas continuam pertencendo ao chamador e precisam sobreviver à sessão.
  void initialize(const ui::UiFont *font, const ui::UiIconAtlas *icons);

  // A superfície é sempre o espaço LÓGICO em paisagem, sem pré-rotação.
  //
  // A rotação do display é aplicada uma única vez, no vertex shader da interface,
  // ao converter pixel lógico em NDC. Aplicá-la também na projeção da cena
  // rodaria a grade e o gizmo dentro de um retângulo que já está no espaço
  // certo — foi exatamente o que aconteceu na primeira execução no aparelho, e
  // a grade saiu atravessando a tela na diagonal.
  void setSurface(const ui::UiRect &surface, const ui::UiInsets &safeArea);
  void setProjectName(const char *name);
  // Seleção inicial vinda de fora, para uma cena recém-carregada já abrir com
  // algo no Inspector em vez de "nada selecionado".
  void setSelection(EditorEntityId entity);

  // Um evento de ponteiro já em coordenadas lógicas. Devolve true quando a
  // interface o consumiu — a plataforma usa isso para não repassar o mesmo
  // toque ao controlador de jogo.
  bool handlePointer(const ui::UiPointerEvent &event);
  void cancelPointers();

  // Reconstrói a lista de desenho e as instâncias do frame.
  void update();

  std::span<const ui::UiInstance> instances() const noexcept { return instances_; }
  const EditorScreenLayout &layout() const noexcept { return layout_; }
  const EditorScreenState &screen() const noexcept { return state_; }
  const EditorCamera &camera() const noexcept { return camera_; }
  const EditorViewport &view() const noexcept { return view_; }
  EditorDocument &document() noexcept { return document_; }
  EditorHistory &history() noexcept { return history_; }
  EditorEntityId selection() const noexcept { return state_.selection; }
  bool playRequested() const noexcept { return playRequested_; }
  void clearPlayRequest() noexcept { playRequested_ = false; }

  // **Editar é ver a cena PARADA.** A água não ondula, o casco não anda e a
  // física não integra enquanto alguém posiciona um objeto — senão o que se vê
  // não é um editor, é um vídeo com painéis por cima. A aba Play é o que solta.
  bool isPlaying() const noexcept { return state_.workspace == EditorWorkspace::Play; }
  // Relógio da CENA, separado do relógio de parede. Ele só avança em Play, e é
  // ele que alimenta a animação e a simulação — congelar apenas o desenho
  // mostraria uma imagem parada sobre um estado que continua mudando, e apertar
  // Play daria um salto.
  float sceneTime() const noexcept { return sceneTime_; }
  // `wallSeconds` é o relógio contínuo da plataforma. A sessão deriva o próprio
  // passo dele, o que a torna imune a um primeiro quadro com valor arbitrário.
  void advanceClock(float wallSeconds) noexcept;
  // Enquadra o objeto selecionado. É o gesto de "onde ele está?", e sem ele um
  // objeto longe do alvo da órbita fica inalcançável.
  void frameSelection();

private:
  struct ViewportPointer final {
    u32 id = 0;
    ui::UiPoint position{};
    bool moved = false;
  };

  void buildPickCandidates();
  bool handleViewportPointer(const ui::UiPointerEvent &event, const ui::UiPointerRouting &routing);
  void handleGizmoPointer(const ui::UiPointerRouting &routing, u32 axis);
  ViewportPointer *findViewportPointer(u32 id) noexcept;

  const ui::UiFont *font_ = nullptr;
  const ui::UiIconAtlas *icons_ = nullptr;
  EditorDocument document_;
  EditorHistory history_;
  EditorCamera camera_;
  EditorViewport view_{};
  EditorScreenState state_{};
  EditorScreenLayout layout_{};
  ui::UiDrawList list_;
  ui::UiInputRouter router_;
  std::vector<ui::UiInstance> instances_;
  std::vector<EditorPickCandidate> candidates_;
  std::vector<ViewportPointer> viewportPointers_;
  // Distância entre dois dedos no frame anterior, para derivar a pinça.
  float pinchDistance_ = 0.0f;
  EditorGizmoDrag gizmoDrag_{};
  bool gizmoTransactionOpen_ = false;
  bool playRequested_ = false;
  float sceneTime_ = 0.0f;
  float lastWallSeconds_ = 0.0f;
  bool clockPrimed_ = false;
};

} // namespace ae::editor

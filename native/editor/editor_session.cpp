#include "editor/editor_session.h"

#include <algorithm>
#include <cmath>

namespace ae::editor {
namespace {

using namespace ae::ui;

// Raio de seleção de uma entidade enquanto o documento não carrega os limites
// reais da malha. Ele é generoso de propósito: errar o toque num editor de
// celular custa um undo, e um alvo pequeno demais é pior do que um impreciso.
// Quando a malha entrar no documento, este número sai.
constexpr float kProvisionalPickRadius = 0.75f;

float distanceBetween(UiPoint a, UiPoint b) noexcept {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  return std::sqrt(dx * dx + dy * dy);
}

} // namespace

void EditorSession::initialize(const UiFont *font, const UiIconAtlas *icons) {
  font_ = font;
  icons_ = icons;
  state_.document = &document_;
  camera_.target[1] = 0.5f;
}

void EditorSession::setSurface(const UiRect &surface, const UiInsets &safeArea) {
  state_.surface = surface;
  state_.safeArea = safeArea;
}

void EditorSession::setSelection(EditorEntityId entity) {
  if (document_.exists(entity)) state_.selection = entity;
}

void EditorSession::setProjectName(const char *name) {
  if (name != nullptr) state_.projectName = name;
}

EditorSession::ViewportPointer *EditorSession::findViewportPointer(u32 id) noexcept {
  for (ViewportPointer &pointer : viewportPointers_)
    if (pointer.id == id) return &pointer;
  return nullptr;
}

void EditorSession::buildPickCandidates() {
  candidates_.clear();
  std::vector<EditorEntityId> subtree;
  document_.collectSubtree(document_.root(), subtree);
  for (const EditorEntityId id : subtree) {
    if (id == document_.root()) continue;
    const EditorEntity *entity = document_.find(id);
    if (entity == nullptr) continue;
    // Uma pasta não é selecionável no viewport: ela não tem corpo, e deixar o
    // toque acertá-la selecionaria um grupo quando o usuário mirou uma peça.
    if (entity->kind == EditorEntityKind::Folder) continue;
    EditorPickCandidate candidate{};
    candidate.id = id;
    for (u32 axis = 0; axis < 3; ++axis) candidate.center[axis] = entity->transform.position[axis];
    const float scale = std::max({entity->transform.scale[0], entity->transform.scale[1],
                                  entity->transform.scale[2], 0.1f});
    candidate.radius = kProvisionalPickRadius * scale;
    candidate.selectable = entity->visible && entity->active;
    candidates_.push_back(candidate);
  }
}

void EditorSession::handleGizmoPointer(const UiPointerRouting &routing, u32 axis) {
  const EditorEntity *entity = document_.find(state_.selection);
  if (entity == nullptr || axis > 2) return;

  if (!gizmoDrag_.active) {
    EditorGizmoSettings settings{};
    settings.screenLengthPixels = 72.0f;
    const EditorGizmoFrame frame =
        buildGizmoFrame(view_, entity->transform.position, settings);
    const EditorGizmoHandle handle = static_cast<EditorGizmoHandle>(axis + 1);
    if (!beginGizmoDrag(frame, handle, entity->transform, settings, gizmoDrag_)) return;
    state_.activeGizmoAxis = handle;
    // Uma transação por gesto: o arraste inteiro vira UM passo de desfazer, e
    // não um por frame. É a razão de o histórico ter transações.
    gizmoTransactionOpen_ = history_.begin("Move");
  }

  EditorTransform moved{};
  if (resolveGizmoTranslation(gizmoDrag_, routing.totalDelta, moved)) {
    // O token de fusão é o eixo: comandos consecutivos do mesmo arraste viram
    // um só dentro da transação.
    history_.setTransform(document_, state_.selection, moved, 0x6000u + axis);
  }

  if (routing.released) {
    if (gizmoTransactionOpen_) history_.end();
    gizmoTransactionOpen_ = false;
    gizmoDrag_ = EditorGizmoDrag{};
    state_.activeGizmoAxis = EditorGizmoHandle::None;
  }
}

bool EditorSession::handleViewportPointer(const UiPointerEvent &event,
                                          const UiPointerRouting &routing) {
  if (event.phase == UiPointerPhase::Down) {
    viewportPointers_.push_back({event.pointerId, event.position, false});
    pinchDistance_ = viewportPointers_.size() == 2
        ? distanceBetween(viewportPointers_[0].position, viewportPointers_[1].position)
        : 0.0f;
    return false;
  }

  ViewportPointer *pointer = findViewportPointer(event.pointerId);
  if (pointer == nullptr) return false;

  if (event.phase == UiPointerPhase::Move) {
    const UiPoint previous = pointer->position;
    pointer->position = event.position;
    if (routing.dragging) pointer->moved = true;

    if (viewportPointers_.size() >= 2) {
      // Dois dedos: o deslocamento do ponto médio desloca o alvo, e a variação
      // da distância entre eles afasta ou aproxima. As duas coisas ao mesmo
      // tempo, porque é assim que a mão faz.
      const UiPoint first = viewportPointers_[0].position;
      const UiPoint second = viewportPointers_[1].position;
      const float distance = distanceBetween(first, second);
      if (pinchDistance_ > 1.0f && distance > 1.0f)
        zoomEditorCamera(camera_, pinchDistance_ / distance);
      pinchDistance_ = distance;
      const UiPoint delta{(event.position.x - previous.x) * 0.5f,
                          (event.position.y - previous.y) * 0.5f};
      panEditorCamera(camera_, delta, layout_.viewport);
    } else {
      orbitEditorCamera(camera_, {event.position.x - previous.x, event.position.y - previous.y});
    }
    return true;
  }

  // Levantou. Um toque curto que não virou arraste é uma seleção.
  const bool wasTap = !pointer->moved && event.phase == UiPointerPhase::Up;
  const UiPoint at = pointer->position;
  viewportPointers_.erase(viewportPointers_.begin() +
                          (pointer - viewportPointers_.data()));
  pinchDistance_ = viewportPointers_.size() == 2
      ? distanceBetween(viewportPointers_[0].position, viewportPointers_[1].position)
      : 0.0f;

  if (wasTap && viewportPointers_.empty()) {
    buildPickCandidates();
    const EditorPickResult hit = pickNearest(candidates_, screenPointToRay(view_, at));
    // Tocar no vazio LIMPA a seleção. É o gesto que todo editor tem, e sem ele
    // não há como desmarcar sem selecionar outra coisa.
    state_.selection = hit.hit ? hit.id : kInvalidEntity;
  }
  return true;
}

bool EditorSession::handlePointer(const UiPointerEvent &event) {
  const UiPointerRouting routing = router_.route(event);
  if (routing.target == UiPointerTarget::Viewport) return handleViewportPointer(event, routing);
  if (routing.target == UiPointerTarget::None) return true;

  const u32 gizmoBase = widgetId(EditorWidget::GizmoAxisBase);
  if (routing.widgetId >= gizmoBase && routing.widgetId < gizmoBase + 3) {
    handleGizmoPointer(routing, routing.widgetId - gizmoBase);
    return true;
  }

  const EditorPointerOutcome outcome =
      applyEditorPointer(state_, layout_, routing, document_, history_);
  if (outcome.requestPlay) playRequested_ = true;
  return outcome.consumed;
}

void EditorSession::cancelPointers() {
  router_.cancelAllPointers();
  viewportPointers_.clear();
  pinchDistance_ = 0.0f;
  if (gizmoTransactionOpen_) history_.end();
  gizmoTransactionOpen_ = false;
  gizmoDrag_ = EditorGizmoDrag{};
  state_.activeGizmoAxis = EditorGizmoHandle::None;
}

void EditorSession::frameSelection() {
  const EditorEntity *entity = document_.find(state_.selection);
  if (entity == nullptr) return;
  frameEditorCamera(camera_, entity->transform.position, kProvisionalPickRadius * 2.0f);
}

void EditorSession::update() {
  if (font_ == nullptr || icons_ == nullptr) return;
  state_.canUndo = history_.canUndo();
  state_.canRedo = history_.canRedo();
  u32 pressed = 0;
  state_.pressedWidget = router_.pressedWidget(pressed) ? pressed : 0;

  const UiFontMetrics metrics = font_->metrics(UiFontWeight::Regular);

  // Duas passagens, e é o preço de fazer o certo. A grade e o gizmo precisam da
  // projeção, a projeção precisa do retângulo da cena, e o retângulo da cena só
  // existe depois que os painéis tomaram a largura deles. Usar o retângulo do
  // frame anterior custaria um quadro de atraso justamente quando o usuário
  // está arrastando um divisor — o momento em que ele mais olha para a borda.
  list_.begin(state_.surface, metrics);
  router_.beginFrame();
  layout_ = buildEditorScreen(state_, defaultTheme(), list_, router_);

  // Identidade, e não a pré-rotação do display: o retângulo da vista já está no
  // espaço lógico em paisagem, e o shader da interface roda tudo uma vez no fim.
  view_ = buildEditorViewport(camera_, layout_.viewport, {});
  state_.view = &view_;

  list_.begin(state_.surface, metrics);
  router_.beginFrame();
  layout_ = buildEditorScreen(state_, defaultTheme(), list_, router_);

  instances_.clear();
  buildUiInstances(list_, *font_, *icons_, kMaximumInstances, instances_);
}

} // namespace ae::editor

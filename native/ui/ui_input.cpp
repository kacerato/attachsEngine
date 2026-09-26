#include "ui/ui_input.h"

#include <cmath>

namespace ae::ui {
namespace {

float distanceSquared(UiPoint first, UiPoint second) noexcept {
  const float dx = first.x - second.x;
  const float dy = first.y - second.y;
  return dx * dx + dy * dy;
}

} // namespace

void UiInputRouter::beginFrame() noexcept { regions_.clear(); }

bool UiInputRouter::addRegion(const UiRect &rect, u32 widgetId, float minimumTouchSide) {
  if (regions_.size() >= kMaximumRegions) return false;
  if (!isFinite(rect) || rect.isEmpty()) return false;
  if (!std::isfinite(minimumTouchSide) || minimumTouchSide < 0.0f) return false;
  Region region{};
  region.rect = minimumTouchSide > 0.0f ? expandToMinimumTouchTarget(rect, minimumTouchSide) : rect;
  region.exact = rect;
  region.widgetId = widgetId;
  regions_.push_back(region);
  return true;
}

bool UiInputRouter::addBlocker(const UiRect &rect) {
  if (regions_.size() >= kMaximumRegions) return false;
  if (!isFinite(rect) || rect.isEmpty()) return false;
  Region region{};
  region.rect = rect;
  region.exact = rect;
  region.blocker = true;
  regions_.push_back(region);
  return true;
}

const UiInputRouter::ActivePointer *UiInputRouter::find(u32 pointerId) const noexcept {
  for (const ActivePointer &pointer : pointers_)
    if (pointer.pointerId == pointerId) return &pointer;
  return nullptr;
}

UiInputRouter::ActivePointer *UiInputRouter::find(u32 pointerId) noexcept {
  for (ActivePointer &pointer : pointers_)
    if (pointer.pointerId == pointerId) return &pointer;
  return nullptr;
}

void UiInputRouter::erase(u32 pointerId) noexcept {
  for (usize index = 0; index < pointers_.size(); ++index) {
    if (pointers_[index].pointerId != pointerId) continue;
    pointers_[index] = pointers_.back();
    pointers_.pop_back();
    return;
  }
}

UiPointerRouting UiInputRouter::hitTest(UiPoint position) const noexcept {
  UiPointerRouting result{};result.position=position;
  if(!std::isfinite(position.x) || !std::isfinite(position.y)) return result;
  result.target=UiPointerTarget::Viewport;
  // A expansão até o alvo mínimo serve para o toque que cai PERTO de um alvo
  // pequeno, nunca para o que cai DENTRO de outro (a regra do TouchDelegate do
  // Android e dos alvos de toque da web). Sem isso, numa lista de linhas mais
  // baixas que o alvo mínimo, a expansão da linha de baixo tomava a metade de
  // baixo da linha de cima — o toque no meio de uma linha pegava a vizinha.
  // A ordem continua valendo: um bloqueador por cima encerra a busca.
  const Region *expanded=nullptr;
  for(usize i=regions_.size();i>0;--i) {
    const auto &region=regions_[i-1];
    if(!region.rect.contains(position)) continue;
    if(region.blocker || region.exact.contains(position)) {
      const auto &chosen=region.blocker && expanded?*expanded:region;
      result.target=chosen.blocker?UiPointerTarget::None:UiPointerTarget::Widget;
      result.widgetId=chosen.widgetId;
      return result;
    }
    if(!expanded) expanded=&region;
  }
  if(expanded) {result.target=UiPointerTarget::Widget;result.widgetId=expanded->widgetId;}
  return result;
}

UiPointerRouting UiInputRouter::route(const UiPointerEvent &event) noexcept {
  UiPointerRouting routing{};
  if (!std::isfinite(event.position.x) || !std::isfinite(event.position.y)) return routing;

  if (event.phase == UiPointerPhase::Down) {
    // Um `Down` repetido para o mesmo id é entrada malformada (o sistema perdeu
    // um `Up`). Descartar o estado antigo é a recuperação honesta: manter os
    // dois deixaria uma captura órfã que nunca mais recebe `Up`.
    erase(event.pointerId);

    ActivePointer pointer{};
    pointer.pointerId = event.pointerId;
    pointer.start = event.position;
    pointer.previous = event.position;
    pointer.startTime = event.timeSeconds;
    const auto hit=hitTest(event.position);
    pointer.target=hit.target;pointer.widgetId=hit.widgetId;

    // Da última registrada para a primeira: a ordem de desenho é a ordem de
    // profundidade, e o que foi desenhado por último está por cima.
    pointers_.push_back(pointer);
    routing.target = pointer.target;
    routing.widgetId = pointer.widgetId;
    routing.start = pointer.start;
    routing.position = event.position;
    return routing;
  }

  ActivePointer *pointer = find(event.pointerId);
  // Movimento ou `Up` de um dedo que nunca desceu (perdido numa troca de
  // superfície, por exemplo) não pertence a ninguém. Inventar um alvo aqui
  // moveria a câmera sem que o usuário tivesse tocado nela.
  if (pointer == nullptr) return routing;

  routing.target = pointer->target;
  routing.widgetId = pointer->widgetId;
  routing.start = pointer->start;
  routing.position = event.position;
  routing.totalDelta = {event.position.x - pointer->start.x, event.position.y - pointer->start.y};
  routing.stepDelta = {event.position.x - pointer->previous.x,
                       event.position.y - pointer->previous.y};
  routing.heldSeconds = event.timeSeconds > pointer->startTime ? event.timeSeconds - pointer->startTime : 0.0;
  pointer->previous = event.position;

  if (!pointer->dragging &&
      distanceSquared(event.position, pointer->start) > dragSlop_ * dragSlop_)
    pointer->dragging = true;
  routing.dragging = pointer->dragging;

  if (event.phase == UiPointerPhase::Move) return routing;

  routing.released = true;
  if (event.phase == UiPointerPhase::Up && pointer->target == UiPointerTarget::Widget &&
      !pointer->dragging) {
    // O clique exige que o dedo ainda esteja sobre o widget NESTE frame. Sair
    // da área antes de levantar é o gesto universal de desistir do botão, e o
    // widget pode ter mudado de lugar durante o toque.
    const auto hit=hitTest(event.position);
    routing.tapped=hit.target==UiPointerTarget::Widget && hit.widgetId==pointer->widgetId;
  }
  erase(event.pointerId);
  return routing;
}

bool UiInputRouter::pressedWidget(u32 &outWidgetId) const noexcept {
  for (const ActivePointer &pointer : pointers_) {
    if (pointer.target != UiPointerTarget::Widget) continue;
    outWidgetId = pointer.widgetId;
    return true;
  }
  return false;
}

bool UiInputRouter::isPointerActive(u32 pointerId) const noexcept {
  return find(pointerId) != nullptr;
}

u32 UiInputRouter::viewportPointerCount() const noexcept {
  u32 count = 0;
  for (const ActivePointer &pointer : pointers_)
    if (pointer.target == UiPointerTarget::Viewport) ++count;
  return count;
}

void UiInputRouter::setDragSlop(float slop) noexcept {
  if (!std::isfinite(slop) || slop < 0.0f) return;
  dragSlop_ = slop;
}

void UiInputRouter::cancelAllPointers() noexcept { pointers_.clear(); }

} // namespace ae::ui

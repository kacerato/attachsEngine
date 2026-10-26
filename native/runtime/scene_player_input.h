#pragma once
// G7: Entrada do jogador (PlayerInput, Behavior = Invoke Unity Events). A cada
// quadro de Play, depois que a entrada do quadro chegou, as ações observadas
// que dispararam ou foram soltas viram eventos do componente na mesma fila que
// scripts e Conexões de evento consomem (runtime/component_operations.h).
#include "runtime/component_operations.h"
#include "runtime/input_actions.h"

namespace ae::runtime {
// Devolve quantos eventos foram emitidos.
u32 emitPlayerInputEvents(GameWorld &world,const InputService &input,ComponentEventQueue &events);
} // namespace ae::runtime

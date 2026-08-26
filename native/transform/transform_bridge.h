// Kernel em lote para a etapa matemática da propagação de transforms.
//
// A fronteira é deliberadamente estreita: arrays POD densos entram e saem em
// uma única chamada. Hierarquia, ECS, lifecycle e dirty tracking continuam no
// Aether.Core; nenhum ponteiro interno de Chunk atravessa a ABI.
#pragma once

#include <cstdint>

#if defined(_WIN32)
#define AETHER_TRANSFORM_API __declspec(dllexport)
#else
#define AETHER_TRANSFORM_API __attribute__((visibility("default")))
#endif

extern "C" {

struct AetherTransform {
  float position[3];
  float rotation[4];
  float scale[3];
};

struct AetherTransformPlanEntry {
  const AetherTransform *local;
  AetherTransform *world;
  std::int32_t parent;
  std::int32_t reserved;
};

// parents precisa estar em ordem topológica: -1 representa raiz e todo outro
// índice deve ser menor que o índice do filho. Retorna 1 em sucesso e 0 para
// argumentos inválidos, sem escrever além da posição validada.
AETHER_TRANSFORM_API std::int32_t
AetherTransform_Propagate(const AetherTransform *local,
                          const std::int32_t *parents,
                          AetherTransform *world,
                          std::int32_t count);

// Variante usada pelo ECS: os endereços apontam para componentes em buffers
// pinados e permanecem válidos durante toda a chamada síncrona.
AETHER_TRANSFORM_API std::int32_t
AetherTransform_PropagatePlan(const AetherTransformPlanEntry *entries,
                              std::int32_t count);

} // extern "C"

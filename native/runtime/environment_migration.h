#pragma once

#include "runtime/scene_graph.h"

namespace ae::runtime {

// PK-1 (A-01/A-04): migração única das cenas anteriores ao Ambiente v17.
//
// 1. A escala escondida do céu (skyScale): até a v16, com o sol em lux, o céu
//    procedural/HDRI, o reflexo e a luz ambiente eram multiplicados pela razão
//    "intensidade do sol / sol de referência do motor (2)". Ela vira o que a
//    Unity tem para isso: Exposure do Skybox (procedural) e EV do HDRI, gravados
//    no Ambiente -- a mesma imagem, sem o acoplamento.
// 2. Os quatro multiplicadores legados da raiz ("Sol potência ×", "Ambiente ×",
//    "Exposição ×", "Rotação do céu") viram os campos donos: Intensity
//    Multiplier, compensação de exposição e rotação do HDRI de cada Ambiente.
//    A raiz volta ao neutro e a janela antiga deixa de existir.
//
// Só toca Ambientes lidos de uma versão anterior (loadedVersion) e a raiz não
// neutra; rodar de novo não muda nada. Devolve quantos objetos mudaram.
struct EnvironmentMigrationReport final {
  u32 environments = 0;
  float skyScale = 1.0f;
  bool rootMultipliers = false;
};
EnvironmentMigrationReport migrateLegacyEnvironmentLighting(SceneGraph &graph);

} // namespace ae::runtime

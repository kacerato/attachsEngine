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

// PK-4 (V-04): até o Ambiente v18 o volume tinha camada própria ("Ambiente
// 0..7") e a câmera um filtro de um bit. Na Unity o Volume usa a camada do
// GameObject e a câmera a Volume Mask (LayerMask). Cada "Ambiente N" (N > 0)
// vira a camada de objeto de nome "Ambiente N" (criada numa vaga livre do
// projeto) e o objeto do volume passa para ela; a máscara antiga da câmera
// vira as camadas onde estão os volumes que ela via. "Ambiente 0" e "todos"
// não mudam nada. Devolve quantos objetos (volumes e câmeras) mudaram.
u32 migrateLegacyVolumeLayers(SceneGraph &graph);

} // namespace ae::runtime

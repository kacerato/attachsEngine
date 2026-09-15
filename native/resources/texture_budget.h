#pragma once
#include "core/base.h"
#include "renderer/authoring_texture.h"
#include <span>
#include <vector>

namespace ae::resources {
// Orçamento agregado de residência de texturas importadas (R2).
//
// O limite por arquivo (`GltfImportLimits::maximumTextureBytes`) não enxerga o
// projeto: oito fontes dentro do limite cada uma podem, juntas, passar do que o
// aparelho aguenta. Este passo roda sobre a biblioteca inteira, na hora de
// publicar, e reduz a RESIDÊNCIA: tira o nível mais alto da cadeia de mips da
// maior textura até o conjunto caber ou todas chegarem ao piso.
//
// Não toca a fonte nem o derivado em cache: as texturas reduzidas são cópias
// novas, as originais continuam com quem as tinha. Uma textura presente mais de
// uma vez na lista (mesmo ponteiro) conta uma vez e é reduzida uma vez.
struct TextureBudgetReport {
  u64 budgetBytes = 0;
  u64 requestedBytes = 0; // soma das cadeias recebidas, compartilhadas contadas uma vez
  u64 residentBytes = 0;  // soma depois das reduções
  u32 textures = 0;       // texturas distintas
  u32 reducedTextures = 0;
  u32 droppedLevels = 0;
  bool withinBudget() const noexcept { return residentBytes <= budgetBytes; }
};

// Troca, em `textures`, as entradas reduzidas por cópias com menos níveis.
// Nunca reduz uma textura abaixo de `minimumDimension` no menor lado, nem a que
// tem um único nível. Entradas nulas ou inválidas ficam como estão e não contam.
TextureBudgetReport applyTextureBudget(std::vector<renderer::SharedAuthoringTexture> &textures, u64 budgetBytes,
                                       u32 minimumDimension);
} // namespace ae::resources

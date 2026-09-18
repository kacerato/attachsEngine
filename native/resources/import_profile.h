#pragma once
#include "core/base.h"
#include "resources/asset_registry.h"
#include "resources/gltf_import.h"
#include <array>
#include <string>
#include <string_view>

namespace ae::resources {
// Perfil de importação (R3): as escolhas AUTORAIS de como uma fonte vira recurso.
//
// Separado dos limites do aparelho (`GltfImportLimits`): os limites dizem o que
// o aparelho aguenta, o perfil diz o que o autor quer. A combinação é feita por
// `applyImportProfile`, e o perfil nunca sobe acima do teto do aparelho.
//
// Só entram aqui campos com consumidor real no importador. Cada campo novo sobe
// `ImportProfileSchema` e muda a chave do cache de derivados (via limites).
//
// Onde vive:
//  - `.astra/imports/<guid>.profile` — o perfil com que a fonte foi publicada;
//    reimportar e reabrir o projeto usam este.
//  - `.astra/import-default.profile` — o padrão do projeto para fontes novas
//    (preset reutilizável, I19).
// Arquivo ausente ou inválido cai no próximo nível (fonte → projeto → embutido),
// com o valor embutido igual ao comportamento anterior ao perfil.
inline constexpr u32 ImportProfileSchema = 2; // 2: normais e tangentes derivadas
// Escala uniforme aplicada às raízes (I01). Passos, não campo livre: o toque no
// aparelho erra fácil um número digitado, e os casos reais são conversões de unidade.
inline constexpr std::array<float, 9> ImportScaleSteps{0.001f, 0.01f, 0.1f, 0.5f, 1.0f, 2.0f, 10.0f, 100.0f, 1000.0f};
// Maior lado residente de cada textura (T07).
inline constexpr std::array<u32, 4> ImportTextureDimensionSteps{256, 512, 1024, 2048};
inline constexpr const char *ImportProfileDefaultPath = ".astra/import-default.profile";

struct ImportProfile {
  float scale = 1.0f;
  u32 maximumTextureDimension = 2048;
  // Geometria derivada (G2). Os nomes seguem o Model Import Settings da Unity —
  // Normals, Normals Mode, Tangents — e os valores são os de `gltf_import.h`,
  // para não existir uma segunda tabela de constantes entre perfil e importador.
  // Os padrões reproduzem exatamente o comportamento anterior ao campo.
  u8 normals = GltfNormalsImport;
  u8 normalWeighting = GltfNormalWeightArea;
  u8 tangents = GltfTangentsImport;
};
bool sameImportProfile(const ImportProfile &a, const ImportProfile &b) noexcept;
bool validImportProfile(const ImportProfile &profile) noexcept;
std::string serializeImportProfile(const ImportProfile &profile);
// Falha fechada: schema diferente, campo fora dos passos ou JSON inválido.
bool parseImportProfile(std::string_view text, ImportProfile &out);
GltfImportLimits applyImportProfile(GltfImportLimits limits, const ImportProfile &profile);
std::string importProfilePath(const AssetGuid &source);
// Índice do passo mais próximo, para os controles de passo.
u32 nearestImportScaleStep(float scale) noexcept;
} // namespace ae::resources

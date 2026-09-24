#pragma once
#include "core/base.h"
#include "resources/asset_registry.h"
#include "resources/gltf_import.h"
#include "resources/mesh_derived.h"
#include <array>
#include <string>
#include <string_view>
#include <vector>

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
inline constexpr u32 ImportProfileSchema = 9; // 9: compressão das texturas no aparelho (S1)
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
  // Smoothing Angle, em graus inteiros. Perfil NOVO nasce com os 60° da Unity;
  // um perfil gravado antes do campo (schema < 5) lê 180, que é suavizar tudo —
  // o comportamento com que ele foi publicado.
  u32 smoothingAngle = 60;
  u8 tangents = GltfTangentsImport;
  // "Import Cameras" da Unity. Padrão desligado porque ligá-lo muda o que a
  // cena recebe (objetos com componente de câmera) e o padrão precisa ser o
  // comportamento que os projetos já publicados tiveram.
  bool importCameras = false;
  bool importLights = false;
  // Formato das texturas PNG/JPEG no aparelho (S1), `resources::TextureCompression`:
  // 0 RGBA8, 4/6/8 ASTC NxN. O valor EMBUTIDO é 0 — o comportamento com que as
  // fontes já publicadas foram preparadas; fonte NOVA recebe o preset de
  // importação (ASTC 6x6, o padrão da Unity no Android) em `EditorSession`.
  // Aparelho sem ASTC recebe RGBA8 pelos limites, sem mudar a escolha salva.
  u8 textureCompression = 0;
  // Nós da fonte que NÃO vêm para a cena, por identidade do mapa de nós. A
  // exclusão vale para a subárvore. Identidade, e não nome nem caminho: o nó
  // renomeado no editor 3D continua excluído, porque o mapa o reconhece como o
  // mesmo nó. Não muda a saída do importador — muda o que a reconciliação
  // instancia —, e por isso não entra nos limites nem na chave do cache.
  std::vector<AssetGuid> excludedNodes;
  // Recursos derivados por primitiva. Não alteram o parse do GLB; são
  // recompostos depois que as identidades persistentes das primitivas existem.
  std::vector<CollisionMeshRecipe> collisionMeshes;
  bool excludes(const AssetGuid &node) const noexcept {
    for (const auto &excluded : excludedNodes) if (excluded == node) return true;
    return false;
  }
};
bool sameImportProfile(const ImportProfile &a, const ImportProfile &b) noexcept;
// O que muda a SAÍDA DO IMPORTADOR para os mesmos bytes. A exclusão de nós não
// entra: ela muda o que a cena recebe, não a geometria preparada. É esta
// comparação que decide se a prévia precisa ser preparada de novo — pedir nova
// preparação por um nó desmarcado seria reler o arquivo inteiro à toa.
bool sameImportPreparation(const ImportProfile &a, const ImportProfile &b) noexcept;
bool validImportProfile(const ImportProfile &profile) noexcept;
std::string serializeImportProfile(const ImportProfile &profile);
// Falha fechada: schema diferente, campo fora dos passos ou JSON inválido.
bool parseImportProfile(std::string_view text, ImportProfile &out);
GltfImportLimits applyImportProfile(GltfImportLimits limits, const ImportProfile &profile);
std::string importProfilePath(const AssetGuid &source);
// Índice do passo mais próximo, para os controles de passo.
u32 nearestImportScaleStep(float scale) noexcept;
} // namespace ae::resources

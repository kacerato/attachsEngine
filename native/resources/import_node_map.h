#pragma once
#include "core/base.h"
#include "resources/asset_registry.h"
#include "resources/gltf_import.h"
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ae::resources {

// Mapa persistente dos nós de uma fonte importada (M08.2).
//
// É o que responde "este nó do arquivo novo é qual nó do arquivo anterior?".
// Nome e índice de array são pistas, nunca a identidade inteira: nomes se
// repetem, exportadores reordenam, e um nó renomeado continua sendo a mesma
// peça. A identidade de um nó nasce uma vez e passa de revisão em revisão pela
// correspondência; o arquivo guarda o resultado para que a próxima reimportação
// parta dele, e não do zero.
//
// Ordem das evidências, da mais forte para a mais fraca:
//  1. identificador autoral no `extras` do nó (`astra_id`, `uuid`, `guid` ou
//     `id`), quando único nos dois lados;
//  2. mesmo pai já correspondido + mesmo nome, desempatando por geometria e
//     pose idênticas;
//  3. renomeação: mesmo pai, geometria e pose idênticas, únicas dos dois lados;
//  4. mudança de pai: mesmo nome e geometria, únicos no arquivo inteiro.
// Sobrou mais de um candidato para mais de um nó? É AMBIGUIDADE. Ela não é
// resolvida pela ordem da lista a menos que o usuário escolha isso.
struct ImportNodeRecord {
  AssetGuid id;
  AssetGuid parent; // inválido na raiz
  std::string name;
  std::string authoredId;
  // Assinatura da geometria do nó: quantidade de primitivas e, por primitiva,
  // índices e limites. Zero para nó sem malha.
  u64 signature = 0;
  float localMatrix[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  // Identidade de cada desenho do nó, na ordem das primitivas.
  std::vector<AssetGuid> draws;
  // Revisão em que o nó apareceu. Uma instância reconciliada numa revisão
  // anterior sem este nó recebe o nó; uma que já o conhecia e não o tem mais
  // foi editada localmente, e a remoção é respeitada.
  u32 introduced = 1;
  // Câmera que o nó carregava no arquivo, quando o perfil pediu para importar.
  //
  // Ela viaja pelo mapa, e não direto do importador para a cena, porque é o mapa
  // que sobrevive à reimportação: é ele que diz qual objeto da cena corresponde
  // a este nó. O componente de câmera é criado só quando o OBJETO é criado —
  // numa reimportação, uma câmera que o autor removeu ou reconfigurou fica como
  // ele deixou, pela mesma regra que vale para o resto da edição local.
  bool camera = false;
  bool cameraOrthographic = false;
  float cameraVerticalFov = 60, cameraNear = .1f, cameraFar = 0, cameraHalfHeight = 5;
  // O perfil da fonte pediu para não trazer este nó — ou um ancestral dele —
  // para a cena.
  //
  // O nó CONTINUA no mapa, com a identidade dele. Excluir não é a fonte ter
  // perdido o nó: se a exclusão apagasse o registro, reincluir depois criaria
  // uma identidade nova, e os objetos que o autor tinha montado em cima do nó
  // não voltariam a casar. Para a cena, o excluído se comporta como removido —
  // sai quem não tem edição local, fica órfão quem tem — pela mesma regra que
  // a reconciliação já aplica.
  bool excluded = false;
};

struct ImportNodeMap {
  static constexpr u32 FormatVersion = 3; // 2: camera do no; 3: no excluido pelo perfil
  static constexpr usize MaximumNodes = 65536;
  u32 revision = 0;
  std::string sourceHash;
  std::vector<ImportNodeRecord> nodes; // ordem do arquivo: pai antes do filho
  const ImportNodeRecord *find(const AssetGuid &id) const;
  i32 indexOf(const AssetGuid &id) const;
  bool valid() const;
  std::string serialize() const;
  static bool deserialize(std::string_view text, ImportNodeMap &out);
};

enum class ImportAmbiguityPolicy : u8 {
  Refuse,       // relata e não publica: quem decide é o usuário
  MatchInOrder, // o usuário escolheu associar pela ordem do arquivo
  TreatAsNew    // o usuário escolheu tratar como peças novas
};

struct ImportAmbiguity {
  std::string parent; // nome do pai no arquivo novo, vazio na raiz
  std::string name;
  u32 previous = 0, incoming = 0;
};

struct ImportMatchReport {
  u32 byAuthoredId = 0, byStructure = 0, renamed = 0, reparented = 0;
  u32 added = 0, removed = 0;
  // Mesmos BYTES da revisão anterior. Não quer dizer "nada muda na cena": a
  // exclusão de nós pode mudar com o mesmo arquivo, e é contada à parte.
  bool sameContent = false;
  // Nós que entraram na exclusão (`excluded`) ou saíram dela (`included`) em
  // relação à revisão anterior. Qualquer um dos dois avança a revisão do mapa.
  u32 excluded = 0, included = 0;
  std::vector<AssetGuid> removedNodes;
  std::vector<ImportAmbiguity> ambiguities;
};

// Assinatura geométrica de cada nó do modelo, calculada dos desenhos.
std::vector<u64> importNodeSignatures(const GltfImport &model);

// Constrói o mapa do modelo novo. Sem `previous`, todas as identidades nascem
// agora — deterministicamente a partir da fonte e do caminho de nomes, e as dos
// desenhos pela chave legada, para que cenas gravadas antes do mapa continuem
// apontando para as mesmas malhas.
//
// Devolve falso quando há ambiguidade e a política é `Refuse`, ou quando o
// modelo é incoerente; `report` explica.
//
// `excludedNodes` são identidades de nó que o perfil da fonte pede para não
// trazer à cena; a exclusão vale para a subárvore inteira.
bool buildImportNodeMap(const GltfImport &model, const AssetGuid &source, std::string_view contentHash,
                        const ImportNodeMap *previous, ImportAmbiguityPolicy policy, ImportNodeMap &out,
                        ImportMatchReport &report, std::string &diagnostic,
                        std::span<const AssetGuid> excludedNodes = {});

// Marca `excluded` nos nós pedidos e em toda a subárvore deles. Não mexe em
// revisão nem em `introduced` — isso é decisão de publicação, feita pelo
// `buildImportNodeMap`. Existe separada porque a prévia precisa refazer a marca
// a cada toque do autor sem reler o arquivo; e é a MESMA regra nos dois lugares.
void markExcludedNodes(ImportNodeMap &map, std::span<const AssetGuid> excluded);

// Caminho do mapa dentro do projeto.
std::string importNodeMapPath(const AssetGuid &source);

} // namespace ae::resources

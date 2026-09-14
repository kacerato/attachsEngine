#pragma once
#include "core/base.h"
#include "renderer/map_package.h"
#include <span>
#include <string>
#include <vector>

namespace ae::resources {

// Importação de GLB estático, feita NO APARELHO.
//
// Escopo declarado, porque o que não está aqui não pode ser apresentado como
// pronto:
//
// | Traz | Não traz |
// |---|---|
// | hierarquia de nós, com TRS e `matrix` | skins e animações |
// | instâncias (o mesmo mesh em vários nós) | câmeras e luzes do arquivo |
// | TRIANGLES, TRIANGLE_STRIP e TRIANGLE_FAN | pontos e linhas |
// | POSITION, NORMAL, TEXCOORD_0/1, TANGENT, COLOR_0 | texturas (imagens) |
// | fatores PBR, emissivo, alfa e dupla face | Draco, meshopt e materiais avançados |
//
// Texturas ficam de fora por um motivo concreto, não por preguiça: o runtime
// não decodifica PNG nem JPEG, e o decodificador do sistema (`AImageDecoder`)
// só existe a partir do Android 11 enquanto o mínimo suportado é o 8. Trazer
// meia textura seria pior do que não trazer nenhuma — o material chega com os
// fatores, que é o que o shader consome sem imagem.
//
// Conversão de coordenadas: **nenhuma**. glTF é destro, +Y para cima, e é assim
// que o pacote de mapa desta engine já guarda geometria — o cozinhador offline
// (`tools/cook-gltf-map.py`) também não converte. Introduzir um espelhamento
// aqui faria o mesmo arquivo chegar diferente conforme o caminho de entrada.
//
// Só GLB: um `.gltf` aponta para `.bin` e imagens ao lado dele, e o seletor do
// Android entrega UM arquivo, não a pasta. Recusar com diagnóstico é honesto;
// aceitar e importar sem geometria não é.
struct GltfImportLimits {
  u64 maximumBytes = 256ull << 20;
  u32 maximumNodes = 8192;
  u32 maximumDraws = 4096;
  u64 maximumVertices = 4ull << 20;
  u64 maximumIndices = 12ull << 20;
};

// Progresso e cancelamento. A importação chama `report` em pontos onde o
// trabalho já avançou de verdade, e consulta `cancelled` com frequência
// suficiente para o usuário desistir de um arquivo grande sem esperar o fim.
struct GltfImportProgress {
  void (*report)(void *context, float fraction, const char *stage) = nullptr;
  bool (*cancelled)(void *context) = nullptr;
  void *context = nullptr;
};

// Um nó da árvore do arquivo, com a transformação LOCAL e o pai.
//
// A lista de desenhos é uma saída para renderização; ela não é a árvore. Sem
// esta representação, importar achata a hierarquia: as partes nascem irmãs na
// raiz e mover a carroceria não leva a porta junto. Nós sem malha — grupos,
// pivôs, alvos — sobrevivem porque são exatamente o que segura a articulação.
struct GltfImportNode {
  std::string name;
  i32 parent = -1; // -1 é raiz
  float localMatrix[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  // Identificador que o autor gravou no `extras` do nó, quando existe. É a
  // evidência mais forte de que um nó reexportado é o mesmo (M08.2).
  std::string authoredId;
};

struct GltfImport {
  // Explicit losses in the static geometry profile, shown before publication.
  // Required appearance extensions are preserved in the original GLB, not
  // misrepresented as implemented renderer features.
  std::vector<std::string> appearanceExtensions;
  // Geometria em espaço LOCAL do nó: `model` é identidade e os limites são do
  // mesh. A pose vem do nó, não da matriz do desenho.
  std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  // Nome de cada material do arquivo, alinhado a `materials` (vazio quando o
  // arquivo não dá nome; o último é o material neutro). É o que o inspetor mostra
  // num slot que usa o material da fonte.
  std::vector<std::string> materialNames;
  std::vector<u8> vertices; // passo renderer::MapVertexStride
  std::vector<u32> indices;
  std::vector<std::string> names; // um nome por desenho, para a hierarquia
  // Chave estável de cada desenho dentro do arquivo, para a identidade do
  // recurso sobreviver a uma reimportação.
  //
  // Índice de array NÃO é identidade: acrescentar um objeto no editor 3D
  // reindexa tudo o que vem depois, e a cena passaria a apontar para a malha
  // errada na reimportação seguinte. A chave usa os NOMES — do nó, da malha e a
  // ordem da primitiva dentro dela, que é o único componente que o formato não
  // dá nome. Renomear no editor 3D quebra a ligação, e isso é dito ao usuário
  // em vez de resolvido por adivinhação.
  std::vector<std::string> keys;
  // A árvore do arquivo e, por desenho, o índice do nó dono.
  std::vector<GltfImportNode> nodes;
  std::vector<u32> drawNodes;
  // Motivo concreto quando `importGlb` devolve falso. Nunca "erro ao importar".
  std::string diagnostic;
  bool cancelled = false;
  // O que o arquivo trazia e esta importação deliberadamente não trouxe. Quem
  // chama publica isso: o usuário precisa saber que a animação ficou para trás.
  u32 skippedTextures = 0, skippedAnimations = 0, skippedSkins = 0;
  u32 skippedPrimitives = 0, skippedCameras = 0, skippedLights = 0;
  bool anythingSkipped() const noexcept {
    return !appearanceExtensions.empty() || skippedTextures || skippedAnimations || skippedSkins || skippedPrimitives ||
           skippedCameras || skippedLights;
  }
};

// Falha fechada: em qualquer erro, `out` volta vazio com `diagnostic` preenchido
// e nenhum recurso existente é tocado. Quem chama só publica depois do sucesso.
bool importGlb(std::span<const u8> bytes, const GltfImportLimits &limits,
               const GltfImportProgress &progress, GltfImport &out);

} // namespace ae::resources

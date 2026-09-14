#pragma once
#include "core/base.h"
#include "renderer/authoring_texture.h"
#include "renderer/map_package.h"
#include "resources/image_decode.h"
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
// | POSITION, NORMAL, TEXCOORD_0/1, TANGENT, COLOR_0 | WebP, AVIF e imagens externas |
// | fatores PBR, emissivo, alfa e dupla face | materiais avançados |
// | texturas PNG/JPEG/KTX2 embutidas (cor, normal, MR, emissiva) | oclusão |
// | KHR_texture_transform assado nas UVs quando o material concorda | transformação que diverge no material |
// | Draco e meshopt (resources/gltf_codecs.h) | KTX2 HDR, cubemap e array |
//
// Texturas (Entrega 3) são decodificadas com stb_image vendorizado, porque o
// decodificador do sistema (`AImageDecoder`) só existe a partir do Android 11
// e o mínimo suportado é o 8. Oclusão e transformação de UV são contadas como
// não aplicadas: o bloco de push constants do shader já está cheio.
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
  // Memória das texturas DEPOIS de decodificadas, com mipmaps. O tamanho
  // comprimido do GLB não diz nada sobre isto: um JPEG de 2 MB vira 85 MB de
  // RGBA 4096² com mips.
  u64 maximumTextureBytes = 256ull << 20;
  // Resolução RESIDENTE máxima de cada textura. Uma imagem maior perde os mips
  // de cima (o nível residente fica com o maior lado <= este valor) em vez de
  // ser descartada inteira — é o que o pacote de mapa já faz com o orçamento.
  u32 maximumTextureDimension = 2048;
  // Quando o arquivo inteiro não cabe em `maximumTextureBytes` nem com o limite
  // acima, o limite é reduzido PARA TODAS as texturas (metade por vez) até este
  // piso, antes de qualquer textura ser descartada. Descartar em ordem de
  // declaração deixaria o fim do arquivo sem textura enquanto o começo fica em
  // resolução cheia.
  u32 minimumTextureDimension = 256;
  // Bytes que os codecs de geometria (Draco, meshopt) podem produzir por
  // importação. O tamanho comprimido não limita a expansão: um bloco de poucos
  // KB pode declarar milhões de vértices.
  u64 maximumExpandedBytes = 512ull << 20;
  ImageDecodeLimits image{};
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
  // Texturas decodificadas (Entrega 3), indexadas por
  // `MapMaterialRecord::textureIndices`. Uma mesma imagem usada como cor (sRGB)
  // e como dado (linear) vira duas texturas: o espaço de cor é da textura.
  std::vector<renderer::SharedAuthoringTexture> textures;
  u64 textureBytes = 0;
  // O que o arquivo trazia e esta importação deliberadamente não trouxe. Quem
  // chama publica isso: o usuário precisa saber que a animação ficou para trás.
  // `skippedTextures` conta referências de textura que NÃO foram aplicadas
  // (formato sem decodificador, imagem externa, orçamento, erro de decodificação).
  u32 skippedTextures = 0, skippedAnimations = 0, skippedSkins = 0;
  u32 skippedPrimitives = 0, skippedCameras = 0, skippedLights = 0;
  // Aparência que o perfil ainda não reproduz, contada por slot: a textura é
  // aplicada sem a transformação de UV, e a oclusão não entra no shader.
  u32 unappliedTextureTransforms = 0, unappliedOcclusion = 0;
  // Referências com KHR_texture_transform assadas nas UVs (Entrega 4): todas as
  // texturas do material naquele conjunto de UV concordavam na transformação.
  // As que divergem continuam em `unappliedTextureTransforms`.
  u32 bakedTextureTransforms = 0;
  // Texturas aplicadas com resolução reduzida por `maximumTextureDimension`.
  u32 reducedTextures = 0;
  // Uso efetivo dos codecs (Entrega 4): primitivas Draco decodificadas,
  // bufferViews meshopt decodificadas e imagens KTX2 transcodificadas.
  u32 dracoPrimitives = 0, meshoptViews = 0, ktx2Images = 0;
  // Nós cuja transformação de mundo tem reflexão (escala negativa). A pose local
  // sai com escala positiva e a geometria desses nós sai espelhada — o resultado
  // no mundo é o mesmo do arquivo, sem aproximação.
  u32 mirroredNodes = 0;
  // Primitivas com mapa normal que chegaram sem TANGENT e tiveram as tangentes
  // geradas na importação.
  u32 generatedTangentPrimitives = 0;
  // Maior lado residente escolhido para este arquivo (0 sem texturas).
  u32 residentTextureDimension = 0;
  // Motivos concretos das texturas não aplicadas, sem repetição.
  std::vector<std::string> textureNotes;
  bool anythingSkipped() const noexcept {
    return !appearanceExtensions.empty() || skippedTextures || skippedAnimations || skippedSkins || skippedPrimitives ||
           skippedCameras || skippedLights || unappliedTextureTransforms || unappliedOcclusion;
  }
};

// Falha fechada: em qualquer erro, `out` volta vazio com `diagnostic` preenchido
// e nenhum recurso existente é tocado. Quem chama só publica depois do sucesso.
bool importGlb(std::span<const u8> bytes, const GltfImportLimits &limits,
               const GltfImportProgress &progress, GltfImport &out);

} // namespace ae::resources

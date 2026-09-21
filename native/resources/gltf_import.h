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
// | POSITION, NORMAL, TEXCOORD_0/1, TANGENT, COLOR_0 | WebP, AVIF e imagens por URI dentro deste leitor |
// | fatores PBR, emissivo, alfa e dupla face | materiais avançados |
// | texturas PNG/JPEG/KTX2 embutidas (cor, normal, MR, emissiva) | oclusão |
// | KHR_texture_transform assado nas UVs quando o material concorda | transformação que diverge no material |
// | Draco e meshopt (resources/gltf_codecs.h) | KTX2 HDR, cubemap e array |
//
// Texturas (Entrega 3) são decodificadas com stb_image vendorizado, porque o
// decodificador do sistema (`AImageDecoder`) só existe a partir do Android 11
// e o mínimo suportado é o 8. A oclusão é contada como não aplicada: o bloco de
// push constants do material não tem espaço para ela. A transformação de UV é
// assada nas UVs quando todas as texturas do material concordam no conjunto de
// UV (Entrega 4); quando divergem, continua contada como não aplicada.
//
// Conversão de coordenadas: **nenhuma**. glTF é destro, +Y para cima, e é assim
// que o pacote de mapa desta engine já guarda geometria — o cozinhador offline
// (`tools/cook-gltf-map.py`) também não converte. Introduzir um espelhamento
// aqui faria o mesmo arquivo chegar diferente conforme o caminho de entrada.
//
// Este leitor recebe só GLB autocontido. Um `.gltf`, ou um GLB com URIs
// externas, passa antes por `resources/gltf_package.h` (Entrega 4): o seletor
// do Android entrega os arquivos escolhidos JUNTO com o principal, e as
// dependências entram num GLB único, com manifesto `.deps` ao lado da fonte.
// Uma URI que chegue até aqui sem ter sido empacotada é recusada com diagnóstico.
// Normais e tangentes: escolha AUTORAL, do perfil de importação, e não detecção
// silenciosa. Os nomes seguem o Model Import Settings da Unity (Normals,
// Normals Mode, Tangents), com uma diferença deliberada: não existe a opção
// "None". Uma normal (0,0,0) vira base TBN degenerada e superfície preta neste
// renderer, então oferecer "sem normal" seria oferecer um defeito.
inline constexpr u8 GltfNormalsImport = 0;    // usa NORMAL do arquivo; gera o que faltar
inline constexpr u8 GltfNormalsCalculate = 1; // ignora NORMAL do arquivo e gera tudo
inline constexpr u8 GltfNormalWeightArea = 0;  // contribuição proporcional à área do triângulo
inline constexpr u8 GltfNormalWeightAngle = 1; // contribuição proporcional ao ângulo no vértice
// Smoothing Angle do Model Import Settings: entre faces cujo ângulo passa deste
// limite a aresta fica dura — o vértice é dividido e cada lado leva a própria
// normal. 180 suaviza tudo (o comportamento de antes do campo).
inline constexpr float GltfSmoothingAngleDefault = 60.0f;
inline constexpr float GltfSmoothingAngleNone = 180.0f;
inline constexpr u8 GltfTangentsImport = 0;    // usa TANGENT do arquivo; gera quando faltar e houver mapa normal
inline constexpr u8 GltfTangentsCalculate = 1; // gera sempre que houver UV, mesmo com TANGENT no arquivo

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
  // Aparelho amostra ASTC 4x4 (informado pelo renderer). Com isso, KTX2 com a
  // cadeia completa de mips vira blocos ASTC — 8 bits por texel em vez de 32 —
  // sem RGBA intermediário. Sem isso, ou sem mips no arquivo, RGBA8 como antes.
  bool astc4x4 = false;
  // Escala uniforme das raízes, vinda do perfil de importação (R3,
  // `resources/import_profile.h`). Aplicada à pose local das raízes: a geometria
  // continua no espaço do nó e os filhos herdam pela hierarquia.
  float rootScale = 1.0f;
  // Geometria derivada, também do perfil. `GltfNormalsImport` e
  // `GltfTangentsImport` reproduzem o comportamento anterior ao perfil.
  u8 normals = GltfNormalsImport;
  u8 normalWeighting = GltfNormalWeightArea;
  // Vale para toda normal que a importação GERA: Calcular, ou arquivo sem
  // NORMAL. 180 aqui reproduz quem chama o importador sem perfil.
  float smoothingAngle = GltfSmoothingAngleNone;
  u8 tangents = GltfTangentsImport;
  // "Import Cameras" do Model Import Settings. Desligado reproduz o
  // comportamento anterior, em que a câmera do arquivo era contada como perdida.
  bool importCameras = false;
  // "Import Lights" do Model Import Settings. Segue a mesma política das
  // câmeras: desligado preserva o comportamento dos perfis existentes.
  bool importLights = false;
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

// Uma câmera do arquivo, ligada ao nó que a carrega.
//
// Ela chega como DADO da importação, não como componente: quem monta a cena é o
// editor, que sabe transformar nó em objeto. O importador não conhece
// `scene::Camera` — se conhecesse, a camada de recursos passaria a depender da
// de cena e o cozinhador offline teria de conhecê-la também.
//
// glTF guarda `yfov` em radianos e `zfar` opcional (câmera infinita). Os dois
// viram os campos que a câmera desta engine já tem, com a conversão explícita
// aqui e não espalhada por quem consome.
struct GltfImportCamera {
  u32 node = 0;
  bool orthographic = false;
  float verticalFovDegrees = 60;
  float nearPlane = .1f;
  // Zero significa "o arquivo não declarou": quem monta escolhe o próprio
  // padrão em vez de receber um infinito que o componente recusaria.
  float farPlane = 0;
  // Metade da altura visível na projeção ortográfica (`ymag` do glTF).
  float orthographicHalfHeight = 5;
};

// Luz KHR_lights_punctual ligada ao nó que a carrega. `kind` usa a ordem do
// contrato da cena (direcional, pontual, spot), sem criar dependência da camada
// de recursos com `scene::Light`.
struct GltfImportLight {
  u32 node = 0;
  u8 kind = 1;
  float color[3]{1, 1, 1};
  float intensity = 1;
  // O glTF permite alcance infinito; o componente autoral tem teto explícito.
  // Quando `rangeDeclared` é falso, a reconciliação usa o maior alcance que o
  // contrato Astra representa e o importador publica uma nota sobre a adaptação.
  float range = 1000;
  bool rangeDeclared = false;
  float innerAngle = 0;
  float outerAngle = 45;
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
  // Câmeras do arquivo, quando o perfil pede para importá-las.
  std::vector<GltfImportCamera> cameras;
  std::vector<GltfImportLight> lights;
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
  // R4: materiais com oclusão lida do canal R do mapa metálico/rugosidade.
  u32 appliedOcclusion = 0;
  // Referências com KHR_texture_transform assadas nas UVs (Entrega 4): todas as
  // texturas do material naquele conjunto de UV concordavam na transformação.
  // As que divergem continuam em `unappliedTextureTransforms`.
  u32 bakedTextureTransforms = 0;
  // Texturas aplicadas com resolução reduzida por `maximumTextureDimension`.
  u32 reducedTextures = 0;
  // Uso efetivo dos codecs (Entrega 4): primitivas Draco decodificadas,
  // bufferViews meshopt decodificadas e imagens KTX2 transcodificadas.
  u32 dracoPrimitives = 0, meshoptViews = 0, ktx2Images = 0;
  // Texturas KTX2 que subiram como ASTC 4x4 em vez de RGBA8.
  u32 astcTextures = 0;
  // Nós cuja transformação de mundo tem reflexão (escala negativa). A pose local
  // sai com escala positiva e a geometria desses nós sai espelhada — o resultado
  // no mundo é o mesmo do arquivo, sem aproximação.
  u32 mirroredNodes = 0;
  // Primitivas com mapa normal que chegaram sem TANGENT e tiveram as tangentes
  // geradas na importação.
  u32 generatedTangentPrimitives = 0;
  // Primitivas cujas normais foram calculadas aqui — porque o arquivo não trazia
  // NORMAL (o glTF permite; este renderer não, uma normal nula apaga a
  // superfície) ou porque o perfil pediu recálculo.
  u32 generatedNormalPrimitives = 0;
  // Diagnóstico de mapeamento (o caso "textura esticada" do plano): primitivas
  // que o material manda texturizar e que chegaram SEM o conjunto de UV que ele
  // amostra. Não há correção automática possível — sem UV não existe mapeamento
  // — então o relatório aponta a fonte em vez de a engine inventar coordenadas.
  u32 texturedPrimitivesWithoutUv = 0;
  // Primitivas cuja densidade de texel varia muito entre triângulos no mesmo
  // material: é o sintoma de UV esticada que sobrevive a qualquer resolução de
  // textura. Contado por primitiva, com a pior razão observada no arquivo.
  u32 stretchedUvPrimitives = 0;
  float worstTexelDensityRatio = 0;
  // Maior lado residente escolhido para este arquivo (0 sem texturas).
  u32 residentTextureDimension = 0;
  // Motivos concretos das texturas não aplicadas, sem repetição.
  std::vector<std::string> textureNotes;
  // Diagnósticos que não são de textura — câmera recusada, nó ignorado, o que
  // vier depois. Existe para que o próximo diagnóstico não acabe hospedado em
  // `textureNotes` só porque já havia uma lista ali.
  std::vector<std::string> notes;
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

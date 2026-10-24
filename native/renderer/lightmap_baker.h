#pragma once
// G3c: baker progressivo de lightmap em CPU, com jobs (o Progressive CPU
// Lightmapper da Unity 6, no que ele faz de essencial):
//
//  1. Cada objeto Static com Contribute GI e Receive GI = Lightmaps ganha um
//     retângulo numa página: lado = Lightmap Resolution × √(área de mundo /
//     área de UV2) × Scale In Lightmap, com Lightmap Padding em volta, até o
//     Max Lightmap Size.
//  2. A UV2 de cada triângulo é rasterizada no retângulo: cada texel vira um
//     ponto de mundo com normal (cobertura conservadora; o que sobra é dilatado).
//  3. BVH (SAH em caixas) de todos os triângulos que contribuem; raios com
//     teste de alfa recortado e faces de trás.
//  4. Luz direta das luzes em modo Assado (sombra por raio, raio de sombra
//     suave pelo Baked Shadow Radius), e a direta de Assado+Misto que alimenta
//     os rebotes.
//  5. Rebotes por passes (1..4): cada texel lança N raios em cosseno; o que o
//     raio acerta devolve albedo/π × irradiância do passe anterior + emissão;
//     o que escapa é céu. O céu é assado com radiância 1 e branco (visibilidade
//     do céu), o sol da Hora do dia não entra (G3 item 8).
//  6. Texels com mais da metade dos raios acertando o verso de uma face
//     (dentro de parede) são descartados e preenchidos pelos vizinhos.
//  7. Denoise à-trous guiado por normal e posição, dilatação até o padding e
//     compressão (RGBA8 com raiz quadrada) ou RGBA16F.
//
// Roda numa thread própria com N trabalhadores (prioridade baixa no aparelho),
// com progresso e cancelamento. Não conhece editor nem Vulkan.
#include "renderer/lightmap_data.h"

#include <atomic>
#include <memory>

namespace ae::renderer {

// Geometria LOCAL de uma malha (um slot de Mesh Renderer).
struct LightmapBakeMesh {
  std::vector<float> positions; // xyz
  std::vector<float> normals;   // xyz
  std::vector<float> uv0;       // uv para o albedo
  std::vector<float> uv1;       // UV2 do lightmap
  std::vector<u32> indices;
  bool uv1Valid = false;        // sem sobreposição (scene::auditLightmapUv)
};
// Cor base decodificada em baixa resolução (a prévia RGBA8 da textura).
struct LightmapBakeTexture {
  u32 width = 0, height = 0;
  bool srgb = true;
  std::vector<u8> rgba;
};
struct LightmapBakeMaterial {
  float baseColor[4]{1, 1, 1, 1}; // linear
  i32 baseTexture = -1;
  float uvTransform[6]{1, 0, 0, 0, 1, 0}; // u' = a·u + b·v + c, v' = d·u + e·v + f
  float emission[3]{};                    // radiância linear (cor × potência)
  float alphaCutoff = .5f;
  bool alphaTest = false;
  bool doubleSided = false;
};
struct LightmapBakeInstance {
  u64 objectId = 0;
  u32 slot = 0;
  u32 mesh = 0, material = 0;
  float model[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  bool contributes = true;  // entra na BVH (bloqueia e rebate)
  bool receives = true;     // ganha lightmap
  bool castsShadows = true; // bloqueia a luz direta assada
  float scaleInLightmap = 1;
};
struct LightmapBakeLight {
  u32 kind = 1; // 0 direcional, 1 pontual, 2 spot
  float position[3]{};
  float direction[3]{0, -1, 0}; // direção de emissão
  float radiance[3]{1, 1, 1};   // cor linear × intensidade, como no shader
  float range = 10;
  float coneScale = 0, coneOffset = 1; // clamp(dot(dir,-l)·escala+desl.)², como o shader
  float shadowRadius = 0;              // metros (local) ou radianos (direcional)
  bool bakeDirect = false;             // modo Assado: a direta vai para o lightmap
  bool castsShadows = true;
};
struct LightmapBakeScene {
  std::vector<LightmapBakeMesh> meshes;
  std::vector<LightmapBakeMaterial> materials;
  std::vector<LightmapBakeTexture> textures;
  std::vector<LightmapBakeInstance> instances;
  std::vector<LightmapBakeLight> lights;
};

enum class LightmapBakeStage : u32 { Preparing = 0, Bvh, Layout, Direct, Indirect, Filtering, Encoding, Done };
const char *lightmapBakeStageName(LightmapBakeStage stage) noexcept;

// Compartilhado entre a thread do bake e quem o mostra.
struct LightmapBakeProgress {
  std::atomic<float> fraction{0};
  std::atomic<u32> stage{0};
  std::atomic<u32> pass{0}, passes{0};
  std::atomic<bool> cancel{false};
};

struct LightmapBakeOptions {
  u32 threads = 0;          // 0: núcleos − 1 (mínimo 1)
  bool lowPriority = true;  // nice 10 no Linux/Android: o editor continua fluido
};

// Assina tudo que muda o resultado (geometria, poses, materiais, luzes,
// configurações). Igual ao `sourceHash` do recurso: o bake está em dia.
u64 lightmapSceneHash(const LightmapBakeScene &scene, const LightmapBakeSettings &settings) noexcept;

// Falso em erro ou cancelamento (`error` diz qual). `out` só é escrito no fim.
bool bakeLightmaps(const LightmapBakeScene &scene, const LightmapBakeSettings &settings, const LightmapBakeOptions &options,
                   LightmapBakeProgress &progress, LightmapData &out, std::string &error);

// Peças expostas para teste.
struct LightmapBakeRay { float origin[3], direction[3], maximum; };
// Interseção mais próxima contra a BVH de uma cena (montada a cada chamada;
// só para teste e diagnóstico).
bool lightmapDebugTrace(const LightmapBakeScene &scene, const LightmapBakeRay &ray, float &distance, u32 &instance);
// A BVH do bake montada uma vez, para diagnóstico (visibilidade num ponto,
// o que um raio acerta e se acerta o verso). Mesma interseção do bake.
class LightmapTracer {
public:
  explicit LightmapTracer(const LightmapBakeScene &scene);
  ~LightmapTracer();
  struct Result { bool hit = false, back = false; float distance = 0; u32 instance = 0; };
  Result trace(const float origin[3], const float direction[3], float maximum) const;
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace ae::renderer

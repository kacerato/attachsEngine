#pragma once
// Simulação de partículas em CPU (G5 P1).
//
// Um `ParticleEmitter` é o estado de execução de UM componente ParticleSystem:
// as partículas vivas em estrutura de arrays (SoA), o relógio do ciclo, o
// acumulador de emissão e as rajadas. Ele não conhece cena, editor nem Vulkan:
// recebe a matriz de mundo do objeto e o passo de tempo, e devolve instâncias
// prontas para a GPU. Por isso a mesma classe serve à prévia do editor, ao Play
// e aos testes de host.
//
// Determinismo: o gerador é PCG32 semeado pela semente do componente; cada
// partícula guarda uma semente própria e todo sorteio dela é um hash dessa
// semente com um canal fixo. Mesma semente + mesma sequência de passos = mesmas
// partículas, bit a bit, em qualquer thread (cada emissor roda em um job só).
//
// Curvas e gradientes são cozidos em tabelas de 64 amostras quando o componente
// muda: avaliar Hermite com validação por partícula por quadro custaria mais do
// que a simulação inteira.
#include "core/base.h"
#include "scene/particle_system.h"

#include <array>
#include <cmath>
#include <memory>
#include <span>
#include <vector>

namespace ae::renderer::particles {

// Instância por partícula, consumida como atributo por instância pelos
// rhi/shaders/particle_billboard.vert e particle_mesh.vert. 64 bytes; cor em
// meia precisão (HDR linear).
struct ParticleInstance {
  float position[3]{};
  float size = 0;      // largura (X); com Tamanho 3D, Y e Z ficam em sizeYZ
  float velocity[3]{};
  float rotation = 0;  // radianos, no plano da tela (billboard na Vista)
  u16 color[4]{};      // half float
  u32 random = 0;      // semente da partícula: desloca o ruído do material
  float frame = 0;     // PK-10a: quadro da folha de textura (índice na grade, da esquerda para a direita, de cima para baixo)
  // G6a: altura e profundidade (iguais à largura sem Tamanho 3D) e a
  // orientação 3D em quatérnio snorm16 (x, y, z, w). Lida com a flag
  // Orientada: malha e alinhamentos Mundo/Local/Velocidade.
  float sizeYZ[2]{};
  i16 orientation[4]{0, 0, 0, 32767};
};
static_assert(sizeof(ParticleInstance) == 64);

// Parâmetros do material de um lote, já resolvidos (tinta × emissão etc.).
struct ParticleMaterialParameters {
  u32 blend = 0, texture = 0, noise = 0, mask = 0;
  u32 renderMode = 0;
  // G6a: base de alinhamento (scene::ParticleRenderAlignment) e se a instância
  // traz orientação 3D (malha, Mundo, Local, Velocidade ou Rotação 3D).
  u32 alignment = 0;
  bool oriented = false;
  // G6a: Linha — no esticado, U corre pelo comprimento e V atravessa a faixa.
  bool lineUv = false;
  float tint[3]{1, 1, 1};
  float scroll[2]{}, noiseScroll[2]{};
  float noiseScale = 1, noiseStrength = 1, noiseDistortion = 0;
  float centerGlow = 0, centerGlowPower = 4, softDistance = 0;
  float stretchVelocityScale = 0, stretchLengthScale = 2, maxScreenSize = 0.5f;
  // PK-10a: grade da folha de textura (1x1 = imagem inteira) e a textura do
  // Material de projeto, já resolvida para o índice da biblioteca publicada
  // (~0 = textura própria). A cor base do Material multiplica a tinta.
  float sheetTiles[2]{1, 1};
  u32 projectTexture = ~0u;
};

struct ParticleBatch {
  u32 firstInstance = 0, instanceCount = 0;
  ParticleMaterialParameters material;
  float sortDistance = 0;  // distância à câmera + ajuste de ordem; maior desenha antes
  // G6a: malha do lote (índice em ParticleFrame::meshes->ranges), ~0 = quadrado.
  u32 mesh = ~0u;
};

// G6a: malhas das partículas, em CPU. Posição e UV por vértice; os intervalos
// apontam para dentro dos dois vetores. O conjunto é imutável depois de pronto:
// quem muda a lista cria outro, com revisão nova, e o renderer só sobe de novo
// quando a revisão muda.
struct ParticleMeshVertex {
  float position[3]{};
  float uv[2]{};
};
static_assert(sizeof(ParticleMeshVertex) == 20);
struct ParticleMeshRange {
  u32 firstIndex = 0, indexCount = 0;
  i32 vertexOffset = 0;
  float radius = 0.5f;  // raio envolvente
};
struct ParticleMeshSet {
  u64 revision = 0;
  std::vector<ParticleMeshVertex> vertices;
  std::vector<u32> indices;
  std::vector<ParticleMeshRange> ranges;
};
// Malhas embutidas (scene::ParticleMeshShape), na ordem do enum, já no espaço
// da Astra: a Quad da Unity refletida em Z fica de frente para +Z.
void appendBuiltinParticleMeshes(ParticleMeshSet &set);
// Acrescenta uma malha (posições xyz, UV, índices em triângulos). Falso quando
// passa do orçamento (65536 vértices por malha, 1 Mi de índices no conjunto).
bool appendParticleMesh(ParticleMeshSet &set, std::span<const float> positions, std::span<const float> uvs,
                        std::span<const u32> indices, u32 &range);

// PK-10b: luz pontual acesa por uma partícula (módulo Luzes), em mundo e já
// com a unidade da luz modelo resolvida (renderer::SceneLight).
struct ParticleLight {
  float position[3]{};
  float color[3]{1, 1, 1};
  float intensity = 1, range = 1;
  u32 modality = 0;  // renderer::LightModality da luz modelo (ponto ou spot)
  bool photometric = false;
  u32 cullingMask = ~0u, renderingLayers = 1;
};

// O que o renderer recebe por quadro. Lotes já ordenados de trás para a frente.
struct ParticleFrame {
  std::vector<ParticleInstance> instances;
  std::vector<ParticleBatch> batches;
  std::vector<ParticleLight> lights;
  std::shared_ptr<const ParticleMeshSet> meshes;
  u32 alive = 0, systems = 0, budget = 0;
  float simulationMs = 0, buildMs = 0;
  void clear() { instances.clear(); batches.clear(); lights.clear(); alive = systems = budget = 0; simulationMs = buildMs = 0; }
};

u16 floatToHalf(float value);
float halfToFloat(u16 value);

struct ParticleCurveLut {
  static constexpr u32 kSamples = 64;
  std::array<float, kSamples> values{};
  void bake(const scene::ScriptCurve &curve);
  float sample(float t) const;
};
struct ParticleGradientLut {
  static constexpr u32 kSamples = 64;
  std::array<std::array<float, 4>, kSamples> values{};
  void bake(const scene::ScriptGradient &gradient);
  void sample(float t, float (&out)[4]) const;
};
// Unity MinMaxCurve.Evaluate(tempo, fator): `t` normalizado, `random` em [0,1).
struct BakedMinMaxCurve {
  scene::ParticleCurveMode mode = scene::ParticleCurveMode::Constant;
  float constantMin = 0, constantMax = 0, multiplier = 1;
  ParticleCurveLut curveMin, curveMax;
  void bake(const scene::ParticleMinMaxCurve &source);
  float evaluate(float t, float random) const;
};
struct BakedMinMaxGradient {
  scene::ParticleGradientMode mode = scene::ParticleGradientMode::Color;
  float colorMin[4]{1, 1, 1, 1}, colorMax[4]{1, 1, 1, 1};
  ParticleGradientLut gradientMin, gradientMax;
  void bake(const scene::ParticleMinMaxGradient &source);
  void evaluate(float t, float random, float (&out)[4]) const;
};

// Plano de colisão em mundo: um ponto e a normal unitária (lado de cima).
struct ParticleCollisionPlane {
  float point[3]{}, normal[3]{0, 1, 0};
};
// Acontecimento de uma partícula para os sub-emissores: posição e velocidade
// em mundo no instante do nascimento, do toque ou da morte.
struct ParticleEvent {
  float position[3]{}, velocity[3]{};
  float normal[3]{0, 1, 0};  // G6a: normal da superfície no toque (mundo)
};

// PCG32 (O'Neill, pcg-random.org): pequeno, rápido e reproduzível.
struct Pcg32 {
  u64 state = 0x853c49e6748fea9bULL, increment = 0xda3e39cb94b95bdbULL;
  void seed(u64 value, u64 sequence = 54u);
  u32 next();
  float unit() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }
};
// Sorteio por canal a partir da semente da partícula (hash de 32 bits).
float particleRandom(u32 seed, u32 channel);

class ParticleEmitter {
public:
  enum class State : u32 { Stopped, Playing, Paused };

  // Copia as configurações e recozinha curvas. Não apaga partículas vivas: editar
  // no inspetor continua a simulação, como na Unity com o efeito selecionado.
  void configure(const scene::ParticleSystemSettings &settings, u32 seed);
  // Teto efetivo deste quadro (autoral reduzido pelo nível do aparelho).
  void setCapacityLimit(u32 limit) { capacityLimit_ = limit; }
  // Planos do módulo Colisão neste quadro, em mundo (no máximo dois).
  void setCollisionPlanes(const ParticleCollisionPlane *planes, u32 count);
  // G6a: Colisão Mundo. O dono (runtime/scene_particles) entrega a consulta à
  // física; sem ela (prévia do editor) as partículas atravessam. Recebe o
  // segmento em mundo e devolve ponto e normal do primeiro toque.
  using WorldRaycast = bool (*)(void *context, const float (&from)[3], const float (&to)[3], float radius,
                                float (&point)[3], float (&normal)[3]);
  void setWorldCollider(WorldRaycast raycast, void *context, u32 maximumQueries) {
    worldRaycast_ = raycast; worldContext_ = context; worldQueries_ = maximumQueries;
  }
  // Um ouvinte de colisão (Instanciar ao colidir) quer os toques mesmo sem
  // sub-emissor de colisão.
  void setCollisionEventsWanted(bool wanted) { collisionListeners_ = wanted; }
  // G6a: Modo de escala Hierarquia/Local: multiplica o tamanho desenhado.
  void setSizeScale(float scale) { sizeScale_ = std::isfinite(scale) && scale > 0 ? scale : 1.0f; }
  // Acontecimentos do último simulate/emit, para os sub-emissores. Só são
  // registrados quando o módulo Sub-emissores pede aquele tipo, e no máximo
  // kMaximumEvents por tipo e quadro (o resto é descartado, não adiado).
  static constexpr u32 kMaximumEvents = 256;
  const std::vector<ParticleEvent> &births() const noexcept { return births_; }
  const std::vector<ParticleEvent> &collisions() const noexcept { return collisions_; }
  const std::vector<ParticleEvent> &deaths() const noexcept { return deaths_; }
  void clearEvents() { births_.clear(); collisions_.clear(); deaths_.clear(); }

  void play();
  // `clear` = StopEmittingAndClear; sem ele as vivas terminam a vida.
  void stop(bool clear);
  void pause();
  void clear();
  // Emite agora pela forma, mesmo parado (Unity ParticleSystem.Emit).
  void emit(u32 count, const float (&world)[16]);
  // Reinicia: limpa, volta o relógio e a semente e toca.
  void restart();

  // Passo de simulação. `world` é a matriz de mundo do objeto neste quadro.
  void simulate(float deltaSeconds, const float (&world)[16]);
  // Instâncias deste emissor, em mundo, na ordem do modo de ordenação.
  void build(const float (&world)[16], const float (&camera)[3], std::vector<ParticleInstance> &out) const;
  // PK-10c: segmentos das trilhas, um por par de pontos, como billboards
  // esticados (posição = ponto mais novo, velocidade = do mais velho ao novo).
  void buildTrails(const float (&world)[16], std::vector<ParticleInstance> &out) const;
  // Material dos segmentos: o do sistema em modo esticado, comprimento exato.
  ParticleMaterialParameters trailMaterial() const;
  ParticleMaterialParameters material() const;

  State state() const noexcept { return state_; }
  bool playing() const noexcept { return state_ == State::Playing; }
  bool emitting() const noexcept { return emitting_; }
  u32 alive() const noexcept { return static_cast<u32>(age_.size()); }
  float time() const noexcept { return systemTime_; }
  // Verdadeiro no quadro em que o sistema terminou (sem emissão e sem vivas).
  bool takeFinished() noexcept { const bool value = finished_; finished_ = false; return value; }
  const scene::ParticleSystemSettings &settings() const noexcept { return settings_; }
  float sortingFudge() const noexcept { return settings_.sortingFudge; }
  u32 capacity() const noexcept;

private:
  void spawn(u32 count, const float (&world)[16], float systemNormalized, float spreadSeconds);
  void sampleShape(Pcg32 &rng, float (&position)[3], float (&direction)[3]) const;
  void kill(u32 index);
  void record(std::vector<ParticleEvent> &events, u32 index, const float (&world)[16], const float *normal = nullptr);
  void collide(float dt, const float (&world)[16]);
  bool collideWorld(u32 index, const float (&previous)[3], const float (&world)[16]);
  void touched(u32 index, const float (&world)[16], const float (&normal)[3]);
  // Orientação 3D da partícula (quatérnio x, y, z, w) no alinhamento do sistema.
  void orientation(u32 index, const float (&world)[16], float (&q)[4]) const;
  void advanceParticles(float dt, const float (&gravity)[3], const float (&world)[16]);
  void updateTrails();
  float sheetFrame(float t, u32 seed) const;

  scene::ParticleSystemSettings settings_;
  BakedMinMaxCurve startDelay_, startLifetime_, startSpeed_, startSize_, startRotation_, gravity_, rate_;
  BakedMinMaxCurve sizeOverLifetime_, rotationOverLifetime_;
  BakedMinMaxCurve startSizeY_, startSizeZ_, startRotationX_, startRotationY_;
  BakedMinMaxCurve sizeOverLifetimeY_, sizeOverLifetimeZ_, rotationOverLifetimeX_, rotationOverLifetimeY_;
  BakedMinMaxCurve velocity_[3], speedModifier_, limitSpeed_, force_[3], noiseStrength_, sizeBySpeed_, rotationBySpeed_;
  BakedMinMaxGradient colorBySpeed_;
  BakedMinMaxCurve sheetFrame_, sheetStart_;
  // Velocidade do emissor (mundo), para Herdar velocidade e Taxa por distância.
  float lastOrigin_[3]{}, emitterVelocity_[3]{}, distanceAccumulator_ = 0;
  bool hasLastOrigin_ = false;
  BakedMinMaxGradient startColor_, colorOverLifetime_;
  float shapeMatrix_[12]{1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};  // rotação 3x3 (colunas) + posição

  // SoA. Posição e velocidade no espaço da simulação (local do objeto ou mundo).
  std::vector<float> px_, py_, pz_, vx_, vy_, vz_;
  // Velocidade animada do quadro (Velocity over Lifetime + Noise), que não se
  // acumula em v mas move e estica a partícula. Mesmo espaço da simulação.
  std::vector<float> ax_, ay_, az_;
  std::vector<float> age_, lifetime_, baseSize_, rotation_;
  // G6a: tamanho Y/Z e giro X/Y (radianos), por partícula.
  std::vector<float> baseSizeY_, baseSizeZ_, rotationX_, rotationY_;
  std::vector<float> colorR_, colorG_, colorB_, colorA_;
  std::vector<u32> seed_;
  // Trilhas: kTrailPoints pontos por partícula (x, y, z, idade em que foi
  // gravado), do mais velho ao mais novo; trailCount_ diz quantos valem.
  std::vector<float> trail_;
  std::vector<u8> trailCount_;
  ParticleCollisionPlane planes_[2];
  u32 planeCount_ = 0;
  WorldRaycast worldRaycast_ = nullptr;
  void *worldContext_ = nullptr;
  u32 worldQueries_ = 0;
  bool collisionListeners_ = false;
  float sizeScale_ = 1;
  std::vector<float> previous_;  // posições antes do passo (Colisão Mundo), x y z por partícula
  std::vector<ParticleEvent> births_, collisions_, deaths_;

  Pcg32 rng_;
  u32 baseSeed_ = 0, capacityLimit_ = ~0u;
  State state_ = State::Stopped;
  bool emitting_ = false, finished_ = false, configured_ = false, prewarmPending_ = false;
  float systemTime_ = 0, delayRemaining_ = 0, emitAccumulator_ = 0;
  u32 loop_ = 0;
  std::array<u32, scene::ParticleSystemSettings::kMaximumBursts> burstFired_{};  // ciclos já disparados no laço
};

// Teto de partículas por nível de aparelho (§7 do plano). O valor autoral não é
// alterado; o mundo reparte este teto entre os emissores ativos.
enum class ParticleTier : u32 { C = 0, B = 1, A = 2, S = 3 };
constexpr u32 particleTierBudget(ParticleTier tier) {
  switch (tier) {
  case ParticleTier::C: return 1000;
  case ParticleTier::B: return 8000;
  case ParticleTier::A: return 30000;
  case ParticleTier::S: return 100000;
  }
  return 1000;
}

} // namespace ae::renderer::particles

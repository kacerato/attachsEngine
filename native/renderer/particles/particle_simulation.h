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
#include <vector>

namespace ae::renderer::particles {

// Instância por partícula, consumida como atributo por instância pelo
// rhi/shaders/particle.vert. 44 bytes; cor em meia precisão (HDR linear).
struct ParticleInstance {
  float position[3]{};
  float size = 0;
  float velocity[3]{};
  float rotation = 0;  // radianos, no plano da tela
  u16 color[4]{};      // half float
  u32 random = 0;      // semente da partícula: desloca o ruído do material
};
static_assert(sizeof(ParticleInstance) == 44);

// Parâmetros do material de um lote, já resolvidos (tinta × emissão etc.).
struct ParticleMaterialParameters {
  u32 blend = 0, texture = 0, noise = 0, mask = 0;
  u32 renderMode = 0;
  float tint[3]{1, 1, 1};
  float scroll[2]{}, noiseScroll[2]{};
  float noiseScale = 1, noiseStrength = 1, noiseDistortion = 0;
  float centerGlow = 0, centerGlowPower = 4, softDistance = 0;
  float stretchVelocityScale = 0, stretchLengthScale = 2, maxScreenSize = 0.5f;
};

struct ParticleBatch {
  u32 firstInstance = 0, instanceCount = 0;
  ParticleMaterialParameters material;
  float sortDistance = 0;  // distância à câmera + ajuste de ordem; maior desenha antes
};

// O que o renderer recebe por quadro. Lotes já ordenados de trás para a frente.
struct ParticleFrame {
  std::vector<ParticleInstance> instances;
  std::vector<ParticleBatch> batches;
  u32 alive = 0, systems = 0, budget = 0;
  float simulationMs = 0, buildMs = 0;
  void clear() { instances.clear(); batches.clear(); alive = systems = budget = 0; simulationMs = buildMs = 0; }
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
  void advanceParticles(float dt, const float (&gravity)[3]);

  scene::ParticleSystemSettings settings_;
  BakedMinMaxCurve startDelay_, startLifetime_, startSpeed_, startSize_, startRotation_, gravity_, rate_;
  BakedMinMaxCurve sizeOverLifetime_, rotationOverLifetime_;
  BakedMinMaxGradient startColor_, colorOverLifetime_;
  float shapeMatrix_[12]{1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};  // rotação 3x3 (colunas) + posição

  // SoA. Posição e velocidade no espaço da simulação (local do objeto ou mundo).
  std::vector<float> px_, py_, pz_, vx_, vy_, vz_;
  std::vector<float> age_, lifetime_, baseSize_, rotation_;
  std::vector<float> colorR_, colorG_, colorB_, colorA_;
  std::vector<u32> seed_;

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

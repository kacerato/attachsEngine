#pragma once
// Sistema de partículas (pacote P1 do bloco G5).
//
// Vocabulário da Unity 6000.0 (Manual/ParticleSystemModules.html, "Main module",
// "Emission", "Shape", "Color/Size/Rotation over Lifetime" e Renderer), porque é
// o vocabulário dos pacotes que o autor vai importar e dos tutoriais que ele já
// leu. A referência autoriza estudar a capacidade; a hierarquia não é copiada:
// na Astra o renderer e o material de partícula vivem no MESMO componente (como
// o material embutido do GPUParticles3D da Godot 4.5), porque o P1 não tem um
// recurso de material de partícula separado.
//
// Curvas e gradientes reaproveitam `ScriptCurve`/`ScriptGradient` (plano
// CURVAS-GRADIENTES-2026-10-01): o mesmo texto, os mesmos limites e o mesmo
// editor do campo de script. "Min/Max" segue a Unity: constante, curva, entre
// duas constantes, entre duas curvas (e, para cor, também cor aleatória do
// gradiente).
//
// Persistência: chave-valor com ids estáveis (as mesmas ids da API C# e do
// inspetor). Um id desconhecido é recusado — versão nova precisa de leitor novo,
// nunca de um campo ignorado em silêncio.
#include "scene/components.h"
#include "scene/script_curve.h"
#include "scene/script_gradient.h"

#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace ae::scene {

enum class ParticleCurveMode : u32 { Constant = 0, Curve = 1, TwoConstants = 2, TwoCurves = 3 };
enum class ParticleGradientMode : u32 { Color = 0, Gradient = 1, TwoColors = 2, TwoGradients = 3, RandomColor = 4 };
enum class ParticleSimulationSpace : u32 { Local = 0, World = 1 };
enum class ParticleStopAction : u32 { None = 0, Disable = 1, Destroy = 2, Callback = 3 };
enum class ParticleShapeType : u32 { Sphere = 0, Hemisphere = 1, Cone = 2, Box = 3, Circle = 4, Edge = 5, Donut = 6, Rectangle = 7 };
enum class ParticleConeEmitFrom : u32 { Base = 0, Volume = 1 };
enum class ParticleBoxEmitFrom : u32 { Volume = 0, Shell = 1, Edge = 2 };
enum class ParticleRenderMode : u32 { Billboard = 0, StretchedBillboard = 1 };
enum class ParticleSortMode : u32 { None = 0, ByDistance = 1, OldestInFront = 2, YoungestInFront = 3 };
enum class ParticleBlend : u32 { AlphaBlend = 0, Additive = 1 };
// Texturas próprias, geradas pela engine (renderer/particles/particle_textures):
// nenhuma vem de pacote da Asset Store. Textura do projeto fica para o P2, junto
// do Texture Sheet, porque exige o caminho de residência da biblioteca.
enum class ParticleTexture : u32 { SoftDot = 0, Glow = 1, Flame = 2, Smoke = 3, Spark = 4, Ring = 5, Star = 6, Square = 7 };
enum class ParticleNoise : u32 { None = 0, Clouds = 1, Cells = 2, Streaks = 3 };
enum class ParticleMask : u32 { None = 0, Radial = 1, VerticalFade = 2, Ring = 3 };

inline constexpr u32 kParticleTextureCount = 8;
inline constexpr u32 kParticleNoiseCount = 3;  // sem "Nenhum"
inline constexpr u32 kParticleMaskCount = 3;   // sem "Nenhuma"

// Valor por partícula ou pelo tempo do sistema (Unity MinMaxCurve). `constantMax`
// é a constante do modo Constante; a curva do modo Curva é `curveMax`.
struct ParticleMinMaxCurve {
  ParticleCurveMode mode = ParticleCurveMode::Constant;
  float constantMin = 0, constantMax = 0;
  float multiplier = 1;
  ScriptCurve curveMin, curveMax;
  bool valid() const {
    return static_cast<u32>(mode) <= 3 && std::isfinite(constantMin) && std::isfinite(constantMax) &&
           std::isfinite(multiplier) && curveMin.valid() && curveMax.valid();
  }
  bool usesCurves() const noexcept { return mode == ParticleCurveMode::Curve || mode == ParticleCurveMode::TwoCurves; }
  bool usesRandom() const noexcept { return mode == ParticleCurveMode::TwoConstants || mode == ParticleCurveMode::TwoCurves; }
};

// Cor por partícula ou pelo tempo do sistema (Unity MinMaxGradient). Cor em RGB
// linear com alfa; HDR permitido (fogo pode passar de 1).
struct ParticleMinMaxGradient {
  ParticleGradientMode mode = ParticleGradientMode::Color;
  float colorMin[4]{1, 1, 1, 1}, colorMax[4]{1, 1, 1, 1};
  ScriptGradient gradientMin, gradientMax;
  bool valid() const {
    if (static_cast<u32>(mode) > 4 || !gradientMin.valid(true) || !gradientMax.valid(true)) return false;
    for (u32 i = 0; i < 4; ++i) {
      if (!std::isfinite(colorMin[i]) || !std::isfinite(colorMax[i]) || colorMin[i] < 0 || colorMax[i] < 0) return false;
      if (colorMin[i] > (i == 3 ? 1.f : 65504.f) || colorMax[i] > (i == 3 ? 1.f : 65504.f)) return false;
    }
    return true;
  }
};

// Rajada do módulo Emission (Unity ParticleSystem.Burst): no tempo `time` do
// ciclo, emite entre `countMin` e `countMax`, repetindo `cycles` vezes a cada
// `interval` (zero ciclos = infinito enquanto o sistema emitir), com a
// probabilidade dada. Todos os campos são números refletidos (burst_<n>_*):
// inspetor, API C#, presets e animação endereçam a rajada sem UI paralela.
// Mínimo acima do máximo é aceito e sorteado entre os dois, para que editar um
// campo de cada vez nunca seja recusado no meio do caminho.
struct ParticleBurst {
  float time = 0;
  float countMin = 30, countMax = 30;
  float cycles = 1;
  float interval = 0.01f;
  float probability = 1;
  bool operator==(const ParticleBurst &) const = default;
};

inline ScriptCurve particleConstantCurve(float value) {
  ScriptCurve curve;
  curve.keys = {{0, value, 0, 0}, {1, value, 0, 0}};
  return curve;
}
inline ScriptCurve particleLinearCurve(float from, float to) {
  ScriptCurve curve;
  CurveKey a{0, from, to - from, to - from, CurveTangentMode::Linear, CurveTangentMode::Linear, false};
  CurveKey b{1, to, to - from, to - from, CurveTangentMode::Linear, CurveTangentMode::Linear, false};
  curve.keys = {a, b};
  return curve;
}
inline ParticleMinMaxCurve particleCurveDefault(float constant, float curveValue = 1) {
  ParticleMinMaxCurve value;
  value.constantMin = value.constantMax = constant;
  value.multiplier = constant == 0 ? 1 : constant;
  value.curveMin = value.curveMax = particleConstantCurve(curveValue);
  return value;
}
inline ScriptGradient particleFadeGradient() {
  ScriptGradient gradient;
  gradient.alphas = {{0, 1}, {1, 0}};
  return gradient;
}

// Os dados autorais ficam numa base sem identidade: ler um arquivo monta um
// candidato inteiro e só então o publica, sem tocar no instanceId do componente.
struct ParticleSystemSettings {
  static constexpr usize kMaximumBursts = 4;
  static constexpr float kMaximumParticles = 100000;

  // --- Principal ----------------------------------------------------------
  float duration = 5;
  bool looping = true;
  bool prewarm = false;
  ParticleMinMaxCurve startDelay = particleCurveDefault(0);
  ParticleMinMaxCurve startLifetime = particleCurveDefault(5);
  ParticleMinMaxCurve startSpeed = particleCurveDefault(5);
  ParticleMinMaxCurve startSize = particleCurveDefault(1);
  ParticleMinMaxCurve startRotation = particleCurveDefault(0);
  ParticleMinMaxGradient startColor{};
  ParticleMinMaxCurve gravityModifier = particleCurveDefault(0);
  ParticleSimulationSpace simulationSpace = ParticleSimulationSpace::Local;
  float simulationSpeed = 1;
  bool playOnAwake = true;
  float maxParticles = 1000;
  ParticleStopAction stopAction = ParticleStopAction::None;
  bool autoRandomSeed = true;
  float randomSeed = 0;

  // --- Emissão ------------------------------------------------------------
  bool emissionEnabled = true;
  ParticleMinMaxCurve emissionRate = particleCurveDefault(10);
  float burstCount = 0;
  std::array<ParticleBurst, kMaximumBursts> bursts{};
  usize activeBursts() const noexcept { return std::min(kMaximumBursts, static_cast<usize>(burstCount)); }

  // --- Forma --------------------------------------------------------------
  bool shapeEnabled = true;
  ParticleShapeType shapeType = ParticleShapeType::Cone;
  float shapeRadius = 1;
  float shapeRadiusThickness = 1;
  float shapeAngle = 25;
  float shapeArc = 360;
  float shapeLength = 5;
  float shapeDonutRadius = 0.2f;  // raio do tubo da Rosca (Unity Donut Radius)
  ParticleConeEmitFrom shapeConeEmitFrom = ParticleConeEmitFrom::Base;
  ParticleBoxEmitFrom shapeBoxEmitFrom = ParticleBoxEmitFrom::Volume;
  float shapeBox[3]{1, 1, 1};
  float shapePosition[3]{0, 0, 0};
  float shapeRotation[3]{0, 0, 0};
  float shapeRandomDirection = 0;
  float shapeSpherizeDirection = 0;

  // --- Ao longo da vida -----------------------------------------------------
  bool colorOverLifetimeEnabled = false;
  ParticleMinMaxGradient colorOverLifetime = [] {
    ParticleMinMaxGradient value;
    value.mode = ParticleGradientMode::Gradient;
    value.gradientMin = value.gradientMax = particleFadeGradient();
    return value;
  }();
  bool sizeOverLifetimeEnabled = false;
  ParticleMinMaxCurve sizeOverLifetime = [] {
    auto value = particleCurveDefault(1);
    value.mode = ParticleCurveMode::Curve;
    value.curveMin = value.curveMax = particleLinearCurve(0, 1);
    return value;
  }();
  bool rotationOverLifetimeEnabled = false;
  ParticleMinMaxCurve rotationOverLifetime = particleCurveDefault(45);

  // --- Movimento ao longo da vida (P2a) ------------------------------------
  // Velocity over Lifetime: velocidade somada à da partícula (não acumula) e
  // Speed Modifier, que escala o deslocamento. Espaço próprio, como na Unity.
  bool velocityOverLifetimeEnabled = false;
  ParticleMinMaxCurve velocityX = particleCurveDefault(0), velocityY = particleCurveDefault(0), velocityZ = particleCurveDefault(0);
  ParticleSimulationSpace velocitySpace = ParticleSimulationSpace::Local;
  ParticleMinMaxCurve speedModifier = particleCurveDefault(1);
  // Limit Velocity over Lifetime: teto de velocidade com Dampen e Drag.
  bool limitVelocityEnabled = false;
  ParticleMinMaxCurve limitSpeed = particleCurveDefault(1);
  float limitDampen = 1;
  float limitDrag = 0;
  // Force over Lifetime: aceleração (m/s²) integrada na velocidade.
  bool forceOverLifetimeEnabled = false;
  ParticleMinMaxCurve forceX = particleCurveDefault(0), forceY = particleCurveDefault(0), forceZ = particleCurveDefault(0);
  ParticleSimulationSpace forceSpace = ParticleSimulationSpace::World;
  // Noise: turbulência por ruído de valor 3D no espaço da simulação.
  bool noiseEnabled = false;
  ParticleMinMaxCurve noiseStrength = particleCurveDefault(1);
  float noiseFrequency = 0.5f;
  float noiseScrollSpeed = 0;
  float noiseOctaves = 1;

  // --- Herança e "pela velocidade" (P2c) ------------------------------------
  // Inherit Velocity (modo Initial): a partícula nasce com parte da velocidade
  // do emissor. Rate over Distance fica no módulo Emissão (partículas por metro).
  bool inheritVelocityEnabled = false;
  float inheritVelocityMultiplier = 1;
  float emissionRateOverDistance = 0;
  // Size/Color/Rotation by Speed: a curva ou o gradiente é lido pela
  // velocidade da partícula, normalizada entre Mínimo e Máximo da faixa.
  bool sizeBySpeedEnabled = false;
  ParticleMinMaxCurve sizeBySpeed = [] {
    auto value = particleCurveDefault(1);
    value.mode = ParticleCurveMode::Curve;
    value.curveMin = value.curveMax = particleLinearCurve(0, 1);
    return value;
  }();
  float sizeBySpeedRange[2]{0, 1};
  bool colorBySpeedEnabled = false;
  ParticleMinMaxGradient colorBySpeed = [] {
    ParticleMinMaxGradient value;
    value.mode = ParticleGradientMode::Gradient;
    value.gradientMin = value.gradientMax = particleFadeGradient();
    return value;
  }();
  float colorBySpeedRange[2]{0, 1};
  bool rotationBySpeedEnabled = false;
  ParticleMinMaxCurve rotationBySpeed = particleCurveDefault(45);
  float rotationBySpeedRange[2]{0, 1};

  // --- Colisão (P2b) --------------------------------------------------------
  // Collision no modo Planes da Unity: até dois objetos-plano (o +Y do objeto é
  // a normal, a posição dele passa pelo plano). Sem consulta à física: é o que
  // cabe em milhares de partículas por quadro no celular.
  bool collisionEnabled = false;
  u64 collisionPlane0 = 0, collisionPlane1 = 0;
  float collisionDampen = 0;        // fração da velocidade tangencial perdida no toque
  float collisionBounce = 1;        // fração da velocidade normal devolvida
  float collisionLifetimeLoss = 0;  // fração da vida consumida a cada toque
  float collisionMinKillSpeed = 0;  // abaixo disso depois do toque, morre
  float collisionRadiusScale = 1;   // raio de colisão = tamanho × escala / 2

  // --- Sub-emissores (P2b) --------------------------------------------------
  // Sub Emitters da Unity: outro objeto com Sistema de partículas emite onde a
  // partícula nasce, colide ou morre. A forma e o visual são os do sub-sistema.
  bool subEmittersEnabled = false;
  u64 subEmitterBirth = 0, subEmitterCollision = 0, subEmitterDeath = 0;
  float subEmitterBirthCount = 1, subEmitterCollisionCount = 3, subEmitterDeathCount = 5;

  // --- Renderização -------------------------------------------------------
  ParticleRenderMode renderMode = ParticleRenderMode::Billboard;
  float stretchVelocityScale = 0;
  float stretchLengthScale = 2;
  ParticleSortMode sortMode = ParticleSortMode::None;
  float sortingFudge = 0;
  float maxParticleSize = 0.5f;

  // --- Material de partícula ----------------------------------------------
  ParticleBlend materialBlend = ParticleBlend::AlphaBlend;
  ParticleTexture materialTexture = ParticleTexture::SoftDot;
  float materialTint[3]{1, 1, 1};
  float materialEmission = 1;
  float materialScroll[2]{0, 0};
  ParticleNoise materialNoise = ParticleNoise::None;
  float materialNoiseScale = 1;
  float materialNoiseScroll[2]{0, 0};
  float materialNoiseStrength = 1;
  float materialNoiseDistortion = 0;
  ParticleMask materialMask = ParticleMask::None;
  float materialCenterGlow = 0;
  float materialCenterGlowPower = 4;
  float materialSoftDistance = 0;
};

class ParticleSystem final : public ComponentValue, public ParticleSystemSettings {
public:
  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override { return std::make_unique<ParticleSystem>(*this); }
  bool valid() const override;
  void write(std::ostream &out) const override;
  bool read(std::istream &in, u32 version) override;

  // Curvas e gradientes endereçáveis por id persistente: "<propriedade>.min" e
  // "<propriedade>.max". Usado pelo arquivo, pelo editor de curvas e pela API.
  struct CurveSlot { std::string_view id; ParticleMinMaxCurve ParticleSystemSettings::*member; bool maximum; };
  struct GradientSlot { std::string_view id; ParticleMinMaxGradient ParticleSystemSettings::*member; bool maximum; };
  static std::span<const CurveSlot> curveSlots();
  static std::span<const GradientSlot> gradientSlots();
  ScriptCurve *curve(std::string_view id);
  const ScriptCurve *curve(std::string_view id) const { return const_cast<ParticleSystem *>(this)->curve(id); }
  ScriptGradient *gradient(std::string_view id);
  const ScriptGradient *gradient(std::string_view id) const { return const_cast<ParticleSystem *>(this)->gradient(id); }
};

inline const ParticleSystem &particleSystem(const ComponentValue &value) { return static_cast<const ParticleSystem &>(value); }
inline ParticleSystem &particleSystem(ComponentValue &value) { return static_cast<ParticleSystem &>(value); }

// Predicados de apresentação compartilhados por inspetor e validação de escrita.
template <ParticleMinMaxCurve ParticleSystemSettings::*M> bool particleCurveShowsMin(const ComponentValue &v) {
  return (particleSystem(v).*M).mode == ParticleCurveMode::TwoConstants;
}
template <ParticleMinMaxCurve ParticleSystemSettings::*M> bool particleCurveShowsConstant(const ComponentValue &v) {
  const auto mode = (particleSystem(v).*M).mode;
  return mode == ParticleCurveMode::Constant || mode == ParticleCurveMode::TwoConstants;
}
template <ParticleMinMaxCurve ParticleSystemSettings::*M> bool particleCurveShowsCurve(const ComponentValue &v) {
  return (particleSystem(v).*M).usesCurves();
}
template <ParticleMinMaxGradient ParticleSystemSettings::*M> bool particleGradientShowsColor(const ComponentValue &v) {
  const auto mode = (particleSystem(v).*M).mode;
  return mode == ParticleGradientMode::Color || mode == ParticleGradientMode::TwoColors;
}
template <ParticleMinMaxGradient ParticleSystemSettings::*M> bool particleGradientShowsMinColor(const ComponentValue &v) {
  return (particleSystem(v).*M).mode == ParticleGradientMode::TwoColors;
}
inline bool particleLooping(const ComponentValue &v) { return particleSystem(v).looping; }
inline bool particleManualSeed(const ComponentValue &v) { return !particleSystem(v).autoRandomSeed; }
inline bool particleShapeRadius(const ComponentValue &v) {
  const auto t = particleSystem(v).shapeType;
  return t != ParticleShapeType::Box && t != ParticleShapeType::Rectangle;
}
inline bool particleShapeThickness(const ComponentValue &v) {
  const auto t = particleSystem(v).shapeType;
  return t != ParticleShapeType::Box && t != ParticleShapeType::Edge && t != ParticleShapeType::Rectangle;
}
inline bool particleShapeDonut(const ComponentValue &v) { return particleSystem(v).shapeType == ParticleShapeType::Donut; }
inline bool particleShapeCone(const ComponentValue &v) { return particleSystem(v).shapeType == ParticleShapeType::Cone; }
inline bool particleShapeConeVolume(const ComponentValue &v) {
  const auto &p = particleSystem(v);
  return p.shapeType == ParticleShapeType::Cone && p.shapeConeEmitFrom == ParticleConeEmitFrom::Volume;
}
inline bool particleShapeArc(const ComponentValue &v) {
  const auto t = particleSystem(v).shapeType;
  return t == ParticleShapeType::Cone || t == ParticleShapeType::Circle || t == ParticleShapeType::Donut;
}
inline bool particleShapeBox(const ComponentValue &v) {
  const auto t = particleSystem(v).shapeType;
  return t == ParticleShapeType::Box || t == ParticleShapeType::Rectangle;
}
inline bool particleStretched(const ComponentValue &v) { return particleSystem(v).renderMode == ParticleRenderMode::StretchedBillboard; }
inline bool particleNoiseOn(const ComponentValue &v) { return particleSystem(v).materialNoise != ParticleNoise::None; }
template <usize N> bool particleBurstVisible(const ComponentValue &v) { return particleSystem(v).activeBursts() > N; }

inline constexpr std::array<ComponentEnumOption, 4> particleCurveModeOptions{{
  {0, "Constante"}, {1, "Curva"}, {2, "Entre duas constantes"}, {3, "Entre duas curvas"}}};
inline constexpr std::array<ComponentEnumOption, 2> particleConstantModeOptions{{
  {0, "Constante"}, {2, "Entre duas constantes"}}};
inline constexpr std::array<ComponentEnumOption, 5> particleGradientModeOptions{{
  {0, "Cor"}, {1, "Gradiente"}, {2, "Entre duas cores"}, {3, "Entre dois gradientes"}, {4, "Cor aleatória"}}};
inline constexpr std::array<ComponentEnumOption, 2> particleLifetimeGradientModeOptions{{
  {1, "Gradiente"}, {3, "Entre dois gradientes"}}};
inline constexpr std::array<ComponentEnumOption, 2> particleSpaceOptions{{{0, "Local"}, {1, "Mundo"}}};
inline constexpr std::array<ComponentEnumOption, 4> particleStopActionOptions{{
  {0, "Nenhuma"}, {1, "Desativar objeto"}, {2, "Destruir objeto"}, {3, "Evento Parou"}}};
inline constexpr std::array<ComponentEnumOption, 8> particleShapeOptions{{
  {0, "Esfera"}, {1, "Hemisfério"}, {2, "Cone"}, {3, "Caixa"}, {4, "Círculo"}, {5, "Aresta"}, {6, "Rosca"}, {7, "Retângulo"}}};
inline constexpr std::array<ComponentEnumOption, 2> particleConeEmitOptions{{{0, "Base"}, {1, "Volume"}}};
inline constexpr std::array<ComponentEnumOption, 3> particleBoxEmitOptions{{{0, "Volume"}, {1, "Casca"}, {2, "Arestas"}}};
inline constexpr std::array<ComponentEnumOption, 2> particleRenderModeOptions{{{0, "Billboard"}, {1, "Billboard esticado"}}};
inline constexpr std::array<ComponentEnumOption, 4> particleSortOptions{{
  {0, "Nenhuma"}, {1, "Por distância"}, {2, "Mais antigas na frente"}, {3, "Mais novas na frente"}}};
inline constexpr std::array<ComponentEnumOption, 2> particleBlendOptions{{{0, "Mistura"}, {1, "Aditivo"}}};
inline constexpr std::array<ComponentEnumOption, 8> particleTextureOptions{{
  {0, "Ponto suave"}, {1, "Brilho"}, {2, "Chama"}, {3, "Fumaça"}, {4, "Faísca"}, {5, "Anel"}, {6, "Estrela"}, {7, "Quadrado"}}};
inline constexpr std::array<ComponentEnumOption, 4> particleNoiseOptions{{
  {0, "Nenhum"}, {1, "Nuvens"}, {2, "Células"}, {3, "Faixas"}}};
inline constexpr std::array<ComponentEnumOption, 4> particleMaskOptions{{
  {0, "Nenhuma"}, {1, "Radial"}, {2, "Desvanece para cima"}, {3, "Anel"}}};

inline bool particleCollides(const ComponentValue &v) { return particleSystem(v).collisionEnabled; }
inline bool particleHasSubEmitters(const ComponentValue &v) { return particleSystem(v).subEmittersEnabled; }
#include "scene/generated/particle_system_particleSystemNumbers.inc"
#include "scene/generated/particle_system_particleSystemBooleans.inc"
#include "scene/generated/particle_system_particleSystemEnums.inc"
// Planos e sub-sistemas são objetos da cena, com referência persistente
// remapeada por clone e prefab como qualquer outra.
inline constexpr std::array<ComponentObjectReference, 5> particleSystemReferences{{
  {"collision_plane_0", "Plano 1", "", ObjectReferenceScope::Other, "Nenhum",
   [](const ComponentValue &v) { return particleSystem(v).collisionPlane0; },
   [](ComponentValue &v, u64 id) { particleSystem(v).collisionPlane0 = id; },
   {"Colisão", "", "O +Y do objeto é a normal; a partícula quica do lado de cima", particleCollides, nullptr,
    "render.particles.collision", "renderer/particles/particle_simulation.cpp"}},
  {"collision_plane_1", "Plano 2", "", ObjectReferenceScope::Other, "Nenhum",
   [](const ComponentValue &v) { return particleSystem(v).collisionPlane1; },
   [](ComponentValue &v, u64 id) { particleSystem(v).collisionPlane1 = id; },
   {"Colisão", "", "Segundo plano (parede, teto)", particleCollides, nullptr,
    "render.particles.collision", "renderer/particles/particle_simulation.cpp"}},
  {"sub_emitter_birth", "Ao nascer", "astra.effects.particle_system", ObjectReferenceScope::Other, "Nenhum",
   [](const ComponentValue &v) { return particleSystem(v).subEmitterBirth; },
   [](ComponentValue &v, u64 id) { particleSystem(v).subEmitterBirth = id; },
   {"Sub-emissores", "", "Sistema que emite onde cada partícula nasce", particleHasSubEmitters, nullptr,
    "render.particles.sub_emitters", "runtime/scene_particles.cpp"}},
  {"sub_emitter_collision", "Ao colidir", "astra.effects.particle_system", ObjectReferenceScope::Other, "Nenhum",
   [](const ComponentValue &v) { return particleSystem(v).subEmitterCollision; },
   [](ComponentValue &v, u64 id) { particleSystem(v).subEmitterCollision = id; },
   {"Sub-emissores", "", "Sistema que emite no ponto do toque (respingo, faísca)", particleHasSubEmitters, nullptr,
    "render.particles.sub_emitters", "runtime/scene_particles.cpp"}},
  {"sub_emitter_death", "Ao morrer", "astra.effects.particle_system", ObjectReferenceScope::Other, "Nenhum",
   [](const ComponentValue &v) { return particleSystem(v).subEmitterDeath; },
   [](ComponentValue &v, u64 id) { particleSystem(v).subEmitterDeath = id; },
   {"Sub-emissores", "", "Sistema que emite onde cada partícula morre (fogos)", particleHasSubEmitters, nullptr,
    "render.particles.sub_emitters", "runtime/scene_particles.cpp"}},
}};

inline constexpr std::array<ComponentTriple, 7> particleSystemTriples{{
  {"start_color", "Cor inicial", {"start_color_r", "start_color_g", "start_color_b"}, ComponentTripleKind::LinearColor},
  {"start_color_min", "Cor mínima", {"start_color_min_r", "start_color_min_g", "start_color_min_b"}, ComponentTripleKind::LinearColor},
  {"shape_box", "Tamanho da caixa", {"shape_box_x", "shape_box_y", "shape_box_z"}},
  {"shape_position", "Posição", {"shape_position_x", "shape_position_y", "shape_position_z"}},
  {"shape_rotation", "Rotação", {"shape_rotation_x", "shape_rotation_y", "shape_rotation_z"}},
  {"material_tint", "Tinta", {"material_tint_r", "material_tint_g", "material_tint_b"}, ComponentTripleKind::LinearColor},
  {"material_scroll", "Rolagem de UV", {"material_scroll_x", "material_scroll_y", ""}, ComponentTripleKind::Vector2},
}};

// Operações do mundo de Play (runtime/scene_particles.h). Mesmas palavras da
// Unity: Play, Stop (para de emitir e deixa as vivas terminarem), Pause, Clear,
// Emit; "parar e limpar" é o Stop com StopEmittingAndClear.
inline constexpr std::array<ComponentParameter, 1> particleEmitParameters{{
  {"count", "Quantidade", ComponentValueKind::Integer}}};
inline constexpr std::array<ComponentMethod, 9> particleSystemMethods{{
  {"play", "Tocar", "Inicia ou retoma a simulação e a emissão"},
  {"stop", "Parar", "Para de emitir; as partículas vivas terminam a vida"},
  {"stop_and_clear", "Parar e limpar", "Para de emitir e remove todas as partículas"},
  {"pause", "Pausar", "Congela simulação e emissão"},
  {"clear", "Limpar", "Remove as partículas vivas sem mudar o estado de reprodução"},
  {"emit", "Emitir", "Emite partículas agora, pela forma, mesmo parado", particleEmitParameters},
  {"is_playing", "Tocando", "Verdadeiro enquanto toca (emitindo ou com partículas vivas)", {}, ComponentValueKind::Boolean},
  {"particle_count", "Partículas vivas", "Quantas partículas estão vivas neste quadro", {}, ComponentValueKind::Integer},
  {"time", "Tempo", "Tempo do ciclo atual em segundos", {}, ComponentValueKind::Number},
}};
inline constexpr std::array<ComponentEvent, 1> particleSystemEvents{{
  {"stopped", "Parou", "O sistema terminou: não emite mais e não há partículas vivas (Ação ao parar = Evento Parou)"}}};

inline const ComponentType ParticleSystem::descriptor{
  "astra.effects.particle_system", 2, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<ParticleSystem>(); },
  particleSystemNumbers, particleSystemBooleans, particleSystemEnums, nullptr, false, particleSystemReferences, particleSystemTriples,
  {}, {}, {}, {}, particleSystemMethods, particleSystemEvents};

inline std::span<const ParticleSystem::CurveSlot> ParticleSystem::curveSlots() {
  static constexpr std::array<CurveSlot, 40> slots{{
    {"start_delay.min", &ParticleSystem::startDelay, false}, {"start_delay.max", &ParticleSystem::startDelay, true},
    {"start_lifetime.min", &ParticleSystem::startLifetime, false}, {"start_lifetime.max", &ParticleSystem::startLifetime, true},
    {"start_speed.min", &ParticleSystem::startSpeed, false}, {"start_speed.max", &ParticleSystem::startSpeed, true},
    {"start_size.min", &ParticleSystem::startSize, false}, {"start_size.max", &ParticleSystem::startSize, true},
    {"start_rotation.min", &ParticleSystem::startRotation, false}, {"start_rotation.max", &ParticleSystem::startRotation, true},
    {"gravity_modifier.min", &ParticleSystem::gravityModifier, false}, {"gravity_modifier.max", &ParticleSystem::gravityModifier, true},
    {"emission_rate.min", &ParticleSystem::emissionRate, false}, {"emission_rate.max", &ParticleSystem::emissionRate, true},
    {"size_over_lifetime.min", &ParticleSystem::sizeOverLifetime, false}, {"size_over_lifetime.max", &ParticleSystem::sizeOverLifetime, true},
    {"rotation_over_lifetime.min", &ParticleSystem::rotationOverLifetime, false}, {"rotation_over_lifetime.max", &ParticleSystem::rotationOverLifetime, true},
    {"velocity_x.min", &ParticleSystem::velocityX, false}, {"velocity_x.max", &ParticleSystem::velocityX, true},
    {"velocity_y.min", &ParticleSystem::velocityY, false}, {"velocity_y.max", &ParticleSystem::velocityY, true},
    {"velocity_z.min", &ParticleSystem::velocityZ, false}, {"velocity_z.max", &ParticleSystem::velocityZ, true},
    {"speed_modifier.min", &ParticleSystem::speedModifier, false}, {"speed_modifier.max", &ParticleSystem::speedModifier, true},
    {"limit_speed.min", &ParticleSystem::limitSpeed, false}, {"limit_speed.max", &ParticleSystem::limitSpeed, true},
    {"force_x.min", &ParticleSystem::forceX, false}, {"force_x.max", &ParticleSystem::forceX, true},
    {"force_y.min", &ParticleSystem::forceY, false}, {"force_y.max", &ParticleSystem::forceY, true},
    {"force_z.min", &ParticleSystem::forceZ, false}, {"force_z.max", &ParticleSystem::forceZ, true},
    {"noise_strength.min", &ParticleSystem::noiseStrength, false}, {"noise_strength.max", &ParticleSystem::noiseStrength, true},
    {"size_by_speed.min", &ParticleSystem::sizeBySpeed, false}, {"size_by_speed.max", &ParticleSystem::sizeBySpeed, true},
    {"rotation_by_speed.min", &ParticleSystem::rotationBySpeed, false}, {"rotation_by_speed.max", &ParticleSystem::rotationBySpeed, true},
  }};
  return slots;
}
inline std::span<const ParticleSystem::GradientSlot> ParticleSystem::gradientSlots() {
  static constexpr std::array<GradientSlot, 6> slots{{
    {"start_color.min", &ParticleSystem::startColor, false}, {"start_color.max", &ParticleSystem::startColor, true},
    {"color_over_lifetime.min", &ParticleSystem::colorOverLifetime, false}, {"color_over_lifetime.max", &ParticleSystem::colorOverLifetime, true},
    {"color_by_speed.min", &ParticleSystem::colorBySpeed, false}, {"color_by_speed.max", &ParticleSystem::colorBySpeed, true},
  }};
  return slots;
}
inline ScriptCurve *ParticleSystem::curve(std::string_view id) {
  for (const auto &slot : curveSlots())
    if (slot.id == id) return slot.maximum ? &(this->*slot.member).curveMax : &(this->*slot.member).curveMin;
  return nullptr;
}
inline ScriptGradient *ParticleSystem::gradient(std::string_view id) {
  for (const auto &slot : gradientSlots())
    if (slot.id == id) return slot.maximum ? &(this->*slot.member).gradientMax : &(this->*slot.member).gradientMin;
  return nullptr;
}

inline bool ParticleSystem::valid() const {
  for (const auto &p : descriptor.numbers) {
    const float value = p.read(*this);
    if (!std::isfinite(value) || value < p.minimum || value > p.maximum) return false;
  }
  for (const auto &p : descriptor.enums) {
    const u32 value = p.read(*this);
    bool known = false;
    for (const auto &option : p.options) known = known || option.value == value;
    if (!known) return false;
  }
  for (const auto &slot : curveSlots()) if (!(this->*slot.member).valid()) return false;
  for (const auto &slot : gradientSlots()) if (!(this->*slot.member).valid()) return false;
  for (const auto &burst : bursts)
    if (burst.countMin != std::floor(burst.countMin) || burst.countMax != std::floor(burst.countMax) || burst.cycles != std::floor(burst.cycles))
      return false;
  for (const auto &reference : particleSystemReferences)
    if (reference.read(*this) > std::numeric_limits<u32>::max()) return false;
  return maxParticles == std::floor(maxParticles) && randomSeed == std::floor(randomSeed) && burstCount == std::floor(burstCount) &&
         noiseOctaves == std::floor(noiseOctaves) && subEmitterBirthCount == std::floor(subEmitterBirthCount) &&
         subEmitterCollisionCount == std::floor(subEmitterCollisionCount) && subEmitterDeathCount == std::floor(subEmitterDeathCount);
}

// v1: "<n> id valor … <c> id "curva" … <g> id "gradiente" …".
// Números, booleanos e enumerações saem dos descritores; o leitor aceita
// qualquer ordem e recusa id desconhecido ou repetido.
inline void ParticleSystem::write(std::ostream &out) const {
  out << (descriptor.numbers.size() + descriptor.booleans.size() + descriptor.enums.size() + descriptor.references.size());
  for (const auto &p : descriptor.numbers) out << ' ' << p.id << ' ' << p.read(*this);
  for (const auto &p : descriptor.booleans) out << ' ' << p.id << ' ' << (p.read(*this) ? 1 : 0);
  for (const auto &p : descriptor.enums) out << ' ' << p.id << ' ' << p.read(*this);
  for (const auto &p : descriptor.references) out << ' ' << p.id << ' ' << p.read(*this);
  out << ' ' << curveSlots().size();
  for (const auto &slot : curveSlots())
    out << ' ' << slot.id << ' ' << std::quoted(scriptCurveValue(*curve(slot.id)));
  out << ' ' << gradientSlots().size();
  for (const auto &slot : gradientSlots())
    out << ' ' << slot.id << ' ' << std::quoted(scriptGradientValue(*gradient(slot.id)));
}

inline bool ParticleSystem::read(std::istream &in, u32 version) {
  // v2 (P2a) só acrescenta ids; um arquivo v1 lê os mesmos campos e deixa os
  // módulos novos desligados no padrão. Versão futura é recusada.
  if (version < 1 || version > 2) return false;
  ParticleSystem candidate;
  std::vector<std::string> seen;
  const auto fresh = [&](const std::string &id) {
    for (const auto &s : seen) if (s == id) return false;
    seen.push_back(id);
    return true;
  };
  u32 count = 0;
  if (!(in >> count) || count > 512) return false;
  for (u32 i = 0; i < count; ++i) {
    std::string id;
    double value = 0;
    if (!(in >> id >> value) || !fresh(id) || !std::isfinite(value)) return false;
    bool applied = false;
    for (const auto &p : descriptor.numbers)
      if (p.id == id) { *p.write(candidate) = static_cast<float>(value); applied = true; break; }
    if (!applied) for (const auto &p : descriptor.booleans)
      if (p.id == id) { if (value != 0 && value != 1) return false; p.write(candidate, value != 0); applied = true; break; }
    if (!applied) for (const auto &p : descriptor.enums)
      if (p.id == id) {
        if (value < 0 || value > 1024 || value != std::floor(value)) return false;
        p.write(candidate, static_cast<u32>(value)); applied = true; break;
      }
    if (!applied) for (const auto &p : descriptor.references)
      if (p.id == id) {
        if (value < 0 || value > 4294967295.0 || value != std::floor(value)) return false;
        p.write(candidate, static_cast<u64>(value)); applied = true; break;
      }
    if (!applied) return false;
  }
  if (!(in >> count) || count > 64) return false;
  for (u32 i = 0; i < count; ++i) {
    std::string id, text;
    if (!(in >> id >> std::quoted(text)) || !fresh(id)) return false;
    auto *target = candidate.curve(id);
    if (!target || !parseScriptCurve(text, *target)) return false;
  }
  if (!(in >> count) || count > 16) return false;
  for (u32 i = 0; i < count; ++i) {
    std::string id, text;
    if (!(in >> id >> std::quoted(text)) || !fresh(id)) return false;
    auto *target = candidate.gradient(id);
    if (!target || !parseScriptGradient(text, *target)) return false;
  }
  if (!candidate.valid()) return false;
  static_cast<ParticleSystemSettings &>(*this) = candidate;
  return true;
}

} // namespace ae::scene

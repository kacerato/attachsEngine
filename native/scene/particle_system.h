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
enum class ParticleShapeType : u32 { Sphere = 0, Hemisphere = 1, Cone = 2, Box = 3, Circle = 4, Edge = 5, Donut = 6, Rectangle = 7, Mesh = 8 };
// G6a: forma Malha — nasce num vértice ou num ponto da superfície (por área),
// para a normal do triângulo (Unity Shape Mesh, Vertex/Triangle).
enum class ParticleShapeMeshPlacement : u32 { Vertex = 0, Surface = 1 };
enum class ParticleConeEmitFrom : u32 { Base = 0, Volume = 1 };
enum class ParticleBoxEmitFrom : u32 { Volume = 0, Shell = 1, Edge = 2 };
// G6a: Malha desenha uma malha por partícula (Unity Render Mode Mesh); Nenhum
// simula, acende luzes e alimenta sub-emissores sem desenhar (Unity None).
enum class ParticleRenderMode : u32 { Billboard = 0, StretchedBillboard = 1, Mesh = 2, None = 3 };
// Render Alignment da Unity. Facing vira Vista (a diferença só aparece com a
// câmera muito perto); Velocidade alinha a frente ao movimento.
enum class ParticleRenderAlignment : u32 { View = 0, World = 1, Local = 2, Velocity = 3 };
// Malhas embutidas (as primitivas da Unity que os pacotes usam por GUID fixo).
enum class ParticleMeshShape : u32 { Quad = 0, Cube = 1, Sphere = 2, Cylinder = 3 };
// Scaling Mode da Unity: o que a escala do Transform faz com o sistema.
enum class ParticleScalingMode : u32 { Hierarchy = 0, Local = 1, Shape = 2 };
// Collision Type da Unity: planos escolhidos ou o mundo (colisores da física).
enum class ParticleCollisionType : u32 { Planes = 0, World = 1 };
enum class ParticleSortMode : u32 { None = 0, ByDistance = 1, OldestInFront = 2, YoungestInFront = 3 };
// G6a: Distorção desloca a imagem da cena atrás da partícula pelo mapa normal
// do material (calor, onda de choque, vidro mágico).
enum class ParticleBlend : u32 { AlphaBlend = 0, Additive = 1, Distortion = 2 };
// Texturas próprias, geradas pela engine (renderer/particles/particle_textures):
// nenhuma vem de pacote da Asset Store. Textura do projeto fica para o P2, junto
// do Texture Sheet, porque exige o caminho de residência da biblioteca.
enum class ParticleTexture : u32 { SoftDot = 0, Glow = 1, Flame = 2, Smoke = 3, Spark = 4, Ring = 5, Star = 6, Square = 7 };
enum class ParticleNoise : u32 { None = 0, Clouds = 1, Cells = 2, Streaks = 3 };
enum class ParticleMask : u32 { None = 0, Radial = 1, VerticalFade = 2, Ring = 3 };
// Texture Sheet Animation (Unity Manual/PartSysTexSheetAnimModule.html, modo Grid):
// a imagem do Material vira uma grade de Colunas x Linhas; a partícula percorre a
// folha inteira ou uma linha só.
enum class ParticleSheetAnimation : u32 { WholeSheet = 0, SingleRow = 1 };
enum class ParticleSheetRowMode : u32 { Custom = 0, Random = 1 };

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
  // G6a: tamanho e rotação por eixo (Unity 3D Start Size / 3D Start Rotation).
  // Com Tamanho 3D, `startSize` é o X; com Rotação 3D, `startRotation` é o Z.
  bool size3D = false;
  ParticleMinMaxCurve startSizeY = particleCurveDefault(1), startSizeZ = particleCurveDefault(1);
  bool rotation3D = false;
  ParticleMinMaxCurve startRotationX = particleCurveDefault(0), startRotationY = particleCurveDefault(0);
  // Forma é o comportamento anterior ao G6a (partícula sem a escala do objeto):
  // cena antiga abre igual. A importação da Unity grava o modo do pacote.
  ParticleScalingMode scalingMode = ParticleScalingMode::Shape;

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
  ParticleShapeMeshPlacement shapeMeshPlacement = ParticleShapeMeshPlacement::Surface;
  float shapeMeshScale = 1;
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
  bool sizeSeparateAxes = false;
  ParticleMinMaxCurve sizeOverLifetimeY = sizeOverLifetime, sizeOverLifetimeZ = sizeOverLifetime;
  bool rotationOverLifetimeEnabled = false;
  ParticleMinMaxCurve rotationOverLifetime = particleCurveDefault(45);
  bool rotationSeparateAxes = false;
  ParticleMinMaxCurve rotationOverLifetimeX = particleCurveDefault(0), rotationOverLifetimeY = particleCurveDefault(0);

  // --- Movimento ao longo da vida (P2a) ------------------------------------
  // Velocity over Lifetime: velocidade somada à da partícula (não acumula) e
  // Speed Modifier, que escala o deslocamento. Espaço próprio, como na Unity.
  bool velocityOverLifetimeEnabled = false;
  ParticleMinMaxCurve velocityX = particleCurveDefault(0), velocityY = particleCurveDefault(0), velocityZ = particleCurveDefault(0);
  ParticleSimulationSpace velocitySpace = ParticleSimulationSpace::Local;
  ParticleMinMaxCurve speedModifier = particleCurveDefault(1);
  // G6a: Orbital e Radial (Unity Velocity over Lifetime): giro em volta do
  // centro (graus/s por eixo) e afastamento dele (m/s); o centro é a origem do
  // objeto mais o deslocamento.
  ParticleMinMaxCurve velocityOrbitalX = particleCurveDefault(0), velocityOrbitalY = particleCurveDefault(0),
                      velocityOrbitalZ = particleCurveDefault(0);
  float velocityOrbitalOffset[3]{0, 0, 0};
  ParticleMinMaxCurve velocityRadial = particleCurveDefault(0);
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
  // G6a: Mundo consulta a física do Play (runtime/scene_particles.cpp).
  ParticleCollisionType collisionType = ParticleCollisionType::Planes;
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
  // G6a: até três sub-sistemas a mais, cada um com o seu acontecimento. Como na
  // Unity, cada um emite as próprias rajadas no ponto (ao menos uma partícula).
  static constexpr usize kExtraSubEmitters = 7;
  std::array<u64, kExtraSubEmitters> subEmitterExtra{};
  std::array<u32, kExtraSubEmitters> subEmitterExtraType{};  // 0 nascer, 1 colidir, 2 morrer

  // --- Renderização -------------------------------------------------------
  ParticleRenderMode renderMode = ParticleRenderMode::Billboard;
  ParticleRenderAlignment renderAlignment = ParticleRenderAlignment::View;
  // Malha (G6a): a do projeto, quando escolhida, vale mais que a embutida.
  ParticleMeshShape meshShape = ParticleMeshShape::Quad;
  resources::AssetGuid meshAsset{};
  // G6a (v5): Malha do projeto da forma Malha.
  resources::AssetGuid shapeMeshAsset{};
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
  // G6a: deslocamento da Distorção em pixels a 1 m da câmera (cai com a distância).
  float materialDistortion = 0;

  // --- Material de projeto e Folha de textura (PK-10a) -----------------------
  // Como o Particle System Renderer da Unity: um Material do projeto com shader
  // Partícula (Particles Unlit/Lit do PK-9) dá a imagem; a cor base dele tinge.
  // Sem Material, ou com outro shader, valem a textura própria e a tinta acima.
  resources::AssetGuid materialAsset{};
  bool textureSheetEnabled = false;
  float sheetTiles[2]{1, 1};
  ParticleSheetAnimation sheetAnimation = ParticleSheetAnimation::WholeSheet;
  ParticleSheetRowMode sheetRowMode = ParticleSheetRowMode::Custom;
  float sheetRow = 0;
  // Frame over Time em fração da sequência (0 = primeiro quadro, 1 = depois do
  // último), como a curva da Unity normalizada; o padrão percorre a vida toda.
  ParticleMinMaxCurve sheetFrameOverTime = [] {
    auto value = particleCurveDefault(0);
    value.mode = ParticleCurveMode::Curve;
    value.multiplier = 1;
    value.curveMin = value.curveMax = particleLinearCurve(0, 1);
    return value;
  }();
  ParticleMinMaxCurve sheetStartFrame = particleCurveDefault(0);
  float sheetCycles = 1;

  // --- Luzes (PK-10b) ---------------------------------------------------------
  // Lights da Unity: uma luz modelo (objeto com componente Luz, pode estar
  // inativo) copiada para parte das partículas, com teto por sistema.
  bool lightsEnabled = false;
  u64 lightTemplate = 0;
  float lightRatio = 0.1f;
  float lightMaxLights = 8;
  bool lightUseParticleColor = true;
  bool lightSizeAffectsRange = false;
  bool lightAlphaAffectsIntensity = true;
  float lightRangeMultiplier = 1;
  float lightIntensityMultiplier = 1;

  // --- Trilhas (PK-10c) ------------------------------------------------------
  // Trails da Unity no modo Particles: pontos guardados por partícula (no
  // máximo kTrailPoints), desenhados como segmentos esticados no mesmo passe e
  // com o mesmo material. Morrem com a partícula (Die with Particles).
  static constexpr u32 kTrailPoints = 12;
  bool trailsEnabled = false;
  float trailRatio = 1;
  float trailLifetime = 0.3f;
  float trailMinVertexDistance = 0.2f;
  float trailWidth = 1;
  float trailWidthEnd = 0;
  bool trailInheritColor = true;
  bool trailFade = true;
  u32 sheetFrameCount() const noexcept {
    const u32 columns = static_cast<u32>(sheetTiles[0]), rows = static_cast<u32>(sheetTiles[1]);
    return sheetAnimation == ParticleSheetAnimation::SingleRow ? columns : columns * rows;
  }
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
inline bool particleMeshMode(const ComponentValue &v) { return particleSystem(v).renderMode == ParticleRenderMode::Mesh; }
inline bool particleDistorts(const ComponentValue &v) { return particleSystem(v).materialBlend == ParticleBlend::Distortion; }
inline bool particleShapeMesh(const ComponentValue &v) {
  return particleSystem(v).shapeEnabled && particleSystem(v).shapeType == ParticleShapeType::Mesh;
}
inline bool particleAlignable(const ComponentValue &v) {
  const auto mode = particleSystem(v).renderMode;
  return mode == ParticleRenderMode::Billboard || mode == ParticleRenderMode::Mesh;
}
inline bool particleSize3DOn(const ComponentValue &v) { return particleSystem(v).size3D; }
inline bool particleVelocityOn(const ComponentValue &v) { return particleSystem(v).velocityOverLifetimeEnabled; }
inline bool particleRotation3DOn(const ComponentValue &v) { return particleSystem(v).rotation3D; }
inline bool particleSizeAxesOn(const ComponentValue &v) {
  const auto &p = particleSystem(v);
  return p.sizeOverLifetimeEnabled && p.sizeSeparateAxes;
}
inline bool particleRotationAxesOn(const ComponentValue &v) {
  const auto &p = particleSystem(v);
  return p.rotationOverLifetimeEnabled && p.rotationSeparateAxes;
}
// Curva de um eixo opcional: aparece quando o eixo está ligado E o modo pede.
template <bool (*Gate)(const ComponentValue &), ParticleMinMaxCurve ParticleSystemSettings::*M>
bool particleGatedCurveShowsConstant(const ComponentValue &v) { return Gate(v) && particleCurveShowsConstant<M>(v); }
template <bool (*Gate)(const ComponentValue &), ParticleMinMaxCurve ParticleSystemSettings::*M>
bool particleGatedCurveShowsMin(const ComponentValue &v) { return Gate(v) && particleCurveShowsMin<M>(v); }
template <bool (*Gate)(const ComponentValue &), ParticleMinMaxCurve ParticleSystemSettings::*M>
bool particleGatedCurveShowsCurve(const ComponentValue &v) { return Gate(v) && particleCurveShowsCurve<M>(v); }
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
inline constexpr std::array<ComponentEnumOption, 9> particleShapeOptions{{
  {0, "Esfera"}, {1, "Hemisfério"}, {2, "Cone"}, {3, "Caixa"}, {4, "Círculo"}, {5, "Aresta"}, {6, "Rosca"}, {7, "Retângulo"},
  {8, "Malha"}}};
inline constexpr std::array<ComponentEnumOption, 2> particleShapeMeshPlacementOptions{{{0, "Vértice"}, {1, "Superfície"}}};
inline constexpr std::array<ComponentEnumOption, 2> particleConeEmitOptions{{{0, "Base"}, {1, "Volume"}}};
inline constexpr std::array<ComponentEnumOption, 3> particleBoxEmitOptions{{{0, "Volume"}, {1, "Casca"}, {2, "Arestas"}}};
inline constexpr std::array<ComponentEnumOption, 4> particleRenderModeOptions{{
  {0, "Billboard"}, {1, "Billboard esticado"}, {2, "Malha"}, {3, "Nenhum"}}};
inline constexpr std::array<ComponentEnumOption, 4> particleAlignmentOptions{{
  {0, "Vista"}, {1, "Mundo"}, {2, "Local"}, {3, "Velocidade"}}};
inline constexpr std::array<ComponentEnumOption, 4> particleMeshShapeOptions{{
  {0, "Quadrado"}, {1, "Cubo"}, {2, "Esfera"}, {3, "Cilindro"}}};
inline constexpr std::array<ComponentEnumOption, 3> particleScalingOptions{{
  {0, "Hierarquia"}, {1, "Local"}, {2, "Forma"}}};
inline constexpr std::array<ComponentEnumOption, 2> particleCollisionTypeOptions{{{0, "Planos"}, {1, "Mundo"}}};
inline constexpr std::array<ComponentEnumOption, 3> particleSubEmitterEventOptions{{{0, "Ao nascer"}, {1, "Ao colidir"}, {2, "Ao morrer"}}};
inline constexpr std::array<ComponentEnumOption, 4> particleSortOptions{{
  {0, "Nenhuma"}, {1, "Por distância"}, {2, "Mais antigas na frente"}, {3, "Mais novas na frente"}}};
inline constexpr std::array<ComponentEnumOption, 3> particleBlendOptions{{{0, "Mistura"}, {1, "Aditivo"}, {2, "Distorção"}}};
inline constexpr std::array<ComponentEnumOption, 8> particleTextureOptions{{
  {0, "Ponto suave"}, {1, "Brilho"}, {2, "Chama"}, {3, "Fumaça"}, {4, "Faísca"}, {5, "Anel"}, {6, "Estrela"}, {7, "Quadrado"}}};
inline constexpr std::array<ComponentEnumOption, 4> particleNoiseOptions{{
  {0, "Nenhum"}, {1, "Nuvens"}, {2, "Células"}, {3, "Faixas"}}};
inline constexpr std::array<ComponentEnumOption, 4> particleMaskOptions{{
  {0, "Nenhuma"}, {1, "Radial"}, {2, "Desvanece para cima"}, {3, "Anel"}}};

inline bool particleCollides(const ComponentValue &v) { return particleSystem(v).collisionEnabled; }
inline bool particleCollidesWithPlanes(const ComponentValue &v) {
  return particleCollides(v) && particleSystem(v).collisionType == ParticleCollisionType::Planes;
}
inline bool particleSheetOn(const ComponentValue &v) { return particleSystem(v).textureSheetEnabled; }
inline bool particleLightsOn(const ComponentValue &v) { return particleSystem(v).lightsEnabled; }
inline bool particleTrailsOn(const ComponentValue &v) { return particleSystem(v).trailsEnabled; }
inline bool particleSheetSingleRow(const ComponentValue &v) {
  return particleSheetOn(v) && particleSystem(v).sheetAnimation == ParticleSheetAnimation::SingleRow;
}
inline bool particleSheetCustomRow(const ComponentValue &v) {
  return particleSheetSingleRow(v) && particleSystem(v).sheetRowMode == ParticleSheetRowMode::Custom;
}
template <ParticleMinMaxCurve ParticleSystemSettings::*M> bool particleSheetCurveShowsConstant(const ComponentValue &v) {
  return particleSheetOn(v) && particleCurveShowsConstant<M>(v);
}
template <ParticleMinMaxCurve ParticleSystemSettings::*M> bool particleSheetCurveShowsMin(const ComponentValue &v) {
  return particleSheetOn(v) && particleCurveShowsMin<M>(v);
}
template <ParticleMinMaxCurve ParticleSystemSettings::*M> bool particleSheetCurveShowsCurve(const ComponentValue &v) {
  return particleSheetOn(v) && particleCurveShowsCurve<M>(v);
}
inline constexpr std::array<ComponentEnumOption, 2> particleSheetAnimationOptions{{{0, "Folha inteira"}, {1, "Uma linha"}}};
inline constexpr std::array<ComponentEnumOption, 2> particleSheetRowModeOptions{{{0, "Escolhida"}, {1, "Sorteada"}}};
inline bool particleHasSubEmitters(const ComponentValue &v) { return particleSystem(v).subEmittersEnabled; }
// Extra N aparece quando o anterior está em uso (ou ele mesmo): a lista cresce
// conforme o autor preenche, sem sete linhas vazias de saída.
template <usize N> inline bool particleExtraSubEmitterShows(const ComponentValue &v) {
  const auto &s = particleSystem(v);
  return particleHasSubEmitters(v) && (N == 0 || s.subEmitterExtra[N] != 0 || s.subEmitterExtra[N - 1] != 0);
}
#include "scene/generated/particle_system_particleSystemNumbers.inc"
#include "scene/generated/particle_system_particleSystemBooleans.inc"
#include "scene/generated/particle_system_particleSystemEnums.inc"
// Planos e sub-sistemas são objetos da cena, com referência persistente
// remapeada por clone e prefab como qualquer outra.
inline constexpr const char *kParticleExtraSubEmitterIds[]{"sub_emitter_extra_0", "sub_emitter_extra_1", "sub_emitter_extra_2",
                                                            "sub_emitter_extra_3", "sub_emitter_extra_4", "sub_emitter_extra_5",
                                                            "sub_emitter_extra_6"};
template <usize N> constexpr ComponentObjectReference particleExtraSubEmitter(const char *name) {
  return {kParticleExtraSubEmitterIds[N], name, "astra.effects.particle_system",
          ObjectReferenceScope::Other, "Nenhum",
          [](const ComponentValue &v) { return particleSystem(v).subEmitterExtra[N]; },
          [](ComponentValue &v, u64 id) { particleSystem(v).subEmitterExtra[N] = id; },
          {"Sub-emissores", "", "Mais um sistema disparado no acontecimento escolhido; emite as rajadas dele", particleExtraSubEmitterShows<N>,
           nullptr, "render.particles.sub_emitters", "runtime/scene_particles.cpp"}};
}
inline constexpr std::array<ComponentObjectReference, 13> particleSystemReferences{{
  {"collision_plane_0", "Plano 1", "", ObjectReferenceScope::Other, "Nenhum",
   [](const ComponentValue &v) { return particleSystem(v).collisionPlane0; },
   [](ComponentValue &v, u64 id) { particleSystem(v).collisionPlane0 = id; },
   {"Colisão", "", "O +Y do objeto é a normal; a partícula quica do lado de cima", particleCollidesWithPlanes, nullptr,
    "render.particles.collision", "renderer/particles/particle_simulation.cpp"}},
  {"collision_plane_1", "Plano 2", "", ObjectReferenceScope::Other, "Nenhum",
   [](const ComponentValue &v) { return particleSystem(v).collisionPlane1; },
   [](ComponentValue &v, u64 id) { particleSystem(v).collisionPlane1 = id; },
   {"Colisão", "", "Segundo plano (parede, teto)", particleCollidesWithPlanes, nullptr,
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
  {"light_template", "Luz modelo", "astra.render.light", ObjectReferenceScope::Other, "Nenhuma",
   [](const ComponentValue &v) { return particleSystem(v).lightTemplate; },
   [](ComponentValue &v, u64 id) { particleSystem(v).lightTemplate = id; },
   {"Luzes", "", "Objeto com Luz pontual ou spot: cor, intensidade, alcance e unidade copiados (pode ficar inativo)",
    particleLightsOn, nullptr, "render.particles.lights", "runtime/scene_particles.cpp"}},
  particleExtraSubEmitter<0>("Extra 1"), particleExtraSubEmitter<1>("Extra 2"), particleExtraSubEmitter<2>("Extra 3"),
  particleExtraSubEmitter<3>("Extra 4"), particleExtraSubEmitter<4>("Extra 5"), particleExtraSubEmitter<5>("Extra 6"),
  particleExtraSubEmitter<6>("Extra 7"),
}};

// Material do projeto (PK-10a). Ausente = material embutido do componente.
// Malha do projeto (G6a): no modo Malha, substitui a malha embutida.
inline constexpr std::array<ComponentResourceBinding, 3> particleSystemResources{{
  {"material", "Material", resources::AssetType::Material, [](const ComponentValue &) -> u32 { return 1; },
   [](const ComponentValue &v, u32) { return particleSystem(v).materialAsset; },
   [](ComponentValue &v, u32 slot, resources::AssetGuid value) {
     if (slot) return false;
     particleSystem(v).materialAsset = value;
     return true;
   },
   {"Material", "", "Material do projeto com shader Partícula: a imagem dele (e a folha de quadros) substitui a textura própria",
    nullptr, nullptr, "render.particles.material", "platform/android/instanced_particles.inl"}},
  {"mesh", "Malha do projeto", resources::AssetType::Mesh, [](const ComponentValue &) -> u32 { return 1; },
   [](const ComponentValue &v, u32) { return particleSystem(v).meshAsset; },
   [](ComponentValue &v, u32 slot, resources::AssetGuid value) {
     if (slot) return false;
     particleSystem(v).meshAsset = value;
     return true;
   },
   {"Renderização", "", "Malha do projeto desenhada em cada partícula; vazia = a malha embutida acima",
    particleMeshMode, nullptr, "render.particles.mesh", "platform/android/instanced_particles.inl + rhi/shaders/particle_mesh.vert"}},
  {"shape_mesh", "Malha da forma", resources::AssetType::Mesh, [](const ComponentValue &) -> u32 { return 1; },
   [](const ComponentValue &v, u32) { return particleSystem(v).shapeMeshAsset; },
   [](ComponentValue &v, u32 slot, resources::AssetGuid value) {
     if (slot) return false;
     particleSystem(v).shapeMeshAsset = value;
     return true;
   },
   {"Forma", "", "Malha do projeto de onde as partículas nascem; vazia, nascem no centro", particleShapeMesh, nullptr,
    "render.particles.simulation", "renderer/particles/particle_simulation.cpp"}}}};

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
  "astra.effects.particle_system", 5, []() -> std::unique_ptr<ComponentValue> { return std::make_unique<ParticleSystem>(); },
  particleSystemNumbers, particleSystemBooleans, particleSystemEnums, nullptr, false, particleSystemReferences, particleSystemTriples,
  particleSystemResources, {}, {}, {}, particleSystemMethods, particleSystemEvents};

inline std::span<const ParticleSystem::CurveSlot> ParticleSystem::curveSlots() {
  static constexpr std::array<CurveSlot, 66> slots{{
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
    {"sheet_frame.min", &ParticleSystem::sheetFrameOverTime, false}, {"sheet_frame.max", &ParticleSystem::sheetFrameOverTime, true},
    // G6a: eixos de tamanho e rotação.
    {"start_size_y.min", &ParticleSystem::startSizeY, false}, {"start_size_y.max", &ParticleSystem::startSizeY, true},
    {"start_size_z.min", &ParticleSystem::startSizeZ, false}, {"start_size_z.max", &ParticleSystem::startSizeZ, true},
    {"start_rotation_x.min", &ParticleSystem::startRotationX, false}, {"start_rotation_x.max", &ParticleSystem::startRotationX, true},
    {"start_rotation_y.min", &ParticleSystem::startRotationY, false}, {"start_rotation_y.max", &ParticleSystem::startRotationY, true},
    {"size_over_lifetime_y.min", &ParticleSystem::sizeOverLifetimeY, false}, {"size_over_lifetime_y.max", &ParticleSystem::sizeOverLifetimeY, true},
    {"size_over_lifetime_z.min", &ParticleSystem::sizeOverLifetimeZ, false}, {"size_over_lifetime_z.max", &ParticleSystem::sizeOverLifetimeZ, true},
    {"rotation_over_lifetime_x.min", &ParticleSystem::rotationOverLifetimeX, false},
    {"rotation_over_lifetime_x.max", &ParticleSystem::rotationOverLifetimeX, true},
    {"rotation_over_lifetime_y.min", &ParticleSystem::rotationOverLifetimeY, false},
    {"rotation_over_lifetime_y.max", &ParticleSystem::rotationOverLifetimeY, true},
    {"velocity_orbital_x.min", &ParticleSystem::velocityOrbitalX, false}, {"velocity_orbital_x.max", &ParticleSystem::velocityOrbitalX, true},
    {"velocity_orbital_y.min", &ParticleSystem::velocityOrbitalY, false}, {"velocity_orbital_y.max", &ParticleSystem::velocityOrbitalY, true},
    {"velocity_orbital_z.min", &ParticleSystem::velocityOrbitalZ, false}, {"velocity_orbital_z.max", &ParticleSystem::velocityOrbitalZ, true},
    {"velocity_radial.min", &ParticleSystem::velocityRadial, false}, {"velocity_radial.max", &ParticleSystem::velocityRadial, true},
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
         subEmitterCollisionCount == std::floor(subEmitterCollisionCount) && subEmitterDeathCount == std::floor(subEmitterDeathCount) &&
         sheetTiles[0] == std::floor(sheetTiles[0]) && sheetTiles[1] == std::floor(sheetTiles[1]) && sheetRow == std::floor(sheetRow) &&
         sheetStartFrame.constantMin == std::floor(sheetStartFrame.constantMin) &&
         sheetStartFrame.constantMax == std::floor(sheetStartFrame.constantMax) && !sheetStartFrame.usesCurves() &&
         lightMaxLights == std::floor(lightMaxLights);
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
  // v3: o Material do projeto, por identidade ("-" = material embutido).
  out << ' ' << (materialAsset.valid() ? materialAsset.text() : std::string("-"));
  // v4: a Malha do projeto ("-" = malha embutida).
  out << ' ' << (meshAsset.valid() ? meshAsset.text() : std::string("-"));
  // v5: a Malha da forma ("-" = nenhuma).
  out << ' ' << (shapeMeshAsset.valid() ? shapeMeshAsset.text() : std::string("-"));
}

inline bool ParticleSystem::read(std::istream &in, u32 version) {
  // v2 (P2a) só acrescenta ids; um arquivo v1 lê os mesmos campos e deixa os
  // módulos novos desligados no padrão. Versão futura é recusada.
  // v3 (PK-10a) acrescenta ids e, no fim, o Material do projeto.
  // v4 (G6a) acrescenta ids (malha, alinhamento, eixos 3D, colisão Mundo) e,
  // no fim, a Malha do projeto. Arquivo v3 abre com os módulos novos no padrão.
  // v5 (G6a) acrescenta a forma Malha e, no fim, a Malha da forma.
  if (version < 1 || version > 5) return false;
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
  if (!(in >> count) || count > 128) return false;
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
  if (version >= 3) {
    std::string material;
    if (!(in >> material)) return false;
    if (material != "-" && !resources::AssetGuid::parse(material, candidate.materialAsset)) return false;
  }
  if (version >= 4) {
    std::string mesh;
    if (!(in >> mesh)) return false;
    if (mesh != "-" && !resources::AssetGuid::parse(mesh, candidate.meshAsset)) return false;
  }
  if (version >= 5) {
    std::string mesh;
    if (!(in >> mesh)) return false;
    if (mesh != "-" && !resources::AssetGuid::parse(mesh, candidate.shapeMeshAsset)) return false;
  }
  if (!candidate.valid()) return false;
  static_cast<ParticleSystemSettings &>(*this) = candidate;
  return true;
}

} // namespace ae::scene

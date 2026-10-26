#include "resources/unity_particle_import.h"

#include "core/rotation_math.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace ae::resources {
namespace {
using Node = UnityYamlNode;
using scene::ParticleCurveMode;
using scene::ParticleGradientMode;
constexpr float kDegreesPerRadian = 57.29577951308232f;

struct Notes {
  std::vector<std::string> &losses, &approximations;
  void loss(std::string text) {
    if (std::find(losses.begin(), losses.end(), text) == losses.end()) losses.push_back(std::move(text));
  }
  void approximate(std::string text) {
    if (std::find(approximations.begin(), approximations.end(), text) == approximations.end()) approximations.push_back(std::move(text));
  }
};

float number(const Node *node, float fallback) {
  double value = 0;
  if (!node || !node->number(value, true) || !std::isfinite(value)) return fallback;
  return static_cast<float>(std::clamp(value, -1.0e7, 1.0e7));
}
float number(const Node &node, std::string_view key, float fallback) { return number(node.get(key), fallback); }
i64 integer(const Node &node, std::string_view key, i64 fallback) {
  i64 value = 0;
  const auto *found = node.get(key);
  return found && found->integer(value) ? value : fallback;
}
bool flag(const Node &node, std::string_view key, bool fallback) {
  bool value = false;
  const auto *found = node.get(key);
  return found && found->boolean(value) ? value : fallback;
}
bool enabled(const Node *module) { return module && flag(*module, "enabled", false); }
void vector3(const Node &node, std::string_view key, float (&out)[3], const float (&fallback)[3]) {
  const auto *value = node.get(key);
  for (u32 c = 0; c < 3; ++c) out[c] = value ? number(*value, std::string_view("xyz").substr(c, 1), fallback[c]) : fallback[c];
}
i64 localReference(const Node *node) {
  UnityObjectReference reference;
  return node && unityObjectReference(*node, reference) && !reference.external() ? reference.fileId : 0;
}

// Curva da Unity (AnimationCurve serializado): tempo, valor e as duas
// inclinações. Tangente infinita (degrau) vira Constante.
scene::ScriptCurve curve(const Node *node, float scale) {
  scene::ScriptCurve out;
  const auto *keys = node ? node->get("m_Curve") : nullptr;
  if (keys && keys->kind == Node::Kind::Sequence)
    for (const auto &key : keys->elements) {
      if (out.keys.size() >= scene::ScriptCurve::kMaximumKeys) break;
      scene::CurveKey k;
      k.time = std::clamp(number(key, "time", 0), 0.0f, 1.0f);
      k.value = number(key, "value", 0) * scale;
      double in = 0, outSlope = 0;
      const auto *inNode = key.get("inSlope"), *outNode = key.get("outSlope");
      const bool inFinite = inNode && inNode->number(in, true) && std::isfinite(in);
      const bool outFinite = outNode && outNode->number(outSlope, true) && std::isfinite(outSlope);
      k.in = inFinite ? static_cast<float>(std::clamp(in * scale, -1.0e6, 1.0e6)) : 0.0f;
      k.out = outFinite ? static_cast<float>(std::clamp(outSlope * scale, -1.0e6, 1.0e6)) : 0.0f;
      k.left = inFinite ? scene::CurveTangentMode::Free : scene::CurveTangentMode::Constant;
      k.right = outFinite ? scene::CurveTangentMode::Free : scene::CurveTangentMode::Constant;
      k.broken = true;
      if (!out.keys.empty() && k.time <= out.keys.back().time) k.time = out.keys.back().time + 1e-4f;
      if (k.time > 1.0f) break;
      out.keys.push_back(k);
    }
  if (out.keys.empty()) out = scene::particleConstantCurve(0);
  else if (out.keys.size() == 1) {
    auto last = out.keys.front();
    last.time = out.keys.front().time < 1 ? 1.0f : 0.0f;
    if (last.time > out.keys.front().time) out.keys.push_back(last);
    else out.keys.insert(out.keys.begin(), last);
  }
  return out;
}
float curvePeak(const scene::ScriptCurve &value) {
  float peak = 0;
  for (const auto &key : value.keys) peak = std::max(peak, std::abs(key.value));
  return peak;
}

// MinMaxCurve: minMaxState 0 constante, 1 curva, 2 duas curvas, 3 duas
// constantes (a Astra numera 2 e 3 ao contrário). `unit` converte unidade e
// sentido (radiano→grau, reflexão em Z).
scene::ParticleMinMaxCurve minMax(const Node *node, float unit, const scene::ParticleMinMaxCurve &fallback) {
  if (!node || node->kind != Node::Kind::Map) return fallback;
  scene::ParticleMinMaxCurve out;
  const i64 state = integer(*node, "minMaxState", 0);
  const float scalar = number(*node, "scalar", 0) * unit, minimum = number(*node, "minScalar", 0) * unit;
  out.constantMax = scalar;
  out.constantMin = scalar;
  out.multiplier = scalar;
  out.curveMax = curve(node->get("maxCurve"), 1);
  out.curveMin = curve(node->get("minCurve"), 1);
  switch (state) {
  case 1: out.mode = ParticleCurveMode::Curve; break;
  case 2: out.mode = ParticleCurveMode::TwoCurves; break;
  case 3: out.mode = ParticleCurveMode::TwoConstants; out.constantMin = minimum; break;
  default: out.mode = ParticleCurveMode::Constant; break;
  }
  return out;
}
// Para campos que na Astra só têm número: a constante, o meio das duas, ou o
// pico da curva (com aviso).
float constantOf(const scene::ParticleMinMaxCurve &value, Notes &notes, const char *what) {
  switch (value.mode) {
  case ParticleCurveMode::Constant: return value.constantMax;
  case ParticleCurveMode::TwoConstants: return (value.constantMin + value.constantMax) * .5f;
  default:
    notes.approximate(std::string(what) + ": curva no tempo reduzida ao valor de pico.");
    return value.multiplier * std::max(curvePeak(value.curveMax), curvePeak(value.curveMin));
  }
}

float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
void srgbColor(const Node *node, float (&out)[4]) {
  if (!node) return;
  out[0] = unityLinearFromSrgb(number(*node, "r", 1));
  out[1] = unityLinearFromSrgb(number(*node, "g", 1));
  out[2] = unityLinearFromSrgb(number(*node, "b", 1));
  out[3] = clamp01(number(*node, "a", 1));
  for (u32 c = 0; c < 3; ++c) out[c] = std::clamp(out[c], 0.0f, 65504.0f);
}
// Gradient da Unity: até 8 chaves de cor (key0..7 rgb, ctime) e 8 de alfa
// (key0..7 a, atime), tempos em 0..65535.
scene::ScriptGradient gradient(const Node *node) {
  scene::ScriptGradient out;
  if (!node || node->kind != Node::Kind::Map) return out;
  const auto colors = static_cast<u32>(std::clamp<i64>(integer(*node, "m_NumColorKeys", 2), 1, 8));
  const auto alphas = static_cast<u32>(std::clamp<i64>(integer(*node, "m_NumAlphaKeys", 2), 1, 8));
  out.mode = integer(*node, "m_Mode", 0) == 1 ? scene::GradientMode::Fixed : scene::GradientMode::Blend;
  out.colors.clear();
  out.alphas.clear();
  for (u32 i = 0; i < colors; ++i) {
    float c[4]{1, 1, 1, 1};
    srgbColor(node->get("key" + std::to_string(i)), c);
    scene::GradientColorKey key;
    key.time = clamp01(number(*node, "ctime" + std::to_string(i), 0) / 65535.0f);
    std::copy(c, c + 3, key.rgb);
    out.colors.push_back(key);
  }
  for (u32 i = 0; i < alphas; ++i) {
    const auto *k = node->get("key" + std::to_string(i));
    scene::GradientAlphaKey key;
    key.time = clamp01(number(*node, "atime" + std::to_string(i), 0) / 65535.0f);
    key.alpha = k ? clamp01(number(*k, "a", 1)) : 1.0f;
    out.alphas.push_back(key);
  }
  out.sort();
  return out;
}
scene::ParticleMinMaxGradient minMaxGradient(const Node *node, const scene::ParticleMinMaxGradient &fallback) {
  if (!node || node->kind != Node::Kind::Map) return fallback;
  scene::ParticleMinMaxGradient out;
  const i64 state = std::clamp<i64>(integer(*node, "minMaxState", 0), 0, 4);
  out.mode = static_cast<ParticleGradientMode>(state);
  srgbColor(node->get("minColor"), out.colorMin);
  srgbColor(node->get("maxColor"), out.colorMax);
  out.gradientMax = gradient(node->get("maxGradient"));
  out.gradientMin = gradient(node->get("minGradient"));
  return out;
}
// Cor no tempo da Astra só tem gradiente ou dois gradientes.
scene::ParticleMinMaxGradient lifetimeGradient(scene::ParticleMinMaxGradient value, Notes &notes) {
  const auto constant = [](const float (&c)[4]) {
    scene::ScriptGradient g;
    g.colors = {{0, {c[0], c[1], c[2]}}, {1, {c[0], c[1], c[2]}}};
    g.alphas = {{0, c[3]}, {1, c[3]}};
    return g;
  };
  switch (value.mode) {
  case ParticleGradientMode::Color:
    value.gradientMax = value.gradientMin = constant(value.colorMax);
    value.mode = ParticleGradientMode::Gradient;
    break;
  case ParticleGradientMode::TwoColors:
    value.gradientMax = constant(value.colorMax);
    value.gradientMin = constant(value.colorMin);
    value.mode = ParticleGradientMode::TwoGradients;
    break;
  case ParticleGradientMode::RandomColor:
    notes.approximate("Cor no tempo com cor aleatória do gradiente vira o próprio gradiente.");
    value.mode = ParticleGradientMode::Gradient;
    break;
  default: break;
  }
  return value;
}

float linearScalar(const Node *node) {
  // Campo que na Unity é MinMaxCurve escalar ou um número (versões antigas).
  if (!node) return 0;
  if (node->kind == Node::Kind::Scalar) return number(node, 0);
  return number(*node, "value", number(*node, "scalar", 0));
}

// Quatérnio de um euler da Unity em graus (ordem Z, X, Y).
void unityEulerQuaternion(const float (&degrees)[3], float (&q)[4]) {
  const float x = degrees[0] / kDegreesPerRadian * .5f, y = degrees[1] / kDegreesPerRadian * .5f, z = degrees[2] / kDegreesPerRadian * .5f;
  const float qx[4]{std::sin(x), 0, 0, std::cos(x)}, qy[4]{0, std::sin(y), 0, std::cos(y)}, qz[4]{0, 0, std::sin(z), std::cos(z)};
  const auto mul = [](const float (&a)[4], const float (&b)[4], float (&o)[4]) {
    const float r[4]{a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
                     a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3], a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
    std::copy(r, r + 4, o);
  };
  float t[4];
  mul(qx, qz, t);
  mul(qy, t, q);
}

void convertShape(const Node *shape, scene::ParticleSystemSettings &s, Notes &notes) {
  // A Astra emite para +Z da forma; o objeto refletido em Z pede −Z. Meia-volta
  // em X junto da rotação da forma (refletida: x e y do quatérnio negados).
  float rotation[3]{0, 0, 0};
  if (shape && enabled(shape)) {
    const float zero[3]{0, 0, 0};
    vector3(*shape, "m_Rotation", rotation, zero);
  }
  float q[4];
  unityEulerQuaternion(rotation, q);
  q[0] = -q[0];
  q[1] = -q[1];
  const float flip[4]{1, 0, 0, 0};
  const float r[4]{q[3] * flip[0] + q[0] * flip[3] + q[1] * flip[2] - q[2] * flip[1],
                   q[3] * flip[1] - q[0] * flip[2] + q[1] * flip[3] + q[2] * flip[0],
                   q[3] * flip[2] + q[0] * flip[1] - q[1] * flip[0] + q[2] * flip[3],
                   q[3] * flip[3] - q[0] * flip[0] - q[1] * flip[1] - q[2] * flip[2]};
  float shapeEuler[3]{0, 0, 0};
  rotationEulerXYZ(r, shapeEuler);
  std::copy(shapeEuler, shapeEuler + 3, s.shapeRotation);
  s.shapeEnabled = true;
  if (!shape || !enabled(shape)) {
    // Forma desligada na Unity: tudo nasce no centro, para a frente.
    s.shapeType = scene::ParticleShapeType::Cone;
    s.shapeRadius = 0;
    s.shapeAngle = 0;
    s.shapeConeEmitFrom = scene::ParticleConeEmitFrom::Base;
    return;
  }
  const float zero[3]{0, 0, 0}, one[3]{1, 1, 1};
  float position[3], scale[3];
  vector3(*shape, "m_Position", position, zero);
  vector3(*shape, "m_Scale", scale, one);
  s.shapePosition[0] = position[0];
  s.shapePosition[1] = position[1];
  s.shapePosition[2] = -position[2];
  s.shapeAngle = std::clamp(number(*shape, "angle", 25), 0.0f, 90.0f);
  s.shapeLength = number(*shape, "length", 5);
  s.shapeRadiusThickness = clamp01(number(*shape, "radiusThickness", 1));
  s.shapeDonutRadius = number(*shape, "donutRadius", .2f);
  s.shapeRadius = linearScalar(shape->get("radius"));
  if (const auto *arc = shape->get("arc")) s.shapeArc = std::clamp(linearScalar(arc), 0.0f, 360.0f);
  if (const auto *arc = shape->get("arc"); arc && arc->kind == Node::Kind::Map && integer(*arc, "mode", 0) != 0)
    notes.approximate("Arco da forma em modo Laço/Vai e volta emite pelo arco inteiro, sorteado.");
  s.shapeRandomDirection = clamp01(number(*shape, "randomDirectionAmount", 0));
  s.shapeSpherizeDirection = clamp01(number(*shape, "sphericalDirectionAmount", 0));
  if (number(*shape, "randomPositionAmount", 0) != 0) notes.approximate("Posição aleatória da forma (Randomize Position) não existe na Astra.");
  if (flag(*shape, "alignToDirection", false)) notes.approximate("Alinhar à direção da forma vira o giro inicial autoral.");
  const bool uniform = std::abs(scale[0] - scale[1]) < 1e-4f && std::abs(scale[1] - scale[2]) < 1e-4f;
  // Escala da forma: tamanho da caixa; nas outras formas o raio cresce junto.
  const auto scaleRound = [&] {
    const float mean = (std::abs(scale[0]) + std::abs(scale[1]) + std::abs(scale[2])) / 3.0f;
    if (!uniform) notes.approximate("Escala não uniforme da forma vira a média nos três eixos.");
    s.shapeRadius *= mean;
    s.shapeLength *= mean;
    s.shapeDonutRadius *= mean;
  };
  const i64 type = integer(*shape, "type", 4);
  switch (type) {
  case 0: s.shapeType = scene::ParticleShapeType::Sphere; scaleRound(); break;
  case 1: s.shapeType = scene::ParticleShapeType::Sphere; s.shapeRadiusThickness = 0; scaleRound(); break;
  case 2: s.shapeType = scene::ParticleShapeType::Hemisphere; scaleRound(); break;
  case 3: s.shapeType = scene::ParticleShapeType::Hemisphere; s.shapeRadiusThickness = 0; scaleRound(); break;
  case 4: s.shapeType = scene::ParticleShapeType::Cone; s.shapeConeEmitFrom = scene::ParticleConeEmitFrom::Base; scaleRound(); break;
  case 7: s.shapeType = scene::ParticleShapeType::Cone; s.shapeConeEmitFrom = scene::ParticleConeEmitFrom::Base; s.shapeRadiusThickness = 0; scaleRound(); break;
  case 8: s.shapeType = scene::ParticleShapeType::Cone; s.shapeConeEmitFrom = scene::ParticleConeEmitFrom::Volume; scaleRound(); break;
  case 9: s.shapeType = scene::ParticleShapeType::Cone; s.shapeConeEmitFrom = scene::ParticleConeEmitFrom::Volume; s.shapeRadiusThickness = 0; scaleRound(); break;
  case 10: s.shapeType = scene::ParticleShapeType::Circle; scaleRound(); break;
  case 11: s.shapeType = scene::ParticleShapeType::Circle; s.shapeRadiusThickness = 0; scaleRound(); break;
  case 12: s.shapeType = scene::ParticleShapeType::Edge; scaleRound(); break;
  case 17: s.shapeType = scene::ParticleShapeType::Donut; scaleRound(); break;
  case 5: case 15: case 16: case 18: {
    s.shapeType = type == 18 ? scene::ParticleShapeType::Rectangle : scene::ParticleShapeType::Box;
    s.shapeBoxEmitFrom = type == 15 ? scene::ParticleBoxEmitFrom::Shell : type == 16 ? scene::ParticleBoxEmitFrom::Edge : scene::ParticleBoxEmitFrom::Volume;
    // Versões antigas guardavam a caixa em boxX/boxY/boxZ.
    if (shape->get("boxX")) { scale[0] = number(*shape, "boxX", 1); scale[1] = number(*shape, "boxY", 1); scale[2] = number(*shape, "boxZ", 1); }
    for (u32 c = 0; c < 3; ++c) s.shapeBox[c] = std::abs(scale[c]);
    break;
  }
  default:
    // Malha, Mesh Renderer, Skinned Mesh e Sprite: a Astra não emite da superfície.
    s.shapeType = scene::ParticleShapeType::Sphere;
    s.shapeRadius = .5f;
    notes.loss("Forma de malha/sprite (tipo " + std::to_string(type) + ") emite de uma esfera de 0,5 m.");
    break;
  }
}

} // namespace

float unityLinearFromSrgb(float value) {
  if (!std::isfinite(value) || value <= 0) return 0;
  return value <= .04045f ? value / 12.92f : std::pow((value + .055f) / 1.055f, 2.4f);
}

bool unityBuiltinParticleMesh(const UnityObjectReference &mesh, scene::ParticleMeshShape &shape, bool &approximate) {
  approximate = false;
  if (mesh.guid.text() != "0000000000000000e000000000000000") return false;
  switch (mesh.fileId) {
  case 10210: shape = scene::ParticleMeshShape::Quad; return true;
  case 10202: shape = scene::ParticleMeshShape::Cube; return true;
  case 10207: shape = scene::ParticleMeshShape::Sphere; return true;
  case 10206: shape = scene::ParticleMeshShape::Cylinder; return true;
  case 10208: shape = scene::ParticleMeshShape::Cylinder; approximate = true; return true;  // Capsule
  case 10209: shape = scene::ParticleMeshShape::Quad; approximate = true; return true;      // Plane
  default: return false;
  }
}

bool convertUnityParticleSystem(const Node &system, const Node *renderer, UnityParticleImport &out, std::string &diagnostic) {
  UnityParticleImport result;
  Notes notes{result.losses, result.approximations};
  auto &s = result.settings;
  if (system.kind != Node::Kind::Map) { diagnostic = "ParticleSystem Unity sem mapa de campos."; return false; }
  // --- Raiz e Principal ----------------------------------------------------
  s.duration = std::clamp(number(system, "lengthInSec", 5), 0.05f, 3600.0f);
  s.simulationSpeed = std::clamp(number(system, "simulationSpeed", 1), 0.0f, 100.0f);
  s.looping = flag(system, "looping", true);
  s.prewarm = flag(system, "prewarm", false);
  s.playOnAwake = flag(system, "playOnAwake", true);
  s.autoRandomSeed = flag(system, "autoRandomSeed", true);
  s.randomSeed = static_cast<float>(static_cast<u64>(std::abs(integer(system, "randomSeed", 0))) % 16777216u);
  s.stopAction = static_cast<scene::ParticleStopAction>(std::clamp<i64>(integer(system, "stopAction", 0), 0, 3));
  s.startDelay = minMax(system.get("startDelay"), 1, s.startDelay);
  if (s.startDelay.usesCurves()) {
    const float delay = constantOf(s.startDelay, notes, "Atraso inicial");
    s.startDelay = scene::particleCurveDefault(delay);
  }
  const i64 space = integer(system, "moveWithTransform", 0);
  s.simulationSpace = space == 0 ? scene::ParticleSimulationSpace::Local : scene::ParticleSimulationSpace::World;
  if (space == 2) notes.approximate("Espaço de simulação Personalizado vira Mundo.");
  s.scalingMode = static_cast<scene::ParticleScalingMode>(std::clamp<i64>(integer(system, "scalingMode", 0), 0, 2));
  if (integer(system, "ringBufferMode", 0) != 0) notes.approximate("Ring Buffer não existe na Astra: a partícula mais velha não é reaproveitada.");
  if (flag(system, "useUnscaledTime", false)) notes.approximate("Tempo sem escala vira o tempo da cena.");

  // Renderer primeiro: o sentido do giro de billboard depende do modo.
  i64 renderMode = 0, alignment = 0;
  if (renderer) {
    renderMode = integer(*renderer, "m_RenderMode", 0);
    alignment = integer(*renderer, "m_RenderAlignment", 0);
  }
  const bool viewBillboard = (renderMode == 0 || renderMode == 1) && (alignment == 0 || alignment == 3);
  const auto *initial = system.get("InitialModule");
  if (initial) {
    s.startLifetime = minMax(initial->get("startLifetime"), 1, s.startLifetime);
    s.startSpeed = minMax(initial->get("startSpeed"), 1, s.startSpeed);
    s.startColor = minMaxGradient(initial->get("startColor"), s.startColor);
    s.startSize = minMax(initial->get("startSize"), 1, s.startSize);
    s.size3D = flag(*initial, "size3D", false);
    s.startSizeY = minMax(initial->get("startSizeY"), 1, s.startSizeY);
    s.startSizeZ = minMax(initial->get("startSizeZ"), 1, s.startSizeZ);
    s.rotation3D = flag(*initial, "rotation3D", false);
    // Giro de billboard na Vista: a Unity gira no sentido horário da tela, a
    // Astra no anti-horário. Orientada (3D, malha, Local), reflexão: X e Y
    // trocam de sinal, Z fica.
    s.startRotation = minMax(initial->get("startRotation"), (viewBillboard && !s.rotation3D ? -1.0f : 1.0f) * kDegreesPerRadian, s.startRotation);
    s.startRotationX = minMax(initial->get("startRotationX"), -kDegreesPerRadian, s.startRotationX);
    s.startRotationY = minMax(initial->get("startRotationY"), -kDegreesPerRadian, s.startRotationY);
    if (number(*initial, "randomizeRotationDirection", 0) > 0)
      notes.approximate("Sentido de giro aleatório (Flip Rotation) não existe: todas giram no mesmo sentido.");
    s.maxParticles = std::clamp(std::floor(number(*initial, "maxNumParticles", 1000)), 1.0f, scene::ParticleSystemSettings::kMaximumParticles);
    s.gravityModifier = minMax(initial->get("gravityModifier"), 1, s.gravityModifier);
  }

  convertShape(system.get("ShapeModule"), s, notes);

  // --- Emissão --------------------------------------------------------------
  if (const auto *emission = system.get("EmissionModule")) {
    s.emissionEnabled = enabled(emission);
    s.emissionRate = minMax(emission->get("rateOverTime"), 1, s.emissionRate);
    if (const auto *distance = emission->get("rateOverDistance")) {
      auto value = minMax(distance, 1, scene::particleCurveDefault(0));
      s.emissionRateOverDistance = std::max(0.0f, constantOf(value, notes, "Taxa por distância"));
    }
    const auto *bursts = emission->get("m_Bursts");
    const i64 declared = integer(*emission, "m_BurstCount", bursts && bursts->kind == Node::Kind::Sequence ? static_cast<i64>(bursts->elements.size()) : 0);
    usize count = 0;
    if (bursts && bursts->kind == Node::Kind::Sequence)
      for (const auto &burst : bursts->elements) {
        if (count >= static_cast<usize>(std::max<i64>(0, declared))) break;
        if (count >= scene::ParticleSystemSettings::kMaximumBursts) {
          notes.loss("Mais de 4 rajadas: as seguintes são ignoradas.");
          break;
        }
        auto &b = s.bursts[count++];
        b.time = std::max(0.0f, number(burst, "time", 0));
        if (const auto *countCurve = burst.get("countCurve")) {
          const auto value = minMax(countCurve, 1, scene::particleCurveDefault(30));
          b.countMax = std::floor(std::max(0.0f, value.mode == ParticleCurveMode::TwoConstants ? value.constantMax : constantOf(value, notes, "Rajada")));
          b.countMin = std::floor(std::max(0.0f, value.mode == ParticleCurveMode::TwoConstants ? value.constantMin : b.countMax));
        } else {
          b.countMin = std::floor(std::max(0.0f, number(burst, "minCount", 30)));
          b.countMax = std::floor(std::max(0.0f, number(burst, "maxCount", b.countMin)));
        }
        b.cycles = std::floor(std::max(0.0f, number(burst, "cycleCount", 1)));
        b.interval = std::max(0.01f, number(burst, "repeatInterval", .01f));
        b.probability = clamp01(number(burst, "probability", 1));
      }
    s.burstCount = static_cast<float>(count);
  }

  // --- Ao longo da vida -----------------------------------------------------
  if (const auto *size = system.get("SizeModule")) {
    s.sizeOverLifetimeEnabled = enabled(size);
    s.sizeOverLifetime = minMax(size->get("curve"), 1, s.sizeOverLifetime);
    s.sizeSeparateAxes = flag(*size, "separateAxes", false);
    s.sizeOverLifetimeY = minMax(size->get("y"), 1, s.sizeOverLifetime);
    s.sizeOverLifetimeZ = minMax(size->get("z"), 1, s.sizeOverLifetime);
  }
  if (const auto *rotation = system.get("RotationModule")) {
    s.rotationOverLifetimeEnabled = enabled(rotation);
    s.rotationSeparateAxes = flag(*rotation, "separateAxes", false);
    const bool ownZ = viewBillboard && !s.rotation3D && !s.rotationSeparateAxes;
    s.rotationOverLifetime = minMax(rotation->get("curve"), (ownZ ? -1.0f : 1.0f) * kDegreesPerRadian, s.rotationOverLifetime);
    s.rotationOverLifetimeX = minMax(rotation->get("x"), -kDegreesPerRadian, s.rotationOverLifetimeX);
    s.rotationOverLifetimeY = minMax(rotation->get("y"), -kDegreesPerRadian, s.rotationOverLifetimeY);
  }
  if (const auto *color = system.get("ColorModule")) {
    s.colorOverLifetimeEnabled = enabled(color);
    s.colorOverLifetime = lifetimeGradient(minMaxGradient(color->get("gradient"), s.colorOverLifetime), notes);
  }

  // --- Folha de textura -------------------------------------------------------
  if (const auto *uv = system.get("UVModule"); enabled(uv)) {
    s.textureSheetEnabled = true;
    if (integer(*uv, "mode", 0) == 1) notes.loss("Folha de textura no modo Sprites não existe: fica a grade.");
    s.sheetTiles[0] = std::clamp(std::floor(number(*uv, "tilesX", 1)), 1.0f, 64.0f);
    s.sheetTiles[1] = std::clamp(std::floor(number(*uv, "tilesY", 1)), 1.0f, 64.0f);
    s.sheetAnimation = integer(*uv, "animationType", 0) == 1 ? scene::ParticleSheetAnimation::SingleRow : scene::ParticleSheetAnimation::WholeSheet;
    const bool random = uv->get("rowMode") ? integer(*uv, "rowMode", 0) == 1 : flag(*uv, "randomRow", true);
    s.sheetRowMode = random ? scene::ParticleSheetRowMode::Random : scene::ParticleSheetRowMode::Custom;
    s.sheetRow = std::clamp(std::floor(number(*uv, "rowIndex", 0)), 0.0f, s.sheetTiles[1] - 1);
    s.sheetFrameOverTime = minMax(uv->get("frameOverTime"), 1, s.sheetFrameOverTime);
    auto start = minMax(uv->get("startFrame"), 1, scene::particleCurveDefault(0));
    const float frames = static_cast<float>(std::max(1u, s.sheetFrameCount()));
    // O quadro inicial vem normalizado pela sequência.
    const auto frame = [&](float v) { return std::floor(std::clamp(v <= 1.0f ? v * frames : v, 0.0f, frames - 1)); };
    if (start.usesCurves()) start = scene::particleCurveDefault(constantOf(start, notes, "Quadro inicial"));
    start.constantMin = frame(start.constantMin);
    start.constantMax = frame(start.constantMax);
    start.mode = start.mode == ParticleCurveMode::TwoConstants ? ParticleCurveMode::TwoConstants : ParticleCurveMode::Constant;
    s.sheetStartFrame = start;
    s.sheetCycles = std::max(0.0f, number(*uv, "cycles", 1));
    if (flag(*uv, "flipU", false) || flag(*uv, "flipV", false)) notes.approximate("Espelhar U/V da folha de textura não existe na Astra.");
  }

  // --- Movimento --------------------------------------------------------------
  if (const auto *velocity = system.get("VelocityModule"); velocity) {
    s.velocityOverLifetimeEnabled = enabled(velocity);
    s.velocityX = minMax(velocity->get("x"), 1, s.velocityX);
    s.velocityY = minMax(velocity->get("y"), 1, s.velocityY);
    s.velocityZ = minMax(velocity->get("z"), -1, s.velocityZ);
    s.velocitySpace = flag(*velocity, "inWorldSpace", false) ? scene::ParticleSimulationSpace::World : scene::ParticleSimulationSpace::Local;
    s.speedModifier = minMax(velocity->get("speedModifier"), 1, s.speedModifier);
    // Órbita em radianos/s na Unity; velocidade angular é pseudovetor: na
    // reflexão em Z, X e Y trocam de sinal e Z fica.
    s.velocityOrbitalX = minMax(velocity->get("orbitalX"), -kDegreesPerRadian, s.velocityOrbitalX);
    s.velocityOrbitalY = minMax(velocity->get("orbitalY"), -kDegreesPerRadian, s.velocityOrbitalY);
    s.velocityOrbitalZ = minMax(velocity->get("orbitalZ"), kDegreesPerRadian, s.velocityOrbitalZ);
    s.velocityRadial = minMax(velocity->get("radial"), 1, s.velocityRadial);
    const auto offset = [&](const char *key) {
      return constantOf(minMax(velocity->get(key), 1, scene::particleCurveDefault(0)), notes, "Centro da órbita");
    };
    s.velocityOrbitalOffset[0] = offset("orbitalOffsetX");
    s.velocityOrbitalOffset[1] = offset("orbitalOffsetY");
    s.velocityOrbitalOffset[2] = -offset("orbitalOffsetZ");
  }
  if (const auto *inherit = system.get("InheritVelocityModule"); enabled(inherit)) {
    s.inheritVelocityEnabled = true;
    auto value = minMax(inherit->get("m_Curve"), 1, scene::particleCurveDefault(1));
    s.inheritVelocityMultiplier = constantOf(value, notes, "Herdar velocidade");
    if (integer(*inherit, "m_Mode", 0) == 1) notes.approximate("Herdar velocidade no modo Atual vira Inicial.");
  }
  if (const auto *force = system.get("ForceModule"); force) {
    s.forceOverLifetimeEnabled = enabled(force);
    s.forceX = minMax(force->get("x"), 1, s.forceX);
    s.forceY = minMax(force->get("y"), 1, s.forceY);
    s.forceZ = minMax(force->get("z"), -1, s.forceZ);
    s.forceSpace = flag(*force, "inWorldSpace", false) ? scene::ParticleSimulationSpace::World : scene::ParticleSimulationSpace::Local;
    if (s.forceOverLifetimeEnabled && flag(*force, "randomizePerFrame", false)) notes.approximate("Força sorteada a cada quadro vira sorteio por partícula.");
  }
  if (const auto *clamp = system.get("ClampVelocityModule"); clamp) {
    s.limitVelocityEnabled = enabled(clamp);
    s.limitSpeed = minMax(clamp->get("magnitude"), 1, s.limitSpeed);
    s.limitDampen = clamp01(number(*clamp, "dampen", 1));
    auto drag = minMax(clamp->get("drag"), 1, scene::particleCurveDefault(0));
    s.limitDrag = std::max(0.0f, constantOf(drag, notes, "Arrasto do limite de velocidade"));
    if (s.limitVelocityEnabled && flag(*clamp, "separateAxis", false))
      notes.approximate("Limite de velocidade por eixo vira limite da velocidade total.");
  }
  if (const auto *noise = system.get("NoiseModule"); noise) {
    s.noiseEnabled = enabled(noise);
    s.noiseStrength = minMax(noise->get("strength"), 1, s.noiseStrength);
    s.noiseFrequency = std::max(0.01f, number(*noise, "frequency", .5f));
    s.noiseOctaves = std::clamp(std::floor(number(*noise, "octaves", 1)), 1.0f, 4.0f);
    auto scroll = minMax(noise->get("scrollSpeed"), 1, scene::particleCurveDefault(0));
    s.noiseScrollSpeed = constantOf(scroll, notes, "Rolagem do ruído");
    if (s.noiseEnabled && flag(*noise, "separateAxes", false)) notes.approximate("Ruído com força por eixo usa a força de X nos três.");
    if (s.noiseEnabled && flag(*noise, "remapEnabled", false)) notes.approximate("Remapear o ruído não existe na Astra.");
  }
  if (const auto *bySpeed = system.get("SizeBySpeedModule"); bySpeed) {
    s.sizeBySpeedEnabled = enabled(bySpeed);
    s.sizeBySpeed = minMax(bySpeed->get("curve"), 1, s.sizeBySpeed);
    float range[3]{0, 1, 0};
    const float fallback[3]{0, 1, 0};
    vector3(*bySpeed, "range", range, fallback);
    s.sizeBySpeedRange[0] = range[0];
    s.sizeBySpeedRange[1] = range[1];
  }
  if (const auto *bySpeed = system.get("ColorBySpeedModule"); bySpeed) {
    s.colorBySpeedEnabled = enabled(bySpeed);
    s.colorBySpeed = lifetimeGradient(minMaxGradient(bySpeed->get("gradient"), s.colorBySpeed), notes);
    float range[3]{0, 1, 0};
    const float fallback[3]{0, 1, 0};
    vector3(*bySpeed, "range", range, fallback);
    s.colorBySpeedRange[0] = range[0];
    s.colorBySpeedRange[1] = range[1];
  }
  if (const auto *bySpeed = system.get("RotationBySpeedModule"); bySpeed) {
    s.rotationBySpeedEnabled = enabled(bySpeed);
    s.rotationBySpeed = minMax(bySpeed->get("curve"), (viewBillboard ? -1.0f : 1.0f) * kDegreesPerRadian, s.rotationBySpeed);
    float range[3]{0, 1, 0};
    const float fallback[3]{0, 1, 0};
    vector3(*bySpeed, "range", range, fallback);
    s.rotationBySpeedRange[0] = range[0];
    s.rotationBySpeedRange[1] = range[1];
  }
  if (const auto *external = system.get("ExternalForcesModule"); enabled(external))
    notes.approximate("Forças externas (Wind Zone) não existem na cena da Astra.");

  // --- Colisão ----------------------------------------------------------------
  if (const auto *collision = system.get("CollisionModule"); collision) {
    s.collisionEnabled = enabled(collision);
    s.collisionType = integer(*collision, "type", 0) == 1 ? scene::ParticleCollisionType::World : scene::ParticleCollisionType::Planes;
    u32 planes = 0;
    for (u32 i = 0; i < 6; ++i) {
      const i64 plane = localReference(collision->get("plane" + std::to_string(i)));
      if (!plane) continue;
      if (planes < 2) result.collisionPlanes[planes] = plane;
      ++planes;
    }
    if (s.collisionEnabled && planes > 2) notes.loss("Mais de dois planos de colisão: os seguintes são ignorados.");
    s.collisionDampen = clamp01(constantOf(minMax(collision->get("m_Dampen"), 1, scene::particleCurveDefault(0)), notes, "Amortecimento"));
    s.collisionBounce = std::clamp(constantOf(minMax(collision->get("m_Bounce"), 1, scene::particleCurveDefault(1)), notes, "Quique"), 0.0f, 2.0f);
    s.collisionLifetimeLoss = clamp01(constantOf(minMax(collision->get("m_EnergyLossOnCollision"), 1, scene::particleCurveDefault(0)), notes, "Perda de vida"));
    s.collisionMinKillSpeed = std::max(0.0f, number(*collision, "minKillSpeed", 0));
    s.collisionRadiusScale = std::clamp(number(*collision, "radiusScale", 1), 0.0f, 100.0f);
    if (s.collisionEnabled && integer(*collision, "collisionMode", 0) == 1)
      notes.approximate("Colisão 2D consulta a física 3D da cena.");
  }
  if (const auto *trigger = system.get("TriggerModule"); enabled(trigger))
    notes.approximate("Gatilhos de partícula (Triggers) não existem na Astra.");

  // --- Sub-emissores ------------------------------------------------------------
  if (const auto *sub = system.get("SubModule"); sub) {
    s.subEmittersEnabled = enabled(sub);
    std::set<i64> taken;
    const auto assign = [&](i64 type, i64 emitter) {
      if (!emitter) return;
      i64 *slot = type == 0 ? &result.subEmitterBirth : type == 1 ? &result.subEmitterCollision : type == 2 ? &result.subEmitterDeath : nullptr;
      if (!slot) { notes.loss("Sub-emissor por gatilho ou manual sem consumidor."); return; }
      if (*slot) {
        // Mais um do mesmo acontecimento: vai para os extras.
        for (auto &extra : result.extraSubEmitters)
          if (!extra.first) { extra = {emitter, static_cast<u32>(type)}; return; }
        notes.loss("Mais de quatro sub-emissores no sistema: os seguintes são ignorados.");
        return;
      }
      *slot = emitter;
    };
    if (const auto *list = sub->get("subEmitters"); list && list->kind == Node::Kind::Sequence) {
      for (const auto &entry : list->elements) {
        assign(integer(entry, "type", 0), localReference(entry.get("emitter")));
        if (s.subEmittersEnabled && integer(entry, "properties", 0) != 0)
          notes.approximate("Sub-emissor não herda cor, tamanho, giro nem vida da partícula.");
      }
    } else if (const auto *single = sub->get("subEmitters"); single && single->kind == Node::Kind::Map) {
      assign(integer(*single, "type", 0), localReference(single->get("emitter")));
    } else {
      assign(0, localReference(sub->get("subEmitterBirth")));
      assign(1, localReference(sub->get("subEmitterCollision")));
      assign(2, localReference(sub->get("subEmitterDeath")));
    }
  }

  // --- Luzes ----------------------------------------------------------------------
  if (const auto *lights = system.get("LightsModule"); lights) {
    s.lightsEnabled = enabled(lights);
    s.lightRatio = clamp01(number(*lights, "ratio", 0));
    UnityObjectReference light;
    if (const auto *node = lights->get("light"); node && unityObjectReference(*node, light)) {
      if (light.external() && s.lightsEnabled) notes.loss("Luz modelo de outro prefab: as partículas não acendem luzes.");
      else result.lightTemplate = light.fileId;
    }
    s.lightUseParticleColor = flag(*lights, "color", true);
    s.lightSizeAffectsRange = flag(*lights, "range", true);
    s.lightAlphaAffectsIntensity = flag(*lights, "intensity", true);
    s.lightRangeMultiplier = std::max(0.0f, constantOf(minMax(lights->get("rangeCurve"), 1, scene::particleCurveDefault(1)), notes, "Alcance das luzes"));
    s.lightIntensityMultiplier = std::max(0.0f, constantOf(minMax(lights->get("intensityCurve"), 1, scene::particleCurveDefault(1)), notes, "Intensidade das luzes"));
    s.lightMaxLights = std::clamp(std::floor(number(*lights, "maxLights", 20)), 0.0f, 64.0f);
  }

  // --- Trilhas --------------------------------------------------------------------
  if (const auto *trail = system.get("TrailModule"); trail) {
    s.trailsEnabled = enabled(trail);
    if (s.trailsEnabled && integer(*trail, "mode", 0) == 1) notes.loss("Trilha no modo Ribbon (fita entre partículas) vira trilha por partícula.");
    s.trailRatio = clamp01(number(*trail, "ratio", 1));
    s.trailLifetime = clamp01(constantOf(minMax(trail->get("lifetime"), 1, scene::particleCurveDefault(1)), notes, "Vida da trilha"));
    s.trailMinVertexDistance = std::max(0.0f, number(*trail, "minVertexDistance", .2f));
    s.trailInheritColor = flag(*trail, "inheritParticleColor", true);
    if (const auto *width = trail->get("widthOverTrail")) {
      const auto value = minMax(width, 1, scene::particleCurveDefault(1));
      if (value.usesCurves() && value.curveMax.keys.size() >= 2) {
        s.trailWidth = std::max(0.0f, value.curveMax.keys.front().value * value.multiplier);
        s.trailWidthEnd = std::max(0.0f, value.curveMax.keys.back().value * value.multiplier);
      } else {
        s.trailWidth = s.trailWidthEnd = std::max(0.0f, constantOf(value, notes, "Largura da trilha"));
      }
    }
    if (const auto *fade = trail->get("colorOverTrail")) {
      const auto value = minMaxGradient(fade, {});
      const auto &g = value.gradientMax;
      s.trailFade = (value.mode == ParticleGradientMode::Gradient || value.mode == ParticleGradientMode::TwoGradients) &&
                    !g.alphas.empty() && g.alphas.back().alpha < g.alphas.front().alpha;
    }
    if (s.trailsEnabled && !flag(*trail, "dieWithParticles", true)) notes.approximate("Trilha que sobrevive à partícula some com ela.");
    if (s.trailsEnabled && !flag(*trail, "sizeAffectsWidth", true)) notes.approximate("Largura da trilha sempre acompanha o tamanho da partícula.");
    if (s.trailsEnabled && integer(*trail, "textureMode", 0) != 0) notes.approximate("Textura da trilha em modo Tile/Distribuir vira Esticar.");
  }
  if (const auto *custom = system.get("CustomDataModule"); enabled(custom))
    notes.approximate("Dados personalizados (Custom Data) que o shader do pacote lia não chegam ao material.");

  // --- Renderer -------------------------------------------------------------------------
  if (renderer) {
    result.rendererEnabled = flag(*renderer, "m_Enabled", true);
    switch (renderMode) {
    case 0: s.renderMode = scene::ParticleRenderMode::Billboard; break;
    case 1: s.renderMode = scene::ParticleRenderMode::StretchedBillboard; break;
    case 2: case 3:
      s.renderMode = scene::ParticleRenderMode::Billboard;
      notes.approximate("Billboard horizontal/vertical vira billboard na Vista.");
      break;
    case 4: s.renderMode = scene::ParticleRenderMode::Mesh; break;
    default: s.renderMode = scene::ParticleRenderMode::None; break;
    }
    if (!result.rendererEnabled) s.renderMode = scene::ParticleRenderMode::None;
    s.renderAlignment = alignment == 1 ? scene::ParticleRenderAlignment::World : alignment == 2 ? scene::ParticleRenderAlignment::Local
                        : alignment == 4 ? scene::ParticleRenderAlignment::Velocity : scene::ParticleRenderAlignment::View;
    if (alignment == 3) notes.approximate("Alinhamento Facing vira Vista.");
    const i64 sort = integer(*renderer, "m_SortMode", 0);
    s.sortMode = sort == 2 ? scene::ParticleSortMode::OldestInFront : sort == 3 ? scene::ParticleSortMode::YoungestInFront
                 : sort == 0 ? scene::ParticleSortMode::None : scene::ParticleSortMode::ByDistance;
    s.sortingFudge = std::clamp(number(*renderer, "m_SortingFudge", 0), -1000.0f, 1000.0f);
    s.maxParticleSize = std::clamp(number(*renderer, "m_MaxParticleSize", .5f), 0.0f, 10.0f);
    s.stretchVelocityScale = std::clamp(number(*renderer, "m_VelocityScale", 0), 0.0f, 100.0f);
    s.stretchLengthScale = std::clamp(number(*renderer, "m_LengthScale", 2), 0.0f, 100.0f);
    if (number(*renderer, "m_CameraVelocityScale", 0) != 0) notes.approximate("Esticar pela velocidade da câmera não existe na Astra.");
    {
      const float zero[3]{0, 0, 0};
      float pivot[3];
      vector3(*renderer, "m_Pivot", pivot, zero);
      if (pivot[0] != 0 || pivot[1] != 0 || pivot[2] != 0) notes.approximate("Pivô da partícula fica no centro.");
    }
    if (const auto *materials = renderer->get("m_Materials"); materials && materials->kind == Node::Kind::Sequence) {
      if (!materials->elements.empty()) unityObjectReference(materials->elements[0], result.material);
      if (materials->elements.size() > 1) unityObjectReference(materials->elements[1], result.trailMaterial);
    }
    if (s.renderMode == scene::ParticleRenderMode::Mesh) {
      if (const auto *mesh = renderer->get("m_Mesh")) unityObjectReference(*mesh, result.mesh);
      bool approximate = false;
      if (unityBuiltinParticleMesh(result.mesh, s.meshShape, approximate)) {
        if (approximate) notes.approximate("Malha embutida Capsule/Plane da Unity vira Cilindro/Quadrado.");
        result.mesh = {};
      } else if (!result.mesh.external() || !result.mesh.fileId) {
        notes.loss("Render Mode Malha sem malha: o sistema não desenha.");
        s.renderMode = scene::ParticleRenderMode::None;
        result.mesh = {};
      }
      for (const char *extra : {"m_Mesh1", "m_Mesh2", "m_Mesh3"}) {
        UnityObjectReference other;
        if (const auto *node = renderer->get(extra); node && unityObjectReference(*node, other) && other.fileId) {
          notes.approximate("Várias malhas por sistema (Mesh Distribution) desenham só a primeira.");
          break;
        }
      }
    }
    if (result.trailMaterial.fileId && s.trailsEnabled && !(result.trailMaterial == result.material))
      notes.approximate("Material próprio da trilha vira o do sistema.");
  } else {
    s.renderMode = scene::ParticleRenderMode::None;
    result.rendererEnabled = false;
  }

  // Domínio de cada número refletido: o que veio fora vira o limite, com aviso.
  scene::ParticleSystem probe;
  static_cast<scene::ParticleSystemSettings &>(probe) = s;
  bool clamped = false;
  for (const auto &p : scene::ParticleSystem::descriptor.numbers) {
    float *value = p.write(probe);
    if (!std::isfinite(*value)) { *value = 0; clamped = true; }
    if (*value < p.minimum || *value > p.maximum) { *value = std::clamp(*value, p.minimum, p.maximum); clamped = true; }
  }
  if (clamped) notes.approximate("Valores fora do domínio da Astra foram levados ao limite mais próximo.");
  if (!probe.valid()) { diagnostic = "Sistema de partículas convertido fora do contrato nativo."; return false; }
  result.settings = static_cast<const scene::ParticleSystemSettings &>(probe);
  out = std::move(result);
  diagnostic.clear();
  return true;
}

} // namespace ae::resources

// G6a/G6b: partícula de malha e alinhamentos, colisão Mundo, Linha, a
// conversão do Particle System/prefab/material da Unity e os perfis do
// UniStorm. Fixtures sintéticas e pequenas, escritas aqui: nenhum arquivo dos
// pacotes da Asset Store entra no repositório.
#include "harness.h"

#include "renderer/particles/particle_gpu.h"
#include "renderer/particles/particle_simulation.h"
#include "resources/unistorm_import.h"
#include "resources/unity_material_import.h"
#include "resources/unity_particle_import.h"
#include "resources/unity_prefab_import.h"
#include "runtime/scene_lines.h"
#include "runtime/scene_particles.h"
#include "runtime/scene_vfx.h"
#include "runtime/transform_math.h"
#include "runtime/unity_prefab.h"
#include "scene/audio.h"
#include "scene/component_schema.h"
#include "scene/light.h"
#include "scene/particle_system.h"
#include "scene/vfx_behaviors.h"

#include <cmath>
#include <locale>
#include <sstream>

using namespace ae;
using renderer::particles::ParticleEmitter;
using renderer::particles::ParticleInstance;

namespace {
constexpr float kIdentity[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
const float kCamera[3]{0, 0, -10};

template <class T> bool roundTrip(const T &source, T &out) {
  std::ostringstream text;
  text.imbue(std::locale::classic());
  text << std::setprecision(std::numeric_limits<float>::max_digits10);
  source.write(text);
  std::istringstream in(text.str());
  in.imbue(std::locale::classic());
  return out.read(in, T::descriptor.version);
}
// Curva/gradiente mínimos no formato serializado da Unity.
std::string minMax(std::string_view name, int state, float scalar, float minScalar, int indent) {
  const std::string pad(static_cast<usize>(indent), ' ');
  std::ostringstream out;
  out << pad << name << ":\n" << pad << "  serializedVersion: 2\n" << pad << "  minMaxState: " << state << "\n"
      << pad << "  scalar: " << scalar << "\n" << pad << "  minScalar: " << minScalar << "\n"
      << pad << "  maxCurve:\n" << pad << "    serializedVersion: 2\n" << pad << "    m_Curve:\n"
      << pad << "    - serializedVersion: 3\n" << pad << "      time: 0\n" << pad << "      value: 1\n"
      << pad << "      inSlope: 0\n" << pad << "      outSlope: -1\n"
      << pad << "    - serializedVersion: 3\n" << pad << "      time: 1\n" << pad << "      value: 0\n"
      << pad << "      inSlope: -1\n" << pad << "      outSlope: 0\n"
      << pad << "  minCurve:\n" << pad << "    serializedVersion: 2\n" << pad << "    m_Curve: []\n";
  return out.str();
}
std::string particleYaml(int shapeType, int renderMode, int alignment, std::string_view material) {
  std::ostringstream out;
  out << "%YAML 1.1\n%TAG !u! tag:unity3d.com,2011:\n"
      << "--- !u!1 &10\nGameObject:\n  m_Component:\n  - component: {fileID: 20}\n  - component: {fileID: 30}\n  - component: {fileID: 40}\n"
      << "  m_Layer: 0\n  m_Name: Fogo\n  m_TagString: Untagged\n  m_IsActive: 1\n"
      << "--- !u!4 &20\nTransform:\n  m_GameObject: {fileID: 10}\n  m_LocalRotation: {x: 0, y: 0, z: 0, w: 1}\n"
      << "  m_LocalPosition: {x: 0, y: 0, z: 2}\n  m_LocalScale: {x: 1, y: 1, z: 1}\n  m_Children: []\n  m_Father: {fileID: 0}\n"
      << "--- !u!198 &30\nParticleSystem:\n  m_GameObject: {fileID: 10}\n  lengthInSec: 2\n  simulationSpeed: 1\n  stopAction: 0\n"
      << "  looping: 0\n  prewarm: 0\n  playOnAwake: 1\n  autoRandomSeed: 0\n  randomSeed: 7\n  moveWithTransform: 0\n  scalingMode: 0\n"
      << "  InitialModule:\n    enabled: 1\n"
      << minMax("startLifetime", 0, 2, 2, 4) << minMax("startSpeed", 0, 5, 5, 4) << minMax("startSize", 3, .5f, .25f, 4)
      << minMax("startRotation", 0, 1.5707964f, 0, 4)
      << "    startColor:\n      minMaxState: 0\n      minColor: {r: 1, g: 1, b: 1, a: 1}\n      maxColor: {r: 0.5, g: 1, b: 0, a: 0.5}\n"
      << "    maxNumParticles: 50\n    size3D: 0\n    rotation3D: 0\n"
      << "  ShapeModule:\n    enabled: 1\n    type: " << shapeType << "\n    angle: 20\n    length: 5\n    radiusThickness: 1\n"
      << "    m_Position: {x: 0, y: 0, z: 0}\n    m_Rotation: {x: 0, y: 0, z: 0}\n    m_Scale: {x: 1, y: 1, z: 1}\n"
      << "    radius:\n      value: 0.5\n      mode: 0\n    arc:\n      value: 360\n      mode: 0\n"
      << "  EmissionModule:\n    enabled: 1\n" << minMax("rateOverTime", 0, 10, 10, 4)
      << "    m_BurstCount: 1\n    m_Bursts:\n    - serializedVersion: 2\n      time: 0\n" << minMax("countCurve", 3, 30, 20, 6)
      << "      cycleCount: 1\n      repeatInterval: 0.01\n      probability: 1\n"
      << "  ColorModule:\n    enabled: 1\n    gradient:\n      minMaxState: 1\n      maxGradient:\n        serializedVersion: 2\n"
      << "        key0: {r: 1, g: 0.5, b: 0, a: 1}\n        key1: {r: 1, g: 1, b: 1, a: 0}\n        ctime0: 0\n        ctime1: 65535\n"
      << "        atime0: 0\n        atime1: 65535\n        m_Mode: 0\n        m_NumColorKeys: 2\n        m_NumAlphaKeys: 2\n"
      << "  CustomDataModule:\n    enabled: 1\n"
      << "--- !u!199 &40\nParticleSystemRenderer:\n  m_GameObject: {fileID: 10}\n  m_Enabled: 1\n  m_Materials:\n  - " << material << "\n"
      << "  m_RenderMode: " << renderMode << "\n  m_RenderAlignment: " << alignment << "\n  m_SortMode: 1\n  m_MaxParticleSize: 0.5\n"
      << "  m_VelocityScale: 0\n  m_LengthScale: 2\n  m_Mesh: {fileID: 10210, guid: 0000000000000000e000000000000000, type: 0}\n";
  return out.str();
}
} // namespace

AE_TEST(g6a_particle_archive_v4_keeps_mesh_alignment_and_axes) {
  scene::ParticleSystem source;
  source.renderMode = scene::ParticleRenderMode::Mesh;
  source.renderAlignment = scene::ParticleRenderAlignment::Local;
  source.meshShape = scene::ParticleMeshShape::Sphere;
  source.meshAsset = resources::assetGuidFromSeed("malha:anel");
  source.size3D = true;
  source.startSizeY = scene::particleCurveDefault(2);
  source.rotation3D = true;
  source.startRotationX = scene::particleCurveDefault(30);
  source.scalingMode = scene::ParticleScalingMode::Hierarchy;
  source.collisionEnabled = true;
  source.collisionType = scene::ParticleCollisionType::World;
  scene::ParticleSystem back;
  AE_EXPECT_TRUE(roundTrip(source, back), "ida e volta v4");
  AE_EXPECT_TRUE(back.renderMode == scene::ParticleRenderMode::Mesh && back.renderAlignment == scene::ParticleRenderAlignment::Local &&
                 back.meshShape == scene::ParticleMeshShape::Sphere && back.meshAsset == source.meshAsset && back.size3D &&
                 back.startSizeY.constantMax == 2 && back.rotation3D && back.startRotationX.constantMax == 30 &&
                 back.scalingMode == scene::ParticleScalingMode::Hierarchy && back.collisionType == scene::ParticleCollisionType::World,
                 "malha, alinhamento, eixos, escala e colisão preservados");
  // Arquivo v3 não tem o token da malha no fim e abre com o comportamento antigo.
  std::istringstream v3("1 duration 2 0 0 -");
  scene::ParticleSystem old;
  AE_EXPECT_TRUE(old.read(v3, 3) && !old.meshAsset.valid() && old.scalingMode == scene::ParticleScalingMode::Shape &&
                 old.renderAlignment == scene::ParticleRenderAlignment::View, "v3 abre igual (Forma, Vista)");
  AE_EXPECT_TRUE(scene::ParticleSystem::curveSlots().size() == 66, "24 curvas novas endereçáveis");
}

AE_TEST(g6a_orbital_and_radial_velocity_turn_around_the_offset_center) {
  scene::ParticleSystemSettings s;
  s.startLifetime = scene::particleCurveDefault(10);
  s.startSpeed = scene::particleCurveDefault(0);
  s.emissionRate = scene::particleCurveDefault(0);
  s.shapeEnabled = false;  // nasce na origem do objeto
  s.velocityOverLifetimeEnabled = true;
  s.velocityOrbitalOffset[0] = -1;
  s.velocityOrbitalZ = scene::particleCurveDefault(90);
  ParticleEmitter emitter;
  emitter.configure(s, 1);
  emitter.play();
  emitter.emit(1, kIdentity);
  for (u32 frame = 0; frame < 30; ++frame) emitter.simulate(1.0f / 30, kIdentity);
  std::vector<ParticleInstance> out;
  emitter.build(kIdentity, kCamera, out);
  AE_EXPECT_TRUE(out.size() == 1, "uma partícula");
  // Um segundo a 90°/s em volta de (-1, 0, 0): sai de (0, 0, 0) e chega a (-1, 1, 0).
  AE_EXPECT_TRUE(std::abs(out[0].position[0] + 1) < 0.05f && std::abs(out[0].position[1] - 1) < 0.05f, "um quarto de volta");
  // Radial empurra para fora do centro sem girar.
  s.velocityOrbitalZ = scene::particleCurveDefault(0);
  s.velocityRadial = scene::particleCurveDefault(1);
  ParticleEmitter pushed;
  pushed.configure(s, 1);
  pushed.play();
  pushed.emit(1, kIdentity);
  for (u32 frame = 0; frame < 30; ++frame) pushed.simulate(1.0f / 30, kIdentity);
  out.clear();
  pushed.build(kIdentity, kCamera, out);
  AE_EXPECT_TRUE(out.size() == 1 && std::abs(out[0].position[0] - 1) < 0.05f && std::abs(out[0].position[1]) < 0.01f,
                 "raio de 1 m para 2 m");
  // Sub-emissores extras ouvem o acontecimento que declaram.
  s.subEmittersEnabled = true;
  s.subEmitterExtra[1] = 7;
  s.subEmitterExtraType[1] = 2;
  emitter.configure(s, 1);
  AE_EXPECT_TRUE(emitter.wantsEvents(2) && !emitter.wantsEvents(0) && !emitter.wantsEvents(1), "extra de morte");
}

AE_TEST(g6a_mesh_particles_carry_orientation_and_three_sizes) {
  scene::ParticleSystemSettings s;
  s.startLifetime = scene::particleCurveDefault(5);
  s.startSpeed = scene::particleCurveDefault(0);
  s.emissionRate = scene::particleCurveDefault(0);
  s.renderMode = scene::ParticleRenderMode::Mesh;
  s.renderAlignment = scene::ParticleRenderAlignment::Local;
  s.size3D = true;
  s.startSize = scene::particleCurveDefault(1);
  s.startSizeY = scene::particleCurveDefault(2);
  s.startSizeZ = scene::particleCurveDefault(3);
  ParticleEmitter emitter;
  emitter.configure(s, 1);
  // Objeto girado 90° em Y: a orientação Local é a rotação do objeto.
  const float turned[16]{0, 0, -1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1};
  emitter.emit(1, turned);
  std::vector<ParticleInstance> out;
  emitter.build(turned, kCamera, out);
  AE_EXPECT_TRUE(out.size() == 1, "uma partícula");
  const auto &p = out.front();
  AE_EXPECT_TRUE(p.size == 1 && p.sizeYZ[0] == 2 && p.sizeYZ[1] == 3, "largura, altura e profundidade");
  const float y = static_cast<float>(p.orientation[1]) / 32767.0f, w = static_cast<float>(p.orientation[3]) / 32767.0f;
  AE_EXPECT_TRUE(std::abs(std::abs(y) - 0.7071f) < 0.01f && std::abs(std::abs(w) - 0.7071f) < 0.01f, "quatérnio de 90° em Y");
  AE_EXPECT_TRUE(emitter.material().oriented && emitter.material().alignment == 2, "lote orientado, alinhamento Local");
  // Nenhum: simula, não desenha.
  s.renderMode = scene::ParticleRenderMode::None;
  emitter.configure(s, 1);
  out.clear();
  emitter.build(turned, kCamera, out);
  AE_EXPECT_TRUE(out.empty() && emitter.alive() == 1, "modo Nenhum simula sem instâncias");
  // Malhas embutidas: Quadrado, Cubo, Esfera, Cilindro.
  renderer::particles::ParticleMeshSet set;
  renderer::particles::appendBuiltinParticleMeshes(set);
  AE_EXPECT_TRUE(set.ranges.size() == 4 && set.ranges[0].indexCount == 6 && set.ranges[1].indexCount == 36, "quatro malhas embutidas");
}

namespace {
bool groundAtZero(void *context, const float (&from)[3], const float (&to)[3], float radius, float (&point)[3], float (&normal)[3]) {
  ++*static_cast<int *>(context);
  if (to[1] - radius > 0 || from[1] < 0) return false;
  const float t = from[1] == to[1] ? 0.0f : from[1] / (from[1] - to[1]);
  for (u32 k = 0; k < 3; ++k) point[k] = from[k] + (to[k] - from[k]) * t;
  normal[0] = 0; normal[1] = 1; normal[2] = 0;
  return true;
}
} // namespace

AE_TEST(g6a_world_collision_bounces_on_physics_and_reports_touches) {
  scene::ParticleSystemSettings s;
  s.simulationSpace = scene::ParticleSimulationSpace::World;
  s.startLifetime = scene::particleCurveDefault(10);
  s.startSpeed = scene::particleCurveDefault(0);
  s.startSize = scene::particleCurveDefault(.1f);
  s.emissionRate = scene::particleCurveDefault(0);
  s.gravityModifier = scene::particleCurveDefault(1);
  s.collisionEnabled = true;
  s.collisionType = scene::ParticleCollisionType::World;
  s.collisionBounce = 0;
  ParticleEmitter emitter;
  emitter.configure(s, 3);
  int queries = 0;
  emitter.setWorldCollider(&groundAtZero, &queries, 64);
  emitter.setCollisionEventsWanted(true);
  const float lifted[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 2, 0, 1};
  emitter.play();
  emitter.emit(4, lifted);
  usize touches = 0;
  for (u32 frame = 0; frame < 90; ++frame) {
    emitter.clearEvents();
    emitter.simulate(1.0f / 30, lifted);
    touches += emitter.collisions().size();
  }
  std::vector<ParticleInstance> out;
  emitter.build(lifted, kCamera, out);
  bool above = !out.empty();
  for (const auto &p : out) above = above && p.position[1] > -0.01f;
  AE_EXPECT_TRUE(above, "as partículas param no chão em vez de atravessar");
  AE_EXPECT_TRUE(touches >= 4 && queries > 0, "toques entregues ao ouvinte");
  // Sem consulta (prévia do editor) o tipo Mundo não colide.
  ParticleEmitter preview;
  preview.configure(s, 3);
  preview.play();
  preview.emit(1, lifted);
  for (u32 frame = 0; frame < 90; ++frame) preview.simulate(1.0f / 30, lifted);
  out.clear();
  preview.build(lifted, kCamera, out);
  AE_EXPECT_TRUE(out.size() == 1 && out.front().position[1] < -1, "sem física, atravessa");
}

AE_TEST(g6a_line_renderer_draws_stretched_segments_through_the_particle_pass) {
  const auto *schema = scene::findComponentSchema("astra.render.line");
  AE_EXPECT_TRUE(schema && scene::findComponentSchema("astra.effects.laser") && scene::findComponentSchema("astra.effects.projectile") &&
                 scene::findComponentSchema("astra.effects.collision_spawn") && scene::findComponentSchema("astra.audio.effect_sound"),
                 "cinco componentes de efeito registrados");
  for (const auto *id : {"render.lines", "render.effects.laser", "render.effects.projectile", "render.effects.collision_spawn", "audio.effect_sound"})
    AE_EXPECT_TRUE(core::engineCapabilityAuthorable(id), id);
  runtime::SceneGraph graph;
  const auto beam = graph.createEntity(graph.root(), runtime::ObjectKind::Effect, "Raio");
  auto *components = graph.editComponents(beam);
  auto *line = static_cast<scene::LineRenderer *>(components->add(scene::LineRenderer::descriptor));
  line->worldSpace = false;
  line->points[3] = 0; line->points[4] = 0; line->points[5] = 3;
  line->widthStart = .2f; line->widthEnd = .2f;
  renderer::particles::ParticleFrame frame;
  AE_EXPECT_TRUE(runtime::appendLineRenderers(graph, kCamera, frame) == 1 && frame.instances.size() == 2, "3 m em dois pedaços");
  AE_EXPECT_TRUE(std::abs(frame.instances[0].velocity[2] - 1.5f) < 1e-4f && frame.batches[0].material.lineUv &&
                 std::abs(frame.instances[1].rotation - .5f) < 1e-4f, "Esticar: U de 0 a 1 pela linha inteira");
  line->tileLength = .5f;
  frame.clear();
  runtime::appendLineRenderers(graph, kCamera, frame);
  // 3 m em pedaços de até 2 m (dois de 1,5 m); a imagem repete pelo U contínuo
  // (0 a 3, depois 3 a 6), sem espremer nem emendar.
  AE_EXPECT_TRUE(frame.instances.size() == 2 && std::abs(frame.instances[0].frame - 3) < 1e-4f && frame.instances[0].rotation == 0 &&
                 std::abs(frame.instances[1].rotation - 3) < 1e-4f, "Repetir a cada 0,5 m: U de 0 a 6 contínuo");
  // Segundo trecho (2 m): o U continua de onde o primeiro parou.
  line->pointCount = 3;
  line->points[6] = 2; line->points[7] = 0; line->points[8] = 3;
  frame.clear();
  runtime::appendLineRenderers(graph, kCamera, frame);
  AE_EXPECT_TRUE(frame.instances.size() == 3 && std::abs(frame.instances[2].rotation - 6) < 1e-4f &&
                 std::abs(frame.instances[2].frame - 4) < 1e-4f, "U contínuo entre os trechos");
  line->pointCount = 2;
  scene::LineRenderer back;
  AE_EXPECT_TRUE(roundTrip(*line, back) && back.tileLength == .5f && back.points[5] == 3, "Linha ida e volta");
  // v2 acrescentou o ruído no fim das tabelas: um arquivo v1 (39 números, 3
  // booleanos) abre com o ruído desligado.
  {
    line->noise = true;
    std::ostringstream text;
    text.imbue(std::locale::classic());
    line->write(text);
    std::istringstream all(text.str());
    std::vector<std::string> tokens;
    for (std::string token; all >> token;) tokens.push_back(token);
    std::vector<std::string> v1{"39"};
    v1.insert(v1.end(), tokens.begin() + 1, tokens.begin() + 40);
    v1.push_back("3");
    v1.insert(v1.end(), tokens.begin() + 45, tokens.begin() + 48);
    v1.insert(v1.end(), tokens.begin() + 49, tokens.end());
    std::string joined;
    for (const auto &token : v1) joined += token + " ";
    std::istringstream in(joined);
    in.imbue(std::locale::classic());
    scene::LineRenderer old;
    AE_EXPECT_TRUE(tokens[0] == "43" && tokens[44] == "4" && old.read(in, 1) && !old.noise && old.tileLength == .5f,
                   "Linha v1 abre sem o ruído");
    line->noise = false;
  }
  scene::Projectile projectile, projectileBack;
  projectile.speed = 30;
  projectile.impact = resources::assetGuidFromSeed("prefab:impacto");
  projectile.detach[1] = 7;
  projectile.forwardAxis = scene::VfxForwardAxis::NegativeZ;
  AE_EXPECT_TRUE(roundTrip(projectile, projectileBack) && projectileBack.speed == 30 && projectileBack.impact == projectile.impact &&
                 projectileBack.detach[1] == 7 && projectileBack.forwardAxis == scene::VfxForwardAxis::NegativeZ, "Projétil ida e volta");
}

AE_TEST(g6a_mesh_shape_emits_from_the_triangles_along_their_normal) {
  // Um quadrado de 2 m no plano XY, normal +Z, escalado por 0,5.
  auto set = std::make_shared<renderer::particles::ParticleMeshSet>();
  const float positions[]{-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0};
  const float uvs[]{0, 0, 1, 0, 1, 1, 0, 1};
  const u32 indices[]{0, 1, 2, 0, 2, 3};
  u32 range = ~0u;
  AE_EXPECT_TRUE(renderer::particles::appendParticleMesh(*set, positions, uvs, indices, range), "malha da forma");
  scene::ParticleSystemSettings s;
  s.startLifetime = scene::particleCurveDefault(5);
  s.startSpeed = scene::particleCurveDefault(1);
  s.emissionRate = scene::particleCurveDefault(0);
  s.shapeType = scene::ParticleShapeType::Mesh;
  s.shapeRotation[0] = s.shapeRotation[1] = s.shapeRotation[2] = 0;
  s.shapeMeshScale = .5f;
  ParticleEmitter emitter;
  emitter.configure(s, 64);
  emitter.setShapeMesh(set, range);
  emitter.play();
  emitter.emit(64, kIdentity);
  std::vector<ParticleInstance> out;
  emitter.build(kIdentity, kCamera, out);
  bool inside = out.size() == 64;
  float spread = 0;
  for (const auto &p : out) {
    inside = inside && std::abs(p.position[0]) <= .5f + 1e-4f && std::abs(p.position[1]) <= .5f + 1e-4f && std::abs(p.position[2]) < 1e-4f &&
             p.velocity[2] > .99f;
    spread = std::max(spread, std::abs(p.position[0]));
  }
  AE_EXPECT_TRUE(inside && spread > .2f, "nascem sobre a malha escalada, para a normal");
  // Vértice: só os quatro cantos.
  s.shapeMeshPlacement = scene::ParticleShapeMeshPlacement::Vertex;
  ParticleEmitter corners_;
  corners_.configure(s, 32);
  corners_.setShapeMesh(set, range);
  corners_.play();
  corners_.emit(32, kIdentity);
  out.clear();
  corners_.build(kIdentity, kCamera, out);
  bool corners = !out.empty();
  for (const auto &p : out) corners = corners && std::abs(std::abs(p.position[0]) - .5f) < 1e-4f && std::abs(std::abs(p.position[1]) - .5f) < 1e-4f;
  AE_EXPECT_TRUE(corners, "Vértice nasce nos cantos");
  // Arquivo v5 guarda a Malha da forma.
  scene::ParticleSystem source;
  source.shapeType = scene::ParticleShapeType::Mesh;
  source.shapeMeshAsset = resources::assetGuidFromSeed("g6-shape-mesh");
  scene::ParticleSystem back;
  AE_EXPECT_TRUE(roundTrip(source, back) && back.shapeType == scene::ParticleShapeType::Mesh && back.shapeMeshAsset == source.shapeMeshAsset,
                 "forma Malha na ida e volta");
}

AE_TEST(g6a_rotator_and_eight_sub_emitters) {
  scene::Rotator rotator;
  rotator.speed[1] = 90;
  scene::Rotator back;
  AE_EXPECT_TRUE(roundTrip(rotator, back) && back.speed[1] == 90 && back.enabled, "Girar na ida e volta");
  scene::ParticleSystemSettings s;
  s.subEmittersEnabled = true;
  s.subEmitterBirth = 3;
  for (usize k = 0; k < scene::ParticleSystemSettings::kExtraSubEmitters; ++k) { s.subEmitterExtra[k] = 10 + k; s.subEmitterExtraType[k] = 0; }
  scene::ParticleSystem system;
  static_cast<scene::ParticleSystemSettings &>(system) = s;
  scene::ParticleSystem copy;
  AE_EXPECT_TRUE(scene::ParticleSystemSettings::kExtraSubEmitters == 7 && roundTrip(system, copy) && copy.subEmitterExtra[6] == 16,
                 "oito sub-emissores de nascimento");
}

AE_TEST(g6b_follow_camera_keeps_the_effect_over_the_game_camera) {
  scene::FollowCamera follow;
  follow.offset[1] = 28;
  follow.offset[2] = -1;
  scene::FollowCamera back;
  AE_EXPECT_TRUE(roundTrip(follow, back) && back.offset[1] == 28 && back.offset[2] == -1 && back.enabled, "Seguir câmera na ida e volta");
  // No Play, a chuva vai para a câmera + deslocamento; o giro do autor fica.
  runtime::SceneGraph graph;
  const auto parent = graph.createEntity(graph.root(), runtime::ObjectKind::Folder, "Pai");
  const auto rain = graph.createEntity(parent, runtime::ObjectKind::Folder, "Chuva");
  auto values = *graph.find(parent);
  values.transform.position[0] = 5;
  AE_EXPECT_TRUE(graph.applyEntityValues(parent, values), "pai deslocado");
  values = *graph.find(rain);
  values.transform.position[1] = 32;
  values.transform.rotationDegrees[1] = 30;
  auto *added = static_cast<scene::FollowCamera *>(values.components.add(scene::FollowCamera::descriptor));
  AE_EXPECT_TRUE(added != nullptr, "componente na lista");
  std::copy(follow.offset, follow.offset + 3, added->offset);
  AE_EXPECT_TRUE(graph.applyEntityValues(rain, values), "objeto com Seguir câmera");
  runtime::GameWorld world;
  runtime::ScenePhysics physics;
  runtime::SceneParticles particles;
  runtime::SceneAudio audio;
  runtime::SceneVfx vfx;
  AE_EXPECT_TRUE(world.load(graph) && physics.start(world), "mundo do Play");
  const float camera[3]{1, 2, 3};
  vfx.setCamera(camera);
  AE_EXPECT_TRUE(world.beginFrame(1.0 / 60), "quadro aberto");
  AE_EXPECT_TRUE(vfx.advance(world, physics, particles, audio, 1.0f / 60), vfx.report().lastError.c_str());
  AE_EXPECT_EQ(vfx.report().followers, 1u, "um seguidor");
  float matrix[16];
  AE_EXPECT_TRUE(runtime::worldMatrix(world.graph(), rain, matrix) && std::abs(matrix[12] - 1) < 1e-4f && std::abs(matrix[13] - 30) < 1e-4f &&
                 std::abs(matrix[14] - 2) < 1e-4f, "posição de mundo = câmera + deslocamento, sob um pai deslocado");
  AE_EXPECT_TRUE(std::abs(world.graph().find(rain)->transform.rotationDegrees[1] - 30) < 1e-3f, "giro do autor preservado");
  vfx.setCamera(nullptr);
  AE_EXPECT_TRUE(vfx.advance(world, physics, particles, audio, 1.0f / 60) && vfx.report().followers == 0, "sem câmera, fica onde está");
}

AE_TEST(g6a_distortion_blend_reaches_the_batch_with_its_strength) {
  scene::ParticleSystem source;
  source.materialBlend = scene::ParticleBlend::Distortion;
  source.materialDistortion = 100;
  scene::ParticleSystem back;
  AE_EXPECT_TRUE(roundTrip(source, back) && back.materialBlend == scene::ParticleBlend::Distortion && back.materialDistortion == 100,
                 "Distorção e força preservadas");
  ParticleEmitter emitter;
  emitter.configure(source, 1);
  const auto push = renderer::particles::particlePushConstants(emitter.material(), 0, 1000, 500, false);
  AE_EXPECT_TRUE((push.layers[3] & renderer::particles::kParticleFlagDistortion) != 0 &&
                 (push.layers[3] & renderer::particles::kParticleFlagAdditive) == 0 && push.sheet[2] == 100,
                 "lote distorce na força pedida, sem soma");
  source.materialBlend = scene::ParticleBlend::Additive;
  emitter.configure(source, 1);
  AE_EXPECT_TRUE((renderer::particles::particlePushConstants(emitter.material(), 0, 1000, 500, false).layers[3] &
                  renderer::particles::kParticleFlagDistortion) == 0, "aditivo não distorce");
}

AE_TEST(g6a_unity_particle_system_converts_module_by_module) {
  resources::UnityYamlDocument document;
  std::string error;
  AE_EXPECT_TRUE(resources::readUnityYaml(particleYaml(4, 4, 2, "{fileID: 0}"), document, error), error.c_str());
  const auto *system = document.find(30), *renderer = document.find(40);
  resources::UnityParticleImport converted;
  AE_EXPECT_TRUE(system && renderer && resources::convertUnityParticleSystem(system->value, &renderer->value, converted, error), error.c_str());
  const auto &s = converted.settings;
  AE_EXPECT_TRUE(s.duration == 2 && !s.looping && !s.autoRandomSeed && s.randomSeed == 7 && s.maxParticles == 50, "Principal");
  AE_EXPECT_TRUE(s.startSize.mode == scene::ParticleCurveMode::TwoConstants && s.startSize.constantMin == .25f && s.startSize.constantMax == .5f,
                 "duas constantes da Unity (3) viram Entre duas constantes (2)");
  AE_EXPECT_TRUE(std::abs(s.startRotation.constantMax - 90) < 1e-3f, "radianos para graus");
  AE_EXPECT_TRUE(std::abs(s.startColor.colorMax[0] - resources::unityLinearFromSrgb(.5f)) < 1e-5f && s.startColor.colorMax[3] == .5f,
                 "cor sRGB para linear, alfa igual");
  AE_EXPECT_TRUE(s.burstCount == 1 && s.bursts[0].countMin == 20 && s.bursts[0].countMax == 30, "rajada");
  AE_EXPECT_TRUE(s.colorOverLifetimeEnabled && s.colorOverLifetime.gradientMax.alphas.back().alpha == 0, "gradiente no tempo");
  AE_EXPECT_TRUE(s.renderMode == scene::ParticleRenderMode::Mesh && s.meshShape == scene::ParticleMeshShape::Quad &&
                 s.renderAlignment == scene::ParticleRenderAlignment::Local && !converted.mesh.external(), "Quad embutida, Local");
  AE_EXPECT_TRUE(!converted.approximations.empty(), "Custom Data listado");
  // A frente da Unity (+Z) refletida em Z: o cone emite para −Z local.
  ParticleEmitter emitter;
  auto shot = s;
  shot.emissionRate = scene::particleCurveDefault(0);
  emitter.configure(shot, 1);
  emitter.emit(20, kIdentity);
  std::vector<ParticleInstance> out;
  emitter.build(kIdentity, kCamera, out);
  bool backward = !out.empty();
  for (const auto &p : out) backward = backward && p.velocity[2] < 0;
  AE_EXPECT_TRUE(backward, "cone para −Z");
  // Forma de malha não tem consumidor: perda visível listada.
  resources::UnityYamlDocument meshShape;
  AE_EXPECT_TRUE(resources::readUnityYaml(particleYaml(6, 0, 0, "{fileID: 0}"), meshShape, error), error.c_str());
  AE_EXPECT_TRUE(resources::convertUnityParticleSystem(meshShape.find(30)->value, &meshShape.find(40)->value, converted, error) &&
                 !converted.losses.empty(), "forma Malha vira perda");
}

AE_TEST(g6a_hovl_material_becomes_particle_family_with_component_hints) {
  const std::string material =
      "%YAML 1.1\n--- !u!21 &2100000\nMaterial:\n  m_Name: Fogo\n"
      "  m_Shader: {fileID: 4800000, guid: bd6af5d577c618947b73aa60db554e51, type: 3}\n"
      "  m_SavedProperties:\n    m_TexEnvs:\n    - _MainTex:\n        m_Texture: {fileID: 2800000, guid: 0123456789abcdef0123456789abcdef, type: 3}\n"
      "        m_Scale: {x: 1, y: 1}\n        m_Offset: {x: 0, y: 0}\n"
      "    - _Noise:\n        m_Texture: {fileID: 2800000, guid: fedcba9876543210fedcba9876543210, type: 3}\n"
      "        m_Scale: {x: 1, y: 1}\n        m_Offset: {x: 0, y: 0}\n"
      "    m_Floats:\n    - _Emission: 3\n    - _Usecenterglow: 1\n"
      "    m_Colors:\n    - _Color: {r: 1, g: 0.5, b: 0.2, a: 1}\n    - _SpeedMainTexUVNoiseZW: {r: 0.1, g: 0, b: -0.3, a: 0.2}\n";
  resources::UnityMaterialImport converted;
  AE_EXPECT_TRUE(resources::readUnityMaterial(material, converted) && converted.supported(), converted.diagnostic.c_str());
  AE_EXPECT_TRUE(converted.material.shading.shader == scene::MaterialShaderParticlesUnlit &&
                 converted.material.shading.blend == scene::MaterialBlendAdditive, "Add_CenterGlow: Partícula Unlit aditivo");
  AE_EXPECT_TRUE(converted.textures.size() == 1 && converted.textures[0].binding == 0, "só a imagem principal sobe");
  const auto &look = converted.particle;
  AE_EXPECT_TRUE(look.family == resources::UnityParticleFamily::Additive && look.emission == 3 && look.noise && look.centerGlow &&
                 std::abs(look.scroll[0] - .1f) < 1e-6f && std::abs(look.noiseScroll[0] + .3f) < 1e-6f, "brilho, ruído e rolagem para o componente");
}

AE_TEST(g6a_prefab_with_vfx_scripts_builds_native_components) {
  const resources::AssetGuid mover = resources::assetGuidFromSeed("script:ProjectileMover"),
                             autoDestroy = resources::assetGuidFromSeed("script:AutoDestroyPS");
  std::string yaml = particleYaml(0, 0, 0, "{fileID: 2100000, guid: 11111111111111111111111111111111, type: 2}");
  // O GameObject declara mais quatro componentes: luz, áudio e dois scripts.
  const auto at = yaml.find("  - component: {fileID: 40}\n") + std::string("  - component: {fileID: 40}\n").size();
  yaml.insert(at, "  - component: {fileID: 50}\n  - component: {fileID: 60}\n  - component: {fileID: 70}\n  - component: {fileID: 80}\n");
  yaml += "--- !u!108 &50\nLight:\n  m_GameObject: {fileID: 10}\n  m_Enabled: 1\n  m_Type: 2\n  m_Color: {r: 1, g: 0.6, b: 0.2, a: 1}\n"
          "  m_Intensity: 3\n  m_Range: 6\n";
  yaml += "--- !u!82 &60\nAudioSource:\n  m_GameObject: {fileID: 10}\n  m_Enabled: 1\n"
          "  m_audioClip: {fileID: 8300000, guid: 22222222222222222222222222222222, type: 3}\n  m_PlayOnAwake: 0\n  m_Volume: 0.7\n"
          "  MinDistance: 2\n  MaxDistance: 40\n";
  yaml += "--- !u!114 &70\nMonoBehaviour:\n  m_GameObject: {fileID: 10}\n  m_Enabled: 1\n  m_Script: {fileID: 11500000, guid: " + autoDestroy.text() +
          ", type: 3}\n";
  yaml += "--- !u!114 &80\nMonoBehaviour:\n  m_GameObject: {fileID: 10}\n  m_Enabled: 1\n  m_Script: {fileID: 11500000, guid: " + mover.text() +
          ", type: 3}\n  speed: 25\n  hitOffset: 0.1\n  UseFirePointRotation: 0\n  rotationOffset: {x: 0, y: 0, z: 0}\n"
          "  hit: {fileID: 100, guid: 33333333333333333333333333333333, type: 3}\n  flash: {fileID: 0}\n  Detached: []\n";
  resources::UnityYamlDocument document;
  std::string error;
  AE_EXPECT_TRUE(resources::readUnityYaml(yaml, document, error), error.c_str());
  resources::UnityPrefabImport imported;
  AE_EXPECT_TRUE(!resources::convertUnityPrefab(document, imported, error), "sem contexto do pacote, script continua recusado");
  resources::UnityPrefabContext context;
  context.scriptName = [&](const resources::AssetGuid &guid) {
    return guid == mover ? std::string("ProjectileMover") : guid == autoDestroy ? std::string("AutoDestroyPS") : std::string();
  };
  AE_EXPECT_TRUE(resources::convertUnityPrefab(document, imported, error, &context), error.c_str());
  AE_EXPECT_TRUE(imported.coverage.particleSystems == 1 && imported.coverage.lights == 1 && imported.coverage.audioSources == 1 &&
                 imported.coverage.scripts == 2, "componentes contados");
  const runtime::UnityPrefabResolver resolve = [](const resources::UnityObjectReference &reference, resources::AssetType type,
                                                  std::vector<resources::AssetGuid> &ids, std::string &) {
    ids.push_back(resources::assetGuidFromSeed(reference.guid.text() + ":" + std::to_string(static_cast<u32>(type))));
    return true;
  };
  runtime::Prefab prefab;
  runtime::UnityPrefabIdentityMap identities;
  std::vector<std::string> notes;
  runtime::UnityPrefabEffectServices effects;
  effects.notes = &notes;
  AE_EXPECT_TRUE(runtime::buildUnityPrefab(imported, resources::assetGuidFromSeed("prefab:fogo"), resolve, nullptr, prefab, identities, error,
                                           &effects), error.c_str());
  const auto *root = prefab.graph().find(prefab.root());
  AE_EXPECT_TRUE(root && root->transform.position[2] == -2, "posição refletida em Z");
  const auto *system = static_cast<const scene::ParticleSystem *>(root->components.find(scene::ParticleSystem::descriptor));
  const auto *light = static_cast<const scene::Light *>(root->components.find(scene::Light::descriptor));
  const auto *audio = static_cast<const scene::AudioSource *>(root->components.find(scene::AudioSource::descriptor));
  const auto *projectile = static_cast<const scene::Projectile *>(root->components.find(scene::Projectile::descriptor));
  AE_EXPECT_TRUE(system && system->stopAction == scene::ParticleStopAction::Destroy && system->materialAsset.valid(),
                 "AutoDestroyPS vira Destruir ao parar; material resolvido");
  AE_EXPECT_TRUE(light && light->kind == scene::LightKind::Point && light->intensity == 3 && light->range == 6 &&
                 light->unit == scene::LightUnit::Engine, "luz pontual com a intensidade da Unity");
  AE_EXPECT_TRUE(audio && audio->clip.valid() && audio->playback == scene::AudioPlayback::Stopped && audio->volume == .7f, "fonte de áudio");
  AE_EXPECT_TRUE(projectile && projectile->speed == 25 && projectile->impact.valid() &&
                 projectile->forwardAxis == scene::VfxForwardAxis::NegativeZ, "ProjectileMover vira Projétil com impacto");
}

AE_TEST(g6b_unistorm_weather_and_cloud_profiles_become_astra_values) {
  const std::string weather =
      "%YAML 1.1\n--- !u!114 &11400000\nMonoBehaviour:\n  m_Enabled: 1\n"
      "  m_Script: {fileID: 11500000, guid: e4ec900128fbff24683f8f2aac8c2192, type: 3}\n  m_Name: Tempestade\n"
      "  WeatherTypeName: Thunderstorm\n  FogDensity: 0.005\n  WindIntensity: 1\n  ParticleEffectAmount: 1000\n"
      "  ParticleEffectVector: {x: 0, y: 28, z: 1}\n"
      "  PrecipitationWeatherType: 0\n  UseLightning: 0\n  TemperatureType: 1\n  CloudLevel: 4\n  ShaderControl: 0\n"
      "  UseWeatherEffect: 0\n  UseWeatherSound: 0\n  WeatherVolume: 0.8\n"
      "  WeatherSound: {fileID: 8300000, guid: 44444444444444444444444444444444, type: 3}\n";
  resources::UnityYamlDocument document;
  std::string error;
  resources::UniStormWeather parsed;
  AE_EXPECT_TRUE(resources::readUnityYaml(weather, document, error) && resources::readUniStormWeather(document, parsed, error), error.c_str());
  AE_EXPECT_TRUE(parsed.name == "Thunderstorm" && parsed.weather.kind == scene::WeatherKind::Storm && parsed.weather.lightning &&
                 parsed.weather.intensity == 1 && std::abs(parsed.weather.fogDensity - .005f) < 1e-4f && parsed.weather.rainRate == 7000 &&
                 parsed.weather.surfaceWetness == 1 && parsed.useSound && parsed.soundVolume == .8f, "Thunderstorm vira Tempestade");
  AE_EXPECT_TRUE(parsed.effectOffset[1] == 28 && parsed.effectOffset[2] == 1 && parsed.additionalOffset[1] == 28,
                 "posição do efeito sobre o jogador");
  const std::string cloud =
      "%YAML 1.1\n--- !u!114 &11400000\nMonoBehaviour:\n  m_Enabled: 1\n"
      "  m_Script: {fileID: 11500000, guid: e61fb300c8082e942917c8b932209ad4, type: 3}\n  m_Name: Limpo\n"
      "  EdgeSoftness: 0.025\n  BaseSoftness: 0.1\n  DetailStrength: 0.04\n  Density: 1\n  CoverageBias: -0.05\n  DetailScale: 730\n";
  resources::UniStormCloud clouds;
  resources::UnityYamlDocument cloudDocument;
  AE_EXPECT_TRUE(resources::readUnityYaml(cloud, cloudDocument, error) && resources::readUniStormCloud(cloudDocument, clouds, error), error.c_str());
  AE_EXPECT_TRUE(clouds.clouds.enabled && clouds.clouds.density == 1 && std::abs(clouds.clouds.coverage - .35f) < 1e-4f && clouds.clouds.valid(),
                 "perfil de nuvem vira Cloud Layer");
  resources::UniStormWeather notWeather;
  AE_EXPECT_TRUE(!resources::readUniStormWeather(cloudDocument, notWeather, error), "Cloud Profile não é Weather Type");
  // A Rain comum do pacote (1000 partículas, sem relâmpago) fica no meio da
  // Chuva da Astra; o topo é a tempestade que engole o fundo (POCO, 27/10).
  const std::string rain =
      "%YAML 1.1\n--- !u!114 &11400000\nMonoBehaviour:\n  m_Enabled: 1\n"
      "  m_Script: {fileID: 11500000, guid: e4ec900128fbff24683f8f2aac8c2192, type: 3}\n  m_Name: Rain\n"
      "  WeatherTypeName: Rain\n  ParticleEffectAmount: 1000\n  PrecipitationWeatherType: 0\n  UseLightning: 1\n"
      "  TemperatureType: 1\n  CloudLevel: 4\n";
  resources::UnityYamlDocument rainDocument;
  resources::UniStormWeather rainy;
  AE_EXPECT_TRUE(resources::readUnityYaml(rain, rainDocument, error) && resources::readUniStormWeather(rainDocument, rainy, error) &&
                 rainy.weather.kind == scene::WeatherKind::Rain && std::abs(rainy.weather.intensity - .5f) < 1e-4f, "Rain no meio da escala");
}

AE_TEST(g6a_vfx_components_keep_their_identity_in_a_saved_list) {
  // Cena e prefab gravam a lista com identidades; o read de cada comportamento
  // não pode trocar a identidade pela do temporário (zero).
  scene::Components list;
  for (const auto *type : {&scene::LineRenderer::descriptor, &scene::Laser::descriptor, &scene::Projectile::descriptor,
                           &scene::ParticleCollisionSpawn::descriptor, &scene::EffectSound::descriptor, &scene::Rotator::descriptor,
                           &scene::FollowCamera::descriptor})
    AE_EXPECT_TRUE(list.add(*type) != nullptr, std::string(type->id).c_str());
  std::vector<const scene::ComponentType *> registry;
  for (const auto &schema : scene::componentSchemas) registry.push_back(schema.type);
  std::ostringstream out;
  out.imbue(std::locale::classic());
  AE_EXPECT_TRUE(list.write(out, true), "lista gravada com identidades");
  std::istringstream in(out.str());
  in.imbue(std::locale::classic());
  scene::Components back;
  AE_EXPECT_TRUE(back.read(in, registry, scene::UnknownComponentPolicy::Reject, true) && back.valid() && back.size() == 7,
                 "lista relida e válida");
  bool same = true;
  for (usize k = 0; k < back.size(); ++k) same = same && back.at(k)->instanceId() == list.at(k)->instanceId() && back.at(k)->instanceId() != 0;
  AE_EXPECT_TRUE(same, "cada comportamento mantém a identidade gravada");
}

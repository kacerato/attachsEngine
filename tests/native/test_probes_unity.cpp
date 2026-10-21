// PK-6 da auditoria full Unity: Reflection Probe e Light Probe Group. Cada
// propriedade tem um consumidor que este arquivo exercita, da autoria ao
// registro que o fragmento lê. A captura das faces e o filtro rodam no
// aparelho; aqui ficam a matemática, os formatos, a escolha por objeto, as
// alças, os gizmos e o ciclo de bake do editor com uma captura sintética.
#include "harness.h"
#include "core/engine_capability.h"
#include "editor/editor_archive.h"
#include "editor/editor_component_handles.h"
#include "editor/editor_component_visuals.h"
#include "editor/editor_creation_catalog.h"
#include "editor/editor_document.h"
#include "editor/editor_session.h"
#include "renderer/light_probes.h"
#include "renderer/reflection_probes.h"
#include "runtime/component_operations.h"
#include "runtime/scene_probes.h"
#include "scene/component_properties.h"
#include "scene/component_schema.h"
#include "scene/light_probe_group.h"
#include "scene/mesh_renderer.h"
#include "scene/reflection_probe.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace ae;
using namespace ae::editor;

namespace {
// Cubo de radiância sintético: céu azul em cima, chão vermelho embaixo.
renderer::CubeFaces syntheticRoom(u32 size, float sky, float floorRed) {
  renderer::CubeFaces faces;
  faces.size = size;
  faces.rgb.resize(6ull * size * size * 3);
  for (u32 face = 0; face < 6; ++face)
    for (u32 y = 0; y < size; ++y)
      for (u32 x = 0; x < size; ++x) {
        float d[3];
        renderer::cubeTexelDirection(face, (x + .5f) / size, (y + .5f) / size, d);
        float *rgb = &faces.rgb[((static_cast<usize>(face) * size + y) * size + x) * 3];
        if (d[1] > 0) { rgb[0] = sky * .4f; rgb[1] = sky * .6f; rgb[2] = sky; }
        else { rgb[0] = floorRed; rgb[1] = floorRed * .1f; rgb[2] = floorRed * .1f; }
      }
  return faces;
}
renderer::CubeFaces constantSky(u32 size, float value) {
  renderer::CubeFaces faces;
  faces.size = size;
  faces.rgb.assign(6ull * size * size * 3, value);
  return faces;
}
float irradianceAt(const renderer::ShL2 &sh, float x, float y, float z, u32 channel) {
  const float length = std::sqrt(x * x + y * y + z * z);
  const float d[3]{x / length, y / length, z / length};
  float out[3];
  renderer::evaluateShIrradiance(sh, d, out);
  return out[channel];
}
renderer::SceneReflectionProbe boxProbe(float cx, float half, i32 importance, float blend) {
  renderer::SceneReflectionProbe p;
  p.position[0] = cx;
  for (u32 k = 0; k < 3; ++k) { p.boxMin[k] = (k == 0 ? cx : 0) - half; p.boxMax[k] = (k == 0 ? cx : 0) + half; }
  p.importance = importance;
  p.blendDistance = blend;
  return p;
}
bool loadAsset(const char *relative, std::vector<u8> &out) {
  const std::string path = std::string(AETHER_REPOSITORY_ROOT) + "/" + relative;
  std::FILE *file = std::fopen(path.c_str(), "rb");
  if (!file) return false;
  std::fseek(file, 0, SEEK_END);
  const long size = std::ftell(file);
  std::fseek(file, 0, SEEK_SET);
  out.resize(size > 0 ? static_cast<usize>(size) : 0);
  const bool ok = size > 0 && std::fread(out.data(), 1, out.size(), file) == out.size();
  std::fclose(file);
  return ok;
}
const ui::UiFont &probeFont() {
  static std::vector<u8> bytes;
  static ui::UiFont value = [] { ui::UiFont r; if (loadAsset("assets/astra-visual/ui/astra-ui-font.aeuf", bytes)) r.load(bytes); return r; }();
  return value;
}
const ui::UiIconAtlas &probeIcons() {
  static std::vector<u8> bytes;
  static ui::UiIconAtlas value = [] { ui::UiIconAtlas r; if (loadAsset("assets/astra-visual/ui/astra-ui-icons.aeui", bytes)) r.load(bytes); return r; }();
  return value;
}
} // namespace

// --- SH L2 ----------------------------------------------------------------

AE_TEST(probe_sh_constant_sky_gives_pi_irradiance_everywhere) {
  renderer::ShL2 sh;
  AE_EXPECT_TRUE(renderer::projectCubeToIrradiance(constantSky(16, 2.0f), sh), "projeção aceita");
  const float expected = 3.14159265f * 2.0f;
  for (const auto &d : std::vector<std::array<float, 3>>{{0, 1, 0}, {0, -1, 0}, {1, 0, 0}, {.3f, .5f, -.8f}})
    AE_EXPECT_TRUE(std::abs(irradianceAt(sh, d[0], d[1], d[2], 0) - expected) < expected * 0.01f,
                   "céu constante L dá irradiância πL em toda direção");
}

AE_TEST(probe_sh_room_bleeds_floor_colour_and_darkens_below) {
  renderer::ShL2 sh;
  AE_EXPECT_TRUE(renderer::projectCubeToIrradiance(syntheticRoom(24, 1.0f, .8f), sh), "projeção aceita");
  const float upBlue = irradianceAt(sh, 0, 1, 0, 2), downBlue = irradianceAt(sh, 0, -1, 0, 2);
  const float upRed = irradianceAt(sh, 0, 1, 0, 0), downRed = irradianceAt(sh, 0, -1, 0, 0);
  AE_EXPECT_TRUE(upBlue > downBlue * 3, "a face virada ao céu recebe o azul");
  AE_EXPECT_TRUE(downRed > upRed, "a face virada ao chão vermelho pega o rebate vermelho");
  // Hemisfério inteiro com L=1 dá π na normal dele; L2 aproxima com erro pequeno.
  AE_EXPECT_TRUE(std::abs(upBlue - 3.14159265f) < 0.35f, "cosseno integrado do hemisfério superior");
}

AE_TEST(probe_sh_dering_removes_negative_lobes) {
  // Um sol pequeno e forte: o L2 truncado tem lóbulo negativo do lado oposto.
  renderer::CubeFaces faces = constantSky(16, 0.0f);
  for (u32 y = 6; y < 10; ++y)
    for (u32 x = 6; x < 10; ++x)
      for (u32 k = 0; k < 3; ++k) faces.rgb[((2ull * 16 + y) * 16 + x) * 3 + k] = 400;
  renderer::ShL2 sh;
  AE_EXPECT_TRUE(renderer::projectCubeToIrradiance(faces, sh), "projeção aceita");
  // O L2 truncado de uma fonte pequena fica negativo num anel a ~145° dela
  // (0,25 + 0,5·cos θ + 0,3125·(3cos²θ−1)/2 < 0 perto de cos θ = −0,8).
  const auto lowest = [](const renderer::ShL2 &value) {
    float minimum = 1e9f;
    for (int i = 0; i < 2000; ++i) {
      const float z = 1 - 2 * (i + .5f) / 2000, r = std::sqrt(std::max(0.f, 1 - z * z)), phi = 2.39996323f * i;
      float d[3]{r * std::cos(phi), z, r * std::sin(phi)}, basis[9], sum = 0;
      renderer::shL2Basis(d, basis);
      for (u32 k = 0; k < 9; ++k) sum += value.c[k][0] * basis[k];
      minimum = std::min(minimum, sum);
    }
    return minimum;
  };
  AE_EXPECT_TRUE(lowest(sh) < 0, "sem anti-ringing há irradiância negativa num anel atrás do sol");
  renderer::deringShL2(sh);
  AE_EXPECT_TRUE(lowest(sh) >= -1e-2f * irradianceAt(sh, 0, 1, 0, 0), "Remover anéis tira o anel negativo");
  AE_EXPECT_TRUE(irradianceAt(sh, 0, 1, 0, 0) > 1, "o lado do sol continua claro");
}

// --- Tetraedros -----------------------------------------------------------

AE_TEST(probe_tetrahedra_reproduce_linear_light_inside_the_hull) {
  renderer::LightProbeSet set;
  std::mt19937 random(7);
  std::uniform_real_distribution<float> unit(-4, 4);
  for (int x = 0; x < 3; ++x)
    for (int y = 0; y < 3; ++y)
      for (int z = 0; z < 3; ++z) set.positions.push_back({x * 4.f - 4, y * 2.f, z * 4.f - 4});
  for (int i = 0; i < 12; ++i) set.positions.push_back({unit(random), unit(random) * .5f + 2, unit(random)});
  for (const auto &p : set.positions) {
    renderer::ShL2 sh;
    sh.c[0][0] = 1 + 0.25f * p[0] + 0.5f * p[1] - 0.125f * p[2]; // campo linear
    set.sh.push_back(sh);
  }
  AE_EXPECT_TRUE(set.tetrahedralize(), "tetraedraliza");
  AE_EXPECT_TRUE(set.tetrahedra.size() >= 24, "uma grade 3x3x3 vira dezenas de tetraedros");
  u32 boundary = 0;
  for (const auto &t : set.tetrahedra) for (i32 n : t.neighbor) if (n < 0) ++boundary;
  AE_EXPECT_TRUE(boundary > 0, "o casco tem faces sem vizinho");
  i32 hint = -1;
  for (int i = 0; i < 50; ++i) {
    const float p[3]{unit(random) * .9f, (unit(random) + 4) * .45f + .1f, unit(random) * .9f};
    const auto where = renderer::locateProbes(set, p, hint);
    AE_EXPECT_TRUE(where.inside && where.count == 4, "ponto interior cai num tetraedro");
    float sum = 0;
    for (float w : where.weight) { sum += w; AE_EXPECT_TRUE(w >= -1e-5f, "pesos não negativos"); }
    AE_EXPECT_TRUE(std::abs(sum - 1) < 1e-4f, "pesos somam 1");
    const auto sh = renderer::interpolateProbes(set, where);
    const float expected = 1 + 0.25f * p[0] + 0.5f * p[1] - 0.125f * p[2];
    AE_EXPECT_TRUE(std::abs(sh.c[0][0] - expected) < 1e-3f, "baricêntrico reproduz luz linear exatamente");
    const auto fresh = renderer::locateProbes(set, p, -1);
    AE_EXPECT_TRUE(std::abs(renderer::interpolateProbes(set, fresh).c[0][0] - expected) < 1e-3f,
                   "a caminhada a partir do quadro anterior acha o mesmo resultado");
    hint = where.tetrahedron;
  }
}

AE_TEST(probe_tetrahedra_clamp_outside_and_cover_unity_default_cube) {
  scene::LightProbeGroup group;
  AE_EXPECT_EQ(group.points.size(), usize{8}, "o grupo nasce com 8 sondas num cubo, como na Unity");
  renderer::LightProbeSet set;
  for (const auto &p : group.points) {
    set.positions.push_back({p.position[0], p.position[1], p.position[2]});
    renderer::ShL2 sh;
    sh.c[0][0] = p.position[1] > 0 ? 2.0f : 1.0f;
    set.sh.push_back(sh);
  }
  AE_EXPECT_TRUE(set.tetrahedralize() && set.tetrahedra.size() >= 5, "o cubo cosférico vira volume");
  const float centre[3]{0, 0, 0};
  const auto inside = renderer::locateProbes(set, centre);
  AE_EXPECT_TRUE(inside.inside, "centro do cubo é interior");
  AE_EXPECT_TRUE(std::abs(renderer::interpolateProbes(set, inside).c[0][0] - 1.5f) < 1e-3f, "meio caminho entre o alto e o baixo");
  const float above[3]{0, 5, 0};
  const auto outside = renderer::locateProbes(set, above);
  float sum = 0;
  for (float w : outside.weight) sum += w;
  AE_EXPECT_TRUE(!outside.inside && outside.count == 4 && std::abs(sum - 1) < 1e-4f, "fora do casco: pesos presos e normalizados");
  AE_EXPECT_TRUE(std::abs(renderer::interpolateProbes(set, outside).c[0][0] - 2.0f) < 1e-3f, "acima do grupo usa as sondas de cima");
  renderer::LightProbeSet single;
  single.positions.push_back({1, 2, 3});
  single.sh.push_back({});
  AE_EXPECT_TRUE(single.tetrahedralize() && single.tetrahedra.empty(), "uma sonda não forma tetraedro");
  AE_EXPECT_EQ(renderer::locateProbes(single, centre).count, 1u, "e é usada inteira");
}

// --- Mistura de reflexo por objeto ------------------------------------------

AE_TEST(probe_reflection_blend_follows_mesh_renderer_modes) {
  using U = renderer::ReflectionProbeUsage;
  std::vector<renderer::SceneReflectionProbe> frame{boxProbe(0, 5, 1, 1), boxProbe(6, 5, 2, 1)};
  const float shared[3]{3, 0, 0}, onlyFirst[3]{-3, 0, 0}, edge[3]{-4.5f, 0, 0}, nowhere[3]{40, 0, 0};
  auto blend = renderer::blendReflectionProbes(frame, shared, U::BlendProbes);
  AE_EXPECT_EQ(blend.first, 1u, "a de maior Importance vem primeiro");
  AE_EXPECT_EQ(blend.second, 0u, "a outra completa a mistura");
  AE_EXPECT_TRUE(std::abs(blend.firstWeight + blend.secondWeight - 1) < 1e-5f && blend.skyWeight() < 1e-5f,
                 "Misturar sondas não mistura o céu");
  blend = renderer::blendReflectionProbes(frame, edge, U::BlendProbes);
  AE_EXPECT_TRUE(blend.first == 0 && blend.firstWeight == 1, "uma sonda só fica inteira em Misturar sondas");
  blend = renderer::blendReflectionProbes(frame, edge, U::BlendProbesAndSkybox);
  AE_EXPECT_TRUE(std::abs(blend.firstWeight - .5f) < 1e-4f && std::abs(blend.skyWeight() - .5f) < 1e-4f,
                 "na faixa de mistura a borda passa para o céu em \"Misturar sondas e céu\"");
  blend = renderer::blendReflectionProbes(frame, onlyFirst, U::Simple);
  AE_EXPECT_TRUE(blend.first == 0 && blend.firstWeight == 1 && blend.second == renderer::ReflectionProbeNone, "Simples usa só a principal");
  blend = renderer::blendReflectionProbes(frame, shared, U::Off);
  AE_EXPECT_TRUE(blend.firstWeight == 0 && blend.skyWeight() == 1, "Desligado reflete só o céu");
  blend = renderer::blendReflectionProbes(frame, nowhere, U::BlendProbes);
  AE_EXPECT_TRUE(blend.firstWeight == 0, "fora de toda caixa: céu");
  frame[1].importance = 1;
  frame[0].boxMin[0] = -1; frame[0].boxMax[0] = 4; // caixa menor
  blend = renderer::blendReflectionProbes(frame, shared, U::BlendProbes);
  AE_EXPECT_EQ(blend.first, 0u, "empate de importância fica com a caixa menor");
  const auto word = renderer::encodeReflectionProbeBlend(blend);
  const auto back = renderer::decodeReflectionProbeBlend(word);
  AE_EXPECT_TRUE(back.first == blend.first && back.second == blend.second &&
                 std::abs(back.firstWeight - blend.firstWeight) < 1.f / 255 && std::abs(back.secondWeight - blend.secondWeight) < 1.f / 255,
                 "a palavra do desenho preserva camadas e pesos");
}

AE_TEST(probe_box_projection_moves_reflection_with_the_viewer) {
  const float lo[3]{-5, -5, -5}, hi[3]{5, 5, 5}, capture[3]{0, 0, 0};
  const float d[3]{.70710678f, .70710678f, 0};
  float centre[3], side[3];
  const float atCentre[3]{0, 0, 0}, nearWall[3]{4, 0, 0};
  renderer::boxProjectReflection(atCentre, d, lo, hi, capture, centre);
  renderer::boxProjectReflection(nearWall, d, lo, hi, capture, side);
  AE_EXPECT_TRUE(std::abs(centre[0] - centre[1]) < 1e-4f, "no centro de captura a direção não muda");
  AE_EXPECT_TRUE(std::abs(side[0] - 5) < 1e-4f && std::abs(side[1] - 1) < 1e-4f,
                 "perto da parede o raio bate a 1 m de altura e a sonda é lida nessa direção (paralaxe)");
  const float outside[3]{9, 0, 0};
  float far[3];
  renderer::boxProjectReflection(outside, d, lo, hi, capture, far);
  AE_EXPECT_TRUE(far[0] == d[0] && far[1] == d[1], "fora da caixa vale o reflexo no infinito");
  std::mt19937 random(3);
  std::uniform_real_distribution<float> unit(-1, 1);
  for (int i = 0; i < 200; ++i) {
    float v[3]{unit(random), unit(random), unit(random)};
    const float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l < .1f) continue;
    for (float &c : v) c /= l;
    float uv[2], r[3];
    renderer::octahedralEncode(v, uv);
    renderer::octahedralDecode(uv, r);
    AE_EXPECT_TRUE(std::abs(r[0] - v[0]) + std::abs(r[1] - v[1]) + std::abs(r[2] - v[2]) < 1e-4f, "octaedro ida e volta");
  }
}

AE_TEST(probe_frame_selection_prefers_the_camera_room_and_budget) {
  std::vector<renderer::SceneReflectionProbe> probes;
  for (int i = 0; i < 12; ++i) probes.push_back(boxProbe(static_cast<float>(i) * 20, 5, i == 11 ? 0 : 1, 1));
  const float camera[3]{220, 0, 0};
  std::array<u32, renderer::MaximumFrameReflectionProbes> chosen{};
  const u32 count = renderer::selectFrameReflectionProbes(probes, camera, chosen);
  AE_EXPECT_EQ(count, renderer::MaximumFrameReflectionProbes, "8 sondas por quadro");
  AE_EXPECT_EQ(chosen[0], 11u, "a sonda que contém a câmera entra primeiro, mesmo com importância menor");
  AE_EXPECT_EQ(renderer::reflectionProbeBaseMip(128), 0u, "128 px por face ocupam o octaedro inteiro");
  AE_EXPECT_EQ(renderer::reflectionProbeBaseMip(32), 2u, "32 px começam dois mips acima");
  AE_EXPECT_EQ(renderer::reflectionProbeBaseMip(16), 3u, "16 px, três");
}

// --- Recursos assados -------------------------------------------------------

AE_TEST(probe_baked_resources_round_trip_and_reject_damage) {
  renderer::ReflectionProbeTexture texture;
  texture.guid = {0x1234, 0x5678};
  texture.sourceHash = 0xabcdef;
  texture.bakeSeconds = 1.5f;
  texture.baseMip = 1;
  texture.texels.resize(texture.expectedTexels());
  for (usize i = 0; i < texture.texels.size(); ++i) texture.texels[i] = renderer::floatToHalf(static_cast<float>(i % 97) / 13.0f);
  const auto bytes = texture.serialize();
  renderer::ReflectionProbeTexture back;
  std::string error;
  AE_EXPECT_TRUE(renderer::ReflectionProbeTexture::deserialize(bytes, back, &error), "HDR lido");
  AE_EXPECT_TRUE(back.guid == texture.guid && back.sourceHash == texture.sourceHash && back.texels == texture.texels &&
                 back.bakeSeconds == 1.5f, "HDR idêntico");
  texture.hdr = false;
  const auto ldr = texture.serialize();
  AE_EXPECT_TRUE(ldr.size() < bytes.size() * 0.6, "sem HDR o recurso ocupa metade");
  AE_EXPECT_TRUE(renderer::ReflectionProbeTexture::deserialize(ldr, back) && !back.hdr, "LDR lido");
  AE_EXPECT_TRUE(renderer::halfToFloat(back.texels[13 * 4]) <= 1.0f, "LDR corta em 1");
  auto broken = bytes;
  broken.pop_back();
  AE_EXPECT_TRUE(!renderer::ReflectionProbeTexture::deserialize(broken, back), "truncado recusado");
  broken = bytes;
  broken[0] = 'X';
  AE_EXPECT_TRUE(!renderer::ReflectionProbeTexture::deserialize(broken, back), "assinatura errada recusada");

  renderer::LightProbeBakeData data;
  data.guidHigh = 9; data.guidLow = 10; data.sourceHash = 77; data.bakeSeconds = 2;
  data.pointIds = {1, 2};
  data.positions = {{0, 1, 2}, {3, 4, 5}};
  data.sh.resize(2);
  data.sh[1].c[4][2] = -0.5f;
  const auto lightBytes = data.serialize();
  renderer::LightProbeBakeData lightBack;
  AE_EXPECT_TRUE(renderer::LightProbeBakeData::deserialize(lightBytes, lightBack), "sondas de luz lidas");
  AE_EXPECT_TRUE(lightBack.positions == data.positions && lightBack.pointIds == data.pointIds && lightBack.sh[1].c[4][2] == -0.5f &&
                 lightBack.sourceHash == 77, "sondas de luz idênticas");
  auto lightBroken = lightBytes;
  lightBroken.resize(lightBroken.size() - 4);
  AE_EXPECT_TRUE(!renderer::LightProbeBakeData::deserialize(lightBroken, lightBack), "sondas truncadas recusadas");
  for (float v : {0.0f, 1.0f, -2.5f, 65504.0f, 0.000061035f, 1e-6f}) {
    const float r = renderer::halfToFloat(renderer::floatToHalf(v));
    AE_EXPECT_TRUE(std::abs(r - v) <= std::abs(v) * 1e-3f + 1e-7f, "meia precisão ida e volta");
  }
  AE_EXPECT_TRUE(renderer::halfToFloat(renderer::floatToHalf(1e9f)) == 65504.0f, "satura no maior finito");
}

// --- Componentes ------------------------------------------------------------

AE_TEST(probe_reflection_probe_component_has_unity_defaults_and_round_trips) {
  scene::ReflectionProbe probe;
  AE_EXPECT_TRUE(probe.probeType == 0 && probe.importance == 1 && probe.intensity == 1 && probe.blendDistance == 1 &&
                 probe.boxSize[0] == 10 && probe.resolution == 128 && probe.hdr && !probe.boxProjection && probe.nearClip == .3f &&
                 probe.farClip == 1000 && probe.cullingMask == ~0u && probe.clearFlags == 0,
                 "padrões do Inspector da Unity 6");
  AE_EXPECT_TRUE(probe.valid(), "válida");
  probe.probeType = 1; probe.refreshMode = 1; probe.timeSlicing = 2; probe.boxProjection = true; probe.resolution = 64;
  probe.boxOffset[1] = 1.5f; probe.importance = 3; probe.bakedTexture = {1, 2}; probe.clearFlags = 1;
  std::ostringstream out;
  probe.write(out);
  scene::ReflectionProbe back;
  std::istringstream in(out.str());
  AE_EXPECT_TRUE(back.read(in, 1), "v1 lida");
  AE_EXPECT_TRUE(back.probeType == 1 && back.refreshMode == 1 && back.timeSlicing == 2 && back.boxProjection && back.resolution == 64 &&
                 back.boxOffset[1] == 1.5f && back.importance == 3 && back.bakedTexture == probe.bakedTexture && back.clearFlags == 1,
                 "campos preservados");
  std::istringstream again(out.str());
  AE_EXPECT_TRUE(!back.read(again, 2), "versão futura recusada");
  scene::Components components;
  components.add(scene::ReflectionProbe::descriptor);
  using S = scene::ComponentPropertyStatus;
  AE_EXPECT_TRUE(scene::setComponentProperty(components, "astra.render.reflection_probe", "resolution", u32{256}) == S::Applied,
                 "Resolução 256 aceita");
  AE_EXPECT_TRUE(scene::setComponentProperty(components, "astra.render.reflection_probe", "resolution", u32{100}) != S::Applied,
                 "resolução fora da lista recusada");
  AE_EXPECT_TRUE(scene::setComponentProperty(components, "astra.render.reflection_probe", "box_projection", true) == S::Applied,
                 "Box Projection editável");
  AE_EXPECT_TRUE(scene::setComponentProperty(components, "astra.render.reflection_probe", "box_size.x", 4.0f) == S::Applied,
                 "Box Size editável");
  AE_EXPECT_TRUE(scene::findComponentMethod(scene::ReflectionProbe::descriptor, "render_probe") != nullptr, "RenderProbe declarado");
}

AE_TEST(probe_light_probe_group_points_keep_identity_and_round_trip) {
  scene::LightProbeGroup group;
  const float p[3]{2, 3, 4};
  const u64 id = group.addPoint(p);
  AE_EXPECT_TRUE(id == 9 && group.points.size() == 9, "nova sonda ganha identidade nova");
  AE_EXPECT_TRUE(group.removePoint(3) && !group.point(3), "apagar remove só aquela");
  const float q[3]{-1, 0, 1};
  AE_EXPECT_TRUE(group.movePoint(id, q) && group.point(id)->position[0] == -1, "mover");
  const float bad[3]{std::nanf(""), 0, 0};
  AE_EXPECT_TRUE(!group.movePoint(id, bad), "NaN recusado");
  group.removeRinging = true;
  group.bakedData = {5, 6};
  std::ostringstream out;
  group.write(out);
  scene::LightProbeGroup back;
  std::istringstream in(out.str());
  AE_EXPECT_TRUE(back.read(in, 1), "v1 lida");
  AE_EXPECT_TRUE(back.points == group.points && back.nextPointId == group.nextPointId && back.removeRinging && back.bakedData == group.bakedData,
                 "pontos, identidades e bake preservados");
  back.points.push_back(back.points.front());
  AE_EXPECT_TRUE(!back.valid(), "identidade repetida é inválida");
  const auto &collection = scene::LightProbeGroup::descriptor.collections[0];
  AE_EXPECT_EQ(collection.size(group), static_cast<u32>(group.points.size()), "coleção enumera as sondas");
  scene::Components components;
  auto *added = static_cast<scene::LightProbeGroup *>(components.add(scene::LightProbeGroup::descriptor));
  AE_EXPECT_TRUE(scene::setComponentSlotProperty(components, "astra.render.light_probe_group", "probe_position_y", 2, 7.5f,
                                                 added->instanceId()) == scene::ComponentPropertyStatus::Applied,
                 "posição da sonda por slot");
  const auto *written = static_cast<const scene::LightProbeGroup *>(components.find(scene::LightProbeGroup::descriptor));
  AE_EXPECT_EQ(written->points[2].position[1], 7.5f, "slot escreveu a sonda certa");
}

AE_TEST(probe_mesh_renderer_probe_usage_is_authorable_now) {
  scene::Components components;
  components.add(scene::MeshRenderer::descriptor);
  using S = scene::ComponentPropertyStatus;
  AE_EXPECT_TRUE(scene::setComponentProperty(components, "astra.render.mesh", "light_probes", u32{0}) == S::Applied,
                 "Light Probes Desligado editável");
  AE_EXPECT_TRUE(scene::setComponentProperty(components, "astra.render.mesh", "reflection_probes", u32{3}) == S::Applied,
                 "Reflection Probes Simples editável");
  const auto &mesh = static_cast<const scene::MeshRenderer &>(*components.find(scene::MeshRenderer::descriptor));
  AE_EXPECT_TRUE(mesh.lightProbes == 0 && mesh.reflectionProbes == 3, "valores gravados no renderer");
  AE_EXPECT_TRUE(ae::core::engineCapabilityAuthorable("render.probe.reflection") && core::engineCapabilityAuthorable("render.probe.irradiance"),
                 "capacidades de sonda implementadas");
}

// --- Extração e conjunto ----------------------------------------------------

AE_TEST(probe_scene_extraction_uses_world_box_and_hierarchy) {
  EditorDocument doc;
  const auto parent = doc.createEntity(doc.root(), EditorEntityKind::Folder, "Sala");
  auto parentValues = *doc.find(parent);
  parentValues.transform.position[0] = 10;
  parentValues.transform.rotationDegrees[1] = 90; // a caixa NÃO gira, como na Unity
  AE_EXPECT_TRUE(doc.applyEntityValues(parent, parentValues), "pai");
  const auto probeId = doc.createEntity(parent, EditorEntityKind::Folder, "Sonda");
  auto values = *doc.find(probeId);
  values.transform.position[1] = 2;
  auto *probe = static_cast<scene::ReflectionProbe *>(values.components.add(scene::ReflectionProbe::descriptor));
  probe->boxSize[0] = 4; probe->boxOffset[0] = 1; probe->background[0] = 1;
  AE_EXPECT_TRUE(doc.applyEntityValues(probeId, values), "sonda");
  std::vector<renderer::SceneReflectionProbe> probes;
  AE_EXPECT_TRUE(runtime::collectSceneReflectionProbes(doc, probes) && probes.size() == 1, "uma sonda");
  AE_EXPECT_TRUE(std::abs(probes[0].position[0] - 10) < 1e-4f && std::abs(probes[0].position[1] - 2) < 1e-4f, "posição de mundo");
  AE_EXPECT_TRUE(std::abs(probes[0].boxMin[0] - 9) < 1e-4f && std::abs(probes[0].boxMax[0] - 13) < 1e-4f,
                 "caixa alinhada ao mundo com deslocamento");
  AE_EXPECT_TRUE(probes[0].background[0] == 1 && probes[0].captureKey != 0, "fundo linear e chave de captura");
  parentValues.active = false;
  doc.applyEntityValues(parent, parentValues);
  AE_EXPECT_TRUE(runtime::collectSceneReflectionProbes(doc, probes) && probes.empty(), "pai inativo apaga a sonda");

  const auto groupId = doc.createEntity(doc.root(), EditorEntityKind::Folder, "Grupo");
  auto groupValues = *doc.find(groupId);
  groupValues.transform.position[2] = 5;
  groupValues.transform.scale[0] = 2;
  groupValues.components.add(scene::LightProbeGroup::descriptor);
  AE_EXPECT_TRUE(doc.applyEntityValues(groupId, groupValues), "grupo");
  std::vector<runtime::SceneLightProbeGroup> groups;
  AE_EXPECT_TRUE(runtime::collectSceneLightProbeGroups(doc, groups) && groups.size() == 1 && groups[0].positions.size() == 8, "grupo com 8 sondas");
  const auto &first = groups[0].positions[0];
  AE_EXPECT_TRUE(std::abs(first[0] + 1) < 1e-4f && std::abs(first[2] - 4.5f) < 1e-4f, "posições de mundo seguem escala e posição");
  const u64 hash = groups[0].positionsHash();
  groupValues.transform.position[2] = 6;
  doc.applyEntityValues(groupId, groupValues);
  runtime::collectSceneLightProbeGroups(doc, groups);
  AE_EXPECT_TRUE(groups[0].positionsHash() != hash, "mover o grupo deixa o bake desatualizado");
}

AE_TEST(probe_scene_light_probe_set_joins_groups_and_applies_remove_ringing) {
  runtime::SceneLightProbeGroup a, b;
  a.baked = {1, 1};
  b.baked = {2, 2};
  b.removeRinging = true;
  renderer::LightProbeBakeData bakeA, bakeB;
  for (u32 i = 0; i < 8; ++i) {
    bakeA.pointIds.push_back(i + 1);
    bakeA.positions.push_back({(i & 1) ? 1.f : 0.f, (i & 2) ? 1.f : 0.f, (i & 4) ? 1.f : 0.f});
    renderer::ShL2 sh;
    sh.c[0][0] = 1;
    bakeA.sh.push_back(sh);
  }
  bakeB = bakeA;
  for (auto &p : bakeB.positions) p[0] += 3;
  for (auto &sh : bakeB.sh) sh.c[3][0] = 5; // banda 1 forte: o anti-ringing reduz
  const runtime::LightProbeBakeLookup lookups[]{{a.baked, &bakeA}, {b.baked, &bakeB}};
  const runtime::SceneLightProbeGroup groups[]{a, b};
  renderer::LightProbeSet set;
  AE_EXPECT_TRUE(runtime::buildSceneLightProbeSet(groups, lookups, set), "monta");
  AE_EXPECT_EQ(set.positions.size(), usize{16}, "os dois grupos numa tetraedralização só");
  AE_EXPECT_TRUE(set.sh[8].c[3][0] < 5, "Remover anéis aplicado ao grupo que pediu");
  AE_EXPECT_TRUE(set.sh[0].c[0][0] == 1, "o outro grupo fica intacto");
  runtime::SceneLightProbeGroup unbaked;
  const runtime::SceneLightProbeGroup only[]{unbaked};
  AE_EXPECT_TRUE(runtime::buildSceneLightProbeSet(only, lookups, set) && set.empty(), "grupo sem bake não contribui");
}

// --- Editor -----------------------------------------------------------------

AE_TEST(probe_creation_menu_and_schema_follow_unity_light_menu) {
  bool reflection = false, light = false;
  for (const auto &recipe : editorCreationCatalog) {
    if (std::string_view(recipe.id) == "light.reflection_probe")
      reflection = recipe.category == 4 && !recipe.components.empty() && recipe.components[0].type == "astra.render.reflection_probe";
    if (std::string_view(recipe.id) == "light.light_probe_group")
      light = recipe.category == 4 && !recipe.components.empty() && recipe.components[0].type == "astra.render.light_probe_group";
  }
  AE_EXPECT_TRUE(reflection && light, "Criar › Luzes tem Sonda de reflexo e Grupo de sondas de luz");
  const auto *schema = scene::findComponentSchema("astra.render.reflection_probe");
  AE_EXPECT_TRUE(schema && schema->family == scene::ComponentFamily::Lighting && schema->icon == "lighting/reflection-probe",
                 "família Luz com ícone próprio");
  AE_EXPECT_TRUE(scene::findComponentSchema("astra.render.light_probe_group") != nullptr, "grupo registrado");
}

AE_TEST(probe_handles_resize_box_and_move_selected_probe) {
  EditorDocument doc;
  const auto probeId = doc.createEntity(doc.root(), EditorEntityKind::Folder, "Sonda");
  auto values = *doc.find(probeId);
  values.transform.position[0] = 3;
  values.transform.scale[0] = 4; // a caixa não escala com o objeto
  auto *probe = static_cast<scene::ReflectionProbe *>(values.components.add(scene::ReflectionProbe::descriptor));
  const u64 instance = probe->instanceId();
  AE_EXPECT_TRUE(doc.applyEntityValues(probeId, values), "sonda");
  EditorComponentHandle size, blend;
  AE_EXPECT_TRUE(componentHandleGeometry(doc, probeId, instance, EditorComponentHandleKind::BoxX, size) &&
                 componentHandleGeometry(doc, probeId, instance, EditorComponentHandleKind::BlendDistance, blend), "alças da caixa");
  AE_EXPECT_TRUE(std::abs(size.point[0] - 8) < 1e-4f, "alça na face +X da caixa de 10 m, sem a escala do objeto");
  auto changed = *doc.find(probeId);
  AE_EXPECT_TRUE(applyComponentHandleDelta(changed, size, 1), "arrasta a face");
  AE_EXPECT_EQ(static_cast<const scene::ReflectionProbe *>(changed.components.findInstance(instance))->boxSize[0], 12.f, "um metro na face é dois no tamanho");
  AE_EXPECT_TRUE(applyComponentHandleDelta(changed, blend, .5f), "arrasta a mistura");

  const auto groupId = doc.createEntity(doc.root(), EditorEntityKind::Folder, "Grupo");
  auto groupValues = *doc.find(groupId);
  auto *group = static_cast<scene::LightProbeGroup *>(groupValues.components.add(scene::LightProbeGroup::descriptor));
  const u64 groupInstance = group->instanceId();
  groupValues.transform.scale[1] = 2;
  AE_EXPECT_TRUE(doc.applyEntityValues(groupId, groupValues), "grupo");
  EditorComponentHandle moveY;
  AE_EXPECT_TRUE(!componentHandleGeometry(doc, groupId, groupInstance, EditorComponentHandleKind::ProbePointY, moveY),
                 "sem sonda selecionada não há alça");
  AE_EXPECT_TRUE(componentHandleGeometry(doc, groupId, groupInstance, EditorComponentHandleKind::ProbePointY, moveY, 1),
                 "alça da sonda selecionada");
  auto groupChanged = *doc.find(groupId);
  AE_EXPECT_TRUE(applyComponentHandleDelta(groupChanged, moveY, 2), "move a sonda");
  const auto *moved = static_cast<const scene::LightProbeGroup *>(groupChanged.components.findInstance(groupInstance))->point(1);
  AE_EXPECT_TRUE(moved && std::abs(moved->position[1] - .5f) < 1e-4f, "2 m de mundo em escala 2 é 1 m local");
}

AE_TEST(probe_gizmos_draw_box_and_tetrahedra_edges) {
  EditorDocument doc;
  const auto probeId = doc.createEntity(doc.root(), EditorEntityKind::Folder, "Sonda");
  auto values = *doc.find(probeId);
  values.components.add(scene::ReflectionProbe::descriptor);
  doc.applyEntityValues(probeId, values);
  const auto groupId = doc.createEntity(doc.root(), EditorEntityKind::Folder, "Grupo");
  auto groupValues = *doc.find(groupId);
  auto *group = static_cast<scene::LightProbeGroup *>(groupValues.components.add(scene::LightProbeGroup::descriptor));
  doc.applyEntityValues(groupId, groupValues);
  auto visuals = collectComponentVisuals(doc, probeId, 1.0f);
  const auto probeVisual = std::find_if(visuals.begin(), visuals.end(), [&](const auto &v) { return v.entity == probeId; });
  AE_EXPECT_TRUE(probeVisual != visuals.end() && probeVisual->icon == ui::UiIcon::LightingReflectionProbe, "marcador da sonda");
  usize emphasised = 0;
  for (const auto &s : probeVisual->segments) emphasised += s.emphasis == 1;
  AE_EXPECT_EQ(emphasised, usize{12}, "as 12 arestas da caixa em destaque, a de mistura por dentro");
  const u64 selected = group->points[3].id;
  visuals = collectComponentVisuals(doc, groupId, 1.0f, nullptr, 0, 0, 0, 0, selected);
  const auto groupVisual = std::find_if(visuals.begin(), visuals.end(), [&](const auto &v) { return v.entity == groupId; });
  AE_EXPECT_TRUE(groupVisual != visuals.end(), "grupo desenhado");
  const usize spheres = 8 * 2 * 48, highlight = 3 * 48;
  AE_EXPECT_TRUE(groupVisual->segments.size() > spheres + highlight, "esferas, arestas da tetraedralização e a selecionada");
  usize selectedSegments = 0;
  for (const auto &s : groupVisual->segments) selectedSegments += s.emphasis == 2;
  AE_EXPECT_EQ(selectedSegments, highlight, "a sonda selecionada em destaque");
}

AE_TEST(probe_render_probe_method_counts_requests_in_play) {
  runtime::SceneReflectionProbeRequests requests;
  requests.request(7, 1);
  requests.request(7, 1);
  requests.request(8, 2);
  std::vector<renderer::SceneReflectionProbe> probes(2);
  probes[0].objectId = 7; probes[0].instance = 1;
  probes[1].objectId = 9;
  requests.apply(probes);
  AE_EXPECT_TRUE(probes[0].renderRequests == 2 && probes[1].renderRequests == 0, "cada RenderProbe soma um na sonda certa");
  bool bound = false;
  for (const auto &binding : runtime::componentMethodBindings())
    if (binding.type == &scene::ReflectionProbe::descriptor && binding.method == "render_probe") bound = true;
  AE_EXPECT_TRUE(bound, "RenderProbe tem implementação no runtime");
}

AE_TEST(probe_editor_bake_cycle_writes_resources_and_undoable_links) {
  EditorSession session;
  session.initialize(&probeFont(), &probeIcons());
  session.setSurface({0, 0, 853, 394}, {});
  auto &document = session.document();
  auto &history = session.history();
  const auto groupId = history.createEntity(document, document.root(), EditorEntityKind::Folder, "Grupo");
  auto values = *document.find(groupId);
  auto *group = static_cast<scene::LightProbeGroup *>(values.components.add(scene::LightProbeGroup::descriptor));
  const u64 instance = group->instanceId();
  AE_EXPECT_TRUE(document.applyEntityValues(groupId, values), "grupo autorado");
  session.probeViewForEdit().selectedPoint = group->points[0].id;
  AE_EXPECT_TRUE(session.editLightProbePoints(groupId, instance, 0), "adicionar sonda");
  const auto *after = static_cast<const scene::LightProbeGroup *>(document.find(groupId)->components.findInstance(instance));
  AE_EXPECT_EQ(after->points.size(), usize{9}, "nove sondas");
  AE_EXPECT_TRUE(after->point(session.probeView().selectedPoint)->position[0] == group->points[0].position[0] + 1,
                 "a nova nasce um metro ao lado e fica selecionada");
  AE_EXPECT_TRUE(session.editLightProbePoints(groupId, instance, 2), "apagar a selecionada");
  AE_EXPECT_EQ(static_cast<const scene::LightProbeGroup *>(document.find(groupId)->components.findInstance(instance))->points.size(),
               usize{8}, "volta a oito");
  // Sem projeto aberto o bake é recusado com motivo (não grava nada no vazio).
  AE_EXPECT_TRUE(!session.bakeLightProbes(), "sem projeto não assa");
}

AE_TEST(probe_components_survive_the_scene_archive_with_identity) {
  EditorDocument doc;
  const auto id = doc.createEntity(doc.root(), EditorEntityKind::Folder, "Sondas");
  auto values = *doc.find(id);
  auto *probe = static_cast<scene::ReflectionProbe *>(values.components.add(scene::ReflectionProbe::descriptor));
  probe->boxSize[0] = 23; probe->boxOffset[1] = 4.4f; probe->boxProjection = true;
  auto *group = static_cast<scene::LightProbeGroup *>(values.components.add(scene::LightProbeGroup::descriptor));
  const float p[3]{1.25f, 2, -3};
  group->addPoint(p);
  const u64 probeInstance = probe->instanceId(), groupInstance = group->instanceId();
  AE_EXPECT_TRUE(doc.applyEntityValues(id, values), "autorado");
  const auto text = serializeEditorDocument(doc, 0);
  EditorDocument back;
  AE_EXPECT_TRUE(deserializeEditorDocument(text, 0, back), "a cena com sondas volta pelo leitor");
  const auto *entity = back.find(id);
  const auto *readProbe = entity ? entity->components.findInstance(probeInstance) : nullptr;
  const auto *readGroup = entity ? entity->components.findInstance(groupInstance) : nullptr;
  AE_EXPECT_TRUE(readProbe && readGroup, "identidade das instâncias preservada");
  AE_EXPECT_TRUE(readProbe && static_cast<const scene::ReflectionProbe *>(readProbe)->boxSize[0] == 23 &&
                 static_cast<const scene::ReflectionProbe *>(readProbe)->boxProjection, "campos da sonda");
  AE_EXPECT_TRUE(readGroup && static_cast<const scene::LightProbeGroup *>(readGroup)->points.size() == 9, "pontos do grupo");
  AE_EXPECT_TRUE(serializeEditorDocument(back, 0) == text, "ida e volta idêntica");
}

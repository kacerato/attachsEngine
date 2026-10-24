// G3c: lightmap assado (Generate Lighting da Unity 6) e UV2 automática.
// Cada peça com consumidor real é exercitada aqui: o desembrulho do xatlas no
// formato do pacote, o perfil de importação, o recurso .aelightmap, o baker
// contra resultados analíticos (lei do inverso do quadrado, sombra, céu sob
// um telhado, modo Misto x Assado, emissivo), o cancelamento, o filtro que tira
// a luz Assada dos desenhos com lightmap e a leitura das versões antigas.
#include "harness.h"
#include "core/engine_capability.h"
#include "renderer/lightmap_baker.h"
#include "renderer/lightmap_data.h"
#include "renderer/punctual_lights.h"
#include "resources/import_profile.h"
#include "resources/lightmap_uv_generate.h"
#include "scene/light.h"
#include "scene/lighting_settings.h"
#include "scene/lightmap_uv.h"
#include "scene/mesh_renderer.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

using namespace ae;

namespace {
// Quadrado no plano XZ (normal +Y), de -h a +h, com UV2 cobrindo [0,1].
renderer::LightmapBakeMesh quad(float half, bool upward = true) {
  renderer::LightmapBakeMesh mesh;
  mesh.positions = {-half, 0, -half, half, 0, -half, half, 0, half, -half, 0, half};
  const float ny = upward ? 1.0f : -1.0f;
  mesh.normals = {0, ny, 0, 0, ny, 0, 0, ny, 0, 0, ny, 0};
  mesh.uv0 = {0, 0, 1, 0, 1, 1, 0, 1};
  mesh.uv1 = mesh.uv0;
  // Anti-horário visto de cima (+Y) para a face de cima; o contrário para baixo.
  mesh.indices = upward ? std::vector<u32>{0, 2, 1, 0, 3, 2} : std::vector<u32>{0, 1, 2, 0, 2, 3};
  mesh.uv1Valid = true;
  return mesh;
}
renderer::LightmapBakeInstance placed(u32 mesh, float x, float y, float z, bool receives) {
  renderer::LightmapBakeInstance instance;
  instance.objectId = 100 + mesh;
  instance.mesh = mesh;
  instance.material = 0;
  instance.model[12] = x; instance.model[13] = y; instance.model[14] = z;
  instance.receives = receives;
  return instance;
}
renderer::LightmapBakeSettings quickSettings() {
  renderer::LightmapBakeSettings settings;
  settings.texelsPerUnit = 8;
  settings.padding = 2;
  settings.maxSize = 256;
  settings.indirectSamples = 64;
  settings.environmentSamples = 64;
  settings.directSamples = 8;
  settings.bounces = 1;
  settings.denoise = false;
  settings.compress = false;
  return settings;
}
// Lê o texel do lightmap mais perto da UV2 (u,v) de uma entrada.
bool sampleEntry(const renderer::LightmapData &data, u64 objectId, float u, float v, float rgb[3], float &visibility) {
  for (const auto &entry : data.entries) {
    if (entry.objectId != objectId) continue;
    const auto &page = data.pages[entry.page];
    const u32 x = static_cast<u32>((u * entry.scaleOffset[0] + entry.scaleOffset[2]) * page.width);
    const u32 y = static_cast<u32>((v * entry.scaleOffset[1] + entry.scaleOffset[3]) * page.height);
    renderer::decodeLightmapTexel(page, x, y, rgb, visibility);
    return true;
  }
  return false;
}
// Cubo no formato do pacote (vértice de 48 bytes), 24 vértices e UV1 nula.
void packedCube(std::vector<u8> &vertices, std::vector<u32> &indices) {
  const float p[8][3]{{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}};
  const u32 faces[6][4]{{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 4, 7, 3}, {1, 2, 6, 5}, {3, 7, 6, 2}, {0, 1, 5, 4}};
  const float normals[6][3]{{0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
  for (u32 f = 0; f < 6; ++f) {
    const u32 base = static_cast<u32>(vertices.size() / renderer::MapVertexStride);
    for (u32 c = 0; c < 4; ++c) {
      u8 v[renderer::MapVertexStride]{};
      std::memcpy(v, p[faces[f][c]], 12);
      i16 n[4]{};
      for (u32 k = 0; k < 3; ++k) n[k] = static_cast<i16>(normals[f][k] * 32767);
      std::memcpy(v + 12, n, 8);
      const float uv[2]{c == 1 || c == 2 ? 1.0f : 0.0f, c >= 2 ? 1.0f : 0.0f};
      std::memcpy(v + 28, uv, 8);
      vertices.insert(vertices.end(), v, v + renderer::MapVertexStride);
    }
    for (u32 k : {0u, 1u, 2u, 0u, 2u, 3u}) indices.push_back(base + k);
  }
}
std::vector<scene::LightmapUvTriangle> uvTriangles(const std::vector<u8> &vertices, const std::vector<u32> &indices,
                                                   const renderer::MapDrawRecord &draw) {
  std::vector<scene::LightmapUvTriangle> out(draw.indexCount / 3);
  for (u32 i = 0; i < draw.indexCount; ++i) {
    const usize vertex = usize(draw.vertexOffset) + indices[draw.firstIndex + i];
    std::memcpy(out[i / 3].data() + (i % 3) * 2, vertices.data() + vertex * renderer::MapVertexStride + 36, 8);
  }
  return out;
}
} // namespace

AE_TEST(g3c_generate_lightmap_uvs_gives_cube_non_overlapping_uv2) {
  std::vector<u8> vertices;
  std::vector<u32> indices;
  packedCube(vertices, indices);
  packedCube(vertices, indices);
  // Três desenhos: o mesmo cubo duas vezes (faixa compartilhada) e um segundo cubo deformado.
  std::vector<renderer::MapDrawRecord> draws(3);
  for (auto &d : draws) { std::memset(&d, 0, sizeof d); d.indexCount = 36; }
  draws[2].firstIndex = 36;
  const std::vector<u8> deformed{0, 0, 1};
  AE_EXPECT_TRUE(scene::auditLightmapUv(uvTriangles(vertices, indices, draws[0])) != scene::LightmapUvStatus::Valid,
                 "antes: UV1 nula é recusada");
  resources::LightmapUvSettings settings;
  resources::LightmapUvReport report;
  std::string diagnostic;
  auto keptVertices = vertices;
  AE_EXPECT_TRUE(resources::generateLightmapUvs(vertices, indices, draws, deformed, settings, false, report, diagnostic), diagnostic.c_str());
  AE_EXPECT_EQ(report.drawsUnwrapped, 1u, "a faixa compartilhada é desembrulhada uma vez");
  AE_EXPECT_EQ(report.drawsDeformed, 1u, "o desenho deformado fica de fora");
  AE_EXPECT_TRUE(report.charts >= 1, "ilhas geradas");
  AE_EXPECT_EQ(scene::auditLightmapUv(uvTriangles(vertices, indices, draws[0])), scene::LightmapUvStatus::Valid,
               "depois: UV2 sem sobreposição, dentro de 0..1");
  AE_EXPECT_TRUE(draws[0].firstIndex == draws[1].firstIndex && draws[0].vertexOffset == draws[1].vertexOffset,
                 "os dois desenhos da mesma faixa continuam juntos");
  // Sem compactar, o buffer antigo fica intacto e o deformado aponta para ele.
  AE_EXPECT_TRUE(std::memcmp(vertices.data(), keptVertices.data(), keptVertices.size()) == 0, "vértices antigos preservados");
  AE_EXPECT_EQ(draws[2].vertexOffset, 0u, "desenho deformado no lugar");
  // Ângulo vivo de 88°: o cubo (90°) vira pelo menos seis ilhas.
  AE_EXPECT_TRUE(report.charts >= 6, "Hard Angle 88° corta as quinas do cubo");
}

AE_TEST(g3c_generate_lightmap_uvs_compacts_static_models) {
  std::vector<u8> vertices;
  std::vector<u32> indices;
  packedCube(vertices, indices);
  std::vector<renderer::MapDrawRecord> draws(1);
  std::memset(draws.data(), 0, sizeof(renderer::MapDrawRecord));
  draws[0].indexCount = 36;
  resources::LightmapUvReport report;
  std::string diagnostic;
  AE_EXPECT_TRUE(resources::generateLightmapUvs(vertices, indices, draws, {}, {}, true, report, diagnostic), diagnostic.c_str());
  AE_EXPECT_TRUE(report.verticesAfter <= 36 && report.verticesAfter >= 24, "compactado: só os vértices da malha nova");
  AE_EXPECT_EQ(vertices.size() / renderer::MapVertexStride, report.verticesAfter, "buffer do tamanho relatado");
  AE_EXPECT_EQ(draws[0].vertexOffset, 0u, "faixa no começo");
  resources::LightmapUvSettings bad;
  bad.packMarginPixels = 0;
  AE_EXPECT_TRUE(!resources::generateLightmapUvs(vertices, indices, draws, {}, bad, true, report, diagnostic), "margem 0 recusada");
}

AE_TEST(g3c_import_profile_round_trips_lightmap_uv_choice) {
  resources::ImportProfile profile;
  profile.generateLightmapUvs = true;
  profile.lightmapHardAngle = 60;
  profile.lightmapPackMargin = 8;
  resources::ImportProfile back;
  AE_EXPECT_TRUE(resources::parseImportProfile(resources::serializeImportProfile(profile), back), "perfil lido");
  AE_EXPECT_TRUE(back.generateLightmapUvs && back.lightmapHardAngle == 60 && back.lightmapPackMargin == 8, "UV de lightmap preservado");
  AE_EXPECT_TRUE(!resources::sameImportPreparation(profile, resources::ImportProfile{}), "ligar muda a preparação");
  const auto limits = resources::applyImportProfile({}, profile);
  AE_EXPECT_TRUE(limits.generateLightmapUvs && limits.lightmapHardAngle == 60 && limits.lightmapPackMargin == 8, "limites recebem a escolha");
  // Perfil gravado antes do campo (schema 12) abre desligado.
  auto text = resources::serializeImportProfile(resources::ImportProfile{});
  const auto at = text.find("\"schema\":13");
  AE_EXPECT_TRUE(at != std::string::npos, "schema 13 gravado");
  text.replace(at, 11, "\"schema\":12");
  AE_EXPECT_TRUE(resources::parseImportProfile(text, back) && !back.generateLightmapUvs, "schema 12 abre sem UV de lightmap");
}

AE_TEST(g3c_lightmap_resource_round_trips_both_formats) {
  for (bool compressed : {false, true}) {
    renderer::LightmapData data;
    data.guidHigh = 7; data.guidLow = 8; data.sourceHash = 99; data.bakeSeconds = 3.5f;
    data.settings.compress = compressed;
    renderer::LightmapPage page;
    page.width = page.height = 4;
    page.format = compressed ? renderer::LightmapPage::FormatSqrtRgba8 : renderer::LightmapPage::FormatHalf;
    page.range = compressed ? 2.0f : 1.0f;
    page.texels.assign(page.expectedBytes(), 0);
    if (compressed) { page.texels[0] = 255; page.texels[3] = 128; }
    else {
      const u16 h[4]{renderer::floatToHalf(1.5f), renderer::floatToHalf(.25f), 0, renderer::floatToHalf(.75f)};
      std::memcpy(page.texels.data(), h, 8);
    }
    data.pages = {page, page};
    data.entries = {{42, 1, 1, {.5f, .5f, .25f, .25f}}};
    const auto bytes = data.serialize();
    renderer::LightmapData back;
    std::string error;
    AE_EXPECT_TRUE(renderer::LightmapData::deserialize(bytes, back, &error), error.c_str());
    AE_EXPECT_TRUE(back.pages.size() == 2 && back.entries.size() == 1 && back.entries[0].objectId == 42 && back.entries[0].slot == 1 &&
                   back.sourceHash == 99 && back.settings.compress == compressed, "recurso idêntico");
    float rgb[3], v;
    renderer::decodeLightmapTexel(back.pages[0], 0, 0, rgb, v);
    if (compressed) AE_EXPECT_TRUE(std::abs(rgb[0] - 2.0f) < 1e-3f && std::abs(v - .252f) < .01f, "raiz quadrada e faixa decodificadas");
    else AE_EXPECT_TRUE(std::abs(rgb[0] - 1.5f) < 1e-3f && std::abs(v - .75f) < 1e-3f, "meia precisão decodificada");
    renderer::LightmapLookup lookup(std::make_shared<renderer::LightmapData>(back));
    AE_EXPECT_TRUE(lookup.find(42, 1) && !lookup.find(42, 0) && !lookup.find(43, 1), "consulta por objeto e slot");
    auto broken = bytes;
    broken.pop_back();
    AE_EXPECT_TRUE(!renderer::LightmapData::deserialize(broken, back), "truncado recusado");
  }
}

AE_TEST(g3c_baked_point_light_matches_inverse_square_and_shadows) {
  renderer::LightmapBakeScene scene;
  scene.meshes.push_back(quad(4));
  scene.meshes.push_back(quad(.6f, false)); // bloqueador virado para baixo
  scene.materials.resize(1);
  scene.materials[0].baseColor[0] = scene.materials[0].baseColor[1] = scene.materials[0].baseColor[2] = 0; // sem rebote
  scene.instances.push_back(placed(0, 0, 0, 0, true));
  renderer::LightmapBakeLight light;
  light.kind = 1;
  light.position[0] = 0; light.position[1] = 2; light.position[2] = 0;
  light.radiance[0] = light.radiance[1] = light.radiance[2] = 10;
  light.range = 100;
  light.bakeDirect = true;
  scene.lights.push_back(light);
  renderer::LightmapBakeProgress progress;
  renderer::LightmapData data;
  std::string error;
  auto settings = quickSettings();
  settings.bounces = 0;
  AE_EXPECT_TRUE(renderer::bakeLightmaps(scene, settings, {2, false}, progress, data, error), error.c_str());
  float rgb[3], v;
  // Centro: E = I / d² · janela (alcance 100 m: ~1).
  AE_EXPECT_TRUE(sampleEntry(data, 100, .5f, .5f, rgb, v), "entrada do chão");
  AE_EXPECT_TRUE(std::abs(rgb[1] - 2.5f) < .2f, "centro: 10 / 2² = 2,5");
  // A 2 m do centro: d² = 8, cosseno 2/√8 → E = 10·0,707/8 = 0,884.
  AE_EXPECT_TRUE(sampleEntry(data, 100, .75f, .5f, rgb, v), "entrada do chão");
  AE_EXPECT_TRUE(std::abs(rgb[1] - .884f) < .12f, "lei do inverso do quadrado com cosseno");
  AE_EXPECT_TRUE(v > .9f, "chão aberto vê o céu inteiro");
  AE_EXPECT_TRUE(data.stats.receivers == 1 && data.entries.size() == 1, "um receptor");
  // Com um telhado entre a luz e o centro, o centro fica escuro.
  scene.instances.push_back(placed(1, 0, 1, 0, false));
  AE_EXPECT_TRUE(renderer::bakeLightmaps(scene, settings, {2, false}, progress, data, error), error.c_str());
  AE_EXPECT_TRUE(sampleEntry(data, 100, .5f, .5f, rgb, v) && rgb[1] < .05f, "nenhum ponto acende sob a sombra");
  AE_EXPECT_TRUE(sampleEntry(data, 100, .95f, .5f, rgb, v) && rgb[1] > .1f, "fora da sombra continua iluminado");
}

AE_TEST(g3c_sky_visibility_drops_under_a_roof_and_mixed_bakes_only_bounce) {
  renderer::LightmapBakeScene scene;
  scene.meshes.push_back(quad(3));
  scene.meshes.push_back(quad(2.5f, false));
  scene.materials.resize(1);
  for (u32 k = 0; k < 3; ++k) scene.materials[0].baseColor[k] = .5f;
  scene.instances.push_back(placed(0, 0, 0, 0, true));
  scene.instances.push_back(placed(1, 0, .5f, 0, false)); // telhado baixo: um beiral
  renderer::LightmapBakeProgress progress;
  renderer::LightmapData data;
  std::string error;
  auto settings = quickSettings();
  settings.bounces = 2;
  AE_EXPECT_TRUE(renderer::bakeLightmaps(scene, settings, {2, false}, progress, data, error), error.c_str());
  float rgb[3], v;
  AE_EXPECT_TRUE(sampleEntry(data, 100, .5f, .5f, rgb, v), "centro");
  std::printf("  ceu sob o telhado V=%.3f\n", static_cast<double>(v));
  AE_EXPECT_TRUE(v < .25f, "debaixo do telhado o céu quase não chega");
  float edge[3], vEdge;
  AE_EXPECT_TRUE(sampleEntry(data, 100, .99f, .5f, edge, vEdge) && vEdge > v + .15f, "na borda o céu volta");
  AE_EXPECT_TRUE(rgb[0] < 1e-4f, "sem luz local assada, o RGB é zero (o sol nunca entra)");
  // Luz Mista sob o telhado: a direta não entra no lightmap, o rebote entra.
  renderer::LightmapBakeLight light;
  light.kind = 1;
  light.position[1] = .3f;
  light.radiance[0] = light.radiance[1] = light.radiance[2] = 4;
  light.range = 20;
  light.bakeDirect = false;
  scene.lights.push_back(light);
  AE_EXPECT_TRUE(renderer::bakeLightmaps(scene, settings, {2, false}, progress, data, error), error.c_str());
  float mixed[3];
  AE_EXPECT_TRUE(sampleEntry(data, 100, .5f, .5f, mixed, v), "centro com luz mista");
  const float directOnly = 4.0f / (.3f * .3f);
  AE_EXPECT_TRUE(mixed[1] > .01f && mixed[1] < directOnly * .5f, "Misto: só o rebote no lightmap");
  scene.lights[0].bakeDirect = true;
  AE_EXPECT_TRUE(renderer::bakeLightmaps(scene, settings, {2, false}, progress, data, error), error.c_str());
  float baked[3];
  AE_EXPECT_TRUE(sampleEntry(data, 100, .5f, .5f, baked, v) && baked[1] > mixed[1] + directOnly * .5f, "Assado: direta + rebote");
}

AE_TEST(g3c_mirrored_instance_keeps_its_front_face_and_sky) {
  // Escala negativa num eixo (o glTF do Sponza espelha nós): a frente continua
  // sendo a da normal; sem a correção, o chão espelhado via o próprio verso.
  renderer::LightmapBakeScene scene;
  scene.meshes.push_back(quad(3));
  scene.materials.resize(1);
  auto floor = placed(0, 0, 0, 0, true);
  floor.model[0] = -1; // espelho em X: inverte o enrolamento, a normal continua +Y
  scene.instances.push_back(floor);
  renderer::LightmapBakeProgress progress;
  renderer::LightmapData data;
  std::string error;
  AE_EXPECT_TRUE(renderer::bakeLightmaps(scene, quickSettings(), {2, false}, progress, data, error), error.c_str());
  float rgb[3], v;
  AE_EXPECT_TRUE(sampleEntry(data, 100, .5f, .5f, rgb, v) && v > .9f, "chão espelhado aberto vê o céu inteiro");
  float distance = 0;
  u32 hitInstance = 0;
  renderer::LightmapBakeRay down{{0, 2, 0}, {0, -1, 0}, 10};
  AE_EXPECT_TRUE(renderer::lightmapDebugTrace(scene, down, distance, hitInstance) && std::abs(distance - 2) < 1e-3f, "o raio acerta o chão");
}

AE_TEST(g3c_rotated_instance_uses_inverse_transpose_normals) {
  // Chão girado 90° em X: a normal +Y vira +Z. Uma parede logo à frente (+Z)
  // tampa o hemisfério dele; com a normal errada (−Z) ele veria o céu aberto.
  renderer::LightmapBakeScene scene;
  scene.meshes.push_back(quad(2));
  renderer::LightmapBakeMesh wall;
  wall.positions = {-10, -10, 0, 10, -10, 0, 10, 10, 0, -10, 10, 0};
  wall.normals = {0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1};
  wall.uv0 = wall.uv1 = {0, 0, 1, 0, 1, 1, 0, 1};
  wall.indices = {0, 2, 1, 0, 3, 2};
  scene.meshes.push_back(wall);
  scene.materials.resize(1);
  auto floor = placed(0, 0, 0, 0, true);
  // Coluna principal: rotação de +90° em X (y -> z, z -> -y).
  floor.model[5] = 0; floor.model[6] = 1; floor.model[9] = -1; floor.model[10] = 0;
  scene.instances.push_back(floor);
  scene.instances.push_back(placed(1, 0, 0, .3f, false));
  renderer::LightmapBakeProgress progress;
  renderer::LightmapData data;
  std::string error;
  AE_EXPECT_TRUE(renderer::bakeLightmaps(scene, quickSettings(), {2, false}, progress, data, error), error.c_str());
  float rgb[3], v;
  AE_EXPECT_TRUE(sampleEntry(data, 100, .5f, .5f, rgb, v), "entrada do chão girado");
  std::printf("  chao girado V=%.3f\n", static_cast<double>(v));
  AE_EXPECT_TRUE(v < .4f, "a parede à frente tapa o céu do chão girado (normal errada daria ~1)");
}

AE_TEST(g3c_winding_against_vertex_normals_still_faces_the_sky) {
  // Enrolamento ao contrário da normal (o Sponza mistura): a frente é a normal.
  renderer::LightmapBakeScene scene;
  auto mesh = quad(3);
  std::swap(mesh.indices[1], mesh.indices[2]);
  std::swap(mesh.indices[4], mesh.indices[5]);
  scene.meshes.push_back(mesh);
  scene.materials.resize(1);
  scene.instances.push_back(placed(0, 0, 0, 0, true));
  renderer::LightmapBakeProgress progress;
  renderer::LightmapData data;
  std::string error;
  AE_EXPECT_TRUE(renderer::bakeLightmaps(scene, quickSettings(), {2, false}, progress, data, error), error.c_str());
  float rgb[3], v;
  AE_EXPECT_TRUE(sampleEntry(data, 100, .5f, .5f, rgb, v) && v > .9f, "chão com enrolamento invertido vê o céu");
}

AE_TEST(g3c_double_sided_closed_mesh_with_inward_normals_receives_from_outside) {
  // A copa das árvores da Vila: caixa fechada com enrolamento e normais para
  // dentro, material double-sided. O texel recebe pelo lado de fora.
  renderer::LightmapBakeMesh box;
  const float h = 1;
  const float corners[6][4][3] = {
    {{-h, h, -h}, {h, h, -h}, {h, h, h}, {-h, h, h}},     // topo
    {{-h, -h, -h}, {-h, -h, h}, {h, -h, h}, {h, -h, -h}}, // base
    {{-h, -h, h}, {-h, h, h}, {h, h, h}, {h, -h, h}},     // +Z
    {{-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h}}, // -Z
    {{h, -h, -h}, {h, -h, h}, {h, h, h}, {h, h, -h}},     // +X
    {{-h, -h, -h}, {-h, h, -h}, {-h, h, h}, {-h, -h, h}}, // -X
  };
  for (u32 f = 0; f < 6; ++f) {
    const u32 base = static_cast<u32>(box.positions.size() / 3);
    float center[3]{};
    for (u32 k = 0; k < 4; ++k) for (u32 a = 0; a < 3; ++a) center[a] += corners[f][k][a] * .25f;
    const float tileU = (f % 3) / 3.0f, tileV = (f / 3) / 2.0f;
    const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (u32 k = 0; k < 4; ++k) {
      for (u32 a = 0; a < 3; ++a) { box.positions.push_back(corners[f][k][a]); box.normals.push_back(-center[a] / h); }
      box.uv0.push_back(uv[k][0]); box.uv0.push_back(uv[k][1]);
      box.uv1.push_back(tileU + (.05f + uv[k][0] * .9f) / 3); box.uv1.push_back(tileV + (.05f + uv[k][1] * .9f) / 2);
    }
    // Enrolamento para dentro, como as normais (0,2,1 seria visto de fora).
    for (u32 k : {0u, 1u, 2u, 0u, 2u, 3u}) box.indices.push_back(base + k);
  }
  box.uv1Valid = true;
  const auto topSky = [&](bool doubleSided) {
    renderer::LightmapBakeScene scene;
    scene.meshes.push_back(box);
    scene.materials.resize(1);
    scene.materials[0].doubleSided = doubleSided;
    scene.instances.push_back(placed(0, 0, 3, 0, true));
    renderer::LightmapBakeProgress progress;
    renderer::LightmapData data;
    std::string error;
    float rgb[3], v = -1;
    if (!renderer::bakeLightmaps(scene, quickSettings(), {2, false}, progress, data, error) ||
        !sampleEntry(data, 100, (.05f + .5f * .9f) / 3, (.05f + .5f * .9f) / 2, rgb, v)) return -1.0f;
    return v;
  };
  const float twoSided = topSky(true);
  AE_EXPECT_TRUE(twoSided > .5f, "copa double-sided com normais para dentro vê o céu por fora");
  AE_EXPECT_TRUE(topSky(false) < .05f, "single-sided continua recebendo pelo lado da normal");
}

AE_TEST(g3c_emissive_surface_lights_neighbours_and_cancel_stops_the_bake) {
  renderer::LightmapBakeScene scene;
  scene.meshes.push_back(quad(2));
  scene.meshes.push_back(quad(2, false));
  scene.materials.resize(2);
  for (u32 k = 0; k < 3; ++k) { scene.materials[0].baseColor[k] = .5f; scene.materials[1].baseColor[k] = 0; scene.materials[1].emission[k] = 3; }
  scene.instances.push_back(placed(0, 0, 0, 0, true));
  auto ceiling = placed(1, 0, 1, 0, false);
  ceiling.material = 1;
  scene.instances.push_back(ceiling);
  renderer::LightmapBakeProgress progress;
  renderer::LightmapData data;
  std::string error;
  AE_EXPECT_TRUE(renderer::bakeLightmaps(scene, quickSettings(), {2, false}, progress, data, error), error.c_str());
  float rgb[3], v;
  AE_EXPECT_TRUE(sampleEntry(data, 100, .5f, .5f, rgb, v) && rgb[0] > 1.0f, "teto emissivo ilumina o chão");
  renderer::LightmapBakeProgress cancelled;
  cancelled.cancel = true;
  AE_EXPECT_TRUE(!renderer::bakeLightmaps(scene, quickSettings(), {2, false}, cancelled, data, error), "cancelado");
  AE_EXPECT_TRUE(error.find("cancelado") != std::string::npos, "o motivo é o cancelamento");
  // Hash muda com a pose: o recurso fica desatualizado quando a cena muda.
  const auto before = renderer::lightmapSceneHash(scene, quickSettings());
  scene.instances[1].model[13] = 1.5f;
  AE_EXPECT_TRUE(renderer::lightmapSceneHash(scene, quickSettings()) != before, "mover o teto marca o bake como velho");
}

AE_TEST(g3c_baked_lights_leave_realtime_only_on_lightmapped_draws) {
  renderer::PunctualLightFilter baked;
  baked.baked = true;
  renderer::DrawLightingKey dynamicKey, staticKey;
  staticKey.lightmapPage = 1;
  AE_EXPECT_TRUE(renderer::lightFilterAccepts(baked, dynamicKey), "objeto móvel continua com a luz Assada");
  AE_EXPECT_TRUE(!renderer::lightFilterAccepts(baked, staticKey), "com lightmap, a luz Assada sai do tempo real");
  AE_EXPECT_TRUE(renderer::lightFilterAccepts(renderer::PunctualLightFilter{}, staticKey), "Tempo real e Misto continuam");
  // O Modo virou propriedade com consumidor.
  bool found = false;
  for (const auto &e : scene::lightEnums) found = found || e.id == "mode";
  AE_EXPECT_TRUE(found, "Light › Modo exposto");
  const auto *capability = ae::core::findEngineCapability("render.light.baked");
  AE_EXPECT_TRUE(capability && capability->state == ae::core::CapabilityState::Implemented, "render.light.baked implementado");
}

AE_TEST(g3c_mesh_renderer_v12_and_lighting_settings_v2_migrate) {
  scene::MeshRenderer renderer;
  renderer.contributeGlobalIllumination = false;
  renderer.receiveGlobalIllumination = 1;
  std::stringstream out;
  renderer.write(out);
  scene::MeshRenderer back;
  std::stringstream in(out.str());
  AE_EXPECT_TRUE(back.read(in, 12), "v12 lido");
  AE_EXPECT_TRUE(!back.contributeGlobalIllumination && back.receiveGlobalIllumination == 1 && !back.receivesLightmaps(), "GI preservada");
  // Uma linha v11 é a v12 sem os dois campos finais.
  auto text = out.str();
  while (!text.empty() && text.back() == ' ') text.pop_back();
  text = text.substr(0, text.rfind(' '));
  text = text.substr(0, text.rfind(' '));
  std::stringstream legacy(text + ' ');
  scene::MeshRenderer old;
  AE_EXPECT_TRUE(old.read(legacy, 11), "v11 lido");
  AE_EXPECT_TRUE(old.contributeGlobalIllumination && old.receiveGlobalIllumination == 0, "v11: contribui e recebe lightmap");
  scene::LightingSettings settings;
  settings.lightmapResolution = 12;
  settings.bounces = 3;
  settings.lightingData = {5, 6};
  std::stringstream sout;
  settings.write(sout);
  scene::LightingSettings sback;
  std::stringstream sin(sout.str());
  AE_EXPECT_TRUE(sback.read(sin, 2) && sback.lightmapResolution == 12 && sback.bounces == 3 && (sback.lightingData == resources::AssetGuid{5, 6}),
                 "Configurações v2 idênticas");
  std::stringstream v1("77");
  scene::LightingSettings fromV1;
  AE_EXPECT_TRUE(fromV1.read(v1, 1) && fromV1.sunSource == 77 && fromV1.lightmapResolution == 40 && fromV1.maxLightmapSize == 2,
                 "v1 abre com os padrões da Unity (40 texels/m, 1024 px)");
  AE_EXPECT_EQ(std::string(resources::assetTypeName(resources::AssetType::Lightmap)), std::string("lightmap"), "tipo de recurso registrado");
}

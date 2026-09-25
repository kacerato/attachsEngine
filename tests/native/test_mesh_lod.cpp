// LOD na própria malha e ordem de índices (S3): níveis com ~metade dos índices
// sobre os mesmos vértices, erro por nível não decrescente, fontes pequenas ou
// deformadas sem LOD, ACMR medido, e o resultado atravessa cache e perfil.
#include "harness.h"
#include "renderer/map_package.h"
#include "resources/gltf_import.h"
#include "resources/import_cache.h"
#include "resources/import_profile.h"
#include "resources/mesh_lod_build.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace ae;
using namespace ae::resources;

namespace {
// Grade ondulada de (cells+1)² vértices: tem erro real ao simplificar.
struct Grid {
  std::vector<u8> vertices;
  std::vector<u32> indices;
  renderer::MapDrawRecord draw{};
};
float height(u32 x, u32 z) { return .35f * std::sin(x * .45f) * std::cos(z * .31f); }
Grid grid(u32 cells) {
  Grid g;
  struct Vertex {float position[3];i16 normal[4];i16 tangent[4];float uv[2];float uv1[2];u32 color;};
  static_assert(sizeof(Vertex) == renderer::MapVertexStride);
  for (u32 z = 0; z <= cells; ++z)
    for (u32 x = 0; x <= cells; ++x) {
      Vertex v{};
      v.position[0] = static_cast<float>(x);
      v.position[1] = height(x, z);
      v.position[2] = static_cast<float>(z);
      v.normal[1] = 32767;
      v.uv[0] = x / static_cast<float>(cells);
      v.uv[1] = z / static_cast<float>(cells);
      const usize at = g.vertices.size();
      g.vertices.resize(at + sizeof v);
      std::memcpy(g.vertices.data() + at, &v, sizeof v);
    }
  const u32 row = cells + 1;
  for (u32 z = 0; z < cells; ++z)
    for (u32 x = 0; x < cells; ++x) {
      const u32 a = z * row + x, b = a + 1, c = a + row, d = c + 1;
      g.indices.insert(g.indices.end(), {a, c, b, b, c, d});
    }
  g.draw.indexCount = static_cast<u32>(g.indices.size());
  return g;
}

void appendU32(std::vector<u8> &out, u32 value) {
  for (u32 i = 0; i < 4; ++i) out.push_back(static_cast<u8>(value >> (i * 8)));
}
void appendFloat(std::vector<u8> &out, float value) {
  u32 bits = 0;
  std::memcpy(&bits, &value, 4);
  appendU32(out, bits);
}
// O mesmo relevo num GLB mínimo (POSITION + índices), para a importação real.
std::vector<u8> gridGlb(u32 cells) {
  std::vector<u8> binary;
  const u32 row = cells + 1, vertexCount = row * row;
  float high = 0, low = 0;
  for (u32 z = 0; z <= cells; ++z)
    for (u32 x = 0; x <= cells; ++x) {
      appendFloat(binary, static_cast<float>(x));
      appendFloat(binary, height(x, z));
      appendFloat(binary, static_cast<float>(z));
      high = std::max(high, height(x, z));
      low = std::min(low, height(x, z));
    }
  const u32 positionBytes = static_cast<u32>(binary.size());
  u32 indexCount = 0;
  for (u32 z = 0; z < cells; ++z)
    for (u32 x = 0; x < cells; ++x) {
      const u32 a = z * row + x, b = a + 1, c = a + row, d = c + 1;
      for (const u32 i : {a, c, b, b, c, d}) appendU32(binary, i);
      indexCount += 6;
    }
  const std::string json =
      "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
      "\"nodes\":[{\"name\":\"Relevo\",\"mesh\":0}],"
      "\"meshes\":[{\"name\":\"Relevo\",\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}],"
      "\"buffers\":[{\"byteLength\":" + std::to_string(binary.size()) + "}],"
      "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":" + std::to_string(positionBytes) + "},"
      "{\"buffer\":0,\"byteOffset\":" + std::to_string(positionBytes) + ",\"byteLength\":" +
      std::to_string(binary.size() - positionBytes) + "}],"
      "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":" + std::to_string(vertexCount) +
      ",\"type\":\"VEC3\",\"min\":[0," + std::to_string(low) + ",0],\"max\":[" + std::to_string(cells) + "," +
      std::to_string(high) + "," + std::to_string(cells) + "]},"
      "{\"bufferView\":1,\"componentType\":5125,\"count\":" + std::to_string(indexCount) + ",\"type\":\"SCALAR\"}]}";
  std::string padded = json;
  while (padded.size() % 4) padded.push_back(' ');
  std::vector<u8> glb;
  appendU32(glb, 0x46546C67);
  appendU32(glb, 2);
  appendU32(glb, static_cast<u32>(12 + 8 + padded.size() + 8 + binary.size()));
  appendU32(glb, static_cast<u32>(padded.size()));
  appendU32(glb, 0x4E4F534A);
  glb.insert(glb.end(), padded.begin(), padded.end());
  appendU32(glb, static_cast<u32>(binary.size()));
  appendU32(glb, 0x004E4942);
  glb.insert(glb.end(), binary.begin(), binary.end());
  return glb;
}
} // namespace

AE_TEST(s3_mesh_lod_halves_indices_keeps_vertices_and_error_grows) {
  auto g = grid(32);  // 2048 triângulos
  const auto sourceIndices = g.indices.size();
  MeshLodSettings settings;
  settings.generate = true;
  settings.optimizeOrder = true;
  std::vector<renderer::MeshLodLevel> lods;
  MeshLodReport report;
  std::string diagnostic;
  const std::vector<renderer::MapDrawRecord> draws{g.draw};
  AE_EXPECT_TRUE(buildMeshLods(g.vertices, g.indices, draws, {}, settings, lods, report, diagnostic), diagnostic.c_str());
  AE_EXPECT_TRUE(lods.size() >= 2 && lods.size() <= 3, "até três níveis além do 0");
  AE_EXPECT_EQ(report.sourceTriangles, 2048ull, "triângulos da fonte medidos");
  u32 previous = g.draw.indexCount;
  float previousError = 0;
  for (usize i = 0; i < lods.size(); ++i) {
    const auto &lod = lods[i];
    AE_EXPECT_EQ(lod.level, static_cast<u32>(i + 1), "níveis em ordem");
    AE_EXPECT_TRUE(lod.indexCount >= 3 && lod.indexCount <= previous * 85 / 100, "cada nível reduz de verdade");
    AE_EXPECT_TRUE(lod.firstIndex >= sourceIndices, "faixa acrescentada depois do nível 0");
    AE_EXPECT_TRUE(lod.geometricError >= previousError, "erro não decrescente");
    for (u32 k = 0; k < lod.indexCount; ++k)
      if (g.indices[lod.firstIndex + k] >= 33u * 33u) {AE_EXPECT_TRUE(false, "índice fora dos vértices"); break;}
    previous = lod.indexCount;
    previousError = lod.geometricError;
  }
  AE_EXPECT_TRUE(previousError > 0, "relevo simplificado tem erro medido");
  AE_EXPECT_EQ(g.vertices.size(), 33ull * 33ull * renderer::MapVertexStride, "nenhum vértice criado");
  AE_EXPECT_TRUE(report.acmrAfter > 0 && report.acmrAfter <= report.acmrBefore, "ordem nova não piora o cache");
}

AE_TEST(s3_mesh_lod_skips_small_and_deformed_and_fails_closed) {
  auto small = grid(8);  // 128 triângulos
  MeshLodSettings settings;
  settings.generate = true;
  std::vector<renderer::MeshLodLevel> lods;
  MeshLodReport report;
  std::string diagnostic;
  std::vector<renderer::MapDrawRecord> draws{small.draw};
  AE_EXPECT_TRUE(buildMeshLods(small.vertices, small.indices, draws, {}, settings, lods, report, diagnostic) &&
                 lods.empty() && report.skippedSmall == 1, "menos de 256 triângulos: sem LOD");
  auto big = grid(32);
  draws = {big.draw};
  const std::vector<u8> deformed{1};
  AE_EXPECT_TRUE(buildMeshLods(big.vertices, big.indices, draws, deformed, settings, lods, report, diagnostic) &&
                 lods.empty() && report.skippedDeformed == 1, "skin ou blend shapes: sem LOD");
  auto broken = grid(32);
  const auto before = broken.indices;
  auto draw = broken.draw;
  draw.firstIndex = 10;
  draws = {draw};
  settings.optimizeOrder = true;
  AE_EXPECT_TRUE(!buildMeshLods(broken.vertices, broken.indices, draws, {}, settings, lods, report, diagnostic) &&
                 broken.indices == before, "faixa fora do buffer recusa sem alterar nada");
  auto limited = grid(32);
  draws = {limited.draw};
  settings.maximumAddedIndices = 100;
  AE_EXPECT_TRUE(buildMeshLods(limited.vertices, limited.indices, draws, {}, settings, lods, report, diagnostic) &&
                 lods.empty() && report.budgetReached, "teto de índices para a geração e diz");
}

AE_TEST(s3_import_generates_lods_that_survive_the_cache_and_change_its_key_only_when_on) {
  const auto glb = gridGlb(32);
  GltfImportLimits off;
  GltfImport plain;
  AE_EXPECT_TRUE(importGlb(glb, off, {}, plain), plain.diagnostic.c_str());
  AE_EXPECT_TRUE(plain.meshLods.empty() && plain.lodSourceTriangles == 0, "desligado: saída de antes");
  GltfImportLimits on = off;
  on.generateLods = true;
  on.optimizePolygonOrder = true;
  GltfImport model;
  AE_EXPECT_TRUE(importGlb(glb, on, {}, model), model.diagnostic.c_str());
  AE_EXPECT_TRUE(!model.meshLods.empty() && model.lodDraws == 1 && model.lodSourceTriangles == 2048,
                 "a importação gera níveis para o relevo");
  AE_EXPECT_EQ(model.draws.size(), plain.draws.size(), "LOD não cria desenho novo");
  AE_EXPECT_TRUE(importCacheKey("abc", off) != importCacheKey("abc", on), "ligar o LOD muda a chave");
  std::vector<u8> bytes;
  AE_EXPECT_TRUE(writeImportCache(model, importCacheKey("abc", on), bytes), "derivado gravado");
  GltfImport back;
  AE_EXPECT_TRUE(readImportCache(bytes, importCacheKey("abc", on), back), "derivado lido");
  AE_EXPECT_TRUE(back.meshLods.size() == model.meshLods.size() && back.meshLods.back().indexCount == model.meshLods.back().indexCount &&
                 back.meshLods.back().geometricError == model.meshLods.back().geometricError &&
                 back.lodDraws == model.lodDraws && back.acmrAfter == model.acmrAfter, "níveis e medidas voltam do cache");
  std::vector<u8> plainBytes;
  AE_EXPECT_TRUE(writeImportCache(plain, importCacheKey("abc", off), plainBytes) &&
                 readImportCache(plainBytes, importCacheKey("abc", off), back) && back.meshLods.empty(),
                 "derivado sem LOD segue no formato anterior");
  // Cache adulterado: nível apontando para fora dos índices é recusado inteiro.
  auto corrupted = model;
  corrupted.meshLods.back().firstIndex = static_cast<u32>(corrupted.indices.size());
  AE_EXPECT_TRUE(writeImportCache(corrupted, importCacheKey("abc", on), bytes) &&
                 !readImportCache(bytes, importCacheKey("abc", on), back), "faixa inválida recusa o derivado");
}

AE_TEST(s3_import_profile_schema_11_persists_lod_choices_and_asks_for_preparation) {
  ImportProfile profile;
  profile.generateLods = true;
  profile.maximumLodLevels = 3;
  profile.optimizePolygonOrder = true;
  ImportProfile read;
  AE_EXPECT_TRUE(parseImportProfile(serializeImportProfile(profile), read) && read.generateLods &&
                 read.maximumLodLevels == 3 && read.optimizePolygonOrder, "escolhas de LOD voltam");
  AE_EXPECT_TRUE(!sameImportPreparation(profile, ImportProfile{}), "LOD pede nova preparação");
  const auto limits = applyImportProfile({}, profile);
  AE_EXPECT_TRUE(limits.generateLods && limits.optimizePolygonOrder && limits.maximumLodLevels == 3, "e chega aos limites");
  auto text = serializeImportProfile(profile);
  text.replace(text.find("\"maximumLodLevels\":3"), 20, "\"maximumLodLevels\":9");
  AE_EXPECT_TRUE(!parseImportProfile(text, read), "níveis fora de 2..4 recusam o arquivo");
}

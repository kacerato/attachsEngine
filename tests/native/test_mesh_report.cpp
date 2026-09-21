// Dados da malha e relatório da fonte (G6-A).
//
// Protegido aqui: os canais que a Unity lista na prévia da malha (UV0, UV1,
// normais, tangentes, cores) são lidos do vértice empacotado; a densidade de
// texel é medida em texels por metro e acompanha a escala do objeto; UV
// esticada aparece como razão entre decis; e o relatório da fonte aponta o que
// nenhuma configuração do importador conserta.
#include "harness.h"
#include "renderer/authoring_geometry.h"
#include "renderer/mesh_report.h"
#include "resources/import_report.h"

#include <cmath>
#include <cstring>
#include <vector>

using namespace ae;
using namespace ae::renderer;

namespace {
// Um quadrado de 1 m² no plano XZ com a UV que quem chama escolher. É a figura
// mais simples em que a densidade de texel tem resposta exata: uma textura de
// 1024² cobrindo 0..1 num metro dá 1024 texels/m.
struct Quad {
  std::vector<u8> vertices;
  std::vector<u32> indices;
  MapDrawRecord draw{};
};

void addQuad(Quad &quad, float size, float uvScale, bool normals = true, bool tangents = true) {
  struct Vertex {float position[3];i16 normal[4];i16 tangent[4];float uv[2];float uv1[2];u32 color;};
  static_assert(sizeof(Vertex) == MapVertexStride);
  const u32 base = static_cast<u32>(quad.vertices.size() / MapVertexStride);
  const float corners[4][2]{{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  for (const auto &corner : corners) {
    Vertex vertex{};
    vertex.position[0] = corner[0] * size;
    vertex.position[2] = corner[1] * size;
    if (normals) vertex.normal[1] = 32767;
    if (tangents) {vertex.tangent[0] = 32767; vertex.tangent[3] = 32767;}
    vertex.uv[0] = corner[0] * uvScale;
    vertex.uv[1] = corner[1] * uvScale;
    vertex.color = 0xffffffffu;
    const usize offset = quad.vertices.size();
    quad.vertices.resize(offset + sizeof(Vertex));
    std::memcpy(quad.vertices.data() + offset, &vertex, sizeof(Vertex));
  }
  quad.draw.firstIndex = 0;
  quad.draw.vertexOffset = 0;
  quad.indices.insert(quad.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
  quad.draw.indexCount = static_cast<u32>(quad.indices.size());
  quad.draw.boundsRadius = size;
}
} // namespace

AE_TEST(mesh_data_report_reads_the_channels_the_unity_mesh_inspector_lists) {
  std::vector<u8> vertices;
  std::vector<u32> indices;
  std::vector<MapDrawRecord> draws;
  std::vector<MapMaterialRecord> materials;
  AE_EXPECT_TRUE(appendBoxAuthoringGeometry(MapVertexStride, vertices, indices, draws, materials), "cubo autoral");
  MeshDataReport report;
  AE_EXPECT_TRUE(buildMeshDataReport(vertices, indices, draws[0], 1.0f, report), "o cubo é medido");
  AE_EXPECT_EQ(report.triangleCount, 12u, "seis faces, doze triângulos");
  AE_EXPECT_EQ(report.vertexCount, 24u, "quatro vértices por face");
  AE_EXPECT_TRUE(report.hasUv0 && report.hasNormals && report.hasTangents, "UV0, normais e tangentes presentes");
  AE_EXPECT_TRUE(!report.hasUv1, "o cubo não traz segundo conjunto de UV");
  AE_EXPECT_EQ(report.zeroNormals, 0u, "nenhuma normal nula");
  AE_EXPECT_EQ(report.degenerateTriangles, 0u, "nenhum triângulo sem área");
  // Cubo de 1 m de lado: 6 m² de superfície e 6 folhas de UV inteiras.
  AE_EXPECT_TRUE(std::fabs(report.surfaceArea - 6) < 1e-3f, "seis metros quadrados de superfície");
  AE_EXPECT_TRUE(std::fabs(report.uvArea - 6) < 1e-3f, "cada face ocupa a folha inteira");
  AE_EXPECT_TRUE(std::fabs(report.largestDimension() - 1) < 1e-4f, "um metro na maior dimensão");
  AE_EXPECT_TRUE(std::fabs(report.texelDensity(1024) - 1024) < 1, "1024² sobre um metro dá 1024 texels/m");
  // A densidade é do tamanho NA CENA: o mesmo cubo com o dobro da escala tem
  // metade dos texels por metro.
  MeshDataReport scaled;
  AE_EXPECT_TRUE(buildMeshDataReport(vertices, indices, draws[0], 2.0f, scaled), "cubo com escala 2");
  AE_EXPECT_TRUE(std::fabs(scaled.texelDensity(1024) - 512) < 1, "dobrar o objeto divide a densidade por dois");
  AE_EXPECT_TRUE(std::fabs(scaled.largestDimension() - 2) < 1e-4f, "os limites acompanham a escala");
  AE_EXPECT_TRUE(!buildMeshDataReport(vertices, indices, draws[0], 0.0f, report), "escala zero é recusada");
}

AE_TEST(mesh_data_report_measures_texel_density_and_uv_stretch) {
  Quad uniform;
  addQuad(uniform, 1.0f, 1.0f);
  MeshDataReport report;
  AE_EXPECT_TRUE(buildMeshDataReport(uniform.vertices, uniform.indices, uniform.draw, 1.0f, report), "quadrado medido");
  AE_EXPECT_TRUE(std::fabs(report.texelDensity(512) - 512) < 1, "um metro com a folha inteira em 512²");
  AE_EXPECT_TRUE(std::fabs(report.stretchRatio - 1) < 1e-3f, "densidade uniforme");
  AE_EXPECT_TRUE(report.outsideUnitRatio == 0, "UV dentro do quadrado");

  // Mesma geometria com a UV repetindo quatro vezes: quatro vezes a densidade,
  // e "fora de 0..1" avisando que é repetição, não erro.
  Quad tiled;
  addQuad(tiled, 1.0f, 4.0f);
  MeshDataReport tiledReport;
  AE_EXPECT_TRUE(buildMeshDataReport(tiled.vertices, tiled.indices, tiled.draw, 1.0f, tiledReport), "quadrado repetido");
  AE_EXPECT_TRUE(std::fabs(tiledReport.texelDensity(512) - 2048) < 4, "repetir a textura quadruplica a densidade");
  AE_EXPECT_TRUE(tiledReport.outsideUnitRatio > .5f, "a maioria dos vértices fica fora de 0..1");
  AE_EXPECT_TRUE(tiledReport.uvMaximum[0] == 4, "a faixa de UV é relatada como está");

  // Dois quadrados no mesmo desenho, um com densidade quatro vezes maior: é o
  // caso "parede nítida ao lado de parede borrada".
  Quad mixed;
  addQuad(mixed, 1.0f, 1.0f);
  addQuad(mixed, 1.0f, 4.0f);
  MeshDataReport mixedReport;
  AE_EXPECT_TRUE(buildMeshDataReport(mixed.vertices, mixed.indices, mixed.draw, 1.0f, mixedReport), "malha desigual");
  AE_EXPECT_TRUE(std::fabs(mixedReport.stretchRatio - 4) < .1f, "a razão entre decis aponta o esticamento");
}

AE_TEST(mesh_data_report_counts_what_breaks_the_surface) {
  Quad bare;
  addQuad(bare, 1.0f, 1.0f, false, false);
  MeshDataReport report;
  AE_EXPECT_TRUE(buildMeshDataReport(bare.vertices, bare.indices, bare.draw, 1.0f, report), "malha crua medida");
  AE_EXPECT_TRUE(!report.hasNormals && report.zeroNormals == 6, "normal nula é contada em cada canto");
  AE_EXPECT_TRUE(!report.hasTangents && report.invalidTangents == 6, "tangente ausente é contada");

  // UV degenerada: os quatro cantos no mesmo ponto da folha. A textura não tem
  // para onde mapear — nenhuma resolução conserta.
  Quad collapsed;
  addQuad(collapsed, 1.0f, 0.0f);
  MeshDataReport collapsedReport;
  AE_EXPECT_TRUE(buildMeshDataReport(collapsed.vertices, collapsed.indices, collapsed.draw, 1.0f, collapsedReport),
                 "malha com UV colapsada");
  AE_EXPECT_EQ(collapsedReport.zeroAreaUvTriangles, 2u, "os dois triângulos ficam sem área de UV");
  AE_EXPECT_TRUE(collapsedReport.densityMedian == 0, "sem área de UV não existe densidade a relatar");
}

AE_TEST(import_source_report_points_at_what_the_profile_cannot_fix) {
  resources::GltfImport model;
  std::vector<MapDrawRecord> draws;
  std::vector<MapMaterialRecord> materials;
  AE_EXPECT_TRUE(appendBoxAuthoringGeometry(MapVertexStride, model.vertices, model.indices, draws, materials), "cubo");
  model.draws = draws;
  model.materials = materials;
  model.materialNames.push_back("Concreto");
  model.names.push_back("Parede");
  model.nodes.push_back({});                 // raiz
  model.nodes.push_back({});                 // filho com a malha
  model.nodes[1].parent = 0;
  model.nodes[1].localMatrix[0] = model.nodes[1].localMatrix[5] = model.nodes[1].localMatrix[10] = 2;
  model.drawNodes.push_back(1);
  auto texture = std::make_shared<AuthoringTexture>();
  const_cast<AuthoringTexture *>(texture.get())->width = 1024;
  const_cast<AuthoringTexture *>(texture.get())->height = 1024;
  model.textures.push_back(texture);
  model.materials[0].textureIndices[0] = 0;
  model.materials[0].flags |= MapMaterialNormalMap;

  resources::ImportSourceReport report;
  AE_EXPECT_TRUE(resources::buildImportSourceReport(model, 1.0f, report), "o arquivo é medido");
  AE_EXPECT_EQ(report.meshCount, 1u, "uma malha");
  AE_EXPECT_EQ(report.depth, 2u, "raiz e filho");
  AE_EXPECT_EQ(report.rootCount, 1u, "uma raiz");
  AE_EXPECT_TRUE(report.meshes.size() == 1 && report.meshes[0].material == "Concreto", "o material da fonte é nomeado");
  // A escala do NÓ entra na densidade: o cubo de 1 m dobrado vira 2 m e a
  // mesma textura passa a cobrir o dobro de superfície.
  AE_EXPECT_TRUE(std::fabs(report.meshes[0].texelDensity() - 512) < 2, "a pose do nó decide os texels por metro");
  AE_EXPECT_TRUE(std::fabs(report.largestDimension() - 2 * 2 * .8660254f) < .01f, "os limites saem no espaço do arquivo");
  AE_EXPECT_EQ(report.normalMapsWithoutTangents, 0u, "o cubo traz tangentes");
  AE_EXPECT_TRUE(report.level == resources::ImportIssueLevel::Ok, "nada a apontar numa fonte sadia");

  // Escala do perfil (o Scale Factor do Model Import Settings) multiplica tudo.
  resources::ImportSourceReport scaled;
  AE_EXPECT_TRUE(resources::buildImportSourceReport(model, .5f, scaled), "metade da escala");
  AE_EXPECT_TRUE(std::fabs(scaled.meshes[0].texelDensity() - 1024) < 4, "modelo menor concentra os texels");

  // Mapa normal sem tangente: erro com conserto conhecido no perfil.
  resources::GltfImport bare = model;
  bare.vertices.clear();
  bare.indices.clear();
  Quad quad;
  addQuad(quad, 1.0f, 1.0f, true, false);
  bare.vertices = quad.vertices;
  bare.indices = quad.indices;
  bare.draws[0] = quad.draw;
  bare.draws[0].materialIndex = 0;
  resources::ImportSourceReport bareReport;
  AE_EXPECT_TRUE(resources::buildImportSourceReport(bare, 1.0f, bareReport), "fonte sem tangente");
  AE_EXPECT_EQ(bareReport.normalMapsWithoutTangents, 1u, "mapa normal sem tangente é apontado");
  AE_EXPECT_TRUE(bareReport.level == resources::ImportIssueLevel::Error, "é erro, não observação");
  AE_EXPECT_TRUE(!bareReport.meshes[0].issues.empty(), "a linha explica o conserto no perfil");
}

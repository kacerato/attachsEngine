#pragma once
// Relatório da fonte importada: o que o arquivo traz, na medida.
//
// Uma cena de referência só serve para comparar mudanças visuais se a FONTE for
// conhecida. "A parede ficou borrada" pode ser a iluminação, a textura, o
// filtro — ou UV com metade da densidade do resto do prédio, que nenhuma
// configuração de engine conserta. Este relatório separa os dois casos antes de
// a discussão começar.
//
// O vocabulário é o do Model Import Settings e do Mesh asset Inspector da Unity
// 6 (Scale Factor, canais de UV, Normals, Tangents, hierarquia), com uma medida
// que a Unity não traz de fábrica: **densidade de texel** em texels por metro.
// A prática lá é aplicar uma textura xadrez (UV Checker) e julgar a olho; num
// aparelho isso não distingue 300 de 600 texels/m, e é justamente a diferença
// que aparece como uma parede nítida ao lado de uma borrada.
#include "renderer/mesh_report.h"
#include "resources/gltf_import.h"

#include <string>
#include <vector>

namespace ae::resources {

// Gravidade de uma linha do relatório. Não é opinião: "erro" é o que nenhuma
// configuração do importador conserta, e "atenção" é o que muda a aparência
// sem impedir o uso.
enum class ImportIssueLevel : u8 { Ok = 0, Warning = 1, Error = 2 };

struct ImportMeshReport final {
  std::string name, material;
  u32 draw = 0;
  // Resolução da textura de cor base efetivamente aplicada (0 sem textura): é
  // ela que transforma a densidade medida em texels por metro.
  u32 textureWidth = 0, textureHeight = 0;
  bool normalMap = false;
  renderer::MeshDataReport data;
  // Já em português e em ordem de gravidade, prontas para a linha do Inspector.
  std::vector<std::string> issues;
  ImportIssueLevel level = ImportIssueLevel::Ok;
  // Texels por metro com a textura vinculada; zero quando o material não tem
  // textura de cor base — sem textura não existe densidade a medir.
  float texelDensity() const {
    const u32 resolution = textureWidth && textureHeight
                               ? static_cast<u32>((static_cast<u64>(textureWidth) + textureHeight) / 2)
                               : 0;
    return resolution ? data.texelDensity(resolution) : 0;
  }
};

struct ImportSourceReport final {
  u32 nodeCount = 0, rootCount = 0, depth = 0, meshCount = 0, materialCount = 0, textureCount = 0;
  u64 vertexCount = 0, triangleCount = 0;
  // Limites do modelo INTEIRO, já com a escala do perfil aplicada e a pose de
  // cada nó — é a escala que o autor vai ver na cena, não a do arquivo.
  float boundsMinimum[3]{0, 0, 0}, boundsMaximum[3]{0, 0, 0};
  u32 meshesWithoutUv = 0, meshesWithoutNormals = 0, meshesWithoutTangents = 0;
  // Mapa normal sem tangente é o caso que apaga o relevo inteiro; contado à
  // parte porque tem conserto no perfil (Tangents: Calcular).
  u32 normalMapsWithoutTangents = 0;
  u32 stretchedMeshes = 0, unmappedMeshes = 0;
  // Densidade mediana do arquivo e a faixa entre as malhas texturizadas: duas
  // malhas com densidades muito diferentes aparecem lado a lado na cena.
  float densityMedian = 0, densityLowest = 0, densityHighest = 0;
  std::vector<ImportMeshReport> meshes;
  ImportIssueLevel level = ImportIssueLevel::Ok;
  float largestDimension() const {
    float largest = 0;
    for (u32 axis = 0; axis < 3; ++axis)
      largest = std::max(largest, boundsMaximum[axis] - boundsMinimum[axis]);
    return largest;
  }
};

// Mede o modelo já preparado. `uniformScale` é a escala do perfil (o Scale
// Factor do Model Import Settings) que ainda não está nas posições do buffer;
// passar 1 relata o arquivo como ele está.
//
// Falha fechada: buffer inconsistente devolve falso e `out` intocado.
bool buildImportSourceReport(const GltfImport &model, float uniformScale, ImportSourceReport &out);

} // namespace ae::resources

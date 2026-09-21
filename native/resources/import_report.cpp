#include "resources/import_report.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ae::resources {
namespace {

void multiply(const float a[16], const float b[16], float out[16]) {
  for (u32 column = 0; column < 4; ++column)
    for (u32 row = 0; row < 4; ++row) {
      float sum = 0;
      for (u32 k = 0; k < 4; ++k) sum += a[k * 4 + row] * b[column * 4 + k];
      out[column * 4 + row] = sum;
    }
}

float columnLength(const float matrix[16], u32 column) {
  return std::sqrt(matrix[column * 4] * matrix[column * 4] + matrix[column * 4 + 1] * matrix[column * 4 + 1] +
                   matrix[column * 4 + 2] * matrix[column * 4 + 2]);
}

std::string rounded(float value, u32 decimals = 1) {
  char text[64];
  std::snprintf(text, sizeof(text), "%.*f", static_cast<int>(decimals), static_cast<double>(value));
  return text;
}

// Mediana ponderada pela área: uma malha de 200 m² pesa mais na leitura do
// arquivo do que um parafuso de 2 cm².
float weightedMedian(std::vector<std::pair<float, float>> values) {
  if (values.empty()) return 0;
  std::sort(values.begin(), values.end(),
            [](const std::pair<float, float> &a, const std::pair<float, float> &b) { return a.first < b.first; });
  float total = 0;
  for (const auto &entry : values) total += entry.second;
  if (total <= 0) return values[values.size() / 2].first;
  float accumulated = 0;
  for (const auto &entry : values) {
    accumulated += entry.second;
    if (accumulated >= total * .5f) return entry.first;
  }
  return values.back().first;
}

} // namespace

bool buildImportSourceReport(const GltfImport &model, float uniformScale, ImportSourceReport &out) {
  if (!(uniformScale > 0) || !std::isfinite(uniformScale)) return false;
  if (model.draws.size() != model.drawNodes.size()) return false;

  ImportSourceReport report;
  report.nodeCount = static_cast<u32>(model.nodes.size());
  report.meshCount = static_cast<u32>(model.draws.size());
  report.materialCount = static_cast<u32>(model.materials.size());
  report.textureCount = static_cast<u32>(model.textures.size());

  // Matriz de mundo de cada nó e profundidade da árvore. O arquivo não promete
  // pai antes de filho, então a subida é feita nó a nó, com memória.
  std::vector<float> world(model.nodes.size() * 16, 0.0f);
  std::vector<u8> resolved(model.nodes.size(), 0);
  std::vector<u32> depth(model.nodes.size(), 0);
  for (u32 node = 0; node < model.nodes.size(); ++node) {
    // Cadeia até a raiz, com teto: um pai cíclico num arquivo estranho não pode
    // travar a importação.
    std::vector<u32> chain;
    i32 walk = static_cast<i32>(node);
    while (walk >= 0 && !resolved[static_cast<u32>(walk)] && chain.size() <= model.nodes.size()) {
      chain.push_back(static_cast<u32>(walk));
      walk = model.nodes[static_cast<u32>(walk)].parent;
      if (walk >= static_cast<i32>(model.nodes.size())) return false;
    }
    if (chain.size() > model.nodes.size()) return false;
    for (auto entry = chain.rbegin(); entry != chain.rend(); ++entry) {
      const auto &current = model.nodes[*entry];
      if (current.parent < 0) {
        std::copy(current.localMatrix, current.localMatrix + 16, world.begin() + *entry * 16);
        depth[*entry] = 0;
        ++report.rootCount;
      } else {
        multiply(&world[static_cast<u32>(current.parent) * 16], current.localMatrix, &world[*entry * 16]);
        depth[*entry] = depth[static_cast<u32>(current.parent)] + 1;
      }
      resolved[*entry] = 1;
      report.depth = std::max(report.depth, depth[*entry] + 1);
    }
  }

  std::vector<std::pair<float, float>> fileDensities;
  bool boundsStarted = false;
  for (u32 index = 0; index < model.draws.size(); ++index) {
    const auto &draw = model.draws[index];
    const u32 node = model.drawNodes[index];
    if (node >= model.nodes.size()) return false;
    const float *matrix = &world[node * 16];
    const float axisScale[3]{columnLength(matrix, 0), columnLength(matrix, 1), columnLength(matrix, 2)};
    const float nodeScale = (axisScale[0] + axisScale[1] + axisScale[2]) / 3.0f * uniformScale;

    ImportMeshReport mesh;
    mesh.draw = index;
    mesh.name = index < model.names.size() ? model.names[index] : std::string{};
    if (mesh.name.empty()) mesh.name = "Malha " + std::to_string(index + 1);
    if (draw.materialIndex < model.materialNames.size()) mesh.material = model.materialNames[draw.materialIndex];
    if (mesh.material.empty()) mesh.material = "Material " + std::to_string(draw.materialIndex + 1);
    if (draw.materialIndex < model.materials.size()) {
      const auto &material = model.materials[draw.materialIndex];
      mesh.normalMap = (material.flags & renderer::MapMaterialNormalMap) != 0;
      const u32 base = material.textureIndices[0];
      if (base != renderer::InvalidMapTexture && base < model.textures.size() && model.textures[base]) {
        mesh.textureWidth = model.textures[base]->width;
        mesh.textureHeight = model.textures[base]->height;
      }
    }
    if (!renderer::buildMeshDataReport(model.vertices, model.indices, draw,
                                       nodeScale > 0 ? nodeScale : 1.0f, mesh.data))
      return false;

    report.vertexCount += mesh.data.vertexCount;
    report.triangleCount += mesh.data.triangleCount;
    // Limites do modelo inteiro pela esfera do desenho levada ao mundo: é a
    // mesma esfera que o enquadramento usa, então a escala relatada é a que o
    // autor vai ver.
    const float center[3]{
        matrix[0] * draw.boundsCenter[0] + matrix[4] * draw.boundsCenter[1] + matrix[8] * draw.boundsCenter[2] + matrix[12],
        matrix[1] * draw.boundsCenter[0] + matrix[5] * draw.boundsCenter[1] + matrix[9] * draw.boundsCenter[2] + matrix[13],
        matrix[2] * draw.boundsCenter[0] + matrix[6] * draw.boundsCenter[1] + matrix[10] * draw.boundsCenter[2] + matrix[14]};
    const float radius = draw.boundsRadius * std::max({axisScale[0], axisScale[1], axisScale[2]}) * uniformScale;
    for (u32 axis = 0; axis < 3; ++axis) {
      const float low = center[axis] * uniformScale - radius, high = center[axis] * uniformScale + radius;
      if (!boundsStarted) {
        report.boundsMinimum[axis] = low;
        report.boundsMaximum[axis] = high;
      } else {
        report.boundsMinimum[axis] = std::min(report.boundsMinimum[axis], low);
        report.boundsMaximum[axis] = std::max(report.boundsMaximum[axis], high);
      }
    }
    boundsStarted = true;

    const auto note = [&](ImportIssueLevel level, std::string text) {
      mesh.issues.push_back(std::move(text));
      if (level > mesh.level) mesh.level = level;
    };
    const bool textured = mesh.textureWidth != 0 || mesh.normalMap;
    if (!mesh.data.hasUv0) {
      ++report.meshesWithoutUv;
      if (textured) {
        ++report.unmappedMeshes;
        note(ImportIssueLevel::Error, "Sem TEXCOORD_0 e com textura no material: não há coordenada a inventar, a fonte precisa trazer a UV");
      } else {
        note(ImportIssueLevel::Warning, "Sem TEXCOORD_0: só os fatores do material chegam à superfície");
      }
    }
    if (!mesh.data.hasNormals) {
      ++report.meshesWithoutNormals;
      note(ImportIssueLevel::Error, "Sem normais utilizáveis: a superfície fica sem base para a luz");
    }
    if (!mesh.data.hasTangents) {
      ++report.meshesWithoutTangents;
      if (mesh.normalMap) {
        ++report.normalMapsWithoutTangents;
        note(ImportIssueLevel::Error, "Mapa normal sem tangente: o relevo não aparece — use Tangentes: Calcular no perfil");
      }
    }
    // Malha SEM UV nenhuma já foi dita acima; repetir aqui contaria o mesmo
    // fato duas vezes e, pior, transformaria uma superfície de cor lisa
    // (material sem textura, escolha legítima) em erro vermelho. Só a malha que
    // TEM UV e mesmo assim colapsou triângulos entra nesta linha.
    if (mesh.data.zeroAreaUvTriangles && mesh.data.hasUv0)
      note(textured ? ImportIssueLevel::Error : ImportIssueLevel::Warning,
           std::to_string(mesh.data.zeroAreaUvTriangles) +
           " triângulo(s) com UV sem área: a textura não mapeia ali, em nenhuma resolução");
    if (mesh.data.stretchRatio >= 4) {
      ++report.stretchedMeshes;
      note(ImportIssueLevel::Warning, "Densidade de texel desigual (" + rounded(mesh.data.stretchRatio) +
           "× entre o decil baixo e o alto): UV esticada na fonte");
    }
    if (mesh.data.degenerateTriangles)
      note(ImportIssueLevel::Warning, std::to_string(mesh.data.degenerateTriangles) +
           " triângulo(s) sem área: custam vértice e não desenham nada");
    if (mesh.data.invalidTangents && mesh.data.hasTangents)
      note(ImportIssueLevel::Warning, std::to_string(mesh.data.invalidTangents) +
           " tangente(s) sem orientação: o mapa normal fica instável nesses vértices");
    if (mesh.data.outsideUnitRatio > .001f && mesh.data.hasUv0)
      note(ImportIssueLevel::Ok, "UV fora de 0..1 em " + rounded(mesh.data.outsideUnitRatio * 100) +
           "% dos vértices: a textura repete, que é escolha da fonte");

    const float density = mesh.texelDensity();
    if (density > 0) {
      fileDensities.emplace_back(density, std::max(mesh.data.surfaceArea, 1e-6f));
      report.densityLowest = report.densityLowest > 0 ? std::min(report.densityLowest, density) : density;
      report.densityHighest = std::max(report.densityHighest, density);
    }
    if (mesh.level > report.level) report.level = mesh.level;
    report.meshes.push_back(std::move(mesh));
  }
  report.densityMedian = weightedMedian(std::move(fileDensities));
  out = std::move(report);
  return true;
}

} // namespace ae::resources

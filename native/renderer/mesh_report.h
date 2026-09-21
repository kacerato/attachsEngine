#pragma once
// Dados de uma malha, medidos — o que o Mesh asset Inspector da Unity 6 mostra
// ao lado da prévia (vértices, faces, canais de UV, normais, tangentes, cores)
// mais o que ela deixa para o olho julgar nas visualizações **UV Layout**,
// **UV Checker**, **Normals** e **Tangents**.
//
// Por que medir em vez de só desenhar: num aparelho, olhar um xadrez de UV numa
// tela de seis polegadas não distingue 300 de 600 texels por metro, e é
// exatamente essa diferença que faz uma parede parecer borrada ao lado de outra.
// O número também entra em teste; a imagem não.
//
// Este módulo não conhece importação nem editor: recebe o buffer de vértices no
// passo do pacote de mapa e um desenho, e devolve fatos. Quem interpreta
// (importador, Inspector, relatório) mora acima.
#include "renderer/map_package.h"

#include <span>

namespace ae::renderer {

// Posição, normal e tangente empacotadas, UV0, UV1 e cor — o vértice de 48
// bytes que `MapVertexStride` descreve. Ter a leitura em um lugar só evita que
// cada consumidor redescubra os deslocamentos.
struct MapVertexView final {
  float position[3]{};
  float normal[3]{};
  float tangent[3]{};
  float tangentSign = 0;
  float uv0[2]{};
  float uv1[2]{};
  float color[4]{};
};
MapVertexView readMapVertex(std::span<const u8> vertices, u64 index);

struct MeshDataReport final {
  u32 vertexCount = 0, triangleCount = 0;
  // Canais presentes, na ordem em que a Unity os lista na prévia da malha.
  bool hasUv0 = false, hasUv1 = false, hasNormals = false, hasTangents = false, hasColors = false;
  // Triângulo sem área no mundo não desenha nada e ainda custa vértice.
  u32 degenerateTriangles = 0;
  // Área de UV nula com área no mundo: a textura não tem para onde mapear.
  // É a forma mais severa de "textura esticada" — não é resolução, é a fonte.
  u32 zeroAreaUvTriangles = 0;
  u32 zeroNormals = 0, invalidTangents = 0;
  // Tangente com w negativo: o espelhamento do espaço tangente. Não é defeito —
  // é como o autor desenrolou a UV —, mas explica um mapa normal invertido.
  u32 mirroredTangents = 0;
  float boundsMinimum[3]{0, 0, 0}, boundsMaximum[3]{0, 0, 0};
  // Metros quadrados de superfície e o quanto da folha de UV ela ocupa.
  float surfaceArea = 0, uvArea = 0;
  float uvMinimum[2]{0, 0}, uvMaximum[2]{0, 0};
  // Fração dos vértices com UV fora de 0..1 — repetição da textura, que é
  // escolha válida, e o motivo de "fora do quadrado" não ser um erro sozinho.
  float outsideUnitRatio = 0;
  // Densidade de texel POR PIXEL de textura: multiplique pela resolução para
  // ter texels por metro. Guardada assim porque a mesma malha muda de densidade
  // quando o material troca de textura, e o relatório não deve recalcular a
  // geometria por causa disso.
  float densityLow = 0, densityMedian = 0, densityHigh = 0;
  // Razão entre o decil alto e o baixo: 1 é uniforme, 4 já aparece na tela como
  // uma parte nítida ao lado de uma borrada.
  float stretchRatio = 0;
  float texelDensity(u32 resolution) const { return densityMedian * static_cast<float>(resolution); }
  float largestDimension() const {
    float largest = 0;
    for (u32 axis = 0; axis < 3; ++axis) {
      const float extent = boundsMaximum[axis] - boundsMinimum[axis];
      if (extent > largest) largest = extent;
    }
    return largest;
  }
};

// `uniformScale` é a escala que o objeto (ou o perfil de importação) aplica: a
// densidade de texel de uma parede depende do tamanho que ela tem NA CENA, não
// do que tinha no arquivo. Escala não positiva é recusada.
//
// Falha fechada: contagem inválida, índice fora do buffer ou desenho vazio
// devolvem falso e `out` intocado.
bool buildMeshDataReport(std::span<const u8> vertices, std::span<const u32> indices,
                         const MapDrawRecord &draw, float uniformScale, MeshDataReport &out);

} // namespace ae::renderer

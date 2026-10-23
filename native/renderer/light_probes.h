#pragma once
// PK-6: sondas de luz (Light Probes) como na Unity 6.
//
// Na Unity o autor espalha pontos com um Light Probe Group, o bake grava em cada
// ponto um harmônico esférico L2 (9 coeficientes RGB) da luz que chega ali, e a
// cena inteira é tetraedralizada: cada objeto dinâmico com Light Probes = Blend
// Probes acha o tetraedro que contém o centro dos seus limites e mistura as
// quatro sondas pelos pesos baricêntricos. Fora do casco, a Unity estende as
// células da borda; aqui o objeto usa o tetraedro mais próximo com os pesos
// presos ao intervalo [0,1].
//
// Tudo neste arquivo é matemática de CPU sem dependência do backend: a projeção
// SH das faces capturadas no aparelho, o anti-ringing ("Remove Ringing"), a
// tetraedralização de Delaunay e a busca por caminhada entre vizinhos.
//
// Referências: https://docs.unity3d.com/6000.0/Documentation/Manual/LightProbes.html
//              https://docs.unity3d.com/6000.0/Documentation/Manual/class-LightProbeGroup.html
//              Sloan, "Stupid Spherical Harmonics Tricks" (GDC 2008) para a convolução e a janela.
#include "core/base.h"

#include <array>
#include <span>
#include <string>
#include <vector>

namespace ae::renderer {

// Nove coeficientes RGB, base real de SH na ordem (0,0) (1,-1) (1,0) (1,1)
// (2,-2) (2,-1) (2,0) (2,1) (2,2) com eixos y, z, x — a mesma do AEEN v4 e de
// environmentDiffuseIrradiance no shader. Guardamos IRRADIÂNCIA E (já
// convoluída pelo cosseno); o fragmento difuso usa E/π.
struct ShL2 {
  std::array<std::array<float, 3>, 9> c{};
  void add(const ShL2 &o, float w) noexcept {
    for (u32 i = 0; i < 9; ++i) for (u32 k = 0; k < 3; ++k) c[i][k] += o.c[i][k] * w;
  }
};
// Base de SH avaliada em uma direção unitária.
void shL2Basis(const float direction[3], float out[9]) noexcept;
// Irradiância avaliada na direção (sem dividir por π). Negativos presos a zero.
void evaluateShIrradiance(const ShL2 &sh, const float direction[3], float out[3]) noexcept;

// Face de cubo em RGB linear float, convenção de faces do Vulkan (+X,-X,+Y,-Y,+Z,-Z)
// e de texel (u para a direita, v para baixo), a mesma da captura do aparelho.
struct CubeFaces {
  u32 size = 0;
  // 6 * size * size * 3 floats, face por face, linha por linha.
  std::vector<float> rgb;
  bool valid() const noexcept { return size > 0 && rgb.size() == 6ull * size * size * 3; }
};
// Direção (não normalizada) do centro de um texel numa face do cubo do Vulkan.
void cubeTexelDirection(u32 face, float u, float v, float out[3]) noexcept;
// Projeta a RADIÂNCIA capturada em SH L2 (ponderando o ângulo sólido de cada
// texel) e convolve com o lobo do cosseno: o resultado é irradiância.
bool projectCubeToIrradiance(const CubeFaces &faces, ShL2 &out) noexcept;
// "Remove Ringing" do Light Probe Group: janela de Hanning sobre as bandas,
// que tira os lóbulos negativos atrás de uma fonte forte (sol num canto) às
// custas de suavizar. Também prende o termo DC a não negativo.
void deringShL2(ShL2 &sh) noexcept;

// Tetraedralização de Delaunay (Bowyer-Watson) das posições de mundo de todas
// as sondas assadas da cena. `neighbor[f]` é o tetraedro do outro lado da face
// oposta ao vértice f, ou -1 no casco.
struct ProbeTetrahedron {
  std::array<u32, 4> vertex{};
  std::array<i32, 4> neighbor{-1, -1, -1, -1};
};
struct LightProbeSet {
  std::vector<std::array<float, 3>> positions;
  std::vector<ShL2> sh;
  std::vector<ProbeTetrahedron> tetrahedra;
  bool empty() const noexcept { return positions.empty(); }
  // Monta os tetraedros. Menos de 4 sondas, ou todas coplanares, não formam
  // volume: nesse caso o conjunto usa a sonda mais próxima (ou a média).
  bool tetrahedralize();
};

// Resultado da busca para um objeto: o tetraedro e os quatro pesos.
struct ProbeInterpolation {
  i32 tetrahedron = -1;
  std::array<u32, 4> vertex{};
  std::array<float, 4> weight{};
  u32 count = 0; // 0 nenhuma sonda, 1 sonda única, 4 tetraedro
  bool inside = false;
};
// Caminha a partir de `hint` (o tetraedro do quadro anterior, -1 para começar
// do zero) até o tetraedro que contém o ponto. Fora do casco devolve o último
// tetraedro de borda alcançado com pesos presos e renormalizados.
ProbeInterpolation locateProbes(const LightProbeSet &set, const float point[3], i32 hint = -1) noexcept;
ShL2 interpolateProbes(const LightProbeSet &set, const ProbeInterpolation &where) noexcept;

// Limite de sondas por cena no aparelho: a tetraedralização roda no carregamento
// e a caminhada por objeto é O(arestas atravessadas).
inline constexpr u32 MaximumLightProbes = 2048;

// Recurso assado de um Light Probe Group (.aelightprobes), gravado no projeto
// como a Unity grava o LightingData: posição de MUNDO no momento do bake, o
// identificador do ponto e o SH L2 de irradiância. Mover o grupo depois deixa
// o recurso desatualizado (o Inspector avisa) até assar de novo.
struct LightProbeBakeData {
  static constexpr u32 Version = 1;
  static constexpr usize MaximumBytes = 2u << 20;
  u64 guidHigh = 0, guidLow = 0;
  // Hash das posições de mundo e dos parâmetros do bake: compara com a cena.
  u64 sourceHash = 0;
  float bakeSeconds = 0;
  std::vector<u64> pointIds;
  std::vector<std::array<float, 3>> positions;
  std::vector<ShL2> sh;
  bool valid() const noexcept;
  std::vector<u8> serialize() const;
  static bool deserialize(std::span<const u8> bytes, LightProbeBakeData &out, std::string *error = nullptr);
};
u64 lightProbePositionsHash(std::span<const std::array<float, 3>> positions, std::span<const u64> ids) noexcept;

} // namespace ae::renderer

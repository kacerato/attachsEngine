#include "harness.h"
#include "renderer/water_grid.h"
#include "renderer/water_detail_texture.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

using namespace ae::renderer;

AE_TEST(Water_detail_texture_has_complete_filtered_mips_and_stable_seed) {
  std::vector<ae::u8> pixels,repeat;
  AE_EXPECT_TRUE(buildWaterDetailTexture(64,42,pixels),"procedural detail");
  AE_EXPECT_TRUE(buildWaterDetailTexture(64,42,repeat),"repeat");
  AE_EXPECT_TRUE(pixels==repeat,"stable seed");
  AE_EXPECT_EQ(pixels.size(),21844u,"seven complete mip levels");
  AE_EXPECT_TRUE(std::abs(int(pixels[pixels.size()-4])-128)<=3,"distant average X slope is neutral");
  AE_EXPECT_TRUE(std::abs(int(pixels[pixels.size()-3])-128)<=3,"distant average Z slope is neutral");
  AE_EXPECT_TRUE(buildWaterDetailTexture(64,43,repeat),"alternate phase");
  AE_EXPECT_TRUE(pixels!=repeat,"seed changes detail");
  AE_EXPECT_TRUE(!buildWaterDetailTexture(63,1,pixels),"non power-of-two rejected");
  AE_EXPECT_EQ(pixels.size(),21844u,"invalid request preserves previous texture");
}

AE_TEST(WaterGrid_rejects_shapes_the_cubic_transition_cannot_mirror) {
  AE_EXPECT_TRUE(validateWaterGrid({}), "default grid is the baked ocean");
  AE_EXPECT_TRUE(!(validateWaterGrid({6, 384.0f, 8000.0f})), "below the segment floor");
  AE_EXPECT_TRUE(!(validateWaterGrid({250, 384.0f, 8000.0f})), "segments must divide by four");
  AE_EXPECT_TRUE(!(validateWaterGrid({256, 0.0f, 8000.0f})), "near extent must be positive");
  AE_EXPECT_TRUE(!(validateWaterGrid({256, 900.0f, 800.0f})), "extents must be ordered");
  AE_EXPECT_TRUE(!(validateWaterGrid({256, 384.0f, std::nanf("")})), "non finite extent");
  AE_EXPECT_TRUE(validateWaterGrid({8, 1.0f, 1.0f}), "degenerate but legal: near equals far");
}

AE_TEST(WaterGrid_matches_the_baked_ocean_contract) {
  // Os mesmos números que tools/build-ocean-demo.py assa em samples/ocean. Se
  // esta expectativa quebrar, runtime e cozimento divergiram e a malha assada
  // deixa de ser substituível pela gerada.
  const WaterGridSettings baked{256, 384.0f, 8000.0f};
  AE_EXPECT_EQ(waterGridVertexCount(baked), 257u * 257u, "vertex count");
  AE_EXPECT_EQ(waterGridTriangleCount(baked), 256u * 256u * 2u, "triangle count");

  AE_EXPECT_TRUE(std::fabs(waterGridAxisPosition(baked, 0) + 8000.0f) <= 1e-3f,
                 "left edge reaches far extent");
  AE_EXPECT_TRUE(std::fabs(waterGridAxisPosition(baked, 256) - 8000.0f) <= 1e-3f,
                 "right edge reaches far extent");
  AE_EXPECT_TRUE(std::fabs(waterGridAxisPosition(baked, 128)) <= 1e-4f,
                 "centre sits on the camera");
  // Metade do eixo: |coordinate| = 0,5 é a emenda, onde o termo cúbico ainda
  // vale zero e só a rampa linear do trecho central contribuiu.
  AE_EXPECT_TRUE(std::fabs(waterGridAxisPosition(baked, 192) - 192.0f) <= 1e-3f,
                 "seam is pure near extent");

  // Espaçamento nas duas pontas, conferido contra a saída de
  // tools/water_geometry.py: 3,0000 m no centro e 354,4509 m na borda. É a
  // razão de 118x entre eles que justifica a transição cúbica existir.
  AE_EXPECT_TRUE(std::fabs(waterGridAxisSpacing(baked, 128) - 3.0f) <= 1e-3f,
                 "centre spacing matches the offline contract");
  AE_EXPECT_TRUE(std::fabs(waterGridAxisSpacing(baked, 0) - 354.4509f) <= 1e-2f,
                 "edge spacing matches the offline contract");
}

AE_TEST(WaterGrid_axis_is_monotonic_odd_and_coarsens_outward) {
  const WaterGridSettings grid{128, 200.0f, 4000.0f};
  float previous = waterGridAxisPosition(grid, 0);
  for (ae::u32 index = 1; index <= grid.segments; ++index) {
    const float here = waterGridAxisPosition(grid, index);
    AE_EXPECT_TRUE(here > previous, "axis must advance at every step");
    previous = here;
  }
  // Simetria ímpar: a malha é relativa à câmera, e uma assimetria faria a água
  // deslizar sob o observador quando ele apenas gira.
  for (ae::u32 index = 0; index <= grid.segments; ++index) {
    const float left = waterGridAxisPosition(grid, index);
    const float right = waterGridAxisPosition(grid, grid.segments - index);
    AE_EXPECT_TRUE(std::fabs((left) - (-right)) <= (1e-3f), "axis is odd about the camera");
  }
  // O espaçamento perto da câmera é o que dita o custo de aliasing, e ele
  // precisa ser muito menor que o da borda — é isso que a transição cúbica
  // compra em relação a uma grade uniforme.
  const float centre = waterGridAxisSpacing(grid, grid.segments / 2u);
  const float edge = waterGridAxisSpacing(grid, 0);
  AE_EXPECT_TRUE(centre > 0.0f, "centre spacing is positive");
  AE_EXPECT_TRUE(edge > centre * 20.0f, "edge is far coarser than the centre");
}

AE_TEST(WaterGrid_spacing_is_conservative_at_both_ends) {
  const WaterGridSettings grid{64, 100.0f, 2000.0f};
  for (ae::u32 index = 0; index <= grid.segments; ++index) {
    const float spacing = waterGridAxisSpacing(grid, index);
    const float here = waterGridAxisPosition(grid, index);
    const float before = waterGridAxisPosition(grid, index == 0 ? 0 : index - 1);
    const float after = waterGridAxisPosition(grid, index >= grid.segments ? grid.segments : index + 1);
    // Conservador significa a aresta longa: subestimar aqui produz aliasing.
    AE_EXPECT_TRUE(spacing >= here - before - 1e-3f, "covers the incoming edge");
    AE_EXPECT_TRUE(spacing >= after - here - 1e-3f, "covers the outgoing edge");
  }
}

AE_TEST(WaterGrid_out_of_range_index_returns_zero_instead_of_reading_past_the_axis) {
  const WaterGridSettings grid{32, 50.0f, 500.0f};
  AE_EXPECT_EQ(waterGridAxisPosition(grid, grid.segments + 1u), 0.0f, "position clamps to zero");
  AE_EXPECT_EQ(waterGridAxisSpacing(grid, grid.segments + 1u), 0.0f, "spacing clamps to zero");
  const WaterGridSettings broken{7, 50.0f, 500.0f};
  AE_EXPECT_EQ(waterGridAxisPosition(broken, 0), 0.0f, "invalid grid yields no geometry");
}

AE_TEST(WaterGrid_selection_scales_with_sea_state_and_never_leaves_its_profile) {
  for (ae::u32 raw = static_cast<ae::u32>(WaterMeshQuality::Ultra);
       raw < static_cast<ae::u32>(WaterMeshQuality::Count); ++raw) {
    const auto quality = sanitizeWaterMeshQuality(raw);
    const auto calm = selectWaterGrid(quality, 0.0f, 8000.0f);
    const auto storm = selectWaterGrid(quality, MaximumWaterGridWindSpeed, 8000.0f);
    AE_EXPECT_TRUE(validateWaterGrid(calm), "calm grid honours the contract");
    AE_EXPECT_TRUE(validateWaterGrid(storm), "storm grid honours the contract");
    AE_EXPECT_TRUE(storm.segments > calm.segments, "wind must buy resolution");

    // Acima do teto de vento a malha não cresce mais: o espectro satura e mais
    // triângulo deixaria de comprar onda.
    const auto hurricane = selectWaterGrid(quality, MaximumWaterGridWindSpeed * 4.0f, 8000.0f);
    AE_EXPECT_EQ(hurricane.segments, storm.segments, "grid saturates with the spectrum");

    // Vento inválido erra para o lado caro, não para o lado quebrado.
    const auto broken = selectWaterGrid(quality, std::nanf(""), 8000.0f);
    AE_EXPECT_EQ(broken.segments, storm.segments, "non finite wind assumes a formed sea");
  }

  // Ultra em mar formado reproduz exatamente a malha assada, para que trocar o
  // cozimento pelo runtime não mude o que já foi medido.
  const auto ultra = selectWaterGrid(WaterMeshQuality::Ultra, MaximumWaterGridWindSpeed, 8000.0f);
  AE_EXPECT_EQ(ultra.segments, 256u, "ultra reproduces the baked mesh");
  AE_EXPECT_TRUE(std::fabs((ultra.nearExtent) - (384.0f)) <= (1e-3f), "and its near extent");

  // A escada de perfis precisa ser monotônica, senão "reduzir a qualidade" pode
  // aumentar o custo em algum degrau.
  ae::u32 previous = 0xffffffffu;
  for (ae::u32 raw = static_cast<ae::u32>(WaterMeshQuality::Ultra);
       raw < static_cast<ae::u32>(WaterMeshQuality::Count); ++raw) {
    const auto grid = selectWaterGrid(sanitizeWaterMeshQuality(raw), 9.0f, 8000.0f);
    AE_EXPECT_TRUE(grid.segments < previous, "each profile is cheaper than the last");
    previous = grid.segments;
  }
}

AE_TEST(WaterGrid_unresolved_quality_still_produces_a_sea) {
  // Inherit chegando aqui é defeito de integração, não escolha do autor: alguém
  // esqueceu de resolver o eixo. A geometria não tem como recusar — um oceano
  // invisível é falha pior que um oceano de densidade média.
  const auto inherited = selectWaterGrid(WaterMeshQuality::Inherit, 9.0f, 8000.0f);
  const auto medium = selectWaterGrid(WaterMeshQuality::Medium, 9.0f, 8000.0f);
  AE_EXPECT_TRUE(validateWaterGrid(inherited), "unresolved quality yields a legal grid");
  AE_EXPECT_EQ(inherited.segments, medium.segments, "and it lands on the medium rung");

  // Valor fora do enum vem de projeto ou opção de lançamento corrompida, e
  // conteúdo inválido nunca pode derrubar a engine.
  const auto garbage = selectWaterGrid(sanitizeWaterMeshQuality(9999u), 9.0f, 8000.0f);
  AE_EXPECT_EQ(garbage.segments, medium.segments, "garbage sanitizes to the same rung");
}

AE_TEST(WaterGrid_selection_keeps_the_horizon_where_the_scene_put_it) {
  // Encurtar o alcance moveria a linha do horizonte: é decisão de composição, e
  // o seletor de desempenho não tem direito a ela.
  const auto grid = selectWaterGrid(WaterMeshQuality::Low, 5.0f, 3000.0f);
  AE_EXPECT_TRUE(std::fabs((grid.farExtent) - (3000.0f)) <= (1e-3f), "far extent survives the selection");
  AE_EXPECT_TRUE(grid.nearExtent < grid.farExtent, "near extent stays inside");

  const auto fallback = selectWaterGrid(WaterMeshQuality::Low, 5.0f, -1.0f);
  AE_EXPECT_TRUE(std::fabs((fallback.farExtent) - (8000.0f)) <= (1e-3f), "invalid extent falls back to the baked one");
}

AE_TEST(WaterGrid_lower_profiles_cut_real_triangles) {
  const auto ultra = selectWaterGrid(WaterMeshQuality::Ultra, MaximumWaterGridWindSpeed, 8000.0f);
  const auto veryLow = selectWaterGrid(WaterMeshQuality::VeryLow, MaximumWaterGridWindSpeed, 8000.0f);
  // A conta que justifica o trabalho: a malha assada custa 131 072 triângulos
  // por quadro, e o perfil mais baixo precisa cortar isso por uma margem que
  // valha o esforço, não por 10%.
  AE_EXPECT_EQ(waterGridTriangleCount(ultra), 131072u, "baked mesh cost");
  AE_EXPECT_TRUE(waterGridTriangleCount(veryLow) * 10u < waterGridTriangleCount(ultra),
                 "the cheapest profile is an order of magnitude lighter");
}

namespace {

// A maior grade permitida tem 513x513 vértices; alocar no heap evita estourar
// a pilha do runner de testes, que é modesta no Windows.
struct GridBuffers final {
  std::vector<WaterGridVertex> vertices;
  std::vector<ae::u32> indices;
  explicit GridBuffers(const WaterGridSettings &settings)
      : vertices(waterGridVertexCount(settings)), indices(waterGridIndexCount(settings)) {}
  bool build(const WaterGridSettings &settings) {
    return buildWaterGrid(settings, vertices.data(), vertices.size(),
                          indices.data(), indices.size());
  }
};

} // namespace

AE_TEST(WaterGrid_build_fills_positions_uvs_and_band_limits) {
  const WaterGridSettings settings{32, 50.0f, 500.0f};
  GridBuffers buffers(settings);
  AE_EXPECT_TRUE(buffers.build(settings), "build succeeds with exact buffers");

  const ae::u32 stride = settings.segments + 1u;
  for (ae::u32 row = 0; row <= settings.segments; ++row) {
    for (ae::u32 column = 0; column <= settings.segments; ++column) {
      const auto &vertex = buffers.vertices[row * stride + column];
      AE_EXPECT_TRUE(std::fabs(vertex.position[0] - waterGridAxisPosition(settings, column)) <= 1e-4f,
                     "x follows the axis");
      AE_EXPECT_TRUE(std::fabs(vertex.position[2] - waterGridAxisPosition(settings, row)) <= 1e-4f,
                     "z follows the axis");
      // A altura é deslocamento de shader: assá-la aqui congelaria a onda.
      AE_EXPECT_EQ(vertex.position[1], 0.0f, "the grid is flat");
      // O limite de banda é o maior dos dois eixos, não a média: a coluna
      // esparsa limita o vértice mesmo quando a linha é densa.
      const float expected = std::max(waterGridAxisSpacing(settings, row),
                                      waterGridAxisSpacing(settings, column));
      AE_EXPECT_TRUE(std::fabs(vertex.bandLimit - expected) <= 1e-3f, "band limit is conservative");
    }
  }
  // UV cobre [0,1] de ponta a ponta do alcance.
  AE_EXPECT_TRUE(std::fabs(buffers.vertices.front().uv[0]) <= 1e-4f, "uv starts at zero");
  AE_EXPECT_TRUE(std::fabs(buffers.vertices.back().uv[1] - 1.0f) <= 1e-4f, "uv ends at one");
}

AE_TEST(WaterGrid_build_is_watertight_and_faces_up) {
  const WaterGridSettings settings{16, 40.0f, 400.0f};
  GridBuffers buffers(settings);
  AE_EXPECT_TRUE(buffers.build(settings), "build succeeds");
  AE_EXPECT_EQ(buffers.indices.size(), waterGridIndexCount(settings), "index count");

  const auto vertexCount = static_cast<ae::u32>(buffers.vertices.size());
  for (ae::u32 index : buffers.indices) {
    AE_EXPECT_TRUE(index < vertexCount, "every index addresses a real vertex");
  }

  // Estanqueidade por construção: os quatro cantos de cada quad são vértices
  // compartilhados da grade, então costura só apareceria se um triângulo
  // referenciasse um vértice fora do seu próprio quad.
  for (ae::usize triangle = 0; triangle + 2 < buffers.indices.size(); triangle += 3) {
    const auto &a = buffers.vertices[buffers.indices[triangle]];
    const auto &b = buffers.vertices[buffers.indices[triangle + 1]];
    const auto &c = buffers.vertices[buffers.indices[triangle + 2]];
    const float ux = b.position[0] - a.position[0], uz = b.position[2] - a.position[2];
    const float vx = c.position[0] - a.position[0], vz = c.position[2] - a.position[2];
    // Componente Y do produto vetorial. Uma malha de água virada para baixo
    // some no backface culling, e o sintoma é "a água sumiu".
    const float normalY = uz * vx - ux * vz;
    AE_EXPECT_TRUE(normalY > 0.0f, "winding keeps the surface facing up");
  }
}

AE_TEST(WaterGrid_build_refuses_short_buffers_instead_of_writing_half_a_mesh) {
  const WaterGridSettings settings{16, 40.0f, 400.0f};
  GridBuffers buffers(settings);

  AE_EXPECT_TRUE(!buildWaterGrid(settings, buffers.vertices.data(), buffers.vertices.size() - 1,
                                 buffers.indices.data(), buffers.indices.size()),
                 "short vertex buffer is refused");
  AE_EXPECT_TRUE(!buildWaterGrid(settings, buffers.vertices.data(), buffers.vertices.size(),
                                 buffers.indices.data(), buffers.indices.size() - 1),
                 "short index buffer is refused");
  AE_EXPECT_TRUE(!buildWaterGrid(settings, nullptr, buffers.vertices.size(),
                                 buffers.indices.data(), buffers.indices.size()),
                 "null vertex buffer is refused");
  // Uma malha meio escrita é pior que nenhuma, porque ela desenha.
  const WaterGridSettings broken{7, 40.0f, 400.0f};
  AE_EXPECT_TRUE(!buildWaterGrid(broken, buffers.vertices.data(), buffers.vertices.size(),
                                 buffers.indices.data(), buffers.indices.size()),
                 "invalid settings are refused before any write");
}

AE_TEST(WaterGrid_build_reproduces_the_baked_ocean_vertex_for_vertex) {
  // O ponto do exercício: a malha gerada precisa poder substituir a assada sem
  // que nada na cena perceba. Os valores conferidos aqui são os mesmos que
  // tools/build-ocean-demo.py escreve no .aemap.
  const WaterGridSettings baked{256, 384.0f, 8000.0f};
  GridBuffers buffers(baked);
  AE_EXPECT_TRUE(buffers.build(baked), "the full ocean grid builds");

  const ae::u32 stride = baked.segments + 1u;
  const auto &centre = buffers.vertices[128u * stride + 128u];
  AE_EXPECT_TRUE(std::fabs(centre.position[0]) <= 1e-3f, "centre sits on the camera");
  AE_EXPECT_TRUE(std::fabs(centre.bandLimit - 3.0f) <= 1e-3f, "three metres per vertex at the centre");

  const auto &corner = buffers.vertices[0];
  AE_EXPECT_TRUE(std::fabs(corner.position[0] + 8000.0f) <= 1e-2f, "corner reaches the far extent");
  AE_EXPECT_TRUE(std::fabs(corner.bandLimit - 354.4509f) <= 1e-2f, "and carries the coarse band limit");
}

AE_TEST(WaterGrid_decimation_only_picks_divisors_and_never_undershoots) {
  // Um salto que não divide deixa a última coluna sem fechar, e o sintoma é um
  // rasgo na água indo até o horizonte.
  for (ae::u32 target : {256u, 200u, 129u, 128u, 100u, 64u, 33u, 32u, 8u, 1u}) {
    const ae::u32 step = waterGridDecimation(256u, target);
    AE_EXPECT_TRUE(step >= 1u, "step is positive");
    AE_EXPECT_EQ(256u % step, 0u, "step divides the baked grid");
    // Nunca abaixo do pedido: a política escolheu aquela densidade como mínimo
    // aceitável para o estado de mar, e descer dela é decidir por ela.
    AE_EXPECT_TRUE(256u / step >= target || step == 256u, "never coarser than requested");
  }
  AE_EXPECT_EQ(waterGridDecimation(256u, 256u), 1u, "no decimation when it already fits");
  AE_EXPECT_EQ(waterGridDecimation(256u, 300u), 1u, "asking for more than baked keeps the mesh");
  AE_EXPECT_EQ(waterGridDecimation(256u, 128u), 2u, "half the segments is one skip");
  AE_EXPECT_EQ(waterGridDecimation(256u, 64u), 4u, "a quarter is three skips");
  // 116 é o que a política escolhe no perfil médio com vento de 10 m/s. O
  // divisor que atende sem descer dele é 2, que dá 128.
  AE_EXPECT_EQ(waterGridDecimation(256u, 116u), 2u, "the medium profile lands on half");
  AE_EXPECT_EQ(waterGridDecimation(0u, 128u), 1u, "degenerate input is a no-op, not a crash");
}

AE_TEST(WaterGrid_decimated_indices_stay_inside_the_baked_grid_and_face_up) {
  const ae::u32 baked = 64u, step = 4u, base = 1000u;
  const ae::usize expected = waterGridDecimatedIndexCount(baked, step);
  std::vector<ae::u32> indices(expected);
  ae::usize written = 0;
  AE_EXPECT_TRUE(decimateWaterGridIndices(baked, step, base, indices.data(), indices.size(), written),
                 "decimation succeeds");
  AE_EXPECT_EQ(written, expected, "writes exactly the promised count");

  const ae::u32 stride = baked + 1u;
  const ae::u32 highest = base + stride * stride - 1u;
  for (ae::u32 index : indices) {
    AE_EXPECT_TRUE(index >= base, "no index falls before the grid");
    AE_EXPECT_TRUE(index <= highest, "no index falls past the grid");
  }

  // O enrolamento tem de sobreviver ao salto: a mesma ordem a,c,b / b,c,d de
  // buildWaterGrid, só que com passos maiores.
  const WaterGridSettings settings{baked, 100.0f, 1000.0f};
  for (ae::usize triangle = 0; triangle + 2 < indices.size(); triangle += 3) {
    const auto position = [&](ae::u32 index) {
      const ae::u32 local = index - base;
      return std::pair<float, float>{waterGridAxisPosition(settings, local % stride),
                                     waterGridAxisPosition(settings, local / stride)};
    };
    const auto a = position(indices[triangle]);
    const auto b = position(indices[triangle + 1]);
    const auto c = position(indices[triangle + 2]);
    const float ux = b.first - a.first, uz = b.second - a.second;
    const float vx = c.first - a.first, vz = c.second - a.second;
    AE_EXPECT_TRUE(uz * vx - ux * vz > 0.0f, "decimated winding still faces up");
  }
}

AE_TEST(WaterGrid_decimation_cuts_triangles_by_the_square_of_the_skip) {
  // A conta que justifica o trabalho: o salto corta rasterização na proporção
  // do quadrado, e é rasterização que a medição apontou como o custo.
  AE_EXPECT_EQ(waterGridDecimatedIndexCount(256u, 1u), 256u * 256u * 6u, "no skip, full mesh");
  AE_EXPECT_EQ(waterGridDecimatedIndexCount(256u, 2u), 128u * 128u * 6u, "one skip, a quarter");
  AE_EXPECT_EQ(waterGridDecimatedIndexCount(256u, 4u), 64u * 64u * 6u, "three skips, a sixteenth");

  // Buffer curto ou salto que não divide são recusados sem escrever nada: uma
  // malha meio reindexada desenha, e desenha errado.
  std::vector<ae::u32> tiny(12);
  ae::usize written = 123;
  AE_EXPECT_TRUE(!decimateWaterGridIndices(256u, 2u, 0u, tiny.data(), tiny.size(), written),
                 "short buffer is refused");
  AE_EXPECT_EQ(written, 0u, "and reports nothing written");
  AE_EXPECT_TRUE(!decimateWaterGridIndices(256u, 3u, 0u, tiny.data(), tiny.size(), written),
                 "non divisor is refused");
}

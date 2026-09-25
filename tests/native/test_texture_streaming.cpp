// Streaming de mipmaps (S2): o nível residente sai do tamanho projetado na
// tela, o conjunto cabe no orçamento tirando níveis de quem tem menor
// prioridade, e cada quadro troca só até o teto de bytes — nunca zero trocas.
#include "harness.h"
#include "renderer/texture_streaming.h"

#include <cstring>
#include <vector>

using namespace ae;
using namespace ae::renderer;

namespace {
TextureStreamingTexture square(u32 size, i32 priority = 0) {
  TextureStreamingTexture texture;
  texture.width = texture.height = size;
  u32 levels = 1;
  for (u32 s = size; s > 1; s /= 2) ++levels;
  texture.levels = levels;
  texture.priority = priority;
  return texture;
}
// 2000 px de altura com 90° de campo vertical: 1000 px por metro a 1 m.
TextureStreamingView camera(float x = 0) {
  TextureStreamingView view;
  view.position[0] = x;
  view.pixelsPerMeterAtUnitDistance = 1000;
  return view;
}
TextureStreamingUse use(u32 texture, float x, float metersPerUv = 1, float radius = 0) {
  TextureStreamingUse u;
  u.texture = texture;
  u.center[0] = x;
  u.radius = radius;
  u.metersPerUv = metersPerUv;
  return u;
}
} // namespace

AE_TEST(texture_streaming_screen_mip_follows_texels_per_pixel) {
  const auto texture = square(1024);
  const auto view = camera();
  // 1024 texels/m contra 1000 px/m: ainda cabe no mip 0.
  AE_EXPECT_EQ(textureStreamingScreenMip(texture, 1, 1, view), 0u, "a 1 m, detalhe completo");
  // A 8 m: 125 px/m, 8,2 texels por pixel -> mip 3.
  AE_EXPECT_EQ(textureStreamingScreenMip(texture, 1, 8, view), 3u, "a 8 m, três níveis a menos");
  // A mesma distância com a UV cobrindo 4 m: a textura fica 4× mais esparsa.
  AE_EXPECT_EQ(textureStreamingScreenMip(texture, 4, 8, view), 1u, "UV maior pede menos detalhe");
  AE_EXPECT_EQ(textureStreamingScreenMip(texture, 1, 1e6f, view), 10u, "muito longe para no último nível");
  AE_EXPECT_EQ(textureStreamingScreenMip(texture, 0, 8, view), 0u, "sem métrica de UV, detalhe máximo");
  TextureStreamingView ortho;
  ortho.orthographicPixelsPerMeter = 256;
  AE_EXPECT_EQ(textureStreamingScreenMip(texture, 1, 1000, ortho), 2u, "ortográfica ignora a distância");
}

AE_TEST(texture_streaming_plan_keeps_near_detail_and_caps_far_and_unseen) {
  const std::vector textures{square(1024), square(1024), square(1024)};
  const std::vector uses{use(0, 1), use(1, 400)};
  const std::vector views{camera()};
  TextureStreamingSettings settings;
  settings.budgetBytes = ~0ull;
  settings.maxLevelReduction = 3;
  const std::vector<u32> loaded(3, TextureStreamingNotLoaded);
  TextureStreamingPlan plan;
  AE_EXPECT_TRUE(planTextureStreaming(textures, uses, views, settings, loaded, plan), "plano montado");
  AE_EXPECT_EQ(plan.calculatedMip[0], 0u, "perto: mip 0");
  AE_EXPECT_EQ(plan.calculatedMip[1], 3u, "longe: parado na redução máxima");
  AE_EXPECT_EQ(plan.calculatedMip[2], 3u, "sem uso: fim da faixa");
  AE_EXPECT_TRUE(!plan.overBudget && plan.budgetReducedTextures == 0, "orçamento folgado não reduz");
  AE_EXPECT_EQ(plan.desiredBytes, plan.targetBytes, "alvo igual ao desejado");
  AE_EXPECT_EQ(plan.currentBytes, 0ull, "nada carregado ainda");
  AE_EXPECT_EQ(plan.totalBytes, 3 * textureStreamingChainBytes(textures[0], 0), "total conta as cadeias inteiras");
}

AE_TEST(texture_streaming_budget_takes_levels_from_low_priority_first) {
  const std::vector textures{square(1024, 10), square(1024, -5), square(1024, 0)};
  const std::vector uses{use(0, 1), use(1, 1), use(2, 1)};
  const std::vector views{camera()};
  TextureStreamingSettings settings;
  settings.maxLevelReduction = 2;
  const u64 full = textureStreamingChainBytes(textures[0], 0), one = textureStreamingChainBytes(textures[0], 1);
  // Cabe uma cadeia inteira e duas sem o mip 0.
  settings.budgetBytes = full + 2 * one;
  const std::vector<u32> loaded(3, TextureStreamingNotLoaded);
  TextureStreamingPlan plan;
  AE_EXPECT_TRUE(planTextureStreaming(textures, uses, views, settings, loaded, plan), "plano montado");
  AE_EXPECT_EQ(plan.targetMip[0], 0u, "a maior prioridade fica inteira");
  AE_EXPECT_EQ(plan.targetMip[1], 2u, "a menor prioridade desce até a redução máxima antes das outras");
  AE_EXPECT_EQ(plan.targetMip[2], 1u, "a do meio perde só o que faltou para caber");
  AE_EXPECT_TRUE(!plan.overBudget && plan.targetBytes <= settings.budgetBytes, "coube");
  AE_EXPECT_EQ(plan.budgetReducedTextures, 2u, "duas reduzidas pelo orçamento");
  AE_EXPECT_EQ(plan.calculatedMip[1], 0u, "o desejado continua o da tela");

  settings.budgetBytes = 1;
  AE_EXPECT_TRUE(planTextureStreaming(textures, uses, views, settings, loaded, plan), "plano montado");
  for (u32 t = 0; t < 3; ++t) AE_EXPECT_EQ(plan.targetMip[t], 2u, "ninguém passa da redução máxima");
  AE_EXPECT_TRUE(plan.overBudget, "sem caber, o plano diz");
}

AE_TEST(texture_streaming_respects_non_streamed_requested_and_quality_minimum) {
  auto fixed = square(512);
  fixed.streamable = false;
  auto requested = square(512);
  requested.requestedMip = 4;
  const std::vector textures{fixed, requested, square(512)};
  const std::vector uses{use(0, 500), use(2, 1)};
  const std::vector views{camera()};
  TextureStreamingSettings settings;
  settings.budgetBytes = ~0ull;
  settings.minimumMip = 1;
  settings.maxLevelReduction = 2;
  const std::vector<u32> loaded(3, TextureStreamingNotLoaded);
  TextureStreamingPlan plan;
  AE_EXPECT_TRUE(planTextureStreaming(textures, uses, views, settings, loaded, plan), "plano montado");
  AE_EXPECT_EQ(plan.targetMip[0], 1u, "sem streaming: inteira a partir do mínimo da qualidade, mesmo longe");
  AE_EXPECT_EQ(plan.nonStreamingBytes, textureStreamingChainBytes(fixed, 1), "contada fora do streaming");
  AE_EXPECT_EQ(plan.targetMip[1], 4u, "o nível pedido por script vale mesmo sem uso");
  AE_EXPECT_EQ(plan.targetMip[2], 1u, "perto: o mínimo da qualidade, não o mip 0");
  AE_EXPECT_EQ(plan.streamingTextures, 2u, "duas participam do streaming");
}

AE_TEST(texture_streaming_uploads_are_capped_per_frame_and_drops_come_first) {
  const std::vector textures{square(1024), square(1024), square(1024), square(1024)};
  const std::vector uses{use(0, 1), use(1, 1), use(2, 1)};
  const std::vector views{camera()};
  TextureStreamingSettings settings;
  settings.budgetBytes = ~0ull;
  settings.maxLevelReduction = 3;
  settings.uploadBytesPerFrame = textureStreamingChainBytes(textures[0], 0) + 1024;
  // 0..2 perto e carregadas no fim da faixa; 3 sem uso e carregada inteira.
  const std::vector<u32> loaded{3, 3, 3, 0};
  TextureStreamingPlan plan;
  AE_EXPECT_TRUE(planTextureStreaming(textures, uses, views, settings, loaded, plan), "plano montado");
  AE_EXPECT_TRUE(!plan.loads.empty() && plan.loads[0] == 3u, "primeiro a que libera memória");
  AE_EXPECT_EQ(plan.loads.size(), 1u, "a descida já ocupa parte do teto; só ela cabe junto de nada maior");
  AE_EXPECT_EQ(plan.pendingLoads, 3u, "as subidas esperam o próximo quadro");
  AE_EXPECT_EQ(plan.currentBytes, 3 * textureStreamingChainBytes(textures[0], 3) + textureStreamingChainBytes(textures[0], 0),
               "memória atual pelo nível carregado");

  settings.uploadBytesPerFrame = 1;
  const std::vector<u32> nearLoaded{3, 0, 0, 3};
  AE_EXPECT_TRUE(planTextureStreaming(textures, uses, views, settings, nearLoaded, plan), "plano montado");
  AE_EXPECT_EQ(plan.loads.size(), 1u, "teto menor que uma troca ainda deixa uma por quadro");
  AE_EXPECT_EQ(plan.loads[0], 0u, "a textura que falta subir");
}

AE_TEST(texture_streaming_plan_refuses_inconsistent_input) {
  const std::vector textures{square(64)};
  const std::vector views{camera()};
  TextureStreamingPlan plan;
  plan.streamingTextures = 99;
  const std::vector<u32> wrongSize{};
  AE_EXPECT_TRUE(!planTextureStreaming(textures, {}, views, {}, wrongSize, plan), "estado carregado incoerente");
  const std::vector uses{use(3, 1)};
  const std::vector<u32> loaded{TextureStreamingNotLoaded};
  AE_EXPECT_TRUE(!planTextureStreaming(textures, uses, views, {}, loaded, plan), "uso fora da lista");
  AE_EXPECT_EQ(plan.streamingTextures, 99u, "saída intocada na recusa");
}

AE_TEST(texture_streaming_uv_metric_and_model_scale_measure_the_surface) {
  struct Vertex {float position[3];i16 normal[4];i16 tangent[4];float uv[2];float uv1[2];u32 color;};
  static_assert(sizeof(Vertex) == MapVertexStride);
  std::vector<u8> vertices;
  const float corners[4][2]{{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  for (const auto &corner : corners) {
    Vertex vertex{};
    vertex.position[0] = corner[0] * 2;
    vertex.position[2] = corner[1] * 2;
    vertex.uv[0] = corner[0];
    vertex.uv[1] = corner[1];
    const usize offset = vertices.size();
    vertices.resize(offset + sizeof(Vertex));
    std::memcpy(vertices.data() + offset, &vertex, sizeof(Vertex));
  }
  const std::vector<u32> indices{0, 1, 2, 0, 2, 3};
  MapDrawRecord draw{};
  draw.indexCount = 6;
  const float metric = meshUvMetersPerUnit(vertices, indices, draw);
  AE_EXPECT_TRUE(metric > 1.999f && metric < 2.001f, "quadrado de 2 m com UV 0..1: 2 m por unidade de UV");
  draw.firstIndex = 4;
  AE_EXPECT_EQ(meshUvMetersPerUnit(vertices, indices, draw), 0.0f, "índices fora do buffer: sem métrica");
  const float model[16]{3, 0, 0, 0, 0, 3, 0, 0, 0, 0, 3, 0, 5, 6, 7, 1};
  const float scale = textureStreamingModelScale(model);
  AE_EXPECT_TRUE(scale > 2.999f && scale < 3.001f, "escala uniforme 3");
}

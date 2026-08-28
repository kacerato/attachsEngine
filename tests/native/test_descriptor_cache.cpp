#include "harness.h"
#include "rhi/descriptor_cache.h"

using namespace ae;
using namespace ae::rhi;
using namespace ae::test;

AE_TEST(DescritorCache_descricoes_iguais_dao_mesmo_hash) {
  PipelineDesc a{1, 2, 1, 0, 3, true, true, false};
  PipelineDesc b{1, 2, 1, 0, 3, true, true, false};
  AE_EXPECT_TRUE(hashDesc(a) == hashDesc(b), "descricoes byte-a-byte identicas precisam produzir o mesmo hash");
}

AE_TEST(DescritorCache_descricoes_diferentes_dao_hash_diferente) {
  PipelineDesc a{1, 2, 1, 0, 3, true, true, false};
  PipelineDesc b{1, 2, 1, 0, 3, false, true, false};
  AE_EXPECT_TRUE(hashDesc(a) != hashDesc(b), "um unico campo diferente ja deveria mudar o hash (evita colisao trivial)");
}

AE_TEST(DescritorCache_getOrCreate_reusa_objeto_para_mesma_descricao) {
  int factoryCalls = 0;
  DescriptorCache<PipelineDesc, u64> cache([&factoryCalls](const PipelineDesc &) -> u64 {
    ++factoryCalls;
    return static_cast<u64>(factoryCalls);
  });

  PipelineDesc desc{7, 8, 2, 1, 0, false, false, true};
  u64 h1 = cache.getOrCreate(desc);
  u64 h2 = cache.getOrCreate(desc);

  AE_EXPECT_TRUE(h1 == h2, "a segunda consulta com a mesma descricao deve devolver o mesmo handle");
  AE_EXPECT_TRUE(factoryCalls == 1, "a factory so deveria ser chamada uma vez para a mesma descricao");
  AE_EXPECT_TRUE(cache.size() == 1, "cache deveria ter uma unica entrada");
}

AE_TEST(DescritorCache_descricoes_diferentes_criam_entradas_diferentes) {
  int factoryCalls = 0;
  DescriptorCache<SamplerCacheTestDesc, u64> cache([&factoryCalls](const SamplerCacheTestDesc &) -> u64 {
    return static_cast<u64>(++factoryCalls);
  });

  SamplerCacheTestDesc s1{0, 0, 0, 0, 0, false, 0.0f};
  SamplerCacheTestDesc s2{1, 0, 0, 0, 0, false, 0.0f};
  u64 h1 = cache.getOrCreate(s1);
  u64 h2 = cache.getOrCreate(s2);

  AE_EXPECT_TRUE(h1 != h2, "descricoes diferentes devem gerar handles diferentes");
  AE_EXPECT_TRUE(cache.size() == 2, "cache deveria conter as duas entradas distintas");
}

AE_TEST(DescritorCache_render_pass_desc_cache_funciona) {
  DescriptorCache<RenderPassDesc, u64> cache([](const RenderPassDesc &) -> u64 { return 42; });
  RenderPassDesc rp{};
  rp.colorFormatCount = 2;
  rp.colorFormats[0] = 37;
  rp.colorFormats[1] = 38;
  rp.depthFormat = 126;
  rp.sampleCount = 1;

  u64 h1 = cache.getOrCreate(rp);
  u64 h2 = cache.getOrCreate(rp);
  AE_EXPECT_TRUE(h1 == h2 && h1 == 42, "render pass desc identica deve reusar entrada cacheada");
}

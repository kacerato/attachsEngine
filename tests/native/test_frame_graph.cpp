// Política de anexos do frame derivada do render graph.
//
// O que estes testes protegem não é o compilador do grafo (isso é
// test_render_graph.cpp) — é a tradução da topologia do frame em decisões que
// o backend Vulkan precisa tomar de forma consistente em três lugares.
#include "harness.h"
#include "renderer/frame_graph.h"
#include "rhi/memory_allocator.h"

using namespace ae::test;

using ae::renderer::FrameGraphInputs;
using ae::renderer::resolveFrameAttachmentPolicy;

namespace {
FrameGraphInputs inputs(bool hzbEnabled) {
  FrameGraphInputs in{};
  in.width = 2772;
  in.height = 1280;
  in.hzbEnabled = hzbEnabled;
  return in;
}
} // namespace

AE_TEST(frame_graph_sem_leitor_o_depth_nao_vai_a_dram) {
  // Caso padrão da engine hoje: HZB desligado. O depth é escrito e descartado
  // dentro do mesmo render pass, então numa GPU TBDR ele pode viver só na
  // memória do tile. Antes desta política ele era alocado como render target
  // comum, ocupando DRAM que ninguém lia.
  const auto policy = resolveFrameAttachmentPolicy(inputs(false));
  AE_EXPECT_TRUE(policy.valid, "politica compila com extent valido");
  AE_EXPECT_TRUE(!policy.depthStored, "sem leitor, storeOp e DontCare");
  AE_EXPECT_TRUE(!policy.depthSampled, "sem leitor, nao precisa de SAMPLED");
  AE_EXPECT_TRUE(policy.depthMemoryless, "sem leitor, o depth pode ser memoryless");
}

AE_TEST(frame_graph_leitor_fora_do_pass_obriga_store) {
  // Com HZB ligado a redução Hi-Z amostra o depth depois que o render pass
  // principal termina. Descartar o conteúdo aqui foi a causa raiz da oclusão
  // aparentemente aleatória registrada em PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md.
  const auto policy = resolveFrameAttachmentPolicy(inputs(true));
  AE_EXPECT_TRUE(policy.valid, "politica compila com HZB");
  AE_EXPECT_TRUE(policy.depthStored, "com leitor externo, storeOp e Store");
  AE_EXPECT_TRUE(policy.depthSampled, "o leitor amostra o depth");
  AE_EXPECT_TRUE(!policy.depthMemoryless, "conteudo lido fora do pass nao pode ser memoryless");
}

AE_TEST(frame_graph_taa_sozinho_preserva_depth_amostravel) {
  FrameGraphInputs input{};
  input.width = 1920;
  input.height = 1080;
  input.temporalAaEnabled = true;
  const auto policy = resolveFrameAttachmentPolicy(input);
  AE_EXPECT_TRUE(policy.valid, "grafo temporal compila");
  AE_EXPECT_TRUE(policy.depthStored, "reprojecao temporal preserva depth");
  AE_EXPECT_TRUE(policy.depthSampled, "reprojecao temporal exige usage sampled");
  AE_EXPECT_TRUE(!policy.depthMemoryless, "depth lido fora do tile nao e memoryless");
}

AE_TEST(frame_graph_hzb_e_taa_compartilham_o_mesmo_depth_preservado) {
  FrameGraphInputs input{};
  input.width = 1920;
  input.height = 1080;
  input.hzbEnabled = true;
  input.temporalAaEnabled = true;
  const auto policy = resolveFrameAttachmentPolicy(input);
  AE_EXPECT_TRUE(policy.valid, "os dois consumidores coexistem no grafo");
  AE_EXPECT_TRUE(policy.depthStored && policy.depthSampled,
                 "um unico depth atende HZB e TAA sem copia intermediaria");
  AE_EXPECT_TRUE(!policy.depthMemoryless,
                 "depth compartilhado nao pode usar memoria transitoria");
}

AE_TEST(frame_graph_nunca_pede_memoryless_e_sampled_juntos) {
  // Invariante do Vulkan, não preferência: VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT
  // proíbe SAMPLED. Se a política emitisse os dois, a criação da imagem
  // falharia no device — longe de onde a decisão foi tomada.
  for (const bool hzb : {false, true}) {
    const auto policy = resolveFrameAttachmentPolicy(inputs(hzb));
    AE_EXPECT_TRUE(!(policy.depthMemoryless && policy.depthSampled),
                   "memoryless e sampled sao mutuamente exclusivos");
  }
}

AE_TEST(frame_graph_extent_invalido_falha_fechado_sem_politica) {
  // O chamador trata `valid == false` armazenando: nunca produzir conteúdo
  // indefinido é mais importante que economizar banda.
  FrameGraphInputs empty{};
  const auto policy = resolveFrameAttachmentPolicy(empty);
  AE_EXPECT_TRUE(!policy.valid, "extent zerado nao produz politica");
  AE_EXPECT_TRUE(!policy.depthStored, "politica invalida nao afirma nada");
}

AE_TEST(image_desc_recusa_anexo_transitorio_que_seria_lido) {
  // A recusa acontece no contrato, não no device: um transitório amostrável é
  // VUID-VkImageCreateInfo-usage-00963 e falharia na criação da imagem.
  ae::rhi::ImageDesc desc{};
  desc.width = 2772;
  desc.height = 1280;
  desc.format = VK_FORMAT_D32_SFLOAT;
  desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  desc.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  desc.memoryClass = ae::rhi::MemoryClass::RenderTarget;
  desc.transient = true;
  AE_EXPECT_TRUE(ae::rhi::isImageDescValid(desc), "anexo transitorio puro e valido");

  desc.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
  AE_EXPECT_TRUE(!ae::rhi::isImageDescValid(desc), "transitorio + sampled e recusado");

  desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  AE_EXPECT_TRUE(!ae::rhi::isImageDescValid(desc), "transitorio + transfer e recusado");

  desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  desc.transient = false;
  desc.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
  AE_EXPECT_TRUE(ae::rhi::isImageDescValid(desc), "sem transitorio, sampled continua valido");
}

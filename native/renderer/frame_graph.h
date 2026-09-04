// Topologia do frame declarada como render graph, e a política de anexos que
// sai dela.
//
// Existe porque a mesma decisão — "o depth é lido depois do pass principal?" —
// era escrita à mão em três lugares do renderer: o `storeOp` do anexo, a flag
// `VK_IMAGE_USAGE_SAMPLED_BIT` da imagem e a escolha de formato. Três cópias de
// uma política que precisa concordar: se divergirem, o resultado é ou erro de
// validação, ou — pior — banda desperdiçada em silêncio, que é exatamente o que
// o programa de margem gráfica está tentando medir.
//
// É lógica pura sobre `ae::rendergraph`, sem Vulkan: dá para testar a decisão
// inteira nesta máquina, sem device. O backend traduz o resultado para flags.
#pragma once

#include "core/base.h"

namespace ae::renderer {

// O que muda a topologia do frame. Só entra aqui o que altera quem lê o quê —
// resolução não muda a política, mas entra porque o grafo declara extent e um
// extent zerado marcaria um pass de computação.
struct FrameGraphInputs {
  u32 width = 0;
  u32 height = 0;
  // Quando verdadeiro, a cadeia de redução Hi-Z amostra o depth depois que o
  // render pass principal termina.
  bool hzbEnabled = false;
  // TAA reconstrói a posição do fragmento a partir do depth para reprojetar o
  // histórico. É um consumidor independente de HZB e precisa produzir a mesma
  // política STORE+SAMPLED quando HZB está desligado.
  bool temporalAaEnabled = false;
};

// Política resolvida para o anexo de profundidade. O anexo de cor é sempre
// importado (imagem da swapchain) e sempre armazenado, porque o compositor lê
// fora do grafo; por isso não aparece aqui.
struct FrameAttachmentPolicy {
  bool valid = false;
  // storeOp = STORE. Falso quando ninguém lê o depth depois do pass.
  bool depthStored = false;
  // A imagem precisa de VK_IMAGE_USAGE_SAMPLED_BIT (e de um formato que
  // suporte amostragem).
  bool depthSampled = false;
  // A imagem pode ser VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT com memória
  // LAZILY_ALLOCATED: numa GPU TBDR ela vive só na memória do tile e nunca
  // recebe lastro em DRAM. Nunca é verdadeiro junto com depthSampled —
  // um anexo transitório não pode ser amostrado.
  bool depthMemoryless = false;
};

// Compila a topologia do frame e devolve a política. Nunca aborta: entrada
// inválida (extent zerado) devolve `valid == false` e o chamador mantém o
// comportamento conservador — armazenar.
FrameAttachmentPolicy resolveFrameAttachmentPolicy(const FrameGraphInputs &inputs);

} // namespace ae::renderer

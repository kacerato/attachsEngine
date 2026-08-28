// Cache de descritores hasheados (pipeline / sampler / render pass).
//
// Testável sem GPU: a chave é um hash puro sobre a struct de descrição, e o
// cache apenas garante identidade (mesma chave -> mesmo objeto). A criação
// real do objeto Vulkan (VkPipeline etc.) fica atrás de uma factory function
// injetada, para que o teste possa passar uma factory falsa e nunca tocar um
// VkDevice de verdade.
#pragma once

#include "core/base.h"

#include <functional>
#include <unordered_map>

namespace ae::rhi {

// Descrição mínima de um pipeline gráfico relevante para o hash de cache.
// Campos reais (formatos de anexos, estado de blend, etc.) seriam mais
// numerosos; aqui modelamos o suficiente para exercitar hashing/cache.
struct PipelineDesc {
  u64 vertexShaderHash = 0;
  u64 fragmentShaderHash = 0;
  u32 colorAttachmentCount = 0;
  u32 depthFormat = 0;
  u32 topology = 0;
  bool depthTestEnable = false;
  bool depthWriteEnable = false;
  bool blendEnable = false;

  bool operator==(const PipelineDesc &other) const = default;
};

// Nome distinto de rhi::SamplerDesc (resource.h, tipo real de produção com
// campos Vulkan completos como mipmapMode) — este aqui só existe para
// exercitar DescriptorCache com uma segunda Desc simples no teste, nunca
// usado por código de renderização real. Colidiam em ae::rhi antes desta
// renomeação; a colisão só ficou visível quando pipeline_cache.h (que inclui
// este arquivo) passou a ser incluído pela mesma unidade de tradução que já
// incluía resource.h (via device.h → instanced_renderer.h).
struct SamplerCacheTestDesc {
  u32 magFilter = 0;
  u32 minFilter = 0;
  u32 addressModeU = 0;
  u32 addressModeV = 0;
  u32 addressModeW = 0;
  bool anisotropyEnable = false;
  float maxAnisotropy = 0.0f;

  bool operator==(const SamplerCacheTestDesc &other) const = default;
};

struct RenderPassDesc {
  u32 colorFormatCount = 0;
  u32 colorFormats[8] = {};
  u32 depthFormat = 0;
  u32 sampleCount = 1;

  bool operator==(const RenderPassDesc &other) const = default;
};

// FNV-1a de 64 bits sobre os bytes crus da struct. Determinístico e estável
// entre execuções — é o que a Parte 5.3 exige para caches persistentes de
// pipeline entre sessões do editor.
template <typename T> u64 hashDesc(const T &desc) {
  static_assert(std::is_trivially_copyable_v<T>,
                "descrição precisa ser trivialmente copiável para hash por bytes");
  const auto *bytes = reinterpret_cast<const u8 *>(&desc);
  u64 h = 1469598103934665603ull; // offset basis
  for (usize i = 0; i < sizeof(T); ++i) {
    h ^= bytes[i];
    h *= 1099511628211ull; // FNV prime
  }
  return h;
}

// Cache genérico chave-hash -> objeto opaco (Handle). `Handle` é qualquer
// tipo pequeno e copiável (no runtime real, um VkPipeline/VkSampler; nos
// testes, um u64 ou ponteiro simulado). `Factory` cria o objeto só na
// primeira vez que a chave aparece.
template <typename Desc, typename Handle> class DescriptorCache {
public:
  using Factory = std::function<Handle(const Desc &)>;

  explicit DescriptorCache(Factory factory) : factory_(std::move(factory)) {}

  // Retorna o handle cacheado para `desc`, criando-o via factory na primeira
  // consulta. Descrições iguais (mesmo hash e mesmo conteúdo) sempre
  // retornam o mesmo handle — é a garantia testada sem GPU.
  Handle getOrCreate(const Desc &desc) {
    u64 key = hashDesc(desc);
    auto it = entries_.find(key);
    if (it != entries_.end()) {
      ++it->second.hitCount;
      return it->second.handle;
    }
    Handle handle = factory_(desc);
    entries_.emplace(key, Entry{handle, 1});
    return handle;
  }

  usize size() const { return entries_.size(); }

  // Número de vezes que getOrCreate foi chamado para uma chave já existente
  // (incluindo a primeira criação, contada como 1). Útil em teste para
  // provar reuso.
  u32 hitCountFor(const Desc &desc) const {
    u64 key = hashDesc(desc);
    auto it = entries_.find(key);
    return it == entries_.end() ? 0 : it->second.hitCount;
  }

  // Visita todo handle cacheado — usado por donos que precisam destruir
  // recursos reais (ex.: vkDestroyPipeline) antes de descartar o cache, já
  // que DescriptorCache em si é agnóstico ao tipo de Handle e não sabe como
  // liberá-lo.
  template <typename Fn> void forEachHandle(Fn &&fn) const {
    for (const auto &[key, entry] : entries_) {
      fn(entry.handle);
    }
  }

  void clear() { entries_.clear(); }

private:
  struct Entry {
    Handle handle;
    u32 hitCount;
  };

  Factory factory_;
  std::unordered_map<u64, Entry> entries_;
};

} // namespace ae::rhi

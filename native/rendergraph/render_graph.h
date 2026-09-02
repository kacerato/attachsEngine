// Compilador de render graph (Parte 5.3 do plano).
//
// É lógica pura sobre um grafo declarado pelo usuário (passes com recursos
// lidos/escritos) — não toca GPU, portanto é 100% testável nesta máquina.
// O compilador produz um `CompiledGraph`: ordem de execução, recursos
// mortos podados, tempos de vida, aliasing de memória transitória,
// barreiras mínimas, loadOp/storeOp por anexo, marcação memoryless e
// fusão de passes em subpasses.
#pragma once

#include "core/base.h"

#include <optional>
#include <string>
#include <vector>

namespace ae::rendergraph {

using ResourceId = u32;
using PassId = u32;
constexpr ResourceId kInvalidResource = static_cast<ResourceId>(-1);
constexpr PassId kInvalidPass = static_cast<PassId>(-1);

// Descrição de um recurso transitório ou importado. `imported` marca
// recursos que entram/saem do grafo (ex.: backbuffer da swapchain, textura
// de entrada) — nunca são podados e sempre recebem StoreOp::Store no fim,
// porque um consumidor externo pode lê-los depois do grafo terminar.
struct ResourceDesc {
  std::string name;
  bool imported = false;
  bool isDepth = false;   // afeta o layout escolhido para barreiras/anexos
  u64 sizeBytes = 0;      // usado para decidir aliasing
  enum class Kind : u32 { Image, Buffer } kind = Kind::Image;
};

enum class AccessType : u32 {
  Automatic,
  SampledRead,
  UniformRead,
  StorageRead,
  StorageWrite,
  IndirectRead,
  TransferRead,
  TransferWrite,
};

// Um acesso de leitura ou escrita de um pass a um recurso.
// `asInputAttachment` marca leitura no mesmo pixel dentro do mesmo grupo de
// render target (pré-requisito para fusão em subpasses).
// `fullOverwrite` (só em escritas) marca que o pass sobrescreve o recurso
// inteiro, sem depender do conteúdo anterior — habilita LoadOp::DontCare.
struct ResourceAccess {
  ResourceId resource = kInvalidResource;
  bool asInputAttachment = false;
  bool fullOverwrite = false;
  AccessType type = AccessType::Automatic;
};

enum class PassKind : u32 { Automatic, Raster, Compute, Transfer };
enum class ExecutionQueue : u32 { Graphics, Compute };

struct CompileOptions final {
  // So e verdadeiro quando o RHI detectou uma familia compute distinta e o
  // backend possui sincronizacao/ownership entre filas.
  bool asyncComputeAvailable = false;
};

// Descrição de um pass declarado pelo usuário. Automatic preserva o caminho
// raster legado; compute e transfer devem ser declarados explicitamente.
struct PassDesc {
  std::string name;
  u32 extentWidth = 0;
  u32 extentHeight = 0;
  std::vector<ResourceAccess> reads;
  std::vector<ResourceAccess> writes;

  // Dependencia explicita opcional, declarada pelo usuario (ex.: ordenar um
  // pass de debug-draw depois de outro sem compartilhar recurso nenhum).
  // A maioria das dependencias vem implicitamente do acesso a recursos; este
  // campo existe para os casos raros em que isso nao basta — e e também o
  // unico jeito de introduzir um ciclo no grafo, já que dependências por
  // recurso seguem sempre a ordem de declaração dos passes e por isso nunca
  // fecham um ciclo sozinhas.
  std::vector<PassId> explicitDependsOn;
  PassKind kind = PassKind::Automatic;
  // Dispatch metadata is inspectable even when execution is supplied by a
  // backend callback. Zero is invalid for an explicit compute pass.
  u32 dispatchX = 0;
  u32 dispatchY = 0;
  u32 dispatchZ = 0;
  bool preferAsyncCompute = false;
};

enum class LoadOp : u32 { Load, Clear, DontCare };
enum class StoreOp : u32 { Store, DontCare };

enum class ResourceLayout : u32 {
  Undefined,
  ColorAttachment,
  DepthStencilAttachment,
  ShaderReadOnly,
  General,
};

// Barreira mínima entre duas ocorrências consecutivas de acesso ao mesmo
// recurso. Os campos de stage/access são um modelo simplificado (strings
// legíveis) — o mapeamento para VkPipelineStageFlags2/VkAccessFlags2 reais
// é responsabilidade do backend RHI (não testável sem device nesta
// máquina); aqui o que importa e é testável é a decisão de QUANDO inserir
// barreira e qual transição de layout ela carrega.
struct Barrier {
  ResourceId resource = kInvalidResource;
  PassId beforePass = kInvalidPass; // pass cujo acesso termina antes da barreira
  PassId afterPass = kInvalidPass;  // pass cujo acesso começa depois da barreira
  ResourceLayout oldLayout = ResourceLayout::Undefined;
  ResourceLayout newLayout = ResourceLayout::Undefined;
  const char *srcStage = "";
  const char *dstStage = "";
  const char *srcAccess = "";
  const char *dstAccess = "";
};

// Tempo de vida de um recurso transitório: índice (na ordem de execução
// compilada) do primeiro e último pass que o tocam.
struct ResourceLifetime {
  ResourceId resource = kInvalidResource;
  u32 firstUse = 0;
  u32 lastUse = 0;
};

// Um "slot" de memória compartilhada por aliasing: um ou mais recursos
// transitórios com tempos de vida disjuntos ocupam o mesmo espaço.
struct AliasSlot {
  u64 sizeBytes = 0;
  std::vector<ResourceId> members; // na ordem em que passaram a ocupar o slot
};

// Uso de um recurso como anexo dentro de um pass específico: loadOp/storeOp
// resolvidos e se o anexo pode ser memoryless (nunca vai à DRAM).
struct AttachmentUse {
  ResourceId resource = kInvalidResource;
  PassId pass = kInvalidPass;
  LoadOp loadOp = LoadOp::Load;
  StoreOp storeOp = StoreOp::Store;
};

// Grupo de passes fundidos em um único render pass/subpass Vulkan. Grupos
// com um único pass não representam fusão real (ver `merged` abaixo).
struct SubpassGroup {
  std::vector<PassId> passes;
  bool merged = false; // true quando passes.size() > 1
};

// Resultado da compilação bem-sucedida.
struct CompiledGraph {
  std::vector<PassId> executionOrder;      // já podado e topologicamente ordenado
  std::vector<PassId> culledPasses;         // passes mortos, removidos da execução
  std::vector<ResourceLifetime> lifetimes;  // só recursos transitórios vivos
  std::vector<AliasSlot> aliasSlots;
  u64 aliasingSavingsBytes = 0;
  std::vector<Barrier> barriers;
  std::vector<AttachmentUse> attachmentUses;
  std::vector<ResourceId> memorylessResources;
  std::vector<SubpassGroup> subpassGroups;
  std::vector<PassKind> executionKinds; // parallel to executionOrder
  std::vector<ExecutionQueue> executionQueues; // parallel to executionOrder

  // Serializa o grafo compilado em texto legível — base do "render graph
  // inspecionável pelo usuário" citado no plano, e usado nos testes como
  // saída determinística e legível.
  std::string exportText(const class RenderGraph &graph) const;
};

// Erro de compilação estrutural ou de dependência.
struct CompileError {
  std::string message; // em português, pronto para exibir ao usuário
};

// Grafo declarado pelo usuário antes da compilação.
class RenderGraph {
public:
  ResourceId addResource(ResourceDesc desc);
  PassId addPass(PassDesc desc);

  const ResourceDesc &resource(ResourceId id) const { return resources_[id]; }
  const PassDesc &pass(PassId id) const { return passes_[id]; }
  usize resourceCount() const { return resources_.size(); }
  usize passCount() const { return passes_.size(); }

  // Compila o grafo. Em caso de entrada inválida/ciclo, retorna std::nullopt e preenche
  // `outError` com uma mensagem clara (nunca trava/aborta por grafo
  // inválido — conteúdo de usuário nunca deve derrubar o editor).
  std::optional<CompiledGraph> compile(CompileError *outError,
                                       CompileOptions options = {}) const;

private:
  std::vector<ResourceDesc> resources_;
  std::vector<PassDesc> passes_;
};

} // namespace ae::rendergraph

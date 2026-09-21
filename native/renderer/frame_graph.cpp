#include "renderer/frame_graph.h"

#include "rendergraph/render_graph.h"

#include <algorithm>

namespace ae::renderer {
namespace {

using rendergraph::PassDesc;
using rendergraph::ResourceAccess;
using rendergraph::ResourceDesc;
using rendergraph::ResourceId;

ResourceAccess fullWrite(ResourceId resource) {
  ResourceAccess access{};
  access.resource = resource;
  access.fullOverwrite = true;
  return access;
}

ResourceAccess read(ResourceId resource) {
  ResourceAccess access{};
  access.resource = resource;
  return access;
}

} // namespace

FrameAttachmentPolicy resolveFrameAttachmentPolicy(const FrameGraphInputs &inputs) {
  FrameAttachmentPolicy policy{};
  if (inputs.width == 0 || inputs.height == 0) return policy;

  rendergraph::RenderGraph graph;
  // Cor é importada: a imagem vem da swapchain e o compositor a lê depois que
  // o grafo termina, então ela nunca é podada nem descartada.
  const ResourceId color = graph.addResource(
      ResourceDesc{"Swapchain.Color", /*imported=*/true, /*isDepth=*/false, 0});
  // Profundidade é transitória: nasce e morre dentro do frame. Se ninguém a ler
  // depois do pass principal, é justamente ela que pode ficar memoryless.
  const u64 depthBytes = static_cast<u64>(inputs.width) * inputs.height * 4;
  const ResourceId depth = graph.addResource(
      ResourceDesc{"Frame.Depth", /*imported=*/false, /*isDepth=*/true, depthBytes});

  // O frame de hoje é um único render pass contendo Opaque, Coverage, Sky,
  // Transparent e UI. Declará-los como cinco passes de grafo não mudaria
  // decisão nenhuma — todos escrevem os mesmos dois anexos, na mesma extensão —
  // então a topologia declara o que existe de verdade: um pass de cena.
  PassDesc scene;
  scene.name = "Scene";
  scene.extentWidth = inputs.width;
  scene.extentHeight = inputs.height;
  scene.writes.push_back(fullWrite(color));
  scene.writes.push_back(fullWrite(depth));
  graph.addPass(scene);

  if (inputs.hzbEnabled) {
    // A redução Hi-Z amostra o depth como textura comum: precisa ler texels
    // vizinhos, não o pixel corrente do tile. Por isso não funde com a cena, e
    // é essa não-fusão que obriga o depth a ir à DRAM.
    PassDesc hzb;
    hzb.name = "HzbReduce";
    hzb.extentWidth = std::max(1u, inputs.width / 2);
    hzb.extentHeight = std::max(1u, inputs.height / 2);
    hzb.reads.push_back(read(depth));
    // Escreve num recurso importado para não ser podado: a pirâmide é lida no
    // frame seguinte, fora deste grafo.
    const ResourceId pyramid = graph.addResource(
        ResourceDesc{"Hzb.Pyramid", /*imported=*/true, /*isDepth=*/false, 0});
    hzb.writes.push_back(fullWrite(pyramid));
    graph.addPass(hzb);
  }

  if (inputs.temporalAaEnabled) {
    // O resolve temporal é um consumidor próprio, em resolução de apresentação.
    // Modelá-lo separadamente evita que a topologia minta quando HZB e TAA estão
    // ativos juntos e prepara o caminho para o backend gravar ambos via grafo.
    PassDesc temporal;
    temporal.name = "TemporalResolve";
    temporal.extentWidth = inputs.width;
    temporal.extentHeight = inputs.height;
    temporal.reads.push_back(read(depth));
    const ResourceId history = graph.addResource(
        ResourceDesc{"Temporal.History", /*imported=*/true, /*isDepth=*/false, 0});
    temporal.writes.push_back(fullWrite(history));
    graph.addPass(temporal);
  }

  if (inputs.postDepthEnabled) {
    PassDesc post;
    post.name = "PostDepth";
    post.extentWidth = inputs.width;
    post.extentHeight = inputs.height;
    post.reads.push_back(read(depth));
    const ResourceId output = graph.addResource(
        ResourceDesc{"Post.Output", /*imported=*/true, /*isDepth=*/false, 0});
    post.writes.push_back(fullWrite(output));
    graph.addPass(post);
  }

  rendergraph::CompileError error{};
  const auto compiled = graph.compile(&error);
  if (!compiled.has_value()) return policy;

  policy.valid = true;
  policy.depthInputAttachment = inputs.waterDepthInputEnabled;
  for (const auto &use : compiled->attachmentUses) {
    if (use.resource != depth) continue;
    policy.depthStored = use.storeOp == rendergraph::StoreOp::Store;
  }
  policy.depthMemoryless = std::find(compiled->memorylessResources.begin(),
                                     compiled->memorylessResources.end(),
                                     depth) != compiled->memorylessResources.end();
  // Amostrar é o que os passes de HZB/TAA/pós fazem com o depth; o grafo já provou
  // que ele sobrevive ao pass principal quando algum deles existe.
  policy.depthSampled = policy.depthStored;
  // Invariante do Vulkan, não preferência: TRANSIENT_ATTACHMENT proíbe SAMPLED.
  // Se as duas saíssem verdadeiras, a criação da imagem falharia no device.
  if (policy.depthSampled) policy.depthMemoryless = false;
  return policy;
}

} // namespace ae::renderer

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

  rendergraph::CompileError error{};
  const auto compiled = graph.compile(&error);
  if (!compiled.has_value()) return policy;

  policy.valid = true;
  for (const auto &use : compiled->attachmentUses) {
    if (use.resource != depth) continue;
    policy.depthStored = use.storeOp == rendergraph::StoreOp::Store;
  }
  policy.depthMemoryless = std::find(compiled->memorylessResources.begin(),
                                     compiled->memorylessResources.end(),
                                     depth) != compiled->memorylessResources.end();
  // Amostrar é o que o pass de HZB faz com o depth; o grafo já provou que ele
  // sobrevive ao pass principal quando isso acontece.
  policy.depthSampled = policy.depthStored;
  // Invariante do Vulkan, não preferência: TRANSIENT_ATTACHMENT proíbe SAMPLED.
  // Se as duas saíssem verdadeiras, a criação da imagem falharia no device.
  if (policy.depthSampled) policy.depthMemoryless = false;
  return policy;
}

} // namespace ae::renderer

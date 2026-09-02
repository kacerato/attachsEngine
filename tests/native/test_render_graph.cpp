#include "harness.h"
#include "rendergraph/render_graph.h"

#include <cstdio>

using namespace ae;
using namespace ae::rendergraph;
using namespace ae::test;

namespace {
ResourceAccess read(ResourceId r, bool inputAttachment = false) {
  ResourceAccess a; a.resource = r; a.asInputAttachment = inputAttachment; return a;
}
ResourceAccess write(ResourceId r, bool fullOverwrite = false) {
  ResourceAccess a; a.resource = r; a.fullOverwrite = fullOverwrite; return a;
}
ResourceAccess typedRead(ResourceId r, AccessType type) {
  ResourceAccess a; a.resource = r; a.type = type; return a;
}
ResourceAccess typedWrite(ResourceId r, AccessType type, bool fullOverwrite = false) {
  ResourceAccess a; a.resource = r; a.type = type; a.fullOverwrite = fullOverwrite; return a;
}
} // namespace

// --- Casos degenerados ------------------------------------------------

AE_TEST(RenderGraph_grafo_vazio_compila_sem_passes) {
  RenderGraph graph;
  CompileError err;
  auto compiled = graph.compile(&err);
  AE_EXPECT_TRUE(compiled.has_value(), "grafo vazio e um caso valido, nao um erro");
  AE_EXPECT_TRUE(compiled->executionOrder.empty(), "grafo vazio nao tem passes para executar");
}

AE_TEST(RenderGraph_pass_unico_que_escreve_recurso_importado_sobrevive) {
  RenderGraph graph;
  ResourceId backbuffer = graph.addResource({"Backbuffer", /*imported=*/true});
  PassDesc pass; pass.name = "Clear";
  pass.writes.push_back(write(backbuffer, true));
  PassId p = graph.addPass(pass);

  CompileError err;
  auto compiled = graph.compile(&err);
  AE_EXPECT_TRUE(compiled.has_value(), "pass unico escrevendo recurso importado deve compilar");
  AE_EXPECT_TRUE(compiled->executionOrder.size() == 1 && compiled->executionOrder[0] == p, "o unico pass precisa estar na ordem de execucao");
  AE_EXPECT_TRUE(compiled->culledPasses.empty(), "nao ha nada para podar quando o unico pass e necessario");
}

AE_TEST(RenderGraph_todos_os_passes_mortos_sao_podados) {
  RenderGraph graph;
  ResourceId scratch = graph.addResource({"Scratch", /*imported=*/false});
  PassDesc pass; pass.name = "PassInutil";
  pass.writes.push_back(write(scratch, true));
  PassId p = graph.addPass(pass);

  CompileError err;
  auto compiled = graph.compile(&err);
  AE_EXPECT_TRUE(compiled.has_value(), "grafo todo morto ainda e valido, so nao executa nada");
  AE_EXPECT_TRUE(compiled->executionOrder.empty(), "nenhum pass deveria sobreviver: resultado nao e lido nem exportado");
  AE_EXPECT_TRUE(compiled->culledPasses.size() == 1 && compiled->culledPasses[0] == p, "o unico pass deveria estar na lista de podados");
}

AE_TEST(RenderGraph_dois_passes_independentes_sem_ordem_obrigatoria) {
  RenderGraph graph;
  ResourceId r1 = graph.addResource({"OutA", true});
  ResourceId r2 = graph.addResource({"OutB", true});
  PassDesc pa; pa.name = "A"; pa.writes.push_back(write(r1, true));
  PassDesc pb; pb.name = "B"; pb.writes.push_back(write(r2, true));
  PassId a = graph.addPass(pa);
  PassId b = graph.addPass(pb);

  CompileError err;
  auto compiled = graph.compile(&err);
  AE_EXPECT_TRUE(compiled.has_value(), "grafo com passes independentes deve compilar");
  AE_EXPECT_TRUE(compiled->executionOrder.size() == 2, "os dois passes devem sobreviver: escrevem recursos importados");
  bool hasA = compiled->executionOrder[0] == a || compiled->executionOrder[1] == a;
  bool hasB = compiled->executionOrder[0] == b || compiled->executionOrder[1] == b;
  AE_EXPECT_TRUE(hasA && hasB, "ambos os passes precisam aparecer na ordem, em qualquer posicao relativa");
}

// --- 1. Ordenacao topologica -------------------------------------------

AE_TEST(RenderGraph_ordenacao_topologica_respeita_dependencia_de_recurso) {
  RenderGraph graph;
  ResourceId mid = graph.addResource({"Mid", false});
  ResourceId out = graph.addResource({"Out", true});

  PassDesc pa; pa.name = "Produtor"; pa.writes.push_back(write(mid, true));
  PassDesc pb; pb.name = "Consumidor"; pb.reads.push_back(read(mid)); pb.writes.push_back(write(out, true));
  PassId a = graph.addPass(pa);
  PassId b = graph.addPass(pb);

  CompileError err;
  auto compiled = graph.compile(&err);
  AE_EXPECT_TRUE(compiled.has_value(), "grafo linear simples deve compilar");
  AE_EXPECT_TRUE(compiled->executionOrder.size() == 2, "os dois passes devem sobreviver");
  AE_EXPECT_TRUE(compiled->executionOrder[0] == a && compiled->executionOrder[1] == b, "produtor precisa vir antes do consumidor do mesmo recurso");
}

// --- 2. Poda de passes mortos -------------------------------------------

AE_TEST(RenderGraph_pass_cujo_resultado_ninguem_le_e_podado) {
  RenderGraph graph;
  ResourceId used = graph.addResource({"Usado", true});
  ResourceId unused = graph.addResource({"NaoLido", false});

  PassDesc useful; useful.name = "Util"; useful.writes.push_back(write(used, true));
  PassDesc dead; dead.name = "Morto"; dead.writes.push_back(write(unused, true));
  PassId u = graph.addPass(useful);
  PassId d = graph.addPass(dead);

  CompileError err;
  auto compiled = graph.compile(&err);
  AE_EXPECT_TRUE(compiled.has_value(), "deve compilar mesmo com pass morto presente");
  AE_EXPECT_TRUE(compiled->executionOrder.size() == 1 && compiled->executionOrder[0] == u, "so o pass util deveria sobreviver");
  AE_EXPECT_TRUE(compiled->culledPasses.size() == 1 && compiled->culledPasses[0] == d, "o pass morto precisa aparecer na lista de podados");
}

// --- 3. Tempo de vida ----------------------------------------------------

AE_TEST(RenderGraph_tempo_de_vida_cobre_do_primeiro_ao_ultimo_toque) {
  RenderGraph graph;
  ResourceId a = graph.addResource({"A", false});
  ResourceId out = graph.addResource({"Out", true});

  PassDesc p0; p0.name = "P0"; p0.writes.push_back(write(a, true));
  PassDesc p1; p1.name = "P1"; p1.reads.push_back(read(a)); // so passa adiante, nao escreve `a`
  p1.writes.push_back(write(out, true));
  PassDesc p2; p2.name = "P2"; p2.reads.push_back(read(a)); p2.writes.push_back(write(out, false));
  graph.addPass(p0);
  graph.addPass(p1);
  graph.addPass(p2);

  CompileError err;
  auto compiled = graph.compile(&err);
  AE_EXPECT_TRUE(compiled.has_value(), "grafo deveria compilar");
  const ResourceLifetime *lifetime = nullptr;
  for (const auto &lt : compiled->lifetimes) if (lt.resource == a) lifetime = &lt;
  AE_EXPECT_TRUE(lifetime != nullptr, "recurso transitorio A precisa ter tempo de vida calculado");
  AE_EXPECT_TRUE(lifetime->firstUse == 0, "primeiro uso de A deveria ser a posicao 0 (P0 escreve)");
  AE_EXPECT_TRUE(lifetime->lastUse == 2, "ultimo uso de A deveria ser a posicao 2 (P2 le)");
}

// --- 4. Aliasing de memoria transitoria ----------------------------------

AE_TEST(RenderGraph_aliasing_economiza_memoria_com_lifetimes_disjuntos) {
  // Cadeia com um pass extra entre A e B para que as vidas fiquem
  // estritamente disjuntas: A e tocado pela ultima vez em P1 (posicao 1),
  // B so nasce em P2 (posicao 2) — 1 < 2, entao A e B podem compartilhar
  // slot. A economia esperada e exatamente um recurso de tamanho kSize.
  RenderGraph graph;
  constexpr u64 kSize = 1024 * 1024;
  ResourceId a = graph.addResource({"A", false, false, kSize});
  ResourceId b = graph.addResource({"B", false, false, kSize});
  ResourceId mid = graph.addResource({"Mid", true}); // importado so para manter P1 necessario
  ResourceId out = graph.addResource({"Out", true, false, kSize});

  PassDesc p0; p0.name = "GeraA"; p0.writes.push_back(write(a, true));
  PassDesc p1; p1.name = "ConsomeA"; p1.reads.push_back(read(a)); p1.writes.push_back(write(mid, true));
  PassDesc p2; p2.name = "GeraB"; p2.writes.push_back(write(b, true));
  PassDesc p3; p3.name = "GeraOut"; p3.reads.push_back(read(b)); p3.writes.push_back(write(out, true));
  graph.addPass(p0);
  graph.addPass(p1);
  graph.addPass(p2);
  graph.addPass(p3);

  CompileError err;
  auto compiled = graph.compile(&err);
  AE_EXPECT_TRUE(compiled.has_value(), "grafo em cadeia deveria compilar");
  // Total original: A + B = 2 * kSize. Como as vidas sao disjuntas ([0,1] e
  // [2,3]), esperamos 1 slot de kSize e economia de exatamente kSize.
  AE_EXPECT_TRUE(compiled->aliasingSavingsBytes == kSize, "economia esperada e de exatamente um recurso de tamanho kSize");
  AE_EXPECT_TRUE(compiled->aliasSlots.size() == 1, "A e B deveriam compartilhar um unico slot");
  AE_EXPECT_TRUE(compiled->aliasSlots[0].members.size() == 2, "o slot compartilhado deveria conter os dois recursos");
}

AE_TEST(RenderGraph_recursos_com_vidas_sobrepostas_nao_compartilham_slot) {
  RenderGraph graph;
  constexpr u64 kSize = 256;
  ResourceId a = graph.addResource({"A", false, false, kSize});
  ResourceId b = graph.addResource({"B", false, false, kSize});
  ResourceId out = graph.addResource({"Out", true, false, kSize});

  // P0 escreve A e B; P1 le os dois -> vidas de A e B se sobrepoem em [0,1].
  PassDesc p0; p0.name = "GeraAB"; p0.writes.push_back(write(a, true)); p0.writes.push_back(write(b, true));
  PassDesc p1; p1.name = "Combina"; p1.reads.push_back(read(a)); p1.reads.push_back(read(b)); p1.writes.push_back(write(out, true));
  graph.addPass(p0);
  graph.addPass(p1);

  CompileError err;
  auto compiled = graph.compile(&err);
  AE_EXPECT_TRUE(compiled.has_value(), "grafo deveria compilar");
  AE_EXPECT_TRUE(compiled->aliasingSavingsBytes == 0, "vidas sobrepostas nao podem gerar economia de aliasing");
  AE_EXPECT_TRUE(compiled->aliasSlots.size() == 2, "A e B precisam de slots separados por vida sobreposta");
}

// --- 5. Insercao de barreiras ---------------------------------------------

AE_TEST(RenderGraph_write_seguido_de_read_insere_barreira) {
  RenderGraph graph;
  ResourceId r = graph.addResource({"R", false});
  ResourceId out = graph.addResource({"Out", true});
  PassDesc p0; p0.name = "Escreve"; p0.writes.push_back(write(r, true));
  PassDesc p1; p1.name = "Le"; p1.reads.push_back(read(r)); p1.writes.push_back(write(out, true));
  graph.addPass(p0);
  graph.addPass(p1);

  CompileError err;
  auto compiled = graph.compile(&err);
  AE_EXPECT_TRUE(compiled.has_value(), "grafo deveria compilar");
  bool foundBarrier = false;
  for (const auto &b : compiled->barriers) if (b.resource == r) foundBarrier = true;
  AE_EXPECT_TRUE(foundBarrier, "transicao write->read no mesmo recurso precisa gerar barreira");
}

AE_TEST(RenderGraph_read_seguido_de_read_nao_insere_barreira) {
  RenderGraph graph;
  ResourceId r = graph.addResource({"R", true}); // importado: ja chega com conteudo valido
  ResourceId out1 = graph.addResource({"Out1", true});
  ResourceId out2 = graph.addResource({"Out2", true});
  PassDesc p0; p0.name = "Leitor1"; p0.reads.push_back(read(r)); p0.writes.push_back(write(out1, true));
  PassDesc p1; p1.name = "Leitor2"; p1.reads.push_back(read(r)); p1.writes.push_back(write(out2, true));
  graph.addPass(p0);
  graph.addPass(p1);

  CompileError err;
  auto compiled = graph.compile(&err);
  AE_EXPECT_TRUE(compiled.has_value(), "grafo deveria compilar");
  int barrierCountForR = 0;
  for (const auto &b : compiled->barriers) if (b.resource == r) ++barrierCountForR;
  AE_EXPECT_TRUE(barrierCountForR == 0, "duas leituras consecutivas do mesmo recurso nao precisam de barreira entre si");
}

// --- 6. LoadOp/StoreOp -----------------------------------------------------

AE_TEST(RenderGraph_alvo_totalmente_sobrescrito_recebe_load_dont_care) {
  RenderGraph graph;
  ResourceId target = graph.addResource({"Target", true});
  PassDesc p; p.name = "Escreve"; p.writes.push_back(write(target, /*fullOverwrite=*/true));
  graph.addPass(p);

  CompileError err;
  auto compiled = graph.compile(&err);
  AE_EXPECT_TRUE(compiled.has_value(), "grafo deveria compilar");
  const AttachmentUse *use = nullptr;
  for (const auto &u : compiled->attachmentUses) if (u.resource == target) use = &u;
  AE_EXPECT_TRUE(use != nullptr, "attachment use deveria existir para o alvo");
  AE_EXPECT_TRUE(use->loadOp == LoadOp::DontCare, "sobrescrita total nao precisa carregar o conteudo anterior");
}

AE_TEST(RenderGraph_alvo_nao_lido_depois_recebe_store_dont_care) {
  RenderGraph graph;
  ResourceId scratch = graph.addResource({"Scratch", false});
  ResourceId out = graph.addResource({"Out", true});
  PassDesc p0; p0.name = "Escreve"; p0.writes.push_back(write(scratch, true));
  p0.writes.push_back(write(out, true)); // torna o pass necessario (escreve importado)
  graph.addPass(p0);

  CompileError err;
  auto compiled = graph.compile(&err);
  AE_EXPECT_TRUE(compiled.has_value(), "grafo deveria compilar");
  const AttachmentUse *use = nullptr;
  for (const auto &u : compiled->attachmentUses) if (u.resource == scratch) use = &u;
  AE_EXPECT_TRUE(use != nullptr, "attachment use deveria existir para scratch");
  AE_EXPECT_TRUE(use->storeOp == StoreOp::DontCare, "recurso transitorio nunca mais lido nao precisa ser armazenado");
}

AE_TEST(RenderGraph_recurso_importado_sempre_recebe_store) {
  RenderGraph graph;
  ResourceId backbuffer = graph.addResource({"Backbuffer", true});
  PassDesc p; p.name = "Escreve"; p.writes.push_back(write(backbuffer, true));
  graph.addPass(p);

  CompileError err;
  auto compiled = graph.compile(&err);
  const AttachmentUse *use = nullptr;
  for (const auto &u : compiled->attachmentUses) if (u.resource == backbuffer) use = &u;
  AE_EXPECT_TRUE(use != nullptr, "attachment use deveria existir para o backbuffer");
  AE_EXPECT_TRUE(use->storeOp == StoreOp::Store, "recurso importado precisa ser armazenado: consumidor externo pode le-lo depois");
}

// --- 7 e 8. Memoryless + fusao de subpasses (exemplo GBuffer+Lighting) ----

AE_TEST(RenderGraph_gbuffer_e_lighting_fundem_e_gbuffer_fica_memoryless) {
  RenderGraph graph;
  constexpr u32 W = 1920, H = 1080;
  ResourceId albedo = graph.addResource({"GBuffer.Albedo", false, false, W * H * 4});
  ResourceId normal = graph.addResource({"GBuffer.Normal", false, false, W * H * 4});
  ResourceId depth = graph.addResource({"GBuffer.Depth", false, true, W * H * 4});
  ResourceId sceneColor = graph.addResource({"SceneColor", true});

  PassDesc gbuffer; gbuffer.name = "GBuffer";
  gbuffer.extentWidth = W; gbuffer.extentHeight = H;
  gbuffer.writes.push_back(write(albedo, true));
  gbuffer.writes.push_back(write(normal, true));
  gbuffer.writes.push_back(write(depth, true));

  PassDesc lighting; lighting.name = "Lighting";
  lighting.extentWidth = W; lighting.extentHeight = H;
  lighting.reads.push_back(read(albedo, /*asInputAttachment=*/true));
  lighting.reads.push_back(read(normal, /*asInputAttachment=*/true));
  lighting.reads.push_back(read(depth, /*asInputAttachment=*/true));
  lighting.writes.push_back(write(sceneColor, true));

  graph.addPass(gbuffer);
  graph.addPass(lighting);

  CompileError err;
  auto compiled = graph.compile(&err);
  AE_EXPECT_TRUE(compiled.has_value(), "exemplo GBuffer+Lighting do plano deve compilar");
  AE_EXPECT_TRUE(compiled->subpassGroups.size() == 1, "GBuffer e Lighting deveriam fundir em um unico grupo de subpass");
  AE_EXPECT_TRUE(compiled->subpassGroups[0].merged, "o grupo unico precisa estar marcado como fundido");
  AE_EXPECT_TRUE(compiled->subpassGroups[0].passes.size() == 2, "o grupo fundido precisa conter os dois passes");

  bool albedoMemoryless = false, normalMemoryless = false, depthMemoryless = false;
  for (ResourceId r : compiled->memorylessResources) {
    if (r == albedo) albedoMemoryless = true;
    if (r == normal) normalMemoryless = true;
    if (r == depth) depthMemoryless = true;
  }
  AE_EXPECT_TRUE(albedoMemoryless, "GBuffer.Albedo so e usado dentro do grupo fundido: deveria ser memoryless");
  AE_EXPECT_TRUE(normalMemoryless, "GBuffer.Normal so e usado dentro do grupo fundido: deveria ser memoryless");
  AE_EXPECT_TRUE(depthMemoryless, "GBuffer.Depth so e usado dentro do grupo fundido: deveria ser memoryless");
}

AE_TEST(RenderGraph_pass_que_amostra_textura_arbitraria_nao_funde) {
  // Um pass de pos-processamento que amostra o resultado do pass anterior
  // como textura comum (nao input attachment) nao pode fundir: precisa
  // poder ler qualquer pixel, nao so o pixel corrente do tile.
  RenderGraph graph;
  constexpr u32 W = 640, H = 360;
  ResourceId colorA = graph.addResource({"ColorA", false, false, W * H * 4});
  ResourceId colorB = graph.addResource({"ColorB", true});

  PassDesc passA; passA.name = "Cena";
  passA.extentWidth = W; passA.extentHeight = H;
  passA.writes.push_back(write(colorA, true));

  PassDesc blur; blur.name = "BlurAmostraTexturaCompleta";
  blur.extentWidth = W; blur.extentHeight = H;
  blur.reads.push_back(read(colorA, /*asInputAttachment=*/false)); // amostragem livre
  blur.writes.push_back(write(colorB, true));

  graph.addPass(passA);
  graph.addPass(blur);

  CompileError err;
  auto compiled = graph.compile(&err);
  AE_EXPECT_TRUE(compiled.has_value(), "grafo deveria compilar");
  AE_EXPECT_TRUE(compiled->subpassGroups.size() == 2, "leitura via amostragem livre nao pode fundir: dois grupos separados");
}

// --- 9. Deteccao de ciclo ---------------------------------------------------

AE_TEST(RenderGraph_ciclo_e_detectado_com_mensagem_clara) {
  // Dependencias por recurso sozinhas nunca fecham um ciclo (sempre seguem a
  // ordem de declaracao dos passes) — por isso o unico jeito de testar
  // deteccao de ciclo e via dependencia explicita conflitante: P0 escreve
  // `a`, P1 le `a` (aresta implicita P0->P1), e P0 declara depender
  // explicitamente de P1 (aresta P1->P0) — fechando o ciclo P0<->P1.
  // Os ids sao previsiveis (0, 1, 2, ... na ordem de addPass), entao P0
  // pode referenciar o id de P1 antes mesmo de P1 ser adicionado.
  RenderGraph graph;
  ResourceId a = graph.addResource({"A", false});

  constexpr PassId kFuturePassP1 = 1;
  PassDesc p0; p0.name = "P0";
  p0.writes.push_back(write(a, true));
  p0.explicitDependsOn.push_back(kFuturePassP1);
  PassDesc p1; p1.name = "P1";
  p1.reads.push_back(read(a));

  graph.addPass(p0); // vira id 0
  graph.addPass(p1); // vira id 1, conforme esperado por kFuturePassP1

  CompileError err;
  auto compiled = graph.compile(&err);
  AE_EXPECT_TRUE(!compiled.has_value(), "grafo ciclico nao deveria produzir um plano de execucao");
  AE_EXPECT_TRUE(!err.message.empty(), "erro de ciclo precisa vir com mensagem clara, nao silencioso");
}

// --- 10. Exportacao textual ---------------------------------------------

AE_TEST(RenderGraph_exportText_produz_saida_nao_vazia_e_legivel) {
  RenderGraph graph;
  ResourceId mid = graph.addResource({"Mid", false, false, 1024});
  ResourceId out = graph.addResource({"Out", true});
  PassDesc p0; p0.name = "Produtor"; p0.writes.push_back(write(mid, true));
  PassDesc p1; p1.name = "Consumidor"; p1.reads.push_back(read(mid)); p1.writes.push_back(write(out, true));
  graph.addPass(p0);
  graph.addPass(p1);

  CompileError err;
  auto compiled = graph.compile(&err);
  AE_EXPECT_TRUE(compiled.has_value(), "grafo deveria compilar");
  std::string text = compiled->exportText(graph);
  AE_EXPECT_TRUE(!text.empty(), "exportText nao deveria produzir string vazia");
  AE_EXPECT_TRUE(text.find("Produtor") != std::string::npos, "saida precisa citar o nome dos passes");
  AE_EXPECT_TRUE(text.find("Consumidor") != std::string::npos, "saida precisa citar o nome dos passes");
  AE_EXPECT_TRUE(text.find("Mid") != std::string::npos, "saida precisa citar o nome dos recursos");
}

AE_TEST(RenderGraph_compute_expõe_dispatch_e_barreira_storage) {
  RenderGraph graph;
  ResourceDesc storageDesc{"Storage", false, false, 4096};
  storageDesc.kind = ResourceDesc::Kind::Buffer;
  ResourceId storage = graph.addResource(storageDesc);
  ResourceDesc outputDesc{"Output", true, false, 4096};
  outputDesc.kind = ResourceDesc::Kind::Buffer;
  ResourceId output = graph.addResource(outputDesc);

  PassDesc producer; producer.name = "ComputeProducer"; producer.kind = PassKind::Compute;
  producer.dispatchX = 16; producer.dispatchY = 8; producer.dispatchZ = 1;
  producer.preferAsyncCompute = true;
  producer.writes.push_back(typedWrite(storage, AccessType::StorageWrite, true));
  PassDesc consumer; consumer.name = "ComputeConsumer"; consumer.kind = PassKind::Compute;
  consumer.dispatchX = 4; consumer.dispatchY = 1; consumer.dispatchZ = 1;
  consumer.reads.push_back(typedRead(storage, AccessType::StorageRead));
  consumer.writes.push_back(typedWrite(output, AccessType::StorageWrite, true));
  graph.addPass(producer); graph.addPass(consumer);

  CompileError err;
  auto compiled = graph.compile(&err);
  AE_EXPECT_TRUE(compiled.has_value(), "cadeia compute valida deve compilar");
  AE_EXPECT_TRUE(compiled->executionKinds.size() == 2 &&
                 compiled->executionKinds[0] == PassKind::Compute &&
                 compiled->executionKinds[1] == PassKind::Compute,
                 "backend precisa receber compute como tipo explicito");
  const Barrier *barrier = nullptr;
  for (const auto &candidate : compiled->barriers) if (candidate.resource == storage) barrier = &candidate;
  AE_EXPECT_TRUE(barrier != nullptr, "storage write seguido de storage read exige barreira");
  AE_EXPECT_TRUE(std::string(barrier->srcStage) == "ComputeShader" &&
                 std::string(barrier->dstStage) == "ComputeShader",
                 "barreira compute deve usar estagio compute nos dois lados");
  AE_EXPECT_TRUE(barrier->oldLayout == ResourceLayout::General &&
                 barrier->newLayout == ResourceLayout::General,
                 "storage permanece em layout General");
  AE_EXPECT_TRUE(compiled->attachmentUses.empty(), "buffers compute nao sao attachments raster");
  AE_EXPECT_TRUE(compiled->memorylessResources.empty(), "buffers compute nao podem ser memoryless attachments");
  AE_EXPECT_TRUE(compiled->exportText(graph).find("dispatch=16x8x1, async-preferred") != std::string::npos,
                 "exportacao deve tornar dispatch e preferencia async inspecionaveis");
}

AE_TEST(RenderGraph_compute_para_indirect_draw_gera_barreira_correta) {
  RenderGraph graph;
  ResourceDesc argsDesc{"IndirectArgs", false, false, 64};
  argsDesc.kind = ResourceDesc::Kind::Buffer;
  ResourceId args = graph.addResource(argsDesc);
  ResourceId backbuffer = graph.addResource({"Backbuffer", true});
  PassDesc cull; cull.name = "GpuCull"; cull.kind = PassKind::Compute;
  cull.dispatchX = 32; cull.dispatchY = 1; cull.dispatchZ = 1;
  cull.writes.push_back(typedWrite(args, AccessType::StorageWrite, true));
  PassDesc draw; draw.name = "IndirectDraw"; draw.kind = PassKind::Raster;
  draw.extentWidth = 1280; draw.extentHeight = 720;
  draw.reads.push_back(typedRead(args, AccessType::IndirectRead));
  draw.writes.push_back(write(backbuffer, true));
  graph.addPass(cull); graph.addPass(draw);
  CompileError err;
  auto compiled = graph.compile(&err);
  AE_EXPECT_TRUE(compiled.has_value(), "compute seguido de draw indireto deve compilar");
  const Barrier *barrier = nullptr;
  for (const auto &candidate : compiled->barriers) if (candidate.resource == args) barrier = &candidate;
  AE_EXPECT_TRUE(barrier != nullptr, "argumentos indiretos exigem dependencia visivel");
  AE_EXPECT_TRUE(std::string(barrier->dstStage) == "DrawIndirect" &&
                 std::string(barrier->dstAccess) == "IndirectCommandRead",
                 "consumo indireto precisa de stage/access especificos");
}

AE_TEST(RenderGraph_compute_explicitamente_nulo_e_rejeitado) {
  RenderGraph graph;
  PassDesc pass; pass.name = "InvalidCompute"; pass.kind = PassKind::Compute;
  graph.addPass(pass);
  CompileError err;
  auto compiled = graph.compile(&err);
  AE_EXPECT_TRUE(!compiled.has_value(), "dispatch zero nao pode chegar ao backend");
  AE_EXPECT_TRUE(err.message.find("dispatch nulo") != std::string::npos,
                 "erro deve explicar a causa para editor e debugging");
}

AE_TEST(RenderGraph_async_compute_so_e_agendado_quando_capability_existe) {
  RenderGraph graph;
  ResourceDesc outputDesc{"Output", true, false, 64};
  outputDesc.kind = ResourceDesc::Kind::Buffer;
  const ResourceId output = graph.addResource(outputDesc);
  PassDesc pass; pass.name = "AsyncCandidate"; pass.kind = PassKind::Compute;
  pass.dispatchX = 1; pass.dispatchY = 1; pass.dispatchZ = 1;
  pass.preferAsyncCompute = true;
  pass.writes.push_back(typedWrite(output, AccessType::StorageWrite, true));
  graph.addPass(pass);

  CompileError err;
  auto fallback = graph.compile(&err, {.asyncComputeAvailable = false});
  auto asynchronous = graph.compile(&err, {.asyncComputeAvailable = true});
  AE_EXPECT_TRUE(fallback.has_value() && asynchronous.has_value(),
                 "mesmo grafo deve compilar nos dois perfis");
  AE_EXPECT_TRUE(fallback->executionQueues[0] == ExecutionQueue::Graphics,
                 "sem fila dedicada compute usa a fila grafica universal");
  AE_EXPECT_TRUE(asynchronous->executionQueues[0] == ExecutionQueue::Compute,
                 "capability real e preferencia do pass autorizam fila compute");
}

AE_TEST(RenderGraph_compute_sem_preferencia_permanece_na_fila_grafica) {
  RenderGraph graph;
  ResourceDesc outputDesc{"Output", true, false, 64};
  outputDesc.kind = ResourceDesc::Kind::Buffer;
  const ResourceId output = graph.addResource(outputDesc);
  PassDesc pass; pass.name = "SerialCompute"; pass.kind = PassKind::Compute;
  pass.dispatchX = 1; pass.dispatchY = 1; pass.dispatchZ = 1;
  pass.writes.push_back(typedWrite(output, AccessType::StorageWrite, true));
  graph.addPass(pass);
  CompileError err;
  auto compiled = graph.compile(&err, {.asyncComputeAvailable = true});
  AE_EXPECT_TRUE(compiled.has_value() &&
                 compiled->executionQueues[0] == ExecutionQueue::Graphics,
                 "async compute e opt-in por pass, nao uma migracao global perigosa");
}

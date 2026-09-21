// A matriz de propriedades do plano universal de cenários, verificada no host.
//
// Estes testes existem para que uma propriedade incompleta não chegue ao
// aparelho: sem identidade, sem consumidor, com identidade repetida ou apoiada
// numa capacidade que o motor ainda não implementa. É a regra do plano ("uma
// linha sem consumidor não pode avançar para implementada") escrita como build.
#include "harness.h"

#include "core/engine_capability.h"
#include "renderer/punctual_lights.h"
#include "scene/component_reflection.h"

#include <algorithm>

using namespace ae;

namespace {
const scene::PropertyContract *findContract(const std::vector<scene::PropertyContract> &rows,
                                            std::string_view type, std::string_view property) {
  for (const auto &row : rows)
    if (row.typeId == type && row.propertyId == property) return &row;
  return nullptr;
}
} // namespace

AE_TEST(component_contracts_are_complete_for_every_registered_type) {
  const auto issues = scene::auditComponentContracts();
  for (const auto &issue : issues)
    std::fprintf(stderr, "  contrato incompleto: %.*s.%.*s — %s\n", static_cast<int>(issue.typeId.size()),
                 issue.typeId.data(), static_cast<int>(issue.propertyId.size()), issue.propertyId.data(),
                 issue.message.c_str());
  AE_EXPECT_TRUE(issues.empty(), "há propriedades sem identidade, consumidor ou capacidade válida");
}

AE_TEST(component_matrix_reports_default_domain_and_invalidation) {
  const auto rows = scene::allComponentContracts();
  AE_EXPECT_TRUE(rows.size() > 40, "a matriz deve cobrir todos os tipos registrados");

  const auto *intensity = findContract(rows, "astra.render.light", "intensity");
  AE_EXPECT_TRUE(intensity != nullptr, "luz deve publicar a intensidade");
  AE_EXPECT_TRUE(intensity->kind == scene::PropertyKind::Number, "intensidade é numérica");
  AE_EXPECT_EQ(intensity->defaultValue, std::string("1000"), "o padrão vem do componente recém-criado");
  AE_EXPECT_TRUE(intensity->invalidates & scene::Invalidate::LightCluster, "mudar a luz refaz a seleção do quadro");
  AE_EXPECT_TRUE(!intensity->consumer.empty(), "toda propriedade herda o consumidor do componente");

  // Propriedade condicional continua sendo propriedade: ela aparece na matriz
  // com o domínio inteiro e a marca de condicional, não sumindo do contrato.
  const auto *cone = findContract(rows, "astra.render.light", "outer_angle");
  AE_EXPECT_TRUE(cone && cone->conditional, "o cone só é visível em spot, mas está no contrato");

  const auto *owner = findContract(rows, "astra.physics.collider", "owner");
  AE_EXPECT_TRUE(owner != nullptr, "o colisor publica o corpo proprietário");
  AE_EXPECT_TRUE(owner->kind == scene::PropertyKind::Reference, "proprietário é referência tipada");
  AE_EXPECT_TRUE(owner->domain.find("astra.physics.body") != std::string::npos,
                 "o domínio da referência nomeia o tipo exigido");

  const auto *shape = findContract(rows, "astra.physics.collider", "shape");
  AE_EXPECT_TRUE(shape && shape->kind == scene::PropertyKind::Enum, "forma é enumeração");
  AE_EXPECT_EQ(shape->defaultValue, std::string("Caixa"), "o padrão enumerado vem pelo nome da opção");
  AE_EXPECT_TRUE(scene::Collider::descriptor.resourceBindings.size()==1 &&
                     scene::Collider::descriptor.resourceBindings[0].id=="collision_mesh" &&
                     scene::Collider::descriptor.resourceBindings[0].kind==resources::AssetType::Mesh,
                 "o colisor publica a malha física como binding de recurso tipado");
  const auto *hullTolerance=findContract(rows,"astra.physics.collider","hull_tolerance");
  const auto *activeEdge=findContract(rows,"astra.physics.collider","active_edge_angle");
  const auto *weld=findContract(rows,"astra.physics.collider","weld_vertices");
  const auto *optimize=findContract(rows,"astra.physics.collider","optimize_cooking");
  AE_EXPECT_TRUE(hullTolerance&&activeEdge&&weld&&optimize,"cooking do colisor usa o contrato refletido comum");
  AE_EXPECT_TRUE(hullTolerance->conditional&&activeEdge->conditional&&weld->conditional&&optimize->conditional,
                 "controles de cooking aparecem somente no modo de malha pertinente");

  // O comportamento em C# passou a ter identidade para o próprio interruptor:
  // desligar por API, preset ou animação precisa do mesmo caminho do dedo.
  const auto *behavior = findContract(rows, "astra.script.behavior", "enabled");
  AE_EXPECT_TRUE(behavior != nullptr, "o comportamento publica o próprio estado de execução");
  AE_EXPECT_TRUE(behavior->invalidates & scene::Invalidate::Script, "religar o comportamento é a invalidação");
}

AE_TEST(engine_capabilities_agree_with_the_renderer_tables) {
  const auto *punctualShadow = core::findEngineCapability("render.shadow.punctual");
  AE_EXPECT_TRUE(punctualShadow != nullptr, "a capacidade de sombra local é declarada");
  AE_EXPECT_TRUE(punctualShadow->state == core::CapabilityState::Planned,
                 "não há passe de sombra para luz local");
  AE_EXPECT_TRUE(!renderer::lightCastsShadow(renderer::LightModality::Point),
                 "a tabela do renderer concorda com o registro");
  AE_EXPECT_TRUE(renderer::lightCastsShadow(renderer::LightModality::Directional),
                 "o sol tem cascatas implementadas");

  // Uma capacidade planejada nunca pode ser autorável: é o botão sem shader.
  AE_EXPECT_TRUE(!core::engineCapabilityAuthorable("render.shadow.punctual"), "planejada não é autorável");
  AE_EXPECT_TRUE(core::engineCapabilityAuthorable("render.aa.temporal"),
                 "limitada pelo aparelho continua autorável, com degradação relatada");
  AE_EXPECT_TRUE(core::engineCapabilityAuthorable(""), "sem capacidade declarada é sempre autorável");
  AE_EXPECT_TRUE(!core::engineCapabilityDeclared("render.invencao.inexistente"),
                 "uma capacidade não registrada é recusada, nunca assumida");
}

AE_TEST(component_invalidation_is_reported_in_words) {
  const auto text = scene::describeInvalidation(scene::Invalidate::Draw | scene::Invalidate::MaterialDescriptor);
  AE_EXPECT_TRUE(text.find("desenho") != std::string::npos && text.find("material") != std::string::npos,
                 "o relatório nomeia cada derivado atingido");
  AE_EXPECT_EQ(scene::describeInvalidation(0), std::string("nada"), "sem bits, nada é reconstruído");
}

// Regenera o documento da matriz a partir do código:
//
//   aether_tests --write-property-matrix docs/componentes/MATRIZ-PROPRIEDADES.md
//
// A tabela no repositório é uma cópia publicada desta geração, nunca uma
// segunda fonte de verdade — é por isso que ela é escrita por ferramenta e não
// por mão.
int writePropertyMatrix(const char *path) {
  const auto text = scene::componentMatrixMarkdown();
  std::FILE *file = std::fopen(path, "wb");
  if (!file) {
    std::fprintf(stderr, "não foi possível escrever %s\n", path);
    return 1;
  }
  const bool ok = std::fwrite(text.data(), 1, text.size(), file) == text.size();
  std::fclose(file);
  if (!ok) std::fprintf(stderr, "escrita incompleta em %s\n", path);
  return ok ? 0 : 1;
}

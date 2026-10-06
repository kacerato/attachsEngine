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
#include "editor/editor_component_catalog.h"
#include "scene/component_api_csharp.h"

#include <fstream>
#include <sstream>

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

// Unity não tem teto de componentes por objeto; a Astra tem 64 (formato e
// memória). O teto é atingível com qualquer combinação que caiba e a recusa
// diz o número, em vez de um "limite" sem valor.
AE_TEST(component_limit_is_reachable_and_the_refusal_names_it) {
  scene::Components components;
  for(u32 i=0;i<scene::Components::MaximumCount;++i) {
    auto plan=scene::planComponentAddition(components,"astra.time.timer");
    AE_EXPECT_TRUE(plan.ready,"cabe até o teto");
    if(!plan.ready) return;
    components=std::move(plan.candidate);
  }
  AE_EXPECT_EQ(components.size(),static_cast<usize>(scene::Components::MaximumCount),"64 instâncias");
  const auto refused=scene::planComponentAddition(components,"astra.time.timer");
  AE_EXPECT_TRUE(!refused.ready && refused.error && std::string(refused.error).find("64")!=std::string::npos,
                 "a recusa cita o teto");
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
  // Desde o atlas local (G6-B) existe passe: quadtree com um mapa por spot e
  // seis faces por pontual. A tabela do renderer e o registro têm de dizer a
  // MESMA coisa — é o que impede um controle sem shader atrás.
  AE_EXPECT_TRUE(punctualShadow->state == core::CapabilityState::Implemented,
                 "a sombra de luz local tem passe");
  AE_EXPECT_TRUE(renderer::lightCastsShadow(renderer::LightModality::Point) &&
                 renderer::lightCastsShadow(renderer::LightModality::Spot),
                 "a tabela do renderer concorda com o registro");
  AE_EXPECT_TRUE(renderer::lightCastsShadow(renderer::LightModality::Directional),
                 "o sol tem cascatas implementadas");

  AE_EXPECT_TRUE(core::engineCapabilityAuthorable("render.shadow.punctual"), "implementada é autorável");
  // Uma capacidade planejada nunca pode ser autorável: é o botão sem shader.
  AE_EXPECT_TRUE(!core::engineCapabilityAuthorable("render.probe.reflection"), "planejada não é autorável");
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

// Registro por família: cada tipo entra uma vez, com família, ícone que existe
// no atlas e referência oficial com versão. É isso que permite o Add, o
// Inspector e a API crescerem lendo só o schema.
AE_TEST(every_registered_component_has_family_icon_and_versioned_reference) {
  AE_EXPECT_TRUE(scene::componentSchemaIdsUnique(), "nenhum id de componente registrado duas vezes");
  for (const auto &schema : scene::componentSchemas) {
    AE_EXPECT_TRUE(schema.type != nullptr, "entrada do registro tem descritor");
    AE_EXPECT_TRUE(schema.family < scene::ComponentFamily::Count, "família declarada");
    AE_EXPECT_TRUE(*scene::componentFamilyName(schema.family) != 0, "família tem nome");
    AE_EXPECT_TRUE(!schema.subfamily.empty(), "subfamília declarada");
    AE_EXPECT_TRUE(editor::editorIconByName(schema.icon) != ui::UiIcon::None, "ícone existe no atlas");
    const bool unity = schema.reference.find("docs.unity3d.com/6000.0/") != std::string_view::npos ||
                       schema.reference.find("docs.unity3d.com/Packages/") != std::string_view::npos;
    const bool godot = schema.reference.find("docs.godotengine.org/en/4.5/") != std::string_view::npos;
    AE_EXPECT_TRUE(unity || godot, "referência oficial com versão fixada");
  }
  // Ícones do Add distinguem os tipos: dois tipos com o mesmo desenho deixam a
  // lista ilegível quando ela cresce.
  for (usize i = 0; i < editor::editorComponentCatalog.size(); ++i)
    for (usize j = 0; j < i; ++j)
      AE_EXPECT_TRUE(editor::editorComponentCatalog[i].icon != editor::editorComponentCatalog[j].icon,
                     "ícone exclusivo por tipo anexável");
}

// Fachada C# gerada do schema:
//
//   aether_tests --write-component-api managed/Astra.Scripting/Generated/Components.g.cs
int writeComponentApi(const char *path) {
  const auto text = scene::componentCSharpApi();
  std::FILE *file = std::fopen(path, "wb");
  if (!file) {
    std::fprintf(stderr, "não foi possível escrever %s\n", path);
    return 1;
  }
  const bool ok = std::fwrite(text.data(), 1, text.size(), file) == text.size();
  std::fclose(file);
  return ok ? 0 : 1;
}

// O arquivo no repositório é a geração publicada: um id de propriedade que
// muda no C++ sem regenerar falharia só no aparelho, com o C# ainda compilando.
AE_TEST(generated_csharp_component_api_matches_the_schema_registry) {
  std::ifstream file(std::string(AETHER_REPOSITORY_ROOT) + "/managed/Astra.Scripting/Generated/Components.g.cs",
                     std::ios::binary);
  AE_EXPECT_TRUE(file.good(), "fachada gerada existe no repositório");
  std::stringstream content;content << file.rdbuf();
  std::string committed = content.str(), normalized;
  for (const char c : committed) if (c != '\r') normalized.push_back(c);
  AE_EXPECT_TRUE(normalized == scene::componentCSharpApi(),
                 "Components.g.cs em dia com o schema; rode aether_tests --write-component-api");
  for(const auto &schema:scene::componentSchemas) {
    AE_EXPECT_TRUE(!schema.listedInAdd||!schema.apiName.empty(),"todo tipo anexável tem fachada; metadados autorais são criados pelo seu fluxo real");
  }
}

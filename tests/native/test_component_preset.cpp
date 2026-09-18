// Aplicação por campo, recursos declarados e remapeamento entre projetos.
//
// O que estes testes protegem é a diferença entre "aplicar um preset" e
// "sobrescrever o objeto": um preset aplicado em um objeto já ajustado precisa
// levar o que o autor escolheu e deixar o resto intacto — e precisa recusar por
// inteiro quando a combinação escolhida deixaria o componente inválido.
#include "harness.h"

#include "scene/component_preset.h"
#include "scene/light.h"
#include "scene/mesh_renderer.h"

using namespace ae;

namespace {
const scene::FieldDelta *findDelta(const std::vector<scene::FieldDelta> &rows, std::string_view id, u32 slot = 0) {
  for (const auto &row : rows)
    if (row.address.id == id && row.address.slot == slot) return &row;
  return nullptr;
}
resources::AssetGuid guid(u64 value) { return {0, value}; }
} // namespace

AE_TEST(component_delta_reports_every_field_and_marks_what_changes) {
  scene::Light current, candidate;
  candidate.intensity = 42;
  candidate.kind = scene::LightKind::Spot;
  const auto delta = scene::componentDelta(current, candidate);

  const auto *intensity = findDelta(delta, "intensity");
  AE_EXPECT_TRUE(intensity && intensity->differs, "a intensidade mudou");
  AE_EXPECT_EQ(intensity->candidate, std::string("42"), "o candidato mostra o valor que entraria");
  AE_EXPECT_EQ(intensity->current, std::string("8"), "o atual continua visível para comparação");

  // Campo igual permanece na lista: o autor precisa ver o que o preset NÃO leva.
  const auto *range = findDelta(delta, "range");
  AE_EXPECT_TRUE(range && !range->differs, "o alcance é igual e aparece assim mesmo");

  const auto *kind = findDelta(delta, "kind");
  AE_EXPECT_TRUE(kind && kind->differs && kind->candidate == "Spot", "a enumeração é comparada pelo nome da opção");
}

AE_TEST(apply_component_fields_changes_only_the_chosen_addresses) {
  scene::Components components;
  auto *value = components.add(scene::Light::descriptor);
  AE_EXPECT_TRUE(value != nullptr, "luz anexada");
  auto &light = static_cast<scene::Light &>(*value);
  light.intensity = 3;
  light.range = 7;

  scene::Light preset;
  preset.intensity = 25;
  preset.range = 99;

  const scene::FieldAddress only[]{{"intensity", 0, scene::FieldKind::Number}};
  const auto outcome = scene::applyComponentFields(components, preset, only);
  AE_EXPECT_TRUE(outcome.ok(), "a aplicação seletiva foi aceita");
  AE_EXPECT_EQ(outcome.applied, 1u, "um campo entrou");
  AE_EXPECT_EQ(outcome.rejected, 0u, "nada foi recusado");

  const auto *applied = static_cast<const scene::Light *>(components.find(scene::Light::descriptor));
  AE_EXPECT_TRUE(applied->intensity == 25, "a intensidade do preset entrou");
  AE_EXPECT_TRUE(applied->range == 7, "o alcance ajustado no objeto foi preservado");
}

AE_TEST(apply_component_fields_is_a_single_transaction) {
  scene::Components components;
  auto *value = components.add(scene::Light::descriptor);
  auto &light = static_cast<scene::Light &>(*value);
  light.kind = scene::LightKind::Spot;
  light.innerAngle = 10;
  light.outerAngle = 40;

  // Um cone interno maior que o externo é recusado por `Light::valid()`. Aplicar
  // só o interno produziria exatamente esse estado — e precisa falhar INTEIRO,
  // sem deixar o valor pela metade.
  scene::Light preset;
  preset.kind = scene::LightKind::Spot;
  preset.innerAngle = 80;
  preset.outerAngle = 85;
  const scene::FieldAddress inner[]{{"inner_angle", 0, scene::FieldKind::Number}};
  const auto outcome = scene::applyComponentFields(components, preset, inner);
  AE_EXPECT_TRUE(!outcome.ok(), "a combinação inválida foi recusada");
  AE_EXPECT_EQ(outcome.applied, 0u, "nenhum campo permanece aplicado após a recusa");

  const auto *unchanged = static_cast<const scene::Light *>(components.find(scene::Light::descriptor));
  AE_EXPECT_TRUE(unchanged->innerAngle == 10, "o valor anterior continua no componente");
}

AE_TEST(apply_component_fields_rejects_unknown_address_without_touching_the_rest) {
  scene::Components components;
  components.add(scene::Light::descriptor);
  scene::Light preset;
  preset.intensity = 11;
  const scene::FieldAddress mixed[]{{"intensity", 0, scene::FieldKind::Number},
                                    {"nao_existe", 0, scene::FieldKind::Number}};
  const auto outcome = scene::applyComponentFields(components, preset, mixed);
  AE_EXPECT_TRUE(outcome.ok(), "o endereço válido entrou");
  AE_EXPECT_EQ(outcome.applied, 1u, "um campo aplicado");
  AE_EXPECT_EQ(outcome.rejected, 1u, "o endereço desconhecido é contado como recusa, não ignorado");
}

AE_TEST(mesh_renderer_declares_its_resource_bindings) {
  scene::MeshRenderer mesh;
  mesh.asset = guid(1);
  mesh.materialAsset = guid(2);
  mesh.textures[0] = guid(3);
  mesh.submeshes.push_back({});
  mesh.submeshes[0].asset = guid(4);

  const auto dependencies = scene::componentResourceDependencies(mesh);
  AE_EXPECT_EQ(dependencies.size(), 4u, "malha, material, cor base e a malha do segundo slot");
  bool sawSecondSlot = false;
  for (const auto &dependency : dependencies)
    if (dependency.address.id == "mesh" && dependency.address.slot == 1 && dependency.guid == guid(4)) sawSecondSlot = true;
  AE_EXPECT_TRUE(sawSecondSlot, "o binding endereça cada slot, não só o primeiro");

  // Herança e ausência declarada são estados distintos e não viram dependência.
  mesh.textures[1] = scene::MaterialTextureNone;
  AE_EXPECT_EQ(scene::componentResourceDependencies(mesh).size(), 5u,
               "ausência declarada continua sendo um valor endereçável do binding");
}

AE_TEST(resource_remap_moves_identities_between_projects) {
  scene::MeshRenderer mesh;
  mesh.asset = guid(1);
  mesh.materialAsset = guid(2);
  mesh.submeshes.push_back({});
  mesh.submeshes[0].asset = guid(1); // a MESMA malha em dois slots

  const scene::ResourceRemapEntry mapping[]{{guid(1), guid(100)}};
  AE_EXPECT_EQ(scene::remapComponentResources(mesh, mapping), 2u, "os dois endereços da mesma identidade são trocados");
  AE_EXPECT_TRUE(mesh.asset == guid(100) && mesh.submeshes[0].asset == guid(100), "a nova identidade entrou nos dois slots");
  AE_EXPECT_TRUE(mesh.materialAsset == guid(2), "o que não está no mapa não é tocado");
}

AE_TEST(resource_delta_distinguishes_inherit_from_declared_absence) {
  scene::MeshRenderer current, candidate;
  candidate.textures[0] = scene::MaterialTextureNone;
  const auto delta = scene::componentDelta(current, candidate);
  const auto *base = findDelta(delta, "texture.base_color");
  AE_EXPECT_TRUE(base != nullptr, "o binding de cor base entra no diff");
  AE_EXPECT_EQ(base->current, std::string("herda"), "sem valor local o binding herda");
  AE_EXPECT_EQ(base->candidate, std::string("sem recurso"), "a ausência declarada é um estado próprio");
  AE_EXPECT_TRUE(base->differs, "trocar herança por ausência é uma mudança de verdade");
}

AE_TEST(apply_component_fields_moves_a_resource_slot) {
  scene::Components components;
  auto *value = components.add(scene::MeshRenderer::descriptor);
  AE_EXPECT_TRUE(value != nullptr, "malha anexada");

  scene::MeshRenderer preset;
  preset.materialAsset = guid(77);
  preset.textures[0] = guid(78);
  const scene::FieldAddress fields[]{{"material", 0, scene::FieldKind::Resource}};
  const auto outcome = scene::applyComponentFields(components, preset, fields);
  AE_EXPECT_TRUE(outcome.ok() && outcome.applied == 1u, "o binding de material foi aplicado");

  const auto *applied = static_cast<const scene::MeshRenderer *>(components.find(scene::MeshRenderer::descriptor));
  AE_EXPECT_TRUE(applied->materialAsset == guid(77), "o material do preset entrou");
  AE_EXPECT_TRUE(!applied->textures[0].valid(), "a textura que não foi escolhida continua herdando");
}

AE_TEST(changed_fields_reproduces_a_full_apply_through_the_same_path) {
  scene::Components components;
  auto *value = components.add(scene::Light::descriptor);
  static_cast<scene::Light &>(*value).intensity = 1;

  scene::Light preset;
  preset.intensity = 30;
  preset.range = 55;
  const auto delta = scene::componentDelta(*components.find(scene::Light::descriptor), preset);
  const auto fields = scene::changedFields(delta);
  AE_EXPECT_EQ(fields.size(), 2u, "intensidade e alcance são o que difere");

  const auto outcome = scene::applyComponentFields(components, preset, fields);
  AE_EXPECT_TRUE(outcome.ok() && outcome.applied == 2u, "aplicar tudo usa o mesmo mecanismo do seletivo");
  const auto *applied = static_cast<const scene::Light *>(components.find(scene::Light::descriptor));
  AE_EXPECT_TRUE(applied->intensity == 30 && applied->range == 55, "os dois campos entraram");
}

AE_TEST(slot_properties_address_every_slot_not_only_the_first) {
  scene::MeshRenderer current, candidate;
  current.submeshes.push_back({});
  candidate.submeshes.push_back({});
  // Slot 1 com recorte e corte próprios: é o caso que antes só o dedo alcançava.
  candidate.submeshes[0].surface = {scene::MaterialAlphaMask, scene::MaterialSidesDouble, .25f};
  candidate.submeshes[0].channels.occlusionStrength = .5f;

  const auto delta = scene::componentDelta(current, candidate);
  const auto *mode = findDelta(delta, "surface.alpha_mode", 1);
  AE_EXPECT_TRUE(mode && mode->differs, "o modo de alfa do slot 1 entra no diff");
  AE_EXPECT_EQ(mode->candidate, std::string("Recorte"), "e vem pelo nome da opção");
  AE_EXPECT_TRUE(findDelta(delta, "surface.alpha_mode", 0) != nullptr, "o slot 0 também é endereçado");
  AE_EXPECT_TRUE(!findDelta(delta, "surface.alpha_mode", 0)->differs, "e não muda");

  const auto *cutoff = findDelta(delta, "surface.alpha_cutoff", 1);
  AE_EXPECT_TRUE(cutoff && cutoff->differs && cutoff->candidate == "0.25", "o corte do slot 1 também");
}

AE_TEST(applying_a_slot_property_touches_only_that_slot) {
  scene::Components components;
  auto *value = components.add(scene::MeshRenderer::descriptor);
  auto &mesh = static_cast<scene::MeshRenderer &>(*value);
  mesh.submeshes.push_back({});

  scene::MeshRenderer preset;
  preset.submeshes.push_back({});
  preset.surface.sides = scene::MaterialSidesSingle;
  preset.submeshes[0].surface.sides = scene::MaterialSidesDouble;

  const scene::FieldAddress only[]{{"surface.sides", 1, scene::FieldKind::SlotEnum}};
  const auto outcome = scene::applyComponentFields(components, preset, only);
  AE_EXPECT_TRUE(outcome.ok() && outcome.applied == 1u, "o slot 1 recebeu o valor");

  const auto *applied = static_cast<const scene::MeshRenderer *>(components.find(scene::MeshRenderer::descriptor));
  AE_EXPECT_EQ(applied->slotSurface(1).sides, scene::MaterialSidesDouble, "face dupla no slot escolhido");
  AE_EXPECT_EQ(applied->slotSurface(0).sides, scene::MaterialSidesKeep, "e o slot 0 continua herdando");
}

AE_TEST(a_slot_that_does_not_exist_is_rejected_not_created) {
  scene::Components components;
  components.add(scene::MeshRenderer::descriptor); // um slot só
  scene::MeshRenderer preset;
  preset.submeshes.push_back({});
  preset.submeshes[0].surface.sides = scene::MaterialSidesDouble;

  const scene::FieldAddress missing[]{{"surface.sides", 1, scene::FieldKind::SlotEnum}};
  const auto outcome = scene::applyComponentFields(components, preset, missing);
  AE_EXPECT_TRUE(!outcome.ok(), "criar slot é mudar a geometria, não aplicar um valor");
  AE_EXPECT_EQ(outcome.rejected, 1u, "o endereço inexistente é recusado com contagem");
  const auto *applied = static_cast<const scene::MeshRenderer *>(components.find(scene::MeshRenderer::descriptor));
  AE_EXPECT_EQ(applied->slotCount(), 1u, "e nenhum slot foi inventado");
}

AE_TEST(per_slot_material_factors_reach_the_second_slot) {
  scene::Components components;
  auto *value = components.add(scene::MeshRenderer::descriptor);
  static_cast<scene::MeshRenderer &>(*value).submeshes.push_back({});

  scene::MeshRenderer preset;
  preset.submeshes.push_back({});
  preset.submeshes[0].material.roughness = .125f;

  const scene::FieldAddress only[]{{"material.roughness", 1, scene::FieldKind::SlotNumber}};
  const auto outcome = scene::applyComponentFields(components, preset, only);
  AE_EXPECT_TRUE(outcome.ok() && outcome.applied == 1u, "a rugosidade do slot 1 foi aplicada");

  const auto *applied = static_cast<const scene::MeshRenderer *>(components.find(scene::MeshRenderer::descriptor));
  AE_EXPECT_TRUE(applied->slotMaterial(1).roughness == .125f, "o valor entrou no slot 1");
  AE_EXPECT_TRUE(applied->slotMaterial(1).enabled, "e ligou o override daquele slot");
  AE_EXPECT_TRUE(!applied->slotMaterial(0).enabled, "sem tocar no slot 0");
}

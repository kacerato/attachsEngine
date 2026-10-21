// PK-3 (Luz) e PK-8 (Mesh Renderer) da auditoria full Unity: cada propriedade
// nova tem um consumidor que este arquivo exercita — da autoria ao registro
// que o shader lê. O que não roda no host (o passe de sombra e o fragmento)
// é validado no aparelho; aqui fica o contrato que eles consomem.
#include "harness.h"
#include "editor/editor_archive.h"
#include "editor/editor_creation_catalog.h"
#include "editor/editor_document.h"
#include "renderer/punctual_lights.h"
#include "renderer/rendering_policy.h"
#include "runtime/scene_components.h"
#include "runtime/scene_lights.h"
#include "scene/component_properties.h"
#include "scene/light.h"
#include "scene/lighting_settings.h"
#include "scene/mesh_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

using namespace ae;
using namespace ae::editor;

namespace {
EditorEntityId directionalAt(EditorDocument &doc, const char *name, float intensity) {
  const auto id = doc.createEntity(doc.root(), EditorEntityKind::Folder, name);
  auto values = *doc.find(id);
  auto *light = runtime::editLight(values);
  if (!light) return 0;
  scene::applyLightDefault(*light, scene::LightKind::Directional);
  light->intensity = intensity;
  return doc.applyEntityValues(id, values) ? id : 0;
}
float creationNumber(std::span<const CreationValue> values, std::string_view property) {
  for (const auto &value : values)
    if (value.property == property) {
      if (const auto *number = std::get_if<float>(&value.value)) return *number;
      if (const auto *integer = std::get_if<u32>(&value.value)) return static_cast<float>(*integer);
    }
  return -1.0f;
}
} // namespace

AE_TEST(light_v3_scene_migrates_to_unity_light_without_visual_change) {
  // Um sol v3: cor linear (.25, .5, 1), meio-cones, sem campo de sombra próprio.
  scene::Light sun;
  std::istringstream v3("0 1 0 0 0 0 0.25 0.5 1 6500 3 10 20 35 1 0.05 0.4 0.2");
  AE_EXPECT_TRUE(sun.read(v3, 3), "Luz v3 lida");
  AE_EXPECT_TRUE(sun.kind == scene::LightKind::Directional, "direcional");
  AE_EXPECT_EQ(sun.shadowMode, 2u, "o sol abre em sombra suave: o que as cascatas da política já faziam");
  AE_EXPECT_TRUE(std::abs(scene::srgbChannelToLinear(sun.color[1]) - .5f) < 1e-5f, "cor linear preservada no shader");
  AE_EXPECT_EQ(sun.spotAngle, 70.f, "meio-cone 35 vira Spot Angle 70");
  AE_EXPECT_EQ(sun.innerSpotAngle, 40.f, "meio-cone 20 vira Inner Spot Angle 40");
  AE_EXPECT_TRUE(sun.cullingMask == scene::kAllObjectLayers && sun.renderingLayers == scene::kDefaultRenderingLayers,
                 "máscaras nascem iluminando tudo, como antes");
  AE_EXPECT_TRUE(sun.lightmapMode == 0, "Modo Tempo real");

  // A v4 faz ida e volta com tudo que é novo.
  sun.cullingMask = 0b1011u;
  sun.renderingLayers = 0b110u;
  sun.shadowMode = 1;
  sun.shadowStrength = .35f;
  std::stringstream text;
  sun.write(text);
  scene::Light again;
  AE_EXPECT_TRUE(again.read(text, scene::Light::descriptor.version), "Luz v4 lida");
  AE_EXPECT_TRUE(again.cullingMask == 0b1011u && again.renderingLayers == 0b110u && again.shadowMode == 1 &&
                 again.shadowStrength == .35f, "máscaras e sombra do sol sobrevivem ao arquivo");
  AE_EXPECT_EQ(scene::Light::descriptor.version, 4u, "componente na v4");
}

AE_TEST(light_inspector_follows_unity_light_component) {
  const auto &type = scene::Light::descriptor;
  const auto findEnum = [&](std::string_view id) -> const scene::ComponentEnum * {
    for (const auto &property : type.enums) if (property.id == id) return &property;
    return nullptr;
  };
  const auto findNumber = [&](std::string_view id) -> const scene::ComponentNumber * {
    for (const auto &property : type.numbers) if (property.id == id) return &property;
    return nullptr;
  };
  // Spot Angle e Inner Spot Angle completos, padrões da Unity.
  const auto *spot = findNumber("spot_angle");
  const auto *inner = findNumber("inner_spot_angle");
  AE_EXPECT_TRUE(spot && inner && spot->maximum == 179 && spot->minimum == 1, "Spot Angle 1..179 como na Unity");
  scene::Light fresh;
  AE_EXPECT_TRUE(fresh.spotAngle == 30 && std::abs(fresh.innerSpotAngle - 21.8f) < 1e-4f, "30° e 21,8° de padrão");
  AE_EXPECT_TRUE(findNumber("inner_angle") == nullptr && findNumber("outer_angle") == nullptr,
                 "meio-ângulos saíram do Inspector e da API");
  // Cor sRGB, como o seletor.
  AE_EXPECT_TRUE(type.triples.size() == 1 && type.triples[0].kind == scene::ComponentTripleKind::SrgbColor,
                 "a cor da Luz é sRGB");
  // Modo: gravado no arquivo, fora do Inspector até o bake existir.
  AE_EXPECT_TRUE(findEnum("mode") == nullptr && fresh.lightmapMode == 0,
                 "Modo não vira campo sem consumidor; a luz é de tempo real");
  auto components = scene::Components{};
  auto *light = static_cast<scene::Light *>(components.add(scene::Light::descriptor));
  AE_EXPECT_TRUE(light != nullptr, "luz criada");
  // Máscaras: qualquer união de bits declarados, nada além deles.
  const auto *culling = findEnum("culling_mask");
  const auto *layers = findEnum("rendering_layers");
  AE_EXPECT_TRUE(culling && culling->flags && layers && layers->flags, "Culling Mask e Rendering Layers são máscaras");
  AE_EXPECT_TRUE(scene::setComponentProperty(components, type.id, "rendering_layers", u32{0b101u}) ==
                     scene::ComponentPropertyStatus::Applied, "duas camadas de renderização");
  AE_EXPECT_TRUE(scene::setComponentProperty(components, type.id, "rendering_layers", u32{0x100u}) !=
                     scene::ComponentPropertyStatus::Applied, "bit fora das oito camadas é recusado");
  AE_EXPECT_TRUE(scene::setComponentProperty(components, type.id, "culling_mask", u32{0u}) ==
                     scene::ComponentPropertyStatus::Applied, "Nada é uma máscara válida");
  AE_EXPECT_EQ(scene::componentEnumLabel(*layers, 0b101u), std::string("Padrão, Camada 2"), "rótulo lista as camadas");
  AE_EXPECT_EQ(scene::componentEnumLabel(*layers, scene::kAllRenderingLayers), std::string("Tudo"), "todas é Tudo");
  AE_EXPECT_EQ(scene::componentEnumLabel(*culling, 0u), std::string("Nada"), "nenhuma é Nada");
}

AE_TEST(light_defaults_come_from_one_table) {
  // L-05: menu, sol com dia e noite, classe e modelo leem a mesma tabela.
  AE_EXPECT_EQ(creationNumber(recipe::directionalLight, "intensity"), scene::kDirectionalLightDefault.intensity,
               "Luz direcional usa a tabela");
  AE_EXPECT_EQ(creationNumber(recipe::directionalLight, "unit"), static_cast<float>(scene::kDirectionalLightDefault.unit),
               "unidade relativa, como a URP");
  AE_EXPECT_EQ(creationNumber(recipe::directionalLight, "shadow_mode"), 2.f, "o sol nasce com sombra suave");
  AE_EXPECT_TRUE(recipe::dayNightSun[0].values.data() == recipe::directionalLight,
                 "Sol com dia e noite é a MESMA luz direcional, mais a Hora do dia");
  AE_EXPECT_EQ(creationNumber(recipe::pointLight, "intensity"), scene::kPointLightDefault.intensity, "pontual da tabela");
  AE_EXPECT_EQ(creationNumber(recipe::spotLight, "spot_angle"), 30.f, "spot nasce com 30°");
  scene::Light fresh;
  AE_EXPECT_TRUE(fresh.kind == scene::kPointLightDefault.kind && fresh.unit == scene::kPointLightDefault.unit &&
                 fresh.intensity == scene::kPointLightDefault.intensity && fresh.range == scene::kPointLightDefault.range,
                 "o padrão da classe é a pontual da tabela");
  scene::Light directional;
  scene::applyLightDefault(directional, scene::LightKind::Directional);
  AE_EXPECT_TRUE(directional.valid() && directional.shadowMode == 2 && directional.unit == scene::LightUnit::Engine,
                 "a tabela produz uma luz válida");
}

AE_TEST(sun_source_picks_the_main_light_like_the_unity_lighting_window) {
  EditorDocument doc;
  const auto bright = directionalAt(doc, "Sol forte", 6);
  const auto moon = directionalAt(doc, "Lua", 0.2f);
  AE_EXPECT_TRUE(bright && moon, "duas direcionais");
  std::vector<renderer::SceneLight> lights;
  AE_EXPECT_TRUE(runtime::collectSceneLights(doc, lights), "coleta");
  AE_EXPECT_TRUE(renderer::selectDirectionalLight(lights)->objectId == bright, "sem Fonte do sol: a mais brilhante");

  // Fonte do sol explícita na Iluminação da cena: a lua vira o sol principal.
  const auto settingsId = doc.createEntity(doc.root(), EditorEntityKind::Folder, "Iluminação");
  auto settings = *doc.find(settingsId);
  auto *lighting = static_cast<scene::LightingSettings *>(settings.components.add(scene::LightingSettings::descriptor));
  AE_EXPECT_TRUE(lighting != nullptr, "componente criado");
  AE_EXPECT_TRUE(scene::setComponentProperty(settings.components, scene::LightingSettings::descriptor.id, "sun_source",
                                             scene::ObjectReference{moon}) == scene::ComponentPropertyStatus::Applied,
                 "referência pela API comum");
  AE_EXPECT_TRUE(doc.applyEntityValues(settingsId, settings), "configuração aplicada");
  AE_EXPECT_TRUE(runtime::collectSceneLights(doc, lights), "coleta com Fonte do sol");
  const auto *main = renderer::selectDirectionalLight(lights);
  AE_EXPECT_TRUE(main && main->objectId == moon && main->sunSource, "a Fonte do sol manda");
  // O sol que não é principal entra como luz adicional, sem sombra.
  std::array<renderer::PunctualLight, renderer::MaximumPunctualLights> slots{};
  renderer::LightBudgetReport report{};
  const float camera[3]{0, 2, 0};
  AE_EXPECT_EQ(renderer::selectPunctualLights(lights, camera, slots, report), 1u, "o sol forte vira adicional");
  AE_EXPECT_TRUE(slots[0].directional() && report.directionalAccepted == 2, "as duas direcionais iluminam");

  // Luz desligada na Fonte do sol: volta a regra automática, como no URP.
  auto off = *doc.find(moon);
  runtime::editLight(off)->enabled = false;
  AE_EXPECT_TRUE(doc.applyEntityValues(moon, off), "lua desligada");
  AE_EXPECT_TRUE(runtime::collectSceneLights(doc, lights), "coleta com fonte apagada");
  AE_EXPECT_TRUE(renderer::selectDirectionalLight(lights)->objectId == bright, "referência inválida cai na automática");

  // E o arquivo guarda a escolha.
  std::stringstream text;
  scene::LightingSettings written;written.sunSource = moon;written.write(text);
  scene::LightingSettings read;
  AE_EXPECT_TRUE(read.read(text, 1) && read.sunSource == moon, "Fonte do sol no arquivo");
}

AE_TEST(collected_light_is_linear_and_carries_its_masks_and_shadow) {
  EditorDocument doc;
  const auto id = directionalAt(doc, "Sol", 3);
  auto values = *doc.find(id);
  auto *light = runtime::editLight(values);
  light->color[0] = .5f; light->color[1] = 1; light->color[2] = 0;
  light->cullingMask = 0b11u; light->renderingLayers = 0b10u;
  light->shadowMode = 1; light->shadowStrength = .5f; light->shadowBias = .1f; light->shadowNormalBias = .8f;
  AE_EXPECT_TRUE(doc.applyEntityValues(id, values), "sol autorado");
  std::vector<renderer::SceneLight> lights;
  AE_EXPECT_TRUE(runtime::collectSceneLights(doc, lights) && lights.size() == 1, "coleta");
  AE_EXPECT_TRUE(std::abs(lights[0].color[0] - .21404f) < 1e-4f, "sRGB 0,5 chega ao shader como 0,214 linear");
  AE_EXPECT_TRUE(lights[0].filter.cullingMask == 0b11u && lights[0].filter.renderingLayers == 0b10u, "máscaras coletadas");
  AE_EXPECT_TRUE(lights[0].shadow.mode == 1 && lights[0].shadow.strength == .5f &&
                 lights[0].shadow.bias / renderer::ReferenceShadowBias == 2.f &&
                 lights[0].shadow.normalBias / renderer::ReferenceShadowNormalBias == 2.f,
                 "o desvio da Luz escala o da política: 0,1 é o dobro do padrão");
}

AE_TEST(draw_light_mask_honours_layers_and_the_per_object_limit) {
  // Doze lâmpadas iguais em volta de um objeto: oito chegam ao fragmento, as
  // mais próximas (Per Object Limit da URP).
  std::array<renderer::PunctualLight, 12> lights{};
  for (u32 i = 0; i < lights.size(); ++i) {
    lights[i].positionRange[0] = 1.0f + static_cast<float>(i);
    lights[i].positionRange[3] = 50;
    lights[i].colorIntensity[0] = lights[i].colorIntensity[1] = lights[i].colorIntensity[2] = 1;
    lights[i].directionOffset[3] = 1;
  }
  const float center[3]{0, 0, 0};
  const u32 mask = renderer::punctualLightMask(center, .5f, lights, {}, renderer::DrawLightingKey{}, 8, true);
  AE_EXPECT_EQ(static_cast<u32>(__builtin_popcount(mask)), 8u, "oito luzes por objeto");
  AE_EXPECT_EQ(mask, 0xffu, "as oito mais próximas");

  // Direcional adicional alcança qualquer objeto e entra antes das lâmpadas.
  lights[11].positionRange[3] = -1;
  const u32 withSun = renderer::punctualLightMask(center, .5f, lights, {}, renderer::DrawLightingKey{}, 8, true);
  AE_EXPECT_TRUE((withSun >> 11) & 1u, "a direcional adicional sempre entra");

  // Culling Mask: a luz 0 só ilumina a camada 2; Rendering Layers: a luz 1 só a camada de renderização 1.
  std::array<renderer::PunctualLightFilter, 12> filters{};
  filters[0].cullingMask = 1u << 2;
  filters[1].renderingLayers = 0b10u;
  renderer::DrawLightingKey key{};
  key.objectLayer = 0;
  key.renderingLayers = 0b01u;
  const u32 filtered = renderer::punctualLightMask(center, .5f, lights, filters, key, 16, true);
  AE_EXPECT_TRUE(!(filtered & 1u), "luz com Culling Mask sem a camada do objeto não ilumina");
  AE_EXPECT_TRUE(!(filtered & 2u), "luz sem camada de renderização em comum não ilumina");
  key.objectLayer = 2;
  key.renderingLayers = 0b11u;
  const u32 accepted = renderer::punctualLightMask(center, .5f, lights, filters, key, 16, true);
  AE_EXPECT_TRUE((accepted & 3u) == 3u, "com a camada certa as duas voltam");

  // Receive Shadows e sol: bits da mesma palavra que o vértice decodifica.
  const float encoded = renderer::encodeDrawLighting(accepted, false, renderer::lightFilterAccepts(filters[0], key));
  const u32 word = static_cast<u32>(encoded);
  AE_EXPECT_TRUE((word & renderer::DrawLightingNoShadows) && !(word & renderer::DrawLightingNoSun),
                 "não recebe sombra, recebe o sol");
  AE_EXPECT_TRUE(!renderer::unshadowedPointLighting(std::span<const renderer::PunctualLight>(lights.data(), 12)),
                 "direcional adicional tira o pipeline especializado de pontuais");
}

AE_TEST(light_budget_follows_the_device_profile_like_the_urp_asset) {
  renderer::ProjectRenderingSettings settings{};
  renderer::RenderingCapabilities capabilities{};
  capabilities.profile = rhi::DeviceProfile::C;
  auto policy = renderer::resolveRenderingPolicy(settings, capabilities, renderer::ThermalPressure::None);
  AE_EXPECT_EQ(policy.lights.additionalLights, 8u, "perfil C mantém o teto histórico");
  capabilities.profile = rhi::DeviceProfile::S;
  policy = renderer::resolveRenderingPolicy(settings, capabilities, renderer::ThermalPressure::None);
  AE_EXPECT_EQ(policy.lights.additionalLights, renderer::MaximumPunctualLights, "perfil S usa o bloco inteiro");
  AE_EXPECT_EQ(policy.lights.additionalLightsPerObject, renderer::MaximumLightsPerObject, "Per Object Limit 8");
}

AE_TEST(mesh_renderer_v11_owns_shadows_probes_and_layers) {
  const auto &type = scene::MeshRenderer::descriptor;
  AE_EXPECT_EQ(type.version, 11u, "Mesh Renderer v11");
  scene::MeshRenderer renderer;
  AE_EXPECT_TRUE(renderer.castShadows == 1 && renderer.receiveShadows && renderer.renderingLayerMask == 1,
                 "padrões da Unity: projeta, recebe, camada Padrão");
  renderer.castShadows = 3;
  renderer.receiveShadows = false;
  renderer.renderingLayerMask = 0b1000u;
  std::stringstream text;
  renderer.write(text);
  scene::MeshRenderer again;
  AE_EXPECT_TRUE(again.read(text, type.version), "v11 lida");
  AE_EXPECT_TRUE(again.shadowsOnly() && !again.receiveShadows && again.renderingLayerMask == 0b1000u &&
                 !again.legacyObjectShadows, "Só sombra, Receive Shadows e camadas no arquivo");

  // Arquivo anterior à v11: a sombra estava nos interruptores do objeto.
  scene::MeshRenderer fresh;
  std::stringstream current;
  fresh.write(current);
  std::string payload = current.str();
  // Os cinco números da v11 no fim ("1 1 1 1 1 ") somem: é o texto de uma v10.
  for (int field = 0; field < 5; ++field) {
    payload.pop_back();
    payload.erase(payload.find_last_of(' ') + 1);
  }
  std::istringstream old(payload);
  scene::MeshRenderer legacy;
  AE_EXPECT_TRUE(legacy.read(old, 10) && legacy.legacyObjectShadows, "v10 lida e marcada para adoção");
  runtime::SceneObject object{};
  object.castShadow = false;
  object.receiveShadow = false;
  object.components.add(scene::MeshRenderer::descriptor);
  runtime::editMeshRenderer(object)->legacyObjectShadows = legacy.legacyObjectShadows;
  runtime::adoptLegacyRendererShadows(object);
  const auto *adopted = runtime::meshRenderer(object);
  AE_EXPECT_TRUE(adopted && adopted->castShadows == 0 && !adopted->receiveShadows && !adopted->legacyObjectShadows,
                 "os interruptores antigos do objeto viram o Projetar/Receber do renderer");

  // PK-6: as sondas viraram campos com consumidor (Off/Blend e os 4 modos de
  // reflexo da Unity), com o padrão da Unity; camadas é máscara.
  const scene::ComponentEnum *lightProbes = nullptr, *reflectionProbes = nullptr, *layers = nullptr, *cast = nullptr;
  for (const auto &property : type.enums) {
    if (property.id == "light_probes") lightProbes = &property;
    if (property.id == "reflection_probes") reflectionProbes = &property;
    if (property.id == "rendering_layer_mask") layers = &property;
    if (property.id == "cast_shadows") cast = &property;
  }
  AE_EXPECT_TRUE(lightProbes && reflectionProbes && lightProbes->options.size() == 2 && reflectionProbes->options.size() == 4 &&
                 lightProbes->presentation.hasConsumer() && reflectionProbes->presentation.hasConsumer() &&
                 renderer.lightProbes == 1 && renderer.reflectionProbes == 1,
                 "sondas com consumidor e o padrão da Unity (Misturar sondas)");
  AE_EXPECT_TRUE(layers && layers->flags, "Rendering Layer Mask é máscara");
  AE_EXPECT_TRUE(cast && cast->options.size() == 4 && cast->presentation.hasConsumer(), "Cast Shadows com quatro modos");
}

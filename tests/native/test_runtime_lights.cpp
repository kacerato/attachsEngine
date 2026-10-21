#include "harness.h"
#include "editor/editor_archive.h"
#include "editor/editor_document.h"
#include "editor/editor_component_catalog.h"
#include "editor/editor_play_scene.h"
#include "renderer/punctual_lights.h"
#include "runtime/scene_components.h"
#include "runtime/scene_lights.h"
#include "scene/component_schema.h"
#include "scene/light.h"

#include <algorithm>
#include <string>
#include <sstream>
#include <vector>

using namespace ae;
using namespace ae::editor;

namespace {
EditorEntityId lightAt(EditorDocument &doc, const char *name, scene::LightKind kind,
                       float x, float y, float z, float intensity, float range) {
  const auto id = doc.createEntity(doc.root(), EditorEntityKind::Folder, name);
  auto values = *doc.find(id);
  values.transform.position[0] = x;
  values.transform.position[1] = y;
  values.transform.position[2] = z;
  auto *light = editLight(values);
  if (!light) return 0;
  light->kind = kind;
  light->unit = scene::LightUnit::Engine;
  light->intensity = intensity;
  light->range = range;
  return doc.applyEntityValues(id, values) ? id : 0;
}
} // namespace

AE_TEST(scene_light_is_a_component_with_contract_and_survives_the_archive) {
  // O componente precisa existir para as TRÊS bocas: catálogo do inspetor,
  // schema (e por ele a API em C#) e registro do arquivo. Um componente que
  // salva mas não aparece, ou que aparece mas não salva, é meio componente.
  const auto *schema = scene::findComponentSchema("astra.render.light");
  AE_EXPECT_TRUE(schema != nullptr, "Luz tem schema");
  AE_EXPECT_TRUE(findEditorComponent("astra.render.light") != nullptr, "Luz está no catálogo do inspetor");
  AE_EXPECT_TRUE(schema->propertiesInPlay == scene::PlayMutability::SafePoint,
                 "propriedades de luz podem mudar durante o Play");

  EditorDocument doc;
  const auto id = lightAt(doc, "Poste", scene::LightKind::Spot, 1, 2, 3, 12, 7);
  AE_EXPECT_TRUE(id != 0, "luz criada");
  auto values = *doc.find(id);
  auto *light = editLight(values);
  light->color[0] = .25f;
  light->color[1] = .5f;
  light->color[2] = .75f;
  light->innerSpotAngle = 10;
  light->spotAngle = 40;
  light->unit = scene::LightUnit::LuxCandela;
  light->useColorTemperature = true;
  light->colorTemperature = 3200;
  AE_EXPECT_TRUE(doc.applyEntityValues(id, values), "valores aceitos");

  const auto text = serializeEditorDocument(doc, 7);
  EditorDocument restored;
  AE_EXPECT_TRUE(deserializeEditorDocument(text, 7, restored), "documento relido");
  const auto *back = lightComponent(*restored.find(id));
  AE_EXPECT_TRUE(back != nullptr, "luz preservada no arquivo");
  AE_EXPECT_TRUE(back->kind == scene::LightKind::Spot, "modalidade preservada");
  AE_EXPECT_EQ(back->intensity, 12.f, "intensidade preservada");
  AE_EXPECT_EQ(back->range, 7.f, "alcance preservado");
  AE_EXPECT_EQ(back->color[2], .75f, "cor preservada");
  AE_EXPECT_EQ(back->spotAngle, 40.f, "abertura do spot preservada");
  AE_EXPECT_TRUE(back->unit == scene::LightUnit::LuxCandela, "unidade física preservada");
  AE_EXPECT_TRUE(back->useColorTemperature, "uso de temperatura preservado");
  AE_EXPECT_EQ(back->colorTemperature, 3200.f, "temperatura preservada");

  // Cone interno maior que o externo inverteria a janela e desenharia a borda
  // no lugar errado: recusa na autoria, não no shader.
  auto invalid = *doc.find(id);
  editLight(invalid)->innerSpotAngle = 60;
  AE_EXPECT_TRUE(!doc.applyEntityValues(id, invalid), "cone interno maior que o externo é recusado");
}

AE_TEST(light_v1_migrates_without_changing_legacy_brightness) {
  scene::Light light;
  std::istringstream old("2 1 0.25 0.5 0.75 12 7 10 40");
  AE_EXPECT_TRUE(light.read(old, 1), "payload v1 aceito");
  AE_EXPECT_TRUE(light.kind == scene::LightKind::Spot, "modalidade antiga");
  AE_EXPECT_TRUE(light.unit == scene::LightUnit::Engine, "v1 mantém escala interna");
  AE_EXPECT_TRUE(!light.useColorTemperature, "v1 não ganha filtro novo");
  AE_EXPECT_EQ(scene::lightIntensityForShader(light.kind, light.unit, light.intensity,
                                               light.innerHalfAngle(), light.outerHalfAngle()),
               12.f, "brilho antigo chega igual ao shader");
  // v1 guardava meio-ângulos e cor linear; a v4 guarda a abertura total e a
  // cor do seletor, sem mudar o que o shader recebe.
  AE_EXPECT_EQ(light.spotAngle, 80.f, "meio-cone de 40° vira Spot Angle de 80°");
  AE_EXPECT_EQ(light.innerSpotAngle, 20.f, "meio-cone interno de 10° vira 20°");
  AE_EXPECT_TRUE(std::abs(scene::srgbChannelToLinear(light.color[0]) - .25f) < 1e-5f, "cor linear volta igual ao shader");
  AE_EXPECT_TRUE(light.color[0] > .5f, "e é mostrada em sRGB, como o seletor da Unity");
}

AE_TEST(physical_light_units_and_temperature_feed_the_runtime_consumer) {
  constexpr float pi = 3.14159265358979323846f;
  const float candela = scene::lightIntensityForShader(scene::LightKind::Point,
      scene::LightUnit::LuxCandela, 683.f, 0, 45);
  AE_EXPECT_TRUE(std::abs(candela - 1.f) < 1e-5f, "683 cd viram uma unidade radiométrica");
  const float lumen = scene::lightIntensityForShader(scene::LightKind::Point,
      scene::LightUnit::LuxLumen, 683.f * 4.f * pi, 0, 45);
  AE_EXPECT_TRUE(std::abs(lumen - 1.f) < 1e-5f, "fluxo pontual usa quatro pi esterradianos");

  float neutral[3]{}, warm[3]{}, cold[3]{};
  scene::lightTemperatureColor(6500, neutral);
  scene::lightTemperatureColor(2700, warm);
  scene::lightTemperatureColor(12000, cold);
  for (float channel : neutral)
    AE_EXPECT_TRUE(std::abs(channel - 1.f) < 1e-4f, "D65 é filtro neutro");
  AE_EXPECT_TRUE(warm[0] > warm[2], "2700 K favorece vermelho");
  AE_EXPECT_TRUE(cold[2] > cold[0], "12000 K favorece azul");

  EditorDocument doc;
  const auto id = lightAt(doc, "Luz física", scene::LightKind::Point, 0, 2, 0, 683, 10);
  auto values = *doc.find(id);
  auto *light = editLight(values);
  light->unit = scene::LightUnit::LuxCandela;
  light->useColorTemperature = true;
  light->colorTemperature = 2700;
  AE_EXPECT_TRUE(doc.applyEntityValues(id, values), "luz física autorada");
  std::vector<renderer::SceneLight> lights;
  AE_EXPECT_TRUE(runtime::collectSceneLights(doc, lights), "consumidor coleta");
  AE_EXPECT_EQ(lights.size(), 1u, "uma luz física");
  AE_EXPECT_TRUE(std::abs(lights[0].intensity - 1.f) < 1e-5f, "candela convertida no consumidor");
  AE_EXPECT_TRUE(lights[0].color[0] > lights[0].color[2], "temperatura multiplica a cor consumida");
}

AE_TEST(scene_lights_are_collected_with_world_pose_and_hierarchy_rules) {
  EditorDocument doc;
  const auto parent = doc.createEntity(doc.root(), EditorEntityKind::Folder, "Suporte");
  auto parentValues = *doc.find(parent);
  parentValues.transform.position[1] = 4;
  AE_EXPECT_TRUE(doc.applyEntityValues(parent, parentValues), "pai posicionado");
  const auto child = doc.createEntity(parent, EditorEntityKind::Folder, "Lâmpada");
  auto childValues = *doc.find(child);
  childValues.transform.position[0] = 2;
  auto *light = editLight(childValues);
  light->kind = scene::LightKind::Point;
  light->intensity = 5;
  AE_EXPECT_TRUE(doc.applyEntityValues(child, childValues), "luz filha criada");

  std::vector<renderer::SceneLight> lights;
  AE_EXPECT_TRUE(runtime::collectSceneLights(doc, lights), "coleta");
  AE_EXPECT_EQ(lights.size(), 1u, "uma luz acesa");
  // A pose vem da hierarquia, como qualquer componente: pai em Y=4 mais filho
  // em X=2 dá mundo (2,4,0).
  AE_EXPECT_EQ(lights[0].position[0], 2.f, "X de mundo");
  AE_EXPECT_EQ(lights[0].position[1], 4.f, "Y de mundo");

  // Desativar o ANCESTRAL apaga a luz, pela mesma regra que apaga a malha.
  parentValues = *doc.find(parent);
  parentValues.active = false;
  AE_EXPECT_TRUE(doc.applyEntityValues(parent, parentValues), "pai desativado");
  AE_EXPECT_TRUE(runtime::collectSceneLights(doc, lights), "coleta com pai desativado");
  AE_EXPECT_EQ(lights.size(), 0u, "ancestral desativado apaga a luz");

  parentValues.active = true;
  AE_EXPECT_TRUE(doc.applyEntityValues(parent, parentValues), "pai reativado");
  childValues = *doc.find(child);
  editLight(childValues)->enabled = false;
  AE_EXPECT_TRUE(doc.applyEntityValues(child, childValues), "luz apagada pelo interruptor");
  AE_EXPECT_TRUE(runtime::collectSceneLights(doc, lights), "coleta com luz apagada");
  AE_EXPECT_EQ(lights.size(), 0u, "interruptor apaga a luz");
}

AE_TEST(punctual_light_budget_is_deterministic_and_reports_what_did_not_fit) {
  std::vector<renderer::SceneLight> lights;
  // Nove luzes idênticas, distâncias crescentes: só oito cabem.
  for (u32 i = 0; i < 9; ++i) {
    renderer::SceneLight light{};
    light.objectId = i + 1;
    light.modality = renderer::LightModality::Point;
    light.position[0] = static_cast<float>(i);
    light.intensity = 4;
    light.range = 20;
    lights.push_back(light);
  }
  const float camera[3]{0, 0, 0};
  // Orçamento do perfil C (Additional Lights = 8): o span é o teto do quadro.
  std::array<renderer::PunctualLight, 8> selected{};
  renderer::LightBudgetReport report{};
  const auto accepted = renderer::selectPunctualLights(lights, camera, selected, report);
  AE_EXPECT_EQ(accepted, 8u, "o orçamento enche");
  AE_EXPECT_EQ(report.punctualDropped, 1u, "a que não coube é contada, não sumida");
  // Mais perto da câmera vem primeiro: é o critério, e ele é estável.
  AE_EXPECT_EQ(selected[0].positionRange[0], 0.f, "a mais próxima primeiro");
  AE_EXPECT_EQ(selected[7].positionRange[0], 7.f, "a oitava é a nona mais distante");

  // Duas direcionais: a mais forte vira o sol (luz principal, com cascatas) e a
  // outra entra como luz adicional, sem sombra — a lua ou o preenchimento.
  std::vector<renderer::SceneLight> suns;
  renderer::SceneLight sun{};
  sun.objectId = 10;
  sun.modality = renderer::LightModality::Directional;
  sun.intensity = 2;
  suns.push_back(sun);
  sun.objectId = 11;
  sun.intensity = 9;
  suns.push_back(sun);
  renderer::LightBudgetReport sunReport{};
  AE_EXPECT_EQ(renderer::selectPunctualLights(suns, camera, selected, sunReport), 1u,
               "a segunda direcional ocupa uma vaga adicional");
  AE_EXPECT_EQ(sunReport.directionalAccepted, 2u, "as duas direcionais acendem");
  AE_EXPECT_EQ(sunReport.directionalDropped, 0u, "nenhuma direcional é descartada");
  AE_EXPECT_TRUE(selected[0].directional() && selected[0].colorIntensity[0] == 2.f,
                 "a vaga é a direcional fraca, marcada pelo alcance negativo");
  AE_EXPECT_TRUE(selected[0].shadow[0] < 0, "direcional adicional não tem sombra");
  const auto *chosen = renderer::selectDirectionalLight(suns);
  AE_EXPECT_TRUE(chosen && chosen->objectId == 11, "a direcional mais forte é a escolhida");

  // An enabled authored sun at zero is an explicit black interior, not permission
  // to restore the default daylight source. It still contributes no light slots.
  suns.resize(1);
  suns[0].intensity = 0;
  AE_EXPECT_TRUE(renderer::selectDirectionalLight(suns) == &suns[0],
                 "direcional autoral zero suprime o sol padrão");
  AE_EXPECT_TRUE(!renderer::lightContributes(suns[0]), "zero não passa a contribuir energia");

  // Intensidade zero é luz apagada pelo valor, não candidata: não pode roubar
  // vaga de uma que ilumina.
  std::vector<renderer::SceneLight> mixed;
  renderer::SceneLight dark{};
  dark.objectId = 1;
  dark.modality = renderer::LightModality::Point;
  dark.intensity = 0;
  dark.range = 5;
  mixed.push_back(dark);
  renderer::SceneLight bright = dark;
  bright.objectId = 2;
  bright.intensity = 3;
  mixed.push_back(bright);
  renderer::LightBudgetReport mixedReport{};
  AE_EXPECT_EQ(renderer::selectPunctualLights(mixed, camera, selected, mixedReport), 1u,
               "só a luz com energia ocupa vaga");
  AE_EXPECT_TRUE(mixedReport.complete(), "nada foi descartado por orçamento");
}

AE_TEST(spot_cone_window_is_precomputed_and_point_lights_stay_omnidirectional) {
  std::vector<renderer::SceneLight> lights;
  renderer::SceneLight spot{};
  spot.objectId = 1;
  spot.modality = renderer::LightModality::Spot;
  spot.intensity = 1;
  spot.range = 10;
  spot.innerAngle = 0;
  spot.outerAngle = 60;
  spot.direction[0] = 0;
  spot.direction[1] = 0;
  spot.direction[2] = 2; // não normalizada de propósito: escala do objeto
  lights.push_back(spot);
  const float camera[3]{0, 0, 0};
  std::array<renderer::PunctualLight, renderer::MaximumPunctualLights> selected{};
  renderer::LightBudgetReport report{};
  AE_EXPECT_EQ(renderer::selectPunctualLights(lights, camera, selected, report), 1u, "spot aceito");
  AE_EXPECT_EQ(selected[0].directionOffset[2], 1.f, "direção normalizada apesar da escala");

  // O shader faz clamp(cos*scale+offset,0,1): no eixo vale 1, no ângulo externo
  // vale 0. Verificar aqui é verificar a conta que o pixel faz.
  const float scale = selected[0].colorIntensity[3];
  const float offset = selected[0].directionOffset[3];
  const auto window = [&](float cosine) { return std::clamp(cosine * scale + offset, 0.f, 1.f); };
  AE_EXPECT_EQ(window(1.f), 1.f, "no eixo o spot ilumina inteiro");
  AE_EXPECT_TRUE(window(std::cos(60.f * 0.0174532925199433f)) < 1e-3f, "no cone externo zera");
  AE_EXPECT_TRUE(window(std::cos(30.f * 0.0174532925199433f)) > 0.f, "dentro do cone ilumina");

  std::vector<renderer::SceneLight> point;
  renderer::SceneLight omni = spot;
  omni.modality = renderer::LightModality::Point;
  point.push_back(omni);
  AE_EXPECT_EQ(renderer::selectPunctualLights(point, camera, selected, report), 1u, "pontual aceita");
  const float pointScale = selected[0].colorIntensity[3];
  const float pointOffset = selected[0].directionOffset[3];
  // A mesma conta do spot precisa dar 1 em QUALQUER direção para a pontual,
  // senão o fragmento precisaria de um desvio por tipo de luz.
  for (float cosine : {-1.f, 0.f, .5f, 1.f})
    AE_EXPECT_EQ(std::clamp(cosine * pointScale + pointOffset, 0.f, 1.f), 1.f,
                 "pontual ilumina em todas as direções com a mesma conta");
}

AE_TEST(light_shadow_capability_matrix_is_published_and_honest) {
  // A matriz é publicada porque a interface precisa dela: oferecer sombra numa
  // modalidade sem passe seria um botão sem shader atrás.
  AE_EXPECT_TRUE(renderer::lightCastsShadow(renderer::LightModality::Directional),
                 "direcional tem passe de sombra: as cascatas do sol");
  AE_EXPECT_TRUE(renderer::lightCastsShadow(renderer::LightModality::Point) &&
                 renderer::lightCastsShadow(renderer::LightModality::Spot),
                 "luz local tem passe de sombra: o atlas em quadtree");

  // E a autoria acompanha: desde a Luz v4 o tipo de sombra existe em todas as
  // modalidades (a do sol também é da Luz, como na Unity); resolução e plano
  // próximo são de mapa local e somem na direcional; os ajustes finos só
  // aparecem quando a sombra está ligada.
  const auto mode = std::find_if(scene::Light::descriptor.enums.begin(), scene::Light::descriptor.enums.end(),
                                  [](const auto &property) { return property.id == std::string_view("shadow_mode"); });
  AE_EXPECT_TRUE(mode != scene::Light::descriptor.enums.end(), "a Luz expõe o tipo de sombra");
  scene::Light directional;
  directional.kind = scene::LightKind::Directional;
  scene::Light lamp;
  lamp.kind = scene::LightKind::Point;
  AE_EXPECT_TRUE(mode->presentation.isVisible(directional), "o sol escolhe a própria sombra");
  AE_EXPECT_TRUE(mode->presentation.isVisible(lamp), "pontual escolhe a própria sombra");
  const auto resolution = std::find_if(scene::Light::descriptor.enums.begin(), scene::Light::descriptor.enums.end(),
                                        [](const auto &property) { return property.id == std::string_view("shadow_resolution"); });
  directional.shadowMode = 2;
  AE_EXPECT_TRUE(resolution != scene::Light::descriptor.enums.end() && !resolution->presentation.isVisible(directional),
                 "a resolução do sol é a das cascatas da política");
  const auto strength = std::find_if(scene::Light::descriptor.numbers.begin(), scene::Light::descriptor.numbers.end(),
                                      [](const auto &property) { return property.id == std::string_view("shadow_strength"); });
  AE_EXPECT_TRUE(strength != scene::Light::descriptor.numbers.end(), "força da sombra é propriedade");
  AE_EXPECT_TRUE(!strength->presentation.isVisible(lamp), "sem sombra ligada, o ajuste fino fica escondido");
  lamp.shadowMode = 1;
  AE_EXPECT_TRUE(strength->presentation.isVisible(lamp), "com sombra dura, o ajuste aparece");
}

AE_TEST(light_changed_during_play_reaches_the_collected_frame) {
  // O caminho inteiro: componente no documento, Play abre o mundo, a API comum
  // muda a propriedade e a coleta do quadro já vê o valor novo. É o que um
  // script faz.
  EditorDocument doc;
  const auto id = lightAt(doc, "Lâmpada", scene::LightKind::Point, 0, 3, 0, 6, 12);
  AE_EXPECT_TRUE(id != 0, "luz criada");

  EditorMapScene resources;
  EditorPlayScene play;
  AE_EXPECT_TRUE(play.start(doc, resources), "Play inicia");
  auto &world = play.world();
  const auto handle = world.handle(id);
  const auto light = world.findComponent(handle, "astra.render.light");
  AE_EXPECT_TRUE(light.valid(), "componente de luz endereçável pela API comum");
  AE_EXPECT_EQ(static_cast<u32>(world.setProperty(light, "intensity", 40.f)), 0u, "intensidade escrita");
  AE_EXPECT_EQ(static_cast<u32>(world.setProperty(light, "color.r", .2f)), 0u, "cor escrita");

  std::vector<renderer::SceneLight> lights;
  AE_EXPECT_TRUE(runtime::collectSceneLights(play.document(), lights), "coleta do mundo em execução");
  AE_EXPECT_EQ(lights.size(), 1u, "uma luz");
  AE_EXPECT_EQ(lights[0].intensity, 40.f, "o valor escrito por código chega ao quadro");
  AE_EXPECT_TRUE(std::abs(lights[0].color[0] - scene::srgbChannelToLinear(.2f)) < 1e-6f,
                 "a cor sRGB escrita por código chega ao quadro linearizada");

  // E o documento autoral continua com o valor autorado.
  play.stop();
  AE_EXPECT_EQ(lightComponent(*doc.find(id))->intensity, 6.f, "autoria preservada depois do Stop");
}

AE_TEST(local_shadow_authoring_reaches_the_frame_slot_and_old_scenes_open_without_it) {
  EditorDocument doc;
  const auto lamp = lightAt(doc, "Lâmpada", scene::LightKind::Point, 0, 2.4f, 0, 800, 8);
  const auto sun = lightAt(doc, "Sol", scene::LightKind::Directional, 0, 10, 0, 5, 1);
  AE_EXPECT_TRUE(lamp && sun, "luzes criadas");
  // Sombra suave, resolução Alta, força 0,8 — o que o autor escolhe no Inspector.
  auto values = *doc.find(lamp);
  auto *light = editLight(values);
  light->shadowMode = 2;
  light->shadowResolution = 3;
  light->shadowStrength = .8f;
  light->shadowNearPlane = .3f;
  AE_EXPECT_TRUE(doc.applyEntityValues(lamp, values), "sombra autorada");
  // Sombra dura no sol: chega à coleta para as cascatas; o atlas local só
  // recebe pontuais e spots (o renderer descarta direcionais ali).
  values = *doc.find(sun);
  editLight(values)->shadowMode = 1;
  editLight(values)->shadowStrength = .6f;
  AE_EXPECT_TRUE(doc.applyEntityValues(sun, values), "direcional aceita o valor");

  std::vector<renderer::SceneLight> lights;
  AE_EXPECT_TRUE(runtime::collectSceneLights(doc, lights), "coleta do quadro");
  const renderer::SceneLight *pointLight = nullptr, *directional = nullptr;
  for (const auto &entry : lights) {
    if (entry.modality == renderer::LightModality::Point) pointLight = &entry;
    if (entry.modality == renderer::LightModality::Directional) directional = &entry;
  }
  AE_EXPECT_TRUE(pointLight && pointLight->shadow.casts() && pointLight->shadow.mode == 2,
                 "a sombra suave chega à coleta");
  AE_EXPECT_TRUE(pointLight->shadow.resolution == 3 && pointLight->shadow.strength == .8f &&
                 pointLight->shadow.nearPlane == .3f, "resolução, força e plano próximo chegam intactos");
  AE_EXPECT_TRUE(directional && directional->shadow.mode == 1 && directional->shadow.strength == .6f,
                 "a sombra do sol é da Luz: tipo e força chegam às cascatas");

  // A vaga que o shader lê sabe de qual luz veio e começa sem tile: é o
  // renderer, depois de montar o atlas, que aponta o tile.
  const float camera[3]{0, 1.6f, -4};
  renderer::PunctualLight slots[renderer::MaximumPunctualLights];
  slots[0].shadow[0] = 5; // lixo de um quadro anterior
  u32 sources[renderer::MaximumPunctualLights]{};
  renderer::LightBudgetReport report;
  const u32 accepted = renderer::selectPunctualLights(lights, camera, slots, report, sources);
  AE_EXPECT_EQ(accepted, 1u, "uma luz local aceita");
  AE_EXPECT_TRUE(&lights[sources[0]] == pointLight, "a vaga reata a luz de origem");
  AE_EXPECT_TRUE(slots[0].shadow[0] < 0, "vaga reaproveitada não herda tile de outra luz");

  // Cena salva antes da sombra local (Luz v2) abre sem sombra.
  std::stringstream old;
  old << "1 1 2 0 1 1 1 6500 800 8 20 35";
  scene::Light restored;
  AE_EXPECT_TRUE(restored.read(old, 2), "Luz v2 lida");
  AE_EXPECT_TRUE(restored.shadowMode == 0 && restored.shadowStrength == 1, "v2 abre sem sombra, como foi salva");
  // E a versão atual faz ida e volta com a sombra.
  scene::Light authored;
  authored.shadowMode = 2;
  authored.shadowResolution = 3;
  authored.shadowStrength = .8f;
  std::stringstream current;
  authored.write(current);
  scene::Light again;
  AE_EXPECT_TRUE(again.read(current, scene::Light::descriptor.version), "Luz atual lida");
  AE_EXPECT_TRUE(again.shadowMode == 2 && again.shadowResolution == 3 && again.shadowStrength == .8f,
                 "a sombra sobrevive ao arquivo");
}

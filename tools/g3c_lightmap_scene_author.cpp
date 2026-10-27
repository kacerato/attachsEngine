// G3c: variante de validação do Sponza para o lightmap assado.
//
// Lê a cena importada do projeto Sponza (uma cópia puxada do aparelho, nunca o
// original) e escreve outra pasta com:
//  - a arquitetura marcada Static, com Contribuir para iluminação global e
//    Receber = Lightmaps (o padrão da Unity para um objeto Static);
//  - o sol com Hora do dia (e a Lua), que continua em tempo real com sombra;
//  - três luzes coloridas (vermelha, verde, azul) em modo Assado na arcada
//    norte, cada uma diante de um estandarte da mesma cor, e uma lanterna
//    quente em modo Misto na arcada sul: o rebote colorido debaixo do teto;
//  - as Configurações de iluminação com valores de cena média para o aparelho
//    (8 texels/m, 1024 px, 2 rebotes, 128/128 amostras);
//  - um Grupo de sondas de luz de 50 pontos (o do PK-6) e uma esfera fosca
//    dinâmica na arcada, que recebe a luz assada pelas sondas;
//  - os perfis de importação das fontes com Gerar UV de lightmap ligado.
// Uso: g3c_lightmap_scene_author <projeto-copiado> <saida> [hora] [minutos-por-dia]
//      (minutos-por-dia > 0 faz o Play andar as 24 h: o passeio da validação)
#include "editor/editor_archive.h"
#include "resources/asset_registry.h"
#include "resources/import_profile.h"
#include "scene/camera.h"
#include "scene/environment.h"
#include "scene/light.h"
#include "scene/light_probe_group.h"
#include "scene/lighting_settings.h"
#include "scene/mesh_renderer.h"
#include "scene/time_of_day.h"
#include "runtime/scene_views.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

using namespace ae;
using namespace ae::editor;

namespace {
[[noreturn]] void fail(const std::string &message) {
  std::cerr << message << '\n';
  std::exit(20);
}
std::string readText(const std::filesystem::path &p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), {}};
}
// "luzes=tempo-real": as lanternas saem em Tempo real. Com Iluminação global
// assada ligada o bake ainda gera o lightmap, só com o céu e a visibilidade
// dele -- o par "tempo real" da Unity (luz Realtime + ambiente do lightmap) em
// vez de "tempo real" = cena nunca assada, que vê o céu sem oclusão.
bool realtimeLanterns = false;
// "sombras=0": lanternas sem sombra dos dois lados do par. O atlas de sombra
// local tem 16 tiles e uma pontual usa 6: em tempo real só 2 das 8 lanternas
// fazem sombra, e o assado faz das 8 -- a luz das outras 6 atravessa paredes
// só no tempo real. Sem sombra nos dois, o par compara só a energia.
bool shadowlessLanterns = false;
EditorEntityId lantern(EditorDocument &doc, EditorEntityId parent, const char *name, float x, float y, float z, u32 mode,
                       const float *color = nullptr) {
  const auto id = doc.createEntity(parent, EditorEntityKind::Folder, name);
  auto values = *doc.find(id);
  values.transform.position[0] = x; values.transform.position[1] = y; values.transform.position[2] = z;
  values.isStatic = true;
  auto *light = static_cast<scene::Light *>(values.components.add(scene::Light::descriptor));
  scene::applyLightDefault(*light, scene::LightKind::Point);
  light->useColorTemperature = color == nullptr;
  light->colorTemperature = 2700;
  if (color) for (u32 k = 0; k < 3; ++k) light->color[k] = color[k];
  // 1200 lm, o Point Light do vídeo da Unity (U1). A 8000 lm as superfícies
  // a 1-2 m da luz estouravam e o par ficava duro (validação de 24/10).
  light->intensity = 1200.0f;
  light->range = 8.0f;
  light->shadowMode = shadowlessLanterns ? 0 : 2;
  light->lightmapMode = realtimeLanterns ? 0 : mode;
  if (!light->valid() || !doc.applyEntityValues(id, values)) fail(std::string("lanterna recusada: ") + name);
  return id;
}
// Estandarte de pano colorido (esfera achatada, fosca): a luz assada ao lado
// bate nele e o rebote tinge colunas e chão, como as cortinas do Sponza.
void banner(EditorDocument &doc, EditorEntityId parent, const char *name, float x, float y, float z, const float color[3]) {
  const auto id = doc.createEntity(parent, EditorEntityKind::Mesh, name);
  auto values = *doc.find(id);
  values.transform.position[0] = x; values.transform.position[1] = y; values.transform.position[2] = z;
  values.transform.scale[0] = 1.3f; values.transform.scale[1] = 2.4f; values.transform.scale[2] = .08f;
  values.isStatic = true;
  auto *mesh = static_cast<scene::MeshRenderer *>(values.components.edit(scene::MeshRenderer::descriptor));
  mesh->asset = resources::assetGuidFromSeed("astra:builtin:sphere:v1");
  mesh->mesh = 1;
  mesh->material.enabled = true;
  for (u32 k = 0; k < 3; ++k) mesh->material.baseColor[k] = color[k];
  mesh->material.roughness = 1;
  mesh->contributeGlobalIllumination = true;
  if (!doc.applyEntityValues(id, values)) fail(std::string("estandarte recusado: ") + name);
}
} // namespace

int main(int argc, char **argv) {
  // "ev=<EV100>" em qualquer posição: Ambiente na raiz com exposição Fixa, para
  // o par tempo real x assado ser julgado na mesma exposição (a automática
  // estica o contraste quando a arcada escurece).
  float fixedEv = 0, ambient = 1;
  bool fixedExposure = false, ambientSet = false;
  std::vector<char *> args;
  for (int i = 0; i < argc; ++i) {
    const std::string a = argv[i];
    if (a.rfind("ev=", 0) == 0) { fixedEv = std::stof(a.substr(3)); fixedExposure = true; }
    // "ambiente=0": tira o céu dos dois lados do par (Indirect Diffuse e
    // Specular do Ambiente) -- sobra só a energia das lanternas.
    else if (a.rfind("ambiente=", 0) == 0) { ambient = std::stof(a.substr(9)); ambientSet = true; }
    else if (a == "luzes=tempo-real") realtimeLanterns = true;
    else if (a == "sombras=0") shadowlessLanterns = true;
    else args.push_back(argv[i]);
  }
  argc = static_cast<int>(args.size());
  argv = args.data();
  if (argc < 3 || argc > 5) { std::cerr << "uso: g3c_lightmap_scene_author <projeto-copiado> <saida> [hora] [minutos-por-dia] [ev=EV100] [ambiente=K] [luzes=tempo-real] [sombras=0]\n"; return 1; }
  const std::filesystem::path root = std::filesystem::absolute(argv[1]), output = std::filesystem::absolute(argv[2]);
  // "generico": qualquer projeto (a Vila) -- só Static + GI, as Configurações
  // e a UV2 no perfil; as luzes locais existentes passam a Misto (rebote assado).
  const bool generic = argc >= 4 && std::string(argv[3]) == "generico";
  const float hour = argc >= 4 && !generic ? std::stof(argv[3]) : 10.0f;
  const float dayMinutes = argc == 5 ? std::stof(argv[4]) : 0.0f;
  EditorDocument doc;
  if (!loadEditorDocument((root / "scenes/editor.aescene").string().c_str(), 0, doc)) fail("cena recusada");
  std::vector<EditorEntityId> ids;
  doc.collectSubtree(doc.root(), ids);
  u32 statics = 0;
  for (auto id : ids) {
    const auto *object = doc.find(id);
    if (!object || id == doc.root()) continue;
    if (!object->components.find(scene::MeshRenderer::descriptor)) continue;
    auto values = *object;
    values.isStatic = true;
    auto *render = static_cast<scene::MeshRenderer *>(values.components.edit(scene::MeshRenderer::descriptor));
    render->contributeGlobalIllumination = true;
    render->receiveGlobalIllumination = 0;
    if (!doc.applyEntityValues(id, values)) fail("Static recusado");
    ++statics;
  }
  if (generic) {
    for (auto id : ids) {
      const auto *object = doc.find(id);
      const auto *light = object ? static_cast<const scene::Light *>(object->components.find(scene::Light::descriptor)) : nullptr;
      if (!light || light->kind == scene::LightKind::Directional) continue;
      auto values = *object;
      static_cast<scene::Light *>(values.components.edit(scene::Light::descriptor))->lightmapMode = 1;
      if (!doc.applyEntityValues(id, values)) fail("luz recusada");
    }
  } else {
  // Sol e Lua da Hora do dia, em tempo real (o lightmap nunca guarda o sol).
  EditorEntityId sunId = 0;
  for (auto id : ids) {
    const auto *object = doc.find(id);
    const auto *light = object ? static_cast<const scene::Light *>(object->components.find(scene::Light::descriptor)) : nullptr;
    if (light && light->kind == scene::LightKind::Directional) { sunId = id; break; }
  }
  if (!sunId) {
    sunId = doc.createEntity(doc.root(), EditorEntityKind::Folder, "Sol");
    auto sun = *doc.find(sunId);
    auto *light = static_cast<scene::Light *>(sun.components.add(scene::Light::descriptor));
    scene::applyLightDefault(*light, scene::LightKind::Directional);
    light->shadowMode = 2;
    if (!doc.applyEntityValues(sunId, sun)) fail("sol recusado");
  }
  const auto moonId = doc.createEntity(doc.root(), EditorEntityKind::Folder, "Lua");
  {
    auto moon = *doc.find(moonId);
    auto *moonLight = static_cast<scene::Light *>(moon.components.add(scene::Light::descriptor));
    scene::applyLightDefault(*moonLight, scene::LightKind::Directional);
    moonLight->unit = scene::kMoonLightDefault.unit;
    moonLight->intensity = scene::kMoonLightDefault.intensity;
    moonLight->useColorTemperature = true;
    moonLight->colorTemperature = scene::kMoonColorTemperature;
    moonLight->shadowMode = scene::kMoonLightDefault.shadowMode;
    if (!moonLight->valid() || !doc.applyEntityValues(moonId, moon)) fail("Lua recusada");
  }
  {
    auto sun = *doc.find(sunId);
    auto *time = static_cast<scene::TimeOfDay *>(sun.components.find(scene::TimeOfDay::descriptor) ?
        sun.components.edit(scene::TimeOfDay::descriptor) : sun.components.add(scene::TimeOfDay::descriptor));
    if (!time) fail("Hora do dia recusada");
    time->hour = hour; time->dayLengthMinutes = dayMinutes; time->dayOfYear = 172.0f; time->latitude = -15.0f; time->moonLight = moonId;
    if (!time->valid() || !doc.applyEntityValues(sunId, sun)) fail("Hora do dia recusada");
  }
  // Arcada norte: três luzes Assadas coloridas, cada uma diante de um
  // estandarte da mesma cor (vermelho, verde, azul), como as cortinas e as
  // luzes coloridas do vídeo da Unity; a arcada sul fica com uma Mista quente.
  const auto lights = doc.createEntity(doc.root(), EditorEntityKind::Folder, "Lanternas (G3c)");
  const float red[3]{1.0f, .22f, .16f}, green[3]{.25f, 1.0f, .3f}, blue[3]{.25f, .4f, 1.0f};
  const float redCloth[3]{.75f, .05f, .04f}, greenCloth[3]{.06f, .6f, .1f}, blueCloth[3]{.05f, .12f, .7f};
  lantern(doc, lights, "Luz assada vermelha", -6.5f, 2.3f, 4.9f, 2, red);
  lantern(doc, lights, "Luz assada verde", 0.5f, 2.3f, 4.9f, 2, green);
  lantern(doc, lights, "Luz assada azul", 7.5f, 2.3f, 4.9f, 2, blue);
  // Cenário de custo do plano: sete luzes pontuais com sombra em modo Assado e
  // uma Mista. Em tempo real (Limpar) são oito mapas de sombra por quadro.
  lantern(doc, lights, "Lanterna assada norte 2", -3.0f, 2.3f, 4.9f, 2);
  lantern(doc, lights, "Lanterna assada norte 4", 4.0f, 2.3f, 4.9f, 2);
  lantern(doc, lights, "Lanterna assada sul 1", -6.5f, 2.3f, -4.9f, 2);
  lantern(doc, lights, "Lanterna assada sul 3", 7.5f, 2.3f, -4.9f, 2);
  lantern(doc, lights, "Lanterna mista sul", 0.5f, 2.3f, -4.9f, 1);
  const auto cloths = doc.createEntity(doc.root(), EditorEntityKind::Folder, "Estandartes (G3c)");
  banner(doc, cloths, "Estandarte vermelho", -6.5f, 2.0f, 5.5f, redCloth);
  banner(doc, cloths, "Estandarte verde", 0.5f, 2.0f, 5.5f, greenCloth);
  banner(doc, cloths, "Estandarte azul", 7.5f, 2.0f, 5.5f, blueCloth);
  }
  // Configurações de iluminação na raiz (o Lighting Settings da cena).
  {
    auto rootValues = *doc.find(doc.root());
    auto *settings = static_cast<scene::LightingSettings *>(rootValues.components.find(scene::LightingSettings::descriptor) ?
        rootValues.components.edit(scene::LightingSettings::descriptor) : rootValues.components.add(scene::LightingSettings::descriptor));
    if (!settings) fail("Configurações recusadas");
    settings->lightmapResolution = 8;
    settings->lightmapPadding = 2;
    settings->maxLightmapSize = 2;
    settings->indirectSamples = 128;
    settings->environmentSamples = 128;
    settings->directSamples = 16;
    settings->bounces = 2;
    if (!settings->valid() || !doc.applyEntityValues(doc.root(), rootValues)) fail("Configurações recusadas");
  }
  if (fixedExposure || ambientSet) {
    auto rootValues = *doc.find(doc.root());
    auto *environment = static_cast<scene::Environment *>(rootValues.components.find(scene::Environment::descriptor) ?
        rootValues.components.edit(scene::Environment::descriptor) : rootValues.components.add(scene::Environment::descriptor));
    if (!environment) fail("Ambiente recusado");
    environment->values.active = true;
    if (fixedExposure) {
      environment->values.exposureMode = renderer::ExposureMode::Fixed;
      environment->values.fixedExposure = fixedEv;
    }
    if (ambientSet) environment->values.indirectDiffuse = environment->values.indirectSpecular = ambient;
    if (!environment->values.valid() || !doc.applyEntityValues(doc.root(), rootValues)) fail("exposição fixa recusada");
  }
  if (!generic) {
  // Sondas de luz do PK-6 e uma esfera móvel na arcada.
  const auto groupId = doc.createEntity(doc.root(), EditorEntityKind::Folder, "Sondas de luz do pátio");
  {
    auto groupValues = *doc.find(groupId);
    auto *group = static_cast<scene::LightProbeGroup *>(groupValues.components.add(scene::LightProbeGroup::descriptor));
    group->points.clear();
    group->nextPointId = 1;
    for (float x : {-8.f, -4.f, 0.f, 4.f, 8.f})
      for (float y : {.4f, 2.4f})
        for (float z : {-2.5f, 0.f, 2.5f, 4.3f, 5.6f}) {
          const float p[3]{x, y, z};
          if (!group->addPoint(p)) fail("sonda de luz recusada");
        }
    if (!doc.applyEntityValues(groupId, groupValues)) fail("grupo recusado");
  }
  {
    const auto id = doc.createEntity(doc.root(), EditorEntityKind::Mesh, "Esfera móvel na arcada");
    auto values = *doc.find(id);
    values.transform.position[0] = 1.5f; values.transform.position[1] = .5f; values.transform.position[2] = 4.9f;
    for (u32 k = 0; k < 3; ++k) values.transform.scale[k] = .9f;
    auto *mesh = static_cast<scene::MeshRenderer *>(values.components.edit(scene::MeshRenderer::descriptor));
    mesh->asset = resources::assetGuidFromSeed("astra:builtin:sphere:v1");
    mesh->mesh = 1;
    mesh->material.enabled = true;
    mesh->material.baseColor[0] = mesh->material.baseColor[1] = mesh->material.baseColor[2] = .9f;
    mesh->material.roughness = .9f;
    mesh->lightProbes = 1;
    if (!doc.applyEntityValues(id, values)) fail("esfera recusada");
  }
  // Vista salva para o roteiro do aparelho: de dentro da arcada norte, olhando
  // ao longo dela (+X), com as três luzes coloridas e os estandartes à frente.
  // O mesmo enquadramento em todas as capturas, sem gesto de câmera.
  {
    runtime::SceneViews views = doc.views();
    runtime::SceneView view;
    view.name = "G3c arcada norte";
    view.target[0] = 1.0f; view.target[1] = 1.1f; view.target[2] = 4.4f;
    view.distance = 10.0f;
    view.yaw = 1.5708f;
    view.pitch = .08f;
    if (!views.add(view)) fail("vista recusada");
    doc.setViews(views);
  }
  // Câmera de validação: da arcada norte olhando o pátio e o beiral.
  {
    const auto cameraId = doc.createEntity(doc.root(), EditorEntityKind::Camera, "Câmera de validação G3c");
    auto camera = *doc.find(cameraId);
    camera.transform.position[0] = -7.5f; camera.transform.position[1] = 1.7f; camera.transform.position[2] = 4.6f;
    camera.transform.rotationDegrees[0] = 4; camera.transform.rotationDegrees[1] = 95;
    if (!doc.applyEntityValues(cameraId, camera)) fail("câmera recusada");
  }
  }
  std::filesystem::create_directories(output / "scenes");
  std::filesystem::create_directories(output / ".astra/imports");
  const auto authored = serializeEditorDocument(doc, 0);
  if (!saveEditorDocument((output / "scenes/editor.aescene").string().c_str(), doc, 0)) fail("gravação recusada");
  EditorDocument restored;
  if (!loadEditorDocument((output / "scenes/editor.aescene").string().c_str(), 0, restored) ||
      serializeEditorDocument(restored, 0) != authored) fail("ida e volta da cena divergiu");
  // Perfis das fontes: Gerar UV de lightmap ligado (Hard Angle 88°, margem 4 px).
  u32 profiles = 0;
  std::error_code e;
  for (const auto &entry : std::filesystem::directory_iterator(root / ".astra/imports", e)) {
    if (entry.path().extension() != ".profile") continue;
    resources::ImportProfile profile;
    if (!resources::parseImportProfile(readText(entry.path()), profile)) { std::cerr << "perfil ilegível: " << entry.path() << '\n'; continue; }
    profile.generateLightmapUvs = true;
    std::ofstream(output / ".astra/imports" / entry.path().filename(), std::ios::binary) << resources::serializeImportProfile(profile);
    ++profiles;
  }
  // Modo genérico: fonte sem perfil próprio usa o embutido; ganha um com UV2.
  if (generic)
    for (const auto &entry : std::filesystem::directory_iterator(root / ".astra/imports", e)) {
      if (entry.path().extension() != ".nodes") continue;
      auto profilePath = entry.path();
      profilePath.replace_extension(".profile");
      if (std::filesystem::exists(profilePath)) continue;
      resources::ImportProfile profile;
      profile.generateLightmapUvs = true;
      std::ofstream(output / ".astra/imports" / profilePath.filename(), std::ios::binary) << resources::serializeImportProfile(profile);
      ++profiles;
    }
  std::cout << "Static: " << statics << (generic ? " · luzes locais em Misto" : (realtimeLanterns ? " · luzes: 8 em tempo real com sombra (3 coloridas) · estandartes: 3 · sondas: 50"
                                                : " · luzes: 7 assadas com sombra (3 coloridas) + 1 mista · estandartes: 3 · sondas: 50"))
            << " · perfis com UV de lightmap: " << profiles
            << " · hora " << hour << '\n';
  return 0;
}

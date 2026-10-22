// V-2: variante noturna da Vila para a prancha Unity × POCO.
//
// Lê a cena de uma CÓPIA do projeto Vila Astra (nunca o original) e grava outra
// pasta em que o sol da cena ("Sol de fim de tarde") ganha a Hora do dia na hora
// pedida, com a Lua do "Sol com dia e noite" (0,5 lux, 8500 K, sombra suave)
// ligada em Hora do dia › Lua -- o mesmo par que Criar › Luzes › Sol com dia e
// noite monta. O dia 168 cai perto da lua cheia, como nos vídeos de referência.
// Uso: v2_night_scene_author <projeto-copiado> <saida> <hora>
#include "editor/editor_archive.h"
#include "scene/light.h"
#include "scene/environment.h"
#include "scene/light_defaults.h"
#include "scene/time_of_day.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

using namespace ae;
using namespace ae::editor;

namespace {
[[noreturn]] void fail(const std::string &message) {
  std::cerr << message << '\n';
  std::exit(20);
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 4) { std::cerr << "uso: v2_night_scene_author <projeto-copiado> <saida> <hora>\n"; return 1; }
  const std::filesystem::path root = std::filesystem::absolute(argv[1]), output = std::filesystem::absolute(argv[2]);
  const float hour = std::stof(argv[3]);
  EditorDocument doc;
  if (!loadEditorDocument((root / "scenes/editor.aescene").string().c_str(), 0, doc)) fail("cena recusada");
  std::vector<EditorEntityId> ids;
  doc.collectSubtree(doc.root(), ids);
  EditorEntityId sunId = 0;
  for (auto id : ids) {
    const auto *object = doc.find(id);
    const auto *light = object ? static_cast<const scene::Light *>(object->components.find(scene::Light::descriptor)) : nullptr;
    if (light && light->kind == scene::LightKind::Directional && std::string_view(object->name) == "Sol de fim de tarde") sunId = id;
  }
  if (!sunId) fail("sol da Vila não encontrado");
  // A Vila veio das cenas antigas com a Exposição Fixa (-0,41 EV), calibrada
  // para o dia. Um ciclo de dia e noite na Unity usa a Exposure Automatic do
  // Volume com limites; a variante usa os padrões da Astra (Automática, Peso no
  // centro, -8/10), como o autor faria no Inspector.
  u32 automatic = 0;
  for (auto id : ids) {
    const auto *object = doc.find(id);
    if (!object || !object->components.find(scene::Environment::descriptor)) continue;
    auto values = *object;
    auto *environment = static_cast<scene::Environment *>(values.components.edit(scene::Environment::descriptor));
    const renderer::SceneEnvironment defaults{};
    environment->values.exposureMode = defaults.exposureMode;
    environment->values.exposureMetering = defaults.exposureMetering;
    environment->values.exposureLimitMin = defaults.exposureLimitMin;
    environment->values.exposureLimitMax = defaults.exposureLimitMax;
    environment->values.exposureCompensation = 0.0f;
    if (!doc.applyEntityValues(id, values)) fail("Exposição automática recusada");
    ++automatic;
  }
  std::cout << "Exposição automática em " << automatic << " Ambiente(s)\n";
  const auto moonId = doc.createEntity(doc.find(sunId)->parent, EditorEntityKind::Folder, "Lua");
  auto moon = *doc.find(moonId);
  moon.transform = doc.find(sunId)->transform;
  auto *moonLight = static_cast<scene::Light *>(moon.components.add(scene::Light::descriptor));
  scene::applyLightDefault(*moonLight, scene::LightKind::Directional);
  moonLight->unit = scene::kMoonLightDefault.unit;
  moonLight->intensity = scene::kMoonLightDefault.intensity;
  moonLight->useColorTemperature = true;
  moonLight->colorTemperature = scene::kMoonColorTemperature;
  moonLight->shadowMode = scene::kMoonLightDefault.shadowMode;
  if (!moonLight->valid() || !doc.applyEntityValues(moonId, moon)) fail("Lua recusada");
  auto sun = *doc.find(sunId);
  auto *time = static_cast<scene::TimeOfDay *>(sun.components.add(scene::TimeOfDay::descriptor));
  if (!time) fail("Hora do dia recusada");
  time->hour = hour;time->dayOfYear = 168.0f;time->latitude = -15.0f;time->moonLight = moonId;
  if (!time->valid() || !doc.applyEntityValues(sunId, sun)) fail("Hora do dia recusada");
  std::filesystem::create_directories(output / "scenes");
  const auto authored = serializeEditorDocument(doc, 0);
  if (!saveEditorDocument((output / "scenes/editor.aescene").string().c_str(), doc, 0)) fail("gravação recusada");
  EditorDocument restored;
  if (!loadEditorDocument((output / "scenes/editor.aescene").string().c_str(), 0, restored) ||
      serializeEditorDocument(restored, 0) != authored) fail("ida e volta da cena divergiu");
  std::cout << "Vila às " << hour << " h com Lua ligada (objeto " << moonId << ")\n";
  return 0;
}

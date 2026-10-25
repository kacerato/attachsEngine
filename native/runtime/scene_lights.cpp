#include "runtime/scene_lights.h"
#include "runtime/scene_components.h"
#include "runtime/transform_math.h"
#include "scene/light.h"
#include "scene/lighting_settings.h"
#include "scene/time_of_day.h"
#include "renderer/time_of_day.h"

#include <algorithm>
#include <utility>

namespace ae::runtime {
void sceneLightValues(const scene::Light &light, renderer::SceneLight &entry) {
  entry.modality = static_cast<renderer::LightModality>(light.kind);
  float temperature[3]{1, 1, 1};
  if (light.useColorTemperature) scene::lightTemperatureColor(light.colorTemperature, temperature);
  // A cor autoral é sRGB, como o seletor; a iluminação é linear.
  for (u32 axis = 0; axis < 3; ++axis) entry.color[axis] = scene::srgbChannelToLinear(light.color[axis]) * temperature[axis];
  entry.intensity = scene::lightIntensityForShader(light.kind, light.unit, light.intensity,
                                                   light.innerHalfAngle(), light.outerHalfAngle());
  entry.range = light.range;
  entry.photometric = light.unit != scene::LightUnit::Engine;
  entry.innerAngle = light.innerHalfAngle();
  entry.outerAngle = light.outerHalfAngle();
  entry.filter.cullingMask = light.cullingMask;
  entry.filter.renderingLayers = light.renderingLayers;
}

bool collectSceneLights(const SceneGraph &graph, std::vector<renderer::SceneLight> &out, double sceneSeconds) {
  std::vector<ObjectId> ids;
  graph.collectSubtree(graph.root(), ids);
  std::vector<renderer::SceneLight> collected;
  // Fonte do sol: as Configurações de iluminação da cena (a primeira na ordem
  // da hierarquia, como a cena ativa da Unity tem uma só janela Lighting).
  ObjectId sunSource = 0;
  // V-2: a Lua de cada Hora do dia, como o par Sun/Moon do ciclo do HDRP.
  std::vector<std::pair<ObjectId, const scene::TimeOfDay *>> moons;
  bool settingsFound = false;
  for (auto id : ids) {
    const auto *object = graph.find(id);
    if (!object) return false;
    if (!settingsFound)
      if (const auto *settings = static_cast<const scene::LightingSettings *>(
              object->components.find(scene::LightingSettings::descriptor))) {
        sunSource = settings->sunSource;
        settingsFound = true;
      }
    if (const auto *time = static_cast<const scene::TimeOfDay *>(object->components.find(scene::TimeOfDay::descriptor));
        time && time->enabled && time->valid() && time->moonLight != 0 && time->moonLight != id &&
        object->components.find(scene::Light::descriptor))
      moons.emplace_back(time->moonLight, time);
  }
  const auto timeInput = [&](const scene::TimeOfDay &time) {
    renderer::TimeOfDayInput input;
    input.hour = renderer::advanceTimeOfDayHour(time.hour, time.dayLengthMinutes, sceneSeconds);
    input.dayOfYear = time.dayOfYear;
    input.latitude = time.latitude;
    input.northDegrees = time.northDegrees;
    return input;
  };
  for (auto id : ids) {
    const auto *object = graph.find(id);
    if (!object) return false;
    const auto *light = static_cast<const scene::Light *>(object->components.find(scene::Light::descriptor));
    if (!light || !light->enabled || !light->valid()) continue;
    bool active = true;
    for (const auto *ancestor = object; ancestor; ancestor = graph.find(ancestor->parent))
      if (!ancestor->active) { active = false; break; }
    if (!active) continue;
    float world[16];
    if (!worldMatrix(graph, id, world)) return false;
    renderer::SceneLight entry{};
    entry.objectId = id;
    sceneLightValues(*light, entry);
    for (u32 axis = 0; axis < 3; ++axis) {
      entry.position[axis] = world[12 + axis];
      // Coluna 2 é o +Z de mundo do objeto: a frente, pela convenção da Câmera.
      entry.direction[axis] = world[8 + axis];
    }
    entry.sunSource = sunSource != 0 && id == sunSource;
    // G4: a rotação já veio do transform, girado por runtime/scene_time_of_day
    // (como um script de ciclo na Unity). Aqui só a curva de cor e intensidade
    // pela elevação e a fase da lua que o céu desenha.
    if (light->kind == scene::LightKind::Directional) {
      if (const auto *time = static_cast<const scene::TimeOfDay *>(object->components.find(scene::TimeOfDay::descriptor));
          time && time->enabled && time->valid()) {
        const auto input = timeInput(*time);
        const auto sun = renderer::evaluateTimeOfDay(input);
        for (u32 axis = 0; axis < 3; ++axis) entry.color[axis] *= sun.sunColor[axis];
        entry.intensity *= sun.sunIntensity;
        entry.moonPhase = sun.moonPhase;
        entry.celestialBody = renderer::CelestialLightBody::Sun;
        std::copy_n(sun.toSun, 3, entry.celestialToSun);
        renderer::celestialSphereRows(input, entry.celestialSphere);
      }
      // V-2: a Lua. A cor é a autoral (fria, como a Moon das cenas da Unity);
      // a hora dita só a fase, a altura e o desligamento durante o dia.
      for (const auto &[moonId, time] : moons) {
        if (moonId != id) continue;
        const auto input = timeInput(*time);
        const auto sun = renderer::evaluateTimeOfDay(input);
        entry.intensity *= renderer::evaluateMoon(sun).intensity;
        entry.moonPhase = sun.moonPhase;
        entry.celestialBody = renderer::CelestialLightBody::Moon;
        std::copy_n(sun.toSun, 3, entry.celestialToSun);
        renderer::celestialSphereRows(input, entry.celestialSphere);
        break;
      }
    }
    // Sombra autoral da luz (v4): pontual e spot pedem tiles no atlas local; a
    // direcional que for o sol principal usa estes valores nas cascatas, e uma
    // direcional adicional não projeta (só a luz principal tem cascatas).
    entry.shadow.mode = static_cast<u8>(light->shadowMode);
    entry.shadow.resolution = static_cast<u8>(light->shadowResolution);
    entry.shadow.strength = light->shadowStrength;
    entry.shadow.bias = light->shadowBias;
    entry.shadow.normalBias = light->shadowNormalBias;
    entry.shadow.nearPlane = light->shadowNearPlane;
    // G3c: Modo da luz. O Sol e a Lua da Hora do dia nunca são assados: o
    // lightmap guarda só luz local e céu, e o astro segue direto e com sombra.
    entry.lightmapMode = entry.celestialBody == renderer::CelestialLightBody::None ? static_cast<u8>(light->lightmapMode) : u8{0};
    entry.filter.baked = entry.lightmapMode == 2;
    collected.push_back(entry);
  }
  out = std::move(collected);
  return true;
}
} // namespace ae::runtime

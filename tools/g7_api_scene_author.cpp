// G7: os 6 exemplos da API gráfica universal, em C# e em Flow (Conexão de
// evento), como 12 projetos pequenos e independentes para o Play do POCO.
//
// Cada projeto tem a mesma vila de blocos (chão, casas, Sol com Hora do dia e
// Lua, Volume global com o perfil "Exterior", câmera) e só o que o exemplo
// pede. A variante C# leva o script de examples/g7-api/Scripts; a variante Flow
// faz o mesmo sem código, só com Conexões de evento e os componentes da cena.
// Nenhum projeto do usuário é lido ou alterado.
//
// Uso: g7_api_scene_author <pasta-dos-scripts> <saida>
// Exemplos: 1 dia acelerado, 2 perfil por área, 3 desfoque ao mirar,
// 4 postes ao anoitecer, 5 sonda por script, 6 troca de material.
#include "core/sha256.h"
#include "editor/editor_archive.h"
#include "editor/editor_physics_body.h"
#include "renderer/volume_parameters.h"
#include "resources/asset_registry.h"
#include "resources/environment_profile.h"
#include "resources/material_asset.h"
#include "scene/camera.h"
#include "scene/component_properties.h"
#include "scene/environment.h"
#include "scene/event_connection.h"
#include "scene/light.h"
#include "scene/light_defaults.h"
#include "scene/lighting_settings.h"
#include "scene/mesh_renderer.h"
#include "scene/player_input.h"
#include "scene/reflection_probe.h"
#include "scene/script_behavior.h"
#include "scene/time_of_day.h"
#include "scene/timer.h"
#include "scene/transform_tween.h"
#include "scene/virtual_camera.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>

using namespace ae;
using namespace ae::editor;
namespace fs = std::filesystem;

namespace {
[[noreturn]] void fail(const std::string &message) {
  std::cerr << message << '\n';
  std::exit(20);
}
void write(const fs::path &path, const std::string &text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out << text;
  if (!out) fail("gravação recusada: " + path.string());
}
std::string read(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream text;text << in.rdbuf();
  if (!in && !in.eof()) fail("leitura recusada: " + path.string());
  return text.str();
}

using Value = scene::ComponentPropertyValue;
void set(runtime::SceneObject &object, const scene::ComponentType &type, std::string_view id, Value value) {
  if (scene::setComponentProperty(object.components, type.id, id, value) != scene::ComponentPropertyStatus::Applied)
    fail(std::string("propriedade recusada: ") + std::string(type.id) + "." + std::string(id));
}
void triple(runtime::SceneObject &object, const scene::ComponentType &type, std::string_view id, float x, float y, float z) {
  const float values[3]{x, y, z};
  if (scene::setComponentTriple(object.components, type.id, id, values) != scene::ComponentPropertyStatus::Applied)
    fail(std::string("trio recusado: ") + std::string(type.id) + "." + std::string(id));
}
void resource(scene::ComponentValue &component, std::string_view id, resources::AssetGuid guid, u32 slot = 0) {
  for (const auto &binding : component.type().resourceBindings)
    if (binding.id == id && binding.write && binding.write(component, slot, guid)) return;
  fail(std::string("recurso recusado: ") + std::string(id));
}

struct Assets {
  resources::AssetRegistry registry;
  std::vector<std::pair<std::string, std::string>> files; // caminho, conteúdo
  resources::AssetGuid exterior, interior, gold, neon;
};

class Builder {
public:
  EditorDocument doc;
  EditorEntityId sun = 0, moon = 0, volume = 0, camera = 0, statue = 0;

  void place(EditorEntityId id, float x, float y, float z, float sx = 1, float sy = 1, float sz = 1, float rx = 0, float ry = 0) {
    auto values = *doc.find(id);
    values.transform.position[0] = x;values.transform.position[1] = y;values.transform.position[2] = z;
    values.transform.scale[0] = sx;values.transform.scale[1] = sy;values.transform.scale[2] = sz;
    values.transform.rotationDegrees[0] = rx;values.transform.rotationDegrees[1] = ry;
    if (!doc.applyEntityValues(id, values)) fail("pose recusada");
  }
  EditorEntityId folder(EditorEntityId parent, const char *name) {
    return doc.createEntity(parent ? parent : doc.root(), EditorEntityKind::Folder, name);
  }
  // Primitiva embutida com cor própria (override do slot 0).
  EditorEntityId primitive(EditorEntityId parent, const char *name, const char *shape, const float color[3],
                           float metallic = 0, float roughness = .8f, const float *emission = nullptr, float strength = 1) {
    const auto id = doc.createEntity(parent ? parent : doc.root(), EditorEntityKind::Mesh, name);
    auto values = *doc.find(id);
    auto *mesh = static_cast<scene::MeshRenderer *>(values.components.edit(scene::MeshRenderer::descriptor));
    if (!mesh) fail("sem Mesh Renderer");
    mesh->asset = resources::assetGuidFromSeed(std::string("astra:builtin:") + shape + ":v1");
    mesh->mesh = 1;
    mesh->material.enabled = true;
    for (u32 c = 0; c < 3; ++c) mesh->material.baseColor[c] = color[c];
    mesh->material.metallic = metallic;mesh->material.roughness = roughness;
    if (emission) { for (u32 c = 0; c < 3; ++c) mesh->material.emission[c] = emission[c];mesh->material.emissionStrength = strength; }
    if (!doc.applyEntityValues(id, values)) fail(std::string("primitiva recusada: ") + name);
    return id;
  }
  void physics(EditorEntityId id, scene::BodyMotion motion, bool sensor, float hx, float hy, float hz) {
    auto values = *doc.find(id);
    auto *body = editPhysicsBody(values);body->motion = motion;body->sensor = sensor;
    auto *collider = editCollider(values);collider->halfX = hx;collider->halfY = hy;collider->halfZ = hz;
    if (!doc.applyEntityValues(id, values)) fail("física recusada");
  }
  void connection(EditorEntityId on, u32 event, u32 action, u32 method, u64 receiver, float argument = 0,
                  resources::AssetGuid asset = {}) {
    auto values = *doc.find(on);
    auto *c = static_cast<scene::EventConnection *>(values.components.add(scene::EventConnection::descriptor));
    c->event = event;c->action = action;c->method = method;c->receiver = receiver;c->argument = argument;c->asset = asset;
    if (!c->valid() || !doc.applyEntityValues(on, values)) fail("conexão recusada");
  }
  EditorEntityId timer(const char *name, float seconds, bool repeat) {
    const auto id = folder(0, name);
    auto values = *doc.find(id);
    values.components.add(scene::Timer::descriptor);
    set(values, scene::Timer::descriptor, "interval_seconds", Value{seconds});
    set(values, scene::Timer::descriptor, "repeat", Value{repeat});
    set(values, scene::Timer::descriptor, "auto_start", Value{true});
    if (!doc.applyEntityValues(id, values)) fail("timer recusado");
    return id;
  }
  void script(EditorEntityId id, std::string_view type, std::string_view file,
              std::initializer_list<std::tuple<const char *, const char *, std::string>> fields) {
    auto values = *doc.find(id);
    auto *behavior = static_cast<scene::ScriptBehavior *>(values.components.add(scene::ScriptBehavior::descriptor));
    behavior->scriptType = std::string(type);behavior->source = "Scripts/" + std::string(file);
    for (const auto &[field, kind, value] : fields)
      if (!behavior->setProperty(field, kind, value)) fail(std::string("campo de script recusado: ") + field);
    if (!doc.applyEntityValues(id, values)) fail("script recusado");
  }

  // A vila comum: chão com colisão, três casas, Sol com Hora do dia e Lua,
  // Volume global com o perfil Exterior e a câmera.
  void base(const Assets &assets, float hour, float dayLength) {
    const float grass[3]{.33f, .42f, .27f}, wall[3]{.82f, .74f, .62f}, roof[3]{.55f, .22f, .16f}, stone[3]{.62f, .63f, .66f};
    // O cubo embutido tem identidade do pacote interno (editor_map_scene.cpp);
    // plano, cilindro e esfera têm identidade fixa e servem a uma cena gerada.
    const auto ground = primitive(0, "Chão", "plane", grass, 0, .95f);
    place(ground, 0, 0, 6, 6, 1, 6);
    physics(ground, scene::BodyMotion::Static, false, 5, .05f, 5);
    const auto houses = folder(0, "Casas");
    const float spots[][3]{{-7, 1.5f, 9}, {7, 1.5f, 11}, {0, 2, 17}, {-12, 1.5f, 16}, {12, 1.5f, 3}};
    for (u32 i = 0; i < std::size(spots); ++i) {
      // Torre (cilindro de 2 m na escala 1) com cúpula de telhado.
      const auto house = primitive(houses, ("Casa " + std::to_string(i + 1)).c_str(), "cylinder", i % 2 ? stone : wall);
      place(house, spots[i][0], spots[i][1], spots[i][2], 3.5f, spots[i][1], 3.5f);
      const auto top = primitive(houses, ("Telhado " + std::to_string(i + 1)).c_str(), "sphere", roof);
      place(top, spots[i][0], spots[i][1] * 2, spots[i][2], 3.9f, 2.0f, 3.9f);
    }
    const float chrome[3]{.95f, .95f, .95f};
    statue = primitive(0, "Estátua cromada", "sphere", chrome, 1, .05f);
    place(statue, 0, 1.2f, 6, 2.4f, 2.4f, 2.4f);

    moon = folder(0, "Lua");
    {
      auto values = *doc.find(moon);
      // A Lua de Criar › Luzes › Sol com dia e noite (V-2): 0,5 lux, 12 000 K.
      auto *light = static_cast<scene::Light *>(values.components.add(scene::Light::descriptor));
      scene::applyLightDefault(*light, scene::LightKind::Directional);
      light->unit = scene::kMoonLightDefault.unit;
      light->intensity = scene::kMoonLightDefault.intensity;
      light->useColorTemperature = true;
      light->colorTemperature = scene::kMoonColorTemperature;
      light->shadowMode = scene::kMoonLightDefault.shadowMode;
      if (!light->valid() || !doc.applyEntityValues(moon, values)) fail("lua recusada");
    }
    sun = folder(0, "Sol");
    {
      auto values = *doc.find(sun);
      auto *light = static_cast<scene::Light *>(values.components.add(scene::Light::descriptor));
      scene::applyLightDefault(*light, scene::LightKind::Directional);
      light->shadowMode = 2;
      auto *time = static_cast<scene::TimeOfDay *>(values.components.add(scene::TimeOfDay::descriptor));
      time->hour = hour;time->dayLengthMinutes = dayLength;time->latitude = -15;time->dayOfYear = 172;
      time->moonLight = moon;
      if (!doc.applyEntityValues(sun, values)) fail("sol recusado");
    }
    const auto settings = folder(0, "Configurações de iluminação");
    {
      auto values = *doc.find(settings);
      auto *lighting = static_cast<scene::LightingSettings *>(values.components.add(scene::LightingSettings::descriptor));
      lighting->sunSource = sun;
      if (!doc.applyEntityValues(settings, values)) fail("configurações recusadas");
    }
    volume = folder(0, "Volume global");
    {
      auto values = *doc.find(volume);
      auto *environment = static_cast<scene::Environment *>(values.components.add(scene::Environment::descriptor));
      resources::EnvironmentProfile profile;
      if (!resources::EnvironmentProfile::deserialize(assets.files[0].second, profile)) fail("perfil Exterior");
      environment->values = resources::applyEnvironmentProfile(environment->values, profile);
      environment->setVolumeOverrides(profile.overrides);
      environment->profile = assets.exterior;
      if (!doc.applyEntityValues(volume, values)) fail("volume recusado");
    }
    camera = doc.createEntity(doc.root(), EditorEntityKind::Camera, "Câmera");
    place(camera, 0, 3.2f, -9, 1, 1, 1, 9, 0);
  }
  // Modo de atualização: 0 ao acordar, 1 todo quadro, 2 via script.
  void chromeProbe(EditorEntityId &probe, u32 refresh) {
    probe = folder(0, "Sonda de reflexo");
    place(probe, 0, 1.6f, 6);
    auto values = *doc.find(probe);
    auto *component = static_cast<scene::ReflectionProbe *>(values.components.add(scene::ReflectionProbe::descriptor));
    component->resolution = 128;
    for (u32 k = 0; k < 3; ++k) component->boxSize[k] = 30;
    if (!doc.applyEntityValues(probe, values)) fail("sonda recusada");
    set(values, scene::ReflectionProbe::descriptor, "type", Value{1u});
    set(values, scene::ReflectionProbe::descriptor, "refresh_mode", Value{refresh});
    if (!doc.applyEntityValues(probe, values)) fail("sonda recusada");
  }
};

Assets makeAssets() {
  Assets out;
  scene::Environment defaults;
  const auto profile = [&](const char *name, const char *path, bool inside) {
    resources::EnvironmentProfile p;
    p.name = name;p.values = defaults.values;p.values.active = true;p.values.priority = 0;p.values.post = true;
    p.values.bloomIntensity = .35f;
    if (inside) {
      // Área coberta: tom quente, mais contraste, menos cor e vinheta forte.
      p.values.colorFilter[0] = 1;p.values.colorFilter[1] = .72f;p.values.colorFilter[2] = .5f;
      p.values.postExposure = -.35f;p.values.contrast = 1.18f;p.values.saturation = .85f;
      p.values.vignetteIntensity = .5f;
    }
    p.values.syncEffectSwitches();
    p.overrides = renderer::VolumeOverrides::everything();
    p.guid = resources::assetGuidFromSeed(std::string("astra:g7:") + path);
    if (!p.valid()) fail(std::string("perfil inválido: ") + name);
    resources::AssetRecord record;record.guid = p.guid;record.type = resources::AssetType::EnvironmentProfile;record.path = path;
    const auto text = p.serialize();
    record.contentHash = Sha256::hex(std::span<const u8>(reinterpret_cast<const u8 *>(text.data()), text.size()));
    if (!out.registry.add(record)) fail("registro do perfil");
    out.files.emplace_back(path, text);
    return p.guid;
  };
  out.exterior = profile("Exterior", "Ambientes/Exterior.environment", false);
  out.interior = profile("Área coberta", "Ambientes/Area coberta.environment", true);
  const auto material = [&](const char *name, const char *path, const float color[3], float metallic, float roughness, u8 shader) {
    resources::MaterialAsset m;
    m.name = name;m.values.enabled = true;
    for (u32 c = 0; c < 3; ++c) m.values.baseColor[c] = color[c];
    m.values.metallic = metallic;m.values.roughness = roughness;m.shading.shader = shader;
    m.guid = resources::assetGuidFromSeed(std::string("astra:g7:") + path);
    if (!m.valid()) fail(std::string("material inválido: ") + name);
    resources::AssetRecord record;record.guid = m.guid;record.type = resources::AssetType::Material;record.path = path;
    const auto text = m.serialize();
    record.contentHash = Sha256::hex(std::span<const u8>(reinterpret_cast<const u8 *>(text.data()), text.size()));
    if (!out.registry.add(record)) fail("registro do material");
    out.files.emplace_back(path, text);
    return m.guid;
  };
  const float gold[3]{1, .77f, .34f}, neon[3]{.05f, .95f, 1};
  out.gold = material("Ouro (Lit)", "Materiais/Ouro (Lit).material", gold, 1, .25f, scene::MaterialShaderLit);
  out.neon = material("Neon (Unlit)", "Materiais/Neon (Unlit).material", neon, 0, .5f, scene::MaterialShaderUnlit);
  return out;
}

std::string guidText(resources::AssetGuid guid) { return guid.text(); }

void publish(const fs::path &output, const std::string &name, Builder &scene, const Assets &assets,
             const fs::path &scripts, const char *scriptFile) {
  const auto root = output / name;
  fs::remove_all(root);
  fs::create_directories(root / "scenes");
  if (!saveEditorDocument((root / "scenes/editor.aescene").string().c_str(), scene.doc, 0)) fail("cena recusada: " + name);
  EditorDocument check;
  if (!loadEditorDocument((root / "scenes/editor.aescene").string().c_str(), 0, check) ||
      serializeEditorDocument(check, 0) != serializeEditorDocument(scene.doc, 0)) fail("ida e volta divergiu: " + name);
  auto registry = assets.registry;
  for (const auto &[path, text] : assets.files) write(root / path, text);
  if (scriptFile) {
    const auto source = read(scripts / scriptFile);
    const std::string path = std::string("Scripts/") + scriptFile;
    write(root / path, source);
    resources::AssetRecord record;record.guid = resources::assetGuidFromSeed("astra:g7:" + name + ":" + path);
    record.type = resources::AssetType::Script;record.path = path;
    if (!registry.add(record)) fail("registro do script");
  }
  write(root / ".astra/assets.astra", registry.serialize());
  std::ostringstream project;
  project << "{\"format\": \"ASTRA-PROJECT-1\", \"resourceSource\": \"independent\", \"mainScene\": \"scenes/editor.aescene\", "
             "\"editorScene\": \"scenes/editor.aescene\", \"project\": {\"name\": \"" << name
          << "\", \"path\": \"/sdcard/Android/data/dev.aether.editor.u07/files/Projetos/" << name
          << "\", \"template\": \"empty\", \"scenes\": 1, \"assets\": " << registry.records().size() << "}}";
  write(root / "project.json", project.str());
  std::cout << name << '\n';
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 3) { std::cerr << "uso: g7_api_scene_author <pasta-dos-scripts> <saida>\n"; return 1; }
  const fs::path scripts = fs::absolute(argv[1]), output = fs::absolute(argv[2]);
  const auto assets = makeAssets();
  const u32 call = scene::kEventConnectionCallMethod;

  for (int csharp = 1; csharp >= 0; --csharp) {
    const std::string suffix = csharp ? " (C#)" : " (Flow)";
    // 1. Ciclo de dia acelerado.
    {
      Builder b;b.base(assets, 16, csharp ? 0 : 2);
      // Sonda Via script nas duas variantes: o script (C#) ou a Conexão (Flow)
      // a recaptura com o céu da hora. "Ao acordar" refletiria a tarde à noite.
      EditorEntityId probe = 0;b.chromeProbe(probe, 2u);
      if (csharp) b.script(b.sun, "g7.dia_acelerado", "DiaAcelerado.cs",
                           {{"velocidade", "float", "720"}, {"hora_inicial", "float", "16"}, {"sonda", "object", std::to_string(probe)}});
      // Flow: Duração do dia de 2 min no Inspector; a cada hora a sonda recaptura o céu.
      else b.connection(b.sun, 39, call, 35, probe);
      publish(output, "G7 1 Dia acelerado" + suffix, b, assets, scripts, csharp ? "DiaAcelerado.cs" : nullptr);
    }
    // 2. Trocar o perfil do Volume ao entrar numa área.
    {
      Builder b;b.base(assets, 15, 0);
      const float glass[3]{.6f, .8f, 1};
      const auto area = b.folder(0, "Área coberta");
      b.place(area, 0, 1, 6);
      b.physics(area, scene::BodyMotion::Static, true, 3, 1.5f, 3);
      const float red[3]{.9f, .15f, .1f};
      const auto visitor = b.primitive(0, "Visitante", "sphere", red, 0, .4f);
      b.place(visitor, 0, 24, 6);
      b.physics(visitor, scene::BodyMotion::Dynamic, false, .5f, .5f, .5f);
      (void)glass;
      if (csharp) b.script(area, "g7.perfil_por_area", "PerfilPorArea.cs",
                           {{"volume", "object", std::to_string(b.volume)}, {"perfil_da_area", "asset", guidText(assets.interior)}});
      else {
        b.connection(area, 3, call, 37, b.volume, 0, assets.interior);
        b.connection(area, 4, call, 37, b.volume, 0, assets.exterior);
      }
      publish(output, "G7 2 Perfil por area" + suffix, b, assets, scripts, csharp ? "PerfilPorArea.cs" : nullptr);
    }
    // 3. Desfoque de profundidade ao mirar.
    {
      Builder b;b.base(assets, 15, 0);
      const auto aim = b.folder(0, "Volume de mira");
      {
        auto values = *b.doc.find(aim);
        auto *environment = static_cast<scene::Environment *>(values.components.add(scene::Environment::descriptor));
        environment->values.depthOfField = renderer::DepthOfFieldMode::Bokeh;
        environment->values.focusDistance = 4;environment->values.focalLength = 85;environment->values.aperture = 1.8f;
        environment->values.priority = 10;
        auto overrides = renderer::VolumeOverrides::empty();
        overrides.addGroup(renderer::VolumeGroup::DepthOfField);
        overrides.setGroupParameters(renderer::VolumeGroup::DepthOfField, true);
        environment->setVolumeOverrides(overrides);
        environment->weight = 0;
        if (!b.doc.applyEntityValues(aim, values)) fail("volume de mira recusado");
      }
      // Ação "Mirar" no botão de toque secundário do Play, nas duas variantes.
      auto map = b.doc.inputActions();
      runtime::InputAction action;action.id = "Mirar";action.kind = runtime::ActionKind::Button;
      runtime::InputBinding touch;touch.source = runtime::InputSource::TouchButton;touch.code = 1;
      action.bindings.push_back(touch);
      if (!map.add(action) || !b.doc.setInputActions(map)) fail("ação Mirar recusada");
      const u32 aimAction = static_cast<u32>(map.actions().size());
      if (csharp) b.script(b.camera, "g7.mira_com_desfoque", "MiraComDesfoque.cs", {{"volume_de_mira", "object", std::to_string(aim)}});
      else {
        // Flow: Entrada do jogador (PlayerInput › Invoke Unity Events). Mirar
        // executada liga o Volume de mira; cancelada desliga.
        auto values = *b.doc.find(b.camera);
        values.components.add(scene::PlayerInput::descriptor);
        for (const u32 event : {43u, 44u}) {
          auto *c = static_cast<scene::EventConnection *>(values.components.add(scene::EventConnection::descriptor));
          c->event = event;c->action = call;c->method = 36;c->receiver = aim;c->argument = event == 43 ? 1.0f : 0.0f;
          c->inputAction = aimAction;
          if (!c->valid()) fail("conexão de entrada recusada");
        }
        if (!b.doc.applyEntityValues(b.camera, values)) fail("entrada recusada");
      }
      publish(output, "G7 3 Desfoque ao mirar" + suffix, b, assets, scripts, csharp ? "MiraComDesfoque.cs" : nullptr);
    }
    // 4. Acender os postes ao anoitecer.
    {
      Builder b;b.base(assets, csharp ? 15 : 17.2f, csharp ? 0 : 2);
      const auto posts = b.folder(0, "Postes");
      const float iron[3]{.12f, .12f, .13f}, bulb[3]{1, .85f, .6f}, glow[3]{1, .65f, .3f};
      const float spots[][2]{{-4, 2}, {4, 2}, {-4, 10}, {4, 10}};
      for (u32 i = 0; i < std::size(spots); ++i) {
        const auto post = b.folder(posts, ("Poste " + std::to_string(i + 1)).c_str());
        b.place(post, spots[i][0], 0, spots[i][1]);
        const auto pole = b.primitive(post, "Haste", "cylinder", iron, .6f, .5f);
        b.place(pole, 0, 1.6f, 0, .15f, 1.6f, .15f);
        const auto lamp = b.primitive(post, "Lâmpada", "sphere", bulb, 0, .3f, glow, csharp ? 0.0f : 4.0f);
        b.place(lamp, 0, 3.3f, 0, .45f, .45f, .45f);
        const auto light = b.folder(post, "Luz");
        b.place(light, 0, 3.1f, 0);
        auto values = *b.doc.find(light);
        auto *component = static_cast<scene::Light *>(values.components.add(scene::Light::descriptor));
        scene::applyLightDefault(*component, scene::LightKind::Point);
        if (!b.doc.applyEntityValues(light, values)) fail("luz do poste recusada");
        triple(values, scene::Light::descriptor, "color", 1, .72f, .42f);
        set(values, scene::Light::descriptor, "range", Value{9.0f});
        if (!b.doc.applyEntityValues(light, values)) fail("luz do poste recusada");
      }
      if (csharp) b.script(b.sun, "g7.postes_ao_anoitecer", "PostesAoAnoitecer.cs",
                           {{"postes", "object", std::to_string(posts)}, {"velocidade", "float", "720"}, {"hora_inicial", "float", "17.2"}});
      else {
        auto values = *b.doc.find(posts);values.active = false;
        if (!b.doc.applyEntityValues(posts, values)) fail("postes recusados");
        b.connection(b.sun, 41, 1, 0, posts);
        b.connection(b.sun, 40, 2, 0, posts);
      }
      publish(output, "G7 4 Postes ao anoitecer" + suffix, b, assets, scripts, csharp ? "PostesAoAnoitecer.cs" : nullptr);
    }
    // 5. Assar uma sonda de reflexo por script.
    {
      Builder b;b.base(assets, 15, 0);
      EditorEntityId probe = 0;b.chromeProbe(probe, 2u);
      const float red[3]{1, .1f, .05f};
      // Painel redondo (cilindro achatado) virado para a esfera.
      const auto panel = b.primitive(0, "Painel", "cylinder", red, 0, .5f, red, 3);
      b.place(panel, csharp ? 2.4f : -3, 1.6f, 4.5f, 2.2f, .08f, 2.2f, 0, 0);
      {
        auto values = *b.doc.find(panel);values.transform.rotationDegrees[2] = 90;
        if (!b.doc.applyEntityValues(panel, values)) fail("painel recusado");
      }
      if (csharp) b.script(probe, "g7.sonda_por_script", "SondaPorScript.cs", {{"painel", "object", std::to_string(panel)}, {"intervalo", "float", "2"}});
      else {
        // O painel anda de um lado ao outro; a sonda só o vê quando o Timer manda.
        auto values = *b.doc.find(panel);
        values.components.add(scene::TransformTween::descriptor);
        set(values, scene::TransformTween::descriptor, "duration", Value{3.0f});
        set(values, scene::TransformTween::descriptor, "pingpong", Value{true});
        set(values, scene::TransformTween::descriptor, "position_x", Value{3.0f});
        set(values, scene::TransformTween::descriptor, "position_y", Value{1.6f});
        set(values, scene::TransformTween::descriptor, "position_z", Value{4.5f});
        set(values, scene::TransformTween::descriptor, "loops", Value{0u});
        if (!b.doc.applyEntityValues(panel, values)) fail("tween recusado");
        b.connection(b.timer("Timer: capturar", 2, true), 1, call, 35, probe);
      }
      publish(output, "G7 5 Sonda por script" + suffix, b, assets, scripts, csharp ? "SondaPorScript.cs" : nullptr);
    }
    // 6. Trocar o material de um objeto.
    {
      Builder b;b.base(assets, 15, 0);
      {
        auto values = *b.doc.find(b.statue);
        auto *mesh = static_cast<scene::MeshRenderer *>(values.components.edit(scene::MeshRenderer::descriptor));
        mesh->material.enabled = false;
        resource(*mesh, "material", assets.gold);
        if (!b.doc.applyEntityValues(b.statue, values)) fail("estátua recusada");
      }
      if (csharp) b.script(b.statue, "g7.troca_de_material", "TrocaDeMaterial.cs",
                           {{"material_a", "asset", guidText(assets.gold)}, {"material_b", "asset", guidText(assets.neon)}, {"intervalo", "float", "2"}});
      else {
        b.connection(b.timer("Timer: neon", 2, false), 1, call, 39, b.statue, 0, assets.neon);
        b.connection(b.timer("Timer: ouro", 5, false), 1, call, 39, b.statue, 0, assets.gold);
      }
      publish(output, "G7 6 Troca de material" + suffix, b, assets, scripts, csharp ? "TrocaDeMaterial.cs" : nullptr);
    }
  }
  return 0;
}

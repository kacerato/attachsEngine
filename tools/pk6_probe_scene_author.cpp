// PK-6: variante de validação do Sponza para Reflection Probe e Light Probe Group.
//
// Lê a cena importada do projeto Sponza (uma cópia puxada do aparelho, nunca o
// original) e escreve outra pasta com:
//  - a arquitetura marcada Static (a Unity só assa o que é Static);
//  - um sol direcional com sombra suave que entra no pátio e deixa a arcada
//    norte na sombra do teto;
//  - duas sondas de reflexo com Box Projection, uma no pátio e outra na arcada
//    (importância 2), nas medidas reais do glTF (colunata em z=±3,65 m, arcada
//    até z≈6 m, teto a 3 m);
//  - um Grupo de sondas de luz de 50 pontos cobrindo pátio e arcada;
//  - duas esferas cromadas (uma em cada sonda) e duas esferas brancas foscas
//    dinâmicas, uma no sol e outra na sombra da arcada, com Sondas de luz =
//    Misturar sondas, e uma câmera de cena que enquadra as quatro.
// Uso: pk6_probe_scene_author <projeto-copiado> <saida>
#include "editor/editor_archive.h"
#include "resources/asset_registry.h"
#include "scene/light.h"
#include "scene/light_probe_group.h"
#include "scene/mesh_renderer.h"
#include "scene/reflection_probe.h"
#include "scene/camera.h"

#include <filesystem>
#include <iostream>
#include <sstream>

using namespace ae;
using namespace ae::editor;

namespace {
[[noreturn]] void fail(const std::string &message) {
  std::cerr << message << '\n';
  std::exit(20);
}
EditorEntityId sphere(EditorDocument &doc, EditorEntityId parent, const char *name, const float position[3], float diameter,
                      float metallic, float roughness, bool isStatic) {
  const auto id = doc.createEntity(parent, EditorEntityKind::Mesh, name);
  auto values = *doc.find(id);
  for (u32 k = 0; k < 3; ++k) {
    values.transform.position[k] = position[k];
    values.transform.scale[k] = diameter;
  }
  values.isStatic = isStatic;
  auto *mesh = static_cast<scene::MeshRenderer *>(values.components.edit(scene::MeshRenderer::descriptor));
  if (!mesh) fail("sem Mesh Renderer na entidade de malha");
  mesh->asset = resources::assetGuidFromSeed("astra:builtin:sphere:v1");
  // Slot provisório: a identidade da esfera embutida reconcilia o slot na carga.
  mesh->mesh = 1;
  mesh->material.enabled = true;
  mesh->material.baseColor[0] = mesh->material.baseColor[1] = mesh->material.baseColor[2] = .95f;
  mesh->material.metallic = metallic;
  mesh->material.roughness = roughness;
  mesh->lightProbes = 1;
  mesh->reflectionProbes = 1;
  if (!doc.applyEntityValues(id, values)) fail(std::string("recusou ") + name);
  return id;
}
EditorEntityId reflectionProbe(EditorDocument &doc, const char *name, const float capture[3], const float lo[3], const float hi[3],
                               float importance) {
  const auto id = doc.createEntity(doc.root(), EditorEntityKind::Folder, name);
  auto values = *doc.find(id);
  for (u32 k = 0; k < 3; ++k) values.transform.position[k] = capture[k];
  auto *probe = static_cast<scene::ReflectionProbe *>(values.components.add(scene::ReflectionProbe::descriptor));
  for (u32 k = 0; k < 3; ++k) {
    probe->boxSize[k] = hi[k] - lo[k];
    probe->boxOffset[k] = (lo[k] + hi[k]) * .5f - capture[k];
  }
  probe->boxProjection = true;
  probe->importance = importance;
  probe->blendDistance = .5f;
  probe->resolution = 128;
  if (!probe->valid() || !doc.applyEntityValues(id, values)) fail(std::string("sonda recusada: ") + name);
  return id;
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 3) { std::cerr << "uso: pk6_probe_scene_author <projeto-copiado> <saida>\n"; return 1; }
  const std::filesystem::path root = std::filesystem::absolute(argv[1]), output = std::filesystem::absolute(argv[2]);
  EditorDocument doc;
  if (!loadEditorDocument((root / "scenes/editor.aescene").string().c_str(), 0, doc)) fail("cena recusada");
  const auto check = [&](const char *step) {
    EditorDocument probe;
    if (!deserializeEditorDocument(serializeEditorDocument(doc, 0), 0, probe)) std::cerr << "ida e volta quebra depois de: " << step << std::endl;
  };
  check("carregar");
  std::vector<EditorEntityId> ids;
  doc.collectSubtree(doc.root(), ids);
  u32 statics = 0;
  for (auto id : ids) {
    const auto *object = doc.find(id);
    if (!object || id == doc.root() || !object->components.find(scene::MeshRenderer::descriptor)) continue;
    auto values = *object;
    values.isStatic = true;
    if (!doc.applyEntityValues(id, values)) fail("Static recusado");
    ++statics;
  }
  check("Static");
  // Sol: entra no pátio pela abertura do telhado; a arcada fica sob o teto.
  const auto sunId = doc.createEntity(doc.root(), EditorEntityKind::Folder, "Sol de validação");
  auto sun = *doc.find(sunId);
  sun.transform.rotationDegrees[0] = 58;
  sun.transform.rotationDegrees[1] = 160;
  auto *light = static_cast<scene::Light *>(sun.components.add(scene::Light::descriptor));
  scene::applyLightDefault(*light, scene::LightKind::Directional);
  light->shadowMode = 2;
  if (!doc.applyEntityValues(sunId, sun)) fail("sol recusado");

  check("sol");
  const float courtCapture[3]{.5f, 1.6f, 0}, courtLo[3]{-11, 0, -3.6f}, courtHi[3]{12, 12, 3.6f};
  const float arcadeCapture[3]{.5f, 1.4f, 4.85f}, arcadeLo[3]{-11, 0, 3.7f}, arcadeHi[3]{12, 3, 6.0f};
  reflectionProbe(doc, "Sonda de reflexo do pátio", courtCapture, courtLo, courtHi, 1);
  reflectionProbe(doc, "Sonda de reflexo da arcada", arcadeCapture, arcadeLo, arcadeHi, 2);

  check("sondas de reflexo");
  const auto groupId = doc.createEntity(doc.root(), EditorEntityKind::Folder, "Sondas de luz do pátio");
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

  check("grupo");
  const auto setId = doc.createEntity(doc.root(), EditorEntityKind::Folder, "Objetos de teste das sondas");
  const float chromeCourt[3]{2, .8f, 0}, chromeArcade[3]{2, .8f, 4.85f}, sunlit[3]{-2, .5f, 1.8f}, shaded[3]{-2, .5f, 5.0f};
  sphere(doc, setId, "Esfera cromada do pátio", chromeCourt, 1.4f, 1, .04f, false);
  sphere(doc, setId, "Esfera cromada da arcada", chromeArcade, 1.4f, 1, .04f, false);
  sphere(doc, setId, "Esfera fosca no sol", sunlit, 1.0f, 0, .9f, false);
  sphere(doc, setId, "Esfera fosca na sombra", shaded, 1.0f, 0, .9f, false);

  check("esferas");
  const auto cameraId = doc.createEntity(doc.root(), EditorEntityKind::Camera, "Câmera de validação das sondas");
  auto camera = *doc.find(cameraId);
  camera.transform.position[0] = -7.5f;
  camera.transform.position[1] = 1.7f;
  camera.transform.position[2] = 1.2f;
  camera.transform.rotationDegrees[0] = 6;
  camera.transform.rotationDegrees[1] = 72;
  if (!doc.applyEntityValues(cameraId, camera)) fail("câmera recusada");

  std::filesystem::create_directories(output / "scenes");
  const auto authored = serializeEditorDocument(doc, 0);
  // Diagnóstico: qual componente não volta pela própria leitura.
  doc.collectSubtree(doc.root(), ids);
  for (auto id : ids) {
    const auto *object = doc.find(id);
    if (!object) continue;
    for (usize i = 0; i < object->components.size(); ++i) {
      const auto *component = object->components.at(i);
      std::ostringstream out;
      component->write(out);
      auto copy = component->type().create();
      std::istringstream in(out.str());
      if (!component->valid() || !copy->read(in, component->type().version))
        std::cerr << "componente não volta: " << object->name << " · " << component->type().id << " · " << out.str().substr(0, 160) << '\n';
    }
  }
  {
    EditorDocument check;
    if (!deserializeEditorDocument(authored, 0, check)) std::cerr << "a cena inteira não volta pelo leitor\n";
  }
  if (!saveEditorDocument((output / "scenes/editor.aescene").string().c_str(), doc, 0)) fail("gravação recusada");
  EditorDocument restored;
  if (!loadEditorDocument((output / "scenes/editor.aescene").string().c_str(), 0, restored) ||
      serializeEditorDocument(restored, 0) != authored) fail("ida e volta da cena divergiu");
  std::cout << "Static: " << statics << " objetos; sondas de reflexo: 2; sondas de luz: " << 50 << "; esferas: 4\n";
  return 0;
}

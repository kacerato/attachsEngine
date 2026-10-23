// PK-4/PK-5: variantes da Vila para a prancha Unity × POCO do pós completo.
//
// Lê a cena de uma CÓPIA do projeto Vila Astra (nunca o original) e grava outra
// pasta em que cada Ambiente da cena ganha um efeito do pós como override do
// Volume -- o mesmo que Add Override › <efeito> e ALL no Inspector, com os
// valores dos vídeos de referência (docs/validacao/pk4-pk5-volume-pos-2026-10-22).
// O perfil sai da variante para que os valores da instância valham sozinhos.
// Uso: pk5_post_scene_author <projeto-copiado> <saida> <variante> [qualidade 0..2]
// Variantes: neutro, dof-gauss, dof-bokeh, movimento, aberracao, lente-barril,
// lente-almofada, vinheta, curvas, tudo.
#include "editor/editor_archive.h"
#include "renderer/volume_parameters.h"
#include "scene/environment.h"

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

using G = renderer::VolumeGroup;

void add(renderer::VolumeOverrides &overrides, G group) {
  if (!overrides.hasGroup(group)) overrides.addGroup(group);
  overrides.setGroupParameters(group, true);
  overrides.active |= 1u << static_cast<u32>(group);
}

// Curva com as chaves dadas (tempo, valor), tangentes automáticas.
renderer::ColorCurve curve(std::initializer_list<std::pair<float, float>> keys) {
  renderer::ColorCurve result;
  for (const auto &[time, value] : keys) {
    auto &key = result.keys[result.count++];
    key.time = time;key.value = value;key.left = key.right = 1;
  }
  renderer::updateColorCurveTangents(result);
  return result;
}

bool author(std::string_view variant, renderer::EffectQuality quality, renderer::SceneEnvironment &v,
            renderer::VolumeOverrides &o) {
  const bool all = variant == "tudo";
  v.post = true;
  if (variant == "neutro") return true;
  if (variant == "dof-gauss") {
    // "Dynamic Depth of Field in Unity" 04:30: Gaussian, Start 10, End 30, Max Radius 1.
    add(o, G::DepthOfField);
    v.depthOfField = renderer::DepthOfFieldMode::Gaussian;v.depthOfFieldQuality = quality;
    v.gaussianStart = 6.0f;v.gaussianEnd = 25.0f;v.gaussianMaxRadius = 1.0f;
    v.gaussianHighQuality = quality == renderer::EffectQuality::High;
    return true;
  }
  if (variant == "dof-bokeh" || all) {
    // 07:00/08:30 do mesmo vídeo: Bokeh com foco perto e lente longa (100 mm, f/2,8).
    add(o, G::DepthOfField);
    v.depthOfField = renderer::DepthOfFieldMode::Bokeh;v.depthOfFieldQuality = quality;
    v.focusDistance = 5.0f;v.focalLength = 100.0f;v.aperture = 2.8f;
    v.bladeCount = 5;v.bladeCurvature = 1.0f;v.bladeRotation = 0.0f;
    if (!all) return true;
  }
  if (variant == "movimento" || all) {
    // "EVERY Post-Processing effect" 07:10: Camera Only, Intensity 1, Clamp 0,05.
    add(o, G::MotionBlur);
    v.motionBlurQuality = quality;v.motionBlurIntensity = 1.0f;v.motionBlurClamp = 0.05f;
    if (!all) return true;
  }
  if (variant == "aberracao" || all) {
    add(o, G::ChromaticAberration); // 06:07: Intensity 1.
    v.chromaticAberration = all ? 0.4f : 1.0f;
    if (!all) return true;
  }
  if (variant == "lente-barril" || variant == "lente-almofada" || all) {
    add(o, G::LensDistortion); // 06:48: Intensity -0,5 (barril); 07:00 extremo.
    v.lensDistortion = variant == "lente-almofada" ? 0.5f : -0.5f;
    v.lensDistortionXMultiplier = v.lensDistortionYMultiplier = 1.0f;
    v.lensDistortionCenter[0] = v.lensDistortionCenter[1] = 0.5f;v.lensDistortionScale = 1.0f;
    if (all) v.lensDistortion = -0.2f;
    if (!all) return true;
  }
  if (variant == "vinheta" || all) {
    add(o, G::Vignette); // 07:58/08:06: Intensity 0,45, Smoothness 0,35, preta.
    v.vignetteIntensity = 0.45f;v.vignetteSmoothness = 0.35f;v.vignetteRounded = false;
    v.vignetteColor[0] = v.vignetteColor[1] = v.vignetteColor[2] = 0.0f;
    v.vignetteCenter[0] = v.vignetteCenter[1] = 0.5f;
    if (!all) return true;
  }
  if (variant == "curvas" || all) {
    // 03:19: Master em S (contraste) e Hue Vs Sat puxando o verde para baixo.
    add(o, G::ColorCurves);
    v.curves[static_cast<u32>(renderer::ColorCurveChannel::Master)] =
        curve({{0.0f, 0.0f}, {0.25f, 0.18f}, {0.75f, 0.84f}, {1.0f, 1.0f}});
    v.curves[static_cast<u32>(renderer::ColorCurveChannel::HueVsSat)] =
        curve({{0.0f, 0.5f}, {0.25f, 0.5f}, {0.33f, 0.15f}, {0.42f, 0.5f}});
    return true;
  }
  return all;
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 4 && argc != 5) {
    std::cerr << "uso: pk5_post_scene_author <projeto-copiado> <saida> <variante> [qualidade 0..2]\n";
    return 1;
  }
  const std::filesystem::path root = std::filesystem::absolute(argv[1]), output = std::filesystem::absolute(argv[2]);
  const std::string_view variant = argv[3];
  const auto quality = static_cast<renderer::EffectQuality>(argc == 5 ? std::stoul(argv[4]) : 1u);
  if (static_cast<u32>(quality) > 2) fail("qualidade fora de 0..2");
  EditorDocument doc;
  if (!loadEditorDocument((root / "scenes/editor.aescene").string().c_str(), 0, doc)) fail("cena recusada");
  std::vector<EditorEntityId> ids;
  doc.collectSubtree(doc.root(), ids);
  u32 authored = 0;
  for (auto id : ids) {
    const auto *object = doc.find(id);
    if (!object || !object->components.find(scene::Environment::descriptor)) continue;
    auto values = *object;
    auto *environment = static_cast<scene::Environment *>(values.components.edit(scene::Environment::descriptor));
    environment->profile = {};environment->legacyProfileCopy = false;
    auto overrides = environment->volumeOverrides();
    add(overrides, G::Tonemapping);
    if (!author(variant, quality, environment->values, overrides)) fail("variante desconhecida: " + std::string(variant));
    environment->setVolumeOverrides(overrides);
    if (!environment->valid() || !doc.applyEntityValues(id, values)) fail("Ambiente recusado em " + std::string(object->name));
    ++authored;
  }
  if (!authored) fail("nenhum Ambiente na cena");
  std::filesystem::create_directories(output / "scenes");
  const auto text = serializeEditorDocument(doc, 0);
  if (!saveEditorDocument((output / "scenes/editor.aescene").string().c_str(), doc, 0)) fail("gravação recusada");
  EditorDocument restored;
  if (!loadEditorDocument((output / "scenes/editor.aescene").string().c_str(), 0, restored) ||
      serializeEditorDocument(restored, 0) != text) fail("ida e volta da cena divergiu");
  std::cout << "Vila com " << variant << " (qualidade " << static_cast<u32>(quality) << ") em " << authored << " Ambiente(s)\n";
  return 0;
}

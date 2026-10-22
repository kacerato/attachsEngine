#include "resources/material_asset.h"
#include "scene/mesh_renderer.h"
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace ae::resources {
bool MaterialAsset::valid() const {
  if (!guid.valid() || !revision || name.empty() || name.size() > 128) return false;
  for (unsigned char c : name) if (c < 32 || c == 127) return false;
  for (const auto &value : sampling) if (!scene::validMaterialSampling(value)) return false;
  if (!scene::validMaterialChannels(channels) || !scene::validMaterialShading(shading)) return false;
  for (const auto &texture : extraTextures) if (texture == scene::MaterialTextureNone) return false;
  return scene::MeshRenderer::validMaterial(values) && scene::validMaterialSurface(surface);
}

std::string MaterialAsset::serialize() const {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(std::numeric_limits<float>::max_digits10);
  out << "ASTRA_MATERIAL " << FormatVersion << ' ' << guid.text() << ' ' << revision << ' ' << std::quoted(name);
  // A mesma ordem e os mesmos limites dos números do componente de malha: um
  // campo, uma definição.
  scene::MeshRenderer probe;
  probe.material = values;
  for (const auto &number : scene::MeshRenderer::descriptor.numbers) out << ' ' << number.read(probe);
  for (const auto &texture : textures) out << ' ' << scene::materialTextureToken(texture);
  out << ' ' << static_cast<unsigned>(surface.alphaMode) << ' ' << static_cast<unsigned>(surface.sides) << ' ' << surface.alphaCutoff;
  for (const auto &value : sampling) out << ' ' << scene::materialSamplingToken(value);
  for (const auto &value : sampling)
    out << ' ' << value.offset[0] << ' ' << value.offset[1] << ' ' << value.scale[0] << ' ' << value.scale[1] << ' ' << value.rotation;
  out << ' ' << static_cast<unsigned>(channels.roughness) << ' ' << static_cast<unsigned>(channels.metallic) << ' '
      << static_cast<unsigned>(channels.occlusion) << ' ' << static_cast<unsigned>(channels.occlusionSource) << ' ' << channels.occlusionStrength
      << ' ' << static_cast<unsigned>(channels.normalFlipY) << ' ' << static_cast<unsigned>(channels.alphaSource) << ' '
      << scene::materialTextureToken(occlusionTexture);
  out << ' ' << static_cast<unsigned>(channels.roughnessSource);
  const auto &s = shading;
  out << ' ' << unsigned(s.shader) << ' ' << unsigned(s.workflow) << ' ' << unsigned(s.blend) << ' ' << unsigned(s.preserveSpecular) << ' '
      << unsigned(s.transparentClip) << ' ' << unsigned(s.specularHighlights) << ' ' << unsigned(s.environmentReflections) << ' '
      << int(s.sortingPriority) << ' ' << unsigned(s.particleColorMode) << ' ' << unsigned(s.cameraFading) << ' ' << s.cameraFadeNear << ' '
      << s.cameraFadeFar << ' ' << s.specularColor[0] << ' ' << s.specularColor[1] << ' ' << s.specularColor[2] << ' ' << s.heightScale << ' '
      << s.detailAlbedoScale << ' ' << s.detailNormalScale << ' ' << scene::materialSamplingToken(s.detailSampling) << ' '
      << s.detailSampling.offset[0] << ' ' << s.detailSampling.offset[1] << ' ' << s.detailSampling.scale[0] << ' ' << s.detailSampling.scale[1]
      << ' ' << s.detailSampling.rotation << ' ' << unsigned(s.clearCoat) << ' ' << s.clearCoatMask << ' ' << s.clearCoatSmoothness;
  for (const auto &texture : extraTextures) out << ' ' << scene::materialTextureToken(texture);
  out << '\n';
  return out.str();
}

bool MaterialAsset::deserialize(std::string_view text, MaterialAsset &out) {
  if (text.size() > 64 * 1024) return false;
  std::istringstream in{std::string(text)};
  in.imbue(std::locale::classic());
  std::string magic, guidText;
  u32 version = 0;
  MaterialAsset candidate;
  if (!(in >> magic >> version >> guidText >> candidate.revision >> std::quoted(candidate.name)) || magic != "ASTRA_MATERIAL" ||
      version < 1 || version > FormatVersion || !AssetGuid::parse(guidText, candidate.guid))
    return false;
  scene::MeshRenderer probe;
  for (const auto &number : scene::MeshRenderer::descriptor.numbers) if (!(in >> *number.write(probe))) return false;
  candidate.values = scene::withoutResolvedTextures(probe.material);
  candidate.values.enabled = true;
  if (version >= 2)
    for (auto &texture : candidate.textures) {
      std::string token;
      if (!(in >> token) || !scene::parseMaterialTextureToken(token, texture)) return false;
    }
  if (version >= 3) {
    unsigned alpha = 0, sides = 0;
    float cutoff = .5f;
    if (!(in >> alpha >> sides >> cutoff) || alpha > scene::MaterialAlphaBlend || sides > scene::MaterialSidesBack) return false;
    candidate.surface = {static_cast<std::uint8_t>(alpha), static_cast<std::uint8_t>(sides), cutoff};
  }
  if (version >= 4)
    for (auto &value : candidate.sampling) {
      std::string token;
      if (!(in >> token) || !scene::parseMaterialSamplingToken(token, value)) return false;
    }
  if (version >= 5)
    for (auto &value : candidate.sampling)
      if (!(in >> value.offset[0] >> value.offset[1] >> value.scale[0] >> value.scale[1] >> value.rotation)) return false;
  if (version >= 6) {
    unsigned roughness = 0, metallic = 0, occlusion = 0, source = 0, flip = 0, alpha = 0;
    float strength = -1;
    std::string token;
    if (!(in >> roughness >> metallic >> occlusion >> source >> strength >> flip >> alpha >> token) || roughness > 255 || metallic > 255 ||
        occlusion > 255 || source > 255 || flip > 255 || alpha > 255 || !scene::parseMaterialTextureToken(token, candidate.occlusionTexture))
      return false;
    candidate.channels = {static_cast<std::uint8_t>(roughness), static_cast<std::uint8_t>(metallic), static_cast<std::uint8_t>(occlusion),
                          static_cast<std::uint8_t>(source), strength, static_cast<std::uint8_t>(flip), static_cast<std::uint8_t>(alpha)};
  }
  // v7 (G3a): origem da rugosidade. Antes dela tudo era rugosidade herdada.
  if (version >= 7) {
    unsigned roughnessSource = 0;
    if (!(in >> roughnessSource) || roughnessSource > 255) return false;
    candidate.channels.roughnessSource = static_cast<std::uint8_t>(roughnessSource);
  }
  // v8 (PK-9): shader e opções do URP Lit. Antes dela: Lit, Metallic, Alpha
  // reto -- o transparente antigo multiplicava o brilho pelo alfa, então
  // Preserve Specular nasce desligado na migração (o padrão novo é o da Unity).
  if (version < 8) candidate.shading.preserveSpecular = false;
  if (version >= 8) {
    auto &s = candidate.shading;
    unsigned shader = 0, workflow = 0, blend = 0, preserve = 0, clip = 0, highlights = 0, reflections = 0, colorMode = 0, fading = 0,
             coat = 0;
    int priority = 0;
    std::string token;
    if (!(in >> shader >> workflow >> blend >> preserve >> clip >> highlights >> reflections >> priority >> colorMode >> fading >>
          s.cameraFadeNear >> s.cameraFadeFar >> s.specularColor[0] >> s.specularColor[1] >> s.specularColor[2] >> s.heightScale >>
          s.detailAlbedoScale >> s.detailNormalScale >> token) ||
        shader > 255 || workflow > 255 || blend > 255 || colorMode > 255 || preserve > 1 || clip > 1 || highlights > 1 || reflections > 1 ||
        fading > 1 || priority < -128 || priority > 127 || !scene::parseMaterialSamplingToken(token, s.detailSampling))
      return false;
    if (!(in >> s.detailSampling.offset[0] >> s.detailSampling.offset[1] >> s.detailSampling.scale[0] >> s.detailSampling.scale[1] >>
          s.detailSampling.rotation >> coat >> s.clearCoatMask >> s.clearCoatSmoothness) || coat > 1)
      return false;
    s.shader = static_cast<std::uint8_t>(shader);s.workflow = static_cast<std::uint8_t>(workflow);s.blend = static_cast<std::uint8_t>(blend);
    s.preserveSpecular = preserve;s.transparentClip = clip;s.specularHighlights = highlights;s.environmentReflections = reflections;
    s.sortingPriority = static_cast<std::int8_t>(priority);s.particleColorMode = static_cast<std::uint8_t>(colorMode);
    s.cameraFading = fading;s.clearCoat = coat;
    for (auto &texture : candidate.extraTextures)
      if (!(in >> token) || !scene::parseMaterialTextureToken(token, texture) || texture == scene::MaterialTextureNone) return false;
  }
  in >> std::ws;
  if (!in.eof() || !candidate.valid()) return false;
  out = std::move(candidate);
  return true;
}
} // namespace ae::resources

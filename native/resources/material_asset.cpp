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
  return scene::MeshRenderer::validMaterial(values);
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
      version != FormatVersion || !AssetGuid::parse(guidText, candidate.guid))
    return false;
  scene::MeshRenderer probe;
  for (const auto &number : scene::MeshRenderer::descriptor.numbers) if (!(in >> *number.write(probe))) return false;
  candidate.values = probe.material;
  candidate.values.enabled = true;
  in >> std::ws;
  if (!in.eof() || !candidate.valid()) return false;
  out = std::move(candidate);
  return true;
}
} // namespace ae::resources

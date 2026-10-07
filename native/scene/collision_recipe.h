#pragma once
#include "resources/convex_bake.h"
#include "scene/collider.h"
#include <utility>

namespace ae::scene {
// Authoring provenance lives with its instance. Generated meshes remain normal
// runtime Collider resources. References participate in clone/prefab remapping.
class CollisionRecipe final : public ComponentValue {
public:
  struct Source {
    u64 object = 0;
    u32 slot = 0;
    friend bool operator==(const Source &, const Source &) = default;
  };
  struct Part {
    u64 collider = 0;
    u32 source = 0;
    Collider baseline;
    std::array<float, 3> low{}, high{};
    std::string geometryHash;
  };
  resources::ConvexBakeSettings settings;
  std::vector<Source> sources;
  std::vector<Part> parts;
  resources::AssetGuid bakeSource;
  std::string geometryHash;
  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override {
    return std::make_unique<CollisionRecipe>(*this);
  }
  bool valid() const override {
    const auto hash = [](const std::string &s) {
      return s.size() == 64 &&
             s.find_first_not_of("0123456789abcdef") == std::string::npos;
    };
    if (!resources::validConvexBakeSettings(settings) || sources.size() > 128 ||
        parts.size() > 32)
      return false;
    if (sources.empty() || parts.empty())
      return sources.empty() && parts.empty() && !bakeSource.valid() &&
             geometryHash.empty();
    if (!bakeSource.valid() || !hash(geometryHash))
      return false;
    for (usize i = 0; i < sources.size(); ++i) {
      if (sources[i].object > std::numeric_limits<u32>::max() ||
          sources[i].slot > 65535)
        return false;
      for (usize j = 0; j < i; ++j)
        if (sources[i].object && sources[i] == sources[j])
          return false;
    }
    for (usize i = 0; i < parts.size(); ++i) {
      const auto &p = parts[i];
      if (!p.collider || p.source >= sources.size() || !p.baseline.valid() ||
          p.baseline.owner || !p.baseline.collisionMesh.valid() ||
          !hash(p.geometryHash))
        return false;
      for (u32 k = 0; k < 3; ++k)
        if (!std::isfinite(p.low[k]) || !std::isfinite(p.high[k]) ||
            p.low[k] > p.high[k])
          return false;
      for (usize j = 0; j < i; ++j)
        if (parts[j].collider == p.collider)
          return false;
    }
    return true;
  }
  void write(std::ostream &out) const override {
    out << settings.maximumParts << ' ' << settings.voxelResolution << ' '
        << settings.maximumVertices << ' ' << settings.volumeErrorPercent << ' '
        << settings.timeBudgetSeconds << ' '
        << static_cast<u32>(settings.pose) << ' '
        << settings.animationTime << ' '
        << (bakeSource.valid() ? bakeSource.text() : "-") << ' '
        << std::quoted(geometryHash) << ' ' << sources.size();
    for (const auto &s : sources)
      out << ' ' << s.object << ' ' << s.slot;
    out << ' ' << parts.size();
    for (const auto &p : parts) {
      out << ' ' << p.collider << ' ' << p.source << ' ' << p.geometryHash;
      for (float v : p.low)
        out << ' ' << v;
      for (float v : p.high)
        out << ' ' << v;
      std::ostringstream payload;
      payload.imbue(std::locale::classic());
      payload << std::setprecision(std::numeric_limits<float>::max_digits10);
      p.baseline.write(payload);
      out << ' ' << Collider::descriptor.version << ' '
          << std::quoted(payload.str());
    }
  }
  bool read(std::istream &in, u32 version) override {
    if (version != 1 && version != 2)
      return false;
    CollisionRecipe next;
    std::string guid;
    usize count = 0;
    if (!(in >> next.settings.maximumParts >> next.settings.voxelResolution >>
          next.settings.maximumVertices >> next.settings.volumeErrorPercent >>
          next.settings.timeBudgetSeconds))
      return false;
    u32 pose=0; // Legacy recipes always used undeformed geometry.
    if(version>=2 && !(in>>pose>>next.settings.animationTime))return false;
    next.settings.pose=static_cast<resources::ConvexBakePose>(pose);
    if (!(in >> guid >> std::quoted(next.geometryHash) >> count) ||
        count > 128)
      return false;
    if (guid != "-" && !resources::AssetGuid::parse(guid, next.bakeSource))
      return false;
    next.sources.resize(count);
    for (auto &s : next.sources)
      if (!(in >> s.object >> s.slot))
        return false;
    if (!(in >> count) || count > 32)
      return false;
    next.parts.resize(count);
    for (auto &p : next.parts) {
      std::string payload;
      u32 colliderVersion = 0;
      if (!(in >> p.collider >> p.source >> p.geometryHash))
        return false;
      for (float &v : p.low)
        if (!(in >> v))
          return false;
      for (float &v : p.high)
        if (!(in >> v))
          return false;
      if (!(in >> colliderVersion >> std::quoted(payload)) ||
          payload.size() > 4096)
        return false;
      std::istringstream data(payload);
      data.imbue(std::locale::classic());
      // The recipe can outlive the Collider schema version that created it.
      // Let the real component reader migrate its saved baseline as well.
      if (!p.baseline.read(data, colliderVersion))
        return false;
      data >> std::ws;
      if (!data.eof())
        return false;
    }
    if (!next.valid())
      return false;
    // Do not copy ComponentValue's instance identity from a temporary.
    settings = next.settings;
    sources = std::move(next.sources);
    parts = std::move(next.parts);
    bakeSource = next.bakeSource;
    geometryHash = std::move(next.geometryHash);
    return true;
  }
};
inline const CollisionRecipe *collisionRecipe(const Components &c) {
  return static_cast<const CollisionRecipe *>(
      c.find(CollisionRecipe::descriptor));
}
} // namespace ae::scene

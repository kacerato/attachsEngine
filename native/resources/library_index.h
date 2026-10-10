#pragma once
#include "resources/asset_registry.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace ae::resources {
// Provenance only. The native registry continues to own resources and their
// executable dependencies. A converted entry is not a claim of runtime parity.
enum class LibraryItemKind : u8 { Model, Clip, Material, Texture, Prefab, Scene, Code, Documentation, Other, Controller };
enum class LibraryConversionState : u8 { Pending, Converted, Incomplete, Unsupported };
struct LibraryItem {
  AssetGuid id{},nativeGuid{};
  LibraryItemKind kind=LibraryItemKind::Other;
  LibraryConversionState conversion=LibraryConversionState::Pending;
  std::string path,sourceIdentity,sourceHash;
  std::string converter,recipe,diagnostic;
  u32 converterVersion=0;
  // Package-qualified item IDs, never guessed filesystem paths. An absent
  // dependency is retained and reported rather than silently removed.
  std::vector<AssetGuid> dependencies;
  std::vector<std::string> knownLosses;
};
struct LibraryPackage {
  AssetGuid id{};
  std::string name,version,origin,sourceHash,licenseNotice;
  std::vector<LibraryItem> items;
};
enum class LibraryResolutionState : u8 { Pending, Registered, Incomplete, Unsupported, MissingOutput, MissingDependency };
struct LibraryResolution {
  LibraryResolutionState state=LibraryResolutionState::Pending;
  const AssetRecord *output=nullptr;
  AssetGuid missing{};
  std::string diagnostic;
};
class LibraryIndex final {
public:
  static constexpr u32 FormatVersion=1,MaximumPackages=128,MaximumItems=65536,MaximumDependencies=128,MaximumLosses=64;
  static constexpr usize MaximumBytes=16u*1024u*1024u;
  bool add(LibraryPackage package);
  bool replace(LibraryPackage package);
  bool remove(AssetGuid package);
  const LibraryPackage *package(AssetGuid id) const;
  const LibraryItem *item(AssetGuid id) const;
  std::span<const LibraryPackage> packages() const {return packages_;}
  // Resolves current GUIDs after rename/removal; performs no filesystem I/O and
  // never mistakes a source extension or converter name for working support.
  LibraryResolution resolve(AssetGuid item,const AssetRegistry &registry) const;
  std::string serialize() const;
  static bool deserialize(std::string_view source,LibraryIndex &out,std::string &diagnostic);
private:
  bool valid() const;
  void rebuildLocations();
  std::vector<LibraryPackage> packages_;
  std::unordered_map<AssetGuid,std::pair<usize,usize>,AssetGuidHash> itemLocations_;
};
} // namespace ae::resources

#pragma once
#include "resources/unity_yaml.h"

namespace ae::resources {
struct UnitySubasset {i64 fileId=0;u32 classId=0;std::string name;};
struct UnitySourceMetadata {
  AssetGuid guid{};
  std::vector<UnitySubasset> subassets;
  const UnitySubasset *find(i64 fileId) const;
};
// Both old fileIDToRecycleName and current internalIDToNameTable are read.
// Names come from the importer metadata, never from numeric index order.
bool readUnitySourceMetadata(std::string_view meta,UnitySourceMetadata &out,std::string &diagnostic);
} // namespace ae::resources

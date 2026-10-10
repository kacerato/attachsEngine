#pragma once
// Bounded reader for Unity's text serialization dialect, not a general YAML
// interpreter. No aliases, executable tags or implicit numeric conversions.
// Unity 6000.0: https://docs.unity3d.com/6000.0/Documentation/Manual/FormatDescription.html
#include "resources/asset_registry.h"
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ae::resources {
struct UnityYamlNode {
  enum class Kind : u8 { Scalar, Map, Sequence };
  Kind kind=Kind::Scalar;
  std::string scalar;
  std::vector<std::pair<std::string,UnityYamlNode>> members;
  std::vector<UnityYamlNode> elements;
  const UnityYamlNode *get(std::string_view name) const;
  bool integer(i64 &out) const;
  bool number(double &out,bool allowInfinity=false) const;
  bool boolean(bool &out) const;
  bool empty() const;
};
struct UnityYamlObject {
  i64 fileId=0;
  u32 classId=0;
  bool stripped=false;
  std::string type;
  UnityYamlNode value;
};
struct UnityYamlDocument {
  static constexpr usize MaximumBytes=64u*1024u*1024u,MaximumNodes=524288,MaximumDepth=64,MaximumObjects=65536;
  std::vector<UnityYamlObject> objects;
  const UnityYamlObject *find(i64 fileId) const;
};
struct UnityObjectReference {
  i64 fileId=0;
  AssetGuid guid{};
  u32 type=0;
  bool external() const {return guid.valid();}
  bool operator==(const UnityObjectReference &) const=default;
};
// All-or-nothing: output is unchanged for syntax, budget or cancellation errors.
bool readUnityYaml(std::string_view text,UnityYamlDocument &out,std::string &diagnostic,
                   const std::function<bool()> &cancelled={});
bool readUnityMeta(std::string_view text,UnityYamlNode &out,std::string &diagnostic);
bool unityObjectReference(const UnityYamlNode &node,UnityObjectReference &out);
bool unityObjectReferences(const UnityYamlDocument &document,std::vector<UnityObjectReference> &out,
                           std::string &diagnostic);
} // namespace ae::resources

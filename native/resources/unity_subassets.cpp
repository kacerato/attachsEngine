#include "resources/unity_subassets.h"
#include <charconv>
#include <limits>
#include <set>

namespace ae::resources {
const UnitySubasset *UnitySourceMetadata::find(i64 fileId) const {for(const auto &value:subassets)if(value.fileId==fileId)return &value;return nullptr;}
bool readUnitySourceMetadata(std::string_view meta,UnitySourceMetadata &out,std::string &diagnostic) {
  UnityYamlNode root;if(!readUnityMeta(meta,root,diagnostic))return false;
  UnitySourceMetadata candidate;const auto *guid=root.get("guid");
  const auto fail=[&](const char *message){diagnostic=message;return false;};
  if(!guid||guid->kind!=UnityYamlNode::Kind::Scalar||!AssetGuid::parse(guid->scalar,candidate.guid))return fail("GUID .meta Unity ausente ou inválido.");
  std::set<i64> ids;
  const auto insert=[&](i64 id,u32 type,const std::string &name)->bool {
    if(!id||name.empty()||name.size()>1024||candidate.subassets.size()>=UnityYamlDocument::MaximumObjects||!ids.insert(id).second)return false;
    candidate.subassets.push_back({id,type,name});return true;
  };
  for(const auto &importer:root.members)if(importer.second.kind==UnityYamlNode::Kind::Map) {
    if(const auto *table=importer.second.get("fileIDToRecycleName")) {
      if(table->kind!=UnityYamlNode::Kind::Map)return fail("fileIDToRecycleName Unity inválido.");
      for(const auto &entry:table->members) {
        i64 id=0;const auto parsed=std::from_chars(entry.first.data(),entry.first.data()+entry.first.size(),id);
        if(parsed.ec!=std::errc{}||parsed.ptr!=entry.first.data()+entry.first.size()||entry.second.kind!=UnityYamlNode::Kind::Scalar)
          return fail("Sub-recurso .meta Unity inválido.");
        // The old fixed-ID encoding carries the class in the first digits;
        // generated 64-bit IDs have no such meaning and remain untyped.
        const u32 type=id>0&&id<100000000?static_cast<u32>(id/100000):0;
        if(!insert(id,type,entry.second.scalar))return fail("Sub-recurso Unity duplicado ou inválido.");
      }
    }
    if(const auto *table=importer.second.get("internalIDToNameTable")) {
      if(table->kind!=UnityYamlNode::Kind::Sequence)return fail("internalIDToNameTable Unity inválido.");
      for(const auto &entry:table->elements) {
        const auto *first=entry.get("first"),*second=entry.get("second");
        if(!first||first->kind!=UnityYamlNode::Kind::Map||first->members.size()!=1||!second||second->kind!=UnityYamlNode::Kind::Scalar)
          return fail("Identidade interna Unity inválida.");
        const auto &pair=first->members.front();u32 type=0;i64 id=0;
        const auto parsed=std::from_chars(pair.first.data(),pair.first.data()+pair.first.size(),type);
        if(parsed.ec!=std::errc{}||parsed.ptr!=pair.first.data()+pair.first.size()||!type||!pair.second.integer(id)||!insert(id,type,second->scalar))
          return fail("Identidade interna Unity duplicada ou inválida.");
      }
    }
  }
  out=std::move(candidate);diagnostic.clear();return true;
}
} // namespace ae::resources

#include "resources/library_index.h"
#include <algorithm>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>

namespace ae::resources {
namespace {
bool text(std::string_view value,usize maximum,bool required=false) {
  return value.size()<=maximum&&(!required||value.find_first_not_of(' ')!=value.npos)&&
    std::none_of(value.begin(),value.end(),[](unsigned char c){return c<32||c==127;});
}
bool hash(std::string_view value) {
  return value.size()==64&&std::all_of(value.begin(),value.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
bool recipe(std::string_view value) {
  return value.size()<=65536&&std::none_of(value.begin(),value.end(),[](unsigned char c){
    return (c<32&&c!='\n'&&c!='\r'&&c!='\t')||c==127;
  });
}
bool path(std::string_view value) {
  if(!text(value,4096,true)||value.front()=='/'||value.back()=='/'||value.find_first_of("\\:")!=value.npos) return false;
  while(!value.empty()) {const auto slash=value.find('/');const auto part=value.substr(0,slash);
    if(part.empty()||part=="."||part=="..") return false;
    if(slash==value.npos) break;
    value.remove_prefix(slash+1);
  }
  return true;
}
bool matches(LibraryItemKind kind,AssetType type) {
  switch(kind) {
    case LibraryItemKind::Model:return type==AssetType::Mesh;
    case LibraryItemKind::Clip:return type==AssetType::AnimationClip;
    case LibraryItemKind::Material:return type==AssetType::Material;
    case LibraryItemKind::Texture:return type==AssetType::Texture||type==AssetType::EnvironmentMap;
    case LibraryItemKind::Prefab:return type==AssetType::Prefab;
    case LibraryItemKind::Scene:return type==AssetType::Scene;
    case LibraryItemKind::Code:return type==AssetType::Script;
    case LibraryItemKind::Controller:return type==AssetType::AnimatorController;
    default:return false;
  }
}
}
bool LibraryIndex::valid() const {
  if(packages_.size()>MaximumPackages) return false;
  std::set<AssetGuid> packageIds,itemIds;usize total=0;
  for(const auto &package:packages_) {
    if(!package.id.valid()||!packageIds.insert(package.id).second||!text(package.name,128,true)||
       !text(package.version,128,true)||!text(package.origin,4096,true)||!hash(package.sourceHash)||!text(package.licenseNotice,4096,true)) return false;
    if(package.items.size()>MaximumItems||total>MaximumItems-package.items.size()) return false;
    total+=package.items.size();std::set<std::string> identities,paths;
    for(const auto &item:package.items) {
      if(!item.id.valid()||!itemIds.insert(item.id).second||static_cast<u32>(item.kind)>static_cast<u32>(LibraryItemKind::Controller)||
         static_cast<u32>(item.conversion)>static_cast<u32>(LibraryConversionState::Unsupported)||!path(item.path)||!paths.insert(item.path).second||
         !text(item.sourceIdentity,4096,true)||!identities.insert(item.sourceIdentity).second||!hash(item.sourceHash)||
         !text(item.converter,128)||!recipe(item.recipe)||!text(item.diagnostic,4096)||item.dependencies.size()>MaximumDependencies||item.knownLosses.size()>MaximumLosses) return false;
      if(item.conversion==LibraryConversionState::Converted&&(!item.nativeGuid.valid()||!item.converterVersion||item.converter.empty()||item.recipe.empty())) return false;
      if((item.conversion==LibraryConversionState::Unsupported||item.conversion==LibraryConversionState::Incomplete)&&item.diagnostic.empty()) return false;
      std::set<AssetGuid> dependencies;
      for(const auto id:item.dependencies) if(!id.valid()||id==item.id||!dependencies.insert(id).second) return false;
      for(const auto &loss:item.knownLosses) if(!text(loss,4096,true)) return false;
    }
  }
  return true;
}
bool LibraryIndex::add(LibraryPackage package) {
  if(this->package(package.id)) return false;
  auto candidate=*this;candidate.packages_.push_back(std::move(package));
  if(!candidate.valid()) return false;
  candidate.rebuildLocations();*this=std::move(candidate);return true;
}
bool LibraryIndex::replace(LibraryPackage package) {
  auto candidate=*this;auto found=std::find_if(candidate.packages_.begin(),candidate.packages_.end(),[&](const auto &entry){return entry.id==package.id;});
  if(found==candidate.packages_.end()) return false;
  *found=std::move(package);if(!candidate.valid()) return false;
  candidate.rebuildLocations();*this=std::move(candidate);return true;
}
const LibraryPackage *LibraryIndex::package(AssetGuid id) const {
  for(const auto &entry:packages_) if(entry.id==id) return &entry;
  return nullptr;
}
bool LibraryIndex::remove(AssetGuid id) {
  const auto found=std::find_if(packages_.begin(),packages_.end(),[&](const auto &entry){return entry.id==id;});
  if(found==packages_.end()) return false;
  packages_.erase(found);rebuildLocations();return true;
}
const LibraryItem *LibraryIndex::item(AssetGuid id) const {
  const auto found=itemLocations_.find(id);return found==itemLocations_.end()?nullptr:&packages_[found->second.first].items[found->second.second];
}
void LibraryIndex::rebuildLocations() {
  itemLocations_.clear();
  for(usize p=0;p<packages_.size();++p) for(usize i=0;i<packages_[p].items.size();++i)
    itemLocations_.emplace(packages_[p].items[i].id,std::pair{p,i});
}
LibraryResolution LibraryIndex::resolve(AssetGuid id,const AssetRegistry &registry) const {
  LibraryResolution result;const auto *entry=item(id);
  if(!entry) {result.state=LibraryResolutionState::Incomplete;result.missing=id;result.diagnostic="Item não pertence à biblioteca.";return result;}
  result.diagnostic=entry->diagnostic;
  if(entry->conversion==LibraryConversionState::Unsupported) {result.state=LibraryResolutionState::Unsupported;return result;}
  if(entry->conversion==LibraryConversionState::Incomplete) {result.state=LibraryResolutionState::Incomplete;return result;}
  if(entry->conversion==LibraryConversionState::Pending) return result;
  result.output=registry.find(entry->nativeGuid);
  if(!result.output||!matches(entry->kind,result.output->type)) {
    result.state=LibraryResolutionState::MissingOutput;result.missing=entry->nativeGuid;result.diagnostic="Saída nativa ausente ou de tipo incompatível.";return result;
  }
  // Bounded graph traversal protects transitive dependencies and cycles without
  // recursion or conflating this provenance graph with runtime ownership.
  std::vector<AssetGuid> pending;std::set<AssetGuid> visited{id};
  const auto enqueue=[&](const auto &dependencies) {
    for(const auto dependency:dependencies) if(visited.insert(dependency).second) {
      if(!item(dependency)) {result.state=LibraryResolutionState::MissingDependency;result.missing=dependency;
        result.diagnostic="Dependência de origem ausente do índice.";return false;}
      pending.push_back(dependency);
    }
    return true;
  };
  if(!enqueue(entry->dependencies)) return result;
  while(!pending.empty()) {
    const auto dependency=pending.back();pending.pop_back();
    const auto *source=item(dependency);const auto *native=source?registry.find(source->nativeGuid):nullptr;
    if(!source||source->conversion!=LibraryConversionState::Converted||!native||!matches(source->kind,native->type)) {
      result.state=LibraryResolutionState::MissingDependency;result.missing=dependency;result.diagnostic="Dependência de origem ainda sem saída nativa registrada.";return result;
    }
    if(!enqueue(source->dependencies)) return result;
  }
  result.state=LibraryResolutionState::Registered;return result;
}
std::string LibraryIndex::serialize() const {
  if(!valid()) return {};
  std::ostringstream out;out.imbue(std::locale::classic());out<<"ASTRA_LIBRARY_INDEX_1\nPACKAGES "<<packages_.size()<<'\n';
  for(const auto &package:packages_) {
    out<<"PACKAGE "<<package.id.text()<<' '<<std::quoted(package.name)<<' '<<std::quoted(package.version)<<' '<<std::quoted(package.origin)<<' '<<std::quoted(package.sourceHash)<<' '<<std::quoted(package.licenseNotice)<<"\nITEMS "<<package.items.size()<<'\n';
    for(const auto &item:package.items) {
      out<<"ITEM "<<item.id.text()<<' '<<static_cast<u32>(item.kind)<<' '<<static_cast<u32>(item.conversion)<<' '<<(item.nativeGuid.valid()?item.nativeGuid.text():"-")<<' '<<item.converterVersion<<' '<<std::quoted(item.path)<<' '<<std::quoted(item.sourceIdentity)<<' '<<std::quoted(item.sourceHash)<<' '<<std::quoted(item.converter)<<' '<<std::quoted(item.recipe)<<' '<<std::quoted(item.diagnostic)<<"\nDEPENDENCIES "<<item.dependencies.size();
      for(const auto dependency:item.dependencies) out<<' '<<dependency.text();
      out<<"\nLOSSES "<<item.knownLosses.size();for(const auto &loss:item.knownLosses) out<<' '<<std::quoted(loss);
      out<<"\nEND_ITEM\n";
      if(out.tellp()>static_cast<std::streamoff>(MaximumBytes)) return {};
    }
    out<<"END_PACKAGE\n";
  }
  out<<"END_LIBRARY\n";auto value=out.str();return value.size()<=MaximumBytes?value:std::string();
}
bool LibraryIndex::deserialize(std::string_view source,LibraryIndex &out,std::string &diagnostic) {
  diagnostic.clear();const auto fail=[&](const char *why){diagnostic=why;return false;};
  if(source.size()>MaximumBytes||source.find('\0')!=source.npos) return fail("Índice de biblioteca excede o limite ou contém dados binários.");
  std::istringstream in{std::string(source)};in.imbue(std::locale::classic());std::string word;
  const auto expect=[&](const char *expected){return static_cast<bool>(in>>word)&&word==expected;};
  const auto guid=[&](AssetGuid &id,bool optional=false){std::string value;if(!(in>>value)) return false;if(optional&&value=="-") {id={};return true;}return AssetGuid::parse(value,id)&&id.valid();};
  u32 count=0;if(!expect("ASTRA_LIBRARY_INDEX_1")||!expect("PACKAGES")||!(in>>count)||count>MaximumPackages) return fail("Cabeçalho ou quantidade de pacotes inválida.");
  LibraryIndex candidate;usize total=0;
  for(u32 p=0;p<count;++p) {
    LibraryPackage package;u32 items=0;
    if(!expect("PACKAGE")||!guid(package.id)||!(in>>std::quoted(package.name)>>std::quoted(package.version)>>std::quoted(package.origin)>>std::quoted(package.sourceHash)>>std::quoted(package.licenseNotice))||!expect("ITEMS")||!(in>>items)||items>MaximumItems||total>MaximumItems-items) return fail("Metadados ou quantidade de itens inválida.");
    total+=items;
    for(u32 i=0;i<items;++i) {
      LibraryItem item;u32 kind=0,state=0,dependencies=0,losses=0;
      if(!expect("ITEM")||!guid(item.id)||!(in>>kind>>state)||kind>static_cast<u32>(LibraryItemKind::Controller)||state>static_cast<u32>(LibraryConversionState::Unsupported)||!guid(item.nativeGuid,true)||!(in>>item.converterVersion>>std::quoted(item.path)>>std::quoted(item.sourceIdentity)>>std::quoted(item.sourceHash)>>std::quoted(item.converter)>>std::quoted(item.recipe)>>std::quoted(item.diagnostic))) return fail("Item ou receita inválida.");
      item.kind=static_cast<LibraryItemKind>(kind);item.conversion=static_cast<LibraryConversionState>(state);
      if(!expect("DEPENDENCIES")||!(in>>dependencies)||dependencies>MaximumDependencies) return fail("Quantidade de dependências inválida.");
      for(u32 d=0;d<dependencies;++d) {AssetGuid dependency;if(!guid(dependency)) return fail("Identidade de dependência inválida.");item.dependencies.push_back(dependency);}
      if(!expect("LOSSES")||!(in>>losses)||losses>MaximumLosses) return fail("Quantidade de perdas inválida.");
      for(u32 l=0;l<losses;++l) {std::string loss;if(!(in>>std::quoted(loss))) return fail("Registro de perda inválido.");item.knownLosses.push_back(std::move(loss));}
      if(!expect("END_ITEM")) return fail("Item incompleto.");
      package.items.push_back(std::move(item));
    }
    if(!expect("END_PACKAGE")) return fail("Pacote incompleto.");
    candidate.packages_.push_back(std::move(package));
  }
  if(!expect("END_LIBRARY")||(in>>word)||!candidate.valid()) return fail("Índice inconsistente, duplicado ou com conteúdo adicional.");
  candidate.rebuildLocations();out=std::move(candidate);return true;
}
} // namespace ae::resources

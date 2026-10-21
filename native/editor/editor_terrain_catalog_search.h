#pragma once
#include "editor/editor_screen.h"
#include "editor/editor_terrain_noise_controls.h"
#include "editor/editor_global_search.h"
#include <limits>
#include <cctype>
namespace ae::editor {
inline constexpr u32 terrainCatalogActionBase=0xEC000F00u;
inline EditorWidget terrainCatalogAction(u32 action) {return EditorWidget(terrainCatalogActionBase+action);}
inline bool terrainCatalogMaterialScope(u32 view) {return view==1 || (view>=80 && view<=82);}
inline bool terrainCatalogSearchView(u32 view) {
  return terrainCatalogMaterialScope(view) || view==190 || terrainNoisePicker(view) ||
    (view>=90 && view<=92) || (view>=220 && view<=227) || view==150 || view==151 || (view>=180 && view<=183) || terrainFlowPicker(view);
}
// A new picker starts from an unequivocal authored role. Saved context,
// including an explicit All selection, always wins over this default.
inline u32 terrainCatalogDefaultRole(u32 view,u32 mapRole) {
  if(terrainFlowPicker(view))return terrainFlowRain(view)?10:terrainFlowLava(view)?9:8;
  if((view==190 || (view>=90 && view<=92)) && mapRole==1)return 2;
  if(view==151 || view==181 || view==222 || view==225)return 2;
  if(terrainNoisePicker(view) && terrainNoiseNormal(view))return 2;
  return 0;
}
inline u32 terrainCatalogContextKey(u32 view,u32 mapRole) {
  const bool roleScoped=view==190 || (view>=90 && view<=92) || view==182;
  return view*8+(roleScoped?std::min(mapRole,6u):0);
}
inline usize terrainCatalogCount(const EditorScreenState &,bool);
struct TerrainCatalogContextGuard {
  EditorScreenState &state;EditorScreenState::TerrainCatalogSearchContext previous;
  explicit TerrainCatalogContextGuard(EditorScreenState &s):state(s),previous{s.terrainMaterialView,
    terrainCatalogContextKey(s.terrainMaterialView,s.terrainMapRole),s.terrainCatalogPage,
    s.terrainCatalogRoleFilter,s.terrainCatalogQuery,s.terrainCatalogPackageFilter}{}
  ~TerrainCatalogContextGuard() {
    const auto nextKey=terrainCatalogContextKey(state.terrainMaterialView,state.terrainMapRole);
    if(previous.key==nextKey)return;
    auto &contexts=state.terrainCatalogSearchContexts;
    if(terrainCatalogSearchView(previous.view)) {
      const auto found=std::find_if(contexts.begin(),contexts.end(),[&](const auto &c){return c.key==previous.key;});
      if(found==contexts.end())contexts.push_back(previous);else *found=previous;
    }
    if(!terrainCatalogSearchView(state.terrainMaterialView))return;
    const auto found=std::find_if(contexts.begin(),contexts.end(),[&](const auto &c){return c.key==nextKey;});
    state.terrainCatalogPage=found==contexts.end()?0:found->page;
    state.terrainCatalogRoleFilter=found==contexts.end()?terrainCatalogDefaultRole(state.terrainMaterialView,state.terrainMapRole):found->role;
    state.terrainCatalogQuery=found==contexts.end()?std::string{}:found->query;
    state.terrainCatalogPackageFilter=found==contexts.end()?std::string{}:found->package;
    const auto count=terrainCatalogCount(state,terrainCatalogMaterialScope(state.terrainMaterialView));
    state.terrainCatalogPage=std::min(state.terrainCatalogPage,count?u32((count-1)/6):0u);
  }
};
inline std::string terrainCatalogSignature(const EditorScreenState &s) {
  return s.terrainCatalogQuery+"\n"+s.terrainCatalogPackageFilter+"\n"+std::to_string(s.terrainCatalogRoleFilter)+"\n"+std::to_string(s.terrainMapRole)+"\n"+std::to_string(s.terrainBrush.layer);
}
inline std::string terrainCatalogPackageName(std::string package) {
  if(const auto end=package.find(" - ");end!=std::string::npos)package.resize(end);
  if(const auto end=package.find("_v");end!=std::string::npos)package.resize(end);
  if(const auto end=package.find(" v");end!=std::string::npos)package.resize(end);
  if(package.ends_with(".unitypackage"))package.resize(package.size()-13);
  return package;
}
inline std::vector<std::string> terrainCatalogPackages(const EditorScreenState &s) {
  std::vector<std::string> result;
  if(s.terrainCatalog)for(const auto &m:s.terrainCatalog->materials)
    if(!m.package.empty() && std::find(result.begin(),result.end(),m.package)==result.end())result.push_back(m.package);
  if(s.terrainCatalog)for(const auto &texture:s.terrainCatalog->textures)
    if(!texture.package.empty() && std::find(result.begin(),result.end(),texture.package)==result.end())result.push_back(texture.package);
  std::sort(result.begin(),result.end());return result;
}
inline std::vector<u32> terrainCatalogComputeIndices(const EditorScreenState &s,bool material) {
  std::vector<u32> result;if(!s.terrainCatalog)return result;
  const auto &c=*s.terrainCatalog;
  std::vector<std::string> words;std::string word;
  for(const unsigned char c:s.terrainCatalogQuery) {
    if(std::isspace(c)) {if(!word.empty()){words.push_back(editorSearchKey(word));word.clear();}}
    else word+=char(c);
  }
  if(!word.empty())words.push_back(editorSearchKey(word));
  const auto matches=[&](std::string text){u8 rank=0;return editorSearchWords(editorSearchKey(text),words,rank);};
  const auto role=std::min(s.terrainCatalogRoleFilter,10u);
  const auto materialAllowed=[&](const resources::TerrainCatalogMaterial &m){return
    (s.terrainCatalogPackageFilter.empty() || m.package==s.terrainCatalogPackageFilter) &&
    (!role || (role<=7 && (m.layer.maps[role-1].texture.valid() ||
      std::any_of(m.layer.cluster.variants.begin(),m.layer.cluster.variants.end(),[&](const auto &v){return v.maps[role-1].texture.valid();}))));};
  if(material) {
    for(u32 i=0;i<c.materials.size();++i){const auto &m=c.materials[i];
      if(materialAllowed(m) && matches(m.id+" "+m.layer.name+" "+m.package+" "+m.sourcePath))result.push_back(i);}
  } else {
    for(u32 i=0;i<c.textures.size();++i){const auto &t=c.textures[i];
      if(!matches(t.id+" "+t.originalAssetPath+" "+t.sourcePath+" "+t.identity.text()+" "+t.package))continue;
      if(!s.terrainCatalogPackageFilter.empty() && t.package!=s.terrainCatalogPackageFilter)continue;
      bool allowed=!role || (t.roleMask&(1u<<(role-1)));
      // Legacy textures lack source_roles. Derive only their actual authored
      // map roles; never infer role/packing or origin from file names.
      if(!allowed && t.roleMask==0 && role<=7)for(const auto &m:c.materials){
        allowed=m.layer.maps[role-1].texture==t.identity;
        for(const auto &v:m.layer.cluster.variants)allowed=allowed || v.maps[role-1].texture==t.identity;
        if(allowed)break;
      }
      if(allowed)result.push_back(i);
    }
  }
  return result;
}
inline const std::vector<u32> &terrainCatalogIndices(const EditorScreenState &s,bool material) {
  const auto signature=terrainCatalogSignature(s);
  if(s.terrainCatalogFilterSource!=s.terrainCatalog || s.terrainCatalogFilterSignature!=signature) {
    s.terrainCatalogMaterialIndices=terrainCatalogComputeIndices(s,true);
    s.terrainCatalogTextureIndices=terrainCatalogComputeIndices(s,false);
    s.terrainCatalogFilterSource=s.terrainCatalog;s.terrainCatalogFilterSignature=signature;
  }
  return material?s.terrainCatalogMaterialIndices:s.terrainCatalogTextureIndices;
}
template<class T> struct TerrainCatalogEntries {
  const std::vector<T> *source=nullptr;std::vector<u32> indices;
  usize size()const{return indices.size();}bool empty()const{return indices.empty();}
  const T &operator[](usize index)const{return (*source)[indices[index]];}
};
struct TerrainCatalogResults {
  TerrainCatalogEntries<resources::TerrainCatalogMaterial> materials;
  TerrainCatalogEntries<resources::TerrainCatalogTexture> textures;
};
inline std::shared_ptr<const TerrainCatalogResults> terrainFilteredCatalog(const EditorScreenState &s) {
  if(!s.terrainCatalog)return {};
  auto result=std::make_shared<TerrainCatalogResults>();
  result->materials={&s.terrainCatalog->materials,terrainCatalogIndices(s,true)};
  result->textures={&s.terrainCatalog->textures,terrainCatalogIndices(s,false)};
  return result;
}
inline usize terrainCatalogCount(const EditorScreenState &s,bool material) {return terrainCatalogIndices(s,material).size();}
// Drawn identities guard against stale taps after query/page/catalog changes.
inline void terrainCatalogRecordVisible(const EditorScreenState &s) {
  s.terrainCatalogVisibleIds={};s.terrainCatalogVisibleView=s.terrainMaterialView;
  s.terrainCatalogVisiblePage=s.terrainCatalogPage;s.terrainCatalogVisibleSignature=terrainCatalogSignature(s);
  if(!s.terrainCatalog)return;
  const bool material=terrainCatalogMaterialScope(s.terrainMaterialView);
  const auto indices=terrainCatalogIndices(s,material);
  for(u32 slot=0;slot<6 && usize(s.terrainCatalogPage)*6+slot<indices.size();++slot) {
    auto index=indices[usize(s.terrainCatalogPage)*6+slot];
    s.terrainCatalogVisibleIds[slot]=material?s.terrainCatalog->materials[index].id:s.terrainCatalog->textures[index].identity.text();
  }
}
inline u32 terrainCatalogResolve(const EditorScreenState &s,u32 slot,bool material) {
  constexpr auto invalid=std::numeric_limits<u32>::max();
  if(slot>=6 || !s.terrainCatalog || s.terrainCatalogVisibleView!=s.terrainMaterialView ||
    s.terrainCatalogVisiblePage!=s.terrainCatalogPage || s.terrainCatalogVisibleSignature!=terrainCatalogSignature(s))return invalid;
  const auto indices=terrainCatalogIndices(s,material);const auto row=usize(s.terrainCatalogPage)*6+slot;
  if(row>=indices.size())return invalid;
  const auto index=indices[row];const auto id=material?s.terrainCatalog->materials[index].id:s.terrainCatalog->textures[index].identity.text();
  return id==s.terrainCatalogVisibleIds[slot]?index:invalid;
}
}

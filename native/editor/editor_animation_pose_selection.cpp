#include "editor/editor_animation_pose_selection.h"
#include "resources/animation_binding_path.h"
#include "ui/ui_text.h"
#include <algorithm>
#include <iomanip>
#include <set>
#include <map>
#include <sstream>

namespace ae::editor {
namespace {
bool validUtf8(std::string_view text) {
  for(usize cursor=0;cursor<text.size();) {
    const auto start=cursor;
    if(ui::nextUiCodepoint(text,cursor)==0xfffd&&text.substr(start,cursor-start)!="\xef\xbf\xbd")return false;
  }
  return true;
}
bool validPath(std::string_view path) {
  if(path.size()>1024||!validUtf8(path)||(!path.empty()&&(path.front()=='/'||path.back()=='/')))return false;
  while(!path.empty()) {
    const auto end=path.find('/');std::string decoded;
    if(!resources::animationBindingName(path.substr(0,end),decoded))return false;
    if(end==path.npos)break;
    path.remove_prefix(end+1);
  }
  return true;
}
bool validSets(std::span<const AnimationPoseSelectionSet> sets) {
  if(sets.size()>MaximumAnimationPoseSets)return false;
  std::set<std::pair<resources::AssetGuid,std::string>> names;
  std::map<resources::AssetGuid,usize> clipCounts;
  for(const auto &set:sets) {
    if(!set.clip.valid()||!validUtf8(set.name)||set.name.find_first_not_of(' ')==set.name.npos||set.name.size()>96||
       std::any_of(set.name.begin(),set.name.end(),[](unsigned char c){return c<32||c==127;})||
       !names.insert({set.clip,set.name}).second||++clipCounts[set.clip]>MaximumAnimationPoseSetsPerClip||set.paths.empty()||set.paths.size()>MaximumAnimationPoseSelection)return false;
    std::set<std::string> paths;
    for(const auto &path:set.paths)if(!validPath(path)||!paths.insert(path).second)return false;
  }
  return true;
}
}
bool animationPoseTargetPath(const runtime::SceneGraph &scene,runtime::ObjectId owner,runtime::ObjectId target,std::string &out,std::string &error) {
  error.clear();if(!scene.exists(target)||!scene.exists(owner)||(target!=owner&&!scene.isDescendantOf(target,owner))) {error="Objeto deve pertencer à raiz do clipe";return false;}
  std::string result;
  for(auto current=target;current!=owner;) {
    const auto *node=scene.find(current);if(!node||!node->parent) {error="Caminho do objeto indisponível";return false;}
    u32 matches=0;for(const auto sibling:scene.childrenOf(node->parent))if(const auto *other=scene.find(sibling);other&&std::string_view(other->name)==node->name)++matches;
    if(matches!=1) {error="Nomes iguais entre irmãos tornam o binding ambíguo; renomeie o objeto";return false;}
    const auto segment=resources::animationBindingSegment(node->name);result=segment+(result.empty()?"":"/"+result);current=node->parent;
  }
  if(!validPath(result)) {error="Caminho de seleção excede o contrato de binding";return false;}
  out=std::move(result);return true;
}
bool resolveAnimationPoseTargetPath(const runtime::SceneGraph &scene,runtime::ObjectId owner,std::string_view path,runtime::ObjectId &out,std::string &error) {
  error.clear();if(!scene.exists(owner)||!validPath(path)) {error="Raiz ou caminho de seleção inválido";return false;}
  auto current=owner;
  while(!path.empty()) {
    const auto end=path.find('/');std::string name;resources::animationBindingName(path.substr(0,end),name);
    runtime::ObjectId child=0;
    for(const auto id:scene.childrenOf(current))if(const auto *node=scene.find(id);node&&std::string_view(node->name)==name) {
      if(child) {error="Seleção salva possui caminho ambíguo: "+std::string(path);return false;}child=id;
    }
    if(!child) {error="Alvo da seleção salva ausente: "+std::string(path);return false;}
    current=child;if(end==path.npos)break;path.remove_prefix(end+1);
  }
  out=current;return true;
}
std::vector<runtime::ObjectId> animationPoseSelectionRoots(const runtime::SceneGraph &scene,std::span<const runtime::ObjectId> selection) {
  std::vector<runtime::ObjectId> roots;
  for(const auto id:selection) {
    if(!scene.exists(id))continue;
    bool nested=false;for(const auto ancestor:selection)if(id!=ancestor&&scene.isDescendantOf(id,ancestor)) {nested=true;break;}
    if(!nested&&std::find(roots.begin(),roots.end(),id)==roots.end())roots.push_back(id);
  }
  return roots;
}
std::string serializeAnimationPoseSets(std::span<const AnimationPoseSelectionSet> sets) {
  if(!validSets(sets))return {};
  std::ostringstream out;out<<"ASTRA_ANIMATION_SELECTION_1 "<<sets.size()<<'\n';
  for(const auto &set:sets) {
    out<<set.clip.text()<<' '<<std::quoted(set.name)<<' '<<set.paths.size()<<'\n';
    for(const auto &path:set.paths)out<<std::quoted(path)<<'\n';
  }
  auto result=out.str();return result.size()<=1024*1024?result:std::string{};
}
bool deserializeAnimationPoseSets(std::string_view text,std::vector<AnimationPoseSelectionSet> &out,std::string &error) {
  error.clear();if(text.size()>1024*1024) {error="Seleções excedem 1 MiB";return false;}
  std::istringstream in{std::string(text)};std::string magic;usize count=0;
  if(!(in>>magic>>count)||magic!="ASTRA_ANIMATION_SELECTION_1"||count>MaximumAnimationPoseSets) {error="Cabeçalho de seleções inválido";return false;}
  std::vector<AnimationPoseSelectionSet> candidate;
  for(usize i=0;i<count;++i) {
    AnimationPoseSelectionSet set;std::string guid;usize paths=0;
    if(!(in>>guid>>std::quoted(set.name)>>paths)||!resources::AssetGuid::parse(guid,set.clip)||!paths||paths>MaximumAnimationPoseSelection) {error="Conjunto de seleção inválido";return false;}
    for(usize p=0;p<paths;++p) {std::string path;if(!(in>>std::quoted(path))) {error="Caminho de seleção truncado";return false;}set.paths.push_back(std::move(path));}
    candidate.push_back(std::move(set));
  }
  in>>std::ws;if(!in.eof()||!validSets(candidate)) {error="Seleções inválidas, duplicadas ou dados excedentes";return false;}
  out=std::move(candidate);return true;
}
}

#include "editor/editor_fbx_rig_binding.h"
#include "editor/editor_import_reconcile.h"
#include "scene/import_link.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace ae::editor {
bool validateFbxClipRig(const EditorDocument &document,const EditorMapScene &mapScene,EditorEntityId owner,
    resources::AssetGuid sourceGuid,const resources::ImportNodeMap &source,const resources::AnimationClip &clip,u32 sourceRoot,
    std::span<const resources::MorphTargetSet *const> sourceMorphs,std::vector<EditorEntityId> &targets,std::string &diagnostic) {
  targets.clear();diagnostic.clear();
  const auto fail=[&](std::string message){targets.clear();diagnostic=std::move(message);return false;};
  if(!document.exists(owner)||sourceRoot>=source.nodes.size()||clip.channels.empty()||!source.valid())return fail("Selecione raiz de fonte, clipe e raiz da cena válidos.");
  std::map<resources::AssetGuid,u32> indexOf;
  for(u32 index=0;index<source.nodes.size();++index)indexOf.emplace(source.nodes[index].id,index);
  std::vector<i32> parents(source.nodes.size(),-1);
  for(u32 index=0;index<source.nodes.size();++index)if(source.nodes[index].parent.valid()) {
    const auto found=indexOf.find(source.nodes[index].parent);
    if(found==indexOf.end()||found->second>=index)return fail("Mapa de rig não possui pais válidos antes dos filhos.");
    parents[index]=static_cast<i32>(found->second);
  }
  std::vector<u8> used(source.nodes.size());bool rootAnimated=false;
  for(const auto &channel:clip.channels) {
    if(channel.node>=source.nodes.size())return fail("Clipe referencia nó ausente no rig de origem.");
    if(channel.node==sourceRoot&&channel.path!=resources::AnimationPath::Weights)rootAnimated=true;
    u32 node=channel.node;usize depth=0;
    for(;;) {
      if(++depth>source.nodes.size())return fail("Ciclo na hierarquia do rig de origem.");
      used[node]=1;if(node==sourceRoot)break;
      const auto parent=parents[node];
      if(parent<0||static_cast<usize>(parent)>=source.nodes.size())return fail("A raiz de origem não contém todos os canais do clipe.");
      node=static_cast<u32>(parent);
    }
  }
  targets.resize(source.nodes.size());targets[sourceRoot]=owner;
  std::vector<EditorEntityId> subtree;document.collectSubtree(owner,subtree);
  std::set<EditorEntityId> assigned{owner};
  const auto resolve=[&](auto &&self,u32 index)->bool {
    if(targets[index])return true;
    const auto parent=parents[index];
    if(parent<0||static_cast<usize>(parent)>=source.nodes.size()||!self(self,static_cast<u32>(parent)))return false;
    EditorEntityId match=0;
    for(const auto id:subtree)if(const auto *object=document.find(id)) {
      const auto *link=scene::importLink(object->components);
      if(link&&!link->orphan&&!link->unlinked&&link->primitive<0&&link->source==sourceGuid&&link->node==source.nodes[index].id) {
        if(match)return false;
        match=id;
      }
    }
    if(!match) {
      const auto name=importEntityName(source.nodes[index].name);
      for(const auto id:document.childrenOf(targets[static_cast<usize>(parent)]))if(const auto *object=document.find(id);object&&name==object->name) {
        if(match)return false;
        match=id;
      }
    }
    const auto *object=document.find(match);
    if(!object||object->parent!=targets[static_cast<usize>(parent)]||!assigned.insert(match).second)return false;
    targets[index]=match;return true;
  };
  for(u32 index=0;index<used.size();++index)if(used[index]&&!resolve(resolve,index))
    return fail("Hierarquia ausente ou ambígua para o nó: "+source.nodes[index].name);
  for(u32 index=0;index<used.size();++index)if(used[index]) {
    const auto *object=document.find(targets[index]);auto rest=object->transform;
    if(const auto *link=scene::importLink(object->components);link&&!link->orphan&&!link->unlinked&&link->primitive<0) {
      std::copy_n(link->basePosition,3,rest.position);std::copy_n(link->baseRotation,3,rest.rotationDegrees);std::copy_n(link->baseScale,3,rest.scale);
    }
    float matrix[16];runtime::transformMatrix(rest,matrix);
    for(u32 c=0;c<4;++c)for(u32 r=0;r<3;++r) {
      // Translation of an unanimated owner may differ. Orientation and scale
      // must agree; descendant bind transforms never get this exemption.
      if(index==sourceRoot&&!rootAnimated&&c==3)continue;
      const float expected=source.nodes[index].localMatrix[c*4+r],actual=matrix[c*4+r];
      if(!std::isfinite(expected)||!std::isfinite(actual)||std::abs(expected-actual)>1e-4f*std::max(1.f,std::abs(expected)))
        return fail("Pose de referência incompatível no nó: "+source.nodes[index].name+". Retargeting é uma operação distinta.");
    }
  }
  for(const auto &channel:clip.channels)if(channel.path==resources::AnimationPath::Weights) {
    const auto *expected=channel.node<sourceMorphs.size()?sourceMorphs[channel.node]:nullptr;
    const auto *object=document.find(targets[channel.node]);const auto *renderer=runtime::meshRenderer(*object);
    const auto *actual=renderer&&renderer->mesh?mapScene.drawMorph(renderer->mesh-1):nullptr;
    if(!expected||!actual||expected->targetCount!=channel.weightCount||actual->targetCount!=channel.weightCount||expected->names!=actual->names)
      return fail("Alvos morph incompatíveis; quantidade e ordem dos nomes precisam corresponder ao recurso real.");
  }
  return true;
}
}

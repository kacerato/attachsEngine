#include "editor/editor_gui_tree.h"
#include "scene/ui_canvas.h"
#include <unordered_set>

namespace ae::editor {
const EditorGuiRow *EditorGuiTree::find(u32 token) const {
  for(const auto &row:rows_)if(row.token==token)return &row;
  return nullptr;
}
void EditorGuiTree::rebuild(const runtime::SceneGraph &graph,const resources::AssetRegistry &assets,
                          const Loader &load,std::string_view editedPath,const ui::GuiDocument &edited) {
  bool sceneChanged=graph_!=&graph || graphRevision_!=graph.revision();
  if(sceneChanged) {
    graph_=&graph;graphRevision_=graph.revision();scopes_.clear();
    std::vector<runtime::ObjectId> objects;graph.collectSubtree(graph.root(),objects);
    for(const auto owner:objects)if(const auto *object=graph.find(owner))for(usize i=0;i<object->components.size();++i) {
      const auto *value=object->components.at(i);
      if(value&&value->type().id==scene::UiCanvas::descriptor.id)scopes_.push_back({owner,value->instanceId(),static_cast<const scene::UiCanvas&>(*value).document,0});
    }
  }
  std::string stamp;
  for(const auto &scope:scopes_) {
    stamp+=scope.document.text()+":";
    if(const auto *record=assets.find(scope.document))stamp+=record->path+":"+record->contentHash+":"+std::to_string(static_cast<u32>(record->type));
    else stamp+="missing";
    stamp+=';';
  }
  if(!sceneChanged&&stamp_==stamp&&editedPath_==editedPath&&editedRevision_==edited.revision())return;
  stamp_=std::move(stamp);editedPath_=editedPath;editedRevision_=edited.revision();
  rows_.clear();diagnostic_.clear();std::unordered_set<resources::AssetGuid,resources::AssetGuidHash> used;
  auto append=[&](EditorGuiRow row) {
    if(!tokens_.contains(row.target)&&nextToken_>=0x00100000u){diagnostic_="Limite de identidades da arvore UI excedido; reabra o projeto";return;}
    auto [it,inserted]=tokens_.try_emplace(row.target,0);
    if(inserted)it->second=nextToken_++;
    row.token=it->second;rows_.push_back(std::move(row));
  };
  u32 canvases=0;
  for(const auto &scope:scopes_) {
      if(++canvases>64) {
        diagnostic_="Limite de 64 Canvas UI excedido";
        append({scope,0,0,0,ui::GuiKind::Panel,"Canvas #"+std::to_string(scope.component),{},diagnostic_});break;
      }
      const auto *record=assets.find(scope.document);const ui::GuiDocument *doc=nullptr;
      std::string error,path=record?record->path:"";
      if(!record || record->type!=resources::AssetType::UiDocument)error="Documento UI ausente ou invalido";
      else if(path==editedPath)doc=&edited;
      else {
        used.insert(record->guid);const auto stamp=record->path+":"+record->contentHash;
        auto [it,fresh]=sources_.try_emplace(record->guid);auto &source=it->second;
        if(fresh || source.stamp!=stamp) {
          source=Source{};source.stamp=stamp;
          if(!load(record->guid,source.document,source.error)&&source.error.empty())source.error="Falha ao carregar UI";
        }
        error=source.error;if(error.empty())doc=&source.document;
      }
      append({scope,0,0,0,ui::GuiKind::Panel,"Canvas #"+std::to_string(scope.component),path,error});
      if(!doc)continue;
      // Walk actual parent relations: sibling order is the document order,
      // including after duplicate/reorder. No independent hierarchy is stored.
      struct Frame {ui::GuiId parent;u32 depth;};std::vector<Frame> stack{{0,1}};
      std::unordered_map<ui::GuiId,std::vector<ui::GuiId>> children;
      for(const auto &node:doc->nodes())children[node.parent].push_back(node.id);
      while(!stack.empty()) {
        const auto frame=stack.back();stack.pop_back();
        if(frame.depth>ui::GuiDocument::kMaximumNodes+1)continue;
        if(frame.parent) {
          const auto *node=doc->find(frame.parent);if(!node)continue;
          auto target=scope;target.node=node->id;
          append({target,node->parent,frame.depth-1,0,node->kind,node->name,path,{}});
        }
        if(const auto at=children.find(frame.parent);at!=children.end())
          for(auto n=at->second.rbegin();n!=at->second.rend();++n)stack.push_back({*n,frame.depth+1});
      }
    }
  for(auto it=sources_.begin();it!=sources_.end();)if(!used.contains(it->first))it=sources_.erase(it);else ++it;
}
}

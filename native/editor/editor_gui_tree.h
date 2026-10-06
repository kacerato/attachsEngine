#pragma once
#include "runtime/scene_graph.h"
#include "resources/asset_registry.h"
#include "ui/gui_document.h"
#include <functional>
#include <map>
#include <tuple>

namespace ae::editor {
// Authoring identity. A UI node is never a SceneGraph entity or a runtime lease.
struct EditorGuiTarget {
  runtime::ObjectId owner=0;
  u64 component=0;
  resources::AssetGuid document{};
  ui::GuiId node=0; // zero selects the Canvas document root
  bool valid() const {return owner && component;}
  friend bool operator==(const EditorGuiTarget &,const EditorGuiTarget &)=default;
  friend bool operator<(const EditorGuiTarget &a,const EditorGuiTarget &b) {
    return std::tie(a.owner,a.component,a.document.high,a.document.low,a.node)<
           std::tie(b.owner,b.component,b.document.high,b.document.low,b.node);
  }
};
struct EditorGuiRow {
  EditorGuiTarget target;
  ui::GuiId parent=0;
  u32 depth=0,token=0;
  ui::GuiKind kind=ui::GuiKind::Panel;
  std::string name,path,error;
};
// Read-only projection. Cached files are immutable; the workbench owns the only
// editable document. Shared instances always project that same authoring source.
class EditorGuiTree {
public:
  using Loader=std::function<bool(resources::AssetGuid,ui::GuiDocument &,std::string &)>;
  void rebuild(const runtime::SceneGraph &,const resources::AssetRegistry &,const Loader &,
               std::string_view editedPath,const ui::GuiDocument &edited);
  std::span<const EditorGuiRow> rows() const {return rows_;}
  const EditorGuiRow *find(u32 token) const;
  const std::string &diagnostic() const {return diagnostic_;}
  void clear(){rows_.clear();sources_.clear();tokens_.clear();nextToken_=1;graph_=nullptr;scopes_.clear();stamp_.clear();diagnostic_.clear();}
private:
  struct Source {std::string stamp,error;ui::GuiDocument document;};
  std::map<resources::AssetGuid,Source> sources_;
  std::map<EditorGuiTarget,u32> tokens_;
  std::vector<EditorGuiRow> rows_;
  u32 nextToken_=1;
  const runtime::SceneGraph *graph_=nullptr;
  u64 graphRevision_=~u64{0},editedRevision_=~u64{0};
  std::vector<EditorGuiTarget> scopes_;
  std::string stamp_,editedPath_,diagnostic_;
};
}

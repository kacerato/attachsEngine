#pragma once
#include "ui/gui_document.h"
#include "ui/immediate_gui.h"
#include <functional>

namespace ae::ui {
class GuiWorkbench final {
public:
  GuiWorkbench();
  using Tool = std::function<void(GuiWorkbench &)>;
  bool addTool(std::string name,Tool draw);
  using Storage = std::function<bool(GuiDocument &,std::string_view,bool,std::string &)>;
  void draw(const UiRect &area,const UiRect &surface,UiDrawList &list,float deltaSeconds=1.0f/60.0f);
  bool pointer(const UiPointerEvent &event);
  void cancelPointers();
  void text(std::string_view text) { immediate_.text(text); }
  void key(int imguiKey,bool down) { immediate_.key(imguiKey,down); }
  void wheel(float x,float y) { immediate_.wheel(x,y); }
  bool wantsKeyboard() const { return immediate_.wantsKeyboard(); }
  void setStorage(Storage storage) { storage_=std::move(storage); }
  void setImages(const GuiImageAtlas *images) {images_=images;authorView_.setImages(images);preview_.setImages(images);}
  void setImageChoices(std::function<std::vector<std::string>()> choices) {imageChoices_=std::move(choices);}
  GuiDocument &document() { return document_; }
  GuiHistory &history() { return history_; }
  GuiRuntime &preview() { return preview_; }
  ImmediateGui &immediate() { return immediate_; }
  const UiRect &canvas() const { return canvas_; }
  GuiId selected() const { return selection_; }
  void select(GuiId id) { selection_=document_.find(id)?id:0; }
  void setPreview(bool enabled);
  void setDiagnostic(std::string text) { diagnostic_=std::move(text); }
  bool isPreview() const { return previewing_; }
  bool save();
  bool setResource(std::string_view path);
private:
  void tree();
  void properties();
  void canvasProperties();
  GuiDocument document_, dragBefore_;
  GuiHistory history_;
  GuiRuntime authorView_, preview_;
  ImmediateGui immediate_;
  UiDrawList canvasDrawing_;
  UiDrawList toolbarDrawing_;
  UiDrawList propertyIconDrawing_;
  UiDrawList behaviorIconDrawing_[2];
  Storage storage_;
  UiRect area_{},canvas_{};
  GuiId selection_=0;
  GuiId propertiesSelection_=0;
  GuiNode dragNode_{};
  UiPoint dragStart_{};
  bool previewing_=false, captured_=false, canvasPointer_=false;
  u32 pointerId_=0;
  int compactRoute_=0;
  int actionSelection_=0,stateSelection_=0;
  char resource_[256]="UI/main.aeui";
  std::string diagnostic_, lastEvent_;
  struct ToolEntry {std::string name;Tool draw;};
  std::vector<ToolEntry> tools_;
  u32 activeTool_=0;
  bool drawingTool_=false;
  bool canvasSettings_=false,dragEditable_=false;
  const GuiImageAtlas *images_=nullptr;
  std::function<std::vector<std::string>()> imageChoices_;
  std::vector<std::string> imagePaths_;
  char imageQuery_[128]{};
};
} // namespace ae::ui

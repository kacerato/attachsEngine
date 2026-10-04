#pragma once
#include "ui/ui_draw_list.h"
#include "ui/ui_input.h"
#include <span>
#include <vector>
struct ImGuiContext;
struct ImGuiInputTextCallbackData;
namespace ae::ui {
// Owns a real Dear ImGui context. Call begin -> regular ImGui API -> end.
// No global singleton ownership, filesystem .ini or secondary platform windows.
class ImmediateGui final {
public:
  ImmediateGui();
  ~ImmediateGui();
  ImmediateGui(const ImmediateGui &) = delete;
  ImmediateGui &operator=(const ImmediateGui &) = delete;
  void begin(float width, float height, float deltaSeconds);
  bool setFont(std::span<const u8> ttf,float pixels=17);
  void end(UiDrawList &list);
  // Insert native engine drawing at the current ImGui draw order. The source
  // must remain alive through end(). Popups and clipping keep their real order.
  void insert(const UiDrawList &nativeList);
  void pointer(const UiPointerEvent &event);
  void text(std::string_view utf8);
  void key(int imguiKey, bool down);
  void wheel(float x, float y);
  bool wantsKeyboard() const noexcept;
  u32 inputId() const noexcept;
  std::string inputText() const;
  bool replaceInput(u32 id,std::string_view text);
  void finishInput(bool accept);
  bool overlayAt(UiPoint point,const char *mainWindow) const noexcept;
  static int inputCallback(ImGuiInputTextCallbackData *data);
  std::span<const u8> atlas() const noexcept { return atlas_; }
  u32 atlasWidth() const noexcept { return atlasWidth_; }
  u32 atlasHeight() const noexcept { return atlasHeight_; }
  u32 rejectedCommands() const noexcept { return rejectedCommands_; }
  void activate() const noexcept;
private:
  ImGuiContext *context_ = nullptr;
  std::vector<u8> atlas_;
  std::vector<u8> fontData_;
  bool hasBegun_=false;
  u32 atlasWidth_ = 0, atlasHeight_ = 0, rejectedCommands_ = 0;
  bool pointerActive_ = false;
  u32 pointerId_ = 0;
  u32 replacementId_ = 0;
  std::string replacement_;
};
} // namespace ae::ui

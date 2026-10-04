#pragma once

#include "ui/ui_draw_list.h"
#include "ui/ui_input.h"
#include <iosfwd>
#include <string>
#include <vector>
#include <unordered_map>

namespace ae::ui {
using GuiId = u32;
enum class GuiKind : u8 { Panel, Text, Button, Toggle, Slider, Progress, Image, HBox, VBox, Grid };
inline constexpr u32 kGuiKindCount=10;
bool guiContainer(GuiKind kind) noexcept;
enum class GuiAlignment : u8 { Start, Center, End, Stretch };
enum class GuiImageFit : u8 { Stretch, Contain, Cover };
enum class GuiCanvasMode : u8 { Screen, World };
enum class GuiClickAction : u8 { Notify, ToggleVisible, ToggleEnabled, SetValue, PlayAnimation, StopAnimation };
enum class GuiEasing : u8 { Linear, Smooth, EaseIn, EaseOut };
enum class GuiEventKind : u8 { Click, ValueChanged };
enum class GuiVisualState : u8 { Normal, Pressed, Disabled };
struct GuiActionBinding final {
  GuiEventKind event=GuiEventKind::Click;
  GuiClickAction action=GuiClickAction::Notify;
  GuiId target=0;
  float value=0;
};
struct GuiInteraction final {
  bool clickable=false;
  GuiClickAction action=GuiClickAction::Notify;
  GuiId target=0; // 0 = this element; stable ID, not a display name
  float value=0;
};
struct GuiPose final {
  float x=0,y=0,scale=1,opacity=1;
  bool operator==(const GuiPose &) const = default;
};
struct GuiMotion final {
  bool enabled=false,autoPlay=false,loop=false,pingPong=false;
  GuiEasing easing=GuiEasing::Smooth;
  float duration=.3f,delay=0;
  GuiPose from{},to{};
  bool operator==(const GuiMotion &) const = default;
};
struct GuiStateStyle final {
  GuiPose pose{};
  UiColor tint=0xFFFFFFFF;
  bool operator==(const GuiStateStyle &) const = default;
};
struct GuiTransitions final {
  bool enabled=false;
  float duration=.12f;
  GuiEasing easing=GuiEasing::Smooth;
  GuiStateStyle normal{},pressed{{0,0,.94f,1},0xFFFFFFFF},disabled{{0,0,1,.45f},0xFFFFFFFF};
  bool operator==(const GuiTransitions &) const = default;
};
struct GuiCanvas final {
  GuiCanvasMode mode=GuiCanvasMode::Screen;
  UiPoint resolution{800,600};
  float position[3]{0,1,0}, rotation[3]{}; // Euler degrees; XY plane, top-left pixels
  float unitsPerPixel=.005f;
  bool occlusion=true;
};
struct GuiSizing final {
  UiPoint minimum{}, preferred{}, flexible{};
  UiInsets padding=UiInsets::all(8);
  UiPoint spacing{8,8};
  GuiAlignment alignment=GuiAlignment::Stretch;
  u32 columns=2;
  bool ignore=false;
};
class GuiImageAtlas;
const char *guiKindName(GuiKind kind) noexcept;

// Authoring data, independent of ImGui state and of the resolved pixel layout.
struct GuiNode final {
  GuiId id = 0, parent = 0;
  GuiKind kind = GuiKind::Panel;
  std::string name, text;
  UiPoint anchorMin{0, 0}, anchorMax{0, 0};
  UiRect offsets{24, 24, 224, 72}; // left, top, right, bottom relative to anchors
  UiColor background = 0xFF343C49, foreground = 0xFFEEF1F5, accent = 0xFF70ACF5;
  float fontSize = 18, radius = 4, value = 0, minimum = 0, maximum = 1;
  bool visible = true, enabled = true, clipChildren = true;
  GuiSizing sizing{};
  std::string image; // project-relative resource path, never an external filename
  GuiImageFit imageFit=GuiImageFit::Contain;
  UiColor imageTint=0xFFFFFFFF;
  GuiInteraction interaction{};
  GuiMotion motion{};
  // Additional ordered listeners; the legacy click action executes first.
  std::vector<GuiActionBinding> actions;
  GuiTransitions transitions{};
};

class GuiDocument final {
public:
  static constexpr u32 kMaximumNodes = 1024;
  static constexpr u32 kMaximumActions = 16;
  GuiId create(GuiKind kind, GuiId parent = 0);
  bool update(const GuiNode &node, std::string &error);
  bool remove(GuiId id);
  GuiId duplicate(GuiId id);
  bool reorder(GuiId id,int direction);
  const GuiNode *find(GuiId id) const noexcept;
  usize indexOf(GuiId id) const noexcept;
  GuiId findByName(std::string_view name) const noexcept;
  std::span<const GuiNode> nodes() const noexcept { return nodes_; }
  bool validate(std::string &error) const;
  void write(std::ostream &stream) const;
  bool read(std::istream &stream, std::string &error);
  u64 revision() const noexcept { return revision_; }
  const GuiCanvas &canvas() const noexcept {return canvas_;}
  bool setCanvas(const GuiCanvas &canvas,std::string &error);
private:
  GuiCanvas canvas_{};
  std::vector<GuiNode> nodes_;
  std::unordered_map<GuiId,usize> index_;
  void rebuildIndex();
  GuiId nextId_ = 1;
  u64 revision_ = 0;
};

// Snapshot history is bounded. One property drag is one operation, not one per frame.
class GuiHistory final {
public:
  void begin(const GuiDocument &document);
  bool commit(const GuiDocument &document);
  bool undo(GuiDocument &document);
  bool redo(GuiDocument &document);
  void clear();
  bool canUndo() const noexcept { return !undo_.empty(); }
  bool canRedo() const noexcept { return !redo_.empty(); }
private:
  GuiDocument before_;
  bool editing_ = false;
  std::vector<GuiDocument> undo_, redo_;
};

struct GuiEvent final { GuiId node = 0; GuiEventKind kind = GuiEventKind::Click; float value = 0; };
struct GuiPlacement final { GuiId node = 0; UiRect bounds{}, clip{}; bool enabled = true; float opacity=1,scale=1; UiColor tint=0xFFFFFFFF; };

// Game/runtime UI uses its own document copy. Preview/Play never mutates authoring.
class GuiRuntime final {
public:
  void load(const GuiDocument &document,bool animate=true);
  void advance(double seconds);
  bool playAnimation(GuiId id);
  bool stopAnimation(GuiId id);
  const std::string &diagnostic() const noexcept {return diagnostic_;}
  GuiDocument &document() noexcept { return document_; }
  const GuiDocument &document() const noexcept { return document_; }
  void layout(const UiRect &canvas);
  void draw(UiDrawList &list) const;
  void setImages(const GuiImageAtlas *images) {images_=images;layoutRevision_=~u64{0};}
  bool pointer(const UiPointerEvent &event);
  GuiId hit(UiPoint point, bool interactiveOnly = false) const noexcept;
  const GuiPlacement *placement(GuiId id) const noexcept;
  std::span<const GuiPlacement> placements() const noexcept { return placements_; }
  bool setText(GuiId id, std::string text);
  bool setValue(GuiId id, float value);
  bool setVisible(GuiId id, bool visible);
  bool setEnabled(GuiId id, bool enabled);
  bool poll(GuiEvent &event);
  u32 droppedEvents() const noexcept { return droppedEvents_; }
  bool captures(u32 pointerId) const noexcept { return pressed_ && pointer_==pointerId; }
  void cancelPointers() noexcept { pressed_=0;motionDirty_=true; }
private:
  void changeValue(GuiId id, float value, bool emit);
  void emit(GuiEvent event);
  void click(GuiId id);
  void dispatch(GuiEvent event);
  void execute(GuiId source,const GuiActionBinding &action);
  GuiStateStyle stateStyle(GuiId id) const;
  void syncState(const GuiNode &node,bool enabled);
  void syncMotions();
  GuiPose pose(GuiId id) const;
  struct MotionState {GuiMotion config{};double elapsed=0;bool running=false,active=false;};
  std::unordered_map<GuiId,MotionState> motions_;
  struct VisualState {GuiTransitions config{};GuiVisualState state=GuiVisualState::Normal;GuiStateStyle from{},to{};double elapsed=0;};
  std::unordered_map<GuiId,VisualState> states_;
  u64 motionRevision_=~u64{0};
  bool animate_=true;
  std::string diagnostic_;
  GuiDocument document_;
  std::vector<GuiPlacement> placements_;
  std::vector<GuiPlacement> basePlacements_;
  struct PoseTransform {float scale=1,x=0,y=0,opacity=1;};
  std::vector<PoseTransform> transforms_;
  bool motionDirty_=false;
  std::vector<usize> placementByNode_;
  std::vector<GuiEvent> events_;
  std::vector<GuiEvent> pendingActions_;
  bool dispatching_=false;
  UiRect canvas_{};
  u64 layoutRevision_=~u64{0};
  const GuiImageAtlas *images_=nullptr;
  GuiId pressed_ = 0;
  UiRect captureBounds_{},captureClip_{};
  bool pressedInside_=false;
  float captureScale_=1;
  u32 pointer_ = 0, droppedEvents_ = 0;
};
} // namespace ae::ui

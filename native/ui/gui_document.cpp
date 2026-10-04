#include "ui/gui_document.h"
#include "ui/gui_images.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <atomic>

namespace ae::ui {
namespace {
u64 nextRevision() { static std::atomic<u64> counter{0};return counter.fetch_add(1,std::memory_order_relaxed)+1; }
}
const char *guiKindName(GuiKind kind) noexcept {
  static constexpr const char *names[]{"Panel", "Text", "Button", "Toggle", "Slider", "Progress", "Image", "HBox", "VBox", "Grid"};
  const auto index = static_cast<u32>(kind);
  return index < kGuiKindCount ? names[index] : "Invalid";
}
bool guiContainer(GuiKind k) noexcept {return k==GuiKind::HBox || k==GuiKind::VBox || k==GuiKind::Grid;}
bool GuiDocument::setCanvas(const GuiCanvas &canvas,std::string &error) {auto old=canvas_;canvas_=canvas;if(!validate(error)){canvas_=old;return false;}revision_=nextRevision();return true;}
const GuiNode *GuiDocument::find(GuiId id) const noexcept {
  const auto i=indexOf(id);return i<nodes_.size()?&nodes_[i]:nullptr;
}
usize GuiDocument::indexOf(GuiId id) const noexcept {const auto i=index_.find(id);return i==index_.end()?nodes_.size():i->second;}
void GuiDocument::rebuildIndex() {index_.clear();index_.reserve(nodes_.size());for(usize i=0;i<nodes_.size();++i)index_.emplace(nodes_[i].id,i);}
GuiId GuiDocument::findByName(std::string_view name) const noexcept {
  for (const auto &node : nodes_) if (node.name == name) return node.id;
  return 0;
}
GuiId GuiDocument::create(GuiKind kind, GuiId parent) {
  if (nodes_.size() >= kMaximumNodes || (parent && !find(parent)) ||
      static_cast<u32>(kind) >= kGuiKindCount || nextId_ == std::numeric_limits<GuiId>::max()) return 0;
  GuiNode node;
  node.id = nextId_++;
  node.parent = parent;
  node.kind = kind;
  node.name = std::string(guiKindName(kind)) + std::to_string(node.id);
  const auto base=node.name;
  for(u32 suffix=1;findByName(node.name);++suffix) node.name=base+"_"+std::to_string(suffix);
  node.text = kind == GuiKind::Panel || guiContainer(kind) ? "" : guiKindName(kind);
  if (kind == GuiKind::Image) {node.text="";node.background=0;node.offsets={24,24,224,224};}
  if (kind == GuiKind::Panel || guiContainer(kind)) { node.offsets = {24, 24, 344, 264}; node.background = 0xE6242830; }
  if (kind == GuiKind::Text) node.background = 0;
  if (kind == GuiKind::Toggle) node.value = 0;
  if (kind == GuiKind::Slider || kind == GuiKind::Progress) node.value = 0.5f;
  nodes_.push_back(std::move(node));
  index_.emplace(nodes_.back().id,nodes_.size()-1);
  revision_=nextRevision();
  return nodes_.back().id;
}
bool GuiDocument::validate(std::string &error) const {
  std::unordered_set<GuiId> seen;
  if (nodes_.size() > kMaximumNodes) { error = "UI exceeds 1024 nodes"; return false; }
  const auto &c=canvas_;
  if(static_cast<u32>(c.mode)>1 || !std::isfinite(c.resolution.x) || !std::isfinite(c.resolution.y) || c.resolution.x<32 || c.resolution.y<32 || c.resolution.x>8192 || c.resolution.y>8192 || !std::isfinite(c.unitsPerPixel) || c.unitsPerPixel<.00001f || c.unitsPerPixel>10) {error="Invalid canvas geometry";return false;}
  for(float v:c.position) if(!std::isfinite(v) || std::abs(v)>1000000) {error="Invalid canvas position";return false;}
  for(float v:c.rotation) if(!std::isfinite(v) || std::abs(v)>36000) {error="Invalid canvas rotation";return false;}
  for (const auto &n : nodes_) {
    const auto &m=n.motion;
    if(static_cast<u32>(n.interaction.action)>5 || !std::isfinite(n.interaction.value) || static_cast<u32>(m.easing)>3 ||
       !std::isfinite(m.duration) || m.duration<.01f || m.duration>3600 || !std::isfinite(m.delay) || m.delay<0 || m.delay>3600) {error="Invalid UI behavior in "+n.name;return false;}
    for(const auto &p:{m.from,m.to}) if(!std::isfinite(p.x) || !std::isfinite(p.y) || std::abs(p.x)>100000 || std::abs(p.y)>100000 || !std::isfinite(p.scale) || p.scale<.01f || p.scale>100 || !std::isfinite(p.opacity) || p.opacity<0 || p.opacity>1) {error="Invalid UI animation pose in "+n.name;return false;}
    if(n.actions.size()>kMaximumActions) {error="UI exceeds 16 additional actions in "+n.name;return false;}
    for(const auto &a:n.actions) if(static_cast<u32>(a.event)>1 || static_cast<u32>(a.action)>5 || !std::isfinite(a.value)) {error="Invalid UI action in "+n.name;return false;}
    const auto &t=n.transitions;
    if(!std::isfinite(t.duration) || t.duration<0 || t.duration>60 || static_cast<u32>(t.easing)>3) {error="Invalid UI state transition in "+n.name;return false;}
    for(const auto &v:{t.normal,t.pressed,t.disabled}) {const auto &p=v.pose;if(!std::isfinite(p.x) || !std::isfinite(p.y) || std::abs(p.x)>100000 || std::abs(p.y)>100000 || !std::isfinite(p.scale) || p.scale<.01f || p.scale>100 || !std::isfinite(p.opacity) || p.opacity<0 || p.opacity>1) {error="Invalid UI state pose in "+n.name;return false;}}
    const auto &z=n.sizing;
    const float values[]{z.minimum.x,z.minimum.y,z.preferred.x,z.preferred.y,z.flexible.x,z.flexible.y,z.padding.left,z.padding.top,z.padding.right,z.padding.bottom,z.spacing.x,z.spacing.y};
    for(float v:values) if(!std::isfinite(v) || v<0 || v>100000) {error="Invalid layout sizing in "+n.name;return false;}
    if(static_cast<u32>(z.alignment)>3 || !z.columns || z.columns>64 || static_cast<u32>(n.imageFit)>2 || n.image.size()>1024 || n.image.find('\0')!=std::string::npos) {error="Invalid UI image/layout";return false;}
    if(!n.image.empty() && (n.image.front()=='/' || n.image.find('\\')!=std::string::npos || n.image.find(':')!=std::string::npos)) {error="UI image path must be project-relative";return false;}
    for(usize a=0;a<n.image.size();) {const auto b=n.image.find('/',a);const auto part=n.image.substr(a,b==std::string::npos?b:b-a);if(part.empty() || part==".." || part=="."){error="Unsafe UI image path";return false;}if(b==std::string::npos)break;a=b+1;}
    if (!n.id || !seen.insert(n.id).second || (n.parent && !seen.contains(n.parent)) || n.parent == n.id) {
      error = "Invalid UI tree: duplicate ID, cycle or parent after child"; return false;
    }
    if (static_cast<u32>(n.kind) >= kGuiKindCount || n.name.empty() || n.name.size() > 256 || n.text.size() > 4096 ||
        !isFinite(n.offsets) || std::abs(n.offsets.x)>1000000 || std::abs(n.offsets.y)>1000000 ||
        std::abs(n.offsets.width)>1000000 || std::abs(n.offsets.height)>1000000 ||
        !std::isfinite(n.anchorMin.x) || !std::isfinite(n.anchorMin.y) ||
        !std::isfinite(n.anchorMax.x) || !std::isfinite(n.anchorMax.y) ||
        n.anchorMin.x < 0 || n.anchorMin.y < 0 || n.anchorMax.x > 1 || n.anchorMax.y > 1 ||
        n.anchorMin.x > n.anchorMax.x || n.anchorMin.y > n.anchorMax.y ||
        !std::isfinite(n.fontSize) || n.fontSize < 6 || n.fontSize > 128 ||
        !std::isfinite(n.radius) || n.radius < 0 || n.radius > 512 ||
        !std::isfinite(n.value) || !std::isfinite(n.minimum) || !std::isfinite(n.maximum) ||
        n.maximum <= n.minimum || n.value < n.minimum || n.value > n.maximum) {
      error = "Invalid UI property/domain in " + n.name; return false;
    }
  }
  std::unordered_set<std::string> names;
  for (const auto &n : nodes_) if (!names.insert(n.name).second) { error = "UI names must be unique"; return false; }
  error.clear(); return true;
}
bool GuiDocument::update(const GuiNode &node, std::string &error) {
  for (auto &stored : nodes_) if (stored.id == node.id) {
    GuiNode old = stored;
    stored = node;
    if (!validate(error)) { stored = std::move(old); return false; }
    revision_=nextRevision(); return true;
  }
  error = "UI node missing"; return false;
}
bool GuiDocument::remove(GuiId id) {
  if (!find(id)) return false;
  std::unordered_set<GuiId> removed{id};
  // Parents precede children; a single forward pass discovers the full subtree.
  for (const auto &node : nodes_) if (removed.contains(node.parent)) removed.insert(node.id);
  std::erase_if(nodes_, [&](const GuiNode &node) { return removed.contains(node.id); });
  rebuildIndex();revision_=nextRevision(); return true;
}
GuiId GuiDocument::duplicate(GuiId id) {
  const auto *root = find(id);
  if (!root) return 0;
  std::vector<GuiNode> copies;
  std::vector<std::pair<GuiId, GuiId>> remap;
  GuiId next = nextId_;
  for (const auto &node : nodes_) {
    GuiId parent = node.parent;
    bool include = node.id == id;
    for (const auto &pair : remap) if (pair.first == parent) { parent = pair.second; include = true; break; }
    if (!include) continue;
    if (next == std::numeric_limits<GuiId>::max()) return 0;
    GuiNode copy = node; copy.id = next++; copy.parent = parent;
    copy.name = node.name.substr(0, 220) + "_copy" + std::to_string(copy.id);
    const auto base=copy.name;
    for(u32 suffix=1;findByName(copy.name);++suffix) copy.name=base+"_"+std::to_string(suffix);
    remap.emplace_back(node.id, copy.id); copies.push_back(std::move(copy));
  }
  if (nodes_.size() + copies.size() > kMaximumNodes) return 0;
  for(auto &copy:copies) for(const auto &pair:remap) if(copy.interaction.target==pair.first) {copy.interaction.target=pair.second;break;}
  for(auto &copy:copies) for(auto &action:copy.actions) for(const auto &pair:remap) if(action.target==pair.first){action.target=pair.second;break;}
  const GuiId result = copies.front().id;
  nodes_.insert(nodes_.end(), copies.begin(), copies.end()); nextId_ = next;rebuildIndex();revision_=nextRevision(); return result;
}
bool GuiDocument::reorder(GuiId id,int direction) {
  const auto *node=find(id);if(!node || (direction!=-1&&direction!=1))return false;
  std::unordered_map<GuiId,std::vector<GuiId>> children;for(const auto &n:nodes_)children[n.parent].push_back(n.id);
  auto &siblings=children[node->parent];const auto at=std::find(siblings.begin(),siblings.end(),id);
  if(at==siblings.end() || (direction<0&&at==siblings.begin()) || (direction>0&&at+1==siblings.end()))return false;
  std::iter_swap(at,at+direction);std::vector<GuiNode> ordered;ordered.reserve(nodes_.size());
  auto pending=children[0];std::reverse(pending.begin(),pending.end());
  while(!pending.empty()){const auto next=pending.back();pending.pop_back();ordered.push_back(*find(next));const auto &descendants=children[next];pending.insert(pending.end(),descendants.rbegin(),descendants.rend());}
  nodes_=std::move(ordered);rebuildIndex();revision_=nextRevision();return true;
}
void GuiDocument::write(std::ostream &s) const {
  s << "AEUI 4 " << nodes_.size() << ' ' << nextId_ << '\n' << std::setprecision(9);
  const auto &c=canvas_;
  s<<static_cast<u32>(c.mode)<<' '<<c.resolution.x<<' '<<c.resolution.y;
  for(float v:c.position)s<<' '<<v;
  for(float v:c.rotation)s<<' '<<v;
  s<<' '<<c.unitsPerPixel<<' '<<c.occlusion<<'\n';
  for (const auto &n : nodes_) {
    s << n.id << ' ' << n.parent << ' ' << static_cast<u32>(n.kind) << ' ' << std::quoted(n.name) << ' ' << std::quoted(n.text)
      << ' ' << n.anchorMin.x << ' ' << n.anchorMin.y << ' ' << n.anchorMax.x << ' ' << n.anchorMax.y
      << ' ' << n.offsets.x << ' ' << n.offsets.y << ' ' << n.offsets.width << ' ' << n.offsets.height
      << ' ' << n.background << ' ' << n.foreground << ' ' << n.accent << ' ' << n.fontSize << ' ' << n.radius
      << ' ' << n.value << ' ' << n.minimum << ' ' << n.maximum << ' ' << n.visible << ' ' << n.enabled << ' ' << n.clipChildren;
    const auto &z=n.sizing;
    s<<' '<<z.minimum.x<<' '<<z.minimum.y<<' '<<z.preferred.x<<' '<<z.preferred.y<<' '<<z.flexible.x<<' '<<z.flexible.y
     <<' '<<z.padding.left<<' '<<z.padding.top<<' '<<z.padding.right<<' '<<z.padding.bottom<<' '<<z.spacing.x<<' '<<z.spacing.y
     <<' '<<static_cast<u32>(z.alignment)<<' '<<z.columns<<' '<<z.ignore<<' '<<std::quoted(n.image)<<' '<<static_cast<u32>(n.imageFit)<<' '<<n.imageTint;
    const auto &i=n.interaction;const auto &m=n.motion;
    s<<' '<<i.clickable<<' '<<static_cast<u32>(i.action)<<' '<<i.target<<' '<<i.value
     <<' '<<m.enabled<<' '<<m.autoPlay<<' '<<m.loop<<' '<<m.pingPong<<' '<<static_cast<u32>(m.easing)<<' '<<m.duration<<' '<<m.delay;
    for(const auto &p:{m.from,m.to})s<<' '<<p.x<<' '<<p.y<<' '<<p.scale<<' '<<p.opacity;
    s<<' '<<n.actions.size();
    for(const auto &a:n.actions)s<<' '<<static_cast<u32>(a.event)<<' '<<static_cast<u32>(a.action)<<' '<<a.target<<' '<<a.value;
    const auto &t=n.transitions;s<<' '<<t.enabled<<' '<<t.duration<<' '<<static_cast<u32>(t.easing);
    for(const auto &v:{t.normal,t.pressed,t.disabled})s<<' '<<v.pose.x<<' '<<v.pose.y<<' '<<v.pose.scale<<' '<<v.pose.opacity<<' '<<v.tint;
    s<<'\n';
  }
}
bool GuiDocument::read(std::istream &s, std::string &error) {
  GuiDocument candidate;
  std::string magic; u32 version = 0, count = 0;
  if (!(s >> magic >> version >> count >> candidate.nextId_) || magic != "AEUI" || version<1 || version>4 || count > kMaximumNodes) {
    error = "Unsupported/corrupt AEUI header"; return false;
  }
  if(version>=2) {
    auto &c=candidate.canvas_;u32 mode;
    if(!(s>>mode>>c.resolution.x>>c.resolution.y>>c.position[0]>>c.position[1]>>c.position[2]>>c.rotation[0]>>c.rotation[1]>>c.rotation[2]>>c.unitsPerPixel>>c.occlusion) || mode>1) {error="Corrupt canvas";return false;}c.mode=static_cast<GuiCanvasMode>(mode);
  }
  for (u32 i = 0; i < count; ++i) {
    GuiNode n; u32 kind = 0;
    if (!(s >> n.id >> n.parent >> kind >> std::quoted(n.name) >> std::quoted(n.text)
      >> n.anchorMin.x >> n.anchorMin.y >> n.anchorMax.x >> n.anchorMax.y
      >> n.offsets.x >> n.offsets.y >> n.offsets.width >> n.offsets.height
      >> n.background >> n.foreground >> n.accent >> n.fontSize >> n.radius
      >> n.value >> n.minimum >> n.maximum >> n.visible >> n.enabled >> n.clipChildren) || kind >= (version==1?6:kGuiKindCount)) {
      error = "Corrupt AEUI node " + std::to_string(i); return false;
    }
    if(version>=2) {
      auto &z=n.sizing;u32 alignment,fit;
      if(!(s>>z.minimum.x>>z.minimum.y>>z.preferred.x>>z.preferred.y>>z.flexible.x>>z.flexible.y>>z.padding.left>>z.padding.top>>z.padding.right>>z.padding.bottom>>z.spacing.x>>z.spacing.y>>alignment>>z.columns>>z.ignore>>std::quoted(n.image)>>fit>>n.imageTint) || alignment>3 || fit>2) {error="Corrupt layout/image";return false;}
      z.alignment=static_cast<GuiAlignment>(alignment);n.imageFit=static_cast<GuiImageFit>(fit);
    }
    if(version>=3) {
      auto &a=n.interaction;auto &m=n.motion;u32 action,easing;
      if(!(s>>a.clickable>>action>>a.target>>a.value>>m.enabled>>m.autoPlay>>m.loop>>m.pingPong>>easing>>m.duration>>m.delay
           >>m.from.x>>m.from.y>>m.from.scale>>m.from.opacity>>m.to.x>>m.to.y>>m.to.scale>>m.to.opacity) || action>5 || easing>3) {error="Corrupt UI behavior";return false;}
      a.action=static_cast<GuiClickAction>(action);m.easing=static_cast<GuiEasing>(easing);
    }
    if(version>=4) {
      u32 total=0,easing=0;
      if(!(s>>total) || total>kMaximumActions){error="Corrupt UI action count";return false;}
      for(u32 j=0;j<total;++j){GuiActionBinding a;u32 event,action;if(!(s>>event>>action>>a.target>>a.value) || event>1 || action>5){error="Corrupt UI action";return false;}a.event=static_cast<GuiEventKind>(event);a.action=static_cast<GuiClickAction>(action);n.actions.push_back(a);}
      auto &t=n.transitions;
      if(!(s>>t.enabled>>t.duration>>easing) || easing>3){error="Corrupt UI state transition";return false;}t.easing=static_cast<GuiEasing>(easing);
      for(auto *v:{&t.normal,&t.pressed,&t.disabled})if(!(s>>v->pose.x>>v->pose.y>>v->pose.scale>>v->pose.opacity>>v->tint)){error="Corrupt UI state style";return false;}
    }
    n.kind = static_cast<GuiKind>(kind); candidate.nodes_.push_back(std::move(n));
  }
  s >> std::ws;
  if (!s.eof() || !candidate.validate(error)) { if (error.empty()) error = "Unexpected AEUI trailing data"; return false; }
  for (const auto &n : candidate.nodes_) if (n.id >= candidate.nextId_) { error = "Invalid AEUI next ID"; return false; }
  if (!candidate.nextId_) { error = "Invalid AEUI ID allocator"; return false; }
  candidate.rebuildIndex();candidate.revision_=nextRevision();*this = std::move(candidate);return true;
}
void GuiHistory::begin(const GuiDocument &d) { if (!editing_) { before_ = d; editing_ = true; } }
bool GuiHistory::commit(const GuiDocument &d) {
  if (!editing_) return false;
  editing_ = false;
  std::ostringstream a, b; before_.write(a); d.write(b);
  if (a.str() == b.str()) return false;
  if (undo_.size() == 64) undo_.erase(undo_.begin());
  undo_.push_back(std::move(before_)); redo_.clear(); return true;
}
bool GuiHistory::undo(GuiDocument &d) {
  commit(d); if (undo_.empty()) return false;
  redo_.push_back(d); d = std::move(undo_.back()); undo_.pop_back(); return true;
}
bool GuiHistory::redo(GuiDocument &d) {
  if (redo_.empty()) return false;
  undo_.push_back(d); d = std::move(redo_.back()); redo_.pop_back(); return true;
}
void GuiHistory::clear() { undo_.clear(); redo_.clear(); editing_ = false; }

void GuiRuntime::load(const GuiDocument &d,bool animate) { document_ = d; placements_.clear();placementByNode_.clear();layoutRevision_=~u64{0};events_.clear();pressed_=0;droppedEvents_=0;motions_.clear();basePlacements_.clear();motionDirty_=true;motionRevision_=~u64{0};animate_=animate;diagnostic_.clear();states_.clear();pendingActions_.clear();dispatching_=false;syncMotions(); }
void GuiRuntime::syncMotions() {
  if(motionRevision_==document_.revision())return;
  motionRevision_=document_.revision();motionDirty_=true;
  std::erase_if(motions_,[&](const auto &item){const auto *n=document_.find(item.first);return !n || !n->motion.enabled || !animate_;});
  std::erase_if(states_,[&](const auto &item){const auto *n=document_.find(item.first);return !n || !n->transitions.enabled || !animate_;});
  if(!animate_)return;
  for(const auto &n:document_.nodes()) if(n.motion.enabled) {
    auto [at,inserted]=motions_.try_emplace(n.id);
    if(inserted || at->second.config!=n.motion)at->second={n.motion,0,n.motion.autoPlay,n.motion.autoPlay};
  }
}
GuiPose GuiRuntime::pose(GuiId id) const {
  const auto at=motions_.find(id);if(at==motions_.end() || !at->second.active)return {};
  const auto &s=at->second;const auto &m=s.config;
  const double time=std::max(0.0,s.elapsed-m.delay),cycle=double(m.duration)*(m.pingPong?2:1);
  double phase=m.loop?std::fmod(time,cycle):std::min(time,cycle);
  if(m.pingPong && phase>m.duration)phase=cycle-phase;
  float t=std::clamp(static_cast<float>(phase/m.duration),0.f,1.f);
  if(m.easing==GuiEasing::Smooth)t=t*t*(3-2*t);
  else if(m.easing==GuiEasing::EaseIn)t=t*t;
  else if(m.easing==GuiEasing::EaseOut)t=1-(1-t)*(1-t);
  const auto lerp=[&](float a,float b){return a+(b-a)*t;};
  return {lerp(m.from.x,m.to.x),lerp(m.from.y,m.to.y),lerp(m.from.scale,m.to.scale),lerp(m.from.opacity,m.to.opacity)};
}
namespace {
float guiCurve(float t,GuiEasing easing) {
  if(easing==GuiEasing::Smooth)return t*t*(3-2*t);
  if(easing==GuiEasing::EaseIn)return t*t;
  if(easing==GuiEasing::EaseOut)return 1-(1-t)*(1-t);
  return t;
}
UiColor multiplyTint(UiColor a,UiColor b) {
  UiColor c=0;for(u32 shift=0;shift<32;shift+=8)c|=(((a>>shift)&255)*((b>>shift)&255)+127)/255<<shift;return c;
}
}
GuiStateStyle GuiRuntime::stateStyle(GuiId id) const {
  const auto at=states_.find(id);if(at==states_.end())return {};
  const auto &s=at->second;const float t=guiCurve(s.config.duration>0?std::clamp(float(s.elapsed/s.config.duration),0.f,1.f):1.f,s.config.easing);
  const auto lerp=[&](float a,float b){return a+(b-a)*t;};
  GuiStateStyle out{{lerp(s.from.pose.x,s.to.pose.x),lerp(s.from.pose.y,s.to.pose.y),lerp(s.from.pose.scale,s.to.pose.scale),lerp(s.from.pose.opacity,s.to.pose.opacity)},0};
  for(u32 shift=0;shift<32;shift+=8)out.tint|=static_cast<u32>(std::lround(lerp(float((s.from.tint>>shift)&255),float((s.to.tint>>shift)&255))))<<shift;
  return out;
}
void GuiRuntime::syncState(const GuiNode &n,bool enabled) {
  if(!animate_ || !n.transitions.enabled)return;
  const auto state=!enabled?GuiVisualState::Disabled:pressed_==n.id && pressedInside_?GuiVisualState::Pressed:GuiVisualState::Normal;
  const auto target=state==GuiVisualState::Disabled?n.transitions.disabled:state==GuiVisualState::Pressed?n.transitions.pressed:n.transitions.normal;
  auto [at,inserted]=states_.try_emplace(n.id);
  if(inserted){at->second={n.transitions,state,target,target,n.transitions.duration};return;}
  if(at->second.state!=state || at->second.config!=n.transitions){const auto current=stateStyle(n.id);at->second={n.transitions,state,current,target,0};}
}
void GuiRuntime::advance(double seconds) {
  syncMotions();if(!std::isfinite(seconds) || seconds<=0)return;
  // Synchronize changed enable/configuration before consuming this frame's clock.
  layout(canvas_);
  bool changed=false;
  for(auto &[id,s]:states_) if(s.elapsed<s.config.duration){s.elapsed=std::min(double(s.config.duration),s.elapsed+seconds);changed=true;}
  for(auto &[id,s]:motions_) if(s.running) {
    const double end=double(s.config.delay)+double(s.config.duration)*(s.config.pingPong?2:1);
    if(s.config.loop) {
      const double cycle=end-s.config.delay;
      // Bound the clock even after many hours, preserving the one-time start delay.
      double remaining=seconds;
      if(s.elapsed<s.config.delay){const double start=std::min(remaining,double(s.config.delay)-s.elapsed);s.elapsed+=start;remaining-=start;}
      s.elapsed+=std::fmod(remaining,cycle);
      if(s.elapsed>=end)s.elapsed=s.config.delay+std::fmod(s.elapsed-s.config.delay,cycle);
    } else {s.elapsed=std::min(end,s.elapsed+seconds);if(s.elapsed>=end)s.running=false;}
    changed=true;
  }
  if(changed || layoutRevision_!=document_.revision()){motionDirty_=true;layout(canvas_);}
}
bool GuiRuntime::playAnimation(GuiId id) {
  syncMotions();const auto at=motions_.find(id);if(at==motions_.end()){diagnostic_="Animation unavailable for UI ID "+std::to_string(id);return false;}
  at->second.elapsed=0;at->second.running=at->second.active=true;diagnostic_.clear();motionDirty_=true;layout(canvas_);return true;
}
bool GuiRuntime::stopAnimation(GuiId id) {
  syncMotions();const auto at=motions_.find(id);if(at==motions_.end()){diagnostic_="Animation unavailable for UI ID "+std::to_string(id);return false;}
  at->second.running=at->second.active=false;diagnostic_.clear();motionDirty_=true;layout(canvas_);return true;
}
const GuiPlacement *GuiRuntime::placement(GuiId id) const noexcept {
  const auto nodeIndex=document_.indexOf(id);
  if(nodeIndex>=placementByNode_.size()) return nullptr;
  const auto index=placementByNode_[nodeIndex];return index<placements_.size()?&placements_[index]:nullptr;
}
namespace {
float alignSlack(GuiAlignment a,float slack) {return a==GuiAlignment::Center?slack*.5f:a==GuiAlignment::End?slack:0;}
}
void GuiRuntime::layout(const UiRect &canvas) {
  syncMotions();
  const bool rebuild=layoutRevision_!=document_.revision() || canvas.x!=canvas_.x || canvas.y!=canvas_.y || canvas.width!=canvas_.width || canvas.height!=canvas_.height;
  if(!rebuild && !motionDirty_)return;
  const auto nodes=document_.nodes();const usize count=nodes.size();
  if(rebuild) {
  canvas_=canvas;layoutRevision_=document_.revision();placements_.clear();
  placementByNode_.assign(count,std::numeric_limits<usize>::max());
  std::vector<std::vector<usize>> children(count);
  for(usize i=0;i<count;++i) if(nodes[i].parent)children[document_.indexOf(nodes[i].parent)].push_back(i);
  std::vector<UiPoint> minimum(count),preferred(count);
  for(usize i=count;i-->0;) {
    const auto &n=nodes[i];const auto &z=n.sizing;minimum[i]=z.minimum;
    UiPoint content{std::max(0.f,n.offsets.width-n.offsets.x),std::max(0.f,n.offsets.height-n.offsets.y)};
    if(n.kind==GuiKind::Text) content={std::max(16.f,float(n.text.size())*n.fontSize*.55f+16),n.fontSize*1.4f+16};
    if(n.kind==GuiKind::Image && images_) if(const auto *im=images_->find(n.image);im && im->error.empty()) content={float(im->width),float(im->height)};
    if(guiContainer(n.kind)) {
      content={};UiPoint minContent{};u32 total=0;
      for(const auto child:children[i]) if(nodes[child].visible && !nodes[child].sizing.ignore) {
        ++total;
        if(n.kind==GuiKind::HBox) {content.x+=preferred[child].x;content.y=std::max(content.y,preferred[child].y);minContent.x+=minimum[child].x;minContent.y=std::max(minContent.y,minimum[child].y);}
        else if(n.kind==GuiKind::VBox) {content.y+=preferred[child].y;content.x=std::max(content.x,preferred[child].x);minContent.y+=minimum[child].y;minContent.x=std::max(minContent.x,minimum[child].x);}
        else {content.x=std::max(content.x,preferred[child].x);content.y=std::max(content.y,preferred[child].y);minContent.x=std::max(minContent.x,minimum[child].x);minContent.y=std::max(minContent.y,minimum[child].y);}
      }
      const u32 cols=std::min(z.columns,total),rows=cols?(total+cols-1)/cols:0;
      if(n.kind==GuiKind::Grid) {content.x*=cols;minContent.x*=cols;content.y*=rows;minContent.y*=rows;}
      const float gx=n.kind==GuiKind::HBox?float(total?total-1:0)*z.spacing.x:n.kind==GuiKind::Grid?float(cols?cols-1:0)*z.spacing.x:0;
      const float gy=n.kind==GuiKind::VBox?float(total?total-1:0)*z.spacing.y:n.kind==GuiKind::Grid?float(rows?rows-1:0)*z.spacing.y:0;
      content.x+=z.padding.left+z.padding.right+gx;content.y+=z.padding.top+z.padding.bottom+gy;
      minimum[i].x=std::max(minimum[i].x,minContent.x+z.padding.left+z.padding.right+gx);minimum[i].y=std::max(minimum[i].y,minContent.y+z.padding.top+z.padding.bottom+gy);
    }
    preferred[i]={std::max(minimum[i].x,z.preferred.x>0?z.preferred.x:content.x),std::max(minimum[i].y,z.preferred.y>0?z.preferred.y:content.y)};
  }
  std::vector<UiRect> assigned(count);std::vector<bool> managed(count,false);
  for(usize i=0;i<count;++i) {
    const auto &n=nodes[i];if(!n.visible)continue;
    UiRect parent=canvas,clip=canvas;bool enabled=n.enabled;
    if(n.parent) {const auto *p=placement(n.parent);const auto *pn=document_.find(n.parent);if(!p)continue;parent=p->bounds;clip=pn->clipChildren?intersect(p->clip,parent):p->clip;enabled&=p->enabled;}
    const float left=parent.x+parent.width*n.anchorMin.x+n.offsets.x,top=parent.y+parent.height*n.anchorMin.y+n.offsets.y;
    UiRect r=managed[i]?assigned[i]:UiRect{left,top,std::max(0.f,parent.width*n.anchorMax.x+n.offsets.width+parent.x-left),std::max(0.f,parent.height*n.anchorMax.y+n.offsets.height+parent.y-top)};
    placementByNode_[i]=placements_.size();placements_.push_back({n.id,r,clip,enabled});
    if(!guiContainer(n.kind))continue;
    const auto &z=n.sizing;const UiRect inner=deflate(r,z.padding);
    std::vector<usize> items;for(auto c:children[i])if(nodes[c].visible&&!nodes[c].sizing.ignore)items.push_back(c);
    if(items.empty())continue;
    if(n.kind==GuiKind::Grid) {
      const u32 cols=std::min(z.columns,static_cast<u32>(items.size())),rows=(static_cast<u32>(items.size())+cols-1)/cols;
      float cw=std::max(0.f,(inner.width-(cols-1)*z.spacing.x)/cols),ch=std::max(0.f,(inner.height-(rows-1)*z.spacing.y)/rows);
      for(auto c:items){cw=std::max(cw,minimum[c].x);ch=std::max(ch,minimum[c].y);}
      for(usize j=0;j<items.size();++j) {const auto c=items[j];const float w=z.alignment==GuiAlignment::Stretch?cw:std::min(cw,preferred[c].x),h=z.alignment==GuiAlignment::Stretch?ch:std::min(ch,preferred[c].y);assigned[c]={inner.x+float(j%cols)*(cw+z.spacing.x)+alignSlack(z.alignment,cw-w),inner.y+float(j/cols)*(ch+z.spacing.y)+alignSlack(z.alignment,ch-h),w,h};managed[c]=true;}
    } else {
      const bool horizontal=n.kind==GuiKind::HBox;const float space=horizontal?inner.width:inner.height,gap=horizontal?z.spacing.x:z.spacing.y;
      float sumMin=0,sumPref=0,sumFlex=0;
      for(auto c:items){sumMin+=horizontal?minimum[c].x:minimum[c].y;sumPref+=horizontal?preferred[c].x:preferred[c].y;sumFlex+=horizontal?nodes[c].sizing.flexible.x:nodes[c].sizing.flexible.y;}
      const float available=std::max(0.f,space-float(items.size()-1)*gap),blend=sumPref>sumMin?std::clamp((available-sumMin)/(sumPref-sumMin),0.f,1.f):1;
      const float extra=std::max(0.f,available-sumPref);float cursor=(!sumFlex)?alignSlack(z.alignment,extra):0;
      for(auto c:items) {
        const float mn=horizontal?minimum[c].x:minimum[c].y,pr=horizontal?preferred[c].x:preferred[c].y,fl=horizontal?nodes[c].sizing.flexible.x:nodes[c].sizing.flexible.y;
        const float main=mn+(pr-mn)*blend+(sumFlex?extra*fl/sumFlex:0),crossSpace=horizontal?inner.height:inner.width;
        const float cross=std::max(horizontal?minimum[c].y:minimum[c].x,z.alignment==GuiAlignment::Stretch?crossSpace:std::min(crossSpace,horizontal?preferred[c].y:preferred[c].x));
        const float offset=alignSlack(z.alignment,std::max(0.f,crossSpace-cross));
        assigned[c]=horizontal?UiRect{inner.x+cursor,inner.y+offset,main,cross}:UiRect{inner.x+offset,inner.y+cursor,cross,main};managed[c]=true;cursor+=main+gap;
      }
    }
  }
  basePlacements_=placements_;
  } else placements_=basePlacements_;
  motionDirty_=false;
  // Reuse measured layout and storage while animating. No resource rebuild or sibling reflow.
  transforms_.assign(count,PoseTransform{});
  for(auto &p:placements_) {
    const auto index=document_.indexOf(p.node);const auto &n=nodes[index];const auto original=p.bounds;
    const auto parent=n.parent?transforms_[document_.indexOf(n.parent)]:PoseTransform{};
    syncState(n,p.enabled);const auto visual=stateStyle(n.id);auto local=pose(n.id);
    local.x+=visual.pose.x;local.y+=visual.pose.y;local.scale*=visual.pose.scale;local.opacity*=visual.pose.opacity;
    p.tint=visual.tint;
    auto &a=transforms_[index];a.scale=parent.scale*local.scale;
    a.x=parent.x+parent.scale*((original.x+original.width*.5f)*(1-local.scale)+local.x);
    a.y=parent.y+parent.scale*((original.y+original.height*.5f)*(1-local.scale)+local.y);
    a.opacity=parent.opacity*local.opacity;
    p.bounds={original.x*a.scale+a.x,original.y*a.scale+a.y,original.width*a.scale,original.height*a.scale};
    p.opacity=a.opacity;p.scale=a.scale;p.clip=canvas;
    if(!isFinite(p.bounds) || !std::isfinite(a.scale)) {
      diagnostic_="UI animation transform overflow at ID "+std::to_string(n.id);
      a={0,0,0,0};p.bounds={};p.opacity=0;p.enabled=false;
    }
    if(n.parent){const auto *pp=placement(n.parent);p.clip=nodes[document_.indexOf(n.parent)].clipChildren?intersect(pp->clip,pp->bounds):pp->clip;}
  }
  if(pressed_) {
    const auto *p=placement(pressed_);const auto *n=document_.find(pressed_);
    if(!p || !p->enabled || !n || (!n->interaction.clickable && n->kind!=GuiKind::Button && n->kind!=GuiKind::Toggle && n->kind!=GuiKind::Slider)) {pressed_=0;motionDirty_=true;}
  }
}
void GuiRuntime::draw(UiDrawList &list) const {
  for (const auto &p : placements_) {
    const auto *n = document_.find(p.node); if (!n || p.bounds.isEmpty() || p.opacity<=0) continue;
    if(!list.pushClip(p.clip)) continue;
    const UiRect r = p.bounds;const float scale=p.scale;
    const auto fade=[&](UiColor c){c=multiplyTint(c,p.tint);return (c&0x00FFFFFF)|(static_cast<UiColor>(std::lround((c>>24)*p.opacity))<<24);};
    const UiColor accent=fade(n->accent),foreground=fade(n->foreground);
    UiColor color = fade(n->background);
    if (!n->transitions.enabled && !p.enabled) color = (color & 0x00FFFFFF) | ((color >> 25) << 24);
    // Interaction changes the tint, never the authored background opacity.
    if (!n->transitions.enabled && pressed_ == n->id && pressedInside_) color = (n->accent & 0x00FFFFFF) | (color & 0xFF000000);
    if (color >> 24) list.addRect(r, color, n->radius*scale);
    if(n->kind==GuiKind::Image) {
      const auto *image=images_?images_->find(n->image):nullptr;
      if(image && image->error.empty() && image->width && image->height) {
        UiRect target=r,texels=image->texels;
        const float ratio=float(image->width)/image->height,box=r.width/std::max(1.f,r.height);
        if(n->imageFit==GuiImageFit::Contain) {if(box>ratio){target.width=r.height*ratio;target.x+=(r.width-target.width)/2;}else{target.height=r.width/ratio;target.y+=(r.height-target.height)/2;}}
        if(n->imageFit==GuiImageFit::Cover) {if(box>ratio){const float height=texels.width/box;texels.y+=(texels.height-height)/2;texels.height=height;}else{const float width=texels.height*box;texels.x+=(texels.width-width)/2;texels.width=width;}}
        list.addGuiImage(target,texels,fade(n->imageTint),n->radius*scale);
      } else if(p.opacity>0) {list.addBorder(r,fade(0xFFFF7755),2*scale,0);UiTypeStyle style{};style.size=12*scale;list.addText(r,image?image->error:"Selecione uma imagem",fade(0xFFFFAA88),style);}
      list.popClip();continue;
    }
    const float t = std::clamp((n->value-n->minimum)/(n->maximum-n->minimum), 0.0f, 1.0f);
    UiRect label = deflate(r, UiInsets::all(8*scale));
    if (n->kind == GuiKind::Toggle) {
      const float side = std::min(24*scale, r.height-8*scale);
      UiRect box{r.x+8*scale, r.y+(r.height-side)/2, side, side};
      list.addBorder(box, accent, 2*scale, 2*scale);
      if (t >= 0.5f) list.addRect(deflate(box, UiInsets::all(4*scale)), accent, scale);
      label.x += side+8*scale; label.width = std::max(0.0f, label.width-side-8*scale);
    } else if (n->kind == GuiKind::Slider || n->kind == GuiKind::Progress) {
      UiRect track{r.x+10*scale, r.y+r.height*0.68f, std::max(0.0f, r.width-20*scale), 6*scale};
      list.addRect(track, fade(0xFF171B22), 3*scale);
      list.addRect({track.x,track.y,track.width*t,track.height}, accent, 3*scale);
      if (n->kind == GuiKind::Slider) list.addRect({track.x+track.width*t-6*scale,track.y-5*scale,12*scale,16*scale},foreground,3*scale);
      label.height = r.height*0.6f;
    }
    UiTypeStyle style{}; style.size = n->fontSize*scale;
    const bool centered = n->kind == GuiKind::Button;
    list.addText(label, n->text, foreground, style, centered ? UiAlign::Center : UiAlign::Start);
    list.popClip();
  }
}
GuiId GuiRuntime::hit(UiPoint point, bool interactiveOnly) const noexcept {
  for (auto it = placements_.rbegin(); it != placements_.rend(); ++it) {
    const auto *n = document_.find(it->node);
    if (!n || !it->bounds.contains(point) || !it->clip.contains(point)) continue;
    if (interactiveOnly && (!it->enabled || it->opacity<=0 || (!n->interaction.clickable && n->kind != GuiKind::Button && n->kind != GuiKind::Toggle && n->kind != GuiKind::Slider))) continue;
    return it->node;
  }
  return 0;
}
void GuiRuntime::emit(GuiEvent e) { if (events_.size() == 256) { ++droppedEvents_; return; } events_.push_back(e); }
void GuiRuntime::changeValue(GuiId id, float value, bool send) {
  const auto *n = document_.find(id);
  if (!n || !std::isfinite(value)) return;
  GuiNode edited = *n; edited.value = std::clamp(value, n->minimum, n->maximum);
  if (n->kind == GuiKind::Toggle) edited.value = edited.value >= (n->minimum+n->maximum)*0.5f ? n->maximum : n->minimum;
  if (edited.value == n->value) return;
  std::string error;
  if (document_.update(edited, error) && send) dispatch({id,GuiEventKind::ValueChanged,edited.value});
}
void GuiRuntime::execute(GuiId id,const GuiActionBinding &action) {
  if(action.action==GuiClickAction::Notify)return;
  const auto target=action.target?action.target:id;const auto *n=document_.find(target);
  if(!n){diagnostic_="Action target missing: UI ID "+std::to_string(target);return;}
  bool ok=false;
  switch(action.action) {
    case GuiClickAction::ToggleVisible:ok=setVisible(target,!n->visible);break;
    case GuiClickAction::ToggleEnabled:ok=setEnabled(target,!n->enabled);break;
    case GuiClickAction::SetValue:
      ok=n->kind==GuiKind::Toggle || n->kind==GuiKind::Slider || n->kind==GuiKind::Progress;
      if(ok)changeValue(target,action.value,true);
      break;
    case GuiClickAction::PlayAnimation:ok=playAnimation(target);break;
    case GuiClickAction::StopAnimation:ok=stopAnimation(target);break;
    default:break;
  }
  if(!ok)diagnostic_="Action incompatible with UI ID "+std::to_string(target);
}
void GuiRuntime::dispatch(GuiEvent event) {
  emit(event);
  if(pendingActions_.size()>=256){diagnostic_="UI action dispatch limit exceeded";return;}
  pendingActions_.push_back(event);if(dispatching_)return;
  dispatching_=true;diagnostic_.clear();usize cursor=0;
  while(cursor<pendingActions_.size() && cursor<256) {
    const auto e=pendingActions_[cursor++];const auto *source=document_.find(e.node);if(!source)continue;
    // Snapshot listeners: mutations caused by an action cannot invalidate this iteration.
    const auto listeners=source->actions;const auto primary=source->interaction;
    std::string error=diagnostic_;
    if(e.kind==GuiEventKind::Click)execute(e.node,{e.kind,primary.action,primary.target,primary.value});
    if(!diagnostic_.empty())error=diagnostic_;
    for(const auto &a:listeners)if(a.event==e.kind){execute(e.node,a);if(!diagnostic_.empty())error=diagnostic_;}
    if(!error.empty())diagnostic_=std::move(error);
  }
  if(cursor<pendingActions_.size())diagnostic_="UI action dispatch limit exceeded";
  pendingActions_.clear();dispatching_=false;
}
void GuiRuntime::click(GuiId id) {const auto *n=document_.find(id);if(n)dispatch({id,GuiEventKind::Click,n->value});}
bool GuiRuntime::pointer(const UiPointerEvent &e) {
  layout(canvas_);
  if(e.phase==UiPointerPhase::Down) {
    if(pressed_)return false;
    pressed_=hit(e.position,true);pointer_=e.pointerId;
    if(const auto *p=placement(pressed_)){captureBounds_=p->bounds;captureClip_=p->clip;captureScale_=p->scale;pressedInside_=true;motionDirty_=true;layout(canvas_);}
  }
  if(!pressed_ || pointer_!=e.pointerId)return false;
  const GuiId id=pressed_;const auto *n=document_.find(id);const auto *p=placement(id);
  if(!n || !p || !p->enabled){cancelPointers();return true;}
  if(e.phase==UiPointerPhase::Cancel){cancelPointers();layout(canvas_);return true;}
  // Capture geometry is frozen for this gesture: a shrinking pressed pose cannot oscillate hit state.
  pressedInside_=captureBounds_.contains(e.position) && captureClip_.contains(e.position);motionDirty_=true;
  const auto kind=n->kind;const auto clickable=n->interaction.clickable;const float value=n->value,minimum=n->minimum,maximum=n->maximum;
  if(kind==GuiKind::Slider) {
    const float t=std::clamp((e.position.x-captureBounds_.x-10*captureScale_)/std::max(1.f,captureBounds_.width-20*captureScale_),0.f,1.f);
    changeValue(id,minimum+(maximum-minimum)*t,true);
  }
  if(e.phase==UiPointerPhase::Up) {
    const bool inside=pressedInside_;cancelPointers();
    // ValueChanged listeners may disable/hide this element while dragging.
    layout(canvas_);const auto *current=placement(id);
    if(inside && current && current->enabled) {
      if(kind==GuiKind::Toggle)changeValue(id,value==maximum?minimum:maximum,true);
      if(kind==GuiKind::Button || clickable)click(id);
    }
  }
  layout(canvas_);return true;
}
bool GuiRuntime::setText(GuiId id, std::string text) { const auto *n=document_.find(id); if (!n) return false; auto copy=*n; copy.text=std::move(text); std::string error; return document_.update(copy,error); }
bool GuiRuntime::setValue(GuiId id, float value) { const auto *n=document_.find(id); if (!n || !std::isfinite(value) || (n->kind!=GuiKind::Toggle && n->kind!=GuiKind::Slider && n->kind!=GuiKind::Progress)) return false; changeValue(id,value,false); return true; }
bool GuiRuntime::setVisible(GuiId id, bool v) { const auto *n=document_.find(id); if (!n) return false; auto copy=*n; copy.visible=v; std::string error; const bool ok=document_.update(copy,error); if(ok) layout(canvas_); return ok; }
bool GuiRuntime::setEnabled(GuiId id, bool v) { const auto *n=document_.find(id); if (!n) return false; auto copy=*n; copy.enabled=v; std::string error; const bool ok=document_.update(copy,error); if(ok) layout(canvas_); return ok; }
bool GuiRuntime::poll(GuiEvent &e) { while(!events_.empty()){e=events_.front();events_.erase(events_.begin());if(document_.find(e.node))return true;}return false; }
} // namespace ae::ui

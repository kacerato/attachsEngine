#pragma once
#include "ui/gui_document.h"
#include <functional>
#include <map>

namespace ae::ui {
struct GuiImage final {UiRect texels{};u32 width=0,height=0;std::string error;};
// Owned CPU atlas; immutable between reconciliations. GPU receives its revision.
class GuiImageAtlas final {
public:
  static constexpr u32 kSize=2048,kMaximumImages=64;
  using Loader=std::function<bool(std::string_view,std::vector<u8>&,u32&,u32&,std::string&)>;
  void reconcile(const GuiDocument &document,u64 sourceRevision,const Loader &load);
  void clear();
  const GuiImage *find(std::string_view path) const;
  std::span<const u8> pixels() const {return pixels_;}
  u32 size() const {return pixels_.empty()?0:kSize;}
  u64 revision() const {return revision_;}
  const std::map<std::string,GuiImage,std::less<>> &entries() const {return entries_;}
private:
  u64 sourceRevision_=~u64{0},revision_=0;
  std::vector<u8> pixels_;
  std::map<std::string,GuiImage,std::less<>> entries_;
};
} // namespace ae::ui

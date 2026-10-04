#include "ui/gui_images.h"
#include <algorithm>
#include <set>
namespace ae::ui {
void GuiImageAtlas::clear() {sourceRevision_=~u64{0};entries_.clear();std::vector<u8>{}.swap(pixels_);++revision_;}
const GuiImage *GuiImageAtlas::find(std::string_view path) const {const auto i=entries_.find(path);return i==entries_.end()?nullptr:&i->second;}
void GuiImageAtlas::reconcile(const GuiDocument &d,u64 source,const Loader &load) {
  if(sourceRevision_==source) return;
  sourceRevision_=source;entries_.clear();
  std::set<std::string> paths;
  for(const auto &n:d.nodes()) if(n.kind==GuiKind::Image && !n.image.empty()) paths.insert(n.image);
  if(paths.empty()){std::vector<u8>{}.swap(pixels_);++revision_;return;}
  pixels_.assign(static_cast<usize>(kSize)*kSize*4,0);
  u32 x=1,y=1,row=0,count=0;
  for(const auto &path:paths) {
    GuiImage entry;std::vector<u8> rgba;
    if(count++>=kMaximumImages) entry.error="UI image budget exceeds 64 resources";
    else if(!load || !load(path,rgba,entry.width,entry.height,entry.error)) {if(entry.error.empty())entry.error="UI image load failed";}
    else if(!entry.width || !entry.height || entry.width>1024 || entry.height>1024 || rgba.size()!=static_cast<usize>(entry.width)*entry.height*4) entry.error="UI image must be at most 1024 x 1024 RGBA8";
    else {
      if(x+entry.width+1>=kSize) {x=1;y+=row+2;row=0;}
      if(y+entry.height+1>=kSize) entry.error="UI image atlas is full";
      else {
        entry.texels={float(x),float(y),float(entry.width),float(entry.height)};
        // Duplicate edge texels into a one-pixel gutter for bilinear sampling.
        for(i32 py=-1;py<=static_cast<i32>(entry.height);++py)
          for(i32 px=-1;px<=static_cast<i32>(entry.width);++px) {
            const usize from=(static_cast<usize>(std::clamp(py,0,static_cast<i32>(entry.height)-1))*entry.width+std::clamp(px,0,static_cast<i32>(entry.width)-1))*4;
            const usize to=(static_cast<usize>(static_cast<i32>(y)+py)*kSize+static_cast<i32>(x)+px)*4;
            std::copy_n(rgba.data()+from,4,pixels_.data()+to);
          }
        x+=entry.width+2;row=std::max(row,entry.height);
      }
    }
    entries_.emplace(path,std::move(entry));
  }
  ++revision_;
}
} // namespace ae::ui

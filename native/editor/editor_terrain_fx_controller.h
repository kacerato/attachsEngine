#pragma once
#include "resources/terrain_fx_field.h"
#include <memory>
namespace ae::editor {
struct TerrainFxEditToken {
  u64 root=0,epoch=0,revision=0;
  bool operator==(const TerrainFxEditToken &)const=default;
};
struct TerrainFxEditorBitmap {
  resources::TerrainFxField field;
  std::vector<u8> pixels;
  resources::TerrainFxDirtyRect dirty;
};
struct TerrainFxPreparedCommit {
  std::shared_ptr<const resources::TerrainFxAsset> before,after;
  std::shared_ptr<const TerrainFxEditorBitmap> bitmap;
  TerrainFxEditToken token;
  u64 retainedBytes=0,sampleRevision=0;
};
// Transient gesture adapter; TerrainRecipe::fx remains the only authored owner.
// It never mutates the destination until the caller atomically records/adopts
// a prepared commit through the editor's existing resource history.
class TerrainFxEditorController {
public:
  using Progress=std::function<bool(double)>;
  static u64 retainedBytes(const resources::TerrainFxAsset &);
  static bool preflight(const resources::TerrainFxAsset &,std::string &,const resources::TerrainFxLimits & = {});
  static bool prepareBitmap(const resources::TerrainFxAsset &,std::span<const float> slope,
    const TerrainFxEditorBitmap *,std::shared_ptr<const TerrainFxEditorBitmap> &,std::string &,
    const resources::TerrainFxLimits & = {},const Progress & = {});
  static bool prepareBlank(u32 width,u32 height,resources::TerrainFxDomain,
    resources::TerrainFxAsset &,std::shared_ptr<const TerrainFxEditorBitmap> &,std::string &,
    const resources::TerrainFxLimits & = {},const Progress & = {});
  bool refresh(const resources::TerrainFxAsset &,std::span<const float>,std::string &,
    const resources::TerrainFxLimits & = {},const Progress & = {});
  bool begin(const resources::TerrainFxAsset &,resources::TerrainFxStroke brush,bool erase,
    TerrainFxEditToken,std::span<const float> slope,std::string &,
    const resources::TerrainFxLimits & = {},const Progress & = {});
  bool sample(TerrainFxEditToken,double localX,double localZ,double sampleSeconds,double pressure,
    std::string &,const resources::TerrainFxLimits & = {},const Progress & = {});
  bool prepareCommit(TerrainFxEditToken,TerrainFxPreparedCommit &,std::string &,
    const resources::TerrainFxLimits & = {},const Progress & = {});
  bool accept(const TerrainFxPreparedCommit &);
  void cancel();
  bool active()const{return active_;}
  bool hasSamples()const{return active_ && draft_.strokes.size()>before_->strokes.size();}
  const std::shared_ptr<const TerrainFxEditorBitmap> &bitmap()const{return bitmap_;}
private:
  bool active_=false,erase_=false;
  TerrainFxEditToken token_;
  resources::TerrainFxStroke brush_;
  resources::TerrainFxAsset draft_;
  std::shared_ptr<const resources::TerrainFxAsset> before_;
  std::shared_ptr<const TerrainFxEditorBitmap> bitmap_;
  resources::TerrainFxLimits capturedLimits_;
  std::vector<float> slope_;
  u64 sampleRevision_=0;
  u64 nextIdentity_=1;u32 gesture_=1;
};
}

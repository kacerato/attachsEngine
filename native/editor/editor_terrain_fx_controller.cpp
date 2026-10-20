#include "editor/editor_terrain_fx_controller.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace ae::editor {
u64 TerrainFxEditorController::retainedBytes(const resources::TerrainFxAsset &a){
  return sizeof(a)+a.seedPixels.capacity()+u64(a.strokes.capacity())*sizeof(resources::TerrainFxStroke)+
    a.sourcePropertiesJson.capacity()+a.checkpointBaseHash.capacity()+a.checkpointPrefixHash.capacity()+a.checkpointPixelsHash.capacity();
}
bool TerrainFxEditorController::preflight(const resources::TerrainFxAsset &a,std::string &error,const resources::TerrainFxLimits &limits){
  if(!resources::validTerrainFxAsset(a,error,limits))return false;
  // Two authoring fields, published/before bytes, three immutable seed copies
  // and a captured slope plane. Conservative peak, not a measured GPU claim.
  const u64 pixels=u64(a.width)*a.height,metadata=retainedBytes(a)-a.seedPixels.capacity();
  if(metadata>limits.maximumWorkingBytes/3 || pixels>(limits.maximumWorkingBytes-metadata*3)/56){
    error="FX editor gesture/checkpoint exceeds working budget; source was not resized";return false;
  }
  return true;
}
bool TerrainFxEditorController::prepareBitmap(const resources::TerrainFxAsset &asset,std::span<const float> slope,
    const TerrainFxEditorBitmap *previous,std::shared_ptr<const TerrainFxEditorBitmap> &out,std::string &error,
    const resources::TerrainFxLimits &limits,const Progress &progress){
  if(!preflight(asset,error,limits))return false;
  auto next=std::make_shared<TerrainFxEditorBitmap>();
  if(!resources::rasterTerrainFx(asset,slope,previous?&previous->field:nullptr,next->field,next->dirty,error,limits,progress) ||
     !resources::quantizeTerrainFx(next->field,next->pixels,error,limits))return false;
  out=std::move(next);return true;
}
bool TerrainFxEditorController::prepareBlank(u32 width,u32 height,resources::TerrainFxDomain domain,
    resources::TerrainFxAsset &asset,std::shared_ptr<const TerrainFxEditorBitmap> &bitmap,std::string &error,
    const resources::TerrainFxLimits &limits,const Progress &progress){
  resources::TerrainFxAsset next;next.enabled=true;next.width=width;next.height=height;next.domain=domain;
  std::shared_ptr<const TerrainFxEditorBitmap> image;
  if(!prepareBitmap(next,{},nullptr,image,error,limits,progress))return false;
  asset=std::move(next);bitmap=std::move(image);return true;
}
bool TerrainFxEditorController::refresh(const resources::TerrainFxAsset &asset,std::span<const float> slope,std::string &error,
    const resources::TerrainFxLimits &limits,const Progress &progress){
  if(active_){error="Finish or cancel the current FX gesture before replacing its source";return false;}
  return prepareBitmap(asset,slope,bitmap_.get(),bitmap_,error,limits,progress);
}
bool TerrainFxEditorController::begin(const resources::TerrainFxAsset &asset,resources::TerrainFxStroke brush,bool erase,
    TerrainFxEditToken token,std::span<const float> slope,std::string &error,const resources::TerrainFxLimits &limits,const Progress &progress){
  if(active_){error="An FX gesture is already active";return false;}
  if(!asset.enabled){error="Create or enable the FX mask before painting";return false;}
  if(brush.slopeFilter && slope.size()!=u64(asset.width)*asset.height){error="Slope-filtered FX requires a corresponding heightfield producer";return false;}
  u64 nextId=1;u32 nextGesture=1;
  for(const auto &s:asset.strokes){
    if(s.identity==std::numeric_limits<u64>::max() || s.gesture==std::numeric_limits<u32>::max()){
      error="FX stroke/gesture identity space exhausted";return false;}
    nextId=std::max(nextId,s.identity+1);nextGesture=std::max(nextGesture,s.gesture+1);
  }
  // Validate prepared brush through the core validator, not duplicate bounds.
  auto candidate=asset;brush.identity=nextId;brush.gesture=nextGesture;
  brush.target=erase?0:brush.target;brush.sampleSeconds=0;brush.pressure=1;
  candidate.strokes.push_back(brush);
  if(!preflight(candidate,error,limits))return false;
  std::shared_ptr<const TerrainFxEditorBitmap> initial;
  if(!prepareBitmap(asset,slope,nullptr,initial,error,limits,progress))return false;
  before_=std::make_shared<const resources::TerrainFxAsset>(asset);draft_=asset;
  bitmap_=std::move(initial);capturedLimits_=limits;slope_.assign(slope.begin(),slope.end());
  brush_=brush;erase_=erase;token_=token;nextIdentity_=nextId;gesture_=nextGesture;sampleRevision_=0;active_=true;return true;
}
bool TerrainFxEditorController::sample(TerrainFxEditToken token,double x,double z,double seconds,double pressure,
    std::string &error,const resources::TerrainFxLimits &limits,const Progress &progress){
  if(!active_){error="FX paint is not armed";return false;}
  if(token!=token_){cancel();error="FX gesture cancelled: terrain or author revision changed";return false;}
  if(!std::isfinite(seconds)||seconds<0||seconds>60||!std::isfinite(pressure)||pressure<0||pressure>1){error="Invalid captured FX sample duration/pressure";return false;}
  if(nextIdentity_==std::numeric_limits<u64>::max()){error="FX identity frontier exhausted";return false;}
  auto candidate=draft_;auto stroke=brush_;stroke.identity=nextIdentity_;stroke.gesture=gesture_;
  stroke.x=x;stroke.z=z;stroke.sampleSeconds=seconds;stroke.pressure=pressure;stroke.target=erase_?0:brush_.target;
  candidate.strokes.push_back(stroke);
  std::shared_ptr<const TerrainFxEditorBitmap> next;
  if(!prepareBitmap(candidate,slope_,bitmap_.get(),next,error,limits,progress))return false;
  draft_=std::move(candidate);bitmap_=std::move(next);++nextIdentity_;++sampleRevision_;return true;
}
bool TerrainFxEditorController::prepareCommit(TerrainFxEditToken token,TerrainFxPreparedCommit &out,std::string &error,
    const resources::TerrainFxLimits &limits,const Progress &progress){
  if(!active_||token!=token_){cancel();error="FX commit cancelled: gesture identity changed";return false;}
  if(!hasSamples()){error="FX gesture has no captured samples";return false;}
  resources::TerrainFxAsset after;
  if(!resources::checkpointTerrainFx(draft_,bitmap_->field,after,error,limits))return false;
  std::shared_ptr<const TerrainFxEditorBitmap> normalized;
  if(!prepareBitmap(after,{},nullptr,normalized,error,limits,progress))return false;
  TerrainFxPreparedCommit next;next.before=before_;next.after=std::make_shared<const resources::TerrainFxAsset>(std::move(after));
  next.bitmap=std::move(normalized);next.token=token;next.sampleRevision=sampleRevision_;next.retainedBytes=retainedBytes(*next.before)+retainedBytes(*next.after);
  out=std::move(next);return true;
}
bool TerrainFxEditorController::accept(const TerrainFxPreparedCommit &commit){
  if(!active_||commit.token!=token_||commit.sampleRevision!=sampleRevision_||commit.before!=before_||!commit.after||!commit.bitmap)return false;
  bitmap_=commit.bitmap;active_=false;before_.reset();draft_={};slope_.clear();return true;
}
void TerrainFxEditorController::cancel(){
  if(active_){
    bitmap_.reset();std::string ignored;
    prepareBitmap(*before_,slope_,nullptr,bitmap_,ignored,capturedLimits_);
  }
  active_=false;before_.reset();draft_={};slope_.clear();
}
}

#pragma once
#include "resources/animation_clip_asset.h"

namespace ae::resources {
// Atomic whole-clip ripple edits. Insertion shifts authored keys at/after the
// boundary (the spanning segment becomes longer). Removal uses [start,end):
// the end survives at start. A layer reference inside the cut refuses it.
bool insertAnimationTime(AnimationClipAsset &,float at,float length,std::string &);
bool removeAnimationTime(AnimationClipAsset &,float start,float end,std::string &);
// Copies exact cropped curves including boundary samples, all layers and cues.
// Inserts the excerpt plus one display frame so both boundary poses survive.
// Source curves and IDs survive; pasted keys/cues receive fresh identities.
bool duplicateAnimationRange(AnimationClipAsset &,float start,float end,float at,std::string &);
// Translates authored keys/cues/reference markers in [start,end), including
// the terminal endpoint when end==duration. Rejects crossing unselected keys.
bool moveAnimationRange(AnimationClipAsset &,float start,float end,float at,std::string &);
bool transformAnimationCueTimes(AnimationClipAsset &,std::span<const u64>,double pivot,double scale,double offset,std::string &);
bool eraseAnimationCues(AnimationClipAsset &,std::span<const u64>,std::string &);
class AnimationCueClipboard {
public:
  bool copy(const AnimationClipAsset &,std::span<const u64>,std::string &);
  bool paste(AnimationClipAsset &,float at,std::vector<u64> &selection,std::string &) const;
  bool empty() const {return cues_.empty();}
  usize size() const {return cues_.size();}
private:
  std::vector<AnimationCue> cues_;
  float first_=0;
};
}

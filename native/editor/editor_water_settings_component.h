#pragma once
#include "editor/editor_document.h"
#include <array>
namespace ae::editor {
// Compatibility payload for historical global water settings, not the new water API.
// Only the archive/property adapters use the historical numeric IDs.
class LegacyWaterSettings final : public EditorComponentValue {
public:
  bool enabled=false,spectrumEnabled=false,layoutEnabled=false;
  float waveHeight=3;
  float waveSpeed=1;
  float steepness=1;
  float microWaves=1.6f;
  float opacity=.72f;
  float absorption=1;
  float foam=1.05f;
  float roughness=.14f;
  float turbidity=.1f;
  float ior=1.333f;
  float directionDegrees=0;
  float level=0;
  float density=1400;
  float windSpeed=10;
  float fetch=100000;
  float depth=20;
  float swell=.8f;
  float spread=.2f;
  float damping=.1f;
  float crossWindSpeed=0;
  float crossDirection=65;
  float crossFetch=100000;
  float crossSwell=1;
  float crossSpread=.1f;
  float crossWeight=.35f;
  float displacement1=1;
  float displacement2=1;
  float displacement3=1;
  float choppiness1=1;
  float choppiness2=1;
  float choppiness3=1;
  float foamThreshold=.8f;
  float foamGrowth=4;
  float foamDecay=.5f;
  float cascadeCount=3;
  float resolutionLog2=7;
  float minimumWavelength=2;
  float maximumWavelength=2048;
  float seed=1;
  float domainScale=1;
  float budgetMiB=8;
  float displacement4=1;
  float choppiness4=1;
  static const EditorComponentType descriptor;
  const EditorComponentType &type() const override {return descriptor;}
  std::unique_ptr<EditorComponentValue> clone() const override {return std::make_unique<LegacyWaterSettings>(*this);}
  static constexpr u32 FieldCount=43;
  const float &legacyField(u32 index) const {
    switch(index) {
      case 0: return waveHeight;
      case 1: return waveSpeed;
      case 2: return steepness;
      case 3: return microWaves;
      case 4: return opacity;
      case 5: return absorption;
      case 6: return foam;
      case 7: return roughness;
      case 8: return turbidity;
      case 9: return ior;
      case 10: return directionDegrees;
      case 11: return level;
      case 12: return density;
      case 13: return windSpeed;
      case 14: return fetch;
      case 15: return depth;
      case 16: return swell;
      case 17: return spread;
      case 18: return damping;
      case 19: return crossWindSpeed;
      case 20: return crossDirection;
      case 21: return crossFetch;
      case 22: return crossSwell;
      case 23: return crossSpread;
      case 24: return crossWeight;
      case 25: return displacement1;
      case 26: return displacement2;
      case 27: return displacement3;
      case 28: return choppiness1;
      case 29: return choppiness2;
      case 30: return choppiness3;
      case 31: return foamThreshold;
      case 32: return foamGrowth;
      case 33: return foamDecay;
      case 34: return cascadeCount;
      case 35: return resolutionLog2;
      case 36: return minimumWavelength;
      case 37: return maximumWavelength;
      case 38: return seed;
      case 39: return domainScale;
      case 40: return budgetMiB;
      case 41: return displacement4;
      case 42: return choppiness4;
      default: return waveHeight;
    }
  }
  float &legacyField(u32 index) {return const_cast<float&>(static_cast<const LegacyWaterSettings&>(*this).legacyField(index));}
  bool valid() const override {
    for(u32 i=0;i<FieldCount;++i) if(!std::isfinite(legacyField(i))) return false;
    return minimumWavelength<maximumWavelength;
  }
  bool defaults() const {
    if(enabled||spectrumEnabled||layoutEnabled) return false;
    const LegacyWaterSettings value;
    for(u32 i=0;i<FieldCount;++i) if(legacyField(i)!=value.legacyField(i)) return false;
    return true;
  }
  void write(std::ostream &out) const override {
    out<<enabled<<' '<<spectrumEnabled<<' '<<layoutEnabled;
    for(u32 i=0;i<FieldCount;++i) out<<' '<<legacyField(i);
  }
  bool read(std::istream &in,u32 version) override {
    if(version!=1 || !(in>>enabled>>spectrumEnabled>>layoutEnabled)) return false;
    for(u32 i=0;i<FieldCount;++i) if(!(in>>legacyField(i))) return false;
    return true;
  }
};
inline const EditorComponentType LegacyWaterSettings::descriptor{
  "astra.legacy.water-settings",1,[]()->std::unique_ptr<EditorComponentValue>{return std::make_unique<LegacyWaterSettings>();}
};
inline const LegacyWaterSettings &waterSettings(const EditorEntity &e) {
  static const LegacyWaterSettings defaults;
  const auto *value=static_cast<const LegacyWaterSettings*>(e.components.find(LegacyWaterSettings::descriptor));
  return value?*value:defaults;
}
inline LegacyWaterSettings *editWaterSettings(EditorEntity &e) {
  return static_cast<LegacyWaterSettings*>(e.components.edit(LegacyWaterSettings::descriptor));
}
inline void pruneDefaultWaterSettings(EditorEntity &e) {if(waterSettings(e).defaults()) e.components.remove(LegacyWaterSettings::descriptor);}
}

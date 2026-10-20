#pragma once
#include "resources/terrain_surface_asset.h"
#include <algorithm>
#include <cmath>
namespace ae::resources {
enum class TerrainSnowCoverageCorrespondence : u32 { Off, ExactSimpleInputs, ExactRichInputs, EstimateSimple, EstimateRich };
struct TerrainSnowCoverageInputs {
  float worldHeight=0;
  std::array<float,3> normalWorld{0,1,0};
  float materialHeight=0,occlusion=1,weightedParticipation=1,puddleHeight=0;
  std::array<float,2> maskRG{1,0};
  bool maskAvailable=false,materialInputsAvailable=false,normalMatchesVisual=false;
  TerrainSnowControllerState controller;
};
struct TerrainSnowCoverageResult {
  float coverage=0,fade=0,heightClearing=0,erosionClearing=0;
  TerrainSnowCoverageCorrespondence correspondence=TerrainSnowCoverageCorrespondence::Off;
};
// Reference capacity: Unity SG17 Dot Product / Sample Texture 2D. This pure
// adaptation follows microsplat_func_snow.txt SnowFade + DoSnow coverage.
// A scene query must report actual input correspondence, not infer it from
// successful arithmetic. Missing enabled masks/controllers fail atomically.
inline bool evaluateTerrainSnowCoverage(const TerrainSnowSettings &s,const TerrainSnowCoverageInputs &input,TerrainSnowCoverageResult &out){
  if(u32(s.mode)>2)return false;
  TerrainSnowCoverageResult next;if(s.mode==TerrainSnowMode::Off){out=next;return true;}
  const auto finite=[](float value){return std::isfinite(value);};
  for(float value:s.upVector)if(!finite(value))return false;
  for(float value:s.heightAngleRange)if(!finite(value))return false;
  for(float value:s.parameters)if(!finite(value))return false;
  for(float value:input.normalWorld)if(!finite(value))return false;
  if(!finite(input.worldHeight)||!finite(input.materialHeight)||!finite(input.occlusion)||!finite(input.weightedParticipation)||!finite(input.puddleHeight)||!finite(s.amount))return false;
  double level=s.amount,start=s.heightAngleRange[0],end=s.heightAngleRange[1];
  if(s.useGlobalLevel||s.useGlobalHeight){
    if(!input.controller.available)return false;
    if(s.useGlobalLevel){if(!finite(input.controller.level))return false;level=input.controller.level;}
    if(s.useGlobalHeight){if(!finite(input.controller.heightStart)||!finite(input.controller.heightEnd))return false;start=input.controller.heightStart;end=input.controller.heightEnd;}
  }
  if(level<0||level>1||input.weightedParticipation<0||input.weightedParticipation>1)return false;
  double upLength=0,normalLength=0,dot=0;
  for(usize i=0;i<3;++i){upLength+=double(s.upVector[i])*s.upVector[i];normalLength+=double(input.normalWorld[i])*input.normalWorld[i];dot+=double(s.upVector[i])*input.normalWorld[i];}
  if(upLength<1e-12||!std::isfinite(upLength)||std::abs(normalLength-1)>1e-3)return false;
  dot/=std::sqrt(upLength); // same direction normalization as the production shader
  const bool rich=s.mode==TerrainSnowMode::Rich;
  if(rich&&s.perLayerEnabled)level*=input.weightedParticipation;
  const auto clamp=[](double value){return std::clamp(value,0.0,1.0);};
  const double snowDot=std::max(level*.5,dot);
  double angle=std::max(0.0,(snowDot-s.heightAngleRange[2])*6.0);
  angle*=1.0-std::max(0.0,(snowDot-s.heightAngleRange[3])*6.0);
  double fade=clamp(level*clamp((input.worldHeight-start)/std::max(end-start,.001))*clamp(angle)-input.puddleHeight);
  if(s.maskEnabled){
    if(!input.maskAvailable||!finite(input.maskRG[0])||!finite(input.maskRG[1])||input.maskRG[0]<0||input.maskRG[0]>1||input.maskRG[1]<0||input.maskRG[1]>1)return false;
    fade=std::max(double(input.maskRG[1]),std::min(double(input.maskRG[0]),fade));
  }
  const double materialHeight=rich?input.materialHeight:0,ao=rich?input.occlusion:1;
  const double height=clamp(materialHeight-(1.0-s.parameters[0]));
  const double erosionBase=clamp(ao*s.parameters[1]),erosion=erosionBase*erosionBase;
  const double mask=clamp(fade-erosion-height);
  next.coverage=float(clamp(mask*mask*mask*clamp(snowDot-(height+erosion)*.5)*8));
  next.fade=float(fade);next.heightClearing=float(height);next.erosionClearing=float(erosion);
  // Puddle clearing belongs to the pending puddle module. Preserve the source
  // formula, but never claim correspondence to a GPU consumer that is absent.
  const bool matchesVisual=input.normalMatchesVisual&&input.puddleHeight==0;
  next.correspondence=rich?(matchesVisual&&input.materialInputsAvailable?TerrainSnowCoverageCorrespondence::ExactRichInputs:TerrainSnowCoverageCorrespondence::EstimateRich):
    (matchesVisual?TerrainSnowCoverageCorrespondence::ExactSimpleInputs:TerrainSnowCoverageCorrespondence::EstimateSimple);
  out=next;return true;
}
} // namespace ae::resources

#pragma once
#include "resources/terrain_surface_asset.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace ae::resources {
// Pure scalar coverage proposal. Texture sampling, painting, dynamic simulation,
// animation/refraction, foam, normal and BRDF are separate producers/consumers.
enum class TerrainFlowCoverageCorrespondence : u32 { Off, EstimateStaticInputs, EstimateDynamicInputs };
struct TerrainFlowCoverageInputs {
  std::array<float,4> staticRgba{0,0,0,0},dynamicRgba{0,0,0,0};
  bool staticAvailable=false,dynamicAvailable=false;
  bool sampledMaskMatchesVisual=false,materialInputsMatchVisual=false,dynamicTimeMatchesVisual=false;
  float worldHeight=0,cameraDistance=0,timeSeconds=0,materialHeight=0;
  float traxBuffer=1;
  bool tracksAvailable=false;
  TerrainFlowControllerState controller;
};
struct TerrainFlowCoverageResult {
  std::array<float,4> rawStatic{},rawDynamic{},processed{},heightFiltered{};
  float dynamicWetTrail=0,dynamicBurnTrail=0,wetness=0,puddle=0,stream=0,lava=0;
  float wetBlend=0,lavaSnowClearing=0,sourceSnowClearing=0,snowClearing=0,specularHeightFactor=1,rainIntensity=0;
  float streamHeightFactor=1,lavaHeightFactor=1;
  bool sampledInputsConfirmed=false,materialInputsConfirmed=false,dynamicTimeConfirmed=false;
  // No Exact enum until the actual visual and CPU producers share a contract.
  TerrainFlowCoverageCorrespondence correspondence=TerrainFlowCoverageCorrespondence::Off;
};
inline bool validTerrainFlowCoverageSettings(const TerrainFlowSettings &s,std::string &error) {
  const auto fail=[&](const char *message){error=message;return false;};
  const auto bounded=[](float x,float low,float high){return std::isfinite(x)&&x>=low&&x<=high;};
  if(u32(s.sourceRainMode)>2)return fail("Invalid source RainMode");
  for(const auto *v:{&s.wetnessParams,&s.puddleParams,&s.heightWetnessParams,&s.distanceWetnessParams,&s.streamFlowParams,&s.streamNormalFoam,
      &s.streamUvScales,&s.streamTint,&s.streamFades,&s.lavaParams,&s.lavaParams2,&s.lavaEdgeColor,&s.lavaColorLow,&s.lavaColorHighlight,
      &s.lavaUvScale,&s.lavaFades,&s.rainIntensityScale,&s.specularFades,&s.traxFxThresholds})
    for(float x:*v)if(!bounded(x,-1e8f,1e8f))return fail("Nonfinite or excessive flow vector");
  for(float x:{s.globalPorosity,s.streamBlend,s.streamMaximum,s.lavaDisplacementScale,s.lavaEmissiveMultiplier,s.lavaStochasticContrast,s.lavaStochasticSize})
    if(!bounded(x,0,1e8f))return fail("Invalid flow scalar");
  if(!bounded(s.globalPorosity,0,1)||!bounded(s.streamMaximum,0,1)||!bounded(s.wetnessParams[0],0,1)||!bounded(s.wetnessParams[1],s.wetnessParams[0],1)||
     !bounded(s.puddleParams[1],0,1)||s.puddleParams[0]<0||!bounded(s.lavaParams[1],0,1)||s.lavaParams[0]<0)
    return fail("Invalid flow amounts or blend");
  if(s.distanceWetness&&s.distanceWetnessParams[2]<=0)return fail("Distance wetness requires positive length");
  if(!bounded(s.rainIntensityScale[0],0,1)||!bounded(s.distanceWetnessParams[1],0,1)||!bounded(s.distanceWetnessParams[3],0,1))return fail("Invalid rain/wetness strengths");
  for(const auto *m:{&s.streamControl,&s.dynamicStreamControl,&s.streamNormal,&s.lavaDiffuse,&s.rainDropTexture}) {
    if(u32(m->channel)>7||!validTerrainTextureResidency(m->textureResidency)||!bounded(m->factor,0,64)||
       !bounded(m->remapMin,-1e8f,1e8f)||!bounded(m->remapMax,-1e8f,1e8f))return fail("Invalid flow map");
    for(const auto *v:{&m->colorFactor,&m->colorRemapMin,&m->colorRemapMax})for(float x:*v)if(!bounded(x,-1e8f,1e8f))return fail("Invalid flow map color transform");
  }
  for(const auto *v:{&s.simulation.evaporation,&s.simulation.strength,&s.simulation.speed,&s.simulation.resistance})for(float x:*v)if(!bounded(x,0,1e8f))return fail("Invalid flow simulation vector");
  if(!bounded(s.simulation.wetnessEvaporation,0,1e8f)||!bounded(s.simulation.burnEvaporation,0,1e8f)||
     !s.simulation.maximumEmitters||!s.simulation.maximumColliders)return fail("Invalid simulation evaporation/capacity");
  for(const auto *range:{&s.streamFades,&s.lavaFades})
    if((*range)[1]<(*range)[0]||(*range)[2]<(*range)[1]||(*range)[3]<(*range)[2])return fail("Unordered flow height range");
  if(s.specularFade&&s.specularFades[1]<s.specularFades[0])return fail("Unordered specular height range");
  for(const auto *raw:{&s.sourcePropertiesJson,&s.sourceKeywordsJson,&s.sourceTextureEnvsJson,&s.simulation.sourcePropertiesJson})
    if(raw->size()>64*1024||raw->find('\0')!=std::string::npos)return fail("Invalid flow provenance");
  error.clear();return true;
}
inline bool evaluateTerrainFlowCoverage(const TerrainFlowSettings &s,const TerrainFlowCoverageInputs &in,
    TerrainFlowCoverageResult &out,std::string &error) {
  const auto fail=[&](const char *message){error=message;return false;};
  if(!validTerrainFlowCoverageSettings(s,error))return false;
  TerrainFlowCoverageResult next;
  if(s.specularFade){
    if(!std::isfinite(in.worldHeight)||std::abs(in.worldHeight)>1e8f)return fail("Invalid specular world height");
    next.specularHeightFactor=float(std::clamp((double(in.worldHeight)-s.specularFades[0])/std::max(double(s.specularFades[1])-s.specularFades[0],.0001),0.0,1.0));
  }
  const bool active=s.wetness||s.puddles||s.streams||s.lava||s.wetnessMaskSnow;
  if(!active){out=next;error.clear();return true;}
  const auto bounded=[](float x,float low,float high){return std::isfinite(x)&&x>=low&&x<=high;};
  const auto sat=[](double x){return std::clamp(x,0.0,1.0);};
  for(float x:{in.worldHeight,in.timeSeconds,in.materialHeight})if(!bounded(x,-1e8f,1e8f))return fail("Invalid flow material/world/time input");
  if(!bounded(in.cameraDistance,0,1e8f)||!bounded(in.traxBuffer,0,1))return fail("Invalid flow distance/tracks input");
  if(!in.staticAvailable)return fail("Static FX producer is unavailable");
  for(float x:in.staticRgba)if(!bounded(x,0,1))return fail("Static FX channels must be normalized");
  next.rawStatic=in.staticRgba;next.processed=in.staticRgba;
  if(s.dynamicFlows){
    if(!in.dynamicAvailable)return fail("Dynamic FX producer is unavailable");
    for(float x:in.dynamicRgba)if(!bounded(x,0,1))return fail("Dynamic FX channels must be normalized");
    next.rawDynamic=in.dynamicRgba;next.dynamicWetTrail=in.dynamicRgba[0];next.dynamicBurnTrail=in.dynamicRgba[1];
    next.processed[2]=float(std::max(double(next.processed[2]),sat(double(in.dynamicRgba[2])*3)));
    next.processed[3]=float(std::max(double(next.processed[3]),sat(double(in.dynamicRgba[3])*3)));
  }
  double streamMax=s.streamMaximum,puddleMax=s.puddleParams[1];std::array<float,2> wet{s.wetnessParams[0],s.wetnessParams[1]};
  if((s.globalWetness||s.globalPuddles||s.globalStreams||s.globalRain)&&s.controller.valid()&&s.controller!=in.controller.identity)
    return fail("Flow controller identity does not match");
  if(s.globalStreams){if(!in.controller.streamAvailable||!bounded(in.controller.streamMaximum,0,1))return fail("Global stream controller unavailable/invalid");streamMax=in.controller.streamMaximum;}
  if(s.globalPuddles){if(!in.controller.puddleAvailable||!bounded(in.controller.puddleMaximum,0,1))return fail("Global puddle controller unavailable/invalid");puddleMax=in.controller.puddleMaximum;}
  if(s.globalWetness){wet=in.controller.wetness;if(!in.controller.wetnessAvailable||!bounded(wet[0],0,1)||!bounded(wet[1],wet[0],1))return fail("Global wetness controller unavailable/invalid");}
  if(s.globalRain){if(!in.controller.rainAvailable||!bounded(in.controller.rainIntensity,0,1))return fail("Global rain controller unavailable/invalid");next.rainIntensity=in.controller.rainIntensity;}
  else next.rainIntensity=s.rainIntensityScale[0];
  if(s.streams)next.processed[2]*=float(streamMax);
  if(s.lava)next.processed[3]*=s.lavaParams[1];
  if(s.tracksEnabled){if(!in.tracksAvailable)return fail("Tracks producer unavailable");for(usize i=0;i<4;++i)next.processed[i]=float(sat(std::max(double(next.processed[i]),double(s.traxFxThresholds[i])*(1-in.traxBuffer))));}
  next.heightFiltered=next.processed;
  if(s.wetness){double amount=std::clamp(double(next.processed[0]),double(wet[0]),double(wet[1]));
    if(s.heightWetness){double waterline=s.heightWetnessParams[0]+std::sin(double(in.timeSeconds)*s.heightWetnessParams[2])*s.heightWetnessParams[3];amount=std::max(amount,sat((waterline-in.worldHeight)*s.heightWetnessParams[1]));}
    if(s.distanceWetness){double fade=sat((double(in.cameraDistance)-s.distanceWetnessParams[0])/s.distanceWetnessParams[2]);amount*=std::lerp(double(s.distanceWetnessParams[1]),double(s.distanceWetnessParams[3]),fade);}
    next.wetness=float(amount);
  }
  if(s.puddles)next.puddle=float(sat((double(next.processed[1])*puddleMax-in.materialHeight)*s.puddleParams[0]));
  const auto heightWindow=[&](const auto &v){return sat((double(in.worldHeight)-v[0])/std::max(double(v[1])-v[0],.0001))*(1-sat((double(in.worldHeight)-v[2])/std::max(double(v[3])-v[2],.0001)));};
  if(s.streams){if(s.heightDampStreams)next.streamHeightFactor=float(heightWindow(s.streamFades));
    next.heightFiltered[2]*=next.streamHeightFactor;next.dynamicWetTrail*=next.streamHeightFactor;
    double amount=sat((double(next.heightFiltered[2])-in.materialHeight)*s.streamBlend);
    // Dynamic wet trail survives after water leaves; source only evaluates it
    // when water coverage or trail is positive.
    if(amount+next.dynamicWetTrail>0&&s.dynamicFlows)amount=std::max(amount,sat((double(next.dynamicWetTrail)*streamMax-in.materialHeight)*s.streamBlend)*.85);
    next.stream=float(amount);
  }
  next.wetBlend=std::max({next.puddle,next.stream,next.wetness});
  if(s.lava){if(s.heightDampLava)next.lavaHeightFactor=float(heightWindow(s.lavaFades));
    next.heightFiltered[3]*=next.lavaHeightFactor;next.dynamicBurnTrail*=next.lavaHeightFactor;
    next.lava=float(sat((double(next.heightFiltered[3])-in.materialHeight)*s.lavaParams[0]));next.lavaSnowClearing=float(sat(double(next.lava)*3));
  }
  // Source ordering is retained as a diagnostic/reference result. Astra's
  // chosen adaptation combines independent coverages: enabling zero Lava must
  // never erase an existing puddle. This is a fixed semantic choice, not a
  // compatibility flag without a real consumer.
  next.sourceSnowClearing=s.lava?std::max({next.stream,next.lavaSnowClearing,next.wetness}):next.wetBlend;
  next.snowClearing=std::max({next.puddle,next.stream,next.lavaSnowClearing,next.wetness});
  if(s.wetnessMaskSnow){const float inverseWet=1-next.processed[0];next.snowClearing=std::max(next.snowClearing,inverseWet);next.sourceSnowClearing=std::max(next.sourceSnowClearing,inverseWet);}
  if(s.specularFade)next.specularHeightFactor=float(sat((double(in.worldHeight)-s.specularFades[0])/std::max(double(s.specularFades[1])-s.specularFades[0],.0001)));
  for(float x:{next.wetness,next.puddle,next.stream,next.lava,next.wetBlend,next.sourceSnowClearing,next.snowClearing,next.specularHeightFactor,next.rainIntensity})if(!std::isfinite(x))return fail("Nonfinite flow coverage result");
  next.sampledInputsConfirmed=in.sampledMaskMatchesVisual;next.materialInputsConfirmed=in.materialInputsMatchVisual;next.dynamicTimeConfirmed=s.dynamicFlows&&in.dynamicTimeMatchesVisual;
  next.correspondence=s.dynamicFlows?TerrainFlowCoverageCorrespondence::EstimateDynamicInputs:TerrainFlowCoverageCorrespondence::EstimateStaticInputs;
  out=next;error.clear();return true;
}
} // namespace ae::resources

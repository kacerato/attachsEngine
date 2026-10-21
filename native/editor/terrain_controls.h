#pragma once
#include "editor/editor_terrain_noise_controls.h"
#include "editor/editor_terrain_modules_controls.h"
#include "editor/editor_terrain_snow_controls.h"
#include "editor/editor_terrain_fx_controls.h"
#include <algorithm>
#include <cmath>

namespace ae::editor {
// The visual scale is shared by drawing and pointer routing. Exact entry
// remains available for world coordinates, seeds and values beyond the
// practical scale.
struct TerrainControlRange {
  float low = 0, high = 1, step = 0;
  bool enabled = false, quadratic = false;
  float value(float t) const {
    t = std::clamp(t, 0.f, 1.f);
    float v = std::lerp(low, high, quadratic ? t * t : t);
    if (step > 0)
      v = low + std::round((v - low) / step) * step;
    return std::clamp(v, low, high);
  }
  float position(float v) const {
    float t = high > low ? std::clamp((v - low) / (high - low), 0.f, 1.f) : 0;
    return quadratic ? std::sqrt(t) : t;
  }
};
template <class State>
TerrainControlRange terrainControlRange(const State &s, unsigned field) {
  const auto &r = s.terrainDraft;
  TerrainControlRange range{0, 1, 0, true, false};
  if(terrainFlowField(field)) {
    if(!terrainFlowNumberAvailable(s,field))return {0,1,0,false};
    const auto &flow=r.surface.flow;
    if(field==429)return {0,1,0,true};
    if(field==430)return {-512,512,0,true};
    if(field==431){const float high=flow.specularFades[1];return {std::min(-2048.f,high),std::min(2048.f,high),0,true};}
    if(field==432){const float low=flow.specularFades[0];return {std::max(-2048.f,low),std::max(2048.f,low),0,true};}
    if((field>=381 && field<=384) || (field>=412 && field<=415)){
      const auto &fades=field>=412?flow.lavaFades:flow.streamFades;const u32 i=field>=412?field-412:field-381;
      const float low=i?fades[i-1]:std::min(-2048.f,fades[0]),high=i<3?fades[i+1]:std::max(2048.f,fades[3]);return {low,high,0,true};
    }
    if(field==368 || field==374 || field==391)return {0,1,0,true};
    if(field==385)return {0,2,0,true};
    if(field==369 || field==390)return {0,64,0,true,true};
    if(field==375 || field==397)return {0,32,0,true,true};
    if(field==376 || field==377 || field==399 || field==400)return {-128,128,0,true};
    if(field==371 || field==373 || field==393 || field==395)return {-2,2,0,true};
    if(field==370 || field==372 || field==392 || field==394 || field==396)return {-4,4,0,true};
    if(field==402)return {.0001f,8,0,true,true};
    if(field==401 || field==398 || field==416)return {0,8,0,true,true};
    if((field>=378 && field<=380) || (field>=403 && field<=411))return {0,4,0,true};
    if(field>=417 && field<=420)return {0,4,0,true};
    return {-1,2,0,true};
  }
  if(terrainStaticFlowField(field)) {
    if(!terrainStaticFlowNumberAvailable(s,field))return {0,1,0,false};
    const auto &flow=r.surface.flow;
    if(field==336)return {0,flow.wetnessParams[1],0,true};
    if(field==337)return {flow.wetnessParams[0],1,0,true};
    if(field==338)return {-128,128,0,true};
    if(field==339 || field==340)return {-8,8,0,true};
    if(field==341)return {-16,16,0,true};
    if(field==342){const float high=std::nextafter(flow.distanceWetnessParams[0]+flow.distanceWetnessParams[2],-INFINITY);return {std::min(-2048.f,high),std::min(2048.f,high),0,true};}
    if(field==343){const float low=std::nextafter(flow.distanceWetnessParams[0],INFINITY);return {low,std::max(2048.f,low),0,true};}
    if(field==346)return {0,16,0,true,true};
    return range;
  }
  if(terrainFxField(field)) {
    if(!terrainFxNumberAvailable(s,field))return {0,1,0,false};
    if(field==321)return {.05f,64,0,true,true};
    if(field==322)return {0,64,0,true,true};
    if(field==323)return {.01f,8,0,true};
    if(field==326)return {0,float(s.terrainFx.brush.slopeMax),0,true};
    if(field==327)return {float(s.terrainFx.brush.slopeMin),1,0,true};
    if(field==328)return {.05f,8,0,true};
    if(field==329)return {-180,180,0,true};
    return range;
  }
  if(terrainSnowField(field)) {
    if(!terrainSnowNumberAvailable(s,field))return {0,1,0,false};
    const auto &snow=r.surface.snow;
    if(field>=260 && field<=262)return {-1,1,0,true};
    if(field==263 || field==264)return {-128,128,0,true};
    if(field==265){const float high=std::min(2048.f,std::nextafter(snow.heightAngleRange[1],-INFINITY));return {std::min(-2048.f,high),high,0,true};}
    if(field==266){const float low=std::nextafter(snow.heightAngleRange[0],INFINITY);return {low,std::max(2048.f,low),0,true};}
    if(field==267)return {-1,snow.heightAngleRange[3],0,true};
    if(field==268)return {snow.heightAngleRange[2],1,0,true};
    if(field==273 || field==275)return {-8,8,0,true};
    if(field==274 || field==304)return {0,8,0,true};
    if(field==277)return {0,std::min(2048.f,std::nextafter(snow.distanceResample[3],0.f)),0,true,true};
    if(field==278){const float low=std::nextafter(snow.distanceResample[2],INFINITY);return {low,std::max(2048.f,low),0,true,true};}
    if(field==279)return {.5f,1.5f,0,true};
    if(field==280)return {.01f,.99f,0,true};
    if(field==290)return {.1f,10,0,true};
    if(field==294 || field==295)return {0,2,0,true};
    if(field>=296 && field<=298)return {0,10,0,true};
    if(field==299)return {0,64,0,true};
    return range;
  }

  if ((field >= 64 && field <= 76) || (field >= 96 && field <= 110)) {
    range.enabled = s.terrainBrush.layer < r.surface.layers.size();
    if (field == 64 || field == 65)
      range = {.01f, 2048, 0, range.enabled, true};
    else if (field == 66 || field == 67)
      range = {-2048, 2048, 0, range.enabled};
    else if (field == 68)
      range = {-180, 180, 0, range.enabled};
    else if (field == 72)
      range = {0, 4, 0, range.enabled};
    else if (field == 73)
      range = {.1f, 32, 0, range.enabled, true};
    else if (field == 74 || field == 75)
      range = {.01f, 16, 0, range.enabled, true};
    else if (field == 76)
      range = {0, 8, 0, range.enabled};
    else if (field == 96)
      range = {0, 8, 0, range.enabled};
    else
      range = {0, 1, 0, range.enabled};
    return range;
  }
  if(terrainModuleField(field)) {
    range.enabled=terrainModuleNumberAvailable(s,field);
    if(!range.enabled || field==255) return {0,1,0,false};
    const auto &geo=r.surface.geo;
    if(field==193 || field==199) return {.0001f,2048,0,true,true};
    if(field==194 || field==200 || field==219 || field==220) return {-2048,2048,0,true};
    if(field==201) return {-180,180,0,true};
    if(field==202) return {0,32,0,true,true};
    if(field==204) return {-1,1,0,true};
    if(field==192 || field==212 || field==215 || field==216 || field==222) return {0,8,0,true};
    if(field==221 && terrainModuleGlobal(s.terrainMaterialView)!=0) return {0,2,0,true};
    if(field==217 || field==218) return {-8,8,0,true};
    if(field>=195 && field<=198) {
      const auto i=field-195;
      return {i?geo.range[i-1]:std::min(-2048.f,geo.range[0]),i<3?geo.range[i+1]:std::max(2048.f,geo.range[i]),0,true};
    }
    if(field==213 || field==214) {
      const auto &global=r.surface.globalMaps[terrainModuleGlobal(s.terrainMaterialView)];
      if(field==213) return {0,std::min(2048.f,std::nextafter(global.fadeEnd,0.f)),0,true,true};
      const auto low=std::nextafter(global.fadeStart,1e8f);return {low,std::max(2048.f,low),0,true,true};
    }
    if(field>=206 && field<=211) {
      const auto index=std::min<usize>(s.terrainProfilePoint,geo.curve.keys.size()-1);
      const auto &key=geo.curve.keys[index];
      if(field==206) {
        const auto low=index?std::nextafter(geo.curve.keys[index-1].time,key.time):std::min(-1.f,key.time);
        const auto high=index+1<geo.curve.keys.size()?std::nextafter(geo.curve.keys[index+1].time,key.time):std::max(2.f,key.time);
        return {low,high,0,low<=high};
      }
      if(field==207) return {-4,4,0,true};
      if(field==208 || field==209) return {-16,16,0,field==208?unsigned(key.incoming)==0:unsigned(key.outgoing)==0};
      if(field==210 || field==211) return {0,1,0,field==210?key.weightedIn:key.weightedOut};
    }
    return range;
  }
  if(terrainClusterField(field)) {
    range.enabled=terrainClusterNumberAvailable(r.surface,s.terrainBrush.layer,s.terrainMaterialView,field);
    if(field==160) return {.0001f,2048,0,range.enabled,true};
    if(field==161 || field==162) return {-2048,2048,0,range.enabled};
    if(field==163) return {-180,180,0,range.enabled};
    if(field==164) return {.0001f,8,0,range.enabled,true};
    if(field==165 || field==176) return {0,8,0,range.enabled};
    return range;
  }
  if(field>=152 && field<=154) {
    range.enabled=s.terrainMaterialView==50 && s.terrainBrush.layer<r.surface.layers.size();
    if(!range.enabled) return range;
    const auto &layer=r.surface.layers[s.terrainBrush.layer];
    if(field==152) return {.0001f,16,0,true,true};
    if(field==153) return {0,std::min(2048.f,std::nextafter(layer.resampleEnd,0.f)),0,true,true};
    const auto low=std::nextafter(layer.resampleStart,1e8f);
    return {low,std::max(2048.f,low),0,true,true};
  }
  if(field>=128 && field<=151) {
    range.enabled=terrainNoiseAvailable(r.surface,s.terrainBrush.layer,s.terrainMaterialView) ||
      (field>=149 && s.terrainMaterialView==30 && s.terrainBrush.layer<r.surface.layers.size());
    if(field==128) return {.0001f,16,0,range.enabled,true};
    if(field==129 || field==133) return {0,8,0,range.enabled};
    if(field==130 || field==131) {
      if(!range.enabled) return range;
      const auto &noise=terrainSelectedNoise(r.surface,s.terrainBrush.layer,s.terrainMaterialView);
      if(field==130) return {0,std::min(2048.f,std::nextafter(noise.fadeEnd,0.f)),0,true,true};
      const auto low=std::nextafter(noise.fadeStart,1e8f);
      return {low,std::max(2048.f,low),0,true,true};
    }
    return range;
  }
  switch (field) {
  case 1:
    range = {8, 2048, 0, true, true};
    break;
  case 2: {
    unsigned maximum = 128;
    if (unsigned(r.mode) != 0)
      while (maximum > 8 &&
             (maximum + 1ull) * (maximum + 1) * (r.verticalCells + 1) > 600000)
        maximum -= 8;
    range = {8, float(maximum), 8, true};
    break;
  }
  case 3:
    range = {0, 512, 0, true, true};
    break;
  case 4:
    range = {0, 256, 0, true, true};
    break;
  case 5:
    range = {.1f, 32, 0, true, true};
    break;
  case 6:
    range = {1, 8, 1, true};
    break;
  case 7:
    range = {
        8,
        float(std::min(
            64u, unsigned(600000ull / ((r.cells + 1ull) * (r.cells + 1)) - 1))),
        1, true};
    break;
  case 8:
    range = {.1f, 256, 0, true, true};
    break;
  case 9:
    range = {.1f, std::max(8.f, r.size * .25f), 0, true, true};
    break;
  case 10:
    range = {0, 8, 0, true, true};
    break;
  case 14:
    range = {.1f, 512, 0, true, true};
    break;
  case 15:
  case 19:
  case 24:
  case 25:
  case 36:
  case 38:
  case 39:
    break;
  case 16:
  case 22:
  case 23:
    range = {-180, 180, 0, true};
    break;
  case 17:
  case 20:
  case 21:
    range = {.1f, 10, 0, true, true};
    break;
  case 18:
    range = {.05f, 1, 0, true};
    break;
  case 26:
  case 28:
    range = {1, 4, 1, true};
    break;
  case 29:
    range = {.01f, 10, 0, r.lodLevels > 1, true};
    break;
  case 60:
    range = {8, 512, 8, unsigned(r.mode) == 0};
    break;
  case 34:
    range = {-512, 512, 0, true};
    break;
  case 37:
    range = {-2048, 2048, 0, true};
    break;
  case 35:
    range = {0, 89.9f, 0, true};
    break;
  case 32:
  case 33:
  case 40: {
    if (s.terrainPath >= r.paths.size()) {
      range.enabled = false;
      break;
    }
    const auto &p = r.paths[s.terrainPath];
    if (field == 32) {
      const float allowed =
          p.keepWidthRatio
              ? std::nextafter(
                    std::min(2048.f, 2048.f * p.width / p.shoulderWidth), .1f)
              : p.shoulderWidth;
      range = {.1f, std::min(allowed, std::max(8.f, r.size * .5f)), 0, true,
               true};
    } else if (field == 33)
      range = {p.width,
               std::max(p.width, std::min(2048.f, std::max(8.f, r.size))), 0,
               true, true};
    else
      range = {0, 512, 0, true, true};
    break;
  }
  case 50:
  case 51: {
    if (s.terrainPath >= r.paths.size()) {
      range.enabled = false;
      break;
    }
    const auto &p = r.paths[s.terrainPath];
    const auto &curve = s.terrainSection == 2 ? p.textureProfile : p.profile;
    if (curve.empty()) {
      range.enabled = false;
      break;
    }
    const auto selected =
        std::min(s.terrainProfilePoint, unsigned(curve.size() - 1));
    if (field == 51)
      range = s.terrainSection == 2 ? TerrainControlRange{0, 1, 0, true}
                                    : TerrainControlRange{-1, 2, 0, true};
    else if (selected && selected + 1 < curve.size()) {
      range = {std::nextafter(curve[selected - 1].x, 1.f),
               std::nextafter(curve[selected + 1].x, 0.f), 0, true};
      range.enabled = range.low < range.high;
    } else
      range.enabled = false;
    break;
  }
  default:
    range.enabled = false;
    break;
  }
  return range;
}
} // namespace ae::editor

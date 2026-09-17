#pragma once
#include "renderer/frustum_visibility.h"
#include <algorithm>
#include <cmath>

namespace ae::renderer {
// An independent view owns its projection and output extent. This snapshot
// never changes the primary view, simulation clock, shadows or HZB histories.
struct RenderViewSnapshot {
  PerspectiveFrustum frustum{};
  u32 width=0,height=0;
  u64 sceneEpoch=0,sceneRevision=0,requestId=0;
  u32 cameraEntity=0;
  bool valid() const {return frustum.valid && width>0 && height>0 && requestId!=0;}
};
struct PreviewViewBudget {
  u32 maximumWidth=640,maximumHeight=360;
  u64 maximumPixels=640u*360u;
  float updatesPerSecond=15;
  bool valid() const {
    return maximumWidth>0 && maximumWidth<=4096 && maximumHeight>0 && maximumHeight<=4096 &&
      maximumPixels>0 && maximumPixels<=4096ull*4096 && std::isfinite(updatesPerSecond) &&
      updatesPerSecond>0 && updatesPerSecond<=60;
  }
};
inline bool previewViewExtent(u32 requestedWidth,u32 requestedHeight,const PreviewViewBudget &budget,
                              u32 &width,u32 &height) {
  width=height=0;
  if(!requestedWidth||!requestedHeight||!budget.valid()) return false;
  const double pixels=static_cast<double>(requestedWidth)*requestedHeight;
  const double scale=std::min({1.0,static_cast<double>(budget.maximumWidth)/requestedWidth,
    static_cast<double>(budget.maximumHeight)/requestedHeight,std::sqrt(budget.maximumPixels/pixels)});
  width=std::max(1u,static_cast<u32>(std::floor(requestedWidth*scale)));
  height=std::max(1u,static_cast<u32>(std::floor(requestedHeight*scale)));
  return static_cast<u64>(width)*height<=budget.maximumPixels;
}
}

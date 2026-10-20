#pragma once
#include "resources/terrain_surface_asset.h"
namespace ae::resources {
// Stable traversal of every authored map, including disabled features. Used by
// versioned residency transport; never omit a map because its consumer is off.
template<class S,class F> void visitTerrainSurfaceMapsV3(S &s,F f){
  f(s.detailNoise.map);f(s.distanceNoise.map);for(auto &n:s.normalNoise)f(n.map);
  f(s.geo.albedo);f(s.geo.normal);
  for(auto &g:s.globalMaps){f(g.map);f(g.smoothness);f(g.ao);f(g.metallic);f(g.mask);}
  for(auto &e:s.antiTileArray.entries){f(e.normal.map);f(e.detail.map);f(e.distance.map);}
  for(auto &l:s.layers){
    for(auto &m:l.maps)f(m);f(l.detailNoise.map);f(l.distanceNoise.map);for(auto &n:l.normalNoise)f(n.map);
    f(l.resampleNoise.map);f(l.cluster.selection.map);for(auto &v:l.cluster.variants)for(auto &m:v.maps)f(m);
  }
  auto &n=s.snow;f(n.diffuse);f(n.height);f(n.normal);f(n.smoothness);f(n.ao);f(n.normalNoiseMap);f(n.mask);f(n.sparkleNoiseMap);
}

// V3 traversal above is frozen: its order/count is the old positional tail.
template<class S,class F> void visitTerrainFlowMaps(S &s,F f){
  auto &v=s.flow;f(v.streamControl);f(v.dynamicStreamControl);f(v.streamNormal);
  f(v.lavaDiffuse);f(v.rainDropTexture);
}
template<class S,class F> void visitTerrainSurfaceMaps(S &s,F f){
  visitTerrainSurfaceMapsV3(s,f);visitTerrainFlowMaps(s,f);
}
}

// Single-sample angular transition shared by depth coverage and color shading.
// Both candidate frames have identical atlas scale, so selecting the UV before
// one textureGrad preserves derivatives and avoids sampling across atlas cells.
highp vec2 aetherImpostorViewUv(uint materialFlags, highp vec2 firstView,
                                highp vec2 secondView, mediump float blend,
                                highp vec2 fragCoord) {
  if((materialFlags&256u)==0u) return firstView;
  ivec2 cell=ivec2(fragCoord)&3;
  mediump float threshold=AETHER_LOD_BAYER_4X4[cell.y*4+cell.x];
  return threshold<blend?secondView:firstView;
}

highp vec2 aetherImpostorSafeUv(highp vec2 uv, highp vec2 dx, highp vec2 dy,
                               highp vec2 textureExtent, highp uint metadata,
                               highp float maximumLod) {
  highp vec2 grid=vec2(float((metadata>>8u)&15u),float((metadata>>12u)&15u));
  highp vec2 cellSize=exp2(-vec2(float(metadata&15u),float((metadata>>4u)&15u)))/grid;
  highp vec2 cell=floor(uv/cellSize);
  highp float rho=max(length(dx*textureExtent),length(dy*textureExtent));
  highp float lod=clamp(ceil(log2(max(rho,1.0))),0.0,maximumLod);
  highp vec2 inset=min(vec2(0.5)*exp2(lod)/textureExtent,cellSize*0.5);
  return clamp(uv,cell*cellSize+inset,(cell+1.0)*cellSize-inset);
}

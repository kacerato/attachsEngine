// Screen-space complementary coverage used by every color/depth consumer of
// a transitioning LOD pair. Keeping this in one include is a correctness
// requirement: if the coverage prepass and shade pass disagree by one sample,
// depth EQUAL turns the disagreement into a visible hole.
const float AETHER_LOD_BAYER_4X4[16]=float[16](
  0.0/16.0,8.0/16.0,2.0/16.0,10.0/16.0,
  12.0/16.0,4.0/16.0,14.0/16.0,6.0/16.0,
  3.0/16.0,11.0/16.0,1.0/16.0,9.0/16.0,
  15.0/16.0,7.0/16.0,13.0/16.0,5.0/16.0);

bool aetherLodDitherDiscard(highp vec2 fragCoord,mediump float dither) {
  if(dither==0.0) return false;
  ivec2 cell=ivec2(fragCoord)&3;
  mediump float threshold=AETHER_LOD_BAYER_4X4[cell.y*4+cell.x];
  return dither>0.0?threshold<dither:threshold>=-dither;
}

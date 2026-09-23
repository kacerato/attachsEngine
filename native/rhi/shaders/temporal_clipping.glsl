// Copyright (c) [2015] [Playdead]. MIT license:
// ../../third_party/playdead_temporal/LICENSE.txt
// Adapted from TemporalReprojection.shader at the revision in VERSION.txt.
vec3 temporalYCoCg(vec3 c) {
  return vec3(c.r*.25+c.g*.5+c.b*.25, c.r*.5-c.b*.5,
              -c.r*.25+c.g*.5-c.b*.25);
}
vec3 temporalRgb(vec3 c) {
  return max(vec3(c.x+c.y-c.z,c.x+c.z,c.x-c.y-c.z),vec3(0));
}
vec3 temporalClipAabb(vec3 lower,vec3 upper,vec3 history) {
  vec3 center=.5*(upper+lower);
  vec3 extent=.5*(upper-lower)+vec3(1e-5);
  vec3 displacement=history-center;
  vec3 normalizedDistance=abs(displacement/extent);
  float distance=max(normalizedDistance.x,max(normalizedDistance.y,normalizedDistance.z));
  return distance>1.0?center+displacement/distance:history;
}

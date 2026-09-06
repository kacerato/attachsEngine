// Shared GGX distribution and height-correlated Smith visibility. Keep highp:
// alpha squared falls below fp16 normal range for smooth dielectric surfaces.
highp float distributionGGX(highp float nh, highp float alpha) {
  highp float a2=alpha*alpha;
  highp float d=nh*nh*(a2-1.0)+1.0;
  return a2/(3.141592653589793*d*d);
}
highp float visibilitySmithGGX(highp float nv, highp float nl, highp float alpha) {
  highp float a2=alpha*alpha;
  return 0.5/max(nl*sqrt(nv*nv*(1.0-a2)+a2)+
                 nv*sqrt(nl*nl*(1.0-a2)+a2),1.0e-6);
}

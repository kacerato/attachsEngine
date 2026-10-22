// Shared GGX distribution and height-correlated Smith visibility. Keep highp:
// alpha squared falls below fp16 normal range for smooth dielectric surfaces.
highp float distributionGGX(highp float nh, highp float alpha) {
  highp float a2=alpha*alpha;
  highp float d=nh*nh*(a2-1.0)+1.0;
  return a2/(3.141592653589793*d*d);
}
// Forma do Filament (D_GGX para mediump): 1-(n.h)^2 sai de |n x h|^2, sem o
// cancelamento de 1-nh*nh perto do pico. Com n.h vindo de vetores mediump o
// pico variava em degraus de ~1e-3 e o realce do sol, em rugosidade baixa,
// virava blocos quadriculados no aparelho.
highp float distributionGGXStable(highp vec3 n, highp vec3 h, highp float nh, highp float alpha) {
  highp vec3 nxh=cross(n,h);
  highp float a=nh*alpha;
  highp float k=alpha/max(dot(nxh,nxh)+a*a,1.0e-7);
  return k*k*0.3183098861837907;
}
highp float visibilitySmithGGX(highp float nv, highp float nl, highp float alpha) {
  highp float a2=alpha*alpha;
  return 0.5/max(nl*sqrt(nv*nv*(1.0-a2)+a2)+
                 nv*sqrt(nl*nl*(1.0-a2)+a2),1.0e-6);
}

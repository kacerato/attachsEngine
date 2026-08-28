#include "material_frame.glsl"
layout(location=0) in vec3 vPosition;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec4 vTangent;
layout(location=3) in vec2 vUv;
layout(location=4) in vec3 vTint;
layout(location=0) out vec4 outColor;
const float PI=3.141592653589793;

// GGX + height-correlated Smith + Schlick. Alpha = perceptual roughness squared.
// Equations/reference: google.github.io/filament/main/filament.html, sections 4/5.
vec3 fresnel(vec3 f0,float vh) { return f0+(1-f0)*pow(1-vh,5); }
float distribution(float nh,float alpha) {
  float a2=alpha*alpha, d=nh*nh*(a2-1)+1;
  return a2/(PI*d*d);
}
float visibility(float nv,float nl,float alpha) {
  float a2=alpha*alpha;
  return .5/max(nl*sqrt(nv*nv*(1-a2)+a2)+nv*sqrt(nl*nl*(1-a2)+a2),1e-6);
}
vec3 evaluateLight(vec3 n,vec3 v,vec3 l,vec3 radiance,vec3 base,vec3 f0,float metal,float rough) {
  float nv=max(dot(n,v),1e-4),nl=max(dot(n,l),0);
  vec3 h=normalize(v+l);float nh=max(dot(n,h),0),lh=max(dot(l,h),0);
  vec3 f=fresnel(f0,lh);
  float alpha=rough*rough;
  // Burley diffuse, with Fresnel energy partition for dielectric base.
  float fd90=.5+2*rough*lh*lh;
  float fd=(1+(fd90-1)*pow(1-nl,5))*(1+(fd90-1)*pow(1-nv,5))/PI;
  return ((1-f)*(1-metal)*base*fd+distribution(nh,alpha)*visibility(nv,nl,alpha)*f)*radiance*nl;
}
vec2 environmentUv(vec3 d) {
  return vec2(atan(d.z,d.x)/(2*PI)+.5,acos(clamp(d.y,-1,1))/PI);
}
void main() {
  vec3 base=texture(MAP(0),vUv).rgb*vTint;
  vec3 arm=texture(MAP(2),vUv).rgb;
  float rough=clamp(arm.g*frame.roughnessFactor,.07,1),metal=clamp(arm.b*frame.metallicFactor,0,1);
  vec3 n=normalize(vNormal);
  vec3 t=normalize(vTangent.xyz-n*dot(n,vTangent.xyz));
  vec3 b=cross(n,t)*vTangent.w;
  vec3 detail=texture(MAP(1),vUv).xyz*2-1;
  detail.xy*=frame.normalScale;
  n=normalize(mat3(t,b,n)*normalize(detail));
  vec3 eye=cameraRotation()*vec3(0,0,-6);
  vec3 v=normalize(eye-vPosition);
  vec3 f0=mix(vec3(.04),base,metal);
  vec3 light=evaluateLight(n,v,normalize(vec3(-3,4,-4)),vec3(2.4,2.2,1.9),base,f0,metal,rough);
  light+=evaluateLight(n,v,normalize(vec3(4,1,-1)),vec3(.5,.7,1.1),base,f0,metal,rough);
  vec3 reflected=reflect(-v,n);
  vec3 env=textureLod(MAP(3),environmentUv(reflected),rough*8).rgb;
  float nv=max(dot(n,v),.001);
  vec2 brdf=texture(MAP(4),vec2(nv,rough)).rg;
  // Actual prefiltered specular environment + split-sum BRDF.
  // Diffuse ambient is deliberately a constant studio fill, not advertised as SH.
  vec3 ambient=(1-metal)*base*vec3(.06,.07,.09);
  light+=(env*(f0*brdf.x+brdf.y)+ambient)*arm.r;
  // Explicit Reinhard display mapping; AgX/bloom/exposure pipeline is a later slice.
  vec3 color=max(light*frame.exposure,vec3(0));
  color=color/(1+color);
  // Exactly one transfer conversion, including swapchain UNORM fallback.
  if(frame.encodeSrgb!=0)
    color=mix(12.92*color,1.055*pow(color,vec3(1.0/2.4))-.055,greaterThan(color,vec3(.0031308)));
  outColor=vec4(color,1);
}

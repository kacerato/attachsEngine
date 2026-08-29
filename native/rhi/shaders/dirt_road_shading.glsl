#include "dirt_road_frame.glsl"
layout(location=0) in vec3 vPosition;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec4 vTangent;
layout(location=3) in vec2 vUv0;
layout(location=4) in vec2 vUv1;
layout(location=5) in vec4 vColor;
layout(location=0) out vec4 outColor;
const float PI=3.141592653589793;
#include "environment_lighting.glsl"
vec2 selectedUv(uint slot) { return ((frame.materialFlags.y>>(slot*2))&3u)==1u?vUv1:vUv0; }
vec3 fresnel(vec3 f0,float vh) { return f0+(1-f0)*pow(1-vh,5); }
float distribution(float nh,float alpha) { float a2=alpha*alpha,d=nh*nh*(a2-1)+1;return a2/(PI*d*d); }
float visibility(float nv,float nl,float alpha) { float a2=alpha*alpha;return .5/max(
  nl*sqrt(nv*nv*(1-a2)+a2)+nv*sqrt(nl*nl*(1-a2)+a2),1e-6); }
vec3 directLight(vec3 n,vec3 v,vec3 l,vec3 radiance,vec3 base,vec3 f0,float metal,float rough) {
  float nv=max(dot(n,v),1e-4),nl=max(dot(n,l),0);vec3 h=normalize(v+l);
  float nh=max(dot(n,h),0),lh=max(dot(l,h),0);vec3 f=fresnel(f0,lh);float alpha=rough*rough;
  float fd90=.5+2*rough*lh*lh;
  float fd=(1+(fd90-1)*pow(1-nl,5))*(1+(fd90-1)*pow(1-nv,5))/PI;
  return ((1-f)*(1-metal)*base*fd+distribution(nh,alpha)*visibility(nv,nl,alpha)*f)*radiance*nl;
}
void main() {
  uint flags=frame.materialFlags.x;
  vec4 baseSample=texture(BASE_MAP,selectedUv(0));
  vec4 base=baseSample*frame.baseColorFactor*vColor;
  // MASK coverage writes depth like opaque geometry. This removes the large
  // overdraw/order artifacts caused by treating foliage cards as BLEND while
  // preserving real soft transparency for water and road decals.
  if((flags&16u)!=0u) {
    float alphaCutoff=float((frame.materialFlags.y>>8u)&255u)/255.0;
    if(base.a<alphaCutoff) discard;
  } else if((flags&1u)!=0u && base.a<(1.0/255.0)) discard;
  vec3 mr=(flags&4u)!=0u?texture(MR_MAP,selectedUv(2)).rgb:vec3(1);
  float rough=clamp(mr.g*frame.materialFactors.x,.07,1);
  float metal=clamp(mr.b*frame.materialFactors.y,0,1);
  vec3 n=normalize(vNormal);
  if((flags&2u)!=0u) {
    vec3 t=normalize(vTangent.xyz-n*dot(n,vTangent.xyz));
    vec3 b=cross(n,t)*vTangent.w;
    vec3 detail=texture(NORMAL_MAP,selectedUv(1)).xyz*2-1;
    detail.xy*=frame.materialFactors.z;n=normalize(mat3(t,b,n)*normalize(detail));
  }
  vec3 eye=frame.cameraPositionNear.xyz;
  vec3 v=normalize(eye-vPosition);
  vec3 f0=mix(vec3(.04*frame.materialFactors.w),base.rgb,metal);
  vec3 sunRadiance=environment.sunColorAngularRadius.rgb*environment.sunDirectionIntensity.w;
  vec3 color=directLight(n,v,normalize(environment.sunDirectionIntensity.xyz),sunRadiance,
                         base.rgb,f0,metal,rough);
  float nv=max(dot(n,v),0.0);
  // Diffuse indirect from the SH9 sky/ground irradiance baked in
  // tools/cook-procedural-sky.py from the same radiance the visible dome
  // and specular reflections use, so ambient color always matches what is
  // actually overhead instead of a disconnected, separately-tinted source.
  // Dividing by PI turns irradiance into the Lambertian outgoing-radiance
  // normalization (albedo/PI * E). ambientColorStrength keeps a global
  // artist tint/strength knob on top (neutral (1,1,1)/1.0 by default).
  vec3 indirect=(evaluateSkyIrradiance(n)/PI)*environment.ambientColorStrength.rgb;
  color+=(1-metal)*base.rgb*indirect*environment.ambientColorStrength.w;
  // Rough dielectrics carry almost no readable high-frequency reflection.
  // Skip that fetch coherently per material while retaining HDR reflections
  // on wet, polished and metallic surfaces.
  if(metal>.01 || rough<.65) {
    vec3 reflection=reflect(-v,n);
    vec3 specularEnvironment=environmentRadiance(reflection,rough*environment.parameters.z);
    vec3 environmentFresnel=fresnel(f0,nv);
    color+=specularEnvironment*environmentFresnel*(.25+.75*(1.0-rough))*frame.materialFactors.w;
  }
  if((flags&8u)!=0u) color+=texture(EMISSIVE_MAP,selectedUv(3)).rgb*frame.emissiveFactorStrength.rgb*frame.emissiveFactorStrength.a;
  color=toneMapEnvironment(color);
  if(frame.materialFlags.z!=0u) color=mix(12.92*color,1.055*pow(color,vec3(1.0/2.4))-.055,
                                        greaterThan(color,vec3(.0031308)));
  outColor=vec4(color,base.a);
}

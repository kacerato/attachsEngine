#include "dirt_road_frame.glsl"
layout(location=0) in vec3 vPosition;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec4 vTangent;
layout(location=3) in vec2 vUv0;
layout(location=4) in vec2 vUv1;
layout(location=5) in vec4 vColor;
layout(location=0) out vec4 outColor;
layout(constant_id=0) const uint GPU_COST_ISOLATION=0u;
const float PI=3.141592653589793;
#include "environment_lighting.glsl"
vec2 selectedUv(uint slot) { return ((frame.materialFlags.y>>(slot*2))&3u)==1u?vUv1:vUv0; }
float pow5(float value) { float squared=value*value;return squared*squared*value; }
vec3 fresnel(vec3 f0,float vh) { return f0+(1-f0)*pow5(1-vh); }
float distribution(float nh,float alpha) { float a2=alpha*alpha,d=nh*nh*(a2-1)+1;return a2/(PI*d*d); }
float visibility(float nv,float nl,float alpha) { float a2=alpha*alpha;return .5/max(
  nl*sqrt(nv*nv*(1-a2)+a2)+nv*sqrt(nl*nl*(1-a2)+a2),1e-6); }
vec3 directLight(vec3 n,vec3 v,vec3 l,vec3 radiance,vec3 base,vec3 f0,float metal,float rough) {
  float nv=max(dot(n,v),1e-4),nl=max(dot(n,l),0);vec3 h=normalize(v+l);
  float nh=max(dot(n,h),0),lh=max(dot(l,h),0);vec3 f=fresnel(f0,lh);float alpha=rough*rough;
  float fd90=.5+2*rough*lh*lh;
  float fd=(1+(fd90-1)*pow5(1-nl))*(1+(fd90-1)*pow5(1-nv))/PI;
  return ((1-f)*(1-metal)*base*fd+distribution(nh,alpha)*visibility(nv,nl,alpha)*f)*radiance*nl;
}
void main() {
  uint flags=frame.materialFlags.x;
  uint isolation=GPU_COST_ISOLATION;
  vec4 baseSample=texture(BASE_MAP,selectedUv(0));
  vec4 base=baseSample*frame.baseColorFactor*vColor;
  // Fully transparent atlas texels cannot affect the framebuffer. Rejecting
  // them before normal/PBR work preserves the exact 8-bit alpha result and is
  // particularly important for dense forest cards.
  if((flags&1u)!=0u && base.a<(1.0/255.0)) discard;
  // BaseColorOnly keeps the exact alpha/depth coverage and geometry while
  // avoiding material/lighting fetches. The branch is uniform for the full run.
  if(isolation==3u) {
    vec3 color=base.rgb;
    if((frame.materialFlags.z&1u)!=0u)
      color=mix(12.92*color,1.055*pow(color,vec3(1.0/2.4))-.055,
                greaterThan(color,vec3(.0031308)));
    outColor=vec4(color,base.a);
    return;
  }
  vec3 mr=(flags&4u)!=0u?texture(MR_MAP,selectedUv(2)).rgb:vec3(1);
  float rough=clamp(mr.g*frame.materialFactors.x,.07,1);
  float metal=clamp(mr.b*frame.materialFactors.y,0,1);
  vec3 n=normalize(vNormal);
  if((flags&2u)!=0u && isolation!=1u) {
    vec3 t=normalize(vTangent.xyz-n*dot(n,vTangent.xyz));
    vec3 b=cross(n,t)*vTangent.w;
    vec3 detail=texture(NORMAL_MAP,selectedUv(1)).xyz*2-1;
    // Normalizing before and after an orthonormal TBN is redundant. Keep the
    // final normalization (same direction, one fewer reciprocal sqrt).
    detail.xy*=frame.materialFactors.z;n=normalize(mat3(t,b,n)*detail);
  }
  vec3 eye=frame.cameraPositionNear.xyz;
  vec3 v=normalize(eye-vPosition);
  vec3 f0=mix(vec3(.04*frame.materialFactors.w),base.rgb,metal);
  vec3 sunRadiance=environment.sunColorAngularRadius.rgb*environment.sunDirectionIntensity.w;
  vec3 color=directLight(n,v,environment.sunDirectionIntensity.xyz,sunRadiance,
                         base.rgb,f0,metal,rough);
  float nv=max(dot(n,v),0.0);
  // A single global hemispherical irradiance keeps upward-facing foliage tied
  // to the sky while giving downward/vertical surfaces a neutral ground bounce.
  // This is constant-time ALU: no extra pass, draw call or texture fetch.
  float skyWeight=clamp(n.y*.5+.5,0.0,1.0);
  vec3 ambientIrradiance=mix(environment.groundColorSaturation.rgb,
                             environment.ambientColorStrength.rgb,skyWeight);
  color+=(1-metal)*base.rgb*ambientIrradiance*environment.ambientColorStrength.w;
  // Rough dielectrics carry almost no readable high-frequency reflection.
  // Skip that fetch coherently per material while retaining HDR reflections
  // on wet, polished and metallic surfaces.
  if(isolation!=2u && (metal>.01 || rough<.65)) {
    vec3 reflection=reflect(-v,n);
    vec3 specularEnvironment=environmentRadiance(reflection,rough*environment.parameters.z);
    vec3 environmentFresnel=fresnel(f0,nv);
    color+=specularEnvironment*environmentFresnel*(.25+.75*(1.0-rough))*frame.materialFactors.w;
  }
  if((flags&8u)!=0u) color+=texture(EMISSIVE_MAP,selectedUv(3)).rgb*frame.emissiveFactorStrength.rgb*frame.emissiveFactorStrength.a;
  color=toneMapEnvironment(color);
  if((frame.materialFlags.z&1u)!=0u) color=mix(12.92*color,1.055*pow(color,vec3(1.0/2.4))-.055,
                                             greaterThan(color,vec3(.0031308)));
  outColor=vec4(color,base.a);
}

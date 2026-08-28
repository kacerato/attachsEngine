#version 450
#extension GL_GOOGLE_include_directive : require
#include "material_frame.glsl"
layout(location=0) in mat4 inModel;
layout(location=4) in vec4 inTint;
layout(location=5) in vec3 inPosition;
layout(location=6) in vec3 inNormal;
layout(location=7) in vec4 inTangent;
layout(location=8) in vec2 inUv;
layout(location=0) out vec3 vPosition;
layout(location=1) out vec3 vNormal;
layout(location=2) out vec4 vTangent;
layout(location=3) out vec2 vUv;
layout(location=4) out vec3 vTint;
void main() {
  vPosition=(inModel*vec4(inPosition,1)).xyz;
  mat3 linear=mat3(inModel);
  vNormal=normalize(transpose(inverse(linear))*inNormal);
  vTangent=vec4(linear*inTangent.xyz,inTangent.w*sign(determinant(linear)));
  vUv=inUv;vTint=inTint.rgb;
  vec3 view=transpose(cameraRotation())*vPosition+vec3(0,0,6);
  vec2 xy=vec2(view.x*1.732050808/frame.aspectRatio,-view.y*1.732050808);
  gl_Position=vec4(dot(frame.surfaceTransform.xy,xy),dot(frame.surfaceTransform.zw,xy),
      (100.0*view.z-10.0)/99.9,view.z);
}

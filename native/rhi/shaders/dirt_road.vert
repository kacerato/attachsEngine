#version 450
#extension GL_GOOGLE_include_directive : require
#include "dirt_road_frame.glsl"
layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec4 inTangent;
layout(location=3) in vec2 inUv0;
layout(location=4) in vec2 inUv1;
layout(location=5) in vec4 inColor;
layout(location=6) in mat4 inModel;
layout(location=10) in vec4 inTint;
layout(location=0) out vec3 vPosition;
layout(location=1) out vec3 vNormal;
layout(location=2) out vec4 vTangent;
layout(location=3) out vec2 vUv0;
layout(location=4) out vec2 vUv1;
layout(location=5) out vec4 vColor;
void main() {
  vPosition=(inModel*vec4(inPosition,1)).xyz;
  mat3 linear=mat3(inModel);
  vNormal=normalize(transpose(inverse(linear))*inNormal);
  vTangent=vec4(normalize(linear*inTangent.xyz),inTangent.w*sign(determinant(linear)));
  vUv0=inUv0; vUv1=inUv1; vColor=inColor*inTint;
  vec3 view=transpose(dirtRoadCameraRotation())*(vPosition-frame.cameraPositionNear.xyz);
  float farPlane=uintBitsToFloat(frame.materialFlags.w);
  float nearPlane=frame.cameraPositionNear.w;
  vec2 xy=vec2(view.x*1.732050808/frame.cameraFrame.x,-view.y*1.732050808);
  gl_Position=vec4(dot(frame.surfaceTransform.xy,xy),dot(frame.surfaceTransform.zw,xy),
      (farPlane*view.z-nearPlane*farPlane)/(farPlane-nearPlane),view.z);
}

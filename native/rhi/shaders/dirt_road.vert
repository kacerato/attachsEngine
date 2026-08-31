#version 450
#extension GL_GOOGLE_include_directive : require
#include "dirt_road_frame.glsl"
layout(set=1,binding=0,std140) uniform EnvironmentLightingBlock {
  vec4 sunDirectionIntensity;
  vec4 sunColorAngularRadius;
  vec4 ambientColorStrength;
  vec4 parameters;
  vec4 skyZenithCloudCoverage;
  vec4 skyHorizonCloudDensity;
  vec4 groundColorSaturation;
  vec4 cloudLightWindSpeed;
  vec4 worldToViewRow0;
  vec4 worldToViewRow1;
  vec4 worldToViewRow2;
} environment;
layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec4 inTangent;
layout(location=3) in vec2 inUv0;
layout(location=4) in vec2 inUv1;
layout(location=5) in vec4 inColor;
layout(location=6) in mat4 inModel;
layout(location=10) in vec4 inTint;
layout(location=11) in vec4 inNormalColumn0;
layout(location=12) in vec4 inNormalColumn1;
layout(location=13) in vec4 inNormalColumn2;
layout(location=0) out vec3 vPosition;
layout(location=1) out vec3 vNormal;
layout(location=2) out vec4 vTangent;
layout(location=3) out vec2 vUv0;
layout(location=4) out vec2 vUv1;
layout(location=5) out vec4 vColor;
void main() {
  vPosition=(inModel*vec4(inPosition,1)).xyz;
  mat3 linear=mat3(inModel);
  mat3 normalMatrix=mat3(inNormalColumn0.xyz,inNormalColumn1.xyz,inNormalColumn2.xyz);
  vNormal=normalize(normalMatrix*inNormal);
  vTangent=vec4(normalize(linear*inTangent.xyz),inTangent.w*inNormalColumn0.w);
  vUv0=inUv0; vUv1=inUv1; vColor=inColor*inTint;
  vec3 relative=vPosition-frame.cameraPositionNear.xyz;
  vec3 view=vec3(dot(environment.worldToViewRow0.xyz,relative),
                 dot(environment.worldToViewRow1.xyz,relative),
                 dot(environment.worldToViewRow2.xyz,relative));
  float farPlane=uintBitsToFloat(frame.materialFlags.w);
  float nearPlane=frame.cameraPositionNear.w;
  vec2 xy=vec2(view.x*1.732050808/frame.cameraFrame.x,-view.y*1.732050808);
  gl_Position=vec4(dot(frame.surfaceTransform.xy,xy),dot(frame.surfaceTransform.zw,xy),
      (farPlane*view.z-nearPlane*farPlane)/(farPlane-nearPlane),view.z);
}

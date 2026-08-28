#version 450
// Builtin cube preview, not the PoC-A workload. World matrices come from ECS.
layout(location = 0) in mat4 inModel;
layout(location = 4) in vec4 inTint;
layout(location = 0) out vec3 vColor;
layout(location = 1) out vec2 vUv;
layout(push_constant) uniform FramePushConstants {
  float timeSeconds;
  float aspectRatio;
  float orbitYaw;
  float orbitPitch;
  vec4 surfaceTransform;
  uint materialIndex;
} frame;

const vec3 kCubeVertices[36] = vec3[](
  vec3(-1,-1, 1), vec3( 1,-1, 1), vec3( 1, 1, 1), vec3(-1,-1, 1), vec3( 1, 1, 1), vec3(-1, 1, 1),
  vec3( 1,-1,-1), vec3(-1,-1,-1), vec3(-1, 1,-1), vec3( 1,-1,-1), vec3(-1, 1,-1), vec3( 1, 1,-1),
  vec3(-1,-1,-1), vec3(-1,-1, 1), vec3(-1, 1, 1), vec3(-1,-1,-1), vec3(-1, 1, 1), vec3(-1, 1,-1),
  vec3( 1,-1, 1), vec3( 1,-1,-1), vec3( 1, 1,-1), vec3( 1,-1, 1), vec3( 1, 1,-1), vec3( 1, 1, 1),
  vec3(-1, 1, 1), vec3( 1, 1, 1), vec3( 1, 1,-1), vec3(-1, 1, 1), vec3( 1, 1,-1), vec3(-1, 1,-1),
  vec3(-1,-1,-1), vec3( 1,-1,-1), vec3( 1,-1, 1), vec3(-1,-1,-1), vec3( 1,-1, 1), vec3(-1,-1, 1)
);

const vec2 kFaceUvs[6] = vec2[](
  vec2(0, 1), vec2(1, 1), vec2(1, 0),
  vec2(0, 1), vec2(1, 0), vec2(0, 0)
);


void main() {
  // Diagnostic camera: LH, 60 degree FOV, near 0.1, far 100, orbit radius 6.
  // A camera component and viewport configuration are a separate planned slice.
  float yaw = -frame.orbitYaw, pitch = -frame.orbitPitch;
  mat3 ry = mat3(cos(yaw),0,-sin(yaw), 0,1,0, sin(yaw),0,cos(yaw));
  mat3 rx = mat3(1,0,0, 0,cos(pitch),sin(pitch), 0,-sin(pitch),cos(pitch));
  vec3 view = rx * ry * (inModel * vec4(kCubeVertices[gl_VertexIndex],1)).xyz + vec3(0,0,6);
  const float nearPlane = 0.1, farPlane = 100.0, focal = 1.732050808;
  vec2 clipXY = vec2(view.x * focal / max(frame.aspectRatio,0.001), -view.y * focal);
  gl_Position = vec4(dot(frame.surfaceTransform.xy,clipXY), dot(frame.surfaceTransform.zw,clipXY),
      (farPlane * view.z - farPlane * nearPlane) / (farPlane - nearPlane), view.z);
  vColor = inTint.rgb;
  vUv = kFaceUvs[gl_VertexIndex % 6];
}

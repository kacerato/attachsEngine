layout(push_constant) uniform FramePushConstants {
  float timeSeconds;
  float aspectRatio;
  float orbitYaw;
  float orbitPitch;
  vec4 surfaceTransform;
  uint materialIndex;
  float exposure;
  float roughnessFactor;
  float metallicFactor;
  uint encodeSrgb;
  float normalScale;
} frame;

mat3 cameraRotation() {
  float y=frame.orbitYaw, p=frame.orbitPitch;
  mat3 ry=mat3(cos(y),0,-sin(y),0,1,0,sin(y),0,cos(y));
  mat3 rx=mat3(1,0,0,0,cos(p),sin(p),0,-sin(p),cos(p));
  return ry*rx;
}

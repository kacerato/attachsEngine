layout(push_constant) uniform DirtRoadPushConstants {
  vec4 cameraFrame;              // aspect, yaw, pitch, exposure
  vec4 surfaceTransform;
  vec4 cameraPositionNear;       // xyz, near
  vec4 baseColorFactor;
  vec4 emissiveFactorStrength;   // rgb, strength
  uvec4 textureIndices;          // base, normal, metallic-roughness, emissive
  uvec4 materialFlags;           // flags, UV selectors + cutoff8, encode sRGB, far plane bits
  vec4 materialFactors;          // roughness, metallic, normal scale, specular
} frame;

mat3 dirtRoadCameraRotation() {
  float yaw=frame.cameraFrame.y, pitch=frame.cameraFrame.z;
  mat3 ry=mat3(cos(yaw),0,-sin(yaw),0,1,0,sin(yaw),0,cos(yaw));
  mat3 rx=mat3(1,0,0,0,cos(pitch),sin(pitch),0,-sin(pitch),cos(pitch));
  return ry*rx;
}

#version 450

// PoC-A (item 0.2 do plano, "Vulkan + .NET"): desenha N instâncias de um cubo
// unitário, cada uma posicionada/colorida por um registro de instância vindo
// de um vertex buffer — a lista de instâncias é montada do lado C# e cruza a
// fronteira em UM crossing (ver DotNetHost::getManagedFunctionPointer e
// NativeEntryPoints.FillInstanceBuffer), não uma chamada nativa por objeto.
// É exatamente o padrão que docs/CONVENCOES.md exige ("chamadas nativas
// sempre em lote") e que esta PoC existe para provar que não mata o
// desempenho.
layout(location = 0) in vec2 inInstancePosition; // por instância (binding 1)
layout(location = 1) in vec3 inInstanceColor;    // por instância (binding 1)

layout(location = 0) out vec3 vColor;
layout(location = 1) out vec2 vUv;

layout(push_constant) uniform FramePushConstants {
  float timeSeconds;
  float aspectRatio;
  float orbitYaw;
  float orbitPitch;
  vec4 surfaceTransform;
  uint materialIndex; // índice no array bindless (rhi/bindless_registry.h) — ver instanced.frag
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
  float angleY = frame.timeSeconds * 0.75 + frame.orbitYaw + float(gl_InstanceIndex) * 0.001;
  float angleX = frame.timeSeconds * 0.43 + frame.orbitPitch + 0.45;
  mat3 rotateY = mat3(cos(angleY), 0, -sin(angleY), 0, 1, 0, sin(angleY), 0, cos(angleY));
  mat3 rotateX = mat3(1, 0, 0, 0, cos(angleX), sin(angleX), 0, -sin(angleX), cos(angleX));
  vec3 local = rotateY * rotateX * kCubeVertices[gl_VertexIndex];

  // A primeira instância vira o cubo de inspeção central; as outras 4.999
  // continuam exercitando o crossing em lote da PoC-A como mini-cubos.
  bool hero = gl_InstanceIndex == 0;
  float scale = hero ? 0.22 : 0.0055;
  vec2 center = hero ? vec2(0.0) : inInstancePosition;
  vec2 screen = center + vec2(local.x / max(frame.aspectRatio, 0.001), local.y) * scale;
  float depth = (hero ? 0.20 : 0.65) + local.z * scale * 0.25;
  vec2 rotatedScreen = vec2(dot(frame.surfaceTransform.xy, screen),
                            dot(frame.surfaceTransform.zw, screen));
  gl_Position = vec4(rotatedScreen, depth, 1.0);
  vColor = hero ? vec3(1.0) : inInstanceColor;
  vUv = kFaceUvs[gl_VertexIndex % 6];
}

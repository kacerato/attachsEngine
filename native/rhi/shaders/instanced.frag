#version 450
#extension GL_EXT_nonuniform_qualifier : require

// Item 2.1.4 do plano ("bindless via descriptor_indexing"): em vez de um
// único `sampler2D` ligado por-objeto, o binding 0 agora é o array bindless
// inteiro do device (rhi/bindless_registry.h) — a mesma declaração de layout
// em todo pipeline bindless da engine. `nonuniformEXT` é obrigatório aqui
// porque materialIndex varia por chamada de draw (mesmo com um único draw
// instanciado nesta PoC), não é um valor uniforme conhecido em tempo de
// compilação — sem o qualificador, alguns drivers têm comportamento
// indefinido ao indexar com um valor não-uniforme.
layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(push_constant) uniform FramePushConstants {
  float timeSeconds;
  float aspectRatio;
  float orbitYaw;
  float orbitPitch;
  vec4 surfaceTransform;
  uint materialIndex;
} frame;

layout(location = 0) in vec3 vColor;
layout(location = 1) in vec2 vUv;
layout(location = 0) out vec4 outColor;

void main() {
  outColor = texture(textures[nonuniformEXT(frame.materialIndex)], vUv) * vec4(vColor, 1.0);
}

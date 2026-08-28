#version 450

layout(location = 0) in vec3 vColor;
layout(location = 1) in vec2 vUv;
layout(set = 0, binding = 0) uniform sampler2D baseTexture;
layout(location = 0) out vec4 outColor;

void main() {
  outColor = texture(baseTexture, vUv) * vec4(vColor, 1.0);
}

#version 450

layout(location=0) out highp vec2 vUv;

void main() {
  // Fullscreen triangle: cobre a tela sem vertex buffer e sem diagonal interna.
  vec2 position = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  vUv = position;
  gl_Position = vec4(position * 2.0 - 1.0, 0.0, 1.0);
}

#version 450

// Triângulo "fullscreen-trick": posição e cor vêm de tabelas indexadas por
// gl_VertexIndex, sem vertex buffer algum. Suficiente e deliberado para o
// item 5.2 do plano (shell gráfico mínimo) — a primeira prova de que o
// pipeline gráfico completo (RenderPass, Pipeline, comandos de draw)
// funciona em hardware real. Vertex buffer/upload de geometria real é
// escopo do RHI completo (Onda 3, item 7.1).
layout(location = 0) out vec3 vColor;
layout(push_constant) uniform SurfacePushConstants {
  vec4 transform;
} surface;

const vec2 kPositions[3] = vec2[](
  vec2(0.0, -0.5),
  vec2(0.5, 0.5),
  vec2(-0.5, 0.5)
);

const vec3 kColors[3] = vec3[](
  vec3(1.0, 0.0, 0.0),
  vec3(0.0, 1.0, 0.0),
  vec3(0.0, 0.0, 1.0)
);

void main() {
  vec2 position = kPositions[gl_VertexIndex];
  gl_Position = vec4(dot(surface.transform.xy, position),
                     dot(surface.transform.zw, position), 0.0, 1.0);
  vColor = kColors[gl_VertexIndex];
}

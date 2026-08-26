#version 450

// PoC-A (item 0.2 do plano, "Vulkan + .NET"): desenha N instâncias de um quad
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

// Quad unitário centrado na origem, pequeno o bastante para 5.000 instâncias
// não se sobreporem completamente numa tela cheia — não é geometria real de
// produção (isso é o RHI completo, Onda 3), é o mínimo para ter algo visível
// por instância e provar o caminho de instancing.
const vec2 kQuadVertices[6] = vec2[](
  vec2(-0.004, -0.004), vec2(0.004, -0.004), vec2(0.004, 0.004),
  vec2(-0.004, -0.004), vec2(0.004, 0.004), vec2(-0.004, 0.004)
);

void main() {
  vec2 worldPosition = kQuadVertices[gl_VertexIndex] + inInstancePosition;
  gl_Position = vec4(worldPosition, 0.0, 1.0);
  vColor = inInstanceColor;
}

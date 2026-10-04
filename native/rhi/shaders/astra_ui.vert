#version 450

// Um quad por instância, sem buffer de vértices. Os quatro cantos saem de
// gl_VertexIndex, o que economiza a montagem e o upload de uma geometria que é
// sempre a mesma — e faz a interface inteira caber em um único desenho.
//
// A instância NÃO é repassada por varyings. O fragmento relê o mesmo registro do
// buffer pelo índice: são 80 bytes que ficariam interpolando sem necessidade
// entre quatro vértices, e o custo de reler é uma leitura de cache.

struct UiInstance {
  vec4 bounds;   // x, y, largura, altura em pixels lógicos
  vec4 clip;     // recorte já intersectado, mesmo espaço
  vec4 atlas;    // retângulo no atlas, em texels
  vec4 params;   // raio, espessura do contorno, tipo, alcance do campo
  uvec4 colors;  // preenchimento, fim do degradê, contorno, reservado
  vec4 extra;
  vec4 projectX, projectY, projectOrigin;
  vec4 worldClip;
};

layout(std430, set = 0, binding = 0) readonly buffer Instances {
  UiInstance instances[];
} ui;

layout(push_constant) uniform UiPushConstants {
  // largura, altura, 1/largura, 1/altura da superfície em pixels lógicos
  vec4 surface;
  // xx, xy, yx, yy da pré-rotação do display
  vec4 surfaceTransform;
  // largura e altura do atlas de fonte, depois do atlas de ícones
  vec4 atlasSizes;
  vec4 outputFlags;
  vec4 depthSurface;
} push;

layout(location = 0) flat out uint vInstance;
layout(location = 1) out vec2 vPixel;
layout(location = 2) noperspective out vec2 vScreen;

void main() {
  vInstance = uint(gl_InstanceIndex);
  const UiInstance instance = ui.instances[gl_InstanceIndex];

  // Faixa de triângulos: 0=(0,0) 1=(1,0) 2=(0,1) 3=(1,1).
  const vec2 corner = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));
  vec2 pixel = instance.bounds.xy + corner * instance.bounds.zw;
  if (uint(instance.params.z + 0.5) == 6u) {
    // Real triangle plus a degenerate second strip triangle. Preserves the
    // hardware top-left fill rule; adjacent ImGui triangles never double-blend.
    pixel = gl_VertexIndex == 0 ? instance.bounds.xy :
            gl_VertexIndex == 1 ? instance.bounds.zw : instance.atlas.xy;
  }
  if (uint(instance.params.z + 0.5) == 3u) {
    // Linha: o quad é orientado ao longo do segmento, e não a caixa envolvente.
    // Para uma diagonal a caixa tem o dobro da área, e cada pixel a mais é um
    // fragmento que só existiria para ser descartado.
    const vec2 from = instance.atlas.xy;
    const vec2 to = instance.atlas.zw;
    const vec2 along = to - from;
    const float segmentLength = max(sqrt(dot(along, along)), 0.0001);
    const vec2 direction = along / segmentLength;
    const vec2 normal = vec2(-direction.y, direction.x);
    const float halfWidth = instance.params.y * 0.5 + 1.0;
    pixel = mix(from, to, corner.x) + normal * (corner.y * 2.0 - 1.0) * halfWidth +
            direction * (corner.x * 2.0 - 1.0) * halfWidth;
  }
  vPixel = pixel;

  // Pixels lógicos para NDC. O Y já cresce para baixo nos dois espaços, então
  // não há inversão aqui — ao contrário do caminho da cena, onde o espaço de
  // vista tem Y para cima.
  if((instance.colors.w & 1u)!=0u) {
    vec4 clip=instance.projectX*pixel.x+instance.projectY*pixel.y+instance.projectOrigin;
    vScreen=(clip.xy/clip.w*.5+.5)*push.surface.xy;
    clip.xy=vec2(dot(push.surfaceTransform.xy,clip.xy),dot(push.surfaceTransform.zw,clip.xy));
    gl_Position=clip;return;
  }
  const vec2 ndc = pixel * push.surface.zw * 2.0 - 1.0;
  vScreen=pixel;
  gl_Position = vec4(dot(push.surfaceTransform.xy, ndc),
                     dot(push.surfaceTransform.zw, ndc), 0.0, 1.0);
}

#version 450

// O fragmento resolve as três primitivas da interface e o recorte.
//
// **O recorte é por fragmento, não por scissor.** Trocar o scissor exigiria uma
// quebra de lote a cada painel; numa hierarquia com dezenas de linhas isso é a
// diferença entre uma submissão e cem. O custo é uma comparação por fragmento,
// que num tile-based renderer é barato e não toca em memória.

struct UiInstance {
  vec4 bounds;
  vec4 clip;
  vec4 atlas;
  vec4 params;   // raio, espessura do contorno, tipo, alcance do campo em texels
  uvec4 colors;
};

layout(std430, set = 0, binding = 0) readonly buffer Instances {
  UiInstance instances[];
} ui;

layout(set = 0, binding = 1) uniform sampler2D fontAtlas;  // campo de distância, R8
layout(set = 0, binding = 2) uniform sampler2D iconAtlas;  // RGBA8, alfa direto
layout(set = 0, binding = 3) uniform sampler2D previewAtlas;  // R4: prévia de texturas, RGBA8

layout(push_constant) uniform UiPushConstants {
  vec4 surface;
  vec4 surfaceTransform;
  vec4 atlasSizes;
  vec4 outputFlags;
} push;

layout(location = 0) flat in uint vInstance;
layout(location = 1) in vec2 vPixel;
layout(location = 0) out vec4 outColor;

#define UI_KIND_RECT 0u
#define UI_KIND_GLYPH 1u
#define UI_KIND_ICON 2u
#define UI_KIND_LINE 3u
#define UI_KIND_PREVIEW 4u

vec4 unpackColor(uint packed) {
  // 0xAARRGGBB, a mesma ordem do literal hexadecimal dos tokens.
  return vec4(float((packed >> 16) & 0xffu), float((packed >> 8) & 0xffu),
              float(packed & 0xffu), float((packed >> 24) & 0xffu)) / 255.0;
}

// Distância com sinal até a borda de um retângulo arredondado. Negativa dentro.
float roundedBoxDistance(vec2 pixel, vec4 bounds, float radius) {
  const vec2 halfSize = bounds.zw * 0.5;
  const float clamped = min(radius, min(halfSize.x, halfSize.y));
  const vec2 centred = abs(pixel - (bounds.xy + halfSize)) - halfSize + clamped;
  return length(max(centred, 0.0)) + min(max(centred.x, centred.y), 0.0) - clamped;
}

// A distância acima já está em pixels, então um pixel de transição é
// literalmente uma unidade. Não há fwidth aqui de propósito: a derivada de uma
// função exata seria a mesma conta com mais instruções e mais ruído nas quinas.
float coverageFromDistance(float distance) {
  return clamp(0.5 - distance, 0.0, 1.0);
}

vec4 outputColor(vec4 color) {
  if(push.outputFlags.x>0.5) color.rgb=mix(color.rgb/12.92,pow((color.rgb+0.055)/1.055,vec3(2.4)),greaterThan(color.rgb,vec3(0.04045)));
  return color;
}

void main() {
  const UiInstance instance = ui.instances[vInstance];

  if (vPixel.x < instance.clip.x || vPixel.y < instance.clip.y ||
      vPixel.x >= instance.clip.x + instance.clip.z ||
      vPixel.y >= instance.clip.y + instance.clip.w) {
    discard;
  }

  const uint kind = uint(instance.params.z + 0.5);
  const vec4 fill = unpackColor(instance.colors.x);

  if (kind == UI_KIND_LINE) {
    // Distância ponto-segmento, presa às pontas: isso arredonda as
    // extremidades, que é o que faz duas linhas de grade se encontrarem sem um
    // degrau visível no cruzamento.
    const vec2 from = instance.atlas.xy;
    const vec2 to = instance.atlas.zw;
    const vec2 along = to - from;
    const float lengthSquared = max(dot(along, along), 0.0001);
    const float t = clamp(dot(vPixel - from, along) / lengthSquared, 0.0, 1.0);
    const float toSegment = length(vPixel - (from + along * t)) - instance.params.y * 0.5;
    outColor = outputColor(vec4(fill.rgb, fill.a * coverageFromDistance(toSegment)));
    return;
  }

  if (kind == UI_KIND_GLYPH) {
    const vec2 uv = (instance.atlas.xy +
                     (vPixel - instance.bounds.xy) / instance.bounds.zw * instance.atlas.zw) /
                    push.atlasSizes.xy;
    const float field = texture(fontAtlas, uv).r;
    // Largura da transição derivada da escala, não da derivada da amostra: o
    // campo é linear por construção, então quantos texels cabem num pixel de
    // tela responde a pergunta exatamente e sem o ruído de fwidth num texto
    // pequeno, que é justamente onde o ruído apareceria.
    const float texelsPerPixel = instance.atlas.w / max(instance.bounds.w, 0.001);
    const float softness = max(texelsPerPixel / (2.0 * max(instance.params.w, 0.001)), 0.0015);
    const float alpha = clamp((field - 0.5) / softness + 0.5, 0.0, 1.0);
    outColor = outputColor(vec4(fill.rgb, fill.a * alpha));
    return;
  }

  if (kind == UI_KIND_PREVIEW) {
    // O tamanho do atlas de prévia viaja em outputFlags.zw: ele muda quando a
    // sessão recompõe o atlas, e o push constant já tem os outros dois tamanhos.
    const vec2 uv = (instance.atlas.xy +
                     (vPixel - instance.bounds.xy) / instance.bounds.zw * instance.atlas.zw) /
                    max(push.outputFlags.zw, vec2(1.0));
    vec4 color = texture(previewAtlas, uv) * fill;
    if (instance.params.x > 0.0) {
      color.a *= coverageFromDistance(
          roundedBoxDistance(vPixel, instance.bounds, instance.params.x));
    }
    outColor = outputColor(color);
    return;
  }

  if (kind == UI_KIND_ICON) {
    const vec2 uv = (instance.atlas.xy +
                     (vPixel - instance.bounds.xy) / instance.bounds.zw * instance.atlas.zw) /
                    push.atlasSizes.zw;
    const vec4 texel = texture(iconAtlas, uv);
    // Multiplicação pela cor: branco deixa o ícone intacto e qualquer outra cor
    // o tinge. É como um ícone desabilitado escurece sem precisar de uma
    // segunda cópia no atlas.
    vec4 color = texel * fill;
    if (instance.params.x > 0.0) {
      color.a *= coverageFromDistance(
          roundedBoxDistance(vPixel, instance.bounds, instance.params.x));
    }
    outColor = outputColor(color);
    return;
  }

  const float distance = roundedBoxDistance(vPixel, instance.bounds, instance.params.x);
  const float outer = coverageFromDistance(distance);
  const float gradient = clamp((vPixel.x - instance.bounds.x) /
                               max(instance.bounds.z, 0.001), 0.0, 1.0);
  vec4 color = mix(fill, unpackColor(instance.colors.y), gradient);

  const float border = instance.params.y;
  if (border > 0.0) {
    // O contorno é desenhado PARA DENTRO, como o `border` do CSS: traçá-lo
    // centrado faria a borda de um card invadir o vizinho encostado nele.
    const vec4 inner = vec4(instance.bounds.xy + border, instance.bounds.zw - border * 2.0);
    const float innerCoverage =
        coverageFromDistance(roundedBoxDistance(vPixel, inner, max(instance.params.x - border, 0.0)));
    const vec4 borderColor = unpackColor(instance.colors.z);
    // Onde o preenchimento é transparente, a borda aparece sozinha; onde não é,
    // ela é composta por cima. Um único comando pode ser fundo, borda ou os dois.
    color = mix(vec4(borderColor.rgb, borderColor.a * (outer - innerCoverage)), color,
                innerCoverage);
  }

  outColor = outputColor(vec4(color.rgb, color.a * outer));
}

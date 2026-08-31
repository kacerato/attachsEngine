#version 450

layout(push_constant) uniform RuntimeHudPushConstants {
  vec4 centerHalfSize;
  vec4 displaySize;
  vec4 surfaceTransform;
  uvec4 parameters;
} hud;

layout(location=0) in vec2 localPosition;
layout(location=0) out vec4 outColor;

uint digitBits(uint digit) {
  const uint glyphs[10]=uint[10](31599u,11415u,29671u,29647u,23497u,
                                 31183u,31215u,29257u,31727u,31695u);
  return glyphs[min(digit,9u)];
}

float digitPixel(uint digit,vec2 uv) {
  if(any(lessThan(uv,vec2(0)))||any(greaterThanEqual(uv,vec2(1)))) return 0.0;
  uvec2 cell=uvec2(min(floor(uv*vec2(3,5)),vec2(2,4)));
  // Glyph constants are encoded left-to-right from the most significant bit
  // of each 3-pixel row. Reverse the bit-column lookup once; geometry and UVs
  // remain in normal Android display order.
  uint bit=(4u-cell.y)*3u+(2u-cell.x);
  return float((digitBits(digit)>>bit)&1u);
}

void main() {
  uint kind=hud.parameters.x;
  float radius=length(localPosition);
  if(kind==0u) {
    if(radius>1.0) discard;
    float ring=smoothstep(.74,.82,radius);
    outColor=vec4(mix(vec3(.08,.12,.14),vec3(.62,.82,.73),ring),mix(.17,.42,ring));
    return;
  }
  if(kind==1u) {
    if(radius>1.0) discard;
    float edge=smoothstep(.72,1.0,radius);
    outColor=vec4(mix(vec3(.72,.92,.82),vec3(.25,.42,.35),edge),.64);
    return;
  }

  vec2 uv=localPosition*.5+.5;
  float rounded=max(abs(localPosition.x)-.88,abs(localPosition.y)-.68);
  if(rounded>.20) discard;
  vec2 content=(uv-vec2(.10,.18))/vec2(.80,.64);
  float slot=floor(content.x*3.0);
  float glyph=0.0;
  if(content.x>=0.0&&content.x<1.0&&content.y>=0.0&&content.y<1.0) {
    uint fps=min(hud.parameters.y,999u);
    uint index=uint(slot);
    uint divisor=index==0u?100u:(index==1u?10u:1u);
    bool leading=(index==0u&&fps<100u)||(index==1u&&fps<10u);
    vec2 digitUv=vec2(fract(content.x*3.0)*.82+.09,content.y);
    if(!leading) glyph=digitPixel((fps/divisor)%10u,digitUv);
  }
  outColor=mix(vec4(.015,.025,.028,.72),vec4(.55,1.0,.68,.96),glyph);
}

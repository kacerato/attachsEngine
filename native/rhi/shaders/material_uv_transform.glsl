// R4: extensão de material por desenho (transformação de UV por binding, canais,
// oclusão, normal, origem do alfa e isolamento na prévia). A entrada do desenho
// viaja nos bits 1..31 de materialFlags.z (0 = sem extensão; o bit 0 é a saída
// sRGB). Cada entrada tem 14 linhas vec4:
//   0..7  duas linhas por binding: (a b c 0) e (d e f 0), uv' = (a·u+b·v+c, d·u+e·v+f)
//   8     força da oclusão, origem (0 nenhuma, 1 metal/rug., 2 textura), canal, índice bindless
//   9     canal da rugosidade, canal do metal, origem do alfa (0 cor, 1 opaco, 2 luminância), inverter Y
//   10    dado isolado na prévia
//   11    lightmap ativo, índice bindless, intensidade, reservado
//   12    escala UV1 xy, deslocamento zw
//   13    reservada
// Precisa casar com renderer::materialExtensionEntry.
layout(std430,set=1,binding=16) readonly buffer MaterialUvTransforms { highp vec4 materialUvTransforms[]; };

const uint AETHER_MATERIAL_EXTENSION_ROWS=14u;

bool aetherHasMaterialExtension() { return (frame.materialFlags.z>>1u)!=0u; }

highp vec4 aetherMaterialRow(uint row) {
  return materialUvTransforms[((frame.materialFlags.z>>1u)-1u)*AETHER_MATERIAL_EXTENSION_ROWS+row];
}

highp vec2 aetherTransformUv(uint slot,highp vec2 uv) {
  if(!aetherHasMaterialExtension()) return uv;
  highp vec3 point=vec3(uv,1.0);
  return vec2(dot(aetherMaterialRow(slot*2u).xyz,point),dot(aetherMaterialRow(slot*2u+1u).xyz,point));
}

// Referencial do mapa normal: a textura girada guarda o detalhe nos próprios
// eixos; a rotação transposta (sem a escala) leva esse detalhe de volta aos eixos
// da tangente da malha.
highp mat2 aetherUvTangentFrame(uint slot) {
  if(!aetherHasMaterialExtension()) return mat2(1.0);
  highp vec4 first=aetherMaterialRow(slot*2u),second=aetherMaterialRow(slot*2u+1u);
  highp float scaleU=max(length(vec2(first.x,second.x)),1.0e-6);
  highp float scaleV=max(length(vec2(first.y,second.y)),1.0e-6);
  return mat2(first.x/scaleU,first.y/scaleV,second.x/scaleU,second.y/scaleV);
}

// Alfa pela origem escolhida no material: cor base, opaco ou luminância da cor.
mediump float aetherAlphaFromSource(mediump vec4 texel) {
  if(!aetherHasMaterialExtension()) return texel.a;
  uint source=uint(aetherMaterialRow(9u).z+0.5);
  if(source==1u) return 1.0;
  if(source==2u) return dot(texel.rgb,vec3(.2126,.7152,.0722));
  return texel.a;
}

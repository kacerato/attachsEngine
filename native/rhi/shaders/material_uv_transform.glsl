// R4: transformação de UV por binding (deslocamento, escala e rotação do material
// do projeto ou da instância). A entrada do desenho viaja nos bits 1..31 de
// materialFlags.z (0 = sem transformação; o bit 0 é a saída sRGB). Cada entrada
// guarda duas linhas vec4 por binding: (a b c 0) e (d e f 0), com
// uv' = (a·u + b·v + c, d·u + e·v + f). Precisa casar com
// renderer::materialUvTransformEntry.
layout(std430,set=1,binding=16) readonly buffer MaterialUvTransforms { highp vec4 materialUvTransforms[]; };

highp vec2 aetherTransformUv(uint slot,highp vec2 uv) {
  uint entry=frame.materialFlags.z>>1u;
  if(entry==0u) return uv;
  uint row=(entry-1u)*8u+slot*2u;
  highp vec3 point=vec3(uv,1.0);
  return vec2(dot(materialUvTransforms[row].xyz,point),dot(materialUvTransforms[row+1u].xyz,point));
}

// Referencial do mapa normal: a textura girada guarda o detalhe nos próprios
// eixos; a rotação transposta (sem a escala) leva esse detalhe de volta aos eixos
// da tangente da malha.
highp mat2 aetherUvTangentFrame(uint slot) {
  uint entry=frame.materialFlags.z>>1u;
  if(entry==0u) return mat2(1.0);
  uint row=(entry-1u)*8u+slot*2u;
  highp vec4 first=materialUvTransforms[row],second=materialUvTransforms[row+1u];
  highp float scaleU=max(length(vec2(first.x,second.x)),1.0e-6);
  highp float scaleV=max(length(vec2(first.y,second.y)),1.0e-6);
  return mat2(first.x/scaleU,first.y/scaleV,second.x/scaleU,second.y/scaleV);
}

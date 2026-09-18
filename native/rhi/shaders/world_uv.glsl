// Projeção por metro, independente da escala da instância. As três direções
// usam a mesma densidade; a face oposta inverte U para manter a orientação.
// UVs glTF continuam intactas quando o material não escolhe Mundo (m).
highp vec2 aetherWorldUv(highp vec3 p, mediump vec3 normal) {
  mediump vec3 a=abs(normal);
  if(a.x>=a.y && a.x>=a.z) return vec2(-sign(normal.x)*p.z,p.y);
  if(a.y>=a.z) return vec2(p.x,-sign(normal.y)*p.z);
  return vec2(sign(normal.z)*p.x,p.y);
}

mediump mat3 aetherWorldUvBasis(mediump vec3 normal) {
  mediump vec3 a=abs(normal);
  if(a.x>=a.y && a.x>=a.z)
    return mat3(vec3(0,0,-sign(normal.x)),vec3(0,1,0),vec3(sign(normal.x),0,0));
  if(a.y>=a.z)
    return mat3(vec3(1,0,0),vec3(0,0,-sign(normal.y)),vec3(0,sign(normal.y),0));
  return mat3(vec3(sign(normal.z),0,0),vec3(0,1,0),vec3(0,0,sign(normal.z)));
}

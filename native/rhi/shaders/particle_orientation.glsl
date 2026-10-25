// Orientação 3D das partículas (G6a), incluída por particle_billboard.vert e
// particle_mesh.vert.
//
// A instância traz um quatérnio (giro próprio da partícula e, no alinhamento
// Local, a rotação do objeto). A base do alinhamento entra depois dele:
//   Vista      — direita, cima e o sentido da câmera (a frente +Z da malha
//                encara quem olha, como a Unity refletida em Z);
//   Mundo/Local — nenhuma (o quatérnio já está em mundo);
//   Velocidade — a frente -Z segue o movimento.
const uint kParticleAlignmentView = 0u;
const uint kParticleAlignmentVelocity = 3u;

vec3 particleRotate(vec4 q, vec3 v) {
  vec3 t = 2.0 * cross(q.xyz, v);
  return v + q.w * t + cross(q.xyz, t);
}

mat3 particleAlignmentBasis(uint alignment, vec3 right, vec3 up, vec3 toCamera, vec3 velocity) {
  if(alignment == kParticleAlignmentView) return mat3(right, up, toCamera);
  if(alignment == kParticleAlignmentVelocity) {
    float speed = length(velocity);
    vec3 forward = speed > 1.0e-5 ? velocity / speed : -toCamera;
    vec3 reference = abs(forward.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 side = normalize(cross(reference, forward));
    vec3 lift = cross(forward, side);
    return mat3(side, lift, -forward);
  }
  return mat3(1.0);
}

#pragma once
#include "core/base.h"
#include "renderer/frustum_visibility.h"
#include "renderer/hzb_visibility.h"

#include <cmath>

// O CONTRATO DE CÂMERA DA VISTA — um lugar só.
//
// A mesma projeção aparece hoje no vértice da cena, no céu, no passe da grade,
// na seleção por toque e na oclusão. Enquanto cada cópia era escrita à mão, uma
// delas divergiu e o defeito levou três rodadas de medição no aparelho para ser
// encontrado (ver docs/runtime-gameplay.md §15). Este cabeçalho é a referência
// que as outras espelham, e os testes de host comparam contra ele.
//
// Convenção: espaço de vista com X à direita, Y para cima, Z PARA FRENTE e
// positivo. A base mundo→vista é ortonormal, então a inversa é a transposta.
//
//   view   = B · (mundo − câmera)
//   xy     = (view.x/tanH, view.y/tanV)      [tanH = tanV · proporção]
//   ndc    = M · (xy.x, −xy.y) / view.z      [M = pré-rotação do display]
//   clip.z = (far·view.z − near·far)/(far − near),  clip.w = view.z
//
// O caminho inverso — de um pixel para o raio que o atravessa — é
// `cameraRayFromNdc`.
namespace ae::renderer {

struct CameraViewBasis final {
  // Linhas da matriz mundo→vista.
  float row0[3]{1, 0, 0};
  float row1[3]{0, 1, 0};
  float row2[3]{0, 0, 1};
};

inline CameraViewBasis buildCameraViewBasis(float yaw, float pitch,float roll=0) noexcept {
  const float cosineYaw = std::cos(yaw), sineYaw = std::sin(yaw);
  const float cosinePitch = std::cos(pitch), sinePitch = std::sin(pitch);
  CameraViewBasis basis{};
  basis.row0[0] = cosineYaw;            basis.row0[1] = 0.0f;         basis.row0[2] = -sineYaw;
  basis.row1[0] = sinePitch * sineYaw;  basis.row1[1] = cosinePitch;  basis.row1[2] = sinePitch * cosineYaw;
  basis.row2[0] = cosinePitch * sineYaw; basis.row2[1] = -sinePitch;  basis.row2[2] = cosinePitch * cosineYaw;
  const float c=std::cos(roll),s=std::sin(roll);
  for(u32 i=0;i<3;++i) {
    const float x=basis.row0[i],y=basis.row1[i];
    basis.row0[i]=c*x+s*y;basis.row1[i]=-s*x+c*y;
  }
  return basis;
}

inline void cameraWorldToView(const CameraViewBasis &basis, const float delta[3],
                              float outView[3]) noexcept {
  outView[0] = basis.row0[0] * delta[0] + basis.row0[1] * delta[1] + basis.row0[2] * delta[2];
  outView[1] = basis.row1[0] * delta[0] + basis.row1[1] * delta[1] + basis.row1[2] * delta[2];
  outView[2] = basis.row2[0] * delta[0] + basis.row2[1] * delta[1] + basis.row2[2] * delta[2];
}

inline void cameraViewToWorld(const CameraViewBasis &basis, const float view[3],
                              float outWorld[3]) noexcept {
  outWorld[0] = basis.row0[0] * view[0] + basis.row1[0] * view[1] + basis.row2[0] * view[2];
  outWorld[1] = basis.row0[1] * view[0] + basis.row1[1] * view[1] + basis.row2[1] * view[2];
  outWorld[2] = basis.row0[2] * view[0] + basis.row1[2] * view[1] + basis.row2[2] * view[2];
}

struct CameraRay final {
  // Direção em espaço de MUNDO, **não normalizada**, com componente de vista
  // Z = 1. Ela é entregue crua de propósito: veja `cameraRayIsAffineInNdc`.
  float direction[3]{};
  bool valid = false;
  // Orthographic rays are parallel and originate at different points on the
  // camera plane. This offset is in world space, relative to cameraPosition.
  float originOffset[3]{};
};

// Desfaz a pré-rotação do display. A matriz é uma rotação de múltiplo de 90°,
// logo a inversa exata existe sempre; o determinante é checado porque um valor
// não inicializado chegando aqui deve falhar fechado, não produzir um raio.
inline bool undoSurfaceTransform(const HzbScreenTransform &transform, float ndcX, float ndcY,
                                 float &outX, float &outY) noexcept {
  const float determinant = transform.xx * transform.yy - transform.xy * transform.yx;
  if (!std::isfinite(determinant) || std::abs(determinant) < 1e-6f) return false;
  const float inverse = 1.0f / determinant;
  outX = (transform.yy * ndcX - transform.xy * ndcY) * inverse;
  outY = (-transform.yx * ndcX + transform.xx * ndcY) * inverse;
  return true;
}

// O raio de câmera que atravessa um ponto em NDC (−1..1 nos dois eixos, já
// relativo ao retângulo da vista, não à tela).
inline CameraRay cameraRayFromNdc(const PerspectiveFrustum &frustum,
                                  const HzbScreenTransform &surfaceTransform, float ndcX,
                                  float ndcY) noexcept {
  CameraRay ray{};
  if (!frustum.valid || !std::isfinite(ndcX) || !std::isfinite(ndcY)) return ray;
  if (!std::isfinite(frustum.tangentHalfHorizontal) || frustum.tangentHalfHorizontal <= 0.0f)
    return ray;
  if (!std::isfinite(frustum.tangentHalfVertical) || frustum.tangentHalfVertical <= 0.0f)
    return ray;
  float planeX = 0.0f, planeY = 0.0f;
  if (!undoSurfaceTransform(surfaceTransform, ndcX, ndcY, planeX, planeY)) return ray;
  const bool orthographic=isOrthographic(frustum);
  const float view[3] = {orthographic?0.f:planeX * frustum.tangentHalfHorizontal,
                         orthographic?0.f:-planeY * frustum.tangentHalfVertical, 1.0f};
  const CameraViewBasis basis = buildCameraViewBasis(frustum.yaw, frustum.pitch,frustum.roll);
  cameraViewToWorld(basis, view, ray.direction);
  if(orthographic) {
    if(!std::isfinite(frustum.orthographicHalfHeight)||frustum.orthographicHalfHeight<=0 ||
       !std::isfinite(frustum.orthographicHalfWidth)||frustum.orthographicHalfWidth<=0) return ray;
    const float origin[3]{planeX*frustum.orthographicHalfWidth,-planeY*frustum.orthographicHalfHeight,0};
    cameraViewToWorld(basis,origin,ray.originOffset);
    for(float value:ray.originOffset) if(!std::isfinite(value)) return ray;
  }
  ray.valid = std::isfinite(ray.direction[0]) && std::isfinite(ray.direction[1]) &&
              std::isfinite(ray.direction[2]);
  return ray;
}

// POR QUE O RAIO SAI CRU, e não normalizado.
//
// Um passe de tela inteira calcula o raio nos VÉRTICES de um triângulo que
// cobre a tela e deixa a interpolação preencher o resto. Isso só está correto
// se a função for afim em NDC. O raio cru é: cada componente é uma combinação
// linear de (ndcX, ndcY, 1). O versor NÃO é — dividir por um comprimento que
// varia destrói a linearidade.
//
// A conta importa porque o erro é grande, não sutil. Com o triângulo usual, de
// vértices em NDC (−1,−1), (3,−1) e (−1,3), os três raios têm comprimentos bem
// diferentes; normalizar antes de interpolar desloca a direção no CENTRO da
// tela em cerca de 17° num campo de 60°. Foi exatamente esse o defeito: a grade
// do editor saía inclinada e escorregava sobre a geometria ao mover a câmera.
//
// Esta função existe para o teste que trava o defeito: ela devolve o raio que a
// interpolação afim produz, e o teste exige que ele seja igual ao de
// `cameraRayFromNdc` em qualquer ponto.
inline CameraRay cameraRayIsAffineInNdc(const PerspectiveFrustum &frustum,
                                        const HzbScreenTransform &surfaceTransform,
                                        const float triangleNdc[3][2], float weight0,
                                        float weight1, float weight2) noexcept {
  CameraRay ray{};
  const CameraRay corner[3] = {
      cameraRayFromNdc(frustum, surfaceTransform, triangleNdc[0][0], triangleNdc[0][1]),
      cameraRayFromNdc(frustum, surfaceTransform, triangleNdc[1][0], triangleNdc[1][1]),
      cameraRayFromNdc(frustum, surfaceTransform, triangleNdc[2][0], triangleNdc[2][1])};
  if (!corner[0].valid || !corner[1].valid || !corner[2].valid) return ray;
  for (u32 axis = 0; axis < 3; ++axis) {
    ray.direction[axis] = corner[0].direction[axis] * weight0 +
                          corner[1].direction[axis] * weight1 +
                          corner[2].direction[axis] * weight2;
    ray.originOffset[axis] = corner[0].originOffset[axis]*weight0 +
                            corner[1].originOffset[axis]*weight1 + corner[2].originOffset[axis]*weight2;
  }
  ray.valid = true;
  return ray;
}

} // namespace ae::renderer

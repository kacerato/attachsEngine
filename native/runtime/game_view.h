// A vista de jogo do Play como os scripts a enxergam: tamanho em pixels, DPI,
// área segura, orientação e a câmera que produz o quadro.
//
// Quem desenha o quadro publica isto a cada quadro (o editor em Play, o shell
// de um jogo exportado). O runtime não descobre tela nem câmera sozinho: a
// câmera usada aqui é a MESMA que o renderer usou, e as conversões passam pelo
// contrato único de renderer/camera_ray.h.
//
// Convenção de tela (Unity 6000.0 Screen/Camera): pixels da vista de jogo,
// origem no canto INFERIOR esquerdo, y para cima. Viewport: 0..1 com a mesma
// origem. Referências:
// https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Screen.html
// https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Camera.ScreenPointToRay.html
#pragma once
#include "renderer/camera_ray.h"
#include "renderer/frustum_visibility.h"

#include <cmath>
#include <deque>

namespace ae::runtime {

enum class GameViewPlatform : u32 { Host=0, Android=1 };

struct GameView {
  bool valid=false;
  float width=0,height=0;           // pixels da vista de jogo
  float dpi=0;                      // zero: o sistema não informou
  bool safeAreaReported=false;      // falso: a plataforma não entregou recortes
  float safeX=0,safeY=0,safeWidth=0,safeHeight=0; // pixels, origem inferior esquerda
  GameViewPlatform platform=GameViewPlatform::Host;
  u64 camera=0;                     // objeto da câmera autorada; zero = câmera do editor
  renderer::PerspectiveFrustum frustum{};
};

// Raio pelo pixel (x,y). Origem no mundo; direção normalizada.
inline bool gameViewScreenRay(const GameView &view,float x,float y,float origin[3],float direction[3]) {
  if(!view.valid || !view.frustum.valid || view.width<=0 || view.height<=0 || !std::isfinite(x) || !std::isfinite(y)) return false;
  const float ndcX=x/view.width*2.f-1.f;
  // NDC do contrato tem y para baixo (ver camera_ray.h); a tela tem y para cima.
  const float ndcY=1.f-y/view.height*2.f;
  const renderer::HzbScreenTransform identity{};
  const auto ray=renderer::cameraRayFromNdc(view.frustum,identity,ndcX,ndcY);
  if(!ray.valid) return false;
  const float length=std::sqrt(ray.direction[0]*ray.direction[0]+ray.direction[1]*ray.direction[1]+ray.direction[2]*ray.direction[2]);
  if(!std::isfinite(length) || length<1e-12f) return false;
  for(u32 a=0;a<3;++a) {origin[a]=view.frustum.cameraPosition[a]+ray.originOffset[a];direction[a]=ray.direction[a]/length;}
  return true;
}

// Ponto do mundo para pixels; z é a profundidade de vista em metros (negativa
// atrás da câmera, como na Unity). Falso só para entrada ou vista inválida.
inline bool gameViewWorldToScreen(const GameView &view,const float world[3],float screen[3]) {
  if(!view.valid || !view.frustum.valid || view.width<=0 || view.height<=0) return false;
  for(u32 a=0;a<3;++a) if(!std::isfinite(world[a])) return false;
  const auto basis=renderer::buildCameraViewBasis(view.frustum.yaw,view.frustum.pitch,view.frustum.roll);
  const float delta[3]{world[0]-view.frustum.cameraPosition[0],world[1]-view.frustum.cameraPosition[1],world[2]-view.frustum.cameraPosition[2]};
  float local[3];renderer::cameraWorldToView(basis,delta,local);
  float planeX=0,planeY=0;
  if(renderer::isOrthographic(view.frustum)) {
    if(view.frustum.orthographicHalfWidth<=0 || view.frustum.orthographicHalfHeight<=0) return false;
    planeX=local[0]/view.frustum.orthographicHalfWidth;planeY=local[1]/view.frustum.orthographicHalfHeight;
  } else {
    if(std::abs(local[2])<1e-9f) return false;
    planeX=local[0]/(local[2]*view.frustum.tangentHalfHorizontal);
    planeY=local[1]/(local[2]*view.frustum.tangentHalfVertical);
  }
  screen[0]=(planeX*.5f+.5f)*view.width;
  screen[1]=(planeY*.5f+.5f)*view.height;
  screen[2]=local[2];
  return std::isfinite(screen[0]) && std::isfinite(screen[1]) && std::isfinite(screen[2]);
}

// Linhas de depuração pedidas pelos scripts (Debug.DrawLine/DrawRay). Estado só
// de Play: nunca entra no documento; Stop descarta tudo.
struct DebugLine { float a[3]{},b[3]{}; u32 rgba=0xffffffff; double remaining=0; };
class DebugLines final {
public:
  static constexpr usize Capacity=4096;
  void reset() {lines_.clear();dropped_=0;}
  // Duração zero: visível no quadro em que foi pedida.
  bool add(const float a[3],const float b[3],u32 rgba,double seconds) {
    for(u32 i=0;i<3;++i) if(!std::isfinite(a[i])||!std::isfinite(b[i])) return false;
    if(!std::isfinite(seconds) || seconds<0 || seconds>3600) return false;
    if(lines_.size()>=Capacity) {lines_.pop_front();++dropped_;}
    DebugLine line;std::copy(a,a+3,line.a);std::copy(b,b+3,line.b);line.rgba=rgba;line.remaining=seconds;
    lines_.push_back(line);return true;
  }
  // No início do quadro: o que já foi mostrado e venceu sai; o resto envelhece.
  void advance(double seconds) {
    std::erase_if(lines_,[](const DebugLine &line){return line.remaining<=0;});
    for(auto &line:lines_) line.remaining-=seconds;
  }
  const std::deque<DebugLine> &lines() const noexcept {return lines_;}
  u64 dropped() const noexcept {return dropped_;}
private:
  std::deque<DebugLine> lines_;
  u64 dropped_=0;
};

} // namespace ae::runtime

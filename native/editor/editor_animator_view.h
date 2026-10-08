#pragma once
// Geometria do editor de grafo do Animator, comum ao desenho
// (editor_screen.cpp) e ao toque (editor_animator.cpp): posição dos nós,
// conversão tela ↔ grafo e as setas das transições.
#include "scene/animator.h"
#include "ui/ui_geometry.h"

#include <cmath>
#include <array>

namespace ae::editor::animator_view {

inline constexpr float NodeWidth=176,NodeHeight=60;
// Nós fixos do grafo: Entrada aponta o estado padrão; Qualquer estado é a
// origem das transições globais (from zero no modelo).
inline constexpr u64 EntryNode=~u64{0},AnyNode=~u64{0}-1,ExitNode=~u64{0}-2;
inline constexpr float EntryX=-320,EntryY=-130,AnyX=-320,AnyY=110;

struct View {ui::UiRect canvas{};float panX=0,panY=0,zoom=1;u64 machine=0;};
inline u64 projection(const View &v,const scene::AnimatorLayer &layer,u64 node) {
  if(!node) return ExitNode;
  for(usize depth=0;depth<=layer.machines.size();++depth) {
    if(layer.parent(node)==v.machine) return node;
    node=layer.parent(node);if(!node||node==v.machine) return ExitNode;
  }
  return ExitNode;
}

inline ui::UiPoint toScreen(const View &v,float x,float y) {
  return {v.canvas.x+v.canvas.width*.5f+(x+v.panX)*v.zoom,v.canvas.y+v.canvas.height*.5f+(y+v.panY)*v.zoom};
}
inline void toGraph(const View &v,ui::UiPoint p,float &x,float &y) {
  x=(p.x-v.canvas.x-v.canvas.width*.5f)/v.zoom-v.panX;
  y=(p.y-v.canvas.y-v.canvas.height*.5f)/v.zoom-v.panY;
}
inline bool nodePosition(const scene::AnimatorLayer &layer,u64 node,float &x,float &y) {
  if(node==EntryNode) {x=EntryX;y=EntryY;return true;}
  if(node==AnyNode) {x=AnyX;y=AnyY;return true;}
  if(node==ExitNode) {x=320;y=-130;return true;}
  if(const auto *s=layer.state(node)) {x=s->x;y=s->y;return true;}
  if(const auto *m=layer.machine(node)) {x=m->x;y=m->y;return true;}
  return false;
}
inline bool setNodePosition(scene::AnimatorLayer &layer,u64 node,float x,float y) {
  if(auto *s=layer.state(node)) {s->x=x;s->y=y;return true;}
  if(auto *m=layer.machine(node)) {m->x=x;m->y=y;return true;}
  return false;
}
// Retângulo do nó na tela; a posição autorada é o canto superior esquerdo.
inline ui::UiRect nodeRect(const View &v,const scene::AnimatorLayer &layer,u64 node) {
  float x=0,y=0;if(!nodePosition(layer,node,x,y)) return {};
  const auto p=toScreen(v,x,y);
  return {p.x,p.y,NodeWidth*v.zoom,NodeHeight*v.zoom};
}
inline bool inside(const ui::UiRect &r,ui::UiPoint p) {return p.x>=r.x&&p.y>=r.y&&p.x<=r.x+r.width&&p.y<=r.y+r.height;}
// Estado por cima primeiro (o último desenhado), depois os nós fixos.
inline u64 hitNode(const View &v,const scene::AnimatorLayer &layer,ui::UiPoint p) {
  for(auto s=layer.states.rbegin();s!=layer.states.rend();++s) if(s->machine==v.machine&&inside(nodeRect(v,layer,s->id),p)) return s->id;
  for(auto m=layer.machines.rbegin();m!=layer.machines.rend();++m) if(m->parent==v.machine&&inside(nodeRect(v,layer,m->id),p)) return m->id;
  if(inside(nodeRect(v,layer,EntryNode),p)) return EntryNode;
  if(inside(nodeRect(v,layer,AnyNode),p)) return AnyNode;
  if(v.machine&&inside(nodeRect(v,layer,ExitNode),p)) return ExitNode;
  return 0;
}
// Seta de `from` a `to`, entre as bordas dos nós e deslocada para a esquerda
// da direção: as duas direções entre o mesmo par não se sobrepõem.
inline bool arrow(const View &v,const scene::AnimatorLayer &layer,u64 from,u64 to,ui::UiPoint &a,ui::UiPoint &b) {
  const auto ra=nodeRect(v,layer,from),rb=nodeRect(v,layer,to);
  if(ra.width<=0||rb.width<=0) return false;
  const ui::UiPoint ca{ra.x+ra.width*.5f,ra.y+ra.height*.5f},cb{rb.x+rb.width*.5f,rb.y+rb.height*.5f};
  float dx=cb.x-ca.x,dy=cb.y-ca.y;const float length=std::sqrt(dx*dx+dy*dy);
  if(length<1) return false;
  dx/=length;dy/=length;
  const float offset=7*v.zoom,nx=dy*offset,ny=-dx*offset;
  const auto edge=[&](const ui::UiRect &r){
    const float hw=r.width*.5f,hh=r.height*.5f;
    return std::min(std::abs(dx)>1e-4f?hw/std::abs(dx):1e9f,std::abs(dy)>1e-4f?hh/std::abs(dy):1e9f);
  };
  const float ea=edge(ra),eb=edge(rb);
  if(ea+eb>=length) return false;
  a={ca.x+dx*ea+nx,ca.y+dy*ea+ny};b={cb.x-dx*eb+nx,cb.y-dy*eb+ny};
  return true;
}
inline float segmentDistance(ui::UiPoint a,ui::UiPoint b,ui::UiPoint p) {
  const float dx=b.x-a.x,dy=b.y-a.y,l=dx*dx+dy*dy;
  float t=l>0?((p.x-a.x)*dx+(p.y-a.y)*dy)/l:0;t=std::fmin(1.f,std::fmax(0.f,t));
  const float x=a.x+dx*t-p.x,y=a.y+dy*t-p.y;return std::sqrt(x*x+y*y);
}
inline u32 transitionPath(const View &v,const scene::AnimatorLayer &layer,u64 from,u64 to,std::array<ui::UiPoint,5> &points) {
  if(from==to&&layer.node(from)) {
    const auto r=nodeRect(v,layer,from);const float gap=24*v.zoom;
    points={ui::UiPoint{r.x+r.width,r.y+r.height*.25f},{r.x+r.width+gap,r.y+r.height*.25f},
      {r.x+r.width+gap,r.y-gap},{r.x+r.width*.5f,r.y-gap},{r.x+r.width*.5f,r.y}};return 5;
  }
  return arrow(v,layer,from,to,points[0],points[1])?2:0;
}
// Projected destinations may coincide after a subtree is collapsed. Give each
// visible edge its own lane; drawing and touch use this same geometry.
inline u32 transitionPath(const View &v,const scene::AnimatorLayer &layer,const scene::AnimatorTransition &transition,std::array<ui::UiPoint,5> &points) {
  const auto from=transition.entry?EntryNode:transition.from?transition.from:AnyNode,to=projection(v,layer,transition.to);
  usize index=0,count=0;
  for(const auto &candidate:layer.transitions) {
    if(candidate.from?layer.parent(candidate.from)!=v.machine:candidate.machine!=v.machine) continue;
    const auto source=candidate.entry?EntryNode:candidate.from?candidate.from:AnyNode;
    if(source!=from||projection(v,layer,candidate.to)!=to) continue;
    if(candidate.id==transition.id) index=count;
    ++count;
  }
  auto vertices=transitionPath(v,layer,from,to,points);
  if(count<2||!vertices) return vertices;
  if(vertices==5) {
    const float extra=28.f*v.zoom*index;
    points[1].x+=extra;points[2].x+=extra;points[2].y-=extra;points[3].y-=extra;
    return vertices;
  }
  const auto a=points[0],b=points[1];const float dx=b.x-a.x,dy=b.y-a.y,length=std::sqrt(dx*dx+dy*dy);
  const float lane=48.f*v.zoom*(static_cast<float>(index)-(static_cast<float>(count)-1)*.5f);
  points[1]={(a.x+b.x)*.5f+dy/length*lane,(a.y+b.y)*.5f-dx/length*lane};points[2]=b;
  return 3;
}
inline u64 hitTransition(const View &v,const scene::AnimatorLayer &layer,ui::UiPoint p) {
  float best=14;u64 found=0;
  for(const auto &t:layer.transitions) {
    if(t.from?layer.parent(t.from)!=v.machine:t.machine!=v.machine) continue;
    std::array<ui::UiPoint,5> points;const auto count=transitionPath(v,layer,t,points);
    for(u32 i=1;i<count;++i) {const float d=segmentDistance(points[i-1],points[i],p);if(d<best) {best=d;found=t.id;}}
  }
  return found;
}
// Lugar livre para um estado novo: à direita do mais à direita, na grade.
inline void freeSpot(const scene::AnimatorLayer &layer,float &x,float &y) {
  x=0;y=-NodeHeight*.5f;
  for(const auto &s:layer.states) if(s.x+NodeWidth+64>x) {x=s.x+NodeWidth+64;y=s.y;}
  if(layer.states.empty()) {x=-NodeWidth*.5f;y=-NodeHeight*.5f;}
  x=std::round(x/16)*16;y=std::round(y/16)*16;
}

} // namespace ae::editor::animator_view

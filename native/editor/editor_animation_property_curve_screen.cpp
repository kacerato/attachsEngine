#include "editor/editor_screen.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ae::editor {
using namespace ui;
namespace {
namespace w=clip_widget;
UiRect top(UiRect &r,float h){h=std::min(h,r.height);UiRect q{r.x,r.y,r.width,h};r.y+=h;r.height-=h;return q;}
UiRect left(UiRect &r,float width){width=std::min(width,r.width);UiRect q{r.x,r.y,width,r.height};r.x+=width;r.width-=width;return q;}
std::string number(double v){char b[48];std::snprintf(b,sizeof b,"%.5g",v);return b;}
}
void buildAnimationPropertyCurveSurface(const EditorScreenState &state,const UiTheme &theme,UiDrawList &list,UiInputRouter &router,const UiRect &area) {
  const auto *track=state.clipAsset?state.clipAsset->propertyTrack(state.clipPropertyTrack):nullptr;
  if(!track||track->data.curves.empty())return;
  const u32 component=std::min<u32>(state.clipPropertyComponent,u32(track->data.curves.size()-1));
  const auto &curve=track->data.curves[component];const auto found=std::find_if(curve.keys.begin(),curve.keys.end(),[&](const auto &k){return k.id==state.clipPropertyKey;});
  const auto label=[&](UiRect r,std::string_view text,UiColor color){list.pushClip(r);r.x+=8;r.width-=16;list.addText(r,text,color,theme.type.caption);list.popClip();};
  const auto button=[&](UiRect r,std::string_view text,u32 code,bool enabled=true){const auto hit=r;list.addRect(deflate(r,{2,3,2,3}),theme.color.raised,2);label(r,text,enabled?theme.color.text:theme.color.textFaint);if(enabled)router.addRegion(hit,w::id(code));};
  UiRect all{area.x+8,area.y+4,area.width-16,std::min(244.f,area.height-8)};
  list.addRect(all,theme.color.surface);router.addBlocker(all);
  const float controls=std::min(300.f,all.width*.54f);
  auto plot=left(all,all.width-controls);auto title=top(all,44);
  button(left(title,44),"<",w::PropertyCurve);label(title,state.clipPropertyLabel,theme.color.text);
  auto row=top(all,44);button(left(row,88),"Canal "+std::to_string(component+1),w::PropertyComponent);
  button(left(row,44),"<",w::PropertyKeyPrevious);button(left(row,44),">",w::PropertyKeyNext);
  button(row,"Campos",w::PropertyCurveFields);
  row=top(all,44);
  button(left(row,row.width*.5f),found==curve.keys.end()?"Chave…":"Tempo · "+number(found->time),w::PropertyCurveNumber,found!=curve.keys.end());
  button(row,found==curve.keys.end()?"Selecione uma chave":"Valor · "+number(found->value),w::PropertyCurveNumber+1,found!=curve.keys.end());
  row=top(all,44);const bool selected=found!=curve.keys.end();
  if(!state.clipPropertyCurveMode){button(left(row,row.width*.5f),selected?"Tangente · "+std::to_string(u32(found->outgoing)):"Tangente",w::PropertyTangent,selected);button(row,"Ponderação",w::PropertyWeighted,selected);}
  else {
    const bool weights=state.clipPropertyCurveMode==2;const auto base=weights?4:2;
    button(left(row,row.width*.5f),selected?std::string(weights?"Peso in · ":"Slope in · ")+number(weights?found->inWeight:found->inSlope):"Entrada",w::PropertyCurveNumber+base,selected);
    button(row,selected?std::string(weights?"Peso out · ":"Slope out · ")+number(weights?found->outWeight:found->outSlope):"Saída",w::PropertyCurveNumber+base+1,selected);
  }
  row=top(all,44);button(left(row,row.width*.5f),"Gravar no cursor",w::PropertyRecord);button(row,"Apagar chave",w::PropertyKeyDelete,selected&&curve.keys.size()>1);
  label(all,"ID "+std::to_string(state.clipPropertyKey)+" · canal "+std::to_string(component+1),theme.color.textMuted);
  label(top(plot,24),"Curva efetiva · "+std::to_string(curve.keys.size())+" chaves",theme.color.textMuted);
  plot=deflate(plot,{12,8,12,20});if(plot.isEmpty())return;
  if(!resources::validAnimationCurve(curve))return;
  const auto sample=[&](double time){resources::AnimationCurveSample result;resources::sampleValidatedAnimationCurve(curve,time,result);return result.value;};
  double lo=0,hi=0;for(u32 i=0;i<=128;++i){const auto v=sample(double(state.clipAsset->duration)*i/128);if(!i)lo=hi=v;else{lo=std::min(lo,v);hi=std::max(hi,v);}}
  const double margin=std::max(.1,(hi-lo)*.12);lo-=margin;hi+=margin;
  const auto point=[&](double time,double value){return UiPoint{plot.x+float(time/std::max(.0001f,state.clipAsset->duration))*plot.width,plot.bottom()-float((value-lo)/(hi-lo))*plot.height};};
  list.pushClip(plot);
  for(u32 i=0;i<=4;++i){const float x=plot.x+plot.width*i/4,y=plot.y+plot.height*i/4;list.addLine({x,plot.y},{x,plot.bottom()},theme.color.lineSoft,1);list.addLine({plot.x,y},{plot.right(),y},theme.color.lineSoft,1);}
  auto previous=point(0,sample(0));
  for(u32 i=1;i<=256;++i){const auto t=double(state.clipAsset->duration)*i/256;const auto p=point(t,sample(t));list.addLine(previous,p,theme.color.accent,1.8f);previous=p;}
  for(usize i=0;i<curve.keys.size();++i){const auto &key=curve.keys[i];const auto p=point(key.time,key.value);const auto color=key.id==state.clipPropertyKey?theme.color.text:theme.color.accent;list.addLine({p.x-5,p.y},{p.x,p.y-5},color,2);list.addLine({p.x,p.y-5},{p.x+5,p.y},color,2);list.addLine({p.x+5,p.y},{p.x,p.y+5},color,2);list.addLine({p.x,p.y+5},{p.x-5,p.y},color,2);if(i<64)router.addRegion(intersect({p.x-22,p.y-22,44,44},plot),w::id(w::PropertyCurveKey+u32(i)));}
  const auto cursor=point(state.clipTime,lo);list.addLine({cursor.x,plot.y},{cursor.x,plot.bottom()},theme.color.textMuted,1);list.popClip();
}
}

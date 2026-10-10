#include "editor/editor_screen.h"
#include <algorithm>
#include <cstdio>

namespace ae::editor {
using namespace ui;
namespace {
namespace w=clip_widget;
constexpr const char *operations[]={"Entrada","Constante","Somar","Multiplicar","Escala + offset","Clamp","Remapear","Curva"};
UiRect top(UiRect &r,float h){h=std::min(h,r.height);UiRect value{r.x,r.y,r.width,h};r.y+=h;r.height-=h;return value;}
UiRect left(UiRect &r,float width){width=std::min(width,r.width);UiRect value{r.x,r.y,width,r.height};r.x+=width;r.width-=width;return value;}
std::string number(double value){char text[48];std::snprintf(text,sizeof text,"%.5g",value);return text;}
}
void buildAnimationDriverSurface(const EditorScreenState &state,const UiTheme &theme,UiDrawList &list,UiInputRouter &router,const UiRect &area) {
  if(!state.clipDriverDraft)return;
  const auto &draft=*state.clipDriverDraft;const auto &graph=draft.drivers.graph;
  const resources::AnimationDriverNode *node=nullptr;for(const auto &n:graph.nodes)if(n.id==state.clipDriverNode)node=&n;
  const auto label=[&](UiRect r,std::string_view text,UiColor color){list.pushClip(r);r.x+=8;r.width-=16;list.addText(r,text,color,theme.type.caption);list.popClip();};
  const auto button=[&](UiRect r,std::string_view text,u32 code,bool enabled=true,bool active=false,UiIcon icon=UiIcon::None) {
    const auto hit=r;r=deflate(r,{2,3,2,3});list.addRect(r,active?withAlpha(theme.color.accent,.16f):theme.color.raised,2);
    if(icon!=UiIcon::None){const auto slot=left(r,30);list.addImage({slot.x+4,slot.y+(slot.height-22)/2,22,22},static_cast<UiImageId>(icon));}
    label(r,text,enabled?theme.color.text:theme.color.textFaint);if(enabled)router.addRegion(hit,w::id(code));
  };
  auto sheet=UiRect{area.x+8,area.y+4,std::min(460.f,area.width-16),std::min(244.f,area.height-8)};
  list.addRect(sheet,theme.color.surface);router.addBlocker(sheet);auto title=top(sheet,44);
  button(left(title,52),"<",w::DriverBack);label(left(title,std::max(0.f,title.width-96)),"Drivers · rascunho",theme.color.text);
  if(state.clipDriverRoute==1)button(title,"Excluir",w::DriverDelete);
  else button(title,"Objeto…",w::Targets);
  const auto footer=UiRect{sheet.x,sheet.bottom()-44,sheet.width,44};sheet.height=std::max(0.f,sheet.height-44);
  auto actions=footer;button(left(actions,actions.width*.5f),"Aplicar · um Undo",w::DriverApply,true,false,UiIcon::AnimationDriver);
  button(actions,"Cancelar",w::DriverCancel);
  if(state.clipDriverRoute==2) {
    for(u32 row=0;row<2;++row){auto r=top(sheet,44);const auto i=(state.clipDriverPage%2)*4+row*2;button(left(r,r.width*.5f),operations[i],w::DriverOperation+i);button(r,operations[i+1],w::DriverOperation+i+1);}
    auto page=top(sheet,44);button(left(page,44),"<",w::DriverPrevious,state.clipDriverPage>0);button(left(page,44),">",w::DriverNext,state.clipDriverPage<1);
    label(page,"Operações · "+std::to_string(state.clipDriverPage+1)+" / 2",theme.color.textMuted);
  } else if(state.clipDriverRoute==0||state.clipDriverRoute==3||state.clipDriverRoute==4) {
    const usize count=state.clipDriverRoute==0?graph.nodes.size():state.clipDriverEntries.size();
    for(u32 i=0;i<2;++i) {
      auto row=top(sheet,44);const auto index=usize(state.clipDriverPage)*2+i;
      if(index<count) {
        std::string text;
        if(state.clipDriverRoute==0){const auto &n=graph.nodes[index];text="#"+std::to_string(n.id)+" · "+operations[u32(n.operation)];}
        else text=state.clipDriverEntries[index].label;
        button(row,text,w::DriverEntry+i);
      } else if(!i)label(row,state.clipDriverRoute==0?"Crie uma entrada, combine e escolha a saída":"Nenhuma propriedade disponível neste objeto",theme.color.textMuted);
    }
    auto page=top(sheet,44);button(left(page,44),"<",w::DriverPrevious,state.clipDriverPage>0);
    button(left(page,44),">",w::DriverNext,(usize(state.clipDriverPage)+1)*2<count);
    if(state.clipDriverRoute==0)button(page,"+ Operação",w::DriverAdd,true,false,UiIcon::AnimationDriver);
    else label(page,std::to_string(count)+" entradas · página "+std::to_string(state.clipDriverPage+1),theme.color.textMuted);
  } else if(state.clipDriverRoute==1&&node) {
    const auto op=node->operation;auto links=top(sheet,44);
    if(op==resources::AnimationDriverOp::Source)button(links,"Escolher entrada · tempo, propriedade ou Animator",w::DriverInput);
    else if(op==resources::AnimationDriverOp::Constant)button(links,"Constante · "+number(node->a),w::DriverConstant);
    else {
      button(left(links,links.width*.5f),"A ← #"+std::to_string(node->first),w::DriverFirst);
      if(op==resources::AnimationDriverOp::Add||op==resources::AnimationDriverOp::Multiply)button(links,"B ← #"+std::to_string(node->second),w::DriverSecond);
      else label(links,operations[u32(op)],theme.color.textMuted);
    }
    auto parameters=top(sheet,44);
    if(op==resources::AnimationDriverOp::ScaleBias||op==resources::AnimationDriverOp::Clamp||op==resources::AnimationDriverOp::Remap) {
      const bool cd=state.clipDriverFields&&op==resources::AnimationDriverOp::Remap;
      const char *a=op==resources::AnimationDriverOp::ScaleBias?"Escala":op==resources::AnimationDriverOp::Clamp?"Mínimo":cd?"Saída min":"Entrada min";
      const char *b=op==resources::AnimationDriverOp::ScaleBias?"Offset":op==resources::AnimationDriverOp::Clamp?"Máximo":cd?"Saída max":"Entrada max";
      button(left(parameters,parameters.width*.5f),std::string(a)+" · "+number(cd?node->c:node->a),w::DriverConstant+(cd?2:0));
      button(parameters,std::string(b)+" · "+number(cd?node->d:node->b),w::DriverConstant+(cd?3:1));
    } else if(op==resources::AnimationDriverOp::Curve)button(parameters,"Editar curva · "+std::to_string(node->curve.keys.size())+" chaves",w::DriverCurve);
    else label(parameters,state.clipDriverInspection,theme.color.text);
    auto outputs=top(sheet,44);
    button(left(outputs,outputs.width*.5f),"Ligar saída…",w::DriverOutput);
    if(op==resources::AnimationDriverOp::Remap)button(outputs,state.clipDriverFields?"Faixa de entrada":"Faixa de saída",w::DriverFields);
    else button(outputs,"Desligar saídas",w::DriverClearOutput);
    const auto count=std::count(graph.outputs.begin(),graph.outputs.end(),node->id);
    label(sheet,"#"+std::to_string(node->id)+" · "+std::to_string(count)+" saídas · "+state.clipDriverInspection,theme.color.textMuted);
  } else if(state.clipDriverRoute==5&&node&&!node->curve.keys.empty()) {
    const auto index=std::min<usize>(state.clipDriverCurveKey,node->curve.keys.size()-1);const auto &key=node->curve.keys[index];
    auto navigation=top(sheet,44);button(left(navigation,44),"<",w::DriverCurvePrevious,index>0);button(left(navigation,44),">",w::DriverCurveNext,index+1<node->curve.keys.size());
    button(left(navigation,88),state.clipDriverCurveMode==0?"Valor":state.clipDriverCurveMode==1?"Handles":"Pesos",w::DriverFields);
    button(left(navigation,navigation.width*.5f),"+ Chave",w::DriverCurveAdd);button(navigation,"Apagar",w::DriverCurveDelete,node->curve.keys.size()>1);
    auto values=top(sheet,44);button(left(values,values.width*.5f),"Entrada · "+number(key.time),w::DriverCurveValue);
    button(values,"Resultado · "+number(key.value),w::DriverCurveValue+1);
    auto tangents=top(sheet,44);
    if(state.clipDriverCurveMode==1) {
      button(left(tangents,tangents.width*.5f),"Slope entrada · "+number(key.inSlope),w::DriverCurveValue+2);
      button(tangents,"Slope saída · "+number(key.outSlope),w::DriverCurveValue+3);
    } else if(state.clipDriverCurveMode==2) {
      button(left(tangents,tangents.width*.5f),"Peso in · "+number(key.inWeight),w::DriverCurveValue+4);
      button(tangents,"Peso out · "+number(key.outWeight),w::DriverCurveValue+5);
    } else {
      button(left(tangents,tangents.width*.5f),"Tangente · "+std::to_string(u32(key.outgoing)),w::DriverCurveTangent);
      button(tangents,key.weightedIn?"Pesadas":"Ativar pesos",w::DriverCurveWeighted);
    }
    label(sheet,"Curva no domínio da entrada · chave "+std::to_string(index+1),theme.color.textMuted);
  }
}
}

#include "editor/editor_rig_screen.h"
#include <algorithm>
#include <array>

namespace ae::editor {
using namespace ui;
namespace {
constexpr std::array<const char *,5> routeNames{"Controles","Espelho","Poses","Transferência","Diagnóstico"};
constexpr std::array<RigToolAction,5> routeActions{RigToolAction::RouteControls,RigToolAction::RouteMirror,
  RigToolAction::RoutePoses,RigToolAction::RouteRetarget,RigToolAction::RouteDiagnostics};
constexpr std::array<UiIcon,5> routeIcons{UiIcon::AnimationSkeleton,UiIcon::AnimationRigMirror,
  UiIcon::AnimationRecordPose,UiIcon::AnimationRetarget,UiIcon::UiDiagnostics};
constexpr float kHeader=44,kRow=44,kGap=4,kFooter=44;
}
EditorRigScreenLayout rigScreenLayout(UiRect area) {
  const float width=std::min(area.width,std::clamp(area.width*.36f,224.f,336.f));
  return {{area.x,area.y,std::max(0.f,area.width-width),area.height},
          {area.right()-width,area.y,width,area.height}};
}
void buildEditorRigScreen(const EditorRigScreenState &state,const UiTheme &theme,
                          UiDrawList &list,UiInputRouter &router,UiRect area) {
  if(area.width<=0||area.height<=0)return;
  list.addRect(area,theme.color.surface);router.addBlocker(area);
  const auto text=[&](UiRect rect,std::string_view value,UiColor color,const UiTypeStyle &style,UiAlign align=UiAlign::Start) {
    if(rect.width<=0||rect.height<=0)return;
    list.pushClip(rect);list.addText(rect,value,color,style,align);list.popClip();
  };
  const auto icon=[&](UiRect rect,UiIcon id,UiColor tint,float size=18) {
    if(id==UiIcon::None)return;
    list.addImage({rect.x+(rect.width-size)*.5f,rect.y+(rect.height-size)*.5f,size,size},static_cast<UiImageId>(id),tint);
  };
  const auto route=static_cast<usize>(std::min<usize>(static_cast<usize>(state.route),routeNames.size()-1));

  // Cabeçalho: as cinco rotas como abas de ícone sempre visíveis (sem menu
  // escondido), fechar à direita. O nome da rota ativa vai na linha de título.
  const UiRect header{area.x,area.y,area.width,kHeader};
  list.addRect(header,theme.color.silhouette);
  const UiRect close{header.right()-kHeader,header.y,kHeader,kHeader};
  icon(close,UiIcon::UiClose,theme.color.textDim,16);router.addRegion(close,static_cast<u32>(RigToolAction::Close));
  const float tabWidth=std::min(kHeader,std::max(28.f,(close.x-header.x)/float(routeNames.size())));
  const bool tabsEnabled=!state.title.empty()&&state.title.find("juntas")!=std::string::npos;
  for(usize i=0;i<routeNames.size();++i) {
    const UiRect tab{header.x+float(i)*tabWidth,header.y,tabWidth,kHeader};
    const bool active=i==route;
    if(active) {list.addRect({tab.x+4,tab.y+4,tab.width-8,tab.height-8},theme.color.accentWash,theme.radius.control);
                list.addRect({tab.x+8,tab.bottom()-3,tab.width-16,3},theme.color.accent,1.5f);}
    icon(tab,routeIcons[i],active?theme.color.accent:tabsEnabled?theme.color.textDim:theme.color.textFaint,20);
    if(tabsEnabled)router.addRegion(tab,static_cast<u32>(routeActions[i]));
  }
  // Título: rota + objeto/revisão, numa faixa estreita sob as abas.
  const UiRect title{area.x+12,header.bottom(),area.width-24,22};
  text({title.x,title.y,title.width*.42f,title.height},routeNames[route],theme.color.accent,theme.type.caption);
  const bool alert=state.error&&!state.confirmLabel.empty();
  text({title.x+title.width*.42f,title.y,title.width*.58f,title.height},alert?state.status:state.title,alert?theme.color.danger:theme.color.textMuted,theme.type.caption,UiAlign::End);

  // Rodapé: paginação nas extremidades, ação primária ou estado no centro.
  const UiRect footer{area.x,area.bottom()-kFooter,area.width,kFooter};
  const float bodyTop=title.bottom()+2,bodyHeight=std::max(0.f,footer.y-bodyTop-kGap);
  const u32 rows=std::max(1u,static_cast<u32>((bodyHeight+kGap)/(kRow+kGap)));
  const u32 pages=std::max(1u,static_cast<u32>((state.fields.size()+rows-1)/rows));
  const u32 page=std::min(state.page,pages-1);
  for(u32 n=0;n<rows;++n) {
    const usize index=usize(page)*rows+n;
    if(index>=state.fields.size())break;
    const auto &field=state.fields[index];
    const UiRect card{area.x+8,bodyTop+float(n)*(kRow+kGap),area.width-16,kRow};
    list.addRect(card,field.enabled?theme.color.raised:theme.color.silhouette,theme.radius.control);
    UiRect inner{card.x+10,card.y+3,card.width-20,card.height-6};
    if(field.icon!=UiIcon::None) {icon({inner.x,inner.y,22,inner.height},field.icon,theme.color.accent);inner.x+=30;inner.width-=30;}
    if(field.enabled) {icon({inner.right()-16,inner.y,16,inner.height},UiIcon::UiChevronRight,theme.color.textMuted,14);inner.width-=20;}
    text({inner.x,inner.y,inner.width,inner.height*.45f},field.name,field.enabled?theme.color.textDim:theme.color.textMuted,theme.type.caption);
    text({inner.x,inner.y+inner.height*.45f,inner.width,inner.height*.55f},field.value,
         field.enabled?theme.color.text:theme.color.textMuted,theme.type.body);
    if(field.enabled)router.addRegion(card,static_cast<u32>(field.action));
  }
  if(state.fields.empty()) {
    const UiRect empty{area.x+16,bodyTop+8,area.width-32,kRow*2};
    text(empty,state.status.empty()?"Selecione um controle no modelo":state.status,
         state.error?theme.color.danger:theme.color.textDim,theme.type.caption);
  }
  list.addRect({footer.x,footer.y-1,footer.width,1},theme.color.lineSoft);
  const bool paged=pages>1;
  UiRect centre{footer.x+(paged?kFooter:8),footer.y+6,footer.width-(paged?2*kFooter:16),kFooter-12};
  if(paged) {
    const UiRect previous{footer.x,footer.y,kFooter,kFooter},next{footer.right()-kFooter,footer.y,kFooter,kFooter};
    icon(previous,UiIcon::UiChevronLeft,page>0?theme.color.text:theme.color.textFaint,16);
    icon(next,UiIcon::UiChevronRight,page+1<pages?theme.color.text:theme.color.textFaint,16);
    if(page>0)router.addRegion(previous,static_cast<u32>(RigToolAction::PreviousPage));
    if(page+1<pages)router.addRegion(next,static_cast<u32>(RigToolAction::NextPage));
  }
  if(state.hasPreview) {
    const UiRect discard{centre.x,centre.y,72,centre.height};
    list.addRect(discard,theme.color.raised,theme.radius.control);
    text(discard,"Descartar",theme.color.text,theme.type.caption,UiAlign::Center);
    router.addRegion(discard,static_cast<u32>(RigToolAction::CancelPreview));centre.x+=76;centre.width-=76;
  }
  if(!state.confirmLabel.empty()) {
    list.addRect(centre,state.canConfirm?theme.color.accent:theme.color.raised,theme.radius.control);
    text(centre,state.confirmLabel,state.canConfirm?theme.color.accentInk:theme.color.textMuted,theme.type.caption,UiAlign::Center);
    if(state.canConfirm)router.addRegion(centre,static_cast<u32>(RigToolAction::Confirm));
  } else if(!state.fields.empty()) {
    const auto summary=state.status.empty()?std::to_string(page+1)+" / "+std::to_string(pages):state.status;
    text(centre,summary,state.error?theme.color.danger:theme.color.textDim,theme.type.caption,paged&&state.status.empty()?UiAlign::Center:UiAlign::Start);
  }
}
} // namespace ae::editor

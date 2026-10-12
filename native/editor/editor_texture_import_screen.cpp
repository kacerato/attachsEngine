#include "editor/editor_texture_import_screen.h"
#include "editor/editor_widgets.h"
#include "ui/ui_draw_list.h"
#include "resources/texture_profile.h"
#include "ui/ui_icon_id.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace ae::editor {
using namespace ui;
void buildTextureImportWorkspace(const EditorScreenState &state,const UiTheme &theme,
    UiDrawList &list,UiInputRouter &router,UiRect area) {
  router.addBlocker(area);list.addRect(area,theme.color.surface);
  const auto label=[&](UiRect rect,std::string_view text,UiColor color,const UiTypeStyle &style,UiAlign align=UiAlign::Start) {
    list.pushClip(rect);list.addText(rect,text,color,style,align);list.popClip();
  };
  const auto action=[&](UiRect rect,std::string_view text,EditorWidget widget,bool enabled,bool primary=false) {
    list.addRect(rect,primary&&enabled?theme.color.accent:theme.color.raised,theme.radius.control);
    label(rect,text,primary&&enabled?theme.color.accentInk:enabled?theme.color.text:theme.color.textMuted,theme.type.caption,UiAlign::Center);
    if(enabled)router.addRegion(rect,widgetId(widget));
  };
  area={area.x+8,area.y+4,std::max(0.f,area.width-16),std::max(0.f,area.height-8)};
  UiRect header{area.x,area.y,area.width,48};
  const bool recipeReady=state.importReady&&resources::sameTexturePreparation(state.textureImportSettings,state.textureImportPreparedSettings);
  UiRect accept{header.right()-128,header.y,128,48};
  action(accept,state.importReady?"Importar textura":state.importError?"Fonte recusada":"Preparando…",EditorWidget::ImportAccept,recipeReady,true);
  UiRect close{header.x,header.y,48,48};action(close,state.importError?"Fechar":"Voltar",EditorWidget::ImportCancel,true);
  const float titleX=close.right()+(state.textureImportTiff?40:8);
  if(state.textureImportTiff)list.addImage({close.right()+8,header.y+12,24,24},static_cast<UiImageId>(UiIcon::TexturesSourcePolicy),theme.color.accent);
  UiRect title{titleX,header.y,std::max(0.f,accept.x-titleX-8),24};
  const auto slash=state.importPath.find_last_of("/\\");const auto name=state.importPath.substr(slash==state.importPath.npos?0:slash+1);
  label(title,name.empty()?"Importar textura":name,theme.color.text,theme.type.body);
  title.y+=24;label(title,state.importStatus,theme.color.textDim,theme.type.caption);
  UiRect body{area.x,header.bottom()+4,area.width,std::max(0.f,area.bottom()-header.bottom()-4)};
  if(state.importError&&!state.textureImportTiff) {
    // Error occupies the available surface instead of hiding beneath settings.
    std::string line;float y=body.y;usize start=0;
    const usize columns=std::max<usize>(16,static_cast<usize>(body.width/8));
    while(start<state.importSummary.size()&&y+24<=body.bottom()) {
      usize end=std::min(start+columns,state.importSummary.size());
      if(end<state.importSummary.size()) {const auto space=state.importSummary.rfind(' ',end);if(space!=std::string::npos&&space>start)end=space;}
      label({body.x,y,body.width,24},std::string_view(state.importSummary).substr(start,end-start),theme.color.danger,theme.type.caption);
      start=end;while(start<state.importSummary.size()&&state.importSummary[start]==' ')++start;y+=24;
    }
    return;
  }
  const float previewWidth=std::clamp(body.width*.36f,120.f,320.f);
  UiRect preview{body.x,body.y,previewWidth,std::max(0.f,body.height-24)};
  UiRect options{preview.right()+12,body.y,std::max(0.f,body.right()-preview.right()-12),body.height};
  list.addRect(preview,theme.color.canvas);
  if(!state.textureImportImage.isEmpty()&&preview.height>0) {
    const auto &texels=state.textureImportImage;
    const float fit=std::min(preview.width/texels.width,preview.height/texels.height);
    const float width=texels.width*fit,height=texels.height*fit;
    list.addPreviewImage({preview.x+(preview.width-width)*.5f,preview.y+(preview.height-height)*.5f,width,height},texels);
  } else {
    const auto text=state.importError?state.importSummary:std::string("Preparando prévia…");
    usize start=0;float y=preview.y+8;const usize columns=std::max<usize>(12,static_cast<usize>((preview.width-16)/7));
    while(start<text.size()&&y+24<=preview.bottom()) {
      usize end=std::min(text.size(),start+columns);
      if(end<text.size()) {const auto space=text.rfind(' ',end);if(space!=text.npos&&space>start)end=space;}
      label({preview.x+8,y,preview.width-16,24},std::string_view(text).substr(start,end-start),state.importError?theme.color.danger:theme.color.textMuted,theme.type.caption);
      start=end;while(start<text.size()&&text[start]==' ')++start;y+=24;
    }
  }
  label({preview.x,preview.bottom(),preview.width,24},std::to_string(state.textureImportSourceWidth)+"×"+
      std::to_string(state.textureImportSourceHeight)+(state.textureImportSourceHasAlpha?" · alfa":" · opaca"),theme.color.textDim,theme.type.caption,UiAlign::Center);

  const auto &profile=state.textureImportSettings;
  static constexpr const char *types[]{"Pelo uso","Cor (sRGB)","Dados (linear)","Mapa normal"};
  struct Setting {const char *name;std::string value;EditorWidget widget;bool enabled=true;};
  const bool normal=profile.interpretation==resources::TextureInterpretationNormal;
  static constexpr const char *extraModes[]{"Recusar","Ignorar","Usar como alfa"};
  const std::array<Setting,14> settings{{
    {"Interpretação",types[std::min<u32>(profile.interpretation,3)],EditorWidget::TextureProfileInterpretation},
    {"Tamanho máximo",profile.maximumDimension?std::to_string(profile.maximumDimension)+" px":"Projeto",EditorWidget::TextureProfileDimension},
    {"Mipmaps",profile.mipmaps?"Gerar":"Desligados",EditorWidget::TextureProfileMipmaps},
    {"Anisotropia",profile.anisotropy?"Permitir":"Desligada",EditorWidget::TextureProfileAnisotropy},
    {"Bordas alfa",profile.dilateEdges?"Dilatar":"Preservar",EditorWidget::TextureProfileEdges},
    {"Cobertura alfa",profile.preserveAlphaCoverage?"Preservar":"Desligada",EditorWidget::TextureProfileCoverage,!normal&&profile.interpretation!=resources::TextureInterpretationData},
    {"Normal Y",profile.invertNormalGreen?"Inverter (DX)":"Manter (GL)",EditorWidget::TextureProfileNormalGreen,normal},
    {"Corte de cobertura",std::to_string(static_cast<u32>(std::lround(profile.alphaCoverageCutoff*100)))+"%",EditorWidget::TextureProfileCoverageCutoff,!normal&&profile.preserveAlphaCoverage},
    {"Streaming de mips",profile.streamingMipmaps?"Sim":"Não",EditorWidget::TextureProfileStreaming},
    {"Prioridade",std::to_string(profile.streamingPriority),EditorWidget::TextureProfileStreamingPriority},
    {"Canal indefinido",state.textureImportTiffUndefinedExtra?extraModes[std::min<u8>(profile.tiffUndefinedExtra,2)]:"Declarado na fonte",EditorWidget::TextureTiffExtra,state.textureImportTiffUndefinedExtra},
    {"16 → 8 bits",state.textureImportTiffBits==16?(profile.tiffQuantize16?"Autorizar":"Recusar"):"Fonte de 8 bits",EditorWidget::TextureTiffPrecision,state.textureImportTiffBits==16},
    {"ICC desconhecido",state.textureImportTiffMatrixSrgb?"sRGB reconhecido":!state.textureImportTiffHasIcc?"Sem perfil":profile.tiffIgnoreColorProfile?"Valores sem conversão":"Recusar",EditorWidget::TextureTiffColor,state.textureImportTiffHasIcc&&!state.textureImportTiffMatrixSrgb},
    {"Perdas da conversão", "Ler relatório",EditorWidget::TextureTiffLosses}}};
  constexpr const char *groups[]{"Essenciais","Qualidade","Transparência","Normal / alfa","Streaming","Fonte / canais","Fonte / cor","Perdas"};
  const u32 count=textureImportPageCount(state),page=std::min(state.importPage,count-1);
  for(u32 n=0;page<7&&n<2;++n) {
    const auto &setting=settings[page*2+n];const UiRect row{options.x,options.y+n*48,options.width,48};
    list.addRect({row.x,row.bottom()-1,row.width,1},theme.color.silhouette);
    const UiRect nameArea{row.x+8,row.y,row.width*.48f-8,row.height};
    const UiRect valueArea{nameArea.right(),row.y,std::max(0.f,row.right()-nameArea.right()-8),row.height};
    label(nameArea,setting.name,setting.enabled?theme.color.text:theme.color.textMuted,theme.type.caption);
    label(valueArea,setting.value,setting.enabled?theme.color.accent:theme.color.textMuted,theme.type.caption,UiAlign::End);
    if(setting.enabled&&(state.importReady||(state.importError&&state.textureImportTiff)))router.addRegion(row,widgetId(setting.widget));
  }
  const UiRect pager{options.x,page==7?options.bottom()-48:options.y+96,options.width,48};
  if(page==7) {
    const auto text=state.importSummary.empty()?std::string("Nenhuma perda de canal, precisão ou perfil ICC nesta receita. A fonte original permanece íntegra."):state.importSummary;
    usize start=0;float y=options.y;const usize columns=std::max<usize>(16,static_cast<usize>(options.width/7));
    while(start<text.size()&&y+24<=pager.y) {
      usize end=std::min(text.size(),start+columns);
      if(end<text.size()) {const auto space=text.rfind(' ',end);if(space!=text.npos&&space>start)end=space;}
      label({options.x,y,options.width,24},std::string_view(text).substr(start,end-start),state.importError?theme.color.danger:theme.color.textDim,theme.type.caption);
      start=end;while(start<text.size()&&text[start]==' ')++start;y+=24;
    }
  }
  const UiRect previous{pager.x,pager.y,48,48},next{pager.right()-48,pager.y,48,48};
  action(previous,"‹",EditorWidget::ImportPreviousPage,page>0);
  action(next,"›",EditorWidget::ImportNextPage,page+1<count);
  label({previous.right(),pager.y,std::max(0.f,next.x-previous.right()),48},std::string(groups[page])+" · "+std::to_string(page+1)+" / "+std::to_string(count),theme.color.textDim,theme.type.caption,UiAlign::Center);
  if(page!=7&&options.height>166) {
    const auto text=!state.importSummary.empty()?state.importSummary:std::string("Wrap e filtro pertencem ao uso no Material.");
    usize start=0;float y=options.y+144;const usize columns=std::max<usize>(16,static_cast<usize>(options.width/7));
    while(start<text.size()&&y+24<=options.bottom()) {
      usize end=std::min(text.size(),start+columns);
      if(end<text.size()) {const auto space=text.rfind(' ',end);if(space!=text.npos&&space>start)end=space;}
      label({options.x,y,options.width,24},std::string_view(text).substr(start,end-start),state.importError?theme.color.danger:theme.color.textMuted,theme.type.caption);
      start=end;while(start<text.size()&&text[start]==' ')++start;y+=24;
    }
  }
}
}

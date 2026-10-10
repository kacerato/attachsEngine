#include "editor/editor_screen.h"
#include "resources/animation_binding_path.h"
#include "scene/skinned_mesh.h"
#include "runtime/transform_math.h"
#include "ui/ui_text.h"
#include <cmath>
#include <cstdio>
#include <unordered_set>

namespace ae::editor {
using namespace ui;
namespace {
namespace w=clip_widget;
UiRect centred(const UiRect &r,float w,float h) {return {r.x+(r.width-w)*.5f,r.y+(r.height-h)*.5f,w,h};}
std::string fitTextMiddle(const UiDrawList &list,std::string_view value,float width,const UiTypeStyle &style) {
  std::string text(value);
  const auto &metrics=list.fontMetrics();
  if(width<=0||measureTextWidth(text,metrics,style)<=width) return text;
  const auto start=[&](usize at){while(at>0&&at<text.size()&&(static_cast<u8>(text[at])&0xC0u)==0x80u)--at;return at;};
  const auto next=[&](usize at){while(at<text.size()&&(static_cast<u8>(text[at])&0xC0u)==0x80u)++at;return at;};
  for(usize keep=text.size();keep>1;--keep) {
    const usize head=start((keep+1)/2),tail=next(text.size()-keep/2);
    if(tail<head) continue;
    std::string candidate=text.substr(0,head)+"..."+text.substr(tail);
    if(measureTextWidth(candidate,metrics,style)<=width) return candidate;
  }
  return "...";
}
std::string number(double value,const char *suffix="") {char s[64];std::snprintf(s,sizeof s,"%.4g%s",value,suffix);return s;}
std::string bindingLabel(const resources::AnimationClipBinding &binding) {
  if(binding.path.empty())return binding.name;
  std::string label;
  for(usize start=0;start<binding.path.size();) {
    const auto end=binding.path.find('/',start);std::string name;
    if(!resources::animationBindingName(std::string_view(binding.path).substr(start,end==std::string::npos?binding.path.size()-start:end-start),name))return binding.name;
    if(!label.empty())label+=" › ";
    label+=name;
    if(end==std::string::npos)break;
    start=end+1;
  }
  return label;
}
const char *pathName(const resources::AnimationClipTrack &track) {
  switch(track.path) {case resources::AnimationPath::Translation:return "Posição";case resources::AnimationPath::Rotation:return track.rotationMode==resources::AnimationRotationMode::Euler?"Euler":track.rotationMode==resources::AnimationRotationMode::ProgressiveQuaternion?"Progressivo":"Quaternion";
    case resources::AnimationPath::Scale:return "Escala";default:return "Morph";}
}
std::string componentName(const resources::AnimationClipTrack &track,u32 component) {
  if(track.path==resources::AnimationPath::Rotation&&component==4)return "Progresso °";
  if(track.path==resources::AnimationPath::Weights)return std::to_string(component);
  return component<4?std::string(1,"XYZW"[component]):std::to_string(component);
}
const char *tangentName(resources::AnimationTangentMode mode) {
  switch(mode) {case resources::AnimationTangentMode::Free:return "Livre";case resources::AnimationTangentMode::Linear:return "Linear";
    case resources::AnimationTangentMode::Constant:return "Degrau";case resources::AnimationTangentMode::Auto:return "Auto";
    case resources::AnimationTangentMode::ClampedAuto:return "Sem overshoot";case resources::AnimationTangentMode::Flat:return "Plana";default:return "Degrau inverso";}
}
UiRect top(UiRect &r,float h) {const float height=std::min(h,r.height);UiRect result{r.x,r.y,r.width,height};r.y+=height;r.height-=height;return result;}
UiRect left(UiRect &r,float w) {const float width=std::min(w,r.width);UiRect result{r.x,r.y,width,r.height};r.x+=width;r.width-=width;return result;}
}
EditorScreenLayout buildAnimationClipScreen(const EditorScreenState &state,const UiTheme &theme,UiDrawList &list,UiInputRouter &router) {
  EditorScreenLayout layout;const auto *asset=state.clipAsset;if(!asset)return layout;
  auto area=deflate(state.surface,state.safeArea);
  // Nomes de recurso, camada e objeto costumam diferir no fim ("Consolidado 2",
  // "Base · mecanismo"): o texto que não cabe perde o meio, não o fim.
  const auto label=[&](const UiRect &r,std::string_view text,UiColor color,UiAlign align=UiAlign::Start) {
    const auto inner=deflate(r,{6,0,6,0});
    list.pushClip(r);list.addText(inner,fitTextMiddle(list,text,inner.width,theme.type.caption),color,theme.type.caption,align);list.popClip();
  };
  const auto button=[&](UiRect r,std::string_view text,u32 code,bool enabled=true,bool active=false,UiIcon icon=UiIcon::None) {
    const auto hit=r;r=deflate(r,{2,3,2,3});const auto foreground=enabled?theme.color.text:theme.color.textFaint;
    list.addRect(r,active?withAlpha(theme.color.accent,.16f):state.pressedWidget==w::id(code)?theme.color.line:theme.color.raised,2);
    if(active)list.addRect({r.x,r.bottom()-2,r.width,2},theme.color.accent);
    if(icon!=UiIcon::None) {
      auto slot=left(r,text.empty()?r.width:30);list.addImage(centred(slot,22,22),static_cast<UiImageId>(icon),enabled?0xffffffff:withAlpha(0xffffffff,.35f));
    }
    if(!text.empty())label(r,text,foreground,UiAlign::Center);
    if(enabled)router.addRegion(hit,w::id(code));
  };
  layout.topBar=top(area,48);list.addRect(layout.topBar,theme.color.surface);router.addBlocker(layout.topBar);
  auto header=layout.topBar;
  auto close=UiRect{header.right()-44,header.y,44,header.height};header.width-=44;button(close,"",w::Close,true,false,UiIcon::UiClose);
  button(left(header,header.width>620?110:42),header.width>620?"Objetos":"",w::Targets,true,state.clipAuthoringPicker!=0,UiIcon::AnimationBinding);
  button(left(header,std::min(210.f,header.width*.29f)),asset->name,w::Choose,true,state.clipPicker,UiIcon::AssetsAnimation);
  button(left(header,42),"",w::Undo,state.canUndo,false,UiIcon::EditorUndo);
  button(left(header,42),"",w::Redo,state.canRedo,false,UiIcon::EditorRedo);
  button(left(header,42),"",w::Frame,true,false,UiIcon::AnimationFrame);
  button(left(header,42),"",w::Expand,true,state.clipExpanded,UiIcon::EditorAuthorFrame);
  if(header.width>=56)button(left(header,56),"Nome",w::Name);
  if(header.width>=84)button(left(header,84),"Pose",w::Pose,state.clipTarget!=0&&!state.clipExpanded,state.clipPoseShown,UiIcon::AnimationReferencePose);
  if(header.width>=52)button(left(header,52),"Novo",w::New);
  const resources::AnimationClipTrack *selected=asset->track(state.clipTrack);
  const resources::AnimationCurve *curve=selected&&state.clipComponent<selected->curves.size()?&selected->curves[state.clipComponent]:nullptr;
  const resources::AnimationCurveKey *key=nullptr;if(curve)for(const auto &k:curve->keys)if(k.id==state.clipKey)key=&k;
  // Keep the graph's useful height when key properties appear. Idle editing
  // preserves the viewport; selecting a key lends space to the authoring task.
  const bool multi=state.clipSelectionMode&&!state.clipSelection.empty()&&!state.clipSelecting;
  const bool bake=state.clipBakeShown&&selected;
  const bool layers=state.clipLayersShown;
  const bool cues=state.clipCues&&!bake&&!layers;
  const auto *cue=cues?asset->cue(state.clipCue):nullptr;
  const bool previewFocus=!cues&&state.clipPoseShown&&state.clipPreviewExpanded&&!bake&&!layers&&!state.clipExpanded;
  const float editingHeight=previewFocus?42:state.clipExpanded?area.height:state.clipPoseShown&&!bake&&!layers&&!cues?
      std::min(148.f,area.height*.4f):std::min(std::max(0.f,area.height-std::min(110.f,area.height*.25f)),std::max(172.f,area.height*.46f)+(key||multi||bake||layers||cue?42.f:0.f));
  layout.viewport={area.x,area.y,area.width,std::max(0.f,area.height-editingHeight)};
  UiRect editor{area.x,layout.viewport.bottom(),area.width,editingHeight};
  list.addRect(editor,theme.color.canvas);router.addBlocker(editor);
  if(cue) {
    auto property=top(editor,42);list.addRect(property,theme.color.surface);const float width=property.width;
    button(left(property,width*.29f),cue->name,w::CueName,true,true,cue->kind==resources::AnimationCueKind::Event?UiIcon::AnimationEvent:UiIcon::AnimationMarker);
    button(left(property,width*.18f),number(cue->time," s"),w::CueTime);
    button(left(property,width*.16f),"#"+std::to_string(cue->tag),w::CueTag,cue->kind==resources::AnimationCueKind::Event);
    button(left(property,width*.18f),"Valor · "+number(cue->value),w::CueValue,cue->kind==resources::AnimationCueKind::Event);
    button(property,"Opções",w::CueOptions,true,state.clipCueOptions);
  } else if(multi&&!bake&&!layers&&!previewFocus&&!cues) {
    auto property=top(editor,42);list.addRect(property,theme.color.surface);
    label(left(property,std::min(150.f,property.width*.25f)),std::to_string(state.clipSelection.size())+" chaves",theme.color.textDim);
    button(left(property,110),"Mover · s",w::SelectionOffset);
    button(left(property,110),"Escala",w::SelectionScale);
    button(left(property,84),"Somar",w::SelectionAdd,true,state.clipSelectionAdd);
    button(left(property,74),"Limpar",w::SelectionClear);
  } else if(key&&!bake&&!layers&&!previewFocus&&!cues) {
    auto property=top(editor,42);list.addRect(property,theme.color.surface);
    const float nameWidth=std::min(160.f,property.width*.22f);
    const bool progressive=selected&&selected->rotationMode==resources::AnimationRotationMode::ProgressiveQuaternion;
    label(left(property,nameWidth),selected?std::string(pathName(*selected))+" / "+componentName(*selected,state.clipComponent):"Canal",theme.color.textDim);
    button(left(property,110),number(key->time," s"),w::KeyTime);
    button(left(property,110),number(key->value),w::KeyValue,!progressive||state.clipComponent!=4);
    button(left(property,std::min(130.f,property.width*.45f)),tangentName(key->outgoing),w::Tangent,!progressive||state.clipComponent==4,false,UiIcon::AnimationTangent);
  }
  const float transportHeight=bake||layers?84.f:42.f;
  auto transport=UiRect{editor.x,editor.bottom()-transportHeight,editor.width,transportHeight};editor.height=std::max(0.f,editor.height-transportHeight);
  list.addRect(transport,theme.color.surface);
  if(layers) {
    const auto *layer=asset->layer(state.clipLayer);const auto index=static_cast<usize>(layer-asset->layers.data());
    auto actions=top(transport,42);const float width=actions.width;
    button(left(actions,42),"",w::LayerPrevious,index>0,false,UiIcon::UiChevronLeft);
    button(left(actions,42),"",w::LayerNext,index+1<asset->layers.size(),false,UiIcon::UiChevronRight);
    button(left(actions,std::max(90.f,width-84-7*42)),std::to_string(index+1)+" / "+std::to_string(asset->layers.size())+" · "+layer->name,w::LayerChoose,true,true,UiIcon::AnimationLayers);
    button(left(actions,42),"Nome",w::LayerName);
    button(left(actions,42),"+",w::LayerAdd,asset->layers.size()<resources::AnimationClipAsset::MaximumLayers);
    button(left(actions,42),"",w::LayerDuplicate,asset->layers.size()<resources::AnimationClipAsset::MaximumLayers,false,UiIcon::AnimationDuplicate);
    button(left(actions,42),"",w::LayerRemove,layer->id!=0,false,UiIcon::UiRemove);
    button(left(actions,42),"↑",w::LayerUp,index>1);
    button(left(actions,42),"↓",w::LayerDown,index>0&&index+1<asset->layers.size());
    button(actions,"",w::LayerClose,true,false,UiIcon::UiClose);
    auto properties=top(transport,42);const bool additive=layer->blend==resources::AnimationAuthorBlend::Additive;
    button(left(properties,width*.19f),layer->id?(additive?"Aditiva":"Substituir"):"Base",w::LayerBlend,layer->id!=0,false,UiIcon::AnimationAdditive);
    button(left(properties,width*.15f),"Peso · "+number(layer->weight),w::LayerWeight,layer->id!=0);
    button(left(properties,width*.17f),layer->referenceTime<0?"Ref. neutra":"Ref. tempo",w::LayerReference,layer->id!=0&&additive,false,UiIcon::AnimationReferencePose);
    button(left(properties,width*.13f),number(std::max(0.f,layer->referenceTime)," s"),w::LayerReferenceTime,additive&&layer->referenceTime>=0);
    button(left(properties,width*.12f),"Mudo",w::LayerMute,true,layer->muted,UiIcon::AnimationLayerMute);
    button(left(properties,width*.12f),"Solo",w::LayerSolo,true,layer->solo,UiIcon::AnimationLayerSolo);
    button(properties,"Da base",w::LayerCopy,layer->id&&selected,false,UiIcon::AssetsCopy);
  } else if(bake) {
    auto controls=top(transport,42);const float width=controls.width;
    if(state.clipBakeReferenceShown&&state.clipBakeSettings.rotation==1) {
      button(left(controls,width*.28f),"Ramo XYZ",w::BakeSeedUse,true,state.clipBakeSettings.eulerReferenceExplicit,UiIcon::AnimationRotationBranch);
      for(u32 c=0;c<3;++c)button(left(controls,width*.24f),std::string(c==0?"X · ":c==1?"Y · ":"Z · ")+number(state.clipBakeSettings.eulerReference[c]," °"),w::BakeSeedX+c);
    } else {
    button(left(controls,width*.21f),std::to_string(state.clipBakeSettings.sampleRate)+" amostras/s",w::BakeRate);
    const bool rotation=state.clipBakeConsolidate||selected->path==resources::AnimationPath::Rotation;const auto mode=state.clipBakeSettings.rotation;
    button(left(controls,width*.25f),"Erro · "+number(static_cast<float>(state.clipBakeSettings.tolerance),rotation?" °":""),w::BakeTolerance);
    button(left(controls,width*.32f),!rotation||mode==3?"Preservar":mode==0?"Quaternion":mode==1?"Euler XYZ":"Progressivo",w::BakeMode,rotation);
    button(controls,"Redução",w::BakeReduction,true,state.clipBakeSettings.reduce,UiIcon::AnimationReduce);
    }
    const auto &report=state.clipBakeReport;auto outcome=top(transport,42);
    button(left(outcome,118),state.clipBakeConsolidate?"Clipe novo":"Canal",w::BakeTarget,true,state.clipBakeConsolidate,UiIcon::AnimationConsolidate);
    if(state.clipBakeSettings.rotation==1)button(left(outcome,74),"XYZ",w::BakeReference,true,state.clipBakeReferenceShown,UiIcon::AnimationRotationBranch);
    auto close=UiRect{outcome.right()-42,outcome.y,42,outcome.height};outcome.width-=42;
    auto apply=UiRect{outcome.right()-110,outcome.y,110,outcome.height};outcome.width-=110;
    label({outcome.x,outcome.y,outcome.width,21},state.clipBakeHasReport?std::to_string(report.inputKeys)+" → "+std::to_string(report.outputKeys)+" chaves":state.clipBakeConsolidate?"Original preservado":"Bake · "+std::string(pathName(*selected)),theme.color.textDim);
    label({outcome.x,outcome.y+21,outcome.width,21},state.clipBakeHasReport?"Erro · "+number(static_cast<float>(report.maximumError),state.clipBakeReportConsolidated?" u/°":selected->path==resources::AnimationPath::Rotation?" °":"")+" · "+std::to_string(report.verifiedSamples)+" pontos":state.clipBakeSettings.rotation==1&&!state.clipBakeSettings.eulerReferenceExplicit?"Confirme o ramo XYZ":state.clipBakeConsolidate?"Graus / unidades por canal":"Verificação amostrada",theme.color.textMuted);
    const bool needsBranch=state.clipBakeSettings.rotation==1&&(state.clipBakeConsolidate||selected->rotationMode!=resources::AnimationRotationMode::Euler);
    button(apply,state.clipBakeConsolidate?"Criar":"Aplicar",w::BakeApply,!needsBranch||state.clipBakeSettings.eulerReferenceExplicit,false,UiIcon::AnimationBake);button(close,"",w::BakeClose,true,false,UiIcon::UiClose);
  } else {
  const bool compact=state.surface.width<768;
  button(left(transport,compact?44:102),compact?"":"Camadas",w::Layers,true,false,UiIcon::AnimationLayers);
  button(left(transport,44),"",w::Keys,true,!state.clipCurves&&!cues,UiIcon::AnimationKey);
  button(left(transport,44),"",w::Curves,true,state.clipCurves&&!cues,UiIcon::AnimationCurve);
  button(left(transport,44),"",w::Cues,true,cues,UiIcon::AnimationEvent);
  if(!cues)button(left(transport,44),"",w::Selection,true,state.clipSelectionMode,UiIcon::EditorAuthorSelect);
  button(left(transport,44),"Editar",w::Edits,true,state.clipEditPicker);
  button(left(transport,44),"|<",w::Restart);
  button(left(transport,40),"",w::PreviousFrame,true,false,UiIcon::UiChevronLeft);
  button(left(transport,44),"",w::Play,state.clipDiagnostic.empty(),state.clipPlaying,state.clipPlaying?UiIcon::RuntimePause:UiIcon::RuntimePlay);
  button(left(transport,40),"",w::NextFrame,true,false,UiIcon::UiChevronRight);
  button(left(transport,44),"Loop",w::Loop,true,state.clipLoop);
  button(left(transport,76),number(state.clipTime," s"),w::Time);
  if(transport.width>=90)button(left(transport,90),number(asset->duration," s"),w::Duration);
  if(transport.width>=88) {button(left(transport,40),"-",w::ZoomOut);button(left(transport,40),"+",w::ZoomIn);}
  }
  if(cues) {
    auto body=editor;auto names=left(body,std::min(185.f,body.width*.27f));layout.clipRows=names;
    auto actions=top(names,42);
    const auto addCue=[&](UiRect r,u32 code,UiIcon icon) {
      const bool enabled=asset->cues.size()<resources::MaximumAnimationCues;
      button(r,"",code,enabled,false,icon);
      const auto color=enabled?theme.color.text:theme.color.textFaint;
      list.addLine({r.right()-14,r.y+8},{r.right()-6,r.y+8},color,1.5f);
      list.addLine({r.right()-10,r.y+4},{r.right()-10,r.y+12},color,1.5f);
    };
    addCue(left(actions,42),w::CueAddEvent,UiIcon::AnimationEvent);
    addCue(left(actions,42),w::CueAddMarker,UiIcon::AnimationMarker);
    button(left(actions,42),"<",w::CuePrevious,!asset->cues.empty());button(actions,">",w::CueNext,!asset->cues.empty());
    label(top(names,26),"Eventos · "+std::to_string(std::count_if(asset->cues.begin(),asset->cues.end(),[](const auto &c){return c.kind==resources::AnimationCueKind::Event;})),theme.color.text);
    label(top(names,26),"Marcadores",theme.color.textDim);
    const auto ruler=top(body,34);const auto canvas=deflate(body,{2,2,8,3});layout.clipCanvas=canvas;router.addRegion(canvas,w::id(w::Canvas));
    const float span=std::max(.0001f,state.clipEnd-state.clipStart);
    const auto xAt=[&](float t){return canvas.x+(t-state.clipStart)/span*canvas.width;};
    for(u32 i=0;i<=4;++i) {const float x=canvas.x+canvas.width*i/4;
      label({x-24,ruler.y,55,ruler.height},number(state.clipStart+span*i/4,"s"),theme.color.textMuted,UiAlign::Center);
      list.addLine({x,canvas.y},{x,canvas.bottom()},theme.color.lineSoft,1);
    }
    list.pushClip(canvas);
    for(u32 row=0;row<2;++row) {const float y=canvas.y+canvas.height*(row?.7f:.3f);list.addLine({canvas.x,y},{canvas.right(),y},theme.color.line,1);}
    for(const auto &c:asset->cues)if(c.time>=state.clipStart&&c.time<=state.clipEnd) {
      const float x=xAt(c.time),y=canvas.y+canvas.height*(c.kind==resources::AnimationCueKind::Event?.3f:.7f);
      const bool active=c.id==state.clipCue;const auto color=active?theme.color.accent:c.enabled?theme.color.text:theme.color.textFaint;
      list.addImage({x-8,y-8,16,16},static_cast<UiImageId>(c.kind==resources::AnimationCueKind::Event?UiIcon::AnimationEvent:UiIcon::AnimationMarker),c.enabled?0xffffffff:withAlpha(0xffffffff,.35f));
      if(active) {list.addBorder({x-11,y-11,22,22},color,1,0);label({x+14,y-13,std::min(160.f,canvas.right()-x-14),26},c.name,color);}
    }
    const float playhead=xAt(state.clipTime);list.addLine({playhead,canvas.y},{playhead,canvas.bottom()},theme.color.accent,1.3f);
    if(asset->cues.empty())label({canvas.x+10,canvas.y+4,canvas.width-20,32},"+ Evento · + Marcador",theme.color.textDim);
    list.popClip();
  }
  if(!previewFocus&&!cues) {
  auto body=editor;const float rowsWidth=std::min(185.f,body.width*.27f);
  auto names=left(body,rowsWidth);layout.clipRows=names;
  auto navigation=top(names,34);button(left(navigation,36),"<",w::RowsPrevious,state.clipRow>0);button(left(navigation,36),">",w::RowsNext);
  button(left(navigation,42),"",w::AddKey,selected!=nullptr,false,UiIcon::AnimationKey);button(left(navigation,42),"",w::DeleteKey,multi||(key&&curve->keys.size()>1),false,UiIcon::UiRemove);
  const auto ruler=top(body,34);layout.clipCanvas=deflate(body,{2,2,8,3});
  const auto &canvas=layout.clipCanvas;router.addRegion(canvas,w::id(w::Canvas));
  layout.clipVisibleRows=std::min(64u,std::max(1u,static_cast<u32>(names.height/30)));
  u32 index=0,visible=0;
  struct VisibleRow {const resources::AnimationClipTrack *track;u32 component;};std::vector<VisibleRow> visibleRows;
  for(const auto &t:asset->tracks)if(t.layer==state.clipLayer)for(u32 c=0;c<t.components();++c,++index) {
    if(index<state.clipRow||visible>=layout.clipVisibleRows)continue;
    const auto binding=std::find_if(asset->bindings.begin(),asset->bindings.end(),[&](const auto &b){return b.id==t.binding;});
    const std::string path=binding==asset->bindings.end()?"?":bindingLabel(*binding);
    const auto row=top(names,30);const bool active=t.id==state.clipTrack&&c==state.clipComponent;
    const bool progress=t.rotationMode==resources::AnimationRotationMode::ProgressiveQuaternion&&c==4;
    if(active) {list.addRect(row,withAlpha(theme.color.accent,.12f));list.addRect({row.x,row.y,2,row.height},theme.color.accent);}
    label({row.x+2,row.y,row.width-30,15},path,theme.color.textDim);
    label({row.x+2,row.y+15,row.width-30,15},progress?"Progresso °":pathName(t),active?theme.color.text:theme.color.textMuted);
    const UiColor axes[]{theme.color.axisX,theme.color.axisY,theme.color.axisZ,theme.color.textDim};
    label({row.right()-30,row.y,30,row.height},progress?"°":componentName(t,c),axes[c%4],UiAlign::Center);
    router.addRegion(row,w::id(w::Row+visible));visibleRows.push_back({&t,c});++visible;
  }
  layout.clipVisibleRows=std::max(1u,visible);
  if(!selected) {
    label({canvas.x+8,canvas.y+8,std::max(0.f,canvas.width-16),32},"Camada vazia · Objetos → escolher propriedade",theme.color.textDim);
    button({canvas.x+8,canvas.y+42,156,42},"Adicionar canal",w::Targets,true,false,UiIcon::AnimationBinding);
  }
  if(!canvas.isEmpty()&&curve) {
    const float span=std::max(.0001f,state.clipEnd-state.clipStart);
    const auto xAt=[&](double t){return canvas.x+static_cast<float>((t-state.clipStart)/span*canvas.width);};
    const auto yAt=[&](double v){return canvas.bottom()-static_cast<float>((v-state.clipMinimum)/(state.clipMaximum-state.clipMinimum)*canvas.height);};
    for(u32 i=0;i<=8;++i) {
      const float x=canvas.x+canvas.width*i/8;list.addLine({x,canvas.y},{x,canvas.bottom()},theme.color.lineSoft,1);
      label({x-25,ruler.y,58,ruler.height},number(state.clipStart+span*i/8,"s"),theme.color.textMuted,UiAlign::Center);
    }
    list.pushClip(canvas);
    if(state.clipCurves) {
      for(u32 i=0;i<=4;++i) {const float y=canvas.y+canvas.height*i/4;list.addLine({canvas.x,y},{canvas.right(),y},theme.color.lineSoft,1);}
      const UiColor colors[]{theme.color.axisX,theme.color.axisY,theme.color.axisZ,theme.color.textDim};
      // Bounded sampling regardless of source key count and zoom. Render the
      // selected component only; unrelated units cannot share one value axis.
      UiPoint previous{};bool hasPrevious=false;
      for(u32 i=0;i<=384;++i) {
        const double t=state.clipStart+span*i/384;resources::AnimationCurveSample sample;
        if(resources::sampleValidatedAnimationCurve(*curve,t,sample)) {const UiPoint point{xAt(t),yAt(sample.value)};
          if(hasPrevious)list.addLine(previous,point,colors[state.clipComponent%4],1.8f);
          previous=point;hasPrevious=true;
        } else hasPrevious=false;
      }
    }
    std::unordered_set<u64> selectedIds;for(const auto &address:state.clipSelection)selectedIds.insert(address.key);
    const auto diamond=[&](const resources::AnimationCurveKey &k,float y,bool selectedCurve) {
      const UiPoint p{xAt(k.time),y};
      const bool chosen=state.clipSelectionMode?selectedIds.contains(k.id):selectedCurve&&k.id==state.clipKey;
      const UiColor color=chosen?theme.color.accent:selectedCurve?theme.color.text:theme.color.textDim;
      list.addLine({p.x-5,p.y},{p.x,p.y-5},color,2);list.addLine({p.x,p.y-5},{p.x+5,p.y},color,2);
      list.addLine({p.x+5,p.y},{p.x,p.y+5},color,2);list.addLine({p.x,p.y+5},{p.x-5,p.y},color,2);
      if(chosen)list.addBorder({p.x-7,p.y-7,14,14},theme.color.accent,2,1);
    };
    if(state.clipCurves) {
      const usize step=std::max<usize>(1,(curve->keys.size()+511)/512);
      for(usize i=0;i<curve->keys.size();i+=step)if(const auto &k=curve->keys[i];k.time>=state.clipStart&&k.time<=state.clipEnd)diamond(k,yAt(k.value),true);
      if(key&&key->time>=state.clipStart&&key->time<=state.clipEnd)diamond(*key,yAt(key->value),true);
      if(key&&!multi&&(!selected||selected->rotationMode!=resources::AnimationRotationMode::ProgressiveQuaternion||state.clipComponent==4)) {
        const usize index=static_cast<usize>(key-curve->keys.data());const UiPoint anchor{xAt(key->time),yAt(key->value)};
        for(bool incoming:{true,false}) {
          if((incoming&&!index)||(!incoming&&index+1==curve->keys.size()))continue;
          const auto mode=incoming?key->incoming:key->outgoing;
          if(mode==resources::AnimationTangentMode::Constant||mode==resources::AnimationTangentMode::NextConstant||mode==resources::AnimationTangentMode::Linear)continue;
          const float span=incoming?key->time-curve->keys[index-1].time:curve->keys[index+1].time-key->time;
          const float weight=(incoming?key->weightedIn:key->weightedOut)?(incoming?key->inWeight:key->outWeight):1.f/3;
          const double dx=(incoming?-1:1)*span*weight;
          const UiPoint handle{xAt(key->time+dx),yAt(key->value+dx*resources::animationCurveSlope(*curve,index,incoming))};
          list.addLine(anchor,handle,theme.color.textMuted,1);list.addBorder({handle.x-4,handle.y-4,8,8},theme.color.accent,1,0);
          if(canvas.contains(handle))router.addRegion({handle.x-10,handle.y-10,20,20},w::id(incoming?w::HandleIn:w::HandleOut));
        }
      }
    } else for(usize row=0;row<visibleRows.size();++row) {
      const auto &entry=visibleRows[row];const bool selectedCurve=entry.track->id==state.clipTrack&&entry.component==state.clipComponent;
      const float y=canvas.y+row*30+13;
      if(selectedCurve)list.addRect({canvas.x,canvas.y+row*30,canvas.width,30},withAlpha(theme.color.accent,.08f));
      list.addLine({canvas.x,y+15},{canvas.right(),y+15},theme.color.lineSoft,1);
      const auto &rowCurve=entry.track->curves[entry.component];const usize step=std::max<usize>(1,(rowCurve.keys.size()+511)/512);
      for(usize i=0;i<rowCurve.keys.size();i+=step)if(const auto &k=rowCurve.keys[i];k.time>=state.clipStart&&k.time<=state.clipEnd)diamond(k,y,selectedCurve);
      if(selectedCurve&&key&&key->time>=state.clipStart&&key->time<=state.clipEnd)diamond(*key,y,true);
    }
    const float playhead=xAt(state.clipTime);list.addLine({playhead,canvas.y},{playhead,canvas.bottom()},theme.color.accent,1.3f);
    if(state.clipSelecting) {list.addRect(state.clipSelectionBox,withAlpha(theme.color.accent,.08f));list.addBorder(state.clipSelectionBox,theme.color.accent,1,0);}
    list.popClip();
    if(state.clipCurves)for(u32 i=0;i<3;++i) {
      const float value=state.clipMinimum+(state.clipMaximum-state.clipMinimum)*i/2;
      const UiRect marker{canvas.x+4,yAt(value)-8,44,16};list.addRect(marker,theme.color.canvas);label(marker,number(value),theme.color.textMuted);
    }
  }
  if(state.clipLayerPicker&&layers) {
    const u32 first=state.clipLayerPage*4,count=static_cast<u32>(std::min<usize>(4,asset->layers.size()-std::min<usize>(first,asset->layers.size())));
    UiRect picker{area.x+8,std::max(area.y,editor.bottom()-42*float(count+1)),std::min(370.f,area.width-16),42*float(count+1)};
    list.addRect(picker,theme.color.surface);list.addBorder(picker,theme.color.line,1,0);router.addBlocker(picker);
    auto nav=top(picker,42);label(left(nav,nav.width-126),"Camadas · ordem de composição",theme.color.textDim);
    button(left(nav,42),"<",w::LayerPickerPrevious,state.clipLayerPage>0);button(left(nav,42),">",w::LayerPickerNext,first+count<asset->layers.size());
    button(nav,"",w::LayerPickerClose,true,false,UiIcon::UiClose);
    for(u32 i=0;i<count;++i) {
      const auto &layer=asset->layers[first+i];
      button(top(picker,42),std::to_string(first+i+1)+" · "+layer.name+(layer.muted?" · M":layer.solo?" · S":""),w::LayerChoice+i,true,layer.id==state.clipLayer,UiIcon::AnimationLayers);
    }
  }
  }
  const auto previewTarget=state.clipPreviewTarget?state.clipPreviewTarget:state.clipTarget;
  const bool targetChannel=previewTarget==state.clipTarget;
  if(state.clipPoseShown&&!state.clipExpanded&&state.clipPoseGraph&&state.view&&state.clipPoseJoints) {
    // These are the actual evaluated joint origins. Deliberately draw an
    // X-ray overlay so covered joints remain discoverable; the hierarchy is
    // the precise alternative when several joints project onto one pixel.
    const auto &graph=*state.clipPoseGraph;
    list.pushClip(layout.viewport);
    for(const auto bone:state.clipPreviewJoints) {
      const auto *node=graph.find(bone);if(!node||!graph.activeInHierarchy(bone)||!node->visible)continue;
      bool visible=true;
      for(auto parent=node;parent;parent=graph.find(parent->parent))
        if(!parent->visible||state.sceneHiddenHas(parent->id)||(parent->layer<32&&(state.hiddenLayers&(1u<<parent->layer)))) {visible=false;break;}
      if(!visible)continue;
      float world[16];if(!runtime::worldMatrix(graph,bone,world))continue;
      const auto projected=projectWorldToScreen(*state.view,world+12);if(!projected.valid)continue;
      const bool active=bone==previewTarget;
      if(std::binary_search(state.clipPreviewJoints.begin(),state.clipPreviewJoints.end(),node->parent)) {
        float parent[16];UiPoint a,b;
        if(runtime::worldMatrix(graph,node->parent,parent)&&projectSegmentToScreen(*state.view,parent+12,world+12,a,b))
          list.addLine(a,b,active?theme.color.accent:withAlpha(theme.color.text,.55f),active?3:1.5f);
      }
      if(layout.viewport.contains(projected.screen)) {
        const float radius=active?6:3;
        list.addRect({projected.screen.x-radius-1,projected.screen.y-radius-1,2*radius+2,2*radius+2},theme.color.canvas,radius+1);
        list.addRect({projected.screen.x-radius,projected.screen.y-radius,2*radius,2*radius},active?theme.color.accent:theme.color.text,radius);
      }
    }
    list.popClip();
  }
  if(state.clipPoseShown&&targetChannel&&state.clipPoseIsolated&&!state.clipExpanded&&state.clipPoseGraph&&state.view&&selected&&selected->path!=resources::AnimationPath::Weights) {
    float world[16];EditorGizmoSettings settings;settings.screenLengthPixels=72;
    const auto frame=runtime::worldMatrix(*state.clipPoseGraph,state.clipTarget,world)?
        selected->path==resources::AnimationPath::Scale?buildLocalScaleGizmoFrame(*state.view,world,settings):buildGizmoFrame(*state.view,world+12,settings):EditorGizmoFrame{};
    if(frame.valid) {
      list.pushClip(layout.viewport);
      const UiColor colours[]{theme.color.axisX,theme.color.axisY,theme.color.axisZ};
      for(u32 axis=0;axis<3;++axis) {
        const bool active=static_cast<u32>(state.clipPoseAxis)==axis+1;
        const auto color=active?theme.color.accent:colours[axis];
        if(selected->path==resources::AnimationPath::Rotation) {
          for(u32 segment=0;segment<64;++segment) {
            float from[3],to[3];gizmoRingPoint(frame.origin,axis,frame.axisWorldLength,segment*6.28318530718f/64,from);
            gizmoRingPoint(frame.origin,axis,frame.axisWorldLength,(segment+1)*6.28318530718f/64,to);
            UiPoint a,b;if(!projectSegmentToScreen(*state.view,from,to,a,b))continue;
            list.addLine(a,b,color,active?4:2);
            const UiPoint mid{(a.x+b.x)*.5f,(a.y+b.y)*.5f};float angle;
            if(layout.viewport.contains(mid)&&gizmoRingAngle(*state.view,frame.origin,axis,mid,angle))router.addRegion({mid.x-9,mid.y-9,18,18},w::id(w::PoseGizmo+axis));
          }
        } else if(frame.axisUsable[axis]) {
          list.addLine(frame.originScreen,frame.axisEndScreen[axis],color,active?5:3);
          const auto tip=frame.axisEndScreen[axis];list.addRect({tip.x-6,tip.y-6,12,12},color,selected->path==resources::AnimationPath::Scale?0:6);
          for(u32 step=3;step<=12;++step) {
            const float t=step/12.f,side=step==12?theme.touch.minimumTarget:18;
            const UiPoint at{frame.originScreen.x+t*(tip.x-frame.originScreen.x),frame.originScreen.y+t*(tip.y-frame.originScreen.y)};
            const auto hit=intersect(layout.viewport,{at.x-side*.5f,at.y-side*.5f,side,side});
            if(!hit.isEmpty())router.addRegion(hit,w::id(w::PoseGizmo+axis));
          }
        }
      }
      list.addRect({frame.originScreen.x-3,frame.originScreen.y-3,6,6},theme.color.text,3);list.popClip();
    }
  }
  if(state.clipPoseShown&&!state.clipExpanded&&!layout.viewport.isEmpty()) {
    auto tools=deflate(UiRect{layout.viewport.x,layout.viewport.y+4,layout.viewport.width,44},{8,0,8,0});
    list.addRect(tools,theme.color.surface);router.addBlocker(tools);
    button(left(tools,100),"Juntas",w::PoseJoints,!state.clipPreviewJoints.empty(),state.clipPoseJoints&&!state.clipPreviewJoints.empty(),UiIcon::AnimationSkeleton);
    const auto path=targetChannel&&selected?selected->path:resources::AnimationPath::Weights;
    button(left(tools,44),"",w::PoseTranslate,previewTarget!=0,path==resources::AnimationPath::Translation,UiIcon::EditorMove);
    button(left(tools,44),"",w::PoseRotate,previewTarget!=0,path==resources::AnimationPath::Rotation,UiIcon::EditorRotate);
    button(left(tools,44),"",w::PoseScale,previewTarget!=0,path==resources::AnimationPath::Scale,UiIcon::EditorScale);
    button(left(tools,56),targetChannel&&selected&&selected->path==resources::AnimationPath::Weights?"Pesos":"XYZ",w::PoseNumbers,targetChannel&&selected,state.clipPoseNumbers);
    button(left(tools,44),"",w::PoseFocus,previewTarget!=0,false,UiIcon::EditorFrameObject);
    button(left(tools,44),"",w::PoseOrbit,true,state.navigation==EditorNavigationMode::Orbit,UiIcon::EditorOrbit);
    button(left(tools,44),"",w::PosePan,true,state.navigation==EditorNavigationMode::Pan,UiIcon::EditorPan);
    button(left(tools,44),"",w::PoseZoom,true,state.navigation==EditorNavigationMode::Zoom,UiIcon::EditorAuthorZoom);
    button(left(tools,44),"",w::PoseViewport,true,previewFocus,UiIcon::EditorAuthorFrame);
    const auto *object=state.document?state.document->find(previewTarget):nullptr;
    label(tools,object?std::string(object->name)+(targetChannel?"":" · escolher canal"):"Toque uma junta ou objeto",theme.color.textDim);
  }
  if(state.clipPoseShown&&targetChannel&&selected&&state.clipPoseCount&&!layout.viewport.isEmpty()) {
    auto actions=deflate(UiRect{layout.viewport.x,layout.viewport.bottom()-44,layout.viewport.width,44},{8,0,8,0});
    list.addRect(actions,theme.color.surface);router.addBlocker(actions);
    button(left(actions,98),"Auto-key",w::PoseAutoKey,true,state.clipPoseAutoKey,UiIcon::AnimationAutoKey);
    button(left(actions,94),"Gravar",w::PoseRecord,state.clipPosePending,false,UiIcon::AnimationRecordPose);
    button(left(actions,102),"Cancelar",w::PoseCancel,state.clipPosePending,false,UiIcon::UiClose);
    button(left(actions,94),"Camada",w::PoseIsolate,true,state.clipPoseIsolated,UiIcon::AnimationReferencePose);
    const auto *poseLayer=asset->layer(state.clipLayer);
    label(actions,std::string(state.clipPosePending?"Sem gravar · ":"")+(poseLayer?poseLayer->name:"")+(state.clipPoseIsolated?" · isolada":" · resultado"),state.clipPosePending?theme.color.warning:theme.color.textDim);
    if(state.clipPoseNumbers) {
    const u32 first=std::min(state.clipPosePage*3,(state.clipPoseCount-1)/3*3),count=std::min(3u,state.clipPoseCount-first);
    {
      // A low landscape viewport cannot fit a vertical XYZ inspector. Keep
      // all three values reachable in one contextual strip, without clipping
      // fields or taking the timeline's working area.
      auto strip=deflate(UiRect{layout.viewport.x,layout.viewport.y+48,layout.viewport.width,44},{8,0,8,0});
      list.addRect(strip,theme.color.surface);router.addBlocker(strip);
      button(left(strip,34),"",w::RemoveTrack,asset->tracks.size()>1,false,UiIcon::UiRemove);
      const auto *object=state.document?state.document->find(state.clipTarget):nullptr;
      label(left(strip,std::min(160.f,strip.width*.26f)),(object?std::string(object->name):"?")+" / "+pathName(*selected),theme.color.textDim);
      if(state.clipPoseCount>3) {
        button(left(strip,38),"<",w::PosePrevious,state.clipPosePage>0);
        button(left(strip,38),">",w::PoseNext,first+3<state.clipPoseCount);
      }
      const float fieldWidth=strip.width/count;
      for(u32 i=0;i<count;++i) {
        const u32 component=first+i;
        const auto name=selected->path==resources::AnimationPath::Weights?std::to_string(component):std::string(1,"XYZ"[component]);
        button(left(strip,fieldWidth),name+" · "+number(state.clipPoseValues[component],selected->path==resources::AnimationPath::Rotation?" °":selected->path==resources::AnimationPath::Weights?" %":""),w::PoseValue+component);
      }
    }
    }
  }
  if(!state.clipDiagnostic.empty()) {
    // Context tools must not paint over errors. When XYZ occupies the low
    // viewport, lend the key area to the diagnostic instead of hiding fields.
    const float y=layout.viewport.y+(state.clipPoseShown?state.clipPoseNumbers?92.f:48.f:8.f);
    const bool room=y+32<=layout.viewport.bottom()-(state.clipPoseShown?44.f:0.f);
    const UiRect note{area.x+8,room?y:std::max(layout.viewport.bottom()+42,area.bottom()-32),std::max(0.f,area.width-16),32};
    list.addRect(note,theme.color.surface);label(note,state.clipDiagnostic,theme.color.warning);
  }
  if(state.clipAuthoringPicker&&state.document) {
    const auto *branch=state.document->find(state.clipAuthoringPicker==2?state.clipTargetNode:state.clipTargetBranch);
    if(branch) {
      layout.clipTargetRows=std::clamp(static_cast<u32>(std::max(40.f,area.height-128)/40),1u,6u);
      const auto children=state.document->childrenOf(state.clipTargetBranch);
      const u32 count=state.clipAuthoringPicker==2?6:static_cast<u32>(std::min<usize>(children.size(),layout.clipTargetRows));
      auto picker=UiRect{layout.topBar.x+8,layout.topBar.bottom()+4,std::min(380.f,area.width-16),std::min(area.height-4,84.f+40*count)};
      list.addRect(picker,theme.color.surface);router.addBlocker(picker);
      auto header=top(picker,40);button(left(header,40),"<",w::TargetUp,state.clipAuthoringPicker==2||state.clipTargetBranch!=state.clipOwner);
      auto close=UiRect{header.right()-40,header.y,40,header.height};header.width-=40;button(close,"",w::TargetClose,true,false,UiIcon::UiClose);
      label(header,branch->name,theme.color.text);
      if(state.clipAuthoringPicker==1) {
        auto use=top(picker,44);button(left(use,std::max(0.f,use.width-88)),"Animar este objeto",w::TargetUse);
        button(left(use,44),"<",w::TargetPrevious,state.clipTargetPage>0);button(use,">",w::TargetNext,(state.clipTargetPage+1)*layout.clipTargetRows<children.size());
        for(u32 i=0;i<layout.clipTargetRows&&state.clipTargetPage*layout.clipTargetRows+i<children.size();++i) {
          const auto child=children[state.clipTargetPage*layout.clipTargetRows+i];const auto *object=state.document->find(child);if(!object)continue;
          auto row=top(picker,40);button(left(row,std::max(0.f,row.width-44)),object->name,w::TargetChoice+i,true,false,UiIcon::AnimationBinding);
          button(row,">",w::TargetEnter+i,!state.document->childrenOf(child).empty());
        }
      } else {
        const char *properties[]{"Posição","Escala","Rotação · Quaternion","Rotação · Euler","Rotação · Progressivo","Morphs"};
        const auto *skin=static_cast<const scene::SkinnedMesh*>(branch->components.find(scene::SkinnedMesh::descriptor));
        for(u32 i=0;i<6;++i)button(top(picker,40),properties[i],w::PropertyChoice+i,i!=5||(skin&&!skin->blendShapeWeights.empty()));
      }
    }
  }
  if(state.clipTangentPicker&&key&&curve&&selected) {
    auto sheet=UiRect{area.right()-std::min(490.f,std::max(0.f,area.width-16))-8,layout.topBar.bottom()+4,std::min(490.f,std::max(0.f,area.width-16)),std::min(128.f,area.height)};
    list.addRect(sheet,theme.color.surface);router.addBlocker(sheet);
    auto header=top(sheet,40);auto exit=UiRect{header.right()-44,header.y,44,header.height};header.width-=44;
    button(exit,"",w::TangentClose,true,false,UiIcon::UiClose);
    label(left(header,std::max(0.f,header.width-104)),"Tangentes",theme.color.text);
    button(header,key->broken?"Separadas":"Ligadas",w::TangentLink,true,!key->broken,UiIcon::AnimationTangent);
    const usize index=static_cast<usize>(key-curve->keys.data());
    for(bool incoming:{true,false}) {
      auto line=top(sheet,44);const auto mode=incoming?key->incoming:key->outgoing;
      bool enabled=incoming?index>0:index+1<curve->keys.size();
      if(enabled&&selected->rotationMode==resources::AnimationRotationMode::ProgressiveQuaternion&&state.clipComponent==4)
        enabled=incoming?key->value!=curve->keys[index-1].value:key->value!=curve->keys[index+1].value;
      const bool curved=mode!=resources::AnimationTangentMode::Linear&&mode!=resources::AnimationTangentMode::Constant&&mode!=resources::AnimationTangentMode::NextConstant;
      label(left(line,66),incoming?"Entrada":"Saída",enabled?theme.color.textDim:theme.color.textFaint);
      button(left(line,118),tangentName(mode),incoming?w::ModeIn:w::ModeOut,enabled);
      button(left(line,104),number(static_cast<float>(resources::animationCurveSlope(*curve,index,incoming))," /s"),incoming?w::SlopeIn:w::SlopeOut,enabled&&mode==resources::AnimationTangentMode::Free);
      const bool weighted=incoming?key->weightedIn:key->weightedOut;
      button(left(line,80),"Peso",incoming?w::WeightedIn:w::WeightedOut,enabled&&curved,weighted);
      button(line,number(incoming?key->inWeight:key->outWeight),incoming?w::WeightIn:w::WeightOut,enabled&&curved&&weighted);
    }
    if(state.clipModeSide) {
      auto menu=UiRect{sheet.x+66,layout.topBar.bottom()+48,150,std::min(252.f,area.height-44)};
      list.addRect(menu,theme.color.surface);router.addBlocker(menu);
      for(u32 mode=0;mode<7;++mode)button(top(menu,36),tangentName(static_cast<resources::AnimationTangentMode>(mode)),w::SelectMode+mode);
    }
  }
  if(cue&&state.clipCueOptions) {
    auto sheet=UiRect{area.right()-std::min(360.f,area.width-16)-8,layout.topBar.bottom()+4,std::min(360.f,area.width-16),std::min(172.f,area.height-4)};
    list.addRect(sheet,theme.color.surface);router.addBlocker(sheet);
    auto title=top(sheet,42);auto close=UiRect{title.right()-42,title.y,42,title.height};title.width-=42;
    label(title,cue->name,theme.color.text);button(close,"",w::CueOptionsClose,true,false,UiIcon::UiClose);
    auto directions=top(sheet,42);button(left(directions,directions.width*.5f),"Avançar",w::CueForward,cue->kind==resources::AnimationCueKind::Event,cue->forward);
    button(directions,"Reverter",w::CueReverse,cue->kind==resources::AnimationCueKind::Event,cue->reverse);
    button(top(sheet,42),"Ativo",w::CueEnabled,true,cue->enabled);
    auto actions=top(sheet,42);button(left(actions,actions.width*.5f),"Duplicar",w::CueDuplicate,true,false,UiIcon::AnimationDuplicate);
    button(actions,"Excluir",w::CueDelete,true,false,UiIcon::UiRemove);
  }
  if(state.clipEditPicker) {
    auto picker=UiRect{area.x+8,layout.topBar.bottom()+4,std::min(320.f,area.width-16),std::min(248.f,area.height-4)};
    list.addRect(picker,theme.color.surface);router.addBlocker(picker);
    auto title=top(picker,40);auto close=UiRect{title.right()-40,title.y,40,title.height};title.width-=40;
    label(title,"Edição do clipe",theme.color.text);button(close,"",w::EditsClose,true,false,UiIcon::UiClose);
    const bool hasKeys=!cues&&(!state.clipSelection.empty()||key);
    const bool canPaste=!cues&&state.clipClipboard&&!state.clipClipboard->empty();
    auto copy=top(picker,40);button(left(copy,copy.width*.5f),"Copiar",w::CopyKeys,hasKeys,false,UiIcon::AssetsCopy);
    button(copy,"Recortar",w::CutKeys,hasKeys);
    button(top(picker,40),"Colar · substituir",w::PasteReplace,canPaste);
    auto insert=top(picker,40);button(left(insert,insert.width*.5f),"Inserir copiados",w::PasteInsertTracks,canPaste);
    button(insert,"Inserir todos",w::PasteInsertAll,canPaste);
    auto frames=top(picker,44);button(left(frames,frames.width*.5f),"Duração · "+number(asset->duration," s"),w::Duration);
    button(left(frames,frames.width*.5f),"− quadro",w::PreviousFrame);button(frames,"+ quadro",w::NextFrame);
    button(top(picker,44),"Bake e redução",w::BakeOpen,selected!=nullptr,false,UiIcon::AnimationBake);
  } else if(state.clipNewPicker) {
    auto picker=UiRect{layout.topBar.x+8,layout.topBar.bottom()+4,std::min(340.f,std::max(0.f,area.width-16)),std::min(area.height,120.f)};
    list.addRect(picker,theme.color.surface);router.addBlocker(picker);
    button(top(picker,40),"Novo · Quaternion",w::NewQuaternion);
    button(top(picker,40),"Novo · Euler",w::NewEuler);
    button(top(picker,40),"Novo · Progressivo",w::NewProgressive);
  } else if(state.clipPicker) {
    const usize entries=state.clipImportedPicker?state.clipSourceEntries.size():state.clipAssets?state.clipAssets->size():0;
    layout.clipPickerRows=std::clamp(static_cast<u32>(std::max(40.f,area.height-122)/40),1u,8u);
    auto picker=UiRect{layout.topBar.x+8,layout.topBar.bottom()+4,std::min(380.f,area.width-16),std::min(area.height-4,116.f+std::max<usize>(1,std::min<usize>(layout.clipPickerRows,entries))*40)};
    list.addRect(picker,theme.color.surface);router.addBlocker(picker);
    auto catalogs=top(picker,40);button(left(catalogs,catalogs.width*.5f),"Editáveis",w::OwnedCatalog,true,!state.clipImportedPicker);
    button(catalogs,"Extrair da fonte",w::ImportedCatalog,!state.clipSourceEntries.empty(),state.clipImportedPicker,UiIcon::AssetsImport);
    button(top(picker,40),"+ Novo clipe",w::New);
    auto pages=top(picker,34);button(left(pages,44),"<",w::PickerPrevious,state.clipPage>0);
    button(left(pages,44),">",w::PickerNext,(state.clipPage+1)*layout.clipPickerRows<entries);
    label(pages,"Página "+std::to_string(state.clipPage+1),theme.color.textMuted);
    if(state.clipImportedPicker)for(u32 i=0;i<layout.clipPickerRows&&state.clipPage*layout.clipPickerRows+i<entries;++i) {
      const auto &entry=state.clipSourceEntries[state.clipPage*layout.clipPickerRows+i];auto row=top(picker,40);
      button(left(row,std::max(0.f,row.width-74)),entry.name,w::Choice+i,true,false,UiIcon::AssetsImport);label(row,number(entry.duration," s"),theme.color.textMuted);
    } else if(state.clipAssets)for(u32 i=0;i<layout.clipPickerRows&&state.clipPage*layout.clipPickerRows+i<state.clipAssets->size();++i) {
      const auto &entry=(*state.clipAssets)[state.clipPage*layout.clipPickerRows+i];button(top(picker,40),entry.name,w::Choice+i,true,entry.guid==state.clipGuid);
    }
    if(!entries)label(top(picker,40),"Sem clipes nesta origem",theme.color.textMuted);
  }
  return layout;
}
} // namespace ae::editor


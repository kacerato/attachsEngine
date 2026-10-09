#include "editor/editor_screen.h"
#include "resources/animation_binding_path.h"
#include "scene/skinned_mesh.h"
#include <cmath>
#include <cstdio>
#include <unordered_set>

namespace ae::editor {
using namespace ui;
namespace {
namespace w=clip_widget;
UiRect centred(const UiRect &r,float w,float h) {return {r.x+(r.width-w)*.5f,r.y+(r.height-h)*.5f,w,h};}
std::string number(float value,const char *suffix="") {char s[64];std::snprintf(s,sizeof s,"%.4g%s",static_cast<double>(value),suffix);return s;}
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
  const auto label=[&](const UiRect &r,std::string_view text,UiColor color,UiAlign align=UiAlign::Start) {
    list.pushClip(r);list.addText(deflate(r,{6,0,6,0}),text,color,theme.type.caption,align);list.popClip();
  };
  const auto button=[&](UiRect r,std::string_view text,u32 code,bool enabled=true,bool active=false,UiIcon icon=UiIcon::None) {
    r=deflate(r,{2,3,2,3});const auto hit=r;const auto foreground=enabled?theme.color.text:theme.color.textFaint;
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
  if(header.width>=42)button(left(header,42),"Nome",w::Name);
  if(header.width>=64)button(left(header,64),"Pose",w::Pose,state.clipTarget!=0&&!state.clipExpanded,state.clipPoseShown,UiIcon::AnimationReferencePose);
  if(header.width>=52)button(left(header,52),"Novo",w::New);
  const resources::AnimationClipTrack *selected=asset->track(state.clipTrack);
  const resources::AnimationCurve *curve=selected&&state.clipComponent<selected->curves.size()?&selected->curves[state.clipComponent]:nullptr;
  const resources::AnimationCurveKey *key=nullptr;if(curve)for(const auto &k:curve->keys)if(k.id==state.clipKey)key=&k;
  // Keep the graph's useful height when key properties appear. Idle editing
  // preserves the viewport; selecting a key lends space to the authoring task.
  const bool multi=state.clipSelectionMode&&!state.clipSelection.empty()&&!state.clipSelecting;
  const bool bake=state.clipBakeShown&&selected;
  const bool layers=state.clipLayersShown;
  const float editingHeight=state.clipExpanded?area.height:std::min(std::max(0.f,area.height-std::min(110.f,area.height*.25f)),std::max(172.f,area.height*.46f)+(key||multi||bake||layers?42.f:0.f));
  layout.viewport={area.x,area.y,area.width,std::max(0.f,area.height-editingHeight)};
  UiRect editor{area.x,layout.viewport.bottom(),area.width,editingHeight};
  list.addRect(editor,theme.color.canvas);router.addBlocker(editor);
  if(multi&&!bake&&!layers) {
    auto property=top(editor,42);list.addRect(property,theme.color.surface);
    label(left(property,std::min(150.f,property.width*.25f)),std::to_string(state.clipSelection.size())+" chaves",theme.color.textDim);
    button(left(property,110),"Mover · s",w::SelectionOffset);
    button(left(property,110),"Escala",w::SelectionScale);
    button(left(property,84),"Somar",w::SelectionAdd,true,state.clipSelectionAdd);
    button(left(property,74),"Limpar",w::SelectionClear);
  } else if(key&&!bake&&!layers) {
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
  button(left(transport,100),"Camadas",w::Layers,true,false,UiIcon::AnimationLayers);
  button(left(transport,74),"Chaves",w::Keys,true,!state.clipCurves);
  button(left(transport,74),"Curvas",w::Curves,true,state.clipCurves,UiIcon::AnimationCurve);
  button(left(transport,92),"Selecionar",w::Selection,true,state.clipSelectionMode,UiIcon::EditorAuthorSelect);
  button(left(transport,68),"Edição",w::Edits,true,state.clipEditPicker);
  if(state.surface.width>=768)button(left(transport,40),"",w::PreviousFrame,true,false,UiIcon::UiChevronLeft);
  button(left(transport,44),"",w::Play,state.clipDiagnostic.empty(),state.clipPlaying,state.clipPlaying?UiIcon::RuntimePause:UiIcon::RuntimePlay);
  if(state.surface.width>=768)button(left(transport,40),"",w::NextFrame,true,false,UiIcon::UiChevronRight);
  button(left(transport,56),"Loop",w::Loop,true,state.clipLoop);
  button(left(transport,90),number(state.clipTime," s"),w::Time);
  if(transport.width>=90)button(left(transport,90),number(asset->duration," s"),w::Duration);
  if(transport.width>=88) {button(left(transport,40),"-",w::ZoomOut);button(left(transport,40),"+",w::ZoomIn);}
  }
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
  if(!state.clipDiagnostic.empty()) {
    const UiRect note{layout.viewport.x+8,layout.viewport.y+8,std::max(0.f,layout.viewport.width-16),32};
    list.addRect(note,theme.color.surface);label(note,state.clipDiagnostic,theme.color.warning);
  }
  if(state.clipPoseShown&&selected&&state.clipPoseCount&&!layout.viewport.isEmpty()) {
    const u32 first=std::min(state.clipPosePage*3,(state.clipPoseCount-1)/3*3),count=std::min(3u,state.clipPoseCount-first);
    if(layout.viewport.height<180) {
      // A low landscape viewport cannot fit a vertical XYZ inspector. Keep
      // all three values reachable in one contextual strip, without clipping
      // fields or taking the timeline's working area.
      auto strip=deflate(UiRect{layout.viewport.x,layout.viewport.y+4,layout.viewport.width,44},{8,0,8,0});
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
    } else {
    auto panel=UiRect{layout.viewport.right()-286,layout.viewport.y+8,std::min(278.f,layout.viewport.width-16),std::min(40.f+count*44+(state.clipPoseCount>3?34.f:0),layout.viewport.height-16)};
    panel.x=std::max(panel.x,layout.viewport.x+8);list.addRect(panel,theme.color.surface);router.addBlocker(panel);
    auto title=top(panel,40);button(left(title,34),"",w::RemoveTrack,asset->tracks.size()>1,false,UiIcon::UiRemove);
    const auto *object=state.document?state.document->find(state.clipTarget):nullptr;
    label(title,(object?std::string(object->name):"?")+" / "+pathName(*selected),theme.color.textDim);
    for(u32 i=0;i<count;++i) {
      auto row=top(panel,44);const u32 component=first+i;
      label(left(row,54),selected->path==resources::AnimationPath::Weights?std::to_string(component):std::string(1,"XYZ"[component]),theme.color.textMuted);
      button(row,number(state.clipPoseValues[component],selected->path==resources::AnimationPath::Rotation?" °":selected->path==resources::AnimationPath::Weights?" %":""),w::PoseValue+component);
    }
    if(state.clipPoseCount>3) {auto pages=top(panel,34);button(left(pages,44),"<",w::PosePrevious,state.clipPosePage>0);button(left(pages,44),">",w::PoseNext,first+3<state.clipPoseCount);}
    }
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
  if(state.clipEditPicker) {
    auto picker=UiRect{area.x+8,layout.topBar.bottom()+4,std::min(320.f,area.width-16),std::min(248.f,area.height-4)};
    list.addRect(picker,theme.color.surface);router.addBlocker(picker);
    auto title=top(picker,40);auto close=UiRect{title.right()-40,title.y,40,title.height};title.width-=40;
    label(title,"Edição do clipe",theme.color.text);button(close,"",w::EditsClose,true,false,UiIcon::UiClose);
    const bool hasKeys=!state.clipSelection.empty()||key;
    const bool canPaste=state.clipClipboard&&!state.clipClipboard->empty();
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


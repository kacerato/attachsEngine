#include "editor/editor_session.h"
#include "runtime/animation_property_binding.h"
#include "scene/ui_canvas.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace ae::editor {
namespace {
using namespace resources;
namespace w=clip_widget;
bool fail(std::string &error,const char *text){error=text;return false;}
void unpack(const AnimationPropertyValue &value,float (&out)[4]) {
  if(const auto *v=std::get_if<float>(&value))out[0]=*v;
  else if(const auto *v=std::get_if<AnimationPropertyVector2>(&value)){out[0]=v->x;out[1]=v->y;}
  else if(const auto *v=std::get_if<AnimationPropertyVector3>(&value)){out[0]=v->x;out[1]=v->y;out[2]=v->z;}
  else if(const auto *v=std::get_if<AnimationPropertyColor>(&value)){out[0]=v->r;out[1]=v->g;out[2]=v->b;}
}
AnimationPropertyValue pack(AnimationPropertyKind kind,const float (&v)[4]) {
  if(kind==AnimationPropertyKind::Vector2)return AnimationPropertyVector2{v[0],v[1]};
  if(kind==AnimationPropertyKind::Vector3)return AnimationPropertyVector3{v[0],v[1],v[2]};
  if(kind==AnimationPropertyKind::LinearColor)return AnimationPropertyColor{v[0],v[1],v[2]};
  return v[0];
}
}
std::vector<EditorScreenState::ClipPropertyEntry> EditorSession::animationPropertyCatalog(EditorEntityId target,std::string &error) const {
  error.clear();std::vector<EditorScreenState::ClipPropertyEntry> result;
  const auto *object=document_.find(target);if(!object){error="Objeto de propriedade removido";return result;}
  for(usize i=0;i<object->components.size();++i) {
    const auto &component=*object->components.at(i);
    const auto *schema=scene::findComponentSchema(component.type().id);
    for(auto descriptor:runtime::animationPropertyCatalog(component))
      result.push_back({std::move(descriptor.address),std::string(schema?schema->name:component.type().id)+" · "+descriptor.name,
        descriptor.unit,descriptor.help,descriptor.minimum,descriptor.maximum});
    if(&component.type()!=&scene::UiCanvas::descriptor)continue;
    const auto &canvas=static_cast<const scene::UiCanvas&>(component);
    ui::GuiDocument document;std::string diagnostic;
    if(!loadGuiDocument(canvas.document,document,diagnostic))continue;
    for(const auto &node:document.nodes()) {
      AnimationPropertyAddress address{AnimationPropertyDomain::GuiNode,AnimationPropertyKind::Number,
        component.instanceId(),component.type().version,std::string(component.type().id),"opacity",0,node.id,canvas.document};
      result.push_back({std::move(address),"UI · "+node.name+" · Opacidade","","Opacidade local do nó UI",0,1});
    }
  }
  return result;
}
bool EditorSession::addAnimationPropertyTrack(AssetGuid guid,u32 revision,EditorEntityId owner,EditorEntityId target,
    const AnimationPropertyAddress &address,u64 &created,std::string &error) {
  u64 id=0;const bool ok=editAnimationClipAsset(guid,revision,[&](auto &candidate){
    return prepareAnimationProperty(candidate,owner,target,address,id,error);
  },error);
  if(ok)created=id;
  return ok;
}
bool EditorSession::recordAnimationProperty(AssetGuid guid,u32 revision,EditorEntityId owner,u64 track,float time,
    const AnimationPropertyValue &value,std::string &error) {
  return editAnimationClipAsset(guid,revision,[&](auto &candidate){
    return candidate.putPropertyValue(track,time,value,error)&&validateAnimationPropertyClip(candidate,owner,time,error);
  },error);
}

void EditorSession::refreshAnimationPropertySurface() {
  if(!state_.clipPropertiesShown)return;
  const auto *asset=animationClipAsset(state_.clipGuid);if(!asset)return;
  if(clipPropertySceneRevision_==document_.revision()&&clipPropertyResourceRevision_==asset->revision&&
     clipPropertySampleTime_==state_.clipTime&&clipPropertySampleTrack_==state_.clipPropertyTrack&&
     clipPropertyCatalogTarget_==state_.clipTargetNode)return;
  clipPropertySceneRevision_=document_.revision();clipPropertyResourceRevision_=asset->revision;
  clipPropertySampleTime_=state_.clipTime;clipPropertySampleTrack_=state_.clipPropertyTrack;clipPropertyCatalogTarget_=state_.clipTargetNode;
  state_.clipPropertyError.clear();
  state_.clipPropertyEntries=animationPropertyCatalog(state_.clipTargetNode,state_.clipPropertyError);
  const auto *remap=asset->propertyTrack(state_.clipPropertyTrack);
  std::erase_if(state_.clipPropertyEntries,[&](const auto &entry){return !animationPropertySearchMatches(entry,state_.clipPropertyQuery)||
    (state_.clipPropertyRemap&&remap&&entry.address.kind!=remap->data.kind);});
  state_.clipPropertyKeys.clear();state_.clipPropertyOptions.clear();state_.clipPropertyDimensions=0;
  const auto *track=asset->propertyTrack(state_.clipPropertyTrack);
  if(!track){state_.clipPropertyTrack=0;state_.clipPropertyKey=0;return;}
  state_.clipPropertyLabel=track->address.property;state_.clipPropertyUnit.clear();state_.clipPropertyInfo.clear();
  std::string catalogError;
  for(const auto &entry:animationPropertyCatalog(state_.clipTargetNode,catalogError))if(entry.address==track->address) {
    state_.clipPropertyLabel=entry.label;state_.clipPropertyUnit=entry.unit;
    if(entry.minimum!=entry.maximum) {
      std::ostringstream range;range.precision(4);range<<entry.minimum<<"…"<<entry.maximum;
      state_.clipPropertyInfo=range.str()+" "+entry.unit;
    }
    if(!entry.help.empty())state_.clipPropertyInfo+=" · "+entry.help;
  }
  CompiledAnimationProperty compiled;AnimationPropertyValue value;
  if(!compileAnimationProperty(track->data,asset->duration,compiled,state_.clipPropertyError)||!compiled.sample(state_.clipTime,value))return;
  state_.clipPropertyDimensions=animationPropertyComponents(track->data.kind);unpack(value,state_.clipPropertyValues);
  if(const auto *v=std::get_if<bool>(&value))state_.clipPropertyBoolean=*v;
  if(const auto *v=std::get_if<u32>(&value))state_.clipPropertyEnum=*v;
  if(!track->data.curves.empty())for(const auto &key:track->data.curves[std::min<usize>(state_.clipPropertyComponent,track->data.curves.size()-1)].keys)state_.clipPropertyKeys.push_back({key.id,key.time});
  else for(const auto &key:track->data.keys)state_.clipPropertyKeys.push_back({key.id,key.time});
  if(std::none_of(state_.clipPropertyKeys.begin(),state_.clipPropertyKeys.end(),[&](const auto &key){return key.id==state_.clipPropertyKey;}))state_.clipPropertyKey=0;
  // Enumerations retain their schema values and labels; never treat them as an
  // arbitrary numeric field or lose precision through the floating point UI.
  if(track->address.kind==AnimationPropertyKind::Enumeration) {
    const auto *object=document_.find(state_.clipTargetNode);
    const auto *component=object?object->components.findInstance(track->address.component):nullptr;
    if(component)for(const auto &property:component->type().enums)if(property.id==track->address.property)
      for(const auto &option:property.options)state_.clipPropertyOptions.emplace_back(option.value,option.name);
  }
}
bool EditorSession::handleAnimationPropertyWidget(u32 code) {
  if(code<w::Properties||code>=0xb600)return false;
  const auto *asset=animationClipAsset(state_.clipGuid);if(!asset)return true;
  std::string error;
  if(handleAnimationPropertyCurveWidget(code))return true;
  if(code==w::PropertySearch) {beginAnimatorName(code,state_.clipPropertyQuery);return true;}
  if(code==w::PropertyTime) {beginAnimationClipNumber(code,state_.clipTime);return true;}
  if(code==w::PropertyRemap) {state_.clipPropertyRemap=true;state_.clipPropertyCatalog=true;state_.clipPropertyQuery.clear();state_.clipPropertyPage=0;clipPropertyResourceRevision_=0;}
  else if(code==w::Properties) {
    state_.clipPropertiesShown=true;state_.clipDriversShown=false;state_.clipPropertyCatalog=false;state_.clipPropertyRemap=false;state_.clipPropertyCurves=false;state_.clipPropertyPage=0;
    state_.clipEditPicker=state_.clipRegionsShown=state_.clipRangeShown=state_.clipLayersShown=state_.clipBakeShown=false;
    state_.clipPoseToolsShown=state_.clipPoseShown=state_.clipCurveView=state_.clipTangentPicker=false;
    state_.clipAuthoringPicker=0;state_.clipPlaying=false;
    if(!document_.exists(state_.clipTargetNode))state_.clipTargetNode=state_.clipOwner;
    clipPropertyResourceRevision_=0;
  } else if(code==w::PropertyClose)state_.clipPropertiesShown=false;
  else if(code==w::PropertyCatalog||code==w::PropertyTracks) {
    state_.clipPropertyCatalog=code==w::PropertyCatalog;state_.clipPropertyPage=0;state_.clipPropertyRemap=state_.clipPropertyCurves=false;clipPropertyResourceRevision_=0;
    if(code==w::PropertyTracks)state_.clipPropertyTrack=0;
  } else if(code==w::PropertyPrevious) {if(state_.clipPropertyPage)--state_.clipPropertyPage;}
  else if(code==w::PropertyNext) {
    const auto count=state_.clipPropertyCatalog?state_.clipPropertyEntries.size():asset->properties.size();
    if((state_.clipPropertyPage+1)*2<count)++state_.clipPropertyPage;
  } else if(code>=w::PropertyEntry&&code<w::PropertyEntry+2) {
    const usize index=state_.clipPropertyPage*2+code-w::PropertyEntry;
    if(state_.clipPropertyCatalog&&index<state_.clipPropertyEntries.size()) {
      const auto address=state_.clipPropertyEntries[index].address;u64 created=0;
      const bool ok=state_.clipPropertyRemap?editAnimationClipAsset(asset->guid,asset->revision,[&](auto &candidate){
        return remapAnimationProperty(candidate,state_.clipOwner,state_.clipTargetNode,state_.clipPropertyTrack,address,error);
      },error):addAnimationPropertyTrack(asset->guid,asset->revision,state_.clipOwner,state_.clipTargetNode,address,created,error);
      if(ok) {
        if(!state_.clipPropertyRemap)state_.clipPropertyTrack=created;
        state_.clipPropertyCatalog=state_.clipPropertyRemap=false;state_.clipPropertyQuery.clear();clipPropertyResourceRevision_=0;
      }
    } else if(!state_.clipPropertyCatalog&&index<asset->properties.size()) {
      const auto &track=asset->properties[index];state_.clipPropertyTrack=track.id;state_.clipPropertyKey=0;
      runtime::SourceAnimations source;source.source=asset->source.valid()?asset->source:asset->guid;
      for(const auto &binding:asset->bindings){source.nodes.push_back(binding.sourceNode);source.nodeNames.push_back(binding.name);source.nodePaths.push_back(binding.path);}
      std::vector<runtime::ObjectId> targets;runtime::resolveAnimationTargets(document_,state_.clipOwner,source,targets);
      for(usize i=0;i<asset->bindings.size();++i)if(asset->bindings[i].id==track.binding&&i<targets.size())state_.clipTargetNode=targets[i];
    }
  } else if(code==w::PropertyRemove) {
    if(editAnimationClipAsset(asset->guid,asset->revision,[&](auto &candidate){return candidate.removePropertyTrack(state_.clipPropertyTrack,error);},error))
      state_.clipPropertyTrack=state_.clipPropertyKey=0;
  } else if(code==w::PropertyKeyPrevious||code==w::PropertyKeyNext) {
    const auto &keys=state_.clipPropertyKeys;
    if(!keys.empty()) {
      const auto found=std::find_if(keys.begin(),keys.end(),[&](const auto &key){return key.id==state_.clipPropertyKey;});
      usize index=found==keys.end()?0:static_cast<usize>(found-keys.begin());
      if(found!=keys.end()) {if(code==w::PropertyKeyNext&&index+1<keys.size())++index;else if(code==w::PropertyKeyPrevious&&index)--index;}
      state_.clipPropertyKey=keys[index].id;seekAnimationClip(keys[index].time,error);
    }
  } else if(code==w::PropertyKeyTime) {
    for(const auto &key:state_.clipPropertyKeys)if(key.id==state_.clipPropertyKey)beginAnimationClipNumber(code,key.time);
  } else if(code==w::PropertyKeyDelete) {
    const auto found=std::find_if(state_.clipPropertyKeys.begin(),state_.clipPropertyKeys.end(),[&](const auto &key){return key.id==state_.clipPropertyKey;});
    if(found!=state_.clipPropertyKeys.end()) {
      const float time=found->time;
      editAnimationClipAsset(asset->guid,asset->revision,[&](auto &candidate){
        auto *track=candidate.propertyTrack(state_.clipPropertyTrack);if(!track)return fail(error,"Canal removido");
        if(state_.clipPropertyCurves)return candidate.eraseKey(track->id,state_.clipPropertyComponent,state_.clipPropertyKey,error);
        for(auto &curve:track->data.curves) {std::erase_if(curve.keys,[&](const auto &key){return key.time==time;});if(curve.keys.empty())return fail(error,"Preserve uma chave no canal");}
        if(track->data.curves.empty()){std::erase_if(track->data.keys,[&](const auto &key){return key.time==time;});if(track->data.keys.empty())return fail(error,"Preserve uma chave no canal");}
        return candidate.valid(&error);
      },error);
    }
  } else if(code==w::PropertyRecord||(code>=w::PropertyValue&&code<w::PropertyValue+4)) {
    const auto *track=asset->propertyTrack(state_.clipPropertyTrack);if(!track)return true;
    if(code>=w::PropertyValue&&state_.clipPropertyDimensions) {
      const auto component=code-w::PropertyValue;if(component<state_.clipPropertyDimensions)beginAnimationClipNumber(code,state_.clipPropertyValues[component]);
    } else {
      AnimationPropertyValue value;
      if(track->address.kind==AnimationPropertyKind::Boolean)value=code==w::PropertyRecord?state_.clipPropertyBoolean:!state_.clipPropertyBoolean;
      else if(track->address.kind==AnimationPropertyKind::Enumeration) {
        auto option=std::find_if(state_.clipPropertyOptions.begin(),state_.clipPropertyOptions.end(),[&](const auto &v){return v.first==state_.clipPropertyEnum;});
        if(code!=w::PropertyRecord&&option!=state_.clipPropertyOptions.end()) {if(++option==state_.clipPropertyOptions.end())option=state_.clipPropertyOptions.begin();}
        if(option==state_.clipPropertyOptions.end())return true;
        value=option->first;
      } else value=pack(track->address.kind,state_.clipPropertyValues);
      recordAnimationProperty(asset->guid,asset->revision,state_.clipOwner,track->id,state_.clipTime,value,error);
    }
  }
  state_.clipDiagnostic=error;refreshAnimationClip();refreshAnimationPropertySurface();appearanceChanged_=true;return true;
}
bool EditorSession::applyAnimationPropertyNumber(u32 code,double input) {
  const auto *asset=animationClipAsset(clipNumberGuid_);std::string error;
  if(!asset||asset->revision!=clipNumberRevision_||state_.clipPropertyTrack!=clipNumberTrack_||
     state_.clipTime!=clipNumberTime_||document_.revision()!=clipNumberSceneRevision_) {
    state_.clipDiagnostic="Canal, tempo ou cena mudou; reabra o campo";return false;
  }
  const auto *track=asset->propertyTrack(clipNumberTrack_);if(!track)return false;
  bool ok=false;
  if(code==w::PropertyKeyTime) {
    const auto found=std::find_if(state_.clipPropertyKeys.begin(),state_.clipPropertyKeys.end(),[&](const auto &key){return key.id==clipNumberKey_;});
    if(found==state_.clipPropertyKeys.end()||input<0||input>asset->duration) {state_.clipDiagnostic="Chave removida ou tempo fora do clipe";return false;}
    const float before=found->time,after=static_cast<float>(input);
    ok=editAnimationClipAsset(asset->guid,asset->revision,[&](auto &candidate){
      auto *t=candidate.propertyTrack(clipNumberTrack_);
      for(auto &curve:t->data.curves) {
        if(std::any_of(curve.keys.begin(),curve.keys.end(),[&](const auto &key){return key.time==after&&key.time!=before;}))return fail(error,"Tempo ocupado por outra chave");
        for(auto &key:curve.keys)if(key.time==before)key.time=after;
        std::sort(curve.keys.begin(),curve.keys.end(),[](const auto &a,const auto &b){return a.time<b.time;});
      }
      if(t->data.curves.empty()) {
        if(std::any_of(t->data.keys.begin(),t->data.keys.end(),[&](const auto &key){return key.time==after&&key.time!=before;}))return fail(error,"Tempo ocupado por outra chave");
        for(auto &key:t->data.keys)if(key.time==before)key.time=after;
        std::sort(t->data.keys.begin(),t->data.keys.end(),[](const auto &a,const auto &b){return a.time<b.time;});
      }
      return candidate.valid(&error);
    },error);
    if(ok)seekAnimationClip(after,error);
  } else {
    CompiledAnimationProperty compiled;AnimationPropertyValue value;float numbers[4]{};
    if(!compileAnimationProperty(track->data,asset->duration,compiled,error)||!compiled.sample(clipNumberTime_,value))return false;
    unpack(value,numbers);const u32 component=code-w::PropertyValue;
    if(component>=animationPropertyComponents(track->address.kind))return false;
    numbers[component]=static_cast<float>(input);value=pack(track->address.kind,numbers);
    // Validate with the actual consumer before writing project bytes. Preview
    // failure cannot leave a persistent key which the runtime cannot apply.
    const auto *object=document_.find(state_.clipTargetNode);const auto *original=object?object->components.findInstance(track->address.component):nullptr;
    if(track->address.domain==AnimationPropertyDomain::GuiNode) {
      if(input<0||input>1){state_.clipDiagnostic="Opacidade exige 0 a 1";return false;}
    } else {
      if(!original){state_.clipDiagnostic="Componente removido";return false;}
      auto components=object->components;auto *candidate=components.editInstance(original->instanceId());
      if(!candidate||!runtime::writeAnimationProperty(*candidate,track->address,value,error)||!candidate->valid()){state_.clipDiagnostic=error;return false;}
    }
    ok=recordAnimationProperty(asset->guid,asset->revision,state_.clipOwner,track->id,clipNumberTime_,value,error);
  }
  state_.clipDiagnostic=error;if(ok){refreshAnimationClip();refreshAnimationPropertySurface();}return ok;
}
}

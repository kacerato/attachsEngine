#include <cstdio>
#include "editor/editor_session.h"
#include "scene/path.h"
namespace ae::editor {
namespace {
scene::Path*editablePath(EditorEntity&value,u64 instance){
 const auto *current=value.components.findInstance(instance);
 if(!current||current->type().id!=scene::Path::descriptor.id)return nullptr;
 return static_cast<scene::Path *>(value.components.edit(scene::Path::descriptor));
}
}
bool EditorSession::insertPathPoint(runtime::ObjectId object,u64 instance,u64 beforeId,const resources::CurvePoint3D&point,u64&outId){
 outId=0;if(isPlaying()||playMirrorOpen_||history_.isOpen()){return false;}const auto*entity=document_.find(object);if(!entity){return false;}auto candidate=*entity;auto*path=editablePath(candidate,instance);u64 allocated=0;
 if(!path||!path->insertPoint(beforeId,point,allocated)||!history_.applyValues(document_,object,candidate)){return false;}outId=allocated;return true;
}
bool EditorSession::removePathPoint(runtime::ObjectId object,u64 instance,u64 pointId){
 if(isPlaying()||playMirrorOpen_||history_.isOpen()){return false;}const auto*entity=document_.find(object);if(!entity){return false;}auto candidate=*entity;auto*path=editablePath(candidate,instance);return path&&path->removePoint(pointId)&&history_.applyValues(document_,object,candidate);
}
bool EditorSession::movePathPoint(runtime::ObjectId object,u64 instance,u64 pointId,u64 beforeId){
 if(isPlaying()||playMirrorOpen_||history_.isOpen()){return false;}const auto*entity=document_.find(object);if(!entity){return false;}auto candidate=*entity;auto*path=editablePath(candidate,instance);return path&&path->movePoint(pointId,beforeId)&&history_.applyValues(document_,object,candidate);
}
bool EditorSession::editPathPoint(runtime::ObjectId object,u64 instance,u64 pointId,const resources::CurvePoint3D&point){
 if(isPlaying()||playMirrorOpen_||history_.isOpen()){return false;}const auto*entity=document_.find(object);if(!entity){return false;}auto candidate=*entity;auto*path=editablePath(candidate,instance);return path&&path->editPoint(pointId,point)&&history_.applyValues(document_,object,candidate);
}
void EditorSession::refreshPathEditorState(){
 const auto target=state_.inspectorTarget?state_.inspectorTarget:state_.selection;
 const auto&graph=(isPlaying()||playMirrorOpen_)&&playScene_.active()?playScene_.document():document_;
 const auto*object=graph.find(target);
 const scene::Path*path=nullptr;
 if(object)for(usize i=0;i<object->components.size();++i){const auto*c=object->components.at(i);if(c->type().id==scene::Path::descriptor.id){path=static_cast<const scene::Path*>(c);break;}}
 if(pathEditorEntity_!=target||(path&&state_.pathInstance!=path->instanceId())){state_.pathEditorOpen=false;state_.pathPointList=false;state_.pathPointPage=0;state_.pathPointId=0;pathEditorEntity_=target;}
 state_.pathStatus.clear();state_.pathStatusWarning=false;state_.followStatus.clear();state_.followStatusWarning=false;
 if(!path){state_.pathEntity=0;state_.pathInstance=0;state_.pathPointId=0;state_.pathEditorOpen=false;state_.pathPointList=false;}
 else{
  state_.pathEntity=target;
  state_.pathInstance=path->instanceId();if(!path->point(state_.pathPointId))state_.pathPointId=path->curve.points.empty()?0:path->curve.points.front().id;
  if(pathStatsTarget_!=target||pathStatsInstance_!=path->instanceId()||pathStatsPlay_!=(isPlaying()||playMirrorOpen_)||pathStatsCurve_.closed!=path->curve.closed||pathStatsCurve_.up!=path->curve.up||pathStatsCurve_.points!=path->curve.points){
   resources::BakedCurve3D baked;pathStatsValid_=baked.bake(path->curve);pathStatsLength_=pathStatsValid_?baked.length():0;
   pathStatsCurve_=path->curve;pathStatsTarget_=target;pathStatsInstance_=path->instanceId();pathStatsPlay_=isPlaying()||playMirrorOpen_;
  }
  if(pathStatsValid_){state_.pathStatus=std::to_string(path->curve.points.size())+" / 128 pontos · "+std::to_string(pathStatsLength_)+" u locais";state_.pathStatusWarning=pathStatsLength_<=1e-12;}
  else{state_.pathStatus="Curva inválida ou orçamento de bake excedido";state_.pathStatusWarning=true;}
  if(isPlaying()||playMirrorOpen_)state_.pathStatus+=" · edição da curva somente fora de Play";
 }
 if(object)for(usize i=0;i<object->components.size();++i)if(object->components.at(i)->type().id=="astra.path.follow"){
  state_.followStatus="Percurso será avaliado no mundo Play";
  if((isPlaying()||playMirrorOpen_)&&playScene_.active()){
   double progress=0;bool playing=false;
   if(playScene_.paths().progress(playScene_.world(),target,progress)&&playScene_.paths().playing(playScene_.world(),target,playing))state_.followStatus=std::string(playing?(state_.playPaused?"Play pausado":"Em execução"):"Percurso parado")+" · "+std::to_string(progress)+" u mundiais";else state_.followStatus="Aguardando avaliação do percurso";
   for(const auto&d:playScene_.paths().diagnostics())if(d.object==target){state_.followStatus=runtime::ScenePaths::issueText(d.issue);state_.followStatusWarning=true;break;}
  }
 }
}
bool EditorSession::handlePathEditorInput(const ui::UiPointerEvent&,const ui::UiPointerRouting&routing){
 if(!routing.tapped)return false;
 const u32 key=routing.widgetId;
 const auto target=state_.inspectorTarget?state_.inspectorTarget:state_.selection;
 if(key==widgetId(EditorWidget::PathFollowRestart)||key==widgetId(EditorWidget::PathFollowStop)){
  if((!isPlaying()&&!playMirrorOpen_)||!playScene_.active()){state_.status="Controle do percurso requer Play";return true;}
  const bool restart=key==widgetId(EditorWidget::PathFollowRestart);
  const bool result=restart?playScene_.paths().restart(playScene_.world(),target):playScene_.paths().stop(playScene_.world(),target);
  state_.status=result?(restart?"Percurso reiniciado; próximo quadro/Step aplica":"Percurso parado; pose conservada"):"Percurso indisponível";refreshPathEditorState();return true;
 }
 const bool action=key>=widgetId(EditorWidget::PathEditorOpen)&&key<=widgetId(EditorWidget::PathPointListNext);
 const bool pointSelect=key>=widgetId(EditorWidget::PathPointSelectBase)&&key<widgetId(EditorWidget::PathPointSelectBase)+128u;
 const bool slotNumber=key>=widgetId(EditorWidget::ComponentSlotNumberBase)&&key<widgetId(EditorWidget::ComponentSlotNumberBase)+0x01000000u;
 if(!action&&!pointSelect&&!slotNumber)return false;
 const auto*object=document_.find(target);if(!object)return action||pointSelect;
 const scene::Path*path=nullptr;for(usize i=0;i<object->components.size();++i){const auto*c=object->components.at(i);if(c->type().id==scene::Path::descriptor.id){path=static_cast<const scene::Path*>(c);break;}}
 if(slotNumber){const u32 type=(key-widgetId(EditorWidget::ComponentSlotNumberBase))&0xffu;if(type>=object->components.size()||object->components.at(type)->type().id!=scene::Path::descriptor.id){numericPathPointId_=0;return false;}path=static_cast<const scene::Path*>(object->components.at(type));}
 if(!path)return action||pointSelect;
 if(isPlaying()||playMirrorOpen_){state_.status="Saia de Play para editar pontos do caminho";return true;}
 if(pathEditorEntity_!=target||state_.pathInstance!=path->instanceId()){pathEditorEntity_=target;state_.pathInstance=path->instanceId();state_.pathPointId=path->curve.points.empty()?0:path->curve.points.front().id;}
 if(!path->point(state_.pathPointId))state_.pathPointId=path->curve.points.empty()?0:path->curve.points.front().id;
 auto at=std::find_if(path->curve.points.begin(),path->curve.points.end(),[&](const auto&p){return p.id==state_.pathPointId;});
 const usize index=at==path->curve.points.end()?0:static_cast<usize>(at-path->curve.points.begin());
 if(slotNumber){
  const u32 field=((key-widgetId(EditorWidget::ComponentSlotNumberBase))>>8)&0xffu;
  if(at==path->curve.points.end()||field>=path->type().slotNumbers.size()||history_.isOpen())return true;
  const auto&property=path->type().slotNumbers[field];numericPathPointId_=at->id;numericPathVersion_=sceneVersion();state_.numericField=key;state_.numericEntity=target;state_.numericInstance=path->instanceId();state_.numericProperty=property.id;
  std::snprintf(state_.numericText,sizeof(state_.numericText),"%.9g",static_cast<double>(property.read(*path,static_cast<u32>(index))));state_.numericReplace=true;state_.numericError=false;return true;
 }
 if(key==widgetId(EditorWidget::PathEditorOpen)){state_.pathEditorOpen=true;state_.pathPointList=false;}
 else if(key==widgetId(EditorWidget::PathEditorClose)){state_.pathEditorOpen=false;state_.pathPointList=false;}
 else if(key==widgetId(EditorWidget::PathPointList))state_.pathPointList=!state_.pathPointList;
 else if(key==widgetId(EditorWidget::PathPointTangents)){state_.pathTangents=true;state_.pathOrientation=false;}
 else if(key==widgetId(EditorWidget::PathPointPosition)){state_.pathTangents=false;state_.pathOrientation=false;}
 else if(key==widgetId(EditorWidget::PathPointOrientation)){state_.pathTangents=false;state_.pathOrientation=true;}
 else if(key==widgetId(EditorWidget::PathPointListPrevious)){if(state_.pathPointPage)--state_.pathPointPage;}
 else if(key==widgetId(EditorWidget::PathPointListNext))++state_.pathPointPage;
 else if(pointSelect){const u32 selected=key-widgetId(EditorWidget::PathPointSelectBase);if(selected<path->curve.points.size()){state_.pathPointId=path->curve.points[selected].id;state_.pathPointList=false;}}
 else if(key==widgetId(EditorWidget::PathPointPrevious)){if(index>0)state_.pathPointId=path->curve.points[index-1].id;}
 else if(key==widgetId(EditorWidget::PathPointNext)){if(index+1<path->curve.points.size())state_.pathPointId=path->curve.points[index+1].id;}
 else if(key==widgetId(EditorWidget::PathPointInsert)){
  resources::CurvePoint3D point;if(at!=path->curve.points.end()){point=*at;point.position[0]+=1;}
  const u64 before=index+1<path->curve.points.size()?path->curve.points[index+1].id:0;u64 allocated=0;
  if(insertPathPoint(target,path->instanceId(),before,point,allocated))state_.pathPointId=allocated;else state_.status="Não foi possível inserir ponto (limite 128 ou gesto aberto)";
 }
 else if(key==widgetId(EditorWidget::PathPointRemove)){const u64 fallback=index+1<path->curve.points.size()?path->curve.points[index+1].id:index?path->curve.points[index-1].id:0;if(removePathPoint(target,path->instanceId(),state_.pathPointId))state_.pathPointId=fallback;}
 else if(key==widgetId(EditorWidget::PathPointMoveUp)){if(index>0)movePathPoint(target,path->instanceId(),state_.pathPointId,path->curve.points[index-1].id);}
 else if(key==widgetId(EditorWidget::PathPointMoveDown)){if(index+1<path->curve.points.size())movePathPoint(target,path->instanceId(),state_.pathPointId,index+2<path->curve.points.size()?path->curve.points[index+2].id:0);}
 refreshPathEditorState();return true;
}
}

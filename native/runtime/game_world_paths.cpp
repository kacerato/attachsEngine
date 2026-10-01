#include "runtime/game_world.h"
#include "scene/path.h"

namespace ae::runtime {
WorldStatus GameWorld::editPathPoint(const ComponentHandle &component,u32 operation,u64 elementId,
                                   u32 index,const float *values,u64 &allocatedId) {
  allocatedId=0;
  const auto status=validate(component.object);
  if(status!=WorldStatus::Ok) return status;
  const auto *source=readComponent(component);
  if(!source) return WorldStatus::ComponentMissing;
  if(source->type().id!=scene::Path::descriptor.id||!((operation>=3&&operation<=6)||operation==9||operation==10)) return WorldStatus::InvalidArgument;
  const auto *schema=scene::findComponentSchema(source->type().id);
  if(!schema||schema->structuralInPlay==scene::PlayMutability::Never) return WorldStatus::NotMutableInPlay;
  auto candidate=source->clone();
  auto &path=static_cast<scene::Path &>(*candidate);
  resources::CurvePoint3D point{};
  if(operation==3||operation==4||operation==9||operation==10) {
    if(!values) return WorldStatus::InvalidArgument;
    for(u32 k=0;k<9;++k) if(!std::isfinite(values[k])||std::abs(values[k])>100000) return WorldStatus::InvalidArgument;
    std::copy_n(values,3,point.position.begin());
    std::copy_n(values+3,3,point.in.begin());
    std::copy_n(values+6,3,point.out.begin());
    if(operation==9||operation==10){point.rollDegrees=values[9];if(!std::isfinite(point.rollDegrees)||std::abs(point.rollDegrees)>3600)return WorldStatus::InvalidArgument;}
    else if(operation==4){const auto *prior=path.point(elementId);if(prior)point.rollDegrees=prior->rollDegrees;}
  }
  if(operation!=3&&operation!=9&&!path.point(elementId)) return WorldStatus::UnknownElement;
  bool changed=false;
  if(operation==3||operation==9) {
    if(index>path.curve.points.size()) return WorldStatus::InvalidArgument;
    if(path.curve.points.size()>=resources::Curve3D::MaximumPoints) return WorldStatus::LimitReached;
    const auto before=index<path.curve.points.size()?path.curve.points[index].id:0;
    changed=path.insertPoint(before,point,allocatedId);
  } else if(operation==4||operation==10) changed=path.editPoint(elementId,point);
  else if(operation==5) changed=path.removePoint(elementId);
  else {
    if(index>=path.curve.points.size()) return WorldStatus::InvalidArgument;
    const auto it=std::find_if(path.curve.points.begin(),path.curve.points.end(),[&](const auto &p){return p.id==elementId;});
    const auto current=static_cast<u32>(it-path.curve.points.begin());
    if(current==index) return WorldStatus::Ok;
    const auto before=index>current?(index+1<path.curve.points.size()?path.curve.points[index+1].id:0):path.curve.points[index].id;
    changed=path.movePoint(elementId,before);
  }
  if(!changed||!candidate->valid()) {allocatedId=0;return WorldStatus::Rejected;}
  auto *components=editComponents(component.object.id);
  if(!components||!components->replaceInstance(component.instance,*candidate)) {allocatedId=0;return WorldStatus::StaleHandle;}
  invalidateType(scene::Path::descriptor.id);
  return WorldStatus::Ok;
}
}

#pragma once
#include "resources/rig_asset.h"
#include "scene/components.h"

namespace ae::scene {
// Persistent instance placement in the root object's PARENT space. Recomputing
// this from each sampled root pose would silently absorb animated root motion
// into placement on reopen. The reusable rig and imported skin own neither.
//
class RigInstance final : public ComponentValue {
public:
  resources::AssetGuid rig;
  u32 rigRevision=0;
  u64 rootJoint=0;
  u64 placementParent=0;
  resources::RigTransform placement;
  // A draft is serializable. Binding/rebinding establishes placement once,
  // atomically with the corresponding rig resource and scene edit.
  bool placed=false;

  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {
    return std::make_unique<RigInstance>(*this);
  }
  bool valid() const override {
    if(placementParent>std::numeric_limits<u32>::max() ||
       !resources::validRigTransform(placement))return false;
    if(!placed)return rigRevision==0 && rootJoint==0 && placementParent==0;
    return rig.valid() && rigRevision>0 && rootJoint!=0 && placementParent!=0;
  }
  bool ready() const {return valid() && placed && rig.valid();}
  void write(std::ostream &out) const override {
    const auto precision=out.precision();
    out<<std::setprecision(std::numeric_limits<float>::max_digits10)
       <<(rig.valid()?rig.text():"-")<<' '<<rigRevision<<' '<<rootJoint<<' '
       <<placementParent<<' '<<placed;
    for(const auto value:placement.position)out<<' '<<value;
    for(const auto value:placement.rotation)out<<' '<<value;
    for(const auto value:placement.scale)out<<' '<<value;
    out.precision(precision);
  }
  bool read(std::istream &in,u32 version) override {
    if(version!=1)return false;
    RigInstance candidate;std::string guid;u32 placedValue=0;
    if(!(in>>guid>>candidate.rigRevision>>candidate.rootJoint>>candidate.placementParent>>placedValue) || placedValue>1)return false;
    candidate.placed=placedValue!=0;
    if(guid!="-" && !resources::AssetGuid::parse(guid,candidate.rig))return false;
    for(auto &value:candidate.placement.position)if(!(in>>value))return false;
    for(auto &value:candidate.placement.rotation)if(!(in>>value))return false;
    for(auto &value:candidate.placement.scale)if(!(in>>value))return false;
    if(!candidate.valid())return false;
    // Keep the instance identity owned by Components when decoding a value.
    rig=candidate.rig;rigRevision=candidate.rigRevision;rootJoint=candidate.rootJoint;
    placementParent=candidate.placementParent;placement=candidate.placement;
    placed=candidate.placed;return true;
  }
};

inline constexpr std::array<ComponentResourceBinding,1> rigInstanceResources{{
  {"rig","Rig",resources::AssetType::Rig,
   [](const ComponentValue &){return 1u;},
   [](const ComponentValue &value,u32){return static_cast<const RigInstance &>(value).rig;},
   [](ComponentValue &value,u32 slot,resources::AssetGuid guid) {
     if(slot)return false;
     auto &instance=static_cast<RigInstance &>(value);
     if(instance.rig==guid)return true;
     instance.rig=guid;instance.rigRevision=0;instance.rootJoint=0;
     instance.placementParent=0;instance.placement={};instance.placed=false;return true;
   },
   {"Instância","","Escolha o rig e estabeleça o vínculo na ferramenta Rig; trocar recurso não infere outra origem."}}
}};
inline constexpr std::array<ComponentObjectReference,1> rigInstanceReferences{{
  {"placement_parent","Pai do placement","",ObjectReferenceScope::SelfOrAncestor,"Sem vínculo",
   [](const ComponentValue &value){return static_cast<const RigInstance &>(value).placementParent;},
   [](ComponentValue &value,u64 parent){static_cast<RigInstance &>(value).placementParent=parent;},
   {"Instância","","Pai capturado pelo comando de vínculo; clonagem remapeia esta referência.",nullptr,
    [](const ComponentValue &){return false;}},
   [](const ComponentValue &value){return static_cast<const RigInstance &>(value).placed;}}
}};
inline const ComponentType RigInstance::descriptor{
  "astra.animation.rig_instance",1,
  []()->std::unique_ptr<ComponentValue>{return std::make_unique<RigInstance>();},
  {},{},{},nullptr,false,rigInstanceReferences,{},rigInstanceResources
};

inline const RigInstance *rigInstance(const Components &values) {
  return static_cast<const RigInstance *>(values.find(RigInstance::descriptor));
}
} // namespace ae::scene

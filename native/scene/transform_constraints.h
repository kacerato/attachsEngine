#pragma once
#include "scene/components.h"
#include <array>
#include <type_traits>
namespace ae::scene {
// Single-source world-space constraints. Reflection is the authoring, clone,
// serialization and public script contract; no second reference list exists.
enum class ConstraintKind { Position, Rotation, Scale, Aim, Parent, LookAt };
template<ConstraintKind Kind> class TransformConstraint final : public ComponentValue {
public:
 u64 target=0; bool enabled=true; float weight=1; float offset[3]{Kind==ConstraintKind::Scale?1.f:0.f,Kind==ConstraintKind::Scale?1.f:0.f,Kind==ConstraintKind::Scale?1.f:0.f};
 float rotationOffset[3]{}; bool rotationAxis[3]{true,true,true}; float roll=0;
 bool axis[3]{true,true,true}; u32 aimAxis=2,upAxis=0;
 static const ComponentType descriptor;
 const ComponentType &type() const override {return descriptor;}
 std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<TransformConstraint>(*this);}
 bool valid() const override {
  if(target>std::numeric_limits<u32>::max()||aimAxis>5||upAxis>2) return false;
  for(const auto &p:descriptor.numbers) {const float n=p.read(*this);if(!std::isfinite(n)||n<p.minimum||n>p.maximum)return false;}
  return true;
 }
 void write(std::ostream &out) const override {out<<target<<' '<<enabled<<' '<<weight;for(float n:offset)out<<' '<<n;for(bool n:axis)out<<' '<<n;if constexpr(Kind==ConstraintKind::Aim || Kind==ConstraintKind::LookAt)out<<' '<<aimAxis<<' '<<upAxis;if constexpr(Kind==ConstraintKind::Parent){for(float n:rotationOffset)out<<' '<<n;for(bool n:rotationAxis)out<<' '<<n;}if constexpr(Kind==ConstraintKind::LookAt)out<<' '<<roll;}
 bool read(std::istream &in,u32 version) override {if(version!=1||!(in>>target>>enabled>>weight))return false;for(float &n:offset)if(!(in>>n))return false;for(auto &n:axis)if(!(in>>n))return false;if constexpr(Kind==ConstraintKind::Aim || Kind==ConstraintKind::LookAt)if(!(in>>aimAxis>>upAxis))return false;if constexpr(Kind==ConstraintKind::Parent){for(float &n:rotationOffset)if(!(in>>n))return false;for(auto &n:rotationAxis)if(!(in>>n))return false;}if constexpr(Kind==ConstraintKind::LookAt)if(!(in>>roll))return false;return valid();}
};
using PositionConstraint=TransformConstraint<ConstraintKind::Position>;
using RotationConstraint=TransformConstraint<ConstraintKind::Rotation>;
using ScaleConstraint=TransformConstraint<ConstraintKind::Scale>;
using AimConstraint=TransformConstraint<ConstraintKind::Aim>;
using ParentConstraint=TransformConstraint<ConstraintKind::Parent>;
using LookAtConstraint=TransformConstraint<ConstraintKind::LookAt>;
template<class T> inline constexpr bool constraintScale=std::is_same_v<T,ScaleConstraint>;
template<class T> inline constexpr bool constraintAngular=std::is_same_v<T,RotationConstraint>||std::is_same_v<T,AimConstraint>;
#include "scene/generated/transform_constraints_constraintNumbers.inc"
#include "scene/generated/transform_constraints_constraintBooleans.inc"
template<class T> inline constexpr std::array<ComponentObjectReference,1> constraintReferences{{
 {"target","Fonte","",ObjectReferenceScope::OtherNonDescendant,"Escolher fonte",[](const ComponentValue &v){return static_cast<const T&>(v).target;},[](ComponentValue &v,u64 n){static_cast<T&>(v).target=n;},{"Influência"},[](const ComponentValue &v){const auto &c=static_cast<const T&>(v);return c.enabled&&c.weight>0;}}
}};
inline constexpr std::array<ComponentTriple,1> constraintTriples{{{"offset","Deslocamento",{"offset_x","offset_y","offset_z"}}}};
inline constexpr std::array<ComponentEnumOption,6> aimAxisOptions{{{0,"+X"},{1,"+Y"},{2,"+Z"},{3,"-X"},{4,"-Y"},{5,"-Z"}}};
inline constexpr std::array<ComponentEnumOption,3> upAxisOptions{{{0,"Y"},{1,"Z"},{2,"X"}}};
#include "scene/generated/transform_constraints_aimEnums.inc"
template<> inline const ComponentType PositionConstraint::descriptor{"astra.constraint.position",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<PositionConstraint>();},constraintNumbers<PositionConstraint>,constraintBooleans<PositionConstraint>,{},nullptr,false,constraintReferences<PositionConstraint>,constraintTriples};
template<> inline const ComponentType RotationConstraint::descriptor{"astra.constraint.rotation",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<RotationConstraint>();},constraintNumbers<RotationConstraint>,constraintBooleans<RotationConstraint>,{},nullptr,false,constraintReferences<RotationConstraint>,constraintTriples};
template<> inline const ComponentType ScaleConstraint::descriptor{"astra.constraint.scale",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<ScaleConstraint>();},constraintNumbers<ScaleConstraint>,constraintBooleans<ScaleConstraint>,{},nullptr,false,constraintReferences<ScaleConstraint>,constraintTriples};
template<> inline const ComponentType AimConstraint::descriptor{"astra.constraint.aim",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<AimConstraint>();},constraintNumbers<AimConstraint>,constraintBooleans<AimConstraint>,aimEnums,nullptr,false,constraintReferences<AimConstraint>,constraintTriples};
inline const auto parentNumbers=[] {std::array<ComponentNumber,7> a{};for(u32 i=0;i<4;++i)a[i]=constraintNumbers<ParentConstraint>[i];
 a[4]={"Rotação offset X",-10000,10000,.1f,[](const ComponentValue&v)->const float&{return static_cast<const ParentConstraint&>(v).rotationOffset[0];},[](ComponentValue&v){return &static_cast<ParentConstraint&>(v).rotationOffset[0];},"rotation_offset_x",{"Rotação","graus"}};
 a[5]={"Rotação offset Y",-10000,10000,.1f,[](const ComponentValue&v)->const float&{return static_cast<const ParentConstraint&>(v).rotationOffset[1];},[](ComponentValue&v){return &static_cast<ParentConstraint&>(v).rotationOffset[1];},"rotation_offset_y",{"Rotação","graus"}};
 a[6]={"Rotação offset Z",-10000,10000,.1f,[](const ComponentValue&v)->const float&{return static_cast<const ParentConstraint&>(v).rotationOffset[2];},[](ComponentValue&v){return &static_cast<ParentConstraint&>(v).rotationOffset[2];},"rotation_offset_z",{"Rotação","graus"}};return a;}();
inline const auto parentBooleans=[] {std::array<ComponentBoolean,7>a{};for(u32 i=0;i<4;++i)a[i]=constraintBooleans<ParentConstraint>[i];
 a[4]={"rotation_x","Rotação X",[](const ComponentValue&v){return static_cast<const ParentConstraint&>(v).rotationAxis[0];},[](ComponentValue&v,bool n){static_cast<ParentConstraint&>(v).rotationAxis[0]=n;},{"Rotação"}};
 a[5]={"rotation_y","Rotação Y",[](const ComponentValue&v){return static_cast<const ParentConstraint&>(v).rotationAxis[1];},[](ComponentValue&v,bool n){static_cast<ParentConstraint&>(v).rotationAxis[1]=n;},{"Rotação"}};
 a[6]={"rotation_z","Rotação Z",[](const ComponentValue&v){return static_cast<const ParentConstraint&>(v).rotationAxis[2];},[](ComponentValue&v,bool n){static_cast<ParentConstraint&>(v).rotationAxis[2]=n;},{"Rotação"}};return a;}();
inline const auto lookNumbers=[] {std::array<ComponentNumber,2>a{};a[0]=constraintNumbers<LookAtConstraint>[0];a[1]={"Roll",-360,360,.1f,[](const ComponentValue&v)->const float&{return static_cast<const LookAtConstraint&>(v).roll;},[](ComponentValue&v){return &static_cast<LookAtConstraint&>(v).roll;},"roll",{"Mira","graus"}};return a;}();
#include "scene/generated/transform_constraints_lookEnums.inc"
inline constexpr std::array<ComponentTriple,2>parentTriples{{{"offset","Posição offset",{"offset_x","offset_y","offset_z"}},{"rotation_offset","Rotação offset",{"rotation_offset_x","rotation_offset_y","rotation_offset_z"}}}};
template<> inline const ComponentType ParentConstraint::descriptor{"astra.constraint.parent",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<ParentConstraint>();},parentNumbers,parentBooleans,{},nullptr,false,constraintReferences<ParentConstraint>,parentTriples};
template<> inline const ComponentType LookAtConstraint::descriptor{"astra.constraint.look_at",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<LookAtConstraint>();},lookNumbers,constraintBooleans<LookAtConstraint>,lookEnums,nullptr,false,constraintReferences<LookAtConstraint>};
}

#pragma once
#include "scene/components.h"
#include <array>
#include <type_traits>
namespace ae::scene {
// Astra adaptation of Unity 6000.0 transform constraints + damped following.
// Authoring is persisted; velocity belongs exclusively to SceneConstraints.
enum class SpringChannel { Position, Rotation, Scale };
template<SpringChannel Channel> class SpringConstraint final : public ComponentValue {
public:
 u64 target=0; bool enabled=true; float weight=1,frequency=3,dampingRatio=1,maxSpeed=1000;
 float offset[3]{Channel==SpringChannel::Scale?1.f:0.f,Channel==SpringChannel::Scale?1.f:0.f,Channel==SpringChannel::Scale?1.f:0.f};
 // Channel is complete XYZ. Kept as a constant contract for shared evaluation.
 bool axis[3]{true,true,true};
 static const ComponentType descriptor;
 const ComponentType &type() const override {return descriptor;}
 std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<SpringConstraint>(*this);}
 bool valid() const override {
  if(target>std::numeric_limits<u32>::max())return false;
  for(const auto &p:descriptor.numbers){auto n=p.read(*this);if(!std::isfinite(n)||n<p.minimum||n>p.maximum)return false;}
  return true;
 }
 void write(std::ostream &out) const override {out<<target<<' '<<enabled<<' '<<weight<<' '<<frequency<<' '<<dampingRatio<<' '<<maxSpeed;for(auto n:offset)out<<' '<<n;}
 bool read(std::istream &in,u32 version) override {
  if(version!=1||!(in>>target>>enabled>>weight>>frequency>>dampingRatio>>maxSpeed))return false;
  for(auto &n:offset)if(!(in>>n))return false;
  return valid();
 }
};
using SpringPositionConstraint=SpringConstraint<SpringChannel::Position>;
using SpringRotationConstraint=SpringConstraint<SpringChannel::Rotation>;
using SpringScaleConstraint=SpringConstraint<SpringChannel::Scale>;
template<class T> inline const std::array<ComponentNumber,7> springNumbers{{
#define AE_SPRING_NUMBER(id,label,field,lo,hi,step,group,unit) {label,lo,hi,step,[](const ComponentValue&v)->const float&{return static_cast<const T&>(v).field;},[](ComponentValue&v){return &static_cast<T&>(v).field;},id,{group,unit}}
 AE_SPRING_NUMBER("weight","Influência",weight,0,1,.05f,"Fonte",""),
 AE_SPRING_NUMBER("frequency","Frequência",frequency,.01f,60,.1f,"Resposta","Hz"),
 AE_SPRING_NUMBER("damping_ratio","Razão de amortecimento",dampingRatio,0,10,.05f,"Resposta",""),
 AE_SPRING_NUMBER("max_speed","Velocidade máxima",maxSpeed,.001f,100000,1,"Resposta",(std::is_same_v<T,SpringRotationConstraint>?"graus/s":std::is_same_v<T,SpringScaleConstraint>?"x/s":"m/s")),
 AE_SPRING_NUMBER("offset_x","Offset X",offset[0],(std::is_same_v<T,SpringScaleConstraint>?.0001f:-10000.f),10000,.1f,"Ajustes",""),
 AE_SPRING_NUMBER("offset_y","Offset Y",offset[1],(std::is_same_v<T,SpringScaleConstraint>?.0001f:-10000.f),10000,.1f,"Ajustes",""),
 AE_SPRING_NUMBER("offset_z","Offset Z",offset[2],(std::is_same_v<T,SpringScaleConstraint>?.0001f:-10000.f),10000,.1f,"Ajustes","")
#undef AE_SPRING_NUMBER
}};
template<class T> inline const std::array<ComponentBoolean,1> springBooleans{{
 {"enabled","Ativo",[](const ComponentValue&v){return static_cast<const T&>(v).enabled;},[](ComponentValue&v,bool n){static_cast<T&>(v).enabled=n;},{"Fonte"}}
}};
template<class T> inline const std::array<ComponentObjectReference,1> springReferences{{
 {"target","Fonte","",ObjectReferenceScope::OtherNonDescendant,"Escolher fonte",[](const ComponentValue&v){return static_cast<const T&>(v).target;},[](ComponentValue&v,u64 n){static_cast<T&>(v).target=n;},{"Fonte"}}
}};
inline const std::array<ComponentTriple,1> springTriples{{{"offset","Offset",{"offset_x","offset_y","offset_z"}}}};
#define AE_SPRING_DESCRIPTOR(T,id) template<> inline const ComponentType T::descriptor{id,1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<T>();},springNumbers<T>,springBooleans<T>,{},nullptr,false,springReferences<T>,springTriples};
AE_SPRING_DESCRIPTOR(SpringPositionConstraint,"astra.spring.position")
AE_SPRING_DESCRIPTOR(SpringRotationConstraint,"astra.spring.rotation")
AE_SPRING_DESCRIPTOR(SpringScaleConstraint,"astra.spring.scale")
#undef AE_SPRING_DESCRIPTOR
}

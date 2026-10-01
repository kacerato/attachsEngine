#pragma once
#include "scene/physics_field.h"
namespace ae::scene {
// Shared field policy, independent authoring types; only XY channels persist.
template<PhysicsFieldKind K> class PhysicsField2D final : public PhysicsFieldProperties {
public:
 PhysicsField2D(){if constexpr(K==PhysicsFieldKind::Gravity)vector[1]=-9.81f;else if constexpr(K==PhysicsFieldKind::Wind)vector[0]=5;}
 static const ComponentType descriptor;
 const ComponentType &type() const override{return descriptor;}
 std::unique_ptr<ComponentValue> clone() const override{return std::make_unique<PhysicsField2D>(*this);}
};
using GravityField2D=PhysicsField2D<PhysicsFieldKind::Gravity>;
using WindField2D=PhysicsField2D<PhysicsFieldKind::Wind>;
using DragField2D=PhysicsField2D<PhysicsFieldKind::Drag>;
using RadialField2D=PhysicsField2D<PhysicsFieldKind::Radial>;
template<PhysicsFieldKind K> inline constexpr auto field2DNumbers=[] {
 constexpr usize extra=K==PhysicsFieldKind::Wind?3:2;
 std::array<ComponentNumber,5+extra> out{};
 for(u32 n=0;n<5;++n)out[n]=fieldVolumeNumbers[n<2?n:n+1];
 if constexpr(K==PhysicsFieldKind::Gravity||K==PhysicsFieldKind::Wind){out[5]=fieldNumbers<K>[7];out[6]=fieldNumbers<K>[8];if constexpr(K==PhysicsFieldKind::Wind)out[7]=fieldNumbers<K>[10];}
 else {out[5]=fieldNumbers<K>[7];out[6]=fieldNumbers<K>[8];}
 return out;
}();
inline constexpr std::array<ComponentEnumOption,2> field2DShapes{{{0,"Retângulo"},{1,"Círculo"}}};
inline constexpr auto field2DEnums=[]{auto out=fieldEnums;out[0].options=field2DShapes;return out;}();
template<PhysicsFieldKind K> inline constexpr auto field2DPairs=[] {
 std::array<ComponentTriple,(K==PhysicsFieldKind::Gravity||K==PhysicsFieldKind::Wind)?3:2> out{};
 out[0]={"half_extents","Meias XY",{"half_x","half_y",{}},ComponentTripleKind::Vector2};
 out[1]={"offset","Centro local",{"offset_x","offset_y",{}},ComponentTripleKind::Vector2};
 if constexpr(K==PhysicsFieldKind::Gravity||K==PhysicsFieldKind::Wind)out[2]={"vector",K==PhysicsFieldKind::Gravity?"Gravidade local":"Vento local",{"vector_x","vector_y",{}},ComponentTripleKind::Vector2};
 return out;
}();
#define AE_FIELD2D_TYPE(T,K,id) template<> inline const ComponentType T::descriptor{id,1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<T>();},field2DNumbers<PhysicsFieldKind::K>,fieldBooleans<PhysicsFieldKind::K>,field2DEnums,nullptr,false,{},field2DPairs<PhysicsFieldKind::K>};
AE_FIELD2D_TYPE(GravityField2D,Gravity,"astra.physics2d.field.gravity")
AE_FIELD2D_TYPE(WindField2D,Wind,"astra.physics2d.field.wind")
AE_FIELD2D_TYPE(DragField2D,Drag,"astra.physics2d.field.drag")
AE_FIELD2D_TYPE(RadialField2D,Radial,"astra.physics2d.field.radial")
#undef AE_FIELD2D_TYPE
inline int physicsField2DKind(const ComponentValue &v){
 const ComponentType *types[]{&GravityField2D::descriptor,&WindField2D::descriptor,&DragField2D::descriptor,&RadialField2D::descriptor};
 for(int n=0;n<4;++n)if(v.type().id==types[n]->id)return n;
 return -1;
}
}

#pragma once
#include "scene/components.h"
#include <array>
namespace ae::scene {
enum class PathFollowMode:u32 {Speed,Duration};
class PathFollow final:public ComponentValue {
public:
 u64 target=0;
#include "scene/generated/path_follow_PathFollow_fields0.inc"

 PathFollowMode mode=PathFollowMode::Speed;
 float progressDistance=0,speed=1,duration=5,offset[3]{};
 static const ComponentType descriptor;
 const ComponentType &type()const override{return descriptor;}
 std::unique_ptr<ComponentValue> clone()const override{return std::make_unique<PathFollow>(*this);}
 bool valid()const override{if(target>std::numeric_limits<u32>::max()||static_cast<u32>(mode)>1)return false;for(const auto&p:descriptor.numbers){const float n=p.read(*this);if(!std::isfinite(n)||n<p.minimum||n>p.maximum)return false;}return true;}
 void write(std::ostream&o)const override{o<<target<<' '<<enabled<<' '<<autoplay<<' '<<loop<<' '<<backwards<<' '<<orient<<' '<<static_cast<u32>(mode);for(const auto&p:descriptor.numbers)o<<' '<<p.read(*this);}
 bool read(std::istream&i,u32 v)override{u32 m;if(v!=1||!(i>>target>>enabled>>autoplay>>loop>>backwards>>orient>>m)||m>1)return false;mode=static_cast<PathFollowMode>(m);for(const auto&p:descriptor.numbers)if(!(i>>*p.write(*this)))return false;return valid();}
};
#include "scene/generated/path_follow_pathFollowNumbers.inc"
#include "scene/generated/path_follow_pathFollowBooleans.inc"
inline constexpr std::array<ComponentEnumOption,2> pathFollowModes{{{0,"Velocidade mundial"},{1,"Duração do percurso"}}};
#include "scene/generated/path_follow_pathFollowEnums.inc"
inline constexpr std::array<ComponentObjectReference,1> pathFollowReferences{{{"target","Caminho","astra.path",ObjectReferenceScope::Other,"Escolher Path",[](const ComponentValue&v){return static_cast<const PathFollow&>(v).target;},[](ComponentValue&v,u64 id){static_cast<PathFollow&>(v).target=id;},{"Percurso"},[](const ComponentValue&v){return static_cast<const PathFollow&>(v).enabled;}}}};
// Operações sobre o seguidor de Play (runtime/scene_paths.h).
inline constexpr std::array<ComponentMethod,4> pathFollowMethods{{
 {"restart","Reiniciar","Volta à distância inicial e avança"},
 {"stop","Parar","Congela pose e progresso"},
 {"progress","Progresso","Distância percorrida no mundo",{},ComponentValueKind::Number},
 {"playing","Em movimento","Verdadeiro enquanto avança",{},ComponentValueKind::Boolean},
}};
inline const ComponentType PathFollow::descriptor{"astra.path.follow",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<PathFollow>();},pathFollowNumbers,pathFollowBooleans,pathFollowEnums,nullptr,false,pathFollowReferences,{},{},{},{},{},pathFollowMethods};
}

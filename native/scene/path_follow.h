#pragma once
#include "scene/components.h"
#include <array>
namespace ae::scene {
enum class PathFollowMode:u32 {Speed,Duration};
class PathFollow final:public ComponentValue {
public:
 u64 target=0;bool enabled=true,autoplay=true,loop=false,backwards=false,orient=true;
 PathFollowMode mode=PathFollowMode::Speed;
 float progressDistance=0,speed=1,duration=5,offset[3]{};
 static const ComponentType descriptor;
 const ComponentType &type()const override{return descriptor;}
 std::unique_ptr<ComponentValue> clone()const override{return std::make_unique<PathFollow>(*this);}
 bool valid()const override{if(target>std::numeric_limits<u32>::max()||static_cast<u32>(mode)>1)return false;for(const auto&p:descriptor.numbers){const float n=p.read(*this);if(!std::isfinite(n)||n<p.minimum||n>p.maximum)return false;}return true;}
 void write(std::ostream&o)const override{o<<target<<' '<<enabled<<' '<<autoplay<<' '<<loop<<' '<<backwards<<' '<<orient<<' '<<static_cast<u32>(mode);for(const auto&p:descriptor.numbers)o<<' '<<p.read(*this);}
 bool read(std::istream&i,u32 v)override{u32 m;if(v!=1||!(i>>target>>enabled>>autoplay>>loop>>backwards>>orient>>m)||m>1)return false;mode=static_cast<PathFollowMode>(m);for(const auto&p:descriptor.numbers)if(!(i>>*p.write(*this)))return false;return valid();}
};
inline constexpr std::array<ComponentNumber,6> pathFollowNumbers{{
#define AE_PATH_NUMBER(id,label,field,min,max,group,unit,condition) {label,min,max,.1f,[](const ComponentValue&v)->const float&{return static_cast<const PathFollow&>(v).field;},[](ComponentValue&v){return &static_cast<PathFollow&>(v).field;},id,{group,unit,nullptr,[](const ComponentValue&v){const auto&c=static_cast<const PathFollow&>(v);(void)c;return condition;}}}
 AE_PATH_NUMBER("progress_distance","Distância inicial",progressDistance,0,1000000,"Percurso","u",true),
 AE_PATH_NUMBER("speed","Velocidade",speed,0,100000,"Percurso","u/s",c.mode==PathFollowMode::Speed),
 AE_PATH_NUMBER("duration","Duração",duration,.001f,100000,"Percurso","s",c.mode==PathFollowMode::Duration),
 AE_PATH_NUMBER("offset_x","Deslocamento lateral",offset[0],-10000,10000,"Orientação","u",true),
 AE_PATH_NUMBER("offset_y","Deslocamento vertical",offset[1],-10000,10000,"Orientação","u",true),
 AE_PATH_NUMBER("offset_z","Deslocamento tangente",offset[2],-10000,10000,"Orientação","u",true)
#undef AE_PATH_NUMBER
}};
inline constexpr std::array<ComponentBoolean,5> pathFollowBooleans{{
#define AE_PATH_BOOL(id,label,field,group) {id,label,[](const ComponentValue&v){return static_cast<const PathFollow&>(v).field;},[](ComponentValue&v,bool b){static_cast<PathFollow&>(v).field=b;},{group}}
 AE_PATH_BOOL("enabled","Ativo",enabled,"Execução"),AE_PATH_BOOL("autoplay","Iniciar no Play",autoplay,"Execução"),AE_PATH_BOOL("loop","Repetir percurso",loop,"Execução"),AE_PATH_BOOL("backwards","Sentido inverso",backwards,"Execução"),AE_PATH_BOOL("orient","Orientar +Z",orient,"Orientação")
#undef AE_PATH_BOOL
}};
inline constexpr std::array<ComponentEnumOption,2> pathFollowModes{{{0,"Velocidade mundial"},{1,"Duração do percurso"}}};
inline constexpr std::array<ComponentEnum,1> pathFollowEnums{{{"mode","Avanço",pathFollowModes,[](const ComponentValue&v){return static_cast<u32>(static_cast<const PathFollow&>(v).mode);},[](ComponentValue&v,u32 m){static_cast<PathFollow&>(v).mode=static_cast<PathFollowMode>(m);},{"Percurso"}}}};
inline constexpr std::array<ComponentObjectReference,1> pathFollowReferences{{{"target","Caminho","astra.path",ObjectReferenceScope::Other,"Escolher Path",[](const ComponentValue&v){return static_cast<const PathFollow&>(v).target;},[](ComponentValue&v,u64 id){static_cast<PathFollow&>(v).target=id;},{"Percurso"},[](const ComponentValue&v){return static_cast<const PathFollow&>(v).enabled;}}}};
inline const ComponentType PathFollow::descriptor{"astra.path.follow",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<PathFollow>();},pathFollowNumbers,pathFollowBooleans,pathFollowEnums,nullptr,false,pathFollowReferences};
}

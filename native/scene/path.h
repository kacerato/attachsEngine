#pragma once
#include "scene/components.h"
#include "resources/curve3d.h"
#include <iomanip>
namespace ae::scene {
class Path final:public ComponentValue {
public:
 resources::Curve3D curve;u64 nextPointId=1;
 // beforeId=0 appends. Identity allocation is persisted; edits/remaps never regenerate IDs.
 bool insertPoint(u64 beforeId,const resources::CurvePoint3D&value,u64&outId){
  outId=0;if(curve.points.size()>=resources::Curve3D::MaximumPoints||nextPointId==std::numeric_limits<u64>::max())return false;
  auto at=curve.points.end();if(beforeId){at=std::find_if(curve.points.begin(),curve.points.end(),[&](const auto&p){return p.id==beforeId;});if(at==curve.points.end())return false;}
  auto point=value;point.id=nextPointId;resources::Curve3D probe;probe.points.push_back(point);if(!probe.valid())return false;
  curve.points.insert(at,point);outId=nextPointId++;return true;
 }
 bool removePoint(u64 id){auto at=std::find_if(curve.points.begin(),curve.points.end(),[&](const auto&p){return p.id==id;});if(at==curve.points.end())return false;curve.points.erase(at);return true;}
 bool movePoint(u64 id,u64 beforeId){if(id==beforeId)return false;auto at=std::find_if(curve.points.begin(),curve.points.end(),[&](const auto&p){return p.id==id;});if(at==curve.points.end())return false;if(beforeId&&!std::any_of(curve.points.begin(),curve.points.end(),[&](const auto&p){return p.id==beforeId;}))return false;const auto point=*at;curve.points.erase(at);auto dest=beforeId?std::find_if(curve.points.begin(),curve.points.end(),[&](const auto&p){return p.id==beforeId;}):curve.points.end();curve.points.insert(dest,point);return true;}
 bool editPoint(u64 id,const resources::CurvePoint3D&value){auto at=std::find_if(curve.points.begin(),curve.points.end(),[&](const auto&p){return p.id==id;});if(at==curve.points.end())return false;auto point=value;point.id=id;resources::Curve3D probe;probe.points.push_back(point);if(!probe.valid())return false;*at=point;return true;}
 const resources::CurvePoint3D*point(u64 id)const {const auto at=std::find_if(curve.points.begin(),curve.points.end(),[&](const auto&p){return p.id==id;});return at==curve.points.end()?nullptr:&*at;}
 static const ComponentType descriptor;
 const ComponentType&type()const override{return descriptor;}
 std::unique_ptr<ComponentValue>clone()const override{return std::make_unique<Path>(*this);}
 bool valid()const override{if(!curve.valid()||!nextPointId)return false;for(const auto&p:curve.points)if(p.id>=nextPointId)return false;return true;}
 void write(std::ostream&o)const override{const auto precision=o.precision();o<<std::setprecision(std::numeric_limits<float>::max_digits10)<<curve.closed<<' '<<nextPointId<<' '<<curve.points.size();for(float n:curve.up)o<<' '<<n;for(const auto&p:curve.points){o<<' '<<p.id;for(const auto*v:{&p.position,&p.in,&p.out})for(float n:*v)o<<' '<<n;o<<' '<<p.rollDegrees;}o.precision(precision);}
 bool read(std::istream&i,u32 version)override{if(version!=1&&version!=2)return false;Path candidate;usize count=0;if(!(i>>candidate.curve.closed>>candidate.nextPointId>>count)||count>resources::Curve3D::MaximumPoints)return false;if(version==2)for(float&n:candidate.curve.up)if(!(i>>n))return false;candidate.curve.points.resize(count);for(auto&p:candidate.curve.points){if(!(i>>p.id))return false;for(auto*v:{&p.position,&p.in,&p.out})for(float&n:*v)if(!(i>>n))return false;if(version==2&&!(i>>p.rollDegrees))return false;}if(!candidate.valid())return false;curve=std::move(candidate.curve);nextPointId=candidate.nextPointId;return true;}
};
#include "scene/generated/path_pathBooleans.inc"
inline const std::array<ComponentSlotNumber,10>pathSlotNumbers=[] {
 std::array<ComponentSlotNumber,10>a{};
 const char*ids[]{"point_position_x","point_position_y","point_position_z","point_in_x","point_in_y","point_in_z","point_out_x","point_out_y","point_out_z"};
 const char*names[]{"Posição X","Posição Y","Posição Z","Entrada X","Entrada Y","Entrada Z","Saída X","Saída Y","Saída Z"};
 // Function pointers stay concrete per channel, retaining the existing slot ABI.
#define AE_PATH_SLOT(N,M,K) a[N]={ids[N],names[N],-100000,100000,.1f,[](const ComponentValue&v){return u32(static_cast<const Path&>(v).curve.points.size());},[](const ComponentValue&v,u32 s){return static_cast<const Path&>(v).curve.points[s].M[K];},[](ComponentValue&v,u32 s,float n){auto&p=static_cast<Path&>(v);if(s>=p.curve.points.size()||!std::isfinite(n)||std::abs(n)>100000)return false;p.curve.points[s].M[K]=n;return true;},{N<3?"Posição":"Tangentes","u"}};
 AE_PATH_SLOT(0,position,0) AE_PATH_SLOT(1,position,1) AE_PATH_SLOT(2,position,2) AE_PATH_SLOT(3,in,0) AE_PATH_SLOT(4,in,1) AE_PATH_SLOT(5,in,2) AE_PATH_SLOT(6,out,0) AE_PATH_SLOT(7,out,1) AE_PATH_SLOT(8,out,2)
#undef AE_PATH_SLOT
 a[9]={"point_roll","Roll",-3600,3600,1,[](const ComponentValue&v){return u32(static_cast<const Path&>(v).curve.points.size());},[](const ComponentValue&v,u32 s){return static_cast<const Path&>(v).curve.points[s].rollDegrees;},[](ComponentValue&v,u32 s,float n){auto&p=static_cast<Path&>(v);if(s>=p.curve.points.size()||!std::isfinite(n)||std::abs(n)>3600)return false;p.curve.points[s].rollDegrees=n;return true;},{"Orientação","°","Ângulo contínuo, sem reduzir módulo 360."}};
 return a;}();
#include "scene/generated/path_pathNumbers.inc"
inline constexpr std::array<ComponentTriple,1>pathTriples{{{"up","Up inicial local",{"up_x","up_y","up_z"},ComponentTripleKind::Vector}}};
inline const std::array<ComponentCollection,1> pathCollections{{
 {"points",[](const ComponentValue&v){return u32(static_cast<const Path&>(v).curve.points.size());},
  [](const ComponentValue&v,u32 slot){return static_cast<const Path&>(v).curve.points[slot].id;},
  [](const ComponentValue&v){return static_cast<const Path&>(v).nextPointId;},
  [](ComponentValue&v,u64 floor){if(!floor)return false;auto &path=static_cast<Path&>(v);path.nextPointId=std::max(path.nextPointId,floor);return path.valid();}}
}};
inline const ComponentType Path::descriptor{"astra.path",2,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<Path>();},pathNumbers,pathBooleans,{},nullptr,false,{},pathTriples,{},pathSlotNumbers,{},pathCollections};
}

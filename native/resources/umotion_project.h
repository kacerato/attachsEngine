#pragma once
// Projeto UMotion Pro (.asset, MonoBehaviour UMotionProjectFile V01_02) como
// dados neutros: objetos animados do rig com pose de referencia, canais por
// clipe e camada (chaves em quadros, tangentes por quadro), IK de plano
// rotativo, Child-Of, propriedades customizadas, camadas e eventos do clipe.
// Formato: manual UMotion Pro 1.29 (Assets/UMotionEditor/Manual) e a
// definicao serializada do proprio plugin, lida para interoperabilidade.
#include "core/base.h"
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace ae::resources {
// Modos de tangente do UMotion (iguais aos do AnimationUtility da Unity).
enum class UMotionTangent : u8 { ClampedAuto=0, Auto=1, Linear=2, Constant=3, Free=4 };
struct UMotionKey {
  i32 frame=0;
  float value=0,inTangent=0,outTangent=0,inWeight=1.f/3,outWeight=1.f/3;
  u32 mode=0;
  UMotionTangent incoming() const {return static_cast<UMotionTangent>(mode&0xF);}
  UMotionTangent outgoing() const {return static_cast<UMotionTangent>((mode>>4)&0xF);}
  bool broken() const {return (mode&0x100)!=0;}
  // Bits 12-15: 1 entrada ponderada, 2 saida ponderada, 3 ambas (WeightedMode da Unity).
  bool weightedIn() const {return ((mode>>12)&1)!=0;}
  bool weightedOut() const {return ((mode>>12)&2)!=0;}
};
struct UMotionReferenceKey {i32 frame=0;i32 object=-1;};
// Por clipe, uma lista de chaves por camada (camada 0 = base).
struct UMotionFloatChannel {
  float reference=0;
  std::vector<std::vector<std::vector<UMotionKey>>> clips;
  const std::vector<UMotionKey> *keys(usize clip,usize layer) const {
    return clip<clips.size()&&layer<clips[clip].size()?&clips[clip][layer]:nullptr;
  }
};
struct UMotionReferenceChannel {
  i32 reference=-1;
  std::vector<std::vector<std::vector<UMotionReferenceKey>>> clips;
};
enum class UMotionRotationMode : u8 { ProgressiveQuaternion=0, Quaternion=1, Euler=2 };
struct UMotionCustomProperty {
  std::string name,propertyPath,propertyType;
  std::vector<i32> driven;
  u32 drivenChannel=0,mode=0;
  bool scaling=false,constrainInput=false;
  float driverMin=0,driverMax=1,drivenMin=0,drivenMax=1;
  UMotionFloatChannel channel;
};
struct UMotionObject {
  std::string path;
  u32 type=0;                       // 0 osso, 1 transform, 2 alvo de IK/objeto auxiliar
  i32 humanBone=-1;
  std::array<UMotionFloatChannel,3> position,scale;
  std::array<UMotionFloatChannel,5> rotation; // x,y,z,w, progressao (graus); Euler usa 0..2
  std::vector<UMotionRotationMode> rotationMode; // por clipe
  bool ik=false,ikLegacy=false;
  i32 ikTarget=-1,ikPole=-1;
  u32 ikChainLength=2,ikReferenceMode=0,ikTargetRotationMode=0;
  i32 ikChainMask=-1;
  float ikPlaneAngle=0;
  UMotionFloatChannel fkIkBlend,poleRotation;
  bool childOf=false;
  UMotionReferenceChannel childOfParent;
  UMotionFloatChannel childOfPinning;
  std::vector<UMotionCustomProperty> customProperties;
};
struct UMotionLayer {std::string name;bool overrideBlend=false,mute=false;float weight=1;};
struct UMotionEventFunction {std::string name;u32 parameterType=0;std::string parameter;};
struct UMotionEvent {i32 frame=0;std::vector<UMotionEventFunction> functions;};
struct UMotionClip {
  std::string name;
  i32 framerate=60,start=0,end=0;
  bool loop=false,rootMotionTranslation=false,rootMotionRotation=false;
  i32 rootMotionObject=-1;
  float speed=1;
  std::vector<UMotionLayer> layers; // camadas acima da base
  std::vector<UMotionEvent> events;
};
struct UMotionProject {
  std::string name;
  u32 rigType=0;                     // 0 generico, 1 humanoide
  std::vector<UMotionObject> objects;
  std::vector<UMotionClip> clips;
  i32 find(std::string_view path) const {
    for(usize i=0;i<objects.size();++i)if(objects[i].path==path)return static_cast<i32>(i);
    return -1;
  }
};
// Tudo ou nada; o diagnostico nomeia o campo que falhou.
bool readUMotionProject(std::string_view text,UMotionProject &out,std::string &error);
// Valor de um canal no quadro (tangentes por quadro; pesos e modos da Unity).
float sampleUMotionChannel(const std::vector<UMotionKey> &keys,double frame,float fallback);
} // namespace ae::resources

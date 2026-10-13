#pragma once
// Projeto UMotion Pro sintetico para o teste de importacao (sem arquivos
// privados): mesma serializacao do plugin (YAML do MonoBehaviour, canais com
// valueInternal, chaves binarias em hexadecimal, enderecos de objeto).
#include "core/base.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace ae::test::umotion {
struct Key {int frame=0;float value=0,in=0,out=0;unsigned mode=0x122;}; // Linear nos dois lados (quebrada)
inline std::string hex(const std::vector<unsigned char> &b) {
  static const char *d="0123456789abcdef";std::string s;
  for(auto c:b){s+=d[c>>4];s+=d[c&15];}
  return s;
}
template<class T> void put(std::vector<unsigned char> &b,T v){unsigned char t[sizeof(T)];std::memcpy(t,&v,sizeof(T));b.insert(b.end(),t,t+sizeof(T));}
inline std::string keys(const std::vector<Key> &list) {
  if(list.empty())return "";
  std::vector<unsigned char> b;put<int>(b,static_cast<int>(list.size()));
  for(const auto &k:list){put<unsigned char>(b,0);put<int>(b,k.frame);put<unsigned char>(b,0);
    put<float>(b,k.value);put<float>(b,k.in);put<float>(b,k.out);put<float>(b,1.f/3);put<float>(b,1.f/3);put<unsigned>(b,k.mode);}
  return hex(b);
}
inline std::string address(int object) {
  if(object<0)return "        Address: \n        Length: 0\n";
  std::vector<unsigned char> b;put<int>(b,2);put<int>(b,object);
  return "        Address: "+hex(b)+"\n        Length: 2\n";
}
inline std::string referenceKeys(const std::vector<std::pair<int,int>> &list) {
  std::vector<unsigned char> b;put<int>(b,static_cast<int>(list.size()));
  for(const auto &[frame,object]:list){put<unsigned char>(b,0);put<int>(b,frame);put<unsigned char>(b,0);
    if(object<0)put<int>(b,0);else{put<int>(b,2);put<int>(b,2);put<int>(b,object);}}
  return hex(b);
}
inline std::string number(float v){char t[64];std::snprintf(t,sizeof t,"%.9g",v);return t;}
inline std::string channel(float reference,const std::vector<Key> &k) {
  return "    - CurrentValue:\n        valueInternal: "+number(reference)+"\n      ReferencePoseValue:\n        valueInternal: "+number(reference)+
         "\n      PoseValueList: []\n      ChannelPerClipList:\n      - LayerList:\n        - CurrentOriginalValue:\n            valueInternal: "+number(reference)+
         "\n          newKeyListSerialized: "+keys(k)+"\n";
}
inline std::string single(const char *name,float reference,const std::vector<Key> &k) {
  return std::string("    ")+name+":\n      CurrentValue:\n        valueInternal: "+number(reference)+"\n      ReferencePoseValue:\n        valueInternal: "+number(reference)+
         "\n      PoseValueList: []\n      ChannelPerClipList:\n      - LayerList:\n        - CurrentOriginalValue:\n            valueInternal: "+number(reference)+
         "\n          newKeyListSerialized: "+keys(k)+"\n";
}
struct Object {
  std::string path;int type=0,rotationMode=0; // 0 progressivo, 1 quaternion, 2 Euler
  std::array<float,3> position{};std::array<float,4> rotation{0,0,0,1};
  std::array<std::vector<Key>,3> positionKeys;std::array<std::vector<Key>,5> rotationKeys;
  bool ik=false;int ikTarget=-1,chain=2,referenceMode=0,targetRotationMode=0;float plane=0;std::vector<Key> blend,poleRotation;
  bool childOf=false;std::vector<std::pair<int,int>> parents;
};
inline std::string project(const std::vector<Object> &objects,int frames) {
  std::string s="%YAML 1.1\n%TAG !u! tag:unity3d.com,2011:\n--- !u!114 &11400000\nMonoBehaviour:\n  m_ObjectHideFlags: 0\n  m_Name: Sintetico\n  EditorState:\n    EditMode:\n      valueInternal: 0\n  Settings:\n    RigType:\n      valueInternal: 0\n  AnimatedObjectList:\n";
  for(const auto &o:objects) {
    s+="  - TransformPath:\n      valueInternal: "+o.path+"\n    Type:\n      valueInternal: "+std::to_string(o.type)+"\n    IsHumanBone:\n      valueInternal: 0\n";
    s+="    LocalPositionChannelList:\n";for(int c=0;c<3;++c)s+=channel(o.position[c],o.positionKeys[c]);
    s+="    RotationModePerClip:\n    - valueInternal: "+std::to_string(o.rotationMode)+"\n    LocalRotationChannelList:\n";
    for(int c=0;c<5;++c)s+=channel(c<4?o.rotation[c]:0.f,o.rotationKeys[c]);
    s+="    LocalScaleChannelList:\n";for(int c=0;c<3;++c)s+=channel(1,{});
    s+="    IkEnable:\n      valueInternal: "+std::string(o.ik?"1":"0")+"\n    IkTarget:\n      valueInternal:\n"+address(o.ik?o.ikTarget:-1);
    s+="    IkPoleTarget:\n      valueInternal:\n"+address(-1);
    s+="    IkChainLength:\n      valueInternal: "+std::to_string(o.chain)+"\n    IkChainMask:\n      valueInternal: -1\n";
    s+="    IkReferenceMode:\n      valueInternal: "+std::to_string(o.referenceMode)+"\n    IkTargetRotationMode:\n      valueInternal: "+std::to_string(o.targetRotationMode)+"\n";
    s+="    IkPlaneAngleOffset:\n      valueInternal: "+number(o.plane)+"\n    IkLegacyMode:\n      valueInternal: 0\n";
    s+=single("IkFkBlendChannel",1,o.blend)+single("IkPoleRotationChannel",0,o.poleRotation);
    s+="    ChildOfEnable:\n      valueInternal: "+std::string(o.childOf?"1":"0")+"\n    ChildOfIKPinning:\n      valueInternal: 0\n";
    s+="    ChildOfParentChannel:\n      CurrentValue:\n        valueInternal:\n"+std::string("          Address: \n          Length: 0\n")+
       "      ChannelPerClipList:\n      - LayerList:\n        - newKeyListSerialized: "+(o.parents.empty()?std::string():referenceKeys(o.parents))+"\n";
    s+="    CustomPropertyList: []\n";
  }
  s+="  PoseList: []\n  ClipList:\n  - Settings:\n      Name:\n        valueInternal: Sintetico\n      Framerate:\n        valueInternal: 60\n      Loop:\n        valueInternal: 1\n";
  s+="      RootMotion:\n        Translation:\n          valueInternal: 0\n        Rotation:\n          valueInternal: 0\n        AnimatedObject:\n          valueInternal:\n            Address: \n            Length: 0\n";
  s+="    LayerList: []\n    Playback:\n      StartFrame:\n        valueInternal: 0\n      EndFrame:\n        valueInternal: "+std::to_string(frames)+"\n      Speed:\n        valueInternal: 1\n";
  s+="    EventList:\n    - Frame:\n        valueInternal: 20\n      EventFunctionList:\n      - FuncName:\n          valueInternal: Pegou\n        ParamType:\n          valueInternal: 1\n        ParamValue:\n          valueInternal: 3\n";
  return s;
}
} // namespace ae::test::umotion

#include "resources/umotion_project.h"
#include "resources/unity_yaml.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace ae::resources {
namespace {
bool fail(std::string &error,std::string text) {error=std::move(text);return false;}
// Campos do UMotion embrulham o valor em `valueInternal`.
const UnityYamlNode *inner(const UnityYamlNode *node) {
  if(!node)return nullptr;
  if(const auto *v=node->get("valueInternal"))return v;
  return node;
}
bool number(const UnityYamlNode &map,const char *name,float &out) {
  const auto *v=inner(map.get(name));double d=0;
  if(!v||!v->number(d))return false;
  out=static_cast<float>(d);return std::isfinite(out);
}
bool integer(const UnityYamlNode &map,const char *name,i64 &out) {const auto *v=inner(map.get(name));return v&&v->integer(out);}
bool boolean(const UnityYamlNode &map,const char *name,bool &out) {
  i64 n=0;if(!integer(map,name,n))return false;out=n!=0;return true;
}
std::string text(const UnityYamlNode &map,const char *name) {const auto *v=inner(map.get(name));return v&&v->kind==UnityYamlNode::Kind::Scalar?v->scalar:std::string();}
bool hexBytes(std::string_view hex,std::vector<u8> &out) {
  out.clear();if(hex.size()%2)return false;
  const auto digit=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};
  for(usize i=0;i<hex.size();i+=2) {const int a=digit(hex[i]),b=digit(hex[i+1]);if(a<0||b<0)return false;out.push_back(static_cast<u8>(a*16+b));}
  return true;
}
template<class T> bool readValue(const std::vector<u8> &b,usize &at,T &out) {
  if(at+sizeof(T)>b.size())return false;
  std::memcpy(&out,b.data()+at,sizeof(T));at+=sizeof(T);return true;
}
// Endereco serializado: n inteiros; o ultimo e o indice do objeto na lista.
i32 address(const UnityYamlNode *node) {
  node=inner(node);if(!node)return -1;
  i64 length=0;const auto *count=node->get("Length");if(!count||!count->integer(length)||length<=0)return -1;
  const auto *bytes=node->get("Address");std::vector<u8> b;
  if(!bytes||!hexBytes(bytes->scalar,b)||b.size()<static_cast<usize>(length)*4)return -1;
  i32 last=-1;std::memcpy(&last,b.data()+(length-1)*4,4);return last;
}
// Chave: u8, i32 quadro, u8, 5 floats, i32 modo (30 bytes).
bool floatKeys(std::string_view hex,std::vector<UMotionKey> &out,std::string &error) {
  out.clear();if(hex.empty())return true;
  std::vector<u8> b;if(!hexBytes(hex,b))return fail(error,"Chaves UMotion com hexadecimal inválido");
  usize at=0;i32 count=0;if(!readValue(b,at,count)||count<0||count>1000000||b.size()!=4+static_cast<usize>(count)*30)return fail(error,"Lista de chaves UMotion com tamanho inesperado");
  for(i32 k=0;k<count;++k) {
    UMotionKey key;u8 skip=0;
    readValue(b,at,skip);readValue(b,at,key.frame);readValue(b,at,skip);
    readValue(b,at,key.value);readValue(b,at,key.inTangent);readValue(b,at,key.outTangent);readValue(b,at,key.inWeight);readValue(b,at,key.outWeight);
    readValue(b,at,key.mode);
    if(!std::isfinite(key.value))return fail(error,"Valor de chave UMotion não finito");
    if(!out.empty()&&key.frame<=out.back().frame)return fail(error,"Chaves UMotion fora de ordem");
    out.push_back(key);
  }
  return true;
}
// Chave de referencia: u8, i32 quadro, u8, i32 n, n x i32 endereco.
bool referenceKeys(std::string_view hex,std::vector<UMotionReferenceKey> &out,std::string &error) {
  out.clear();if(hex.empty())return true;
  std::vector<u8> b;if(!hexBytes(hex,b))return fail(error,"Chaves de referência UMotion inválidas");
  usize at=0;i32 count=0;if(!readValue(b,at,count)||count<0||count>1000000)return fail(error,"Lista de referências UMotion inválida");
  for(i32 k=0;k<count;++k) {
    UMotionReferenceKey key;u8 skip=0;i32 n=0;
    if(!readValue(b,at,skip)||!readValue(b,at,key.frame)||!readValue(b,at,skip)||!readValue(b,at,n)||n<0||n>64)return fail(error,"Chave de referência UMotion truncada");
    for(i32 i=0;i<n;++i) {i32 v=0;if(!readValue(b,at,v))return fail(error,"Endereço UMotion truncado");key.object=v;}
    if(!n)key.object=-1;
    out.push_back(key);
  }
  return at==b.size()||fail(error,"Lista de referências UMotion com bytes sobrando");
}
template<class Keys,class Read> bool clipLayers(const UnityYamlNode *list,std::vector<std::vector<Keys>> &clips,Read read,std::string &error) {
  clips.clear();if(!list||list->kind!=UnityYamlNode::Kind::Sequence)return true;
  for(const auto &clip:list->elements) {
    auto &layers=clips.emplace_back();const auto *layerList=clip.get("LayerList");
    if(!layerList||layerList->kind!=UnityYamlNode::Kind::Sequence)continue;
    for(const auto &layer:layerList->elements) {
      const auto *serialized=layer.get("newKeyListSerialized");
      if(!read(serialized?serialized->scalar:std::string(),layers.emplace_back(),error))return false;
    }
  }
  return true;
}
bool floatChannel(const UnityYamlNode *node,UMotionFloatChannel &out,std::string &error) {
  out={};if(!node||node->kind!=UnityYamlNode::Kind::Map)return true;
  number(*node,"ReferencePoseValue",out.reference);
  return clipLayers(node->get("ChannelPerClipList"),out.clips,floatKeys,error);
}
bool referenceChannel(const UnityYamlNode *node,UMotionReferenceChannel &out,std::string &error) {
  out={};if(!node||node->kind!=UnityYamlNode::Kind::Map)return true;
  out.reference=address(node->get("CurrentValue"));
  return clipLayers(node->get("ChannelPerClipList"),out.clips,referenceKeys,error);
}
template<usize N> bool channels(const UnityYamlNode &object,const char *name,std::array<UMotionFloatChannel,N> &out,usize minimum,std::string &error) {
  const auto *list=object.get(name);
  if(!list||list->kind!=UnityYamlNode::Kind::Sequence||list->elements.size()<minimum||list->elements.size()>N)
    return fail(error,std::string("Canais ")+name+" ausentes ou com contagem inesperada");
  for(usize i=0;i<list->elements.size();++i)if(!floatChannel(&list->elements[i],out[i],error))return false;
  return true;
}
} // namespace

bool readUMotionProject(std::string_view source,UMotionProject &out,std::string &error) {
  UnityYamlDocument document;if(!readUnityYaml(source,document,error))return false;
  const UnityYamlNode *root=nullptr;
  for(const auto &object:document.objects)if(object.value.get("AnimatedObjectList")&&object.value.get("ClipList"))root=&object.value;
  if(!root)return fail(error,"Arquivo não é um projeto UMotion (AnimatedObjectList/ClipList ausentes)");
  UMotionProject project;project.name=root->get("m_Name")?root->get("m_Name")->scalar:std::string();
  if(const auto *settings=root->get("Settings")) {i64 rig=0;if(integer(*settings,"RigType",rig))project.rigType=static_cast<u32>(rig);}
  for(const auto &node:root->get("AnimatedObjectList")->elements) {
    UMotionObject o;o.path=text(node,"TransformPath");
    i64 n=0;if(integer(node,"Type",n))o.type=static_cast<u32>(n);
    bool human=false;if(boolean(node,"IsHumanBone",human)&&human&&integer(node,"HumanBodyBone",n))o.humanBone=static_cast<i32>(n);
    if(!channels(node,"LocalPositionChannelList",o.position,3,error)||!channels(node,"LocalRotationChannelList",o.rotation,4,error)||
       !channels(node,"LocalScaleChannelList",o.scale,3,error))return fail(error,"Objeto '"+o.path+"': "+error);
    if(const auto *modes=node.get("RotationModePerClip");modes&&modes->kind==UnityYamlNode::Kind::Sequence)
      for(const auto &mode:modes->elements) {
        const auto *v=inner(&mode);i64 m=0;if(!v||!v->integer(m)||m<0||m>2)return fail(error,"Modo de rotação UMotion desconhecido em '"+o.path+"'");
        o.rotationMode.push_back(static_cast<UMotionRotationMode>(m));
      }
    boolean(node,"IkEnable",o.ik);boolean(node,"IkLegacyMode",o.ikLegacy);
    o.ikTarget=address(node.get("IkTarget"));o.ikPole=address(node.get("IkPoleTarget"));
    if(integer(node,"IkChainLength",n))o.ikChainLength=static_cast<u32>(std::max<i64>(1,n));
    if(integer(node,"IkChainMask",n))o.ikChainMask=static_cast<i32>(n);
    if(integer(node,"IkReferenceMode",n))o.ikReferenceMode=static_cast<u32>(n);
    if(integer(node,"IkTargetRotationMode",n))o.ikTargetRotationMode=static_cast<u32>(n);
    number(node,"IkPlaneAngleOffset",o.ikPlaneAngle);
    if(!floatChannel(node.get("IkFkBlendChannel"),o.fkIkBlend,error)||!floatChannel(node.get("IkPoleRotationChannel"),o.poleRotation,error))return false;
    boolean(node,"ChildOfEnable",o.childOf);
    if(!referenceChannel(node.get("ChildOfParentChannel"),o.childOfParent,error))return false;
    if(const auto *pin=node.get("ChildOfIKPinning");pin&&pin->kind==UnityYamlNode::Kind::Map&&pin->get("ChannelPerClipList")) {
      if(!floatChannel(pin,o.childOfPinning,error))return false;
    } else {bool pinned=false;boolean(node,"ChildOfIKPinning",pinned);o.childOfPinning.reference=pinned?1.f:0.f;}
    if(const auto *list=node.get("CustomPropertyList");list&&list->kind==UnityYamlNode::Kind::Sequence)
      for(const auto &p:list->elements) {
        UMotionCustomProperty c;c.name=text(p,"Name");c.propertyPath=text(p,"PropertyPath");c.propertyType=text(p,"PropertyType");
        if(integer(p,"DrivenChannel",n))c.drivenChannel=static_cast<u32>(n);
        if(integer(p,"Mode",n))c.mode=static_cast<u32>(n);
        boolean(p,"ScalingEnabled",c.scaling);boolean(p,"ConstrainInput",c.constrainInput);
        number(p,"DriverMin",c.driverMin);number(p,"DriverMax",c.driverMax);number(p,"DrivenMin",c.drivenMin);number(p,"DrivenMax",c.drivenMax);
        if(const auto *driven=p.get("DrivenAnimatedObjectList");driven&&driven->kind==UnityYamlNode::Kind::Sequence)
          for(const auto &d:driven->elements)c.driven.push_back(address(&d));
        if(!floatChannel(p.get("CustomPropertyChannel"),c.channel,error))return false;
        o.customProperties.push_back(std::move(c));
      }
    project.objects.push_back(std::move(o));
  }
  for(const auto &node:root->get("ClipList")->elements) {
    UMotionClip clip;const auto *settings=node.get("Settings");const auto *playback=node.get("Playback");
    if(!settings||!playback)return fail(error,"Clipe UMotion sem Settings/Playback");
    clip.name=text(*settings,"Name");i64 n=0;
    if(integer(*settings,"Framerate",n))clip.framerate=static_cast<i32>(n);
    boolean(*settings,"Loop",clip.loop);
    if(const auto *rm=settings->get("RootMotion")) {boolean(*rm,"Translation",clip.rootMotionTranslation);boolean(*rm,"Rotation",clip.rootMotionRotation);clip.rootMotionObject=address(rm->get("AnimatedObject"));}
    if(integer(*playback,"StartFrame",n))clip.start=static_cast<i32>(n);
    if(integer(*playback,"EndFrame",n))clip.end=static_cast<i32>(n);
    number(*playback,"Speed",clip.speed);
    if(clip.framerate<=0||clip.framerate>1000||clip.end<clip.start)return fail(error,"Clipe UMotion '"+clip.name+"' com taxa ou intervalo inválidos");
    if(const auto *layers=node.get("LayerList");layers&&layers->kind==UnityYamlNode::Kind::Sequence)
      for(const auto &l:layers->elements) {
        UMotionLayer layer;layer.name=text(l,"Name");boolean(l,"BlendOverride",layer.overrideBlend);boolean(l,"Mute",layer.mute);number(l,"BlendWeight",layer.weight);
        clip.layers.push_back(std::move(layer));
      }
    if(const auto *events=node.get("EventList");events&&events->kind==UnityYamlNode::Kind::Sequence)
      for(const auto &e:events->elements) {
        UMotionEvent event;if(integer(e,"Frame",n))event.frame=static_cast<i32>(n);
        if(const auto *list=e.get("EventFunctionList");list&&list->kind==UnityYamlNode::Kind::Sequence)
          for(const auto &f:list->elements) {UMotionEventFunction fn;fn.name=text(f,"FuncName");if(integer(f,"ParamType",n))fn.parameterType=static_cast<u32>(n);fn.parameter=text(f,"ParamValue");event.functions.push_back(std::move(fn));}
        clip.events.push_back(std::move(event));
      }
    project.clips.push_back(std::move(clip));
  }
  if(project.clips.empty())return fail(error,"Projeto UMotion sem clipes");
  for(const auto &o:project.objects) {
    if(o.ikTarget>=static_cast<i32>(project.objects.size())||o.ikPole>=static_cast<i32>(project.objects.size()))return fail(error,"IK de '"+o.path+"' aponta para objeto inexistente");
  }
  out=std::move(project);error.clear();return true;
}

float sampleUMotionChannel(const std::vector<UMotionKey> &keys,double frame,float fallback) {
  if(keys.empty())return fallback;
  if(frame<=keys.front().frame)return keys.front().value;
  if(frame>=keys.back().frame)return keys.back().value;
  const auto upper=std::upper_bound(keys.begin(),keys.end(),frame,[](double f,const UMotionKey &k){return f<k.frame;});
  const auto &a=*(upper-1),&b=*upper;
  if(a.outgoing()==UMotionTangent::Constant||b.incoming()==UMotionTangent::Constant)return a.value;
  const double dt=double(b.frame)-a.frame,t=(frame-a.frame)/dt;
  const double m0=std::isfinite(a.outTangent)?a.outTangent:0.0,m1=std::isfinite(b.inTangent)?b.inTangent:0.0;
  if(!a.weightedOut()&&!b.weightedIn()) {
    const double t2=t*t,t3=t2*t;
    return static_cast<float>((2*t3-3*t2+1)*a.value+(t3-2*t2+t)*dt*m0+(-2*t3+3*t2)*b.value+(t3-t2)*dt*m1);
  }
  // Bezier ponderado (pesos em fracao do segmento, como Keyframe da Unity):
  // resolve o parametro pelo tempo e avalia o valor.
  const double w0=a.weightedOut()?a.outWeight:1.0/3,w1=b.weightedIn()?b.inWeight:1.0/3;
  const double x1=w0,x2=1-w1,y0=a.value,y1=a.value+m0*dt*w0,y2=b.value-m1*dt*w1,y3=b.value;
  double u=t;
  for(int i=0;i<20;++i) {
    const double v=1-u,x=3*v*v*u*x1+3*v*u*u*x2+u*u*u-t,dx=3*v*v*x1+6*v*u*(x2-x1)+3*u*u*(1-x2);
    if(std::fabs(x)<1e-9)break;
    if(std::fabs(dx)<1e-12)break;
    u=std::clamp(u-x/dx,0.0,1.0);
  }
  const double v=1-u;
  return static_cast<float>(v*v*v*y0+3*v*v*u*y1+3*v*u*u*y2+u*u*u*y3);
}
} // namespace ae::resources

// Câmera virtual e Cérebro de câmera (bloco G, F023).
//
// A Câmera virtual não desenha nada: é um objeto que calcula, a cada quadro de
// Play, uma pose e uma lente (posição: fixa/seguir/órbita; rotação: fixa/olhar
// para o alvo/rotação do alvo; desoclusão; tremor). O Cérebro mora no objeto
// com Câmera e escolhe a câmera virtual ativa de maior prioridade, misturando
// pose e lente durante a transição. O avaliador é runtime/scene_virtual_cameras.h.
//
// Referência: Cinemachine 3.1 (Unity 6000.0)
// https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineCamera.html
// https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineBrain.html
// https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineOrbitalFollow.html
// https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineDeoccluder.html
// https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineBasicMultiChannelPerlin.html
// Adaptações: os comportamentos de posição/rotação/ruído/desoclusão são
// propriedades de um só componente em vez de componentes irmãos; todas as
// câmeras virtuais ativas são avaliadas a cada quadro (Standby "Always").
#pragma once
#include "scene/components.h"
#include "scene/physics_field.h"

#include <array>
#include <cmath>
#include <limits>

namespace ae::scene {

enum class VirtualCameraPosition : u32 {Authored=0,Follow=1,Orbit=2};
enum class VirtualCameraRotation : u32 {Authored=0,LookAt=1,TargetRotation=2};
enum class VirtualCameraBinding : u32 {World=0,TargetYaw=1};
// Curvas de transição. Zero na câmera virtual significa "padrão do Cérebro".
enum class CameraBlendStyle : u32 {BrainDefault=0,Cut=1,EaseInOut=2,Linear=3,EaseIn=4,EaseOut=5,HardIn=6,HardOut=7};

inline float cameraBlendWeight(CameraBlendStyle style,float t) {
  t=std::fmin(1.f,std::fmax(0.f,t));
  switch(style) {
    case CameraBlendStyle::Cut: return 1;
    case CameraBlendStyle::Linear: return t;
    case CameraBlendStyle::EaseIn: return t*t*(2-t);          // começa parado, termina linear
    case CameraBlendStyle::EaseOut: return t*(1+t-t*t);       // começa linear, termina parado
    case CameraBlendStyle::HardIn: return t*t;
    case CameraBlendStyle::HardOut: return 1-(1-t)*(1-t);
    default: return t*t*(3-2*t);                              // suave nos dois extremos
  }
}
// Ruído de gradiente 1D (Perlin), contínuo, aproximadamente em [-1,1].
inline float cameraNoise(float x) {
  const auto gradient=[](i64 i) {
    u32 h=static_cast<u32>(i)*0x9E3779B1u;h^=h>>15;h*=0x85EBCA77u;h^=h>>13;
    return static_cast<float>(h&0xffffu)/32767.5f-1.f;
  };
  const double cell=std::floor(static_cast<double>(x));
  const float f=static_cast<float>(static_cast<double>(x)-cell);const auto i=static_cast<i64>(cell);
  const float u=f*f*f*(f*(f*6-15)+10);
  const float a=gradient(i)*f,b=gradient(i+1)*(f-1);
  return 2*(a+(b-a)*u);
}

class VirtualCamera final : public ComponentValue {
public:
  bool enabled=true;
  float priority=0;
  u64 trackingTarget=0,lookAtTarget=0;
  float verticalFov=60,orthographicHalfHeight=5,nearPlane=.1f,farPlane=2000,dutch=0;
  VirtualCameraPosition position=VirtualCameraPosition::Authored;
  VirtualCameraBinding binding=VirtualCameraBinding::World;
  float followOffset[3]{0,2,-5};
  float orbitRadius=5,orbitYaw=0,orbitPitch=20,orbitPitchMin=-10,orbitPitchMax=70;
  bool orbitInput=true;
  float orbitYawSensitivity=300,orbitPitchSensitivity=195;
  float positionDamping=.2f;
  VirtualCameraRotation rotation=VirtualCameraRotation::Authored;
  float aimOffset[3]{0,0,0};
  float rotationDamping=0;
  bool avoidObstacles=false;
  u32 collisionLayer=0;
  float cameraRadius=.2f,minimumDistance=.5f,collisionDamping=.3f;
  float noiseAmplitude=0,noisePositionAmplitude=0,noiseFrequency=1;
  CameraBlendStyle blendStyle=CameraBlendStyle::BrainDefault;
  float blendTime=1;

  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<VirtualCamera>(*this);}
  bool valid() const override {
    for(const auto &p:descriptor.numbers) {const float v=p.read(*this);if(!std::isfinite(v)||v<p.minimum||v>p.maximum) return false;}
    return trackingTarget<=std::numeric_limits<u32>::max() && lookAtTarget<=std::numeric_limits<u32>::max() &&
           priority==std::floor(priority) && farPlane>nearPlane && orbitPitchMin<=orbitPitchMax &&
           static_cast<u32>(position)<=2 && static_cast<u32>(rotation)<=2 && static_cast<u32>(binding)<=1 &&
           static_cast<u32>(blendStyle)<=7 && collisionLayer<=32;
  }
  void write(std::ostream &out) const override {
    out<<enabled<<' '<<trackingTarget<<' '<<lookAtTarget<<' '<<static_cast<u32>(position)<<' '<<static_cast<u32>(binding)<<' '
       <<static_cast<u32>(rotation)<<' '<<orbitInput<<' '<<avoidObstacles<<' '<<collisionLayer<<' '<<static_cast<u32>(blendStyle);
    for(const auto &p:descriptor.numbers) out<<' '<<p.read(*this);
  }
  bool read(std::istream &in,u32 version) override {
    u32 p=0,b=0,r=0,s=0;
    if(version!=1 || !(in>>enabled>>trackingTarget>>lookAtTarget>>p>>b>>r>>orbitInput>>avoidObstacles>>collisionLayer>>s)) return false;
    position=static_cast<VirtualCameraPosition>(p);binding=static_cast<VirtualCameraBinding>(b);
    rotation=static_cast<VirtualCameraRotation>(r);blendStyle=static_cast<CameraBlendStyle>(s);
    for(const auto &n:descriptor.numbers) if(!(in>>*n.write(*this))) return false;
    return valid();
  }
};
inline const VirtualCamera &virtualCamera(const ComponentValue &v) {return static_cast<const VirtualCamera &>(v);}
inline VirtualCamera &virtualCamera(ComponentValue &v) {return static_cast<VirtualCamera &>(v);}

class CameraBrain final : public ComponentValue {
public:
  bool enabled=true,ignoreTimeScale=false;
  CameraBlendStyle defaultBlend=CameraBlendStyle::EaseInOut;
  float defaultBlendTime=2;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<CameraBrain>(*this);}
  bool valid() const override {
    return std::isfinite(defaultBlendTime) && defaultBlendTime>=0 && defaultBlendTime<=60 &&
           defaultBlend!=CameraBlendStyle::BrainDefault && static_cast<u32>(defaultBlend)<=7;
  }
  void write(std::ostream &out) const override {out<<enabled<<' '<<ignoreTimeScale<<' '<<static_cast<u32>(defaultBlend)<<' '<<defaultBlendTime;}
  bool read(std::istream &in,u32 version) override {
    u32 style=0;
    if(version!=1 || !(in>>enabled>>ignoreTimeScale>>style>>defaultBlendTime)) return false;
    defaultBlend=static_cast<CameraBlendStyle>(style);return valid();
  }
};
inline const CameraBrain &cameraBrain(const ComponentValue &v) {return static_cast<const CameraBrain &>(v);}
inline CameraBrain &cameraBrain(ComponentValue &v) {return static_cast<CameraBrain &>(v);}

namespace virtual_camera_detail {
inline bool follows(const ComponentValue &v) {return virtualCamera(v).position==VirtualCameraPosition::Follow;}
inline bool orbits(const ComponentValue &v) {return virtualCamera(v).position==VirtualCameraPosition::Orbit;}
inline bool orbitInput(const ComponentValue &v) {return orbits(v)&&virtualCamera(v).orbitInput;}
inline bool tracks(const ComponentValue &v) {return virtualCamera(v).position!=VirtualCameraPosition::Authored;}
inline bool aims(const ComponentValue &v) {return virtualCamera(v).rotation==VirtualCameraRotation::LookAt;}
inline bool rotates(const ComponentValue &v) {return virtualCamera(v).rotation!=VirtualCameraRotation::Authored;}
inline bool collides(const ComponentValue &v) {return virtualCamera(v).avoidObstacles;}
inline bool shakes(const ComponentValue &v) {const auto &c=virtualCamera(v);return c.noiseAmplitude>0||c.noisePositionAmplitude>0;}
inline bool blends(const ComponentValue &v) {const auto s=virtualCamera(v).blendStyle;return s!=CameraBlendStyle::BrainDefault&&s!=CameraBlendStyle::Cut;}
inline bool brainBlends(const ComponentValue &v) {return cameraBrain(v).defaultBlend!=CameraBlendStyle::Cut;}
} // namespace virtual_camera_detail

#define AE_VCAM_NUMBER(LABEL,MIN,MAX,STEP,MEMBER,ID,GROUP,UNIT,HELP,VISIBLE,TWEEN) \
  ComponentNumber{LABEL,MIN,MAX,STEP,[](const ComponentValue &v)->const float &{return virtualCamera(v).MEMBER;}, \
   [](ComponentValue &v)->float *{return &virtualCamera(v).MEMBER;},ID,{GROUP,UNIT,HELP,VISIBLE},TWEEN}
inline constexpr std::array<ComponentNumber,28> virtualCameraNumbers{{
  AE_VCAM_NUMBER("Prioridade",-10000,10000,1,priority,"priority","Geral","","A maior entre as câmeras virtuais ativas fica ao vivo; no empate vence a ativada por último",nullptr,false),
  AE_VCAM_NUMBER("Campo vertical",1,170,1,verticalFov,"vertical_fov","Lente","°","Usado quando a Câmera do Cérebro é perspectiva",nullptr,true),
  AE_VCAM_NUMBER("Meia altura",.001f,100000,.1f,orthographicHalfHeight,"orthographic_half_height","Lente","m","Usado quando a Câmera do Cérebro é ortográfica",nullptr,true),
  AE_VCAM_NUMBER("Próximo",.001f,10000,.01f,nearPlane,"near_plane","Lente","m",nullptr,nullptr,false),
  AE_VCAM_NUMBER("Distante",.01f,1000000,10,farPlane,"far_plane","Lente","m",nullptr,nullptr,false),
  AE_VCAM_NUMBER("Inclinação holandesa",-180,180,1,dutch,"dutch","Lente","°","Giro em torno da direção de visão",nullptr,true),
  AE_VCAM_NUMBER("Deslocamento X",-1000,1000,.1f,followOffset[0],"offset_x","Posição","m",nullptr,virtual_camera_detail::follows,false),
  AE_VCAM_NUMBER("Deslocamento Y",-1000,1000,.1f,followOffset[1],"offset_y","Posição","m",nullptr,virtual_camera_detail::follows,false),
  AE_VCAM_NUMBER("Deslocamento Z",-1000,1000,.1f,followOffset[2],"offset_z","Posição","m",nullptr,virtual_camera_detail::follows,false),
  AE_VCAM_NUMBER("Raio da órbita",.01f,1000,.1f,orbitRadius,"orbit_radius","Posição","m","Distância ao alvo rastreado",virtual_camera_detail::orbits,true),
  AE_VCAM_NUMBER("Ângulo horizontal",-180,180,1,orbitYaw,"orbit_yaw","Posição","°","Giro em torno do eixo Y do mundo; zero fica atrás do alvo (−Z)",virtual_camera_detail::orbits,true),
  AE_VCAM_NUMBER("Ângulo vertical",-89,89,1,orbitPitch,"orbit_pitch","Posição","°","Positivo põe a câmera acima do alvo",virtual_camera_detail::orbits,true),
  AE_VCAM_NUMBER("Vertical mínimo",-89,89,1,orbitPitchMin,"orbit_pitch_min","Posição","°",nullptr,virtual_camera_detail::orbits,false),
  AE_VCAM_NUMBER("Vertical máximo",-89,89,1,orbitPitchMax,"orbit_pitch_max","Posição","°",nullptr,virtual_camera_detail::orbits,false),
  AE_VCAM_NUMBER("Sensibilidade horizontal",0,2000,5,orbitYawSensitivity,"orbit_yaw_sensitivity","Posição","°","Graus por largura de tela arrastada",virtual_camera_detail::orbitInput,false),
  AE_VCAM_NUMBER("Sensibilidade vertical",0,2000,5,orbitPitchSensitivity,"orbit_pitch_sensitivity","Posição","°","Graus por altura de tela arrastada",virtual_camera_detail::orbitInput,false),
  AE_VCAM_NUMBER("Amortecimento da posição",0,10,.05f,positionDamping,"position_damping","Posição","s","Tempo para alcançar o alvo; zero acompanha sem atraso",virtual_camera_detail::tracks,false),
  AE_VCAM_NUMBER("Mira X",-1000,1000,.1f,aimOffset[0],"aim_offset_x","Rotação","m",nullptr,virtual_camera_detail::aims,false),
  AE_VCAM_NUMBER("Mira Y",-1000,1000,.1f,aimOffset[1],"aim_offset_y","Rotação","m",nullptr,virtual_camera_detail::aims,false),
  AE_VCAM_NUMBER("Mira Z",-1000,1000,.1f,aimOffset[2],"aim_offset_z","Rotação","m",nullptr,virtual_camera_detail::aims,false),
  AE_VCAM_NUMBER("Amortecimento da rotação",0,10,.05f,rotationDamping,"rotation_damping","Rotação","s",nullptr,virtual_camera_detail::rotates,false),
  AE_VCAM_NUMBER("Raio da câmera",0,10,.05f,cameraRadius,"camera_radius","Colisão","m","Distância mantida dos obstáculos; zero usa um raio de luz",virtual_camera_detail::collides,false),
  AE_VCAM_NUMBER("Distância mínima do alvo",0,100,.1f,minimumDistance,"minimum_distance","Colisão","m","Obstáculos mais perto do alvo que isto são ignorados",virtual_camera_detail::collides,false),
  AE_VCAM_NUMBER("Amortecimento ao liberar",0,10,.05f,collisionDamping,"collision_damping","Colisão","s","Tempo para voltar à distância livre; a aproximação é imediata",virtual_camera_detail::collides,false),
  AE_VCAM_NUMBER("Tremor da rotação",0,90,.1f,noiseAmplitude,"noise_amplitude","Tremor","°",nullptr,nullptr,true),
  AE_VCAM_NUMBER("Tremor da posição",0,10,.01f,noisePositionAmplitude,"noise_position_amplitude","Tremor","m",nullptr,nullptr,true),
  AE_VCAM_NUMBER("Frequência",0,50,.1f,noiseFrequency,"noise_frequency","Tremor","Hz",nullptr,virtual_camera_detail::shakes,true),
  AE_VCAM_NUMBER("Duração da entrada",0,60,.05f,blendTime,"blend_time","Transição","s",nullptr,virtual_camera_detail::blends,false),
}};
#undef AE_VCAM_NUMBER
inline constexpr std::array<ComponentBoolean,3> virtualCameraBooleans{{
  {"enabled","Ativa",[](const ComponentValue &v){return virtualCamera(v).enabled;},[](ComponentValue &v,bool b){virtualCamera(v).enabled=b;},
   {"Geral","","Desligada deixa de concorrer; a configuração é preservada"}},
  {"orbit_input","Controlar pela entrada de olhar",[](const ComponentValue &v){return virtualCamera(v).orbitInput;},[](ComponentValue &v,bool b){virtualCamera(v).orbitInput=b;},
   {"Posição","","A ação Olhar do mapa de entrada gira a órbita enquanto esta câmera está ao vivo",virtual_camera_detail::orbits}},
  {"avoid_obstacles","Evitar obstáculos",[](const ComponentValue &v){return virtualCamera(v).avoidObstacles;},[](ComponentValue &v,bool b){virtualCamera(v).avoidObstacles=b;},
   {"Colisão","","Aproxima a câmera do alvo quando um colisor bloqueia a linha de visão"}},
}};
inline constexpr std::array<ComponentEnumOption,3> virtualCameraPositionOptions{{{0,"Fixa (pose autorada)"},{1,"Seguir"},{2,"Órbita"}}};
inline constexpr std::array<ComponentEnumOption,3> virtualCameraRotationOptions{{{0,"Fixa (rotação autorada)"},{1,"Olhar para o alvo"},{2,"Rotação do alvo"}}};
inline constexpr std::array<ComponentEnumOption,2> virtualCameraBindingOptions{{{0,"Eixos do mundo"},{1,"Guinada do alvo"}}};
inline constexpr std::array<ComponentEnumOption,8> virtualCameraBlendOptions{{
  {0,"Padrão do Cérebro"},{1,"Corte"},{2,"Suave"},{3,"Linear"},{4,"Entrada suave"},{5,"Saída suave"},{6,"Entrada brusca"},{7,"Saída brusca"}}};
inline constexpr std::array<ComponentEnumOption,7> cameraBrainBlendOptions{{
  {1,"Corte"},{2,"Suave"},{3,"Linear"},{4,"Entrada suave"},{5,"Saída suave"},{6,"Entrada brusca"},{7,"Saída brusca"}}};
inline constexpr std::array<ComponentEnum,5> virtualCameraEnums{{
  {"position_mode","Posição",virtualCameraPositionOptions,[](const ComponentValue &v){return static_cast<u32>(virtualCamera(v).position);},
   [](ComponentValue &v,u32 x){virtualCamera(v).position=static_cast<VirtualCameraPosition>(x);},
   {"Posição","","Seguir e Órbita usam o alvo rastreado; sem alvo a câmera fica na pose autorada"}},
  {"binding","Referencial do deslocamento",virtualCameraBindingOptions,[](const ComponentValue &v){return static_cast<u32>(virtualCamera(v).binding);},
   [](ComponentValue &v,u32 x){virtualCamera(v).binding=static_cast<VirtualCameraBinding>(x);},
   {"Posição","","Guinada do alvo gira o deslocamento junto com o alvo (Lock To Target With World Up)",virtual_camera_detail::follows}},
  {"rotation_mode","Rotação",virtualCameraRotationOptions,[](const ComponentValue &v){return static_cast<u32>(virtualCamera(v).rotation);},
   [](ComponentValue &v,u32 x){virtualCamera(v).rotation=static_cast<VirtualCameraRotation>(x);},
   {"Rotação","","Olhar para o alvo usa o alvo de mira, ou o rastreado quando ele está vazio"}},
  {"collision_layer","Camada dos obstáculos",fieldLayers,[](const ComponentValue &v){return virtualCamera(v).collisionLayer;},
   [](ComponentValue &v,u32 x){virtualCamera(v).collisionLayer=x;},{"Colisão","","Todas ou só uma camada física do projeto",virtual_camera_detail::collides}},
  {"blend_style","Entrada",virtualCameraBlendOptions,[](const ComponentValue &v){return static_cast<u32>(virtualCamera(v).blendStyle);},
   [](ComponentValue &v,u32 x){virtualCamera(v).blendStyle=static_cast<CameraBlendStyle>(x);},
   {"Transição","","Curva usada quando esta câmera entra ao vivo"}},
}};
inline constexpr std::array<ComponentObjectReference,2> virtualCameraReferences{{
  {"tracking_target","Alvo rastreado","",ObjectReferenceScope::OtherNonDescendant,"Nenhum",
   [](const ComponentValue &v){return virtualCamera(v).trackingTarget;},[](ComponentValue &v,u64 x){virtualCamera(v).trackingTarget=x;},
   {"Geral","","Objeto que Seguir e Órbita acompanham"}},
  {"look_at_target","Alvo de mira","",ObjectReferenceScope::OtherNonDescendant,"O rastreado",
   [](const ComponentValue &v){return virtualCamera(v).lookAtTarget;},[](ComponentValue &v,u64 x){virtualCamera(v).lookAtTarget=x;},
   {"Geral","","Ponto que Olhar para o alvo e a desoclusão usam"}},
}};
inline constexpr std::array<ComponentTriple,2> virtualCameraTriples{{
  {"offset","Deslocamento",{"offset_x","offset_y","offset_z"}},
  {"aim_offset","Ajuste da mira",{"aim_offset_x","aim_offset_y","aim_offset_z"}},
}};
inline constexpr std::array<ComponentMethod,3> virtualCameraMethods{{
  {"prioritize","Priorizar","Vence o empate com outras câmeras de mesma prioridade, como se tivesse sido ativada agora"},
  {"snap","Encaixar","Descarta o amortecimento no próximo quadro (depois de teletransportar o alvo)"},
  {"is_live","Ao vivo","Verdadeiro quando algum Cérebro está mostrando esta câmera",{},ComponentValueKind::Boolean},
}};
inline constexpr std::array<ComponentParameter,1> virtualCameraOtherPayload{{{"other","Outra câmera",ComponentValueKind::Object}}};
inline constexpr std::array<ComponentEvent,2> virtualCameraEvents{{
  {"activated","Entrou ao vivo","Emitido quando um Cérebro passa a mostrar esta câmera; carrega a câmera anterior",virtualCameraOtherPayload},
  {"deactivated","Saiu do ar","Emitido quando outra câmera assume o Cérebro; carrega a nova câmera",virtualCameraOtherPayload},
}};
inline const ComponentType VirtualCamera::descriptor{
  "astra.camera.virtual",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<VirtualCamera>();},
  virtualCameraNumbers,virtualCameraBooleans,virtualCameraEnums,nullptr,false,virtualCameraReferences,virtualCameraTriples,
  {},{},{},{},virtualCameraMethods,virtualCameraEvents};

inline constexpr std::array<ComponentNumber,1> cameraBrainNumbers{{
  {"Duração padrão",0,60,.05f,[](const ComponentValue &v)->const float &{return cameraBrain(v).defaultBlendTime;},
   [](ComponentValue &v)->float *{return &cameraBrain(v).defaultBlendTime;},"default_blend_time",
   {"Transição","s","Usada quando a câmera que entra não define a própria transição",virtual_camera_detail::brainBlends}},
}};
inline constexpr std::array<ComponentBoolean,2> cameraBrainBooleans{{
  {"enabled","Ativo",[](const ComponentValue &v){return cameraBrain(v).enabled;},[](ComponentValue &v,bool b){cameraBrain(v).enabled=b;},
   {"Execução","","Desligado devolve a câmera à autoria; a pose do último quadro permanece"}},
  {"ignore_time_scale","Ignorar escala de tempo",[](const ComponentValue &v){return cameraBrain(v).ignoreTimeScale;},[](ComponentValue &v,bool b){cameraBrain(v).ignoreTimeScale=b;},
   {"Execução","","Câmeras e transições seguem o tempo real mesmo em câmera lenta ou pausa por escala"}},
}};
inline constexpr std::array<ComponentEnum,1> cameraBrainEnums{{
  {"default_blend","Transição padrão",cameraBrainBlendOptions,[](const ComponentValue &v){return static_cast<u32>(cameraBrain(v).defaultBlend);},
   [](ComponentValue &v,u32 x){cameraBrain(v).defaultBlend=static_cast<CameraBlendStyle>(x);},{"Transição"}},
}};
inline constexpr std::array<ComponentMethod,2> cameraBrainMethods{{
  {"live_camera","Câmera ao vivo","Câmera virtual que o Cérebro está mostrando; vazio sem nenhuma",{},ComponentValueKind::Object},
  {"blending","Em transição","Verdadeiro enquanto mistura duas câmeras",{},ComponentValueKind::Boolean},
}};
inline constexpr std::array<ComponentParameter,2> cameraBrainActivatedPayload{{
  {"incoming","Câmera que entrou",ComponentValueKind::Object},{"outgoing","Câmera que saiu",ComponentValueKind::Object}}};
inline constexpr std::array<ComponentParameter,1> cameraBrainCameraPayload{{{"camera","Câmera",ComponentValueKind::Object}}};
inline constexpr std::array<ComponentEvent,3> cameraBrainEvents{{
  {"camera_activated","Câmera ativada","Emitido quando uma câmera virtual assume, já no primeiro quadro da transição",cameraBrainActivatedPayload},
  {"camera_cut","Corte de câmera","Emitido quando a troca acontece sem transição",cameraBrainCameraPayload},
  {"blend_finished","Transição concluída","Emitido quando a mistura termina e só a câmera nova aparece",cameraBrainCameraPayload},
}};
inline const ComponentType CameraBrain::descriptor{
  "astra.camera.brain",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<CameraBrain>();},
  cameraBrainNumbers,cameraBrainBooleans,cameraBrainEnums,nullptr,false,{},{},{},{},{},{},cameraBrainMethods,cameraBrainEvents};

} // namespace ae::scene

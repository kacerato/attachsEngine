#pragma once
// Pacote C da familia IK (N04): efeitos procedurais, interacao e corpo inteiro.
// Autoria persistida aqui; o efeito e o estado (impulsos, molas, interacoes em
// curso) vivem em runtime/scene_ik_impl.h, no mesmo passo do IK em Play.
//
// Referencias estudadas (nao copiadas): Unity Animation Rigging 1.2
// (Damped Transform, Multi-Aim, Override Transform) em
// docs.unity3d.com/Packages/com.unity.animation.rigging@1.2 e Godot 4.5
// SkeletonModifier3D/SpringBoneSimulator3D em docs.godotengine.org/en/4.5;
// o conjunto funcional segue a lista mestre (FinalIK HitReaction, Recoil,
// Inertia, Amplifier, BodyTilt, HandPoser, PenetrationAvoidance,
// InteractionSystem e FullBodyBipedIK), refeito no jeito Astra.
#include "scene/ik_constraints.h"

namespace ae::scene {
#define AE_FX_NUM(T,id,label,member,lo,hi,step,group,unit) ComponentNumber{label,lo,hi,step,[](const ComponentValue&v)->const float&{return static_cast<const T&>(v).member;},[](ComponentValue&v){return &static_cast<T&>(v).member;},id,{group,unit}}
#define AE_FX_BOOL(T,id,label,member,group) ComponentBoolean{id,label,[](const ComponentValue&v){return static_cast<const T&>(v).member;},[](ComponentValue&v,bool n){static_cast<T&>(v).member=n;},{group}}
#define AE_FX_ENUM(T,id,label,options,member,group) ComponentEnum{id,label,options,[](const ComponentValue &v){return static_cast<const T&>(v).member;},[](ComponentValue &v,u32 n){static_cast<T&>(v).member=n;},{group}}
#define AE_FX_REF(T,id,label,member,scope,none,group) ComponentObjectReference{id,label,"",ObjectReferenceScope::scope,none,[](const ComponentValue &v){return static_cast<const T&>(v).member;},[](ComponentValue &v,u64 n){static_cast<T&>(v).member=n;},{group}}
#define AE_FX_VALUE(T) \
  static const ComponentType descriptor; \
  const ComponentType &type() const override {return descriptor;} \
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<T>(*this);}
#define AE_FX_MAKE(T) []()->std::unique_ptr<ComponentValue>{return std::make_unique<T>();}
inline bool ikReference(u64 v) {return v<=std::numeric_limits<u32>::max();}

// Reacao a impacto: o osso atingido e N ossos acima giram para longe do golpe
// e voltam no tempo de recuperacao. Golpe por metodo `hit` ou pela colisao
// solida deste objeto (normal do contato).
class HitReaction final : public ComponentValue {
public:
  bool enabled=true,reactToCollisions=true;float weight=1,strength=25,duration=.4f,chainLength=3;
  AE_FX_VALUE(HitReaction)
  bool valid() const override {return ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<enabled<<' '<<reactToCollisions<<' '<<weight<<' '<<strength<<' '<<duration<<' '<<chainLength;}
  bool read(std::istream &in,u32 version) override {return version==1&&(in>>enabled>>reactToCollisions>>weight>>strength>>duration>>chainLength)&&valid();}
};
// Recuo: `fire` levanta a cadeia (arma, mao, braco) no eixo frontal e recua a
// base; volta ao pose do quadro no tempo de recuperacao. Disparos se somam.
class Recoil final : public ComponentValue {
public:
  bool enabled=true;float weight=1,kickAngle=8,kickBack=.05f,duration=.25f,chainLength=2;u32 axis=2;
  AE_FX_VALUE(Recoil)
  bool valid() const override {return axis<=5&&ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<enabled<<' '<<weight<<' '<<kickAngle<<' '<<kickBack<<' '<<duration<<' '<<chainLength<<' '<<axis;}
  bool read(std::istream &in,u32 version) override {return version==1&&(in>>enabled>>weight>>kickAngle>>kickBack>>duration>>chainLength>>axis)&&valid();}
};
// Inercia: a ponta do osso (primeiro filho) segue a pose por uma mola; o osso
// aponta para a ponta atrasada. Cauda, orelha, antena, cabo.
class Inertia final : public ComponentValue {
public:
  bool enabled=true;float weight=1,stiffness=40,damping=.35f,maxAngle=35;
  AE_FX_VALUE(Inertia)
  bool valid() const override {return ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<enabled<<' '<<weight<<' '<<stiffness<<' '<<damping<<' '<<maxAngle;}
  bool read(std::istream &in,u32 version) override {return version==1&&(in>>enabled>>weight>>stiffness>>damping>>maxAngle)&&valid();}
};
// Amplificador: exagera a rotacao animada do osso em relacao a pose de
// repouso (pose do inicio do Play).
class Amplifier final : public ComponentValue {
public:
  bool enabled=true;float weight=1,amount=.5f;
  AE_FX_VALUE(Amplifier)
  bool valid() const override {return ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<enabled<<' '<<weight<<' '<<amount;}
  bool read(std::istream &in,u32 version) override {return version==1&&(in>>enabled>>weight>>amount)&&valid();}
};
// Inclinacao do corpo: inclina este osso para dentro da curva conforme a
// velocidade de giro do personagem (objeto de topo da hierarquia).
class BodyTilt final : public ComponentValue {
public:
  bool enabled=true;float weight=1,tiltPerTurn=.2f,maxAngle=20,smoothing=.15f;u32 axis=2;
  AE_FX_VALUE(BodyTilt)
  bool valid() const override {return axis<=5&&ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<enabled<<' '<<weight<<' '<<tiltPerTurn<<' '<<maxAngle<<' '<<smoothing<<' '<<axis;}
  bool read(std::istream &in,u32 version) override {return version==1&&(in>>enabled>>weight>>tiltPerTurn>>maxAngle>>smoothing>>axis)&&valid();}
};
// Poser: copia as rotacoes locais da hierarquia de referencia para os
// descendentes deste objeto, casando filhos pela ordem. Mao, garra, pinca.
class HandPoser final : public ComponentValue {
public:
  u64 reference=0;bool enabled=true,includeSelf=false;float weight=1;
  AE_FX_VALUE(HandPoser)
  bool valid() const override {return ikReference(reference)&&ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<reference<<' '<<enabled<<' '<<includeSelf<<' '<<weight;}
  bool read(std::istream &in,u32 version) override {return version==1&&(in>>reference>>enabled>>includeSelf>>weight)&&valid();}
};
// Evitar penetracao: na ponta com IK. Um raio da raiz do membro ate o alvo; se
// houver colisor no caminho, o alvo para na superficie, afastado pelo raio.
class PenetrationAvoidance final : public ComponentValue {
public:
  bool enabled=true;float radius=.05f;
  AE_FX_VALUE(PenetrationAvoidance)
  bool valid() const override {return ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<enabled<<' '<<radius;}
  bool read(std::istream &in,u32 version) override {return version==1&&(in>>enabled>>radius)&&valid();}
};
// Objeto interagivel: onde a mao vai (ponto de pegada), pose da mao, tempos e
// se o objeto passa para a mao ao ser alcancado.
class InteractionObject final : public ComponentValue {
public:
  u64 grip=0,handPose=0;bool pickUp=false;float reachTime=.5f,releaseTime=.35f;
  AE_FX_VALUE(InteractionObject)
  bool valid() const override {return ikReference(grip)&&ikReference(handPose)&&ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<grip<<' '<<handPose<<' '<<pickUp<<' '<<reachTime<<' '<<releaseTime;}
  bool read(std::istream &in,u32 version) override {return version==1&&(in>>grip>>handPose>>pickUp>>reachTime>>releaseTime)&&valid();}
};
// Sistema de interacao: no personagem. `start(mao, objeto)` leva a mao ate o
// objeto com IK de dois ossos; `stop(mao)` solta (e larga o objeto pego).
class InteractionSystem final : public ComponentValue {
public:
  bool enabled=true;float speed=1;
  AE_FX_VALUE(InteractionSystem)
  bool valid() const override {return ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<enabled<<' '<<speed;}
  bool read(std::istream &in,u32 version) override {return version==1&&(in>>enabled>>speed)&&valid();}
};
// Corpo inteiro: ate quatro pontas com alvo; quando um alvo esta alem do
// alcance do membro, a pelve e puxada na direcao dele (limitada) e os membros
// sao resolvidos depois. Bipede, quadrupede ou mecanismo com pelve.
class FullBodyIk final : public ComponentValue {
public:
  u64 pelvis=0,tips[4]{},targets[4]{};bool enabled=true;float weight=1,bodyPull=.6f,maxBodyOffset=.5f;
  AE_FX_VALUE(FullBodyIk)
  bool valid() const override {
    if(!ikReference(pelvis))return false;
    for(u32 i=0;i<4;++i)if(!ikReference(tips[i])||!ikReference(targets[i]))return false;
    return ikNumbersValid(*this);
  }
  void write(std::ostream &out) const override {
    out<<pelvis;for(u32 i=0;i<4;++i)out<<' '<<tips[i]<<' '<<targets[i];
    out<<' '<<enabled<<' '<<weight<<' '<<bodyPull<<' '<<maxBodyOffset;
  }
  bool read(std::istream &in,u32 version) override {
    if(version!=1||!(in>>pelvis))return false;
    for(u32 i=0;i<4;++i)if(!(in>>tips[i]>>targets[i]))return false;
    return (in>>enabled>>weight>>bodyPull>>maxBodyOffset)&&valid();
  }
};

inline constexpr std::array<ComponentNumber,4> hitReactionNumbers{{AE_FX_NUM(HitReaction,"weight","Peso",weight,0,1,.01f,"Influência",""),
  AE_FX_NUM(HitReaction,"strength","Força do golpe",strength,0,90,1,"Golpe","graus"),AE_FX_NUM(HitReaction,"duration","Recuperação",duration,.05f,5,.01f,"Golpe","s"),
  AE_FX_NUM(HitReaction,"chain_length","Ossos que reagem",chainLength,1,8,1,"Golpe","ossos")}};
inline constexpr std::array<ComponentBoolean,2> hitReactionBooleans{{AE_FX_BOOL(HitReaction,"enabled","Ativo",enabled,"Influência"),
  AE_FX_BOOL(HitReaction,"react_to_collisions","Reagir a colisões",reactToCollisions,"Golpe")}};
inline constexpr std::array<ComponentParameter,2> hitReactionHitParameters{{{"direction","Direção do golpe",ComponentValueKind::Vector3},{"force","Força",ComponentValueKind::Number}}};
inline constexpr std::array<ComponentMethod,1> hitReactionMethods{{{"hit","Golpear","Gira a cadeia para longe da direção; força 1 usa a força autorada",hitReactionHitParameters}}};
inline const ComponentType HitReaction::descriptor{"astra.ik.hit_reaction",1,AE_FX_MAKE(HitReaction),hitReactionNumbers,hitReactionBooleans,{},nullptr,false,{},{},{},{},{},{},hitReactionMethods};

inline constexpr std::array<ComponentNumber,5> recoilNumbers{{AE_FX_NUM(Recoil,"weight","Peso",weight,0,1,.01f,"Influência",""),
  AE_FX_NUM(Recoil,"kick_angle","Levantar",kickAngle,0,60,.5f,"Recuo","graus"),AE_FX_NUM(Recoil,"kick_back","Recuar",kickBack,0,1,.005f,"Recuo","m"),
  AE_FX_NUM(Recoil,"duration","Recuperação",duration,.02f,3,.01f,"Recuo","s"),AE_FX_NUM(Recoil,"chain_length","Ossos no recuo",chainLength,1,8,1,"Cadeia","ossos")}};
inline constexpr std::array<ComponentBoolean,1> recoilBooleans{{AE_FX_BOOL(Recoil,"enabled","Ativo",enabled,"Influência")}};
inline constexpr std::array<ComponentEnum,1> recoilEnums{{AE_FX_ENUM(Recoil,"axis","Eixo do cano",ikAxisOptions,axis,"Cadeia")}};
inline constexpr std::array<ComponentParameter,1> recoilFireParameters{{{"magnitude","Intensidade",ComponentValueKind::Number}}};
inline constexpr std::array<ComponentMethod,1> recoilMethods{{{"fire","Disparar","Aplica um recuo; intensidade 1 usa os valores autorados",recoilFireParameters}}};
inline const ComponentType Recoil::descriptor{"astra.ik.recoil",1,AE_FX_MAKE(Recoil),recoilNumbers,recoilBooleans,recoilEnums,nullptr,false,{},{},{},{},{},{},recoilMethods};

inline constexpr std::array<ComponentNumber,4> inertiaNumbers{{AE_FX_NUM(Inertia,"weight","Peso",weight,0,1,.01f,"Influência",""),
  AE_FX_NUM(Inertia,"stiffness","Rigidez",stiffness,1,400,1,"Mola",""),AE_FX_NUM(Inertia,"damping","Amortecimento",damping,0,1,.01f,"Mola",""),
  AE_FX_NUM(Inertia,"max_angle","Desvio máximo",maxAngle,0,120,1,"Mola","graus")}};
inline constexpr std::array<ComponentBoolean,1> inertiaBooleans{{AE_FX_BOOL(Inertia,"enabled","Ativo",enabled,"Influência")}};
inline const ComponentType Inertia::descriptor{"astra.ik.inertia",1,AE_FX_MAKE(Inertia),inertiaNumbers,inertiaBooleans};

inline constexpr std::array<ComponentNumber,2> amplifierNumbers{{AE_FX_NUM(Amplifier,"weight","Peso",weight,0,1,.01f,"Influência",""),
  AE_FX_NUM(Amplifier,"amount","Exagero",amount,0,3,.01f,"Amplificar","")}};
inline constexpr std::array<ComponentBoolean,1> amplifierBooleans{{AE_FX_BOOL(Amplifier,"enabled","Ativo",enabled,"Influência")}};
inline const ComponentType Amplifier::descriptor{"astra.ik.amplifier",1,AE_FX_MAKE(Amplifier),amplifierNumbers,amplifierBooleans};

inline constexpr std::array<ComponentNumber,4> bodyTiltNumbers{{AE_FX_NUM(BodyTilt,"weight","Peso",weight,0,1,.01f,"Influência",""),
  AE_FX_NUM(BodyTilt,"tilt_per_turn","Inclinação por giro",tiltPerTurn,0,2,.01f,"Inclinação","graus por grau/s"),
  AE_FX_NUM(BodyTilt,"max_angle","Inclinação máxima",maxAngle,0,60,1,"Inclinação","graus"),AE_FX_NUM(BodyTilt,"smoothing","Suavização",smoothing,0,2,.01f,"Inclinação","s")}};
inline constexpr std::array<ComponentBoolean,1> bodyTiltBooleans{{AE_FX_BOOL(BodyTilt,"enabled","Ativo",enabled,"Influência")}};
inline constexpr std::array<ComponentEnum,1> bodyTiltEnums{{AE_FX_ENUM(BodyTilt,"axis","Eixo frontal do personagem",ikAxisOptions,axis,"Inclinação")}};
inline const ComponentType BodyTilt::descriptor{"astra.ik.body_tilt",1,AE_FX_MAKE(BodyTilt),bodyTiltNumbers,bodyTiltBooleans,bodyTiltEnums};

inline constexpr std::array<ComponentNumber,1> handPoserNumbers{{AE_FX_NUM(HandPoser,"weight","Peso",weight,0,1,.01f,"Influência","")}};
inline constexpr std::array<ComponentBoolean,2> handPoserBooleans{{AE_FX_BOOL(HandPoser,"enabled","Ativo",enabled,"Influência"),
  AE_FX_BOOL(HandPoser,"include_self","Incluir este osso",includeSelf,"Pose")}};
inline constexpr std::array<ComponentObjectReference,1> handPoserReferences{{AE_FX_REF(HandPoser,"reference","Pose de referência",reference,OtherNonDescendant,"Escolher pose","Pose")}};
inline const ComponentType HandPoser::descriptor{"astra.ik.hand_poser",1,AE_FX_MAKE(HandPoser),handPoserNumbers,handPoserBooleans,{},nullptr,false,handPoserReferences};

inline constexpr std::array<ComponentNumber,1> penetrationNumbers{{AE_FX_NUM(PenetrationAvoidance,"radius","Folga da superfície",radius,0,1,.005f,"Colisão","m")}};
inline constexpr std::array<ComponentBoolean,1> penetrationBooleans{{AE_FX_BOOL(PenetrationAvoidance,"enabled","Ativo",enabled,"Colisão")}};
inline const ComponentType PenetrationAvoidance::descriptor{"astra.ik.penetration_avoidance",1,AE_FX_MAKE(PenetrationAvoidance),penetrationNumbers,penetrationBooleans};

inline constexpr std::array<ComponentNumber,2> interactionObjectNumbers{{AE_FX_NUM(InteractionObject,"reach_time","Tempo para alcançar",reachTime,.05f,5,.01f,"Tempo","s"),
  AE_FX_NUM(InteractionObject,"release_time","Tempo para soltar",releaseTime,.05f,5,.01f,"Tempo","s")}};
inline constexpr std::array<ComponentBoolean,1> interactionObjectBooleans{{AE_FX_BOOL(InteractionObject,"pick_up","Pegar ao alcançar",pickUp,"Pegada")}};
inline constexpr std::array<ComponentObjectReference,2> interactionObjectReferences{{
  AE_FX_REF(InteractionObject,"grip","Ponto de pegada",grip,Descendant,"O próprio objeto","Pegada"),
  AE_FX_REF(InteractionObject,"hand_pose","Pose da mão",handPose,Descendant,"Sem pose","Pegada")}};
inline const ComponentType InteractionObject::descriptor{"astra.ik.interaction_object",1,AE_FX_MAKE(InteractionObject),interactionObjectNumbers,interactionObjectBooleans,{},nullptr,false,interactionObjectReferences};

inline constexpr std::array<ComponentNumber,1> interactionSystemNumbers{{AE_FX_NUM(InteractionSystem,"speed","Velocidade",speed,.1f,5,.05f,"Interação","")}};
inline constexpr std::array<ComponentBoolean,1> interactionSystemBooleans{{AE_FX_BOOL(InteractionSystem,"enabled","Ativo",enabled,"Interação")}};
inline constexpr std::array<ComponentParameter,2> interactionStartParameters{{{"effector","Mão",ComponentValueKind::Object},{"object","Objeto",ComponentValueKind::Object}}};
inline constexpr std::array<ComponentParameter,1> interactionStopParameters{{{"effector","Mão",ComponentValueKind::Object}}};
inline constexpr std::array<ComponentMethod,2> interactionSystemMethods{{
  {"start","Interagir","Leva a mão até o Objeto interagível; falso se a mão ou o objeto não servem",interactionStartParameters,ComponentValueKind::Boolean},
  {"stop","Soltar","Devolve a mão à animação e larga o objeto pego",interactionStopParameters}}};
inline constexpr std::array<ComponentParameter,2> interactionEventPayload{{{"effector","Mão",ComponentValueKind::Object},{"object","Objeto",ComponentValueKind::Object}}};
inline constexpr std::array<ComponentEvent,2> interactionSystemEvents{{
  {"reached","Alcançou","A mão chegou ao ponto de pegada",interactionEventPayload},
  {"released","Soltou","A mão voltou à animação",interactionEventPayload}}};
inline const ComponentType InteractionSystem::descriptor{"astra.ik.interaction_system",1,AE_FX_MAKE(InteractionSystem),interactionSystemNumbers,interactionSystemBooleans,{},nullptr,false,{},{},{},{},{},{},interactionSystemMethods,interactionSystemEvents};

inline constexpr std::array<ComponentNumber,3> fullBodyNumbers{{AE_FX_NUM(FullBodyIk,"weight","Peso",weight,0,1,.01f,"Influência",""),
  AE_FX_NUM(FullBodyIk,"body_pull","Puxar o corpo",bodyPull,0,1,.01f,"Corpo",""),AE_FX_NUM(FullBodyIk,"max_body_offset","Deslocamento máximo da pelve",maxBodyOffset,0,3,.01f,"Corpo","m")}};
inline constexpr std::array<ComponentBoolean,1> fullBodyBooleans{{AE_FX_BOOL(FullBodyIk,"enabled","Ativo",enabled,"Influência")}};
inline constexpr std::array<ComponentObjectReference,9> fullBodyReferences{{
  AE_FX_REF(FullBodyIk,"pelvis","Pelve",pelvis,Descendant,"Escolher pelve","Corpo"),
  AE_FX_REF(FullBodyIk,"tip_1","Ponta 1 (mão esq.)",tips[0],Descendant,"Sem ponta","Ponta 1"),AE_FX_REF(FullBodyIk,"target_1","Alvo 1",targets[0],OtherNonDescendant,"Sem alvo","Ponta 1"),
  AE_FX_REF(FullBodyIk,"tip_2","Ponta 2 (mão dir.)",tips[1],Descendant,"Sem ponta","Ponta 2"),AE_FX_REF(FullBodyIk,"target_2","Alvo 2",targets[1],OtherNonDescendant,"Sem alvo","Ponta 2"),
  AE_FX_REF(FullBodyIk,"tip_3","Ponta 3 (pé esq.)",tips[2],Descendant,"Sem ponta","Ponta 3"),AE_FX_REF(FullBodyIk,"target_3","Alvo 3",targets[2],OtherNonDescendant,"Sem alvo","Ponta 3"),
  AE_FX_REF(FullBodyIk,"tip_4","Ponta 4 (pé dir.)",tips[3],Descendant,"Sem ponta","Ponta 4"),AE_FX_REF(FullBodyIk,"target_4","Alvo 4",targets[3],OtherNonDescendant,"Sem alvo","Ponta 4")}};
inline const ComponentType FullBodyIk::descriptor{"astra.ik.full_body",1,AE_FX_MAKE(FullBodyIk),fullBodyNumbers,fullBodyBooleans,{},nullptr,false,fullBodyReferences};
#undef AE_FX_NUM
#undef AE_FX_BOOL
#undef AE_FX_ENUM
#undef AE_FX_REF
#undef AE_FX_VALUE
#undef AE_FX_MAKE
} // namespace ae::scene

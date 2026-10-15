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
#define AE_FX_NUM(T,id,label,member,lo,hi,step,group,unit) ComponentNumber{label,lo,hi,step,[](const ComponentValue&v)->const float&{return static_cast<const T&>(v).member;},[](ComponentValue&v){return &static_cast<T&>(v).member;},id,{group,unit},true}
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
  bool enabled=true,reactToCollisions=true;float weight=1,strength=25,duration=.4f,chainLength=3;u32 axis=2;
  AE_FX_VALUE(HitReaction)
  bool valid() const override {return axis<=5&&ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<enabled<<' '<<reactToCollisions<<' '<<weight<<' '<<strength<<' '<<duration<<' '<<chainLength<<' '<<axis;}
  bool read(std::istream &in,u32 version) override {
    if(version<1||version>2||!(in>>enabled>>reactToCollisions>>weight>>strength>>duration>>chainLength))return false;
    axis=2;return (version==1||static_cast<bool>(in>>axis))&&valid();
  }
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
  // Duas maos (Final IK 2.2, PickUp2Handed): a segunda mao vai a sua pegada;
  // com Ponto de carregar, o objeto sobe ate ele e as maos ficam presas a ele.
  u64 grip=0,handPose=0,secondGrip=0,holdPoint=0;bool pickUp=false;float reachTime=.5f,releaseTime=.35f,liftTime=.4f;
  AE_FX_VALUE(InteractionObject)
  bool valid() const override {return ikReference(grip)&&ikReference(handPose)&&ikReference(secondGrip)&&ikReference(holdPoint)&&ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<grip<<' '<<handPose<<' '<<pickUp<<' '<<reachTime<<' '<<releaseTime<<' '<<secondGrip<<' '<<holdPoint<<' '<<liftTime;}
  bool read(std::istream &in,u32 version) override {
    if(version<1||version>2||!(in>>grip>>handPose>>pickUp>>reachTime>>releaseTime))return false;
    secondGrip=holdPoint=0;liftTime=.4f;
    if(version>=2&&!(in>>secondGrip>>holdPoint>>liftTime))return false;
    return valid();
  }
};
// Sistema de interacao: no personagem. `start(mao, objeto)` leva a mao ate o
// objeto com IK de dois ossos; `stop(mao)` solta (e larga o objeto pego).
class InteractionSystem final : public ComponentValue {
public:
  // Inicio sem script: com mao e objeto escolhidos, a interacao comeca no Play.
  // Corpo: com Pelve, o corpo agacha quando a pegada fica alem do braco; com
  // Peito, o tronco se inclina para ela. Segunda mao ao iniciar: duas maos.
  // Olhar: com um objeto que tem Look At (a cabeca), ele olha a pegada
  // enquanto a mao vai ate ela e volta ao soltar (Final IK InteractionLookAt).
  u64 autoEffector=0,autoObject=0,autoSecond=0,pelvis=0,chest=0,lookAt=0;bool enabled=true;float speed=1,limbBones=2,crouch=.5f,lean=.5f,lookSpeed=2,lookTurn=6;
  AE_FX_VALUE(InteractionSystem)
  bool valid() const override {return ikReference(autoEffector)&&ikReference(autoObject)&&ikReference(autoSecond)&&ikReference(pelvis)&&ikReference(chest)&&ikReference(lookAt)&&ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<enabled<<' '<<speed<<' '<<limbBones<<' '<<autoEffector<<' '<<autoObject<<' '<<autoSecond<<' '<<pelvis<<' '<<chest<<' '<<crouch<<' '<<lean<<' '<<lookAt<<' '<<lookSpeed<<' '<<lookTurn;}
  bool read(std::istream &in,u32 version) override {
    if(version<1||version>5||!(in>>enabled>>speed))return false;
    limbBones=2;autoEffector=autoObject=autoSecond=pelvis=chest=lookAt=0;crouch=.5f;lean=.5f;lookSpeed=2;lookTurn=6;
    if(version>=2&&!(in>>limbBones))return false;
    if(version>=3&&!(in>>autoEffector>>autoObject))return false;
    if(version>=4&&!(in>>autoSecond>>pelvis>>chest>>crouch>>lean))return false;
    if(version>=5&&!(in>>lookAt>>lookSpeed>>lookTurn))return false;
    return valid();
  }
};
// Pivo de pegada (Final IK InteractionTarget, refeito): este objeto gira sobre
// si para a pegada (descendente) encarar a mao que chega, uma vez no inicio ou
// durante toda a interacao. Torcao: so em volta do eixo; balanco: qualquer
// eixo; completa: a pegada copia a rotacao da mao.
class InteractionGrip final : public ComponentValue {
public:
  u64 grip=0;bool enabled=true,rotateOnce=true;u32 mode=0,axis=1;float twistWeight=1,swingWeight=0,fullWeight=1;
  AE_FX_VALUE(InteractionGrip)
  bool valid() const override {return ikReference(grip)&&mode<=1&&axis<=5&&ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<grip<<' '<<enabled<<' '<<rotateOnce<<' '<<mode<<' '<<axis<<' '<<twistWeight<<' '<<swingWeight<<' '<<fullWeight;}
  bool read(std::istream &in,u32 version) override {return version==1&&(in>>grip>>enabled>>rotateOnce>>mode>>axis>>twistWeight>>swingWeight>>fullWeight)&&valid();}
};
// Gatilho de interacao (Final IK InteractionTrigger, refeito): faixa a frente
// do objeto (raio, angulo maximo, deslocamento, orbita). O personagem com
// Sistema de interacao dentro dela gera o evento `entered`; com Automatico, a
// interacao comeca sozinha com a mao escolhida; `left` ao sair.
class InteractionTrigger final : public ComponentValue {
public:
  u64 object=0;bool enabled=true,automatic=true,orbit=false,fixUp=true;u32 hands=0;float radius=.5f,maxAngle=45,offsetX=0,offsetZ=0,angleOffset=0;
  AE_FX_VALUE(InteractionTrigger)
  bool valid() const override {return ikReference(object)&&hands<=2&&ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<object<<' '<<enabled<<' '<<automatic<<' '<<orbit<<' '<<fixUp<<' '<<hands<<' '<<radius<<' '<<maxAngle<<' '<<offsetX<<' '<<offsetZ<<' '<<angleOffset;}
  bool read(std::istream &in,u32 version) override {return version==1&&(in>>object>>enabled>>automatic>>orbit>>fixUp>>hands>>radius>>maxAngle>>offsetX>>offsetZ>>angleOffset)&&valid();}
};
// Corpo inteiro: ate quatro pontas com alvo; quando um alvo esta alem do
// alcance do membro, a pelve e puxada na direcao dele (limitada) e os membros
// sao resolvidos depois. Bipede, quadrupede ou mecanismo com pelve.
class FullBodyIk final : public ComponentValue {
public:
  u64 pelvis=0,tips[4]{},targets[4]{},chest=0;bool enabled=true;float weight=1,bodyPull=.6f,maxBodyOffset=.5f,spineBend=.5f,limbBones=2;
  AE_FX_VALUE(FullBodyIk)
  bool valid() const override {
    if(!ikReference(pelvis)||!ikReference(chest))return false;
    for(u32 i=0;i<4;++i)if(!ikReference(tips[i])||!ikReference(targets[i]))return false;
    return ikNumbersValid(*this);
  }
  void write(std::ostream &out) const override {
    out<<pelvis;for(u32 i=0;i<4;++i)out<<' '<<tips[i]<<' '<<targets[i];
    out<<' '<<enabled<<' '<<weight<<' '<<bodyPull<<' '<<maxBodyOffset<<' '<<chest<<' '<<spineBend<<' '<<limbBones;
  }
  bool read(std::istream &in,u32 version) override {
    if(version<1||version>2||!(in>>pelvis))return false;
    for(u32 i=0;i<4;++i)if(!(in>>tips[i]>>targets[i]))return false;
    if(!(in>>enabled>>weight>>bodyPull>>maxBodyOffset))return false;
    chest=0;spineBend=.5f;limbBones=2;
    return (version==1||static_cast<bool>(in>>chest>>spineBend>>limbBones))&&valid();
  }
};

// FABRIK em arvore (FinalIK FABRIKRoot): este objeto e a raiz comum de ate
// quatro cadeias (tentaculos, dedos de uma garra, pernas de aranha). Cada passe
// estima onde a raiz ficaria para cada ponta alcancar o alvo; a raiz vai para a
// media (contida por Fixar raiz) e as cadeias sao resolvidas a partir dela.
class FabrikRoot final : public ComponentValue {
public:
  u64 tips[4]{},targets[4]{};bool enabled=true;float weight=1,rootPin=.5f,iterations=8;
  AE_FX_VALUE(FabrikRoot)
  bool valid() const override {for(u32 i=0;i<4;++i)if(!ikReference(tips[i])||!ikReference(targets[i]))return false;return ikNumbersValid(*this);}
  void write(std::ostream &out) const override {for(u32 i=0;i<4;++i)out<<tips[i]<<' '<<targets[i]<<' ';out<<enabled<<' '<<weight<<' '<<rootPin<<' '<<iterations;}
  bool read(std::istream &in,u32 version) override {
    if(version!=1)return false;
    for(u32 i=0;i<4;++i)if(!(in>>tips[i]>>targets[i]))return false;
    return (in>>enabled>>weight>>rootPin>>iterations)&&valid();
  }
};
// Corpo por alvos (estudo do FinalIK VRIK, sem exigir XR): cabeca e maos
// seguem alvos que podem ser quaisquer objetos (controles, ancoras, outro
// personagem). A pelve acompanha a cabeca, o peito se curva, a cabeca copia a
// rotacao do alvo e os bracos sao resolvidos com dois ossos.
class BodyByTargets final : public ComponentValue {
public:
  u64 pelvis=0,chest=0,head=0,leftHand=0,rightHand=0,headTarget=0,leftTarget=0,rightTarget=0;
  bool enabled=true,headRotation=true;float weight=1,bodyFollow=1,spineBend=.6f,maxBodyOffset=1;
  AE_FX_VALUE(BodyByTargets)
  bool valid() const override {
    for(const u64 v:{pelvis,chest,head,leftHand,rightHand,headTarget,leftTarget,rightTarget})if(!ikReference(v))return false;
    return ikNumbersValid(*this);
  }
  void write(std::ostream &out) const override {
    out<<pelvis<<' '<<chest<<' '<<head<<' '<<leftHand<<' '<<rightHand<<' '<<headTarget<<' '<<leftTarget<<' '<<rightTarget<<' '
       <<enabled<<' '<<headRotation<<' '<<weight<<' '<<bodyFollow<<' '<<spineBend<<' '<<maxBodyOffset;
  }
  bool read(std::istream &in,u32 version) override {
    return version==1&&(in>>pelvis>>chest>>head>>leftHand>>rightHand>>headTarget>>leftTarget>>rightTarget>>enabled>>headRotation>>weight>>bodyFollow>>spineBend>>maxBodyOffset)&&valid();
  }
};

// Encaixe (aparencia): acessorio, arma ou parte modular presa a um osso com
// deslocamento. Resolve no fim do passo de IK, entao segue a pose final (com
// IK); osso ausente apos trocar o visual vira diagnostico, nunca o osso errado.
class Socket final : public ComponentValue {
public:
  u64 bone=0;bool enabled=true,followScale=false;float position[3]{},rotation[3]{};
  AE_FX_VALUE(Socket)
  bool valid() const override {return ikReference(bone)&&ikNumbersValid(*this);}
  void write(std::ostream &out) const override {out<<bone<<' '<<enabled<<' '<<followScale;for(float v:position)out<<' '<<v;for(float v:rotation)out<<' '<<v;}
  bool read(std::istream &in,u32 version) override {
    if(version!=1||!(in>>bone>>enabled>>followScale))return false;
    for(float &v:position)if(!(in>>v))return false;
    for(float &v:rotation)if(!(in>>v))return false;
    return valid();
  }
};

// Filho de (Child-Of animavel, UMotion Pro 1.29 "Child-Of Constraint"): troca
// o pai do objeto durante a animacao. Os canais locais do objeto passam a valer
// no espaco do Pai escolhido; Fixo no personagem usa a raiz do personagem
// (IK pinning). Sem pai e sem fixo, segue a hierarquia normal.
class ChildOf final : public ComponentValue {
public:
  u64 parent=0;bool enabled=true,pinned=false;
  AE_FX_VALUE(ChildOf)
  bool valid() const override {return ikReference(parent);}
  void write(std::ostream &out) const override {out<<parent<<' '<<enabled<<' '<<pinned;}
  bool read(std::istream &in,u32 version) override {return version==1&&(in>>parent>>enabled>>pinned)&&valid();}
};

inline constexpr std::array<ComponentNumber,4> hitReactionNumbers{{AE_FX_NUM(HitReaction,"weight","Peso",weight,0,1,.01f,"Influência",""),
  AE_FX_NUM(HitReaction,"strength","Força do golpe",strength,0,90,1,"Golpe","graus"),AE_FX_NUM(HitReaction,"duration","Recuperação",duration,.05f,5,.01f,"Golpe","s"),
  AE_FX_NUM(HitReaction,"chain_length","Ossos que reagem",chainLength,1,8,1,"Golpe","ossos")}};
inline constexpr std::array<ComponentBoolean,2> hitReactionBooleans{{AE_FX_BOOL(HitReaction,"enabled","Ativo",enabled,"Influência"),
  AE_FX_BOOL(HitReaction,"react_to_collisions","Reagir a colisões",reactToCollisions,"Golpe")}};
inline constexpr std::array<ComponentEnum,1> hitReactionEnums{{AE_FX_ENUM(HitReaction,"axis","Frente do osso",ikAxisOptions,axis,"Golpe")}};
inline constexpr std::array<ComponentParameter,2> hitReactionHitParameters{{{"direction","Direção do golpe",ComponentValueKind::Vector3},{"force","Força",ComponentValueKind::Number}}};
inline constexpr std::array<ComponentParameter,1> hitReactionFrontParameters{{{"force","Força",ComponentValueKind::Number}}};
inline constexpr std::array<ComponentMethod,2> hitReactionMethods{{
  {"hit","Golpear","Gira a cadeia para longe da direção; força 1 usa a força autorada",hitReactionHitParameters},
  {"hit_front","Golpear de frente","Golpe contra a Frente do osso (empurra para trás); usado pelas Conexões de evento",hitReactionFrontParameters}}};
inline const ComponentType HitReaction::descriptor{"astra.ik.hit_reaction",2,AE_FX_MAKE(HitReaction),hitReactionNumbers,hitReactionBooleans,hitReactionEnums,nullptr,false,{},{},{},{},{},{},hitReactionMethods};

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

inline constexpr std::array<ComponentNumber,3> interactionObjectNumbers{{AE_FX_NUM(InteractionObject,"reach_time","Tempo para alcançar",reachTime,.05f,5,.01f,"Tempo","s"),
  AE_FX_NUM(InteractionObject,"release_time","Tempo para soltar",releaseTime,.05f,5,.01f,"Tempo","s"),
  AE_FX_NUM(InteractionObject,"lift_time","Tempo para levantar",liftTime,.05f,5,.01f,"Carregar","s")}};
inline constexpr std::array<ComponentBoolean,1> interactionObjectBooleans{{AE_FX_BOOL(InteractionObject,"pick_up","Pegar ao alcançar",pickUp,"Pegada")}};
inline constexpr std::array<ComponentObjectReference,4> interactionObjectReferences{{
  AE_FX_REF(InteractionObject,"grip","Ponto de pegada",grip,Descendant,"O próprio objeto","Pegada"),
  AE_FX_REF(InteractionObject,"second_grip","Pegada da 2ª mão",secondGrip,Descendant,"Mesma pegada","Pegada"),
  AE_FX_REF(InteractionObject,"hand_pose","Pose da mão",handPose,Descendant,"Sem pose","Pegada"),
  AE_FX_REF(InteractionObject,"hold_point","Ponto de carregar",holdPoint,OtherNonDescendant,"Fica na mão","Carregar")}};
inline const ComponentType InteractionObject::descriptor{"astra.ik.interaction_object",2,AE_FX_MAKE(InteractionObject),interactionObjectNumbers,interactionObjectBooleans,{},nullptr,false,interactionObjectReferences};

inline constexpr std::array<ComponentNumber,6> interactionSystemNumbers{{AE_FX_NUM(InteractionSystem,"speed","Velocidade",speed,.1f,5,.05f,"Interação",""),
  AE_FX_NUM(InteractionSystem,"limb_bones","Ossos do braço",limbBones,2,8,1,"Interação","ossos"),
  AE_FX_NUM(InteractionSystem,"crouch","Agachar até",crouch,0,2,.01f,"Corpo","m"),
  AE_FX_NUM(InteractionSystem,"lean","Inclinar o tronco",lean,0,1,.01f,"Corpo",""),
  AE_FX_NUM(InteractionSystem,"look_speed","Entrada do olhar",lookSpeed,.1f,20,.1f,"Olhar","/s"),
  AE_FX_NUM(InteractionSystem,"look_turn","Giro do olhar",lookTurn,.5f,60,.5f,"Olhar","/s")}};
inline constexpr std::array<ComponentBoolean,1> interactionSystemBooleans{{AE_FX_BOOL(InteractionSystem,"enabled","Ativo",enabled,"Interação")}};
inline constexpr std::array<ComponentObjectReference,6> interactionSystemReferences{{
  AE_FX_REF(InteractionSystem,"auto_effector","Mão ao iniciar",autoEffector,Descendant,"Sem início automático","Início sem script"),
  AE_FX_REF(InteractionSystem,"auto_object","Objeto ao iniciar",autoObject,OtherNonDescendant,"Sem objeto","Início sem script"),
  AE_FX_REF(InteractionSystem,"auto_second","Segunda mão ao iniciar",autoSecond,Descendant,"Uma mão","Início sem script"),
  AE_FX_REF(InteractionSystem,"pelvis","Pelve",pelvis,Descendant,"Sem agachar","Corpo"),
  AE_FX_REF(InteractionSystem,"chest","Peito",chest,Descendant,"Sem inclinar","Corpo"),
  AE_FX_REF(InteractionSystem,"look_at","Olhar com",lookAt,Descendant,"Sem olhar","Olhar")}};
inline constexpr std::array<ComponentParameter,2> interactionStartParameters{{{"effector","Mão",ComponentValueKind::Object},{"object","Objeto",ComponentValueKind::Object}}};
inline constexpr std::array<ComponentParameter,1> interactionStopParameters{{{"effector","Mão",ComponentValueKind::Object}}};
inline constexpr std::array<ComponentMethod,3> interactionSystemMethods{{
  {"start","Interagir","Leva a mão até o Objeto interagível; falso se a mão ou o objeto não servem",interactionStartParameters,ComponentValueKind::Boolean},
  {"stop","Soltar","Devolve a mão à animação e larga o objeto pego",interactionStopParameters},
  {"trigger","Interagir pelo gatilho","Começa a interação do Gatilho de interação mais próximo em que o personagem está; falso se não há nenhum",{},ComponentValueKind::Boolean}}};
inline constexpr std::array<ComponentParameter,2> interactionEventPayload{{{"effector","Mão",ComponentValueKind::Object},{"object","Objeto",ComponentValueKind::Object}}};
inline constexpr std::array<ComponentEvent,5> interactionSystemEvents{{
  {"reached","Alcançou","A mão chegou ao ponto de pegada",interactionEventPayload},
  {"picked","Levantou","A última mão chegou e o objeto sobe ao ponto de carregar",interactionEventPayload},
  {"released","Soltou","A mão voltou à animação",interactionEventPayload},
  {"entered","Entrou no gatilho","O personagem entrou na faixa de um Gatilho de interação (Objeto = o gatilho)",interactionEventPayload},
  {"left","Saiu do gatilho","O personagem saiu da faixa do gatilho",interactionEventPayload}}};
inline const ComponentType InteractionSystem::descriptor{"astra.ik.interaction_system",5,AE_FX_MAKE(InteractionSystem),interactionSystemNumbers,interactionSystemBooleans,{},nullptr,false,interactionSystemReferences,{},{},{},{},{},interactionSystemMethods,interactionSystemEvents};

inline constexpr std::array<ComponentEnumOption,2> interactionGripModes{{{0,"Torção e balanço"},{1,"Rotação completa"}}};
inline bool interactionGripTwist(const ComponentValue &v){return static_cast<const InteractionGrip&>(v).mode==0;}
inline bool interactionGripFull(const ComponentValue &v){return static_cast<const InteractionGrip&>(v).mode==1;}
inline constexpr std::array<ComponentNumber,3> interactionGripNumbers{{
  ComponentNumber{"Peso da torção",0,1,.01f,[](const ComponentValue&v)->const float&{return static_cast<const InteractionGrip&>(v).twistWeight;},[](ComponentValue&v){return &static_cast<InteractionGrip&>(v).twistWeight;},"twist_weight",{"Giro","",nullptr,interactionGripTwist},true},
  ComponentNumber{"Peso do balanço",0,1,.01f,[](const ComponentValue&v)->const float&{return static_cast<const InteractionGrip&>(v).swingWeight;},[](ComponentValue&v){return &static_cast<InteractionGrip&>(v).swingWeight;},"swing_weight",{"Giro","",nullptr,interactionGripTwist},true},
  ComponentNumber{"Peso da rotação",0,1,.01f,[](const ComponentValue&v)->const float&{return static_cast<const InteractionGrip&>(v).fullWeight;},[](ComponentValue&v){return &static_cast<InteractionGrip&>(v).fullWeight;},"full_weight",{"Giro","",nullptr,interactionGripFull},true}}};
inline constexpr std::array<ComponentBoolean,2> interactionGripBooleans{{AE_FX_BOOL(InteractionGrip,"enabled","Ativo",enabled,"Giro"),
  AE_FX_BOOL(InteractionGrip,"rotate_once","Girar uma vez",rotateOnce,"Giro")}};
inline constexpr std::array<ComponentEnum,2> interactionGripEnums{{AE_FX_ENUM(InteractionGrip,"mode","Modo",interactionGripModes,mode,"Giro"),
  AE_FX_ENUM(InteractionGrip,"axis","Eixo da torção",ikAxisOptions,axis,"Giro")}};
inline constexpr std::array<ComponentObjectReference,1> interactionGripReferences{{
  AE_FX_REF(InteractionGrip,"grip","Pegada",grip,Descendant,"A pegada do Objeto interagível","Giro")}};
inline const ComponentType InteractionGrip::descriptor{"astra.ik.interaction_grip",1,AE_FX_MAKE(InteractionGrip),interactionGripNumbers,interactionGripBooleans,interactionGripEnums,nullptr,false,interactionGripReferences};

inline constexpr std::array<ComponentEnumOption,3> interactionTriggerHands{{{0,"Mão ao iniciar"},{1,"Segunda mão ao iniciar"},{2,"As duas"}}};
inline constexpr std::array<ComponentNumber,5> interactionTriggerNumbers{{AE_FX_NUM(InteractionTrigger,"radius","Raio",radius,.05f,10,.05f,"Faixa","m"),
  AE_FX_NUM(InteractionTrigger,"max_angle","Ângulo máximo",maxAngle,1,180,1,"Faixa","graus"),
  AE_FX_NUM(InteractionTrigger,"offset_x","Deslocamento lateral",offsetX,-5,5,.05f,"Faixa","m"),
  AE_FX_NUM(InteractionTrigger,"offset_z","Deslocamento à frente",offsetZ,-5,5,.05f,"Faixa","m"),
  AE_FX_NUM(InteractionTrigger,"angle_offset","Giro da direção",angleOffset,-180,180,1,"Faixa","graus")}};
inline constexpr std::array<ComponentBoolean,4> interactionTriggerBooleans{{AE_FX_BOOL(InteractionTrigger,"enabled","Ativo",enabled,"Gatilho"),
  AE_FX_BOOL(InteractionTrigger,"automatic","Automático",automatic,"Gatilho"),
  AE_FX_BOOL(InteractionTrigger,"orbit","Em volta do objeto",orbit,"Faixa"),
  AE_FX_BOOL(InteractionTrigger,"fix_up","Cima fixo",fixUp,"Faixa")}};
inline constexpr std::array<ComponentEnum,1> interactionTriggerEnums{{AE_FX_ENUM(InteractionTrigger,"hands","Mãos",interactionTriggerHands,hands,"Gatilho")}};
inline constexpr std::array<ComponentObjectReference,1> interactionTriggerReferences{{
  AE_FX_REF(InteractionTrigger,"interactable","Objeto interagível",object,Any,"Este objeto","Gatilho")}};
inline const ComponentType InteractionTrigger::descriptor{"astra.ik.interaction_trigger",1,AE_FX_MAKE(InteractionTrigger),interactionTriggerNumbers,interactionTriggerBooleans,interactionTriggerEnums,nullptr,false,interactionTriggerReferences};

inline constexpr std::array<ComponentNumber,5> fullBodyNumbers{{AE_FX_NUM(FullBodyIk,"weight","Peso",weight,0,1,.01f,"Influência",""),
  AE_FX_NUM(FullBodyIk,"body_pull","Puxar o corpo",bodyPull,0,1,.01f,"Corpo",""),AE_FX_NUM(FullBodyIk,"max_body_offset","Deslocamento máximo da pelve",maxBodyOffset,0,3,.01f,"Corpo","m"),
  AE_FX_NUM(FullBodyIk,"spine_bend","Curvar o tronco",spineBend,0,1,.01f,"Corpo",""),AE_FX_NUM(FullBodyIk,"limb_bones","Ossos por membro",limbBones,2,8,1,"Corpo","ossos")}};
inline constexpr std::array<ComponentBoolean,1> fullBodyBooleans{{AE_FX_BOOL(FullBodyIk,"enabled","Ativo",enabled,"Influência")}};
inline constexpr std::array<ComponentObjectReference,10> fullBodyReferences{{
  AE_FX_REF(FullBodyIk,"pelvis","Pelve",pelvis,Descendant,"Escolher pelve","Corpo"),
  AE_FX_REF(FullBodyIk,"chest","Peito",chest,Descendant,"Sem tronco","Corpo"),
  AE_FX_REF(FullBodyIk,"tip_1","Ponta 1 (mão esq.)",tips[0],Descendant,"Sem ponta","Ponta 1"),AE_FX_REF(FullBodyIk,"target_1","Alvo 1",targets[0],OtherNonDescendant,"Sem alvo","Ponta 1"),
  AE_FX_REF(FullBodyIk,"tip_2","Ponta 2 (mão dir.)",tips[1],Descendant,"Sem ponta","Ponta 2"),AE_FX_REF(FullBodyIk,"target_2","Alvo 2",targets[1],OtherNonDescendant,"Sem alvo","Ponta 2"),
  AE_FX_REF(FullBodyIk,"tip_3","Ponta 3 (pé esq.)",tips[2],Descendant,"Sem ponta","Ponta 3"),AE_FX_REF(FullBodyIk,"target_3","Alvo 3",targets[2],OtherNonDescendant,"Sem alvo","Ponta 3"),
  AE_FX_REF(FullBodyIk,"tip_4","Ponta 4 (pé dir.)",tips[3],Descendant,"Sem ponta","Ponta 4"),AE_FX_REF(FullBodyIk,"target_4","Alvo 4",targets[3],OtherNonDescendant,"Sem alvo","Ponta 4")}};
inline const ComponentType FullBodyIk::descriptor{"astra.ik.full_body",2,AE_FX_MAKE(FullBodyIk),fullBodyNumbers,fullBodyBooleans,{},nullptr,false,fullBodyReferences};
inline constexpr std::array<ComponentNumber,3> fabrikRootNumbers{{AE_FX_NUM(FabrikRoot,"weight","Peso",weight,0,1,.01f,"Influência",""),
  AE_FX_NUM(FabrikRoot,"root_pin","Fixar raiz",rootPin,0,1,.01f,"Raiz",""),AE_FX_NUM(FabrikRoot,"iterations","Iterações",iterations,1,64,1,"Raiz","")}};
inline constexpr std::array<ComponentBoolean,1> fabrikRootBooleans{{AE_FX_BOOL(FabrikRoot,"enabled","Ativo",enabled,"Influência")}};
inline constexpr std::array<ComponentObjectReference,8> fabrikRootReferences{{
  AE_FX_REF(FabrikRoot,"tip_1","Ponta 1",tips[0],Descendant,"Sem ponta","Cadeia 1"),AE_FX_REF(FabrikRoot,"target_1","Alvo 1",targets[0],OtherNonDescendant,"Sem alvo","Cadeia 1"),
  AE_FX_REF(FabrikRoot,"tip_2","Ponta 2",tips[1],Descendant,"Sem ponta","Cadeia 2"),AE_FX_REF(FabrikRoot,"target_2","Alvo 2",targets[1],OtherNonDescendant,"Sem alvo","Cadeia 2"),
  AE_FX_REF(FabrikRoot,"tip_3","Ponta 3",tips[2],Descendant,"Sem ponta","Cadeia 3"),AE_FX_REF(FabrikRoot,"target_3","Alvo 3",targets[2],OtherNonDescendant,"Sem alvo","Cadeia 3"),
  AE_FX_REF(FabrikRoot,"tip_4","Ponta 4",tips[3],Descendant,"Sem ponta","Cadeia 4"),AE_FX_REF(FabrikRoot,"target_4","Alvo 4",targets[3],OtherNonDescendant,"Sem alvo","Cadeia 4")}};
inline const ComponentType FabrikRoot::descriptor{"astra.ik.fabrik_root",1,AE_FX_MAKE(FabrikRoot),fabrikRootNumbers,fabrikRootBooleans,{},nullptr,false,fabrikRootReferences};

inline constexpr std::array<ComponentNumber,4> bodyByTargetsNumbers{{AE_FX_NUM(BodyByTargets,"weight","Peso",weight,0,1,.01f,"Influência",""),
  AE_FX_NUM(BodyByTargets,"body_follow","Corpo segue a cabeça",bodyFollow,0,1,.01f,"Corpo",""),AE_FX_NUM(BodyByTargets,"spine_bend","Curvar o tronco",spineBend,0,1,.01f,"Corpo",""),
  AE_FX_NUM(BodyByTargets,"max_body_offset","Deslocamento máximo da pelve",maxBodyOffset,0,5,.01f,"Corpo","m")}};
inline constexpr std::array<ComponentBoolean,2> bodyByTargetsBooleans{{AE_FX_BOOL(BodyByTargets,"enabled","Ativo",enabled,"Influência"),
  AE_FX_BOOL(BodyByTargets,"head_rotation","Cabeça copia a rotação do alvo",headRotation,"Cabeça")}};
inline constexpr std::array<ComponentObjectReference,8> bodyByTargetsReferences{{
  AE_FX_REF(BodyByTargets,"pelvis","Pelve",pelvis,Descendant,"Escolher pelve","Corpo"),AE_FX_REF(BodyByTargets,"chest","Peito",chest,Descendant,"Sem tronco","Corpo"),
  AE_FX_REF(BodyByTargets,"head","Cabeça",head,Descendant,"Escolher cabeça","Cabeça"),AE_FX_REF(BodyByTargets,"head_target","Alvo da cabeça",headTarget,OtherNonDescendant,"Sem alvo","Cabeça"),
  AE_FX_REF(BodyByTargets,"left_hand","Mão esquerda",leftHand,Descendant,"Sem mão","Mãos"),AE_FX_REF(BodyByTargets,"left_target","Alvo esquerdo",leftTarget,OtherNonDescendant,"Sem alvo","Mãos"),
  AE_FX_REF(BodyByTargets,"right_hand","Mão direita",rightHand,Descendant,"Sem mão","Mãos"),AE_FX_REF(BodyByTargets,"right_target","Alvo direito",rightTarget,OtherNonDescendant,"Sem alvo","Mãos")}};
inline const ComponentType BodyByTargets::descriptor{"astra.ik.body_by_targets",1,AE_FX_MAKE(BodyByTargets),bodyByTargetsNumbers,bodyByTargetsBooleans,{},nullptr,false,bodyByTargetsReferences};
inline constexpr std::array<ComponentNumber,6> socketNumbers{{
  AE_FX_NUM(Socket,"position_x","Posição X",position[0],-100,100,.005f,"Deslocamento","m"),AE_FX_NUM(Socket,"position_y","Posição Y",position[1],-100,100,.005f,"Deslocamento","m"),
  AE_FX_NUM(Socket,"position_z","Posição Z",position[2],-100,100,.005f,"Deslocamento","m"),AE_FX_NUM(Socket,"rotation_x","Rotação X",rotation[0],-360,360,.5f,"Deslocamento","graus"),
  AE_FX_NUM(Socket,"rotation_y","Rotação Y",rotation[1],-360,360,.5f,"Deslocamento","graus"),AE_FX_NUM(Socket,"rotation_z","Rotação Z",rotation[2],-360,360,.5f,"Deslocamento","graus")}};
inline constexpr std::array<ComponentBoolean,2> socketBooleans{{AE_FX_BOOL(Socket,"enabled","Ativo",enabled,"Encaixe"),AE_FX_BOOL(Socket,"follow_scale","Seguir escala do osso",followScale,"Encaixe")}};
inline constexpr std::array<ComponentObjectReference,1> socketReferences{{AE_FX_REF(Socket,"bone","Osso",bone,Other,"Escolher osso","Encaixe")}};
inline constexpr std::array<ComponentTriple,2> socketTriples{{{"position","Posição",{"position_x","position_y","position_z"}},{"rotation","Rotação",{"rotation_x","rotation_y","rotation_z"}}}};
inline constexpr std::array<ComponentBoolean,2> childOfBooleans{{AE_FX_BOOL(ChildOf,"enabled","Ativo",enabled,"Filho de"),
  ComponentBoolean{"pinned","Fixo no personagem",[](const ComponentValue&v){return static_cast<const ChildOf&>(v).pinned;},[](ComponentValue&v,bool n){static_cast<ChildOf&>(v).pinned=n;},{"Filho de","","Os canais valem no espaço da raiz do personagem (IK pinning)."},true}}};
inline constexpr std::array<ComponentObjectReference,1> childOfReferences{{
  ComponentObjectReference{"parent","Pai","",ObjectReferenceScope::Other,"Hierarquia normal",[](const ComponentValue &v){return static_cast<const ChildOf&>(v).parent;},[](ComponentValue &v,u64 n){static_cast<ChildOf&>(v).parent=n;},{"Filho de","","Os canais locais do objeto valem no espaço deste pai; troque com uma chave."},nullptr,nullptr,true}}};
inline const ComponentType ChildOf::descriptor{"astra.constraint.child_of",1,AE_FX_MAKE(ChildOf),{},childOfBooleans,{},nullptr,false,childOfReferences};
inline const ComponentType Socket::descriptor{"astra.ik.socket",1,AE_FX_MAKE(Socket),socketNumbers,socketBooleans,{},nullptr,false,socketReferences,socketTriples};
#undef AE_FX_NUM
#undef AE_FX_BOOL
#undef AE_FX_ENUM
#undef AE_FX_REF
#undef AE_FX_VALUE
#undef AE_FX_MAKE
} // namespace ae::scene

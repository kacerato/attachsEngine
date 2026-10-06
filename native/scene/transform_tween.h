#pragma once
#include "scene/components.h"
#include <array>
namespace ae::scene {
class TransformTween final:public ComponentValue {
public:

#include "scene/generated/transform_tween_TransformTween_fields0.inc"

 float duration=1,delay=0;u32 easing=0,loops=1;float destination[9]{0,0,0,0,0,0,1,1,1};
 u32 finishedAction=0;u64 finishedTarget=0;
 static const ComponentType descriptor;
 const ComponentType&type()const override{return descriptor;}
 std::unique_ptr<ComponentValue>clone()const override{return std::make_unique<TransformTween>(*this);}
 bool valid()const override{if(easing>3||loops>100000||finishedAction>3||finishedTarget>std::numeric_limits<u32>::max())return false;for(const auto&p:descriptor.numbers){const float n=p.read(*this);if(!std::isfinite(n)||n<p.minimum||n>p.maximum)return false;}return true;}
 void write(std::ostream&o)const override{o<<enabled<<' '<<autoplay<<' '<<pingpong<<' '<<relative<<' '<<position<<' '<<rotation<<' '<<scale<<' '<<duration<<' '<<delay<<' '<<easing<<' '<<loops;for(float n:destination)o<<' '<<n;o<<' '<<ignoreTimeScale<<' '<<finishedAction<<' '<<finishedTarget;}
 bool read(std::istream&i,u32 v)override{if((v<1||v>3)||!(i>>enabled>>autoplay>>pingpong>>relative>>position>>rotation>>scale>>duration>>delay>>easing>>loops))return false;for(float&n:destination)if(!(i>>n))return false;ignoreTimeScale=false;if(v>=2&&!(i>>ignoreTimeScale))return false;finishedAction=0;finishedTarget=0;if(v>=3&&!(i>>finishedAction>>finishedTarget))return false;return valid();}
};
inline const auto tweenNumbers=[] {
 std::array<ComponentNumber,11>a{};
 a[0]={"Duração",.001f,36000,.1f,[](const ComponentValue&v)->const float&{return static_cast<const TransformTween&>(v).duration;},[](ComponentValue&v){return &static_cast<TransformTween&>(v).duration;},"duration",{"Tempo","s"}};
 a[1]={"Espera",0,36000,.1f,[](const ComponentValue&v)->const float&{return static_cast<const TransformTween&>(v).delay;},[](ComponentValue&v){return &static_cast<TransformTween&>(v).delay;},"delay",{"Tempo","s"}};
#define AE_TWEEN_NUMBER(I,ID,LABEL,GROUP,UNIT) a[I+2]={LABEL,-100000,100000,.1f,[](const ComponentValue&v)->const float&{return static_cast<const TransformTween&>(v).destination[I];},[](ComponentValue&v){return &static_cast<TransformTween&>(v).destination[I];},ID,{GROUP,UNIT}};
 AE_TWEEN_NUMBER(0,"position_x","Destino X","Destino","u") AE_TWEEN_NUMBER(1,"position_y","Destino Y","Destino","u") AE_TWEEN_NUMBER(2,"position_z","Destino Z","Destino","u")
 AE_TWEEN_NUMBER(3,"rotation_x","Rotação X","Destino","graus") AE_TWEEN_NUMBER(4,"rotation_y","Rotação Y","Destino","graus") AE_TWEEN_NUMBER(5,"rotation_z","Rotação Z","Destino","graus")
 AE_TWEEN_NUMBER(6,"scale_x","Escala X","Destino","x") AE_TWEEN_NUMBER(7,"scale_y","Escala Y","Destino","x") AE_TWEEN_NUMBER(8,"scale_z","Escala Z","Destino","x")
#undef AE_TWEEN_NUMBER
 return a;}();
#include "scene/generated/transform_tween_tweenBooleans.inc"
inline constexpr std::array<ComponentEnumOption,4>tweenEasings{{{0,"Linear"},{1,"Smoothstep"},{2,"Quadrático entrada"},{3,"Quadrático saída"}}};
inline constexpr std::array<ComponentEnumOption,5>tweenLoops{{{0,"Infinito"},{1,"Uma vez"},{2,"Duas vezes"},{3,"Três vezes"},{10,"Dez vezes"}}};
inline constexpr std::array<ComponentEnumOption,4>tweenFinishedActions{{{0,"Desconectado"},{1,"Ativar objeto"},{2,"Desativar objeto"},{3,"Alternar objeto"}}};
#include "scene/generated/transform_tween_tweenEnums.inc"
inline constexpr std::array<ComponentObjectReference,1>tweenReferences{{
 {"finished_target","Receptor","",ObjectReferenceScope::Any,"Escolher objeto",
  [](const ComponentValue&v){return static_cast<const TransformTween&>(v).finishedTarget;},[](ComponentValue&v,u64 n){static_cast<TransformTween&>(v).finishedTarget=n;},
  {"Conexão","","Referência persistente remapeada ao clonar; controla activeSelf do receptor",[](const ComponentValue&v){return static_cast<const TransformTween&>(v).finishedAction!=0;}},
  [](const ComponentValue&v){const auto&c=static_cast<const TransformTween&>(v);return c.enabled&&c.finishedAction!=0;}}
}};
inline constexpr std::array<ComponentTriple,3>tweenTriples{{{"position_destination","Posição destino",{"position_x","position_y","position_z"}},{"rotation_destination","Rotação destino",{"rotation_x","rotation_y","rotation_z"}},{"scale_destination","Escala destino",{"scale_x","scale_y","scale_z"}}}};
// Operações sobre o avaliador de Play (runtime/scene_tweens.h).
inline constexpr std::array<ComponentMethod,5> tweenMethods{{
 {"restart","Reiniciar","Recomeça da pose atual, conservando a pausa"},
 {"cancel","Cancelar","Interrompe sem voltar à pose inicial"},
 {"pause","Pausar","Congela o progresso"},
 {"resume","Retomar","Continua o progresso pausado"},
 {"elapsed","Decorrido","Segundos desde o início, incluindo o atraso",{},ComponentValueKind::Number},
}};
inline constexpr std::array<ComponentEvent,1> tweenEvents{{
 {"completed","Concluiu","Emitido uma vez quando as repetições finitas terminam, depois da pose final"}
}};
inline const ComponentType TransformTween::descriptor{"astra.tween.transform",3,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<TransformTween>();},tweenNumbers,tweenBooleans,tweenEnums,nullptr,false,tweenReferences,tweenTriples,{},{},{},{},tweenMethods,tweenEvents};
}

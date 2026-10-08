#pragma once
// Avaliador dos componentes Animator (bloco I, F064) no mundo de Play.
//
// A cada quadro, por Animator e por camada: escolhe transições (Qualquer
// estado primeiro, depois as do estado atual, na ordem autorada), avança o
// tempo normalizado dos estados, calcula os pesos das misturas 1D/2D e entrega
// ao SceneAnimator uma amostra por clipe (tempo local, peso, camada, máscara).
// A mistura entre camadas e a escrita das poses continuam no SceneAnimator.
//
// Estado de execução (valores dos parâmetros, estado atual, tempos) mora aqui
// e nunca vai para o arquivo; o Play sempre parte dos padrões autorados.
#include "runtime/component_operations.h"
#include "runtime/game_world.h"
#include "runtime/scene_animation.h"
#include "scene/animator.h"

#include <string>
#include <string_view>
#include <vector>

namespace ae::runtime {
class ScenePhysics;

namespace animator_detail {
struct WeightedMotion {u32 motion=0;float weight=0;};
// Mistura 1D: linear entre os dois limiares vizinhos; fora da faixa, o extremo.
void blend1D(const std::vector<scene::AnimatorMotion> &motions,float value,std::vector<WeightedMotion> &out);
// Mistura 2D (BlendSpace2D da Godot): triangulação de Delaunay dos pontos;
// dentro de um triângulo, coordenadas baricêntricas; fora, o ponto mais
// próximo da borda, entre os dois vértices daquela aresta.
void blend2D(const std::vector<scene::AnimatorMotion> &motions,float x,float y,std::vector<WeightedMotion> &out);
// Passagem pelo tempo normalizado `mark` entre dois quadros; em estado em laço
// e marca abaixo de 1, conta a cada volta.
bool crossed(float previous,float current,float mark,bool loop);
} // namespace animator_detail

class SceneAnimatorGraphs {
public:
  enum class Status : u32 {Ok=0,UnknownComponent=1,UnknownParameter=2,WrongType=3,UnknownState=4,InvalidArgument=5,UnknownLayer=6,BoundParameter=7};
  enum class ParameterOperation : u32 {Get=0,SetFloat=1,SetInt=2,SetBool=3,SetTrigger=4,ResetTrigger=5};
  struct LayerState {
    u64 layer=0,current=0,next=0;
    float time=0,nextTime=0,elapsed=0,duration=0;
    bool transitioning=false,entered=false,nextEntered=false,currentFresh=true;
  };
  struct Instance {
    ObjectId owner=kInvalidObject;u64 instance=0;
    std::vector<u64> parameterIds;std::vector<float> values;
    std::vector<LayerState> layers;
    bool motionAvailable=false;
    std::string motionDiagnostic;
  };
  struct Info {u64 state=0,next=0;float normalizedTime=0,progress=0;bool transitioning=false;std::string name,nextName;};

  void setEvents(ComponentEventQueue *events) noexcept {events_=events;}
  void setPhysics(const ScenePhysics *physics) noexcept {physics_=physics;}
  void reset() {instances_.clear();}
  // Avalia todos os Animators e acrescenta as amostras para o SceneAnimator.
  bool advance(GameWorld &world,const AnimationLibrary &library,float scaled,float unscaled,std::vector<SceneAnimator::ExternalSample> &samples);

  Status parameter(GameWorld &world,ObjectId owner,u64 instance,std::string_view name,ParameterOperation operation,float value,float &result);
  Status play(GameWorld &world,ObjectId owner,u64 instance,u32 layer,std::string_view state,float crossFade);
  Status info(const GameWorld &world,ObjectId owner,u64 instance,u32 layer,Info &out) const;
  const Instance *find(ObjectId owner,u64 instance) const;

private:
  Instance *ensure(GameWorld &world,ObjectId owner,u64 instance,const scene::Animator **component);
  void sync(Instance &runtime,const scene::Animator &authored);
  void emit(GameWorld &world,const Instance &runtime,std::string_view event,std::initializer_list<scene::ComponentOperationValue> values);
  std::vector<Instance> instances_;
  ComponentEventQueue *events_=nullptr;
  const ScenePhysics *physics_=nullptr;
};

} // namespace ae::runtime

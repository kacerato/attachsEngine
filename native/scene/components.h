#pragma once
#include "core/base.h"
#include "core/engine_capability.h"
#include "resources/asset_registry.h"
#include <memory>
#include <string_view>
#include <vector>
#include <span>
#include <sstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <cmath>
#include <algorithm>

namespace ae::scene {
struct ComponentType;
class Components;
class ComponentValue {
public:
  virtual ~ComponentValue() = default;
  u64 instanceId() const noexcept { return instanceId_; }
  virtual const ComponentType &type() const = 0;
  virtual bool unresolved() const noexcept { return false; }
  virtual std::unique_ptr<ComponentValue> clone() const = 0;
  virtual bool valid() const = 0;
  virtual void write(std::ostream &) const = 0;
  virtual bool read(std::istream &,u32 version) = 0;
private:
  friend class Components;
  u64 instanceId_=0;
};
// O que muda quando a propriedade muda. É um conjunto de bits porque uma única
// edição costuma atingir mais de um derivado — trocar a malha invalida o
// desenho E os derivados dela (tangente, LOD, colisão cozida).
//
// Zero não é "nada acontece": é "nada ALÉM do que o componente já declara".
// A invalidação efetiva é `esquema | propriedade`, o que evita repetir em toda
// linha o que vale para o componente inteiro e evita um sentinela ambíguo.
namespace Invalidate {
inline constexpr u32 Draw=1u<<0;               // descritor/uniforme do desenho
inline constexpr u32 MaterialDescriptor=1u<<1; // material efetivo e seu descritor
inline constexpr u32 MeshDerived=1u<<2;        // tangente, LOD, colisão e bake da malha
inline constexpr u32 TextureResidency=1u<<3;   // mips e bytes residentes
inline constexpr u32 LightCluster=1u<<4;       // seleção e orçamento de luzes do quadro
inline constexpr u32 ShadowMap=1u<<5;          // cascatas e mapas de profundidade
inline constexpr u32 Probes=1u<<6;             // irradiância/reflexão assadas
inline constexpr u32 PhysicsShape=1u<<7;       // forma cozida no solver
inline constexpr u32 PhysicsBody=1u<<8;        // corpo recriado no solver
inline constexpr u32 Policy=1u<<9;             // nova época da política resolvida
inline constexpr u32 Script=1u<<10;            // recompilação/religação de comportamento
inline constexpr u32 Transform=1u<<11;         // pose de mundo e bounds propagados
inline constexpr u32 Input=1u<<12;             // mapeamento de entrada e câmera
}

// Presentation is attached to persistent properties, not inspector indices.
// Predicates are shared by UI and write validation. Hidden is not read-only:
// scripts may configure inactive modes without changing the visible layout.
//
// Os três últimos campos são o contrato do plano universal de cenários (§6):
// qual capacidade do motor a propriedade exige, quem lê o valor e o que
// precisa ser reconstruído. Vazio/zero HERDA o que o esquema do componente
// declara — a matriz de propriedades é gerada dessa resolução, nunca à mão.
struct PropertyPresentation {
  std::string_view group{};
  std::string_view unit{};
  const char *help=nullptr;
  bool (*visible)(const ComponentValue &)=nullptr;
  bool (*editable)(const ComponentValue &)=nullptr;
  std::string_view capability{};
  std::string_view consumer{};
  u32 invalidates=0;
  bool isVisible(const ComponentValue &v) const {return !visible || visible(v);}
  bool isEditable(const ComponentValue &v) const {return !editable || editable(v);}
  // Uma propriedade só é editável quando o consumidor dela existe. Não há
  // caminho que persista uma escolha sem efeito: a validação de escrita e a
  // interface leem esta mesma resposta.
  bool hasConsumer() const {return core::engineCapabilityAuthorable(capability);}
};
struct ComponentNumber {
  const char *name;
  float minimum,maximum,dragStep;
  const float &(*read)(const ComponentValue &);
  float *(*write)(ComponentValue &);
  // Persistent API identity, independent of label, layout and C++ member name.
  // Empty is reserved for legacy fields without a reflected public contract.
  std::string_view id{};
  PropertyPresentation presentation{};
};
struct ComponentBoolean {
  std::string_view id;
  const char *name;
  bool (*read)(const ComponentValue &);
  void (*write)(ComponentValue &,bool);
  PropertyPresentation presentation{};
};
struct ComponentEnumOption { u32 value; const char *name; };
struct ComponentEnum {
  std::string_view id;
  const char *name;
  std::span<const ComponentEnumOption> options;
  u32 (*read)(const ComponentValue &);
  void (*write)(ComponentValue &,u32);
  PropertyPresentation presentation{};
};
enum class ObjectReferenceScope { Any, SelfOrAncestor, Other };
struct ComponentObjectReference {
  std::string_view id;
  const char *name;
  std::string_view requiredType;
  ObjectReferenceScope scope=ObjectReferenceScope::Any;
  const char *nullLabel="Nenhum";
  u64 (*read)(const ComponentValue &)=nullptr;
  void (*write)(ComponentValue &,u64)=nullptr;
  PropertyPresentation presentation{};
  // Execution readiness is separate from authoring validity: incomplete drafts
  // remain editable, serializable and undoable. Never infer this from visibility.
  bool (*requiredForExecution)(const ComponentValue &)=nullptr;
  bool isRequiredForExecution(const ComponentValue &value) const {
    return requiredForExecution && requiredForExecution(value);
  }
};
// Referência a RECURSO, que não é referência a objeto.
//
// Um objeto vive na cena e tem InstanceId/ObjectId; um recurso vive no projeto
// e tem AssetGuid. Confundir os dois é o defeito que faz um preset levado para
// outro projeto apontar para "a malha número 3" em vez de "esta malha", e é o
// que obriga o plano universal a exigir "referência tipada" e "remapeamento".
//
// Até aqui só o MeshRenderer tinha recursos, e quem precisava listá-los — grafo
// de impacto, reparo de referência quebrada, preset, relatório de dependências
// — fazia `static_cast<const MeshRenderer&>` e percorria os slots à mão. Todo
// componente futuro com recurso (LODGroup, Decal, Volume com perfil, Terrain)
// teria de reabrir cada um desses lugares. Declarado aqui, o binding é
// enumerável como número, booleano e enumeração já são.
//
// `slots` existe porque um binding pode ter mais de um endereço no MESMO valor:
// uma malha com várias primitivas é UM objeto autoral com N slots de material.
struct ComponentResourceBinding {
  std::string_view id;
  const char *name;
  resources::AssetType kind;
  // Quantos endereços este binding tem neste valor. Zero esconde o binding.
  u32 (*slots)(const ComponentValue &)=nullptr;
  resources::AssetGuid (*read)(const ComponentValue &,u32 slot)=nullptr;
  // Falso quando o slot não existe mais; nunca cria slot novo.
  bool (*write)(ComponentValue &,u32 slot,resources::AssetGuid value)=nullptr;
  PropertyPresentation presentation{};
  // Alguns bindings distinguem "herda da fonte" de "sem recurso". Quem só
  // pergunta "que recursos este componente usa?" não precisa saber disso; quem
  // remapeia entre projetos precisa, para não transformar herança em ausência.
  // `none` é o valor que significa AUSÊNCIA DECLARADA nesse binding; identidade
  // inválida significa herdar. Sem os dois declarados aqui, cada consumidor
  // reinventaria a sentinela — e um deles a escreveria errado.
  bool inheritable=false;
  resources::AssetGuid none{};
  bool declaresNone(const resources::AssetGuid &value) const noexcept {
    return none.valid() && value==none;
  }
  u32 slotCount(const ComponentValue &v) const {return slots?slots(v):0;}
  resources::AssetGuid at(const ComponentValue &v,u32 slot) const {
    return read&&slot<slotCount(v)?read(v,slot):resources::AssetGuid{};
  }
};

enum class ComponentTripleKind { Vector, LinearColor };
struct ComponentTriple {
  std::string_view id;
  const char *name;
  std::string_view channels[3];
  ComponentTripleKind kind=ComponentTripleKind::Vector;
};
// Descriptors have static lifetime. IDs and versions are archive contracts;
// pointer identity is only a checked, process-local type token (no RTTI).
struct ComponentType {
  std::string_view id;
  u32 version;
  std::unique_ptr<ComponentValue> (*create)();
  std::span<const ComponentNumber> numbers{};
  std::span<const ComponentBoolean> booleans{};
  std::span<const ComponentEnum> enums{};
  // Migration may split old data into several components in a transactional candidate.
  bool (*migrate)(std::istream &,u32,Components &)=nullptr;
  bool allowMultiple=false;
  std::span<const ComponentObjectReference> references{};
  std::span<const ComponentTriple> triples{};
  // Recursos do projeto que este componente endereça por identidade.
  std::span<const ComponentResourceBinding> resourceBindings{};
};
enum class UnknownComponentPolicy { Reject, Preserve };
// An unavailable type is authored data, never a successfully loaded behavior.
// Own both ID and payload; cloning must rebuild the descriptor's string view.
class MissingComponent final : public ComponentValue {
public:
  MissingComponent(std::string id,u32 version,std::string payload)
      : id_(std::move(id)),payload_(std::move(payload)),type_{id_,version,nullptr} {type_.allowMultiple=true;}
  const ComponentType &type() const override { return type_; }
  bool unresolved() const noexcept override { return true; }
  std::unique_ptr<ComponentValue> clone() const override {
    auto copy=std::make_unique<MissingComponent>(id_,type_.version,payload_);
    return copy;
  }
  bool valid() const override { return !id_.empty() && id_.size()<=256 && type_.version>0 && payload_.size()<=1024*1024; }
  void write(std::ostream &out) const override { out<<payload_; }
  bool read(std::istream &,u32) override { return false; }
private:
  std::string id_,payload_;
  ComponentType type_;
};
class Components final {
public:
  static constexpr u32 MaximumCount=64;
  Components() = default;
  Components(const Components &other) : nextId_(other.nextId_) {
    values_.reserve(other.values_.size());
    for(const auto &value:other.values_) {auto copy=value->clone();copy->instanceId_=value->instanceId_;values_.push_back(std::move(copy));}
  }
  Components(Components &&) noexcept = default;
  Components &operator=(const Components &other) {
    if(this!=&other) { Components copy(other);values_.swap(copy.values_);std::swap(nextId_,copy.nextId_); }
    return *this;
  }
  Components &operator=(Components &&) noexcept = default;
  const ComponentValue *find(const ComponentType &type) const {
    for(const auto &value:values_) if(&value->type()==&type) return value.get();
    return nullptr;
  }
  const ComponentValue *find(std::string_view typeId) const {
    for(const auto &value:values_) if(value->type().id==typeId) return value.get();
    return nullptr;
  }
  const ComponentValue *findInstance(u64 id) const {
    for(const auto &value:values_) if(value->instanceId_==id) return value.get();
    return nullptr;
  }
  ComponentValue *editInstance(u64 id) {
    for(auto &value:values_) if(value->instanceId_==id) return value.get();
    return nullptr;
  }
  // Edit preserves singleton call sites; add creates a distinct authored instance.
  ComponentValue *edit(const ComponentType &type) {
    for(auto &value:values_) {
      if(&value->type()==&type) return value.get();
      if(value->type().id==type.id) return nullptr;
    }
    return add(type);
  }
  ComponentValue *add(const ComponentType &type) {
    if(values_.size()>=MaximumCount || type.id.empty() || !type.create) return nullptr;
    for(const auto &value:values_) if(value->type().id==type.id &&
        (!type.allowMultiple || &value->type()!=&type)) return nullptr;
    auto value=type.create();
    if(!value || &value->type()!=&type) return nullptr;
    value->instanceId_=nextIdentity();if(!value->instanceId_) return nullptr;
    auto *result=value.get();values_.push_back(std::move(value));return result;
  }
  bool removeInstance(u64 id) {
    for(auto i=values_.begin();i!=values_.end();++i) if((*i)->instanceId_==id) {
      values_.erase(i);return true;
    }
    return false;
  }
  bool remove(const ComponentType &type) {
    for(auto i=values_.begin();i!=values_.end();++i) if(&(*i)->type()==&type) {
      values_.erase(i);return true;
    }
    return false;
  }
  // Replace only an existing exact type, preserving order and unrelated values.
  // Clone before mutation so clipboard and authoring never share mutable state.
  bool replace(const ComponentValue &source) {return replaceInstance(0,source);}
  bool replaceInstance(u64 id,const ComponentValue &source) {
    if(!source.valid()) return false;
    for(auto &value:values_) if(&value->type()==&source.type() && (!id || value->instanceId_==id)) {
      auto candidate=source.clone();
      if(!candidate || &candidate->type()!=&source.type() || !candidate->valid()) return false;
      candidate->instanceId_=value->instanceId_;
      value=std::move(candidate);return true;
    }
    return false;
  }
  // Migration combines legacy fields and explicit records, never overwrites
  // two representations of the same type. Failure leaves this collection intact.
  bool merge(const Components &other) {
    if(values_.size()+other.values_.size()>MaximumCount) return false;
    for(const auto &incoming:other.values_) for(const auto &existing:values_)
      if(incoming->type().id==existing->type().id && !incoming->type().allowMultiple) return false;
    Components candidate(*this);candidate.nextId_=std::max(candidate.nextId_,other.nextId_);
    for(const auto &value:other.values_) {
      auto copy=value->clone();copy->instanceId_=value->instanceId_;
      if(!copy->instanceId_ || candidate.findInstance(copy->instanceId_)) copy->instanceId_=candidate.nextIdentity();
      if(!copy->instanceId_) return false;
      candidate.values_.push_back(std::move(copy));
    }
    *this=std::move(candidate);return true;
  }
  usize size() const noexcept { return values_.size(); }
  const ComponentValue *at(usize index) const noexcept { return index<values_.size()?values_[index].get():nullptr; }
  bool hasUnresolved() const noexcept {
    for(const auto &value:values_) if(value->unresolved()) return true;
    return false;
  }
  bool registeredWith(std::span<const ComponentType *const> registry) const {
    for(const auto &value:values_) {
      if(value->unresolved()) continue;
      bool found=false;
      for(const auto *entry:registry) if(entry==&value->type()) found=true;
      if(!found) return false;
    }
    return true;
  }
  bool valid() const {
    for(usize i=0;i<values_.size();++i) {
      const auto &value=values_[i];if(!value->valid()||!value->instanceId_) return false;
      for(usize j=0;j<i;++j) if(values_[j]->instanceId_==value->instanceId_) return false;
    }
    return true;
  }
  bool write(std::ostream &out,bool instanceIds=false) const {
    if(!valid()) return false;
    if(!instanceIds) for(usize i=0;i<values_.size();++i) for(usize j=0;j<i;++j)
      if(values_[i]->type().id==values_[j]->type().id) return false;
    out << ' ' << values_.size();
    if(instanceIds) out << ' ' << nextId_;
    for(const auto &value:values_) {
      std::ostringstream payload;payload.imbue(std::locale::classic());
      payload << std::setprecision(std::numeric_limits<float>::max_digits10);
      value->write(payload);
      if(!payload) return false;
      if(instanceIds) out << ' ' << value->instanceId_;
      out << ' ' << std::quoted(std::string(value->type().id)) << ' ' << value->type().version
          << ' ' << std::quoted(payload.str());
    }
    return bool(out);
  }
  bool read(std::istream &in,std::span<const ComponentType *const> registry,
            UnknownComponentPolicy unknown=UnknownComponentPolicy::Reject,bool instanceIds=false) {
    u32 count=0;if(!(in>>count) || count>MaximumCount) return false;
    Components candidate;u64 nextId=1;
    if(instanceIds && (!(in>>nextId) || !nextId)) return false;
    for(u32 i=0;i<count;++i) {
      std::string id,payload;u32 version=0;u64 instance=0;
      if(instanceIds && (!(in>>instance) || !instance || candidate.findInstance(instance))) return false;
      if(!(in>>std::quoted(id)>>version>>std::quoted(payload))) return false;
      if(id.empty() || id.size()>256 || !version || payload.size()>1024*1024) return false;
      const ComponentType *type=nullptr;
      for(const auto *entry:registry) if(entry && entry->id==id) {
        if(type) return false; // Ambiguous registration must never select by order.
        type=entry;
      }
      if(candidate.find(id) && (!instanceIds || (type && !type->allowMultiple))) return false;
      if(!type) {
        if(unknown==UnknownComponentPolicy::Reject) return false;
        auto missing=std::make_unique<MissingComponent>(std::move(id),version,std::move(payload));
        missing->instanceId_=instanceIds?instance:candidate.nextIdentity();
        candidate.values_.push_back(std::move(missing));
        continue;
      }
      std::istringstream data(payload);data.imbue(std::locale::classic());
      if(version!=type->version && type->migrate) {
        if(!type->migrate(data,version,candidate) || !candidate.find(*type) || !candidate.valid()) return false;
      } else {
        auto *value=candidate.add(*type);if(!value) return false;
        if(instanceIds) value->instanceId_=instance;
        if(!value->read(data,version) || !value->valid()) return false;
      }
      if(instanceIds && version!=type->version && type->migrate) {
        // Migration assigns its primary result the original identity. Additional
        // instances retain newly allocated identities, with collisions rejected.
        for(auto &value:candidate.values_) if(&value->type()==type) {
          if(candidate.findInstance(instance) && value->instanceId_!=instance) return false;
          value->instanceId_=instance;break;
        }
      }
      data>>std::ws;if(!data.eof()) return false;
    }
    if(instanceIds) {
      for(const auto &value:candidate.values_) if(value->instanceId_>=nextId) return false;
      candidate.nextId_=nextId;
    }
    if(!candidate.registeredWith(registry)) return false;
    *this=std::move(candidate);return true;
  }
private:
  u64 nextIdentity() {
    if(!nextId_ || nextId_==std::numeric_limits<u64>::max()) return 0;
    return nextId_++;
  }
  u64 nextId_=1;
  std::vector<std::unique_ptr<ComponentValue>> values_;
};
} // namespace ae::scene

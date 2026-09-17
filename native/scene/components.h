#pragma once
#include "core/base.h"
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
// Presentation is attached to persistent properties, not inspector indices.
// Predicates are shared by UI and write validation. Hidden is not read-only:
// scripts may configure inactive modes without changing the visible layout.
struct PropertyPresentation {
  std::string_view group{};
  std::string_view unit{};
  const char *help=nullptr;
  bool (*visible)(const ComponentValue &)=nullptr;
  bool (*editable)(const ComponentValue &)=nullptr;
  bool isVisible(const ComponentValue &v) const {return !visible || visible(v);}
  bool isEditable(const ComponentValue &v) const {return !editable || editable(v);}
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

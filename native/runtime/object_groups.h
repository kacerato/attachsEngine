#pragma once
#include "runtime/object_tags.h"
#include <sstream>

namespace ae::runtime {
// Godot 4.5 groups: an object may belong to multiple named sets. Names are
// identities, not indexes in an editor catalog. Authoring membership travels
// with the object; GameWorld edits its copied graph, never the saved document.
class ObjectGroups final {
public:
  static constexpr u32 MaximumCount=32;
  static bool validName(std::string_view name) {return ObjectTags::validName(name);}
  const std::vector<std::string> &names() const noexcept {return names_;}
  bool contains(std::string_view name) const {
    return std::binary_search(names_.begin(),names_.end(),name);
  }
  bool add(std::string_view name) {
    if(!validName(name)) return false;
    const auto at=std::lower_bound(names_.begin(),names_.end(),name);
    if(at!=names_.end() && *at==name) return true;
    if(names_.size()>=MaximumCount) return false;
    names_.insert(at,std::string(name));return true;
  }
  bool remove(std::string_view name) {
    if(!validName(name)) return false;
    const auto at=std::lower_bound(names_.begin(),names_.end(),name);
    if(at!=names_.end() && *at==name) names_.erase(at);
    return true;
  }
  void write(std::ostream &out) const {
    out<<"GROUPS 1 "<<names_.size();
    for(const auto &name:names_) out<<' '<<std::quoted(name);
  }
  bool read(std::istream &in) {
    std::string marker;u32 version=0,count=0;
    if(!(in>>marker>>version>>count) || marker!="GROUPS" || version!=1 || count>MaximumCount) return false;
    ObjectGroups value;
    for(u32 i=0;i<count;++i) {
      std::string name;
      if(!(in>>std::quoted(name)) || value.contains(name) || !value.add(name)) return false;
    }
    *this=std::move(value);return true;
  }
  std::string description() const {
    if(names_.empty()) return "Nenhum";
    std::ostringstream text;
    for(usize i=0;i<names_.size();++i) {if(i) text<<", ";text<<std::quoted(names_[i]);}
    return text.str();
  }
  friend bool operator==(const ObjectGroups &,const ObjectGroups &)=default;
private:
  std::vector<std::string> names_;
};
}

#pragma once
#include "core/base.h"
#include <algorithm>
#include <iomanip>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace ae::runtime {
// Nomes são identidades de gameplay, como em Unity 6000.0 Tags. Não são
// índices de UI: reordenar o catálogo não muda objetos nem scripts.
class ObjectTags final {
public:
  static constexpr u32 MaximumCount=256, MaximumNameBytes=63;
  static constexpr std::string_view Untagged="Untagged";
  static bool validName(std::string_view name) {
    if(name.empty() || name.size()>MaximumNameBytes || name.front()==' ' || name.back()==' ') return false;
    for(const unsigned char c:name) if(c<32 || c==127 || c=='"' || c=='\\') return false;
    return true;
  }
  const std::vector<std::string> &names() const {return names_;}
  bool contains(std::string_view name) const {
    return std::find(names_.begin(),names_.end(),name)!=names_.end();
  }
  bool add(std::string_view name) {
    if(!validName(name) || contains(name) || names_.size()>=MaximumCount) return false;
    names_.emplace_back(name);return true;
  }
  bool remove(std::string_view name) {
    if(name==Untagged) return false;
    const auto it=std::find(names_.begin(),names_.end(),name);
    if(it==names_.end()) return false;
    names_.erase(it);return true;
  }
  void write(std::ostream &out) const {
    out<<"ASTRA_TAGS 1 "<<names_.size()-1<<'\n';
    for(usize i=1;i<names_.size();++i) out<<std::quoted(names_[i])<<'\n';
  }
  bool read(std::istream &in) {
    std::string magic;u32 version=0,count=0;
    if(!(in>>magic>>version>>count) || magic!="ASTRA_TAGS" || version!=1 || count>=MaximumCount) return false;
    ObjectTags candidate;
    for(u32 i=0;i<count;++i) {std::string name;if(!(in>>std::quoted(name)) || !candidate.add(name)) return false;}
    in>>std::ws;if(!in.eof()) return false;
    *this=std::move(candidate);return true;
  }
  friend bool operator==(const ObjectTags &,const ObjectTags &)=default;
private:
  std::vector<std::string> names_{std::string(Untagged)};
};
}

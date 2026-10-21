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
  // Tag embutida da Unity: marca a câmera do jogo (Camera.main). Existe em todo
  // projeto, não se apaga e não vai para o arquivo do catálogo.
  static constexpr std::string_view MainCamera="MainCamera";
  static constexpr usize BuiltinCount=2;
  static bool builtin(std::string_view name) {return name==Untagged || name==MainCamera;}
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
    if(builtin(name)) return false;
    const auto it=std::find(names_.begin(),names_.end(),name);
    if(it==names_.end()) return false;
    names_.erase(it);return true;
  }
  void write(std::ostream &out) const {
    out<<"ASTRA_TAGS 1 "<<names_.size()-BuiltinCount<<'\n';
    for(usize i=BuiltinCount;i<names_.size();++i) out<<std::quoted(names_[i])<<'\n';
  }
  bool read(std::istream &in) {
    std::string magic;u32 version=0,count=0;
    if(!(in>>magic>>version>>count) || magic!="ASTRA_TAGS" || version!=1 || count>=MaximumCount) return false;
    ObjectTags candidate;
    // Um catálogo antigo que declarou MainCamera à mão continua válido.
    for(u32 i=0;i<count;++i) {
      std::string name;if(!(in>>std::quoted(name))) return false;
      if(name==MainCamera) continue;
      if(!candidate.add(name)) return false;
    }
    in>>std::ws;if(!in.eof()) return false;
    *this=std::move(candidate);return true;
  }
  friend bool operator==(const ObjectTags &,const ObjectTags &)=default;
private:
  std::vector<std::string> names_{std::string(Untagged),std::string(MainCamera)};
};
}

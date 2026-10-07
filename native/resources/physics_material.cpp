#include "resources/physics_material.h"

#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace ae::resources {

bool PhysicsMaterialAsset::valid() const {
  if(!guid.valid() || !revision || name.empty() || name.size()>128) return false;
  for(unsigned char c:name) if(c<32 || c==127) return false;
  return std::isfinite(friction) && friction>=0 && friction<=1 && std::isfinite(restitution) && restitution>=0 &&
         restitution<=1 && frictionCombine<=4 && restitutionCombine<=4;
}

std::string PhysicsMaterialAsset::serialize() const {
  std::ostringstream out;out.imbue(std::locale::classic());
  out<<std::setprecision(std::numeric_limits<float>::max_digits10);
  out<<"ASTRA_PHYSICS_MATERIAL "<<FormatVersion<<' '<<guid.text()<<' '<<revision<<' '<<std::quoted(name)<<' '
     <<friction<<' '<<restitution<<' '<<frictionCombine<<' '<<restitutionCombine<<'\n';
  return out.str();
}

bool PhysicsMaterialAsset::deserialize(std::string_view text,PhysicsMaterialAsset &out) {
  if(text.size()>4096) return false;
  std::istringstream in{std::string(text)};in.imbue(std::locale::classic());
  PhysicsMaterialAsset candidate;std::string magic,guid;u32 version=0;
  if(!(in>>magic>>version>>guid>>candidate.revision>>std::quoted(candidate.name)>>candidate.friction>>candidate.restitution
       >>candidate.frictionCombine>>candidate.restitutionCombine) ||
     magic!="ASTRA_PHYSICS_MATERIAL" || version!=FormatVersion || !AssetGuid::parse(guid,candidate.guid) || !candidate.valid())
    return false;
  out=std::move(candidate);return true;
}

} // namespace ae::resources

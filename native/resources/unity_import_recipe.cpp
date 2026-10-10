#include "resources/unity_import_recipe.h"
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>

namespace ae::resources {
namespace {
bool hash(std::string_view value) {return value.size()==64&&value.find_first_not_of("0123456789abcdef")==value.npos;}
bool valid(const UnityImportRecipe &value) {
  if(static_cast<u8>(value.kind)>2||!value.unityGuid.valid()||!value.fileId||!hash(value.sourceHash)||!hash(value.metaHash)||!hash(value.baselineHash)||
     value.nextIdentity<2||value.nextIdentity==std::numeric_limits<u64>::max()||value.identities.size()>UnityImportRecipe::MaximumIdentities||value.transformations.size()>64)return false;
  std::set<i64> fileIds;std::set<u64> identities;
  for(const auto &[file,id]:value.identities)if(!file||!id||id>=value.nextIdentity||!fileIds.insert(file).second||!identities.insert(id).second)return false;
  for(const auto &message:value.transformations)if(message.empty()||message.size()>4096||message.find_first_of("\r\n\0",0,3)!=message.npos)return false;
  return true;
}
}
std::string UnityImportRecipe::serialize() const {
  if(!valid(*this))return {};
  std::ostringstream out;out.imbue(std::locale::classic());out<<"ASTRA_UNITY_IMPORT 1 "<<static_cast<u32>(kind)<<' '<<unityGuid.text()<<' '<<fileId<<' '
    <<sourceHash<<' '<<metaHash<<' '<<baselineHash<<' '<<loop<<' '<<nextIdentity<<' '<<identities.size();
  for(const auto &[file,id]:identities)out<<' '<<file<<' '<<id;
  out<<' '<<transformations.size();for(const auto &message:transformations)out<<' '<<std::quoted(message);
  auto text=out.str();return text.size()<=MaximumBytes?text:std::string{};
}
bool UnityImportRecipe::deserialize(std::string_view text,UnityImportRecipe &out,std::string &diagnostic) {
  const auto fail=[&](){diagnostic="Receita Unity ausente, incompatível ou inválida.";return false;};
  if(text.size()>MaximumBytes)return fail();
  std::istringstream in{std::string(text)};in.imbue(std::locale::classic());UnityImportRecipe value;
  std::string magic,guid;u32 version=0,kind=0,loop=0;usize count=0;
  if(!(in>>magic>>version>>kind>>guid>>value.fileId>>value.sourceHash>>value.metaHash>>value.baselineHash>>loop>>value.nextIdentity>>count)||
      magic!="ASTRA_UNITY_IMPORT"||version!=1||kind>2||loop>1||count>MaximumIdentities||!AssetGuid::parse(guid,value.unityGuid))return fail();
  value.kind=static_cast<UnityImportKind>(kind);value.loop=loop!=0;
  value.identities.resize(count);for(auto &[file,id]:value.identities)if(!(in>>file>>id))return fail();
  if(!(in>>count)||count>64)return fail();
  value.transformations.resize(count);for(auto &message:value.transformations)if(!(in>>std::quoted(message)))return fail();
  in>>std::ws;if(!in.eof()||!valid(value))return fail();
  out=std::move(value);diagnostic.clear();return true;
}
} // namespace ae::resources

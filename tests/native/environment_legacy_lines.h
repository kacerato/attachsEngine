#pragma once

// Testes de migração montam arquivos antigos a partir da linha atual. Desde a
// Exposure do HDRP (cena v17, perfil v12) o bloco de exposição mudou no meio da
// linha; estes auxiliares o devolvem ao formato anterior para que cortar o fim
// continue produzindo uma linha antiga legítima.

#include "scene/environment.h"

#include <sstream>
#include <string>
#include <vector>

namespace environment_legacy {

inline std::vector<std::string> words(const std::string &line) {
  std::istringstream in(line);
  std::vector<std::string> out;
  for(std::string token;in>>token;) out.push_back(token);
  return out;
}
inline std::string join(const std::vector<std::string> &tokens) {
  std::string text;
  for(const auto &token:tokens) text+=(text.empty()?"":" ")+token;
  return text;
}

// Cena v17 → v16 (keepClouds) ou v15: os 3 enums + 7 overrides da Exposure
// viram as duas chaves antigas (exposição automática e peso central,
// desligadas) e sai o exposure_fixed. Na v15 saem também as nuvens (os 3
// interruptores e os números cloud_*).
inline std::string componentBeforeExposure(const std::string &line,bool keepClouds) {
  const auto tokens=words(line);
  constexpr size_t header=18,exposureTokens=10,cloudTokens=3;
  const size_t numbersAt=header+exposureTokens+cloudTokens;
  std::vector<std::string> out(tokens.begin(),tokens.begin()+header);
  if(out[7]=="3"||out[7]=="4") out[7]="1"; // None/Neutral não existiam: ACES
  out.push_back("0");out.push_back("0");
  if(keepClouds) out.insert(out.end(),tokens.begin()+header+exposureTokens,tokens.begin()+numbersAt);
  const auto &numbers=ae::scene::Environment::descriptor.numbers;
  for(size_t i=0;i<numbers.size()&&numbersAt+i<tokens.size();++i) {
    const std::string_view id=numbers[i].id;
    if(id=="exposure_fixed"||ae::scene::environmentNumberSinceV17(id)||(!keepClouds&&id.starts_with("cloud_"))) continue;
    out.push_back(tokens[numbersAt+i]);
  }
  return join(out);
}
inline std::string componentAsV16(const std::string &line) {return componentBeforeExposure(line,true);}
inline std::string componentAsV15(const std::string &line) {return componentBeforeExposure(line,false);}

// Perfil v12 → v11: os 11 números da Exposure (antes de energia da neblina,
// 31 de gradação, 5 corpos celestes e 16 das nuvens) voltam aos 9 antigos.
// v10 é a v11 sem as 16 palavras das nuvens.
inline std::string profileBeforeExposure(std::string text,bool keepClouds) {
  const bool newline=!text.empty()&&text.back()=='\n';
  if(newline) text.pop_back();
  auto cut=[&](size_t count) {size_t at=text.size();for(size_t i=0;i<count;++i) at=text.rfind(' ',at-1);return at;};
  text=text.substr(0,cut(10)); // os 10 do fim são da v12 (Color Adjustments e Bloom completos)
  const size_t tailStart=cut(53),exposureStart=cut(64);
  text=text.substr(0,exposureStart)+" 0 -8 8 0.05 0.95 0.18 2 3 0"+text.substr(tailStart);
  if(!keepClouds) text=text.substr(0,cut(16));
  const std::string magic="ASTRA_ENVIRONMENT_PROFILE 12 ";
  if(text.rfind(magic,0)==0) text.replace(0,magic.size(),keepClouds?"ASTRA_ENVIRONMENT_PROFILE 11 ":"ASTRA_ENVIRONMENT_PROFILE 10 ");
  return newline?text+"\n":text;
}
inline std::string profileAsV11(const std::string &text) {return profileBeforeExposure(text,true);}
inline std::string profileAsV10(const std::string &text) {return profileBeforeExposure(text,false);}

} // namespace environment_legacy

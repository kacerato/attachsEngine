#pragma once

// Testes de migração montam arquivos antigos a partir da linha atual. Desde a
// Exposure do HDRP (cena v17, perfil v12) o bloco de exposição mudou no meio da
// linha; estes auxiliares o devolvem ao formato anterior para que cortar o fim
// continue produzindo uma linha antiga legítima.

#include "scene/environment.h"
#include "resources/environment_profile.h"

#include <iomanip>
#include <limits>
#include <locale>
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
// Números que a linha v18 já não tem: os do pós novo (v19) e os das nuvens
// volumétricas (v20). Os laços abaixo andam o índice só pelos demais.
inline bool componentNumberSinceV19(std::string_view id) {
  return ae::scene::environmentNumberSinceV19(id)||ae::scene::environmentNumberSinceV20(id);
}
// Cena v20 → v19 (G4d): sai o tipo das nuvens (o 27º campo do cabeçalho) e os
// números novos delas; os números são os últimos tokens da linha.
inline std::string componentAsV19(const std::string &line) {
  auto tokens=words(line);
  tokens.erase(tokens.begin()+26);
  const auto &numbers=ae::scene::Environment::descriptor.numbers;
  const size_t numbersAt=tokens.size()-numbers.size();
  std::vector<std::string> out(tokens.begin(),tokens.begin()+numbersAt);
  for(size_t i=0;i<numbers.size();++i)
    if(!ae::scene::environmentNumberSinceV20(numbers[i].id)) out.push_back(tokens[numbersAt+i]);
  return join(out);
}
// Cena v19 → v18 (PK-4/PK-5): a linha nova é relida e escrita no formato de
// antes -- caixas por grupo (a de cada override que estava no perfil),
// camada própria 0 e os bools de efeito; os números do pós novo saem.
inline std::string componentAsV18(const std::string &line) {
  ae::scene::Environment e;
  std::istringstream in(line);in.imbue(std::locale::classic());
  if(!e.read(in,ae::scene::Environment::CurrentVersion)) return line;
  std::ostringstream out;out.imbue(std::locale::classic());
  out<<std::setprecision(std::numeric_limits<float>::max_digits10);
  const auto &v=e.values;const auto o=e.volumeOverrides();
  using G=ae::renderer::VolumeGroup;
  const auto added=[&](G group){return o.hasGroup(group)?1:0;};
  const auto guid=[](const ae::resources::AssetGuid &g){return g.valid()?g.text():std::string("-");};
  out<<v.active<<' '<<v.fog<<' '<<v.post<<' '<<v.bloom<<' '<<v.vignette<<' '<<v.filmGrain<<' '
     <<static_cast<ae::u32>(v.sky)<<' '<<static_cast<ae::u32>(v.toneMapper)<<' '<<v.ambientOcclusion<<' '
     <<static_cast<ae::u32>(e.shape)<<' '<<added(G::Sky)<<' '<<added(G::Fog)<<' '<<added(G::Tonemapping)<<' '<<0<<' '
     <<guid(e.profile)<<' '<<added(G::Ambient)<<' '<<v.physicalAtmosphereHighQuality<<' '<<guid(v.environmentMap)<<' '
     <<static_cast<ae::u32>(v.exposureMode)<<' '<<static_cast<ae::u32>(v.exposureMetering)<<' '
     <<static_cast<ae::u32>(v.exposureAdaptation)<<' '<<added(G::Exposure)<<' '<<added(G::Bloom)<<' '
     <<added(G::Vignette)<<' '<<added(G::ColorAdjustments)<<' '<<added(G::ColorGrading)<<' '<<added(G::FilmGrain)<<' '
     <<added(G::AmbientOcclusion)<<' '<<v.clouds.enabled<<' '<<v.clouds.castShadows<<' '<<added(G::Clouds)<<' '
     <<static_cast<ae::u32>(v.sunDisk)<<' '<<static_cast<ae::u32>(v.ambientSource)<<' '<<static_cast<ae::u32>(v.fogMode)<<' '
     <<static_cast<ae::u32>(v.fogColorMode)<<' '<<guid(v.skybox);
  for(const auto &property:ae::scene::Environment::descriptor.numbers)
    if(!componentNumberSinceV19(property.id)) out<<' '<<property.read(e);
  return out.str();
}
// Cena v18 → v17: saem os 5 campos da janela Lighting (PK-1/PK-2) depois das
// nuvens e os números novos dela, que são os últimos da tabela.
inline std::string componentAsV17(const std::string &line) {
  auto tokens=words(componentAsV18(line));
  constexpr size_t lightingAt=18+10+3,lightingTokens=5;
  tokens.erase(tokens.begin()+lightingAt,tokens.begin()+lightingAt+lightingTokens);
  std::vector<std::string> out(tokens.begin(),tokens.begin()+lightingAt);
  const auto &numbers=ae::scene::Environment::descriptor.numbers;
  // O texto v18 já não tem os números da v19 nem da v20: o índice só anda pelos demais.
  for(size_t i=0,at=lightingAt;i<numbers.size()&&at<tokens.size();++i) {
    if(componentNumberSinceV19(numbers[i].id)) continue;
    if(!ae::scene::environmentNumberSinceV18(numbers[i].id)) out.push_back(tokens[at]);
    ++at;
  }
  return join(out);
}
inline std::string componentBeforeExposure(const std::string &line,bool keepClouds) {
  const auto tokens=words(componentAsV17(line));
  constexpr size_t header=18,exposureTokens=10,cloudTokens=3;
  const size_t numbersAt=header+exposureTokens+cloudTokens;
  std::vector<std::string> out(tokens.begin(),tokens.begin()+header);
  if(out[7]=="3"||out[7]=="4") out[7]="1"; // None/Neutral não existiam: ACES
  out.push_back("0");out.push_back("0");
  if(keepClouds) out.insert(out.end(),tokens.begin()+header+exposureTokens,tokens.begin()+numbersAt);
  const auto &numbers=ae::scene::Environment::descriptor.numbers;
  // O texto v17 já não tem os números da v18 nem da v19: o índice só anda pelos demais.
  for(size_t i=0,at=numbersAt;i<numbers.size()&&at<tokens.size();++i) {
    const std::string_view id=numbers[i].id;
    if(componentNumberSinceV19(id)||ae::scene::environmentNumberSinceV18(id)) continue;
    const size_t token=at++;
    if(id=="exposure_fixed"||ae::scene::environmentNumberSinceV17(id)||(!keepClouds&&id.starts_with("cloud_"))) continue;
    out.push_back(tokens[token]);
  }
  return join(out);
}
inline std::string componentAsV16(const std::string &line) {return componentBeforeExposure(line,true);}
inline std::string componentAsV15(const std::string &line) {return componentBeforeExposure(line,false);}

// Perfil v12 → v11: os 11 números da Exposure (antes de energia da neblina,
// 31 de gradação, 5 corpos celestes e 16 das nuvens) voltam aos 9 antigos.
// v10 é a v11 sem as 16 palavras das nuvens.
// Perfil v15 → v14 (G4c/G4d): saem as 5 palavras das nuvens (espessura e Volumetric Clouds).
inline std::string profileAsV14(std::string text) {
  const bool newline=!text.empty()&&text.back()=='\n';
  if(newline) text.pop_back();
  size_t at=text.size();for(size_t i=0;i<5;++i) at=text.rfind(' ',at-1);
  text=text.substr(0,at);
  const std::string magic="ASTRA_ENVIRONMENT_PROFILE 15 ";
  if(text.rfind(magic,0)==0) text.replace(0,magic.size(),"ASTRA_ENVIRONMENT_PROFILE 14 ");
  const std::string skybox="ASTRA_SKYBOX_MATERIAL 15 ";
  if(text.rfind(skybox,0)==0) text.replace(0,skybox.size(),"ASTRA_SKYBOX_MATERIAL 14 ");
  return newline?text+"\n":text;
}
// Perfil v14 → v13 (PK-4/PK-5): sai o fim novo -- os 32 do pós completo, as
// curvas de cor (1 + 7 por chave, em cada uma das 8) e os overrides (11).
inline std::string profileAsV13(std::string text) {
  text=profileAsV14(text);
  ae::resources::EnvironmentProfile parsed;
  if(text.rfind("ASTRA_ENVIRONMENT_PROFILE 14 ",0)!=0||!ae::resources::EnvironmentProfile::deserialize(text,parsed)) return text;
  const bool newline=!text.empty()&&text.back()=='\n';
  if(newline) text.pop_back();
  size_t tail=32+11;
  for(const auto &curve:parsed.values.curves) tail+=1+7*curve.count;
  size_t at=text.size();for(size_t i=0;i<tail;++i) at=text.rfind(' ',at-1);
  text="ASTRA_ENVIRONMENT_PROFILE 13 "+text.substr(29,at-29);
  return newline?text+"\n":text;
}
// Perfil v13 → v12: saem as 21 palavras da janela Lighting do fim.
inline std::string profileAsV12(std::string text) {
  text=profileAsV13(text);
  const bool newline=!text.empty()&&text.back()=='\n';
  if(newline) text.pop_back();
  size_t at=text.size();for(size_t i=0;i<21;++i) at=text.rfind(' ',at-1);
  text=text.substr(0,at);
  const std::string magic="ASTRA_ENVIRONMENT_PROFILE 13 ";
  if(text.rfind(magic,0)==0) text.replace(0,magic.size(),"ASTRA_ENVIRONMENT_PROFILE 12 ");
  return newline?text+"\n":text;
}
inline std::string profileBeforeExposure(std::string text,bool keepClouds) {
  text=profileAsV12(text);
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

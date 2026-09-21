#pragma once

#include "renderer/scene_environment.h"
#include "scene/components.h"
#include <array>

namespace ae::scene {

class Environment final : public ComponentValue {
public:
  Environment() { values.active = true; }
  renderer::SceneEnvironment values{};
  renderer::EnvironmentVolumeShape shape=renderer::EnvironmentVolumeShape::Global;
  bool overrideSky=true,overrideFog=true,overridePost=true;
  float weight=1.0f,blendDistance=0.0f,boxSize[3]{10,10,10},sphereRadius=5.0f;
  u32 layer=0;
  resources::AssetGuid profile{};

  static const ComponentType descriptor;
  const ComponentType &type() const override { return descriptor; }
  std::unique_ptr<ComponentValue> clone() const override {
    return std::make_unique<Environment>(*this);
  }
  bool valid() const override {
    if(!values.valid()||static_cast<u32>(shape)>2||layer>7||!std::isfinite(weight)||weight<0||weight>1||
       !std::isfinite(blendDistance)||blendDistance<0||!std::isfinite(sphereRadius)||sphereRadius<=0) return false;
    for(float value:boxSize) if(!std::isfinite(value)||value<=0) return false;
    return true;
  }
  void write(std::ostream &out) const override {
    out << values.active << ' ' << values.fog << ' ' << values.post << ' '
        << values.bloom << ' ' << values.vignette << ' ' << values.filmGrain << ' '
        << static_cast<u32>(values.sky) << ' ' << static_cast<u32>(values.toneMapper) << ' '
        << values.ambientOcclusion << ' '
        << static_cast<u32>(shape) << ' ' << overrideSky << ' ' << overrideFog << ' '
        << overridePost << ' ' << layer << ' ' << (profile.valid()?profile.text():"-");
    for (const auto &property : descriptor.numbers) out << ' ' << property.read(*this);
  }
  bool read(std::istream &in, u32 version) override {
    u32 sky = 0, tone = 0;
    if ((version < 1 || version > 5) || !(in >> values.active >> values.fog >> values.post >> values.bloom >>
                          values.vignette))
      return false;
    if(version>=5 && !(in>>values.filmGrain)) return false;
    if(!(in>>sky>>tone) || sky>1 || tone>1) return false;
    if(version>=3 && !(in>>values.ambientOcclusion)) return false;
    values.sky = static_cast<renderer::SkyModel>(sky);
    values.toneMapper = static_cast<renderer::ToneMapper>(tone);
    if(version>=2) {
      u32 mode=0;
      if(!(in>>mode>>overrideSky>>overrideFog>>overridePost>>layer)||mode>2||layer>31) return false;
      shape=static_cast<renderer::EnvironmentVolumeShape>(mode);
    }
    if(version>=4) {
      std::string token;
      if(!(in>>token) || (token!="-"&&!resources::AssetGuid::parse(token,profile))) return false;
    }
    for (const auto &property : descriptor.numbers) {
      if(version<5 && property.id=="film_grain_intensity") continue;
      // Campos espaciais não existiam no arquivo v1.
      if (version>=3 || (version==2 && !property.id.starts_with("ambient_occlusion_")) ||
          property.id=="priority" || property.id.starts_with("sky_") ||
          property.id.starts_with("ground.") || property.id=="atmosphere" ||
          property.id.starts_with("sun_disk_") || property.id.starts_with("fog_") ||
          property.id=="exposure_ev" || property.id.starts_with("bloom_") ||
          property.id=="contrast" || property.id=="saturation" || property.id=="vignette_intensity")
        if (!(in >> *property.write(*this))) return false;
    }
    return valid();
  }
};

inline bool environmentIsLocal(const ComponentValue &value) {
  return static_cast<const Environment&>(value).shape!=renderer::EnvironmentVolumeShape::Global;
}
inline bool environmentIsBox(const ComponentValue &value) {
  return static_cast<const Environment&>(value).shape==renderer::EnvironmentVolumeShape::Box;
}
inline bool environmentIsSphere(const ComponentValue &value) {
  return static_cast<const Environment&>(value).shape==renderer::EnvironmentVolumeShape::Sphere;
}

inline bool environmentUsesAtmosphere(const ComponentValue &value) {
  return static_cast<const Environment &>(value).values.sky == renderer::SkyModel::Atmosphere;
}
inline bool environmentUsesFog(const ComponentValue &value) {
  return static_cast<const Environment &>(value).values.fog;
}
inline bool environmentUsesPost(const ComponentValue &value) {
  return static_cast<const Environment &>(value).values.post;
}
inline bool environmentUsesBloom(const ComponentValue &value) {
  const auto &environment = static_cast<const Environment &>(value).values;
  return environment.post && environment.bloom;
}
inline bool environmentUsesVignette(const ComponentValue &value) {
  const auto &environment = static_cast<const Environment &>(value).values;
  return environment.post && environment.vignette;
}
inline bool environmentUsesFilmGrain(const ComponentValue &value) {
  const auto &environment = static_cast<const Environment &>(value).values;
  return environment.post && environment.filmGrain;
}
inline bool environmentUsesAmbientOcclusion(const ComponentValue &value) {
  const auto &environment = static_cast<const Environment &>(value).values;
  return environment.post && environment.ambientOcclusion;
}

#define AE_ENV_NUMBER(id, label, field, lo, hi, step, group, unit, visible, capability, consumer, invalidation) \
  {label, lo, hi, step, [](const ComponentValue &v)->const float& { return static_cast<const Environment&>(v).values.field; }, \
   [](ComponentValue &v)->float* { return &static_cast<Environment&>(v).values.field; }, id, \
   {group, unit, nullptr, visible, nullptr, capability, consumer, invalidation}}
#define AE_ENV_COLOR(id, label, field, channel, group, visible, capability, consumer, invalidation) \
  {label, 0, 1, .01f, [](const ComponentValue &v)->const float& { return static_cast<const Environment&>(v).values.field[channel]; }, \
   [](ComponentValue &v)->float* { return &static_cast<Environment&>(v).values.field[channel]; }, id, \
   {group, "", nullptr, visible, nullptr, capability, consumer, invalidation}}

inline constexpr std::array<ComponentNumber, 22> environmentNumbers{{
  AE_ENV_NUMBER("priority", "Prioridade", priority, -1000, 1000, 1, "Geral", "", nullptr, "render.environment.atmosphere", "runtime/scene_environment.cpp → seleção", Invalidate::Draw),
  AE_ENV_COLOR("sky_zenith.r", "Zênite R", skyZenith, 0, "Atmosfera", environmentUsesAtmosphere, "render.environment.atmosphere", "rhi/shaders/dirt_road_sky.frag", Invalidate::Draw),
  AE_ENV_COLOR("sky_zenith.g", "Zênite G", skyZenith, 1, "Atmosfera", environmentUsesAtmosphere, "render.environment.atmosphere", "rhi/shaders/dirt_road_sky.frag", Invalidate::Draw),
  AE_ENV_COLOR("sky_zenith.b", "Zênite B", skyZenith, 2, "Atmosfera", environmentUsesAtmosphere, "render.environment.atmosphere", "rhi/shaders/dirt_road_sky.frag", Invalidate::Draw),
  AE_ENV_COLOR("sky_horizon.r", "Horizonte R", skyHorizon, 0, "Atmosfera", environmentUsesAtmosphere, "render.environment.atmosphere", "rhi/shaders/dirt_road_sky.frag", Invalidate::Draw),
  AE_ENV_COLOR("sky_horizon.g", "Horizonte G", skyHorizon, 1, "Atmosfera", environmentUsesAtmosphere, "render.environment.atmosphere", "rhi/shaders/dirt_road_sky.frag", Invalidate::Draw),
  AE_ENV_COLOR("sky_horizon.b", "Horizonte B", skyHorizon, 2, "Atmosfera", environmentUsesAtmosphere, "render.environment.atmosphere", "rhi/shaders/dirt_road_sky.frag", Invalidate::Draw),
  AE_ENV_COLOR("ground.r", "Chão R", ground, 0, "Atmosfera", environmentUsesAtmosphere, "render.environment.atmosphere", "rhi/shaders/dirt_road_sky.frag", Invalidate::Draw),
  AE_ENV_COLOR("ground.g", "Chão G", ground, 1, "Atmosfera", environmentUsesAtmosphere, "render.environment.atmosphere", "rhi/shaders/dirt_road_sky.frag", Invalidate::Draw),
  AE_ENV_COLOR("ground.b", "Chão B", ground, 2, "Atmosfera", environmentUsesAtmosphere, "render.environment.atmosphere", "rhi/shaders/dirt_road_sky.frag", Invalidate::Draw),
  AE_ENV_NUMBER("atmosphere", "Força atmosférica", atmosphere, 0, 1, .02f, "Atmosfera", "", environmentUsesAtmosphere, "render.environment.atmosphere", "rhi/shaders/dirt_road_sky.frag", Invalidate::Draw),
  AE_ENV_NUMBER("sun_disk_degrees", "Diâmetro do sol", sunDiskDegrees, .05f, 10, .05f, "Atmosfera", "°", environmentUsesAtmosphere, "render.environment.atmosphere", "rhi/shaders/dirt_road_sky.frag", Invalidate::Draw),
  AE_ENV_NUMBER("sun_disk_intensity", "Brilho do disco solar", sunDiskIntensity, 0, 100, .1f, "Atmosfera", "", environmentUsesAtmosphere, "render.environment.atmosphere", "rhi/shaders/dirt_road_sky.frag", Invalidate::Draw),
  AE_ENV_COLOR("fog_color.r", "Neblina R", fogColor, 0, "Neblina", environmentUsesFog, "render.environment.fog", "rhi/shaders/post_process_common.glsl", Invalidate::Draw),
  AE_ENV_COLOR("fog_color.g", "Neblina G", fogColor, 1, "Neblina", environmentUsesFog, "render.environment.fog", "rhi/shaders/post_process_common.glsl", Invalidate::Draw),
  AE_ENV_COLOR("fog_color.b", "Neblina B", fogColor, 2, "Neblina", environmentUsesFog, "render.environment.fog", "rhi/shaders/post_process_common.glsl", Invalidate::Draw),
  AE_ENV_NUMBER("fog_density", "Densidade", fogDensity, 0, 1, .001f, "Neblina", "1/m", environmentUsesFog, "render.environment.fog", "rhi/shaders/post_process_common.glsl", Invalidate::Draw),
  AE_ENV_NUMBER("fog_start", "Início", fogStart, 0, 10000, .5f, "Neblina", "m", environmentUsesFog, "render.environment.fog", "rhi/shaders/post_process_common.glsl", Invalidate::Draw),
  AE_ENV_NUMBER("exposure_ev", "Compensação", exposureEv, -16, 16, .1f, "Pós", "EV", environmentUsesPost, "render.post.tonemap", "rhi/shaders/post_process_common.glsl", Invalidate::Policy),
  AE_ENV_NUMBER("bloom_threshold", "Limiar do bloom", bloomThreshold, 0, 64, .05f, "Pós", "", environmentUsesBloom, "render.post.bloom", "rhi/shaders/post_process_common.glsl", Invalidate::Policy),
  AE_ENV_NUMBER("bloom_intensity", "Intensidade do bloom", bloomIntensity, 0, 2, .02f, "Pós", "", environmentUsesBloom, "render.post.bloom", "rhi/shaders/post_process_common.glsl", Invalidate::Policy),
  AE_ENV_NUMBER("contrast", "Contraste", contrast, .5f, 2, .02f, "Pós", "", environmentUsesPost, "render.post.tonemap", "rhi/shaders/post_process_common.glsl", Invalidate::Policy),
  // Saturação e vinheta são declaradas abaixo como números adicionais para
  // manter os identificadores públicos estáveis e os grupos condicionais.
}};

inline constexpr std::array<ComponentNumber, 3> environmentTailNumbers{{
  AE_ENV_NUMBER("saturation", "Saturação", saturation, 0, 2, .02f, "Pós", "", environmentUsesPost, "render.post.tonemap", "rhi/shaders/post_process_common.glsl", Invalidate::Policy),
  AE_ENV_NUMBER("vignette_intensity", "Intensidade da vinheta", vignetteIntensity, 0, 1, .02f, "Pós", "", environmentUsesVignette, "render.post.tonemap", "rhi/shaders/post_process_common.glsl", Invalidate::Policy),
  AE_ENV_NUMBER("film_grain_intensity", "Intensidade do grão", filmGrainIntensity, 0, 1, .01f, "Pós", "", environmentUsesFilmGrain, "render.post.film_grain", "rhi/shaders/post_process_common.glsl", Invalidate::Policy),
}};

inline constexpr std::array<ComponentNumber, 4> environmentAmbientOcclusionNumbers{{
  AE_ENV_NUMBER("ambient_occlusion_radius", "Raio", ambientOcclusionRadius, .05f, 10, .05f, "Oclusão ambiente", "m", environmentUsesAmbientOcclusion, "render.post.ambient_occlusion", "rhi/shaders/post_process_common.glsl", Invalidate::Policy),
  AE_ENV_NUMBER("ambient_occlusion_intensity", "Intensidade", ambientOcclusionIntensity, 0, 4, .05f, "Oclusão ambiente", "", environmentUsesAmbientOcclusion, "render.post.ambient_occlusion", "rhi/shaders/post_process_common.glsl", Invalidate::Policy),
  AE_ENV_NUMBER("ambient_occlusion_power", "Potência", ambientOcclusionPower, .1f, 4, .05f, "Oclusão ambiente", "", environmentUsesAmbientOcclusion, "render.post.ambient_occlusion", "rhi/shaders/post_process_common.glsl", Invalidate::Policy),
  AE_ENV_NUMBER("ambient_occlusion_bias", "Viés", ambientOcclusionBias, 0, 1, .005f, "Oclusão ambiente", "m", environmentUsesAmbientOcclusion, "render.post.ambient_occlusion", "rhi/shaders/post_process_common.glsl", Invalidate::Policy),
}};

#define AE_ENV_VOLUME_NUMBER(id,label,field,lo,hi,step,unit,visible) \
  {label,lo,hi,step,[](const ComponentValue &v)->const float&{return static_cast<const Environment&>(v).field;}, \
   [](ComponentValue &v)->float*{return &static_cast<Environment&>(v).field;},id,{"Volume",unit,nullptr,visible,nullptr,"render.environment.volumes","renderer/scene_environment.cpp",Invalidate::Draw}}
#define AE_ENV_VOLUME_AXIS(id,label,axis) \
  {label,.01f,100000.0f,.1f,[](const ComponentValue &v)->const float&{return static_cast<const Environment&>(v).boxSize[axis];}, \
   [](ComponentValue &v)->float*{return &static_cast<Environment&>(v).boxSize[axis];},id,{"Volume","m",nullptr,environmentIsBox,nullptr,"render.environment.volumes","renderer/scene_environment.cpp",Invalidate::Draw}}
inline constexpr std::array<ComponentNumber,6> environmentVolumeNumbers{{
  AE_ENV_VOLUME_NUMBER("weight","Peso",weight,0,1,.02f,"",nullptr),
  AE_ENV_VOLUME_NUMBER("blend_distance","Distância de mistura",blendDistance,0,100000,.1f,"m",environmentIsLocal),
  AE_ENV_VOLUME_AXIS("box_size.x","Tamanho X",0),
  AE_ENV_VOLUME_AXIS("box_size.y","Tamanho Y",1),
  AE_ENV_VOLUME_AXIS("box_size.z","Tamanho Z",2),
  AE_ENV_VOLUME_NUMBER("sphere_radius","Raio",sphereRadius,.01f,100000,.1f,"m",environmentIsSphere),
}};
#undef AE_ENV_VOLUME_AXIS
#undef AE_ENV_VOLUME_NUMBER

#undef AE_ENV_COLOR
#undef AE_ENV_NUMBER

// ComponentType recebe um span contínuo. Juntar as duas partes em um único
// array constexpr evita uma tabela paralela apenas para a paginação do editor.
inline constexpr auto environmentAllNumbers = [] {
  std::array<ComponentNumber, environmentNumbers.size() + environmentTailNumbers.size()+
      environmentAmbientOcclusionNumbers.size()+environmentVolumeNumbers.size()> out{};
  for (usize i = 0; i < environmentNumbers.size(); ++i) out[i] = environmentNumbers[i];
  for (usize i = 0; i < environmentTailNumbers.size(); ++i)
    out[environmentNumbers.size() + i] = environmentTailNumbers[i];
  for(usize i=0;i<environmentAmbientOcclusionNumbers.size();++i)
    out[environmentNumbers.size()+environmentTailNumbers.size()+i]=environmentAmbientOcclusionNumbers[i];
  for(usize i=0;i<environmentVolumeNumbers.size();++i)
    out[environmentNumbers.size()+environmentTailNumbers.size()+environmentAmbientOcclusionNumbers.size()+i]=environmentVolumeNumbers[i];
  return out;
}();

inline constexpr std::array<ComponentBoolean, 10> environmentBooleans{{
  {"enabled", "Ativo", [](const ComponentValue &v){return static_cast<const Environment&>(v).values.active;}, [](ComponentValue &v,bool b){static_cast<Environment&>(v).values.active=b;}, {"Geral","","Participa da seleção por prioridade"}},
  {"fog", "Neblina", [](const ComponentValue &v){return static_cast<const Environment&>(v).values.fog;}, [](ComponentValue &v,bool b){static_cast<Environment&>(v).values.fog=b;}, {"Neblina","","Aplica neblina exponencial pela profundidade",nullptr,nullptr,"render.environment.fog","rhi/shaders/post_process_common.glsl",Invalidate::Draw}},
  {"post", "Pós-processamento", [](const ComponentValue &v){return static_cast<const Environment&>(v).values.post;}, [](ComponentValue &v,bool b){static_cast<Environment&>(v).values.post=b;}, {"Pós","","Ativa os overrides autorais desta cena",nullptr,nullptr,"render.post.tonemap","rhi/shaders/post_process_common.glsl",Invalidate::Policy}},
  {"bloom", "Bloom", [](const ComponentValue &v){return static_cast<const Environment&>(v).values.bloom;}, [](ComponentValue &v,bool b){static_cast<Environment&>(v).values.bloom=b;}, {"Pós","","Espalha altas luzes antes do tonemap",environmentUsesPost,nullptr,"render.post.bloom","rhi/shaders/post_process_common.glsl",Invalidate::Policy}},
  {"vignette", "Vinheta", [](const ComponentValue &v){return static_cast<const Environment&>(v).values.vignette;}, [](ComponentValue &v,bool b){static_cast<Environment&>(v).values.vignette=b;}, {"Pós","","Escurece gradualmente as bordas",environmentUsesPost,nullptr,"render.post.tonemap","rhi/shaders/post_process_common.glsl",Invalidate::Policy}},
  {"film_grain", "Grão de filme", [](const ComponentValue &v){return static_cast<const Environment&>(v).values.filmGrain;}, [](ComponentValue &v,bool b){static_cast<Environment&>(v).values.filmGrain=b;}, {"Pós","","Adiciona grão sensível à luminância depois da resolução temporal",environmentUsesPost,nullptr,"render.post.film_grain","rhi/shaders/post_process_common.glsl",Invalidate::Policy}},
  {"ambient_occlusion", "Oclusão ambiente", [](const ComponentValue &v){return static_cast<const Environment&>(v).values.ambientOcclusion;}, [](ComponentValue &v,bool b){static_cast<Environment&>(v).values.ambientOcclusion=b;}, {"Oclusão ambiente","","Escurece contatos e concavidades a partir da profundidade da câmera",environmentUsesPost,nullptr,"render.post.ambient_occlusion","rhi/shaders/post_process_common.glsl",Invalidate::Policy}},
  {"override_sky","Sobrescrever céu",[](const ComponentValue &v){return static_cast<const Environment&>(v).overrideSky;},[](ComponentValue &v,bool b){static_cast<Environment&>(v).overrideSky=b;},{"Volume","","Participa da mistura de céu e atmosfera",nullptr,nullptr,"render.environment.volumes","renderer/scene_environment.cpp",Invalidate::Draw}},
  {"override_fog","Sobrescrever neblina",[](const ComponentValue &v){return static_cast<const Environment&>(v).overrideFog;},[](ComponentValue &v,bool b){static_cast<Environment&>(v).overrideFog=b;},{"Volume","","Participa da mistura de neblina",nullptr,nullptr,"render.environment.volumes","renderer/scene_environment.cpp",Invalidate::Draw}},
  {"override_post","Sobrescrever pós",[](const ComponentValue &v){return static_cast<const Environment&>(v).overridePost;},[](ComponentValue &v,bool b){static_cast<Environment&>(v).overridePost=b;},{"Volume","","Participa da mistura de exposição e pós",nullptr,nullptr,"render.environment.volumes","renderer/scene_environment.cpp",Invalidate::Policy}},
}};

inline constexpr std::array<ComponentEnumOption, 2> skyOptions{{{0,"HDRI"},{1,"Atmosfera"}}};
inline constexpr std::array<ComponentEnumOption, 2> toneOptions{{{0,"Neutro"},{1,"ACES"}}};
inline constexpr std::array<ComponentEnumOption,3> environmentShapeOptions{{{0,"Global"},{1,"Caixa"},{2,"Esfera"}}};
inline constexpr std::array<ComponentEnumOption,8> environmentLayerOptions{{
  {0,"Ambiente 0"},{1,"Ambiente 1"},{2,"Ambiente 2"},{3,"Ambiente 3"},
  {4,"Ambiente 4"},{5,"Ambiente 5"},{6,"Ambiente 6"},{7,"Ambiente 7"}
}};
inline constexpr std::array<ComponentEnum, 4> environmentEnums{{
  {"sky", "Céu", skyOptions, [](const ComponentValue &v){return static_cast<u32>(static_cast<const Environment&>(v).values.sky);}, [](ComponentValue &v,u32 n){static_cast<Environment&>(v).values.sky=static_cast<renderer::SkyModel>(n);}, {"Atmosfera","","Fonte visual do céu",nullptr,nullptr,"render.environment.atmosphere","rhi/shaders/dirt_road_sky.frag",Invalidate::Draw}},
  {"tone_mapper", "Tonemapping", toneOptions, [](const ComponentValue &v){return static_cast<u32>(static_cast<const Environment&>(v).values.toneMapper);}, [](ComponentValue &v,u32 n){static_cast<Environment&>(v).values.toneMapper=static_cast<renderer::ToneMapper>(n);}, {"Pós","","Curva aplicada ao HDR antes da saída",environmentUsesPost,nullptr,"render.post.tonemap","rhi/shaders/post_process_common.glsl",Invalidate::Policy}},
  {"volume_shape","Modo",environmentShapeOptions,[](const ComponentValue &v){return static_cast<u32>(static_cast<const Environment&>(v).shape);},[](ComponentValue &v,u32 n){static_cast<Environment&>(v).shape=static_cast<renderer::EnvironmentVolumeShape>(n);},{"Volume","","Global afeta toda vista; formas usam o Transform do objeto",nullptr,nullptr,"render.environment.volumes","runtime/scene_environment.cpp",Invalidate::Draw}},
  {"volume_layer","Camada",environmentLayerOptions,[](const ComponentValue &v){return static_cast<const Environment&>(v).layer;},[](ComponentValue &v,u32 n){static_cast<Environment&>(v).layer=n;},{"Volume","","Filtro consultado pela câmera",nullptr,nullptr,"render.environment.volumes","renderer/scene_environment.cpp",Invalidate::Draw}},
}};

inline constexpr std::array<ComponentTriple, 5> environmentTriples{{
  {"sky_zenith","Cor do zênite",{"sky_zenith.r","sky_zenith.g","sky_zenith.b"},ComponentTripleKind::LinearColor},
  {"sky_horizon","Cor do horizonte",{"sky_horizon.r","sky_horizon.g","sky_horizon.b"},ComponentTripleKind::LinearColor},
  {"ground","Cor do chão",{"ground.r","ground.g","ground.b"},ComponentTripleKind::LinearColor},
  {"fog_color","Cor da neblina",{"fog_color.r","fog_color.g","fog_color.b"},ComponentTripleKind::LinearColor},
  {"box_size","Tamanho da caixa",{"box_size.x","box_size.y","box_size.z"},ComponentTripleKind::Vector},
}};

inline constexpr std::array<ComponentResourceBinding,1> environmentResources{{
  {"profile","Perfil",resources::AssetType::EnvironmentProfile,
   [](const ComponentValue &){return 1u;},
   [](const ComponentValue &v,u32){return static_cast<const Environment&>(v).profile;},
   [](ComponentValue &v,u32,resources::AssetGuid value){static_cast<Environment&>(v).profile=value;return true;},
   {"Geral","","Aparência compartilhada; forma, peso, prioridade e camada continuam nesta instância",
    nullptr,nullptr,"render.environment.volumes","resources/environment_profile.cpp",Invalidate::Draw|Invalidate::Policy},
   true,{}}
}};

inline const ComponentType Environment::descriptor{
  "astra.render.environment", 5,
  []()->std::unique_ptr<ComponentValue>{return std::make_unique<Environment>();},
  environmentAllNumbers, environmentBooleans, environmentEnums, nullptr, false, {}, environmentTriples,
  environmentResources
};

} // namespace ae::scene

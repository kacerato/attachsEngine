#include "harness.h"

#include "editor/editor_archive.h"
#include "editor/editor_component_catalog.h"
#include "editor/editor_document.h"
#include "runtime/scene_environment.h"
#include "renderer/physical_atmosphere.h"
#include "resources/environment_profile.h"
#include "scene/component_schema.h"
#include "scene/component_properties.h"
#include "scene/environment.h"
#include "environment_legacy_lines.h"

#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

using namespace ae;
using namespace ae::editor;

namespace {
EditorEntityId environmentAt(EditorDocument &document, EditorEntityId parent,
                             const char *name, float priority, float exposure) {
  const auto id=document.createEntity(parent,EditorEntityKind::Folder,name);
  auto values=*document.find(id);
  auto *component=static_cast<scene::Environment *>(
      values.components.add(scene::Environment::descriptor));
  if(!component) return 0;
  component->values.priority=priority;
  component->values.exposureCompensation=exposure;
  return document.applyEntityValues(id,values)?id:0;
}
} // namespace

AE_TEST(scene_environment_has_one_authoring_contract_and_archive_identity) {
  const auto *schema=scene::findComponentSchema("astra.render.environment");
  AE_EXPECT_TRUE(schema!=nullptr,"ambiente tem schema");
  AE_EXPECT_TRUE(findEditorComponent("astra.render.environment")!=nullptr,
                 "ambiente aparece no catálogo do Inspector");
  AE_EXPECT_TRUE(schema->propertiesInPlay==scene::PlayMutability::SafePoint,
                 "aparência pode mudar durante Play no ponto seguro");

  EditorDocument document;
  const auto id=environmentAt(document,document.root(),"Ambiente",7.0f,1.25f);
  AE_EXPECT_TRUE(id!=0,"ambiente criado");
  auto values=*document.find(id);
  auto *environment=static_cast<scene::Environment*>(values.components.edit(scene::Environment::descriptor));
  environment->values.ambientOcclusion=true;environment->values.ambientOcclusionRadius=2.25f;
  environment->values.filmGrain=true;environment->values.filmGrainIntensity=.22f;
  environment->values.sky=renderer::SkyModel::PhysicalAtmosphere;
  environment->values.toneMapper=renderer::ToneMapper::AgX;
  environment->values.physicalAtmosphereHighQuality=true;
  environment->values.aerosolDensity=2.25f;
  environment->values.aerosolAnisotropy=.84f;
  environment->values.fog=true;environment->values.fogBaseHeight=12.5f;
  environment->values.fogHeightFalloff=.075f;
  environment->values.fogLightEnergy=100.0f;
  environment->values.exposureMode=renderer::ExposureMode::AutomaticHistogram;
  environment->values.exposureMetering=renderer::ExposureMetering::Spot;
  environment->values.exposureAdaptation=renderer::ExposureAdaptation::Fixed;
  environment->values.fixedExposure=2.5f;
  environment->values.exposureLimitMin=-3.0f;
  environment->values.exposureLimitMax=4.0f;
  environment->values.exposureHistogramLow=10.0f;
  environment->values.exposureHistogramHigh=90.0f;
  environment->values.exposureTargetMidGrey=.14f;
  environment->values.exposureSpeedDarkToLight=1.5f;
  environment->values.exposureSpeedLightToDark=2.5f;
  environment->overrideBloom=false;environment->overrideExposure=true;
  environment->values.environmentMap=resources::assetGuidFromSeed("panorama-patio");
  environment->values.hdriRotationDegrees=127.5f;environment->values.hdriExposureEv=-2.25f;
  environment->profile=resources::assetGuidFromSeed("perfil-noite");
  AE_EXPECT_TRUE(document.applyEntityValues(id,values),"AO e perfil configurados");
  const auto archive=serializeEditorDocument(document,9);
  EditorDocument restored;
  AE_EXPECT_TRUE(deserializeEditorDocument(archive,9,restored),"cena relida");
  const auto *back=static_cast<const scene::Environment *>(
      restored.find(id)->components.find(scene::Environment::descriptor));
  AE_EXPECT_TRUE(back!=nullptr,"componente preservado");
  AE_EXPECT_TRUE(back->values.toneMapper==renderer::ToneMapper::AgX,"AgX preservado na cena");
  AE_EXPECT_EQ(back->values.exposureCompensation,1.25f,"compensação preservada");
  AE_EXPECT_EQ(back->values.priority,7.0f,"prioridade preservada");
  AE_EXPECT_TRUE(back->values.ambientOcclusion&&back->values.ambientOcclusionRadius==2.25f,
                 "parâmetros de AO preservados");
  AE_EXPECT_TRUE(back->values.filmGrain&&back->values.filmGrainIntensity==.22f,
                 "override de grão preservado");
  AE_EXPECT_TRUE(back->values.sky==renderer::SkyModel::PhysicalAtmosphere&&
                 back->values.physicalAtmosphereHighQuality&&
                 back->values.aerosolDensity==2.25f&&back->values.aerosolAnisotropy==.84f,
                 "atmosfera física preservada no componente v7");
  AE_EXPECT_TRUE(back->profile==environment->profile,"identidade do perfil preservada");
  AE_EXPECT_TRUE(back->values.environmentMap==environment->values.environmentMap&&
                 back->values.hdriRotationDegrees==127.5f&&back->values.hdriExposureEv==-2.25f,
                 "HDRI e seus controles sobrevivem ao arquivo de cena");
  AE_EXPECT_TRUE(back->values.fog&&back->values.fogLightEnergy==100.0f&&back->values.fogBaseHeight==12.5f&&
                 back->values.fogHeightFalloff==.075f,
                 "neblina por altura sobrevive ao contrato autoral único");
  AE_EXPECT_TRUE(back->values.exposureMode==renderer::ExposureMode::AutomaticHistogram&&
                 back->values.exposureMetering==renderer::ExposureMetering::Spot&&
                 back->values.exposureAdaptation==renderer::ExposureAdaptation::Fixed&&
                 back->values.fixedExposure==2.5f&&back->values.exposureLimitMin==-3.0f&&
                 back->values.exposureLimitMax==4.0f&&back->values.exposureHistogramLow==10.0f&&
                 back->values.exposureHistogramHigh==90.0f&&back->values.exposureTargetMidGrey==.14f&&
                 back->values.exposureSpeedDarkToLight==1.5f&&back->values.exposureSpeedLightToDark==2.5f,
                 "Exposure do HDRP sobrevive ao arquivo de cena");
  AE_EXPECT_TRUE(!back->overrideBloom&&back->overrideExposure&&back->overridePost,
                 "cada override de pós é uma caixa própria persistida");
}

AE_TEST(environment_profile_round_trips_and_local_values_form_an_explicit_override) {
  resources::EnvironmentProfile profile;profile.guid=resources::assetGuidFromSeed("ambiente-compartilhado");
  profile.name="Caverna úmida";profile.values.active=true;profile.values.post=true;
  profile.values.ambientOcclusion=true;profile.values.ambientOcclusionRadius=2.5f;
  profile.values.filmGrain=true;profile.values.filmGrainIntensity=.17f;
  profile.values.sky=renderer::SkyModel::PhysicalAtmosphere;
  profile.values.toneMapper=renderer::ToneMapper::AgX;
  profile.values.physicalAtmosphereHighQuality=true;
  profile.values.physicalSkyIntensity=1.7f;profile.values.aerosolDensity=2.1f;
  profile.values.aerosolAnisotropy=.88f;profile.values.observerHeightKm=.12f;
  profile.values.ambientOcclusionIntensity=1.75f;profile.values.exposureCompensation=-.5f;
  profile.values.environmentMap=resources::assetGuidFromSeed("panorama-perfil");
  profile.values.hdriRotationDegrees=45;profile.values.hdriExposureEv=1.5f;
  profile.values.fog=true;profile.values.fogBaseHeight=-18.0f;
  profile.values.fogHeightFalloff=.04f;
  profile.values.fogLightEnergy=100.0f;
  profile.values.exposureMode=renderer::ExposureMode::AutomaticHistogram;
  profile.values.exposureMetering=renderer::ExposureMetering::Average;
  profile.values.exposureLimitMin=-5;profile.values.exposureLimitMax=6;
  profile.values.exposureHistogramLow=12;profile.values.exposureHistogramHigh=88;
  profile.values.exposureTargetMidGrey=.18f;profile.values.fixedExposure=3;
  profile.values.exposureSpeedDarkToLight=4;profile.values.exposureSpeedLightToDark=1.5f;
  resources::EnvironmentProfile restored;
  AE_EXPECT_TRUE(profile.valid()&&resources::EnvironmentProfile::deserialize(profile.serialize(),restored),
                 "perfil válido relê do arquivo próprio");
  AE_EXPECT_TRUE(restored.guid==profile.guid&&restored.values.toneMapper==renderer::ToneMapper::AgX&&restored.values.ambientOcclusionRadius==2.5f&&
                 restored.values.filmGrain&&restored.values.filmGrainIntensity==.17f&&
                 restored.values.sky==renderer::SkyModel::PhysicalAtmosphere&&
                 restored.values.physicalAtmosphereHighQuality&&
                 restored.values.physicalSkyIntensity==1.7f&&restored.values.aerosolDensity==2.1f&&
                 restored.values.aerosolAnisotropy==.88f&&restored.values.observerHeightKm==.12f,
                 "identidade e aparência voltam iguais");
  AE_EXPECT_TRUE(restored.values.fog&&restored.values.fogLightEnergy==100.0f&&
                 restored.values.fogBaseHeight==-18.0f&&
                 restored.values.fogHeightFalloff==.04f,
                 "perfil compartilhado preserva a distribuição vertical da neblina");
  AE_EXPECT_TRUE(restored.values.environmentMap==profile.values.environmentMap&&
                 restored.values.hdriRotationDegrees==45&&restored.values.hdriExposureEv==1.5f,
                 "perfil v4 preserva panorama e radiância");
  AE_EXPECT_TRUE(restored.values.exposureMode==renderer::ExposureMode::AutomaticHistogram&&
                 restored.values.exposureMetering==renderer::ExposureMetering::Average&&
                 restored.values.exposureLimitMin==-5&&restored.values.exposureLimitMax==6&&
                 restored.values.exposureHistogramLow==12&&restored.values.exposureHistogramHigh==88&&
                 restored.values.exposureTargetMidGrey==.18f&&restored.values.fixedExposure==3&&
                 restored.values.exposureSpeedDarkToLight==4&&restored.values.exposureSpeedLightToDark==1.5f&&
                 restored.values.exposureCompensation==-.5f,
                 "perfil v11 preserva a Exposure do HDRP");

  EditorDocument document;const auto id=environmentAt(document,document.root(),"Volume",9,profile.values.exposureCompensation);
  auto values=*document.find(id);auto *environment=static_cast<scene::Environment*>(
      values.components.edit(scene::Environment::descriptor));
  environment->profile=profile.guid;environment->values=resources::applyEnvironmentProfile(environment->values,profile);
  AE_EXPECT_TRUE(document.applyEntityValues(id,values),"perfil atribuído");
  std::vector<renderer::SceneEnvironmentVolume> volumes;
  AE_EXPECT_TRUE(runtime::collectSceneEnvironmentVolumes(document,volumes,std::span{&profile,usize{1}}),
                 "perfil resolvido pelo runtime");
  AE_EXPECT_TRUE(volumes.size()==1&&volumes[0].environment.ambientOcclusion&&
                 volumes[0].environment.priority==9,"aparência compartilhada preserva prioridade da instância");

  values=*document.find(id);environment=static_cast<scene::Environment*>(
      values.components.edit(scene::Environment::descriptor));
  environment->values.exposureCompensation=1.25f;
  AE_EXPECT_TRUE(document.applyEntityValues(id,values),"override local editado");
  AE_EXPECT_TRUE(runtime::collectSceneEnvironmentVolumes(document,volumes,std::span{&profile,usize{1}})&&
                 volumes[0].environment.exposureCompensation==1.25f,"diferença local explícita vence o perfil");
}

AE_TEST(auto_exposure_properties_reject_invalid_pairs_without_mutation) {
  scene::Components components;
  AE_EXPECT_TRUE(components.add(scene::Environment::descriptor)!=nullptr,"ambiente anexado");
  const auto set=[&](std::string_view id,float value) {
    return scene::setComponentProperty(components,"astra.render.environment",id,value);
  };
  AE_EXPECT_TRUE(set("exposure_limit_min",-3.0f)==scene::ComponentPropertyStatus::Applied,
                 "limite inferior editável pelo contrato comum");
  AE_EXPECT_TRUE(set("exposure_limit_max",-4.0f)==scene::ComponentPropertyStatus::InvalidValue,
                 "limites invertidos recusados");
  AE_EXPECT_TRUE(set("exposure_histogram_low",96.0f)==scene::ComponentPropertyStatus::InvalidValue,
                 "percentis invertidos recusados");
  const auto *environment=static_cast<const scene::Environment*>(components.find(scene::Environment::descriptor));
  AE_EXPECT_TRUE(environment&&environment->values.exposureLimitMin==-3.0f&&
                 environment->values.exposureLimitMax==renderer::SceneEnvironment{}.exposureLimitMax&&
                 environment->values.exposureHistogramLow==40.0f,
                 "candidato inválido não alterou o componente");
}

// Números da exposição automática (v11): não existiam antes; exposure_fixed é da v16.
static bool legacyExposureNumber(std::string_view id) {
  return id.starts_with("exposure_")&&id!="exposure_compensation";
}

AE_TEST(scene_environment_v6_archive_keeps_physical_atmosphere_defaults) {
  scene::Environment source;
  source.values.active=true;source.overrideIndirect=false;
  std::ostringstream old;
  old<<source.values.active<<' '<<source.values.fog<<' '<<source.values.post<<' '
     <<source.values.bloom<<' '<<source.values.vignette<<' '<<source.values.filmGrain<<' '
     <<static_cast<u32>(source.values.sky)<<' '<<static_cast<u32>(source.values.toneMapper)<<' '
     <<source.values.ambientOcclusion<<' '<<static_cast<u32>(source.shape)<<' '
     <<source.overrideSky<<' '<<source.overrideFog<<' '<<source.overridePost<<' '
     <<source.layer<<" - "<<source.overrideIndirect;
  for(const auto &property:scene::Environment::descriptor.numbers) {
    if(property.id.starts_with("hdri_")||legacyExposureNumber(property.id)||
       property.id=="fog_base_height"||property.id=="fog_height_falloff"||
       property.id=="fog_light_energy"||
       property.id.starts_with("physical_")||property.id.starts_with("air_")||
       property.id.starts_with("aerosol_")||property.id.starts_with("planet_")||
       property.id.starts_with("observer_")||property.id.starts_with("rayleigh_")||
       property.id=="atmosphere_height_km"||property.id=="ground_albedo") continue;
    old<<' '<<property.read(source);
  }
  scene::Environment restored;restored.values.aerosolDensity=7.0f;
  std::istringstream input(old.str());
  AE_EXPECT_TRUE(restored.read(input,6),"componente v6 continua legível");
  AE_EXPECT_TRUE(restored.values.sky==renderer::SkyModel::Atmosphere&&
                 !restored.values.physicalAtmosphereHighQuality&&
                 restored.values.aerosolDensity==1.0f&&restored.values.planetRadiusKm==6371.0f&&
                 restored.values.fogBaseHeight==0.0f&&restored.values.fogHeightFalloff==0.0f&&
                 restored.values.fogLightEnergy==1.0f,
                 "campos físicos novos recebem padrões terrestres compatíveis");
}

AE_TEST(environment_profile_v2_keeps_physical_atmosphere_defaults) {
  const auto guid=resources::assetGuidFromSeed("perfil-legado");
  std::ostringstream old;
  old<<"ASTRA_ENVIRONMENT_PROFILE 2 "<<guid.text()<<" 1 \"Legado\" "
     <<"0 1 1 0 0 0 1 1 "
     <<"0.025 0.10 0.32 0.28 0.42 0.62 0.11 0.12 0.14 "
     <<"1 0.53 8 0.58 0.67 0.76 0.008 8 0 1 0.1 1 1 0.18 0.05 1 1 1.5 0.02";
  resources::EnvironmentProfile restored;
  AE_EXPECT_TRUE(resources::EnvironmentProfile::deserialize(old.str(),restored),
                 "perfil v2 continua legível");
  AE_EXPECT_TRUE(restored.values.sky==renderer::SkyModel::Atmosphere&&
                 !restored.values.physicalAtmosphereHighQuality&&
                 restored.values.aerosolDensity==1.0f&&restored.values.planetRadiusKm==6371.0f&&
                 restored.values.fogBaseHeight==0.0f&&restored.values.fogHeightFalloff==0.0f&&
                 restored.values.fogLightEnergy==1.0f,
                 "perfil legado recebe padrões terrestres sem mudar o céu selecionado");
}

AE_TEST(daylight_authoring_colours_survive_profile_archive_and_runtime_resolution) {
  EditorDocument document;scene::Environment environment;
  // Como o Skybox/Procedural da Unity: Sky Tint neutro (0,5) e Ground cinza quente.
  AE_EXPECT_TRUE(environment.values.sky==renderer::SkyModel::Atmosphere&&
      environment.values.skyZenith[0]==.5f&&environment.values.skyZenith[2]==.5f&&
      environment.values.ground[0]>environment.values.ground[2],"new procedural environments use Unity's neutral tint and ground");
  resources::EnvironmentProfile profile;profile.guid=resources::assetGuidFromSeed("daylight-profile");
  profile.name="Dia claro";profile.values=environment.values;resources::EnvironmentProfile restored;
  AE_EXPECT_TRUE(resources::EnvironmentProfile::deserialize(profile.serialize(),restored),"profile roundtrip");
  const auto id=document.createEntity(document.root(),runtime::ObjectKind::Folder,"Céu autorado");
  auto value=*document.find(id);auto *component=static_cast<scene::Environment*>(value.components.add(scene::Environment::descriptor));
  component->values=restored.values;
  // An existing project's deliberately dark colour must remain authored data.
  component->values.skyZenith[0]=.025f;component->values.skyZenith[1]=.10f;component->values.skyZenith[2]=.32f;
  AE_EXPECT_TRUE(document.applyEntityValues(id,value),"environment component");
  EditorDocument reopened;AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(document,0),0,reopened),"scene roundtrip");
  const auto *saved=static_cast<const scene::Environment*>(reopened.find(id)->components.find(scene::Environment::descriptor));
  AE_EXPECT_TRUE(saved&&saved->values.skyZenith[2]==.32f&&saved->values.ground[2]==renderer::SceneEnvironment{}.ground[2],
                 "loading preserves custom colours rather than replacing with defaults");
  renderer::SceneEnvironmentVolume volume;volume.environment=saved->values;const float camera[]{0,0,0};
  const auto resolved=renderer::resolveSceneEnvironment(std::span(&volume,1),camera,~u32(0));
  AE_EXPECT_TRUE(resolved.active&&resolved.skyZenith[2]==.32f&&resolved.ground[2]==renderer::SceneEnvironment{}.ground[2],
                 "runtime volume resolver consumes the persisted colours");
}
AE_TEST(scene_default_is_the_component_default_and_is_valid) {
  const renderer::SceneEnvironment authored{};
  const auto view=renderer::defaultSceneEnvironment();
  AE_EXPECT_TRUE(!authored.active&&view.active,"o padrão da cena é o contrato do componente, ativo");
  auto same=view;same.active=false;
  AE_EXPECT_TRUE(view.valid()&&same.post==authored.post&&same.sky==authored.sky&&same.exposureCompensation==authored.exposureCompensation,
                 "editor e Play usam o mesmo padrão, sem ajuste exclusivo da Scene View");
}

AE_TEST(hdri_volume_selection_keeps_resource_rotation_and_exposure_together) {
  renderer::SceneEnvironmentVolume base;base.stableId=1;base.environment.active=true;
  base.environment.sky=renderer::SkyModel::Hdri;
  base.environment.environmentMap=resources::assetGuidFromSeed("exterior-hdr");
  base.environment.hdriRotationDegrees=30;base.environment.hdriExposureEv=2;
  auto local=base;local.stableId=2;local.environment.priority=1;local.weight=.75f;
  local.environment.environmentMap=resources::assetGuidFromSeed("interior-hdr");
  local.environment.hdriRotationDegrees=-70;local.environment.hdriExposureEv=-1;
  base.environment.fogBaseHeight=0;base.environment.fogHeightFalloff=0;
  base.environment.fogLightEnergy=1.0f;
  local.environment.fogBaseHeight=40;local.environment.fogHeightFalloff=.08f;
  local.environment.fogLightEnergy=101.0f;
  local.overrides=renderer::EnvironmentOverrideFog;
  std::array volumes{base,local};const float camera[3]{};
  auto result=renderer::resolveSceneEnvironment(volumes,camera);
  AE_EXPECT_TRUE(result.environmentMap==base.environment.environmentMap&&result.hdriRotationDegrees==30&&
                 result.hdriExposureEv==2,"volume de neblina não troca a fonte de luz");
  AE_EXPECT_TRUE(result.fogBaseHeight==30&&std::abs(result.fogHeightFalloff-.06f)<1e-6f&&
                 result.fogLightEnergy==76.0f,
                 "override de neblina mistura base e decaimento por altura");
  volumes[1].overrides=renderer::EnvironmentOverrideSky;
  result=renderer::resolveSceneEnvironment(volumes,camera);
  AE_EXPECT_TRUE(result.environmentMap==local.environment.environmentMap&&result.hdriRotationDegrees==-70&&
                 result.hdriExposureEv==-1,"panorama troca com sua orientação e exposição");
  volumes[1].weight=.25f;result=renderer::resolveSceneEnvironment(volumes,camera);
  // VolumeParameter.Interp da Unity: o que não interpola troca assim que t > 0.
  AE_EXPECT_TRUE(result.environmentMap==local.environment.environmentMap&&result.hdriRotationDegrees==-70,
                 "qualquer influência troca o mapa junto com sua orientação");
  volumes[1].weight=0.0f;result=renderer::resolveSceneEnvironment(volumes,camera);
  AE_EXPECT_TRUE(result.environmentMap==base.environment.environmentMap&&result.hdriRotationDegrees==30,
                 "peso zero conserva o mapa anterior e sua orientação");
}

// Campos de cena v13 (gradação) e v14 (sol, lua e estrelas): um arquivo antigo não os tem.
static bool newerThanV12(std::string_view id) {
  return id.starts_with("grading_")||id.starts_with("cloud_")||id=="sun_glare"||id.starts_with("moon_")||id=="star_intensity"||
         scene::environmentNumberSinceV17(id)||scene::environmentNumberSinceV18(id);
}

AE_TEST(environment_v7_migrates_hdri_without_consuming_old_numeric_fields) {
  scene::Environment source;source.values.exposureCompensation=2.25f;source.values.groundAlbedo=.3f;
  std::ostringstream legacy;
  legacy<<source.values.active<<' '<<source.values.fog<<' '<<source.values.post<<' '
        <<source.values.bloom<<' '<<source.values.vignette<<' '<<source.values.filmGrain<<' '
        <<static_cast<u32>(source.values.sky)<<' '<<static_cast<u32>(source.values.toneMapper)<<' '
        <<source.values.ambientOcclusion<<' '<<static_cast<u32>(source.shape)<<' '
        <<source.overrideSky<<' '<<source.overrideFog<<' '<<source.overridePost<<' '
        <<source.layer<<" - "<<source.overrideIndirect<<' '<<source.values.physicalAtmosphereHighQuality;
  for(const auto &property:scene::Environment::descriptor.numbers) {
    if(property.id.starts_with("hdri_")||legacyExposureNumber(property.id)||
       property.id=="fog_base_height"||property.id=="fog_height_falloff"||
       property.id=="fog_light_energy"||newerThanV12(property.id)) continue;
    legacy<<' '<<property.read(source);
  }
  scene::Environment restored;restored.values.hdriRotationDegrees=88;
  std::istringstream input(legacy.str());
  AE_EXPECT_TRUE(restored.read(input,7),"v7 reste legível");
  input>>std::ws;
  // A exposição antiga (+2,25 EV de clareamento) vira Fixed com o mesmo multiplicador.
  AE_EXPECT_TRUE(input.eof()&&!restored.values.environmentMap.valid()&&restored.values.hdriRotationDegrees==0&&
                 restored.values.exposureMode==renderer::ExposureMode::Fixed&&
                 std::abs(restored.values.fixedBrighteningEv()-2.25f)<1e-5f&&restored.values.groundAlbedo==.3f,
                 "migração consome todos os campos legados e aplica HDRI neutro");
}

AE_TEST(auto_exposure_migrates_scene_v10_and_profile_v6_disabled) {
  scene::Environment source;source.values.exposureCompensation=1.25f;
  std::ostringstream old;
  old<<source.values.active<<' '<<source.values.fog<<' '<<source.values.post<<' '
     <<source.values.bloom<<' '<<source.values.vignette<<' '<<source.values.filmGrain<<' '
     <<static_cast<u32>(source.values.sky)<<' '<<static_cast<u32>(source.values.toneMapper)<<' '
     <<source.values.ambientOcclusion<<' '<<static_cast<u32>(source.shape)<<' '
     <<source.overrideSky<<' '<<source.overrideFog<<' '<<source.overridePost<<' '
     <<source.layer<<" - "<<source.overrideIndirect<<' '<<source.values.physicalAtmosphereHighQuality
     <<" -";
  for(const auto &property:scene::Environment::descriptor.numbers)
    if(!legacyExposureNumber(property.id)&&property.id!="fog_light_energy"&&!newerThanV12(property.id))
      old<<' '<<property.read(source);
  scene::Environment restored;restored.values.exposureMode=renderer::ExposureMode::AutomaticHistogram;
  restored.values.exposureLimitMin=-12;restored.values.exposureMetering=renderer::ExposureMetering::Spot;
  std::istringstream input(old.str());
  AE_EXPECT_TRUE(restored.read(input,10),"cena v10 relida");
  input>>std::ws;
  const renderer::SceneEnvironment defaults{};
  AE_EXPECT_TRUE(input.eof()&&restored.values.exposureMode==renderer::ExposureMode::Fixed&&
                 restored.values.exposureLimitMin==defaults.exposureLimitMin&&
                 restored.values.exposureLimitMax==defaults.exposureLimitMax&&
                 restored.values.exposureHistogramLow==defaults.exposureHistogramLow&&
                 restored.values.exposureMetering==defaults.exposureMetering&&
                 restored.values.exposureCompensation==0&&
                 std::abs(restored.values.fixedBrighteningEv()-1.25f)<1e-5f&&
                 restored.values.fogLightEnergy==1.0f,
                 "cena sem medição vira Fixed com a mesma imagem");

  resources::EnvironmentProfile profile;
  profile.guid=resources::assetGuidFromSeed("perfil-exposicao-legado");
  profile.name="Legado";profile.values.exposureMode=renderer::ExposureMode::AutomaticHistogram;
  std::istringstream profileTokens(profile.serialize());
  std::vector<std::string> fields;
  for(std::string field;profileTokens>>field;) fields.push_back(field);
  AE_EXPECT_TRUE(fields.size()>12,"perfil v7 gerado");
  // v7..v13 acrescentaram no fim: 11 de exposição (v12; eram 9 na v7) + energia
  // da neblina (v8), 31 de gradação (v9), 5 dos corpos celestes (v10), 16 das nuvens (v11),
  // 10 de Color Adjustments/Bloom (v12) e 21 da janela Lighting (v13).
  fields[1]="6";fields.resize(fields.size()-95);
  std::ostringstream legacy;
  for(const auto &field:fields) legacy<<field<<' ';
  resources::EnvironmentProfile migrated;
  AE_EXPECT_TRUE(resources::EnvironmentProfile::deserialize(legacy.str(),migrated),"perfil v6 relido");
  AE_EXPECT_TRUE(migrated.values.exposureMode==renderer::ExposureMode::Fixed&&
                 migrated.values.fogLightEnergy==1.0f&&
                 migrated.values.exposureLimitMin==renderer::SceneEnvironment{}.exposureLimitMin&&
                 std::abs(migrated.values.fixedBrighteningEv())<1e-5f&&migrated.values.exposureCompensation==0,
                 "perfil antigo recebe exposição fixa com a mesma imagem");
}

AE_TEST(auto_exposure_v15_scene_migrates_to_hdrp_histogram_in_ev100) {
  // Arquivo v15: chaves autoExposure/centro no cabeçalho e números antigos
  // (EV de clareamento, percentis em fração, velocidades lineares).
  scene::Environment source;source.values.exposureCompensation=.5f;source.overridePost=false;
  source.values.exposureLimitMin=-8;source.values.exposureLimitMax=6;
  source.values.exposureHistogramLow=.05f;source.values.exposureHistogramHigh=.95f;
  source.values.exposureTargetMidGrey=.18f;
  source.values.exposureSpeedDarkToLight=2;source.values.exposureSpeedLightToDark=3; // ordem antiga: subir, descer
  std::ostringstream old;
  old<<source.values.active<<' '<<source.values.fog<<' '<<source.values.post<<' '
     <<source.values.bloom<<' '<<source.values.vignette<<' '<<source.values.filmGrain<<' '
     <<static_cast<u32>(source.values.sky)<<' '<<static_cast<u32>(source.values.toneMapper)<<' '
     <<source.values.ambientOcclusion<<' '<<static_cast<u32>(source.shape)<<' '
     <<source.overrideSky<<' '<<source.overrideFog<<' '<<source.overridePost<<' '
     <<source.layer<<" - "<<source.overrideIndirect<<' '<<source.values.physicalAtmosphereHighQuality
     <<" - 1 1";
  for(const auto &property:scene::Environment::descriptor.numbers)
    if(property.id!="exposure_fixed"&&!property.id.starts_with("cloud_")&&!scene::environmentNumberSinceV17(property.id)&&
       !scene::environmentNumberSinceV18(property.id))
      old<<' '<<property.read(source);
  scene::Environment restored;
  std::istringstream input(old.str());
  AE_EXPECT_TRUE(restored.read(input,15),"cena v15 relida");
  input>>std::ws;
  const float toEv100=std::log2(1.2f);
  const auto &v=restored.values;
  AE_EXPECT_TRUE(input.eof()&&v.exposureMode==renderer::ExposureMode::AutomaticHistogram&&
                 v.exposureMetering==renderer::ExposureMetering::CenterWeighted&&
                 v.exposureCompensation==.5f,"medição antiga vira Automatic Histogram com peso no centro");
  AE_EXPECT_TRUE(std::abs(v.exposureLimitMin-(-6-toEv100))<1e-5f&&std::abs(v.exposureLimitMax-(8-toEv100))<1e-5f,
                 "limites de clareamento viram EV100 e trocam de ponta");
  AE_EXPECT_TRUE(std::abs(v.exposureHistogramLow-5)<1e-4f&&std::abs(v.exposureHistogramHigh-95)<1e-4f&&
                 v.exposureTargetMidGrey==.18f,"percentis em fração viram porcentagem");
  AE_EXPECT_TRUE(v.exposureSpeedDarkToLight==3&&v.exposureSpeedLightToDark==2,
                 "velocidade de clarear (cena escureceu) vira Light to Dark");
  AE_EXPECT_TRUE(!restored.overrideExposure&&!restored.overrideBloom&&!restored.overrideColorGrading&&
                 !restored.overrideAmbientOcclusion,"override de pós antigo vale para cada override novo");
}

AE_TEST(exposure_defaults_follow_hdrp_default_volume_profile) {
  // HDRP 17: Exposure.cs e DefaultSettingsVolumeProfile.asset (mode Automatic).
  const renderer::SceneEnvironment world{};
  AE_EXPECT_TRUE(world.exposureMode==renderer::ExposureMode::Automatic&&
                 world.exposureMetering==renderer::ExposureMetering::CenterWeighted&&
                 world.exposureAdaptation==renderer::ExposureAdaptation::Progressive,
                 "Automatic, Center Weighted e Progressive");
  AE_EXPECT_TRUE(world.fixedExposure==0&&world.exposureCompensation==0&&
                 world.exposureSpeedDarkToLight==3&&world.exposureSpeedLightToDark==1&&
                 world.exposureHistogramLow==40&&world.exposureHistogramHigh==90&&
                 world.exposureTargetMidGrey==.125f,"valores do Exposure.cs");
  // EV100 = log2(L·100/K) com K = 12,5: a luminância média vai a 12,5%/1,2.
  const float ev=renderer::exposureEv100FromLuminance(.5f,.125f);
  AE_EXPECT_TRUE(std::abs(ev-2.0f)<1e-6f,"0,5 cd/m² mede EV100 2 com K 12,5");
  AE_EXPECT_TRUE(std::abs(.5f*renderer::exposureMultiplierFromEv100(ev)-.125f/1.2f)<1e-6f,
                 "a média exposta é o cinza alvo / 1,2");
  AE_EXPECT_TRUE(std::abs(std::exp2(renderer::exposureBrighteningEvFromEv100(ev))-
                          renderer::exposureMultiplierFromEv100(ev))<1e-6f,
                 "o pós aplica o mesmo multiplicador por exp2");
  // ComputeLuminanceAdaptation: metade do caminho em 1/velocidade segundos.
  AE_EXPECT_TRUE(std::abs(renderer::adaptExposureEv100(0,4,1.0f/3.0f,3,1)-2)<1e-5f,
                 "escuro para claro usa Speed Dark to Light");
  AE_EXPECT_TRUE(std::abs(renderer::adaptExposureEv100(4,0,1.0f,3,1)-2)<1e-5f,
                 "claro para escuro usa Speed Light to Dark");
}

AE_TEST(exposure_override_unchecked_inherits_from_lower_priority_volume) {
  // Volume da Unity: sem a caixa de Exposure, o volume local não decide a
  // exposição; o global de prioridade menor continua valendo.
  renderer::SceneEnvironmentVolume global;global.stableId=1;global.environment.active=true;
  global.environment.exposureMode=renderer::ExposureMode::AutomaticHistogram;
  global.environment.exposureLimitMin=-3;global.environment.bloomIntensity=.2f;
  auto local=global;local.stableId=2;local.environment.priority=5;
  local.environment.exposureMode=renderer::ExposureMode::Fixed;local.environment.exposureLimitMin=-9;
  local.environment.bloomIntensity=1.0f;
  local.overrides=renderer::EnvironmentOverrideAll&~renderer::EnvironmentOverrideExposure;
  std::array volumes{global,local};const float camera[3]{};
  auto result=renderer::resolveSceneEnvironment(volumes,camera);
  AE_EXPECT_TRUE(result.exposureMode==renderer::ExposureMode::AutomaticHistogram&&result.exposureLimitMin==-3,
                 "exposição herdada do volume global");
  AE_EXPECT_EQ(result.bloomIntensity,1.0f,"bloom marcado vem do volume local");
  volumes[1].overrides=renderer::EnvironmentOverrideExposure;
  result=renderer::resolveSceneEnvironment(volumes,camera);
  AE_EXPECT_TRUE(result.exposureMode==renderer::ExposureMode::Fixed&&result.exposureLimitMin==-9&&
                 result.bloomIntensity==.2f,"só a Exposure marcada troca; o bloom volta a herdar");
  // Sem nenhum volume decidindo a exposição, vale o padrão do mundo (Automatic).
  volumes[0].overrides=renderer::EnvironmentOverrideBloom;volumes[1].overrides=renderer::EnvironmentOverrideBloom;
  result=renderer::resolveSceneEnvironment(volumes,camera);
  AE_EXPECT_TRUE(result.exposureMode==renderer::ExposureMode::Automatic&&
                 result.exposureLimitMin==renderer::SceneEnvironment{}.exposureLimitMin,
                 "ninguém sobrescreve: padrão do Default Volume Profile");
}

AE_TEST(physical_atmosphere_volume_blends_scalars_and_selects_quality_discretely) {
  renderer::SceneEnvironmentVolume base;base.environment.active=true;base.stableId=1;
  base.environment.sky=renderer::SkyModel::PhysicalAtmosphere;
  base.environment.physicalSkyIntensity=1.0f;base.environment.aerosolDensity=1.0f;
  renderer::SceneEnvironmentVolume local=base;local.stableId=2;local.environment.priority=1.0f;
  local.environment.physicalSkyIntensity=3.0f;local.environment.aerosolDensity=5.0f;
  local.environment.physicalAtmosphereHighQuality=true;local.weight=.25f;
  std::array volumes{base,local};const float camera[3]{};
  auto result=renderer::resolveSceneEnvironment(volumes,camera);
  AE_EXPECT_EQ(result.physicalSkyIntensity,1.5f,"intensidade mistura com o peso do volume");
  AE_EXPECT_EQ(result.aerosolDensity,2.0f,"aerossóis misturam continuamente");
  AE_EXPECT_TRUE(result.physicalAtmosphereHighQuality,
                 "qualidade discreta troca com qualquer influência (t > 0, como na Unity)");
  volumes[1].weight=.75f;result=renderer::resolveSceneEnvironment(volumes,camera);
  AE_EXPECT_EQ(result.physicalSkyIntensity,2.5f,"intensidade acompanha a influência atualizada");
  AE_EXPECT_TRUE(result.physicalAtmosphereHighQuality,
                 "qualidade discreta continua trocada com mais peso");
}

AE_TEST(physical_atmosphere_ground_irradiance_is_finite_and_cached_by_inputs) {
  renderer::PhysicalAtmosphereGroundIrradianceInput input;
  input.sunIntensity=100000.0f;
  float irradiance[3]{};
  AE_EXPECT_TRUE(renderer::computePhysicalAtmosphereGroundIrradiance(input,irradiance),
                 "integração atmosférica aceita parâmetros terrestres padrão");
  AE_EXPECT_TRUE(std::isfinite(irradiance[0])&&std::isfinite(irradiance[1])&&
                 std::isfinite(irradiance[2])&&irradiance[0]>0.0f&&
                 irradiance[1]>irradiance[0]&&irradiance[2]>irradiance[1],
                 "irradiância é finita, positiva e conserva espalhamento espectral");

  renderer::PhysicalAtmosphereGroundIrradianceCache cache;
  float cached[3]{};
  AE_EXPECT_TRUE(cache.resolve(input,cached)&&cache.revision()==1,
                 "primeira resolução calcula e publica o valor");
  AE_EXPECT_TRUE(cache.resolve(input,cached)&&cache.revision()==1,
                 "entrada idêntica reutiliza a integração");
  input.aerosolDensity=2.0f;
  AE_EXPECT_TRUE(cache.resolve(input,cached)&&cache.revision()==2,
                 "mudança física invalida o cache");
  input.sunIntensity=0.0f;
  AE_EXPECT_TRUE(cache.resolve(input,cached)&&cache.revision()==3&&
                 cached[0]==0.0f&&cached[1]==0.0f&&cached[2]==0.0f,
                 "sol desligado produz irradiância nula sem resíduo do cache");
}

AE_TEST(scene_environment_priority_and_hierarchy_choose_one_result) {
  EditorDocument document;
  const auto low=environmentAt(document,document.root(),"Dia",1.0f,-1.0f);
  const auto group=document.createEntity(document.root(),EditorEntityKind::Folder,"Clima");
  const auto high=environmentAt(document,group,"Chuva",5.0f,2.0f);
  AE_EXPECT_TRUE(low&&high,"dois ambientes criados");

  renderer::SceneEnvironment selected{};
  AE_EXPECT_TRUE(runtime::collectSceneEnvironment(document,selected),"coleta concluída");
  AE_EXPECT_TRUE(selected.active,"há ambiente ativo");
  AE_EXPECT_EQ(selected.exposureCompensation,2.0f,"maior prioridade vence");

  auto groupValues=*document.find(group);
  groupValues.active=false;
  AE_EXPECT_TRUE(document.applyEntityValues(group,groupValues),"grupo desativado");
  AE_EXPECT_TRUE(runtime::collectSceneEnvironment(document,selected),"coleta refeita");
  AE_EXPECT_EQ(selected.exposureCompensation,-1.0f,"ancestral inativo exclui o ambiente filho");
}

AE_TEST(scene_environment_local_box_blends_by_world_distance_and_priority) {
  EditorDocument document;
  const auto global=environmentAt(document,document.root(),"Mundo",0,0);
  const auto local=environmentAt(document,document.root(),"Interior",10,4);
  auto values=*document.find(local);
  auto *environment=static_cast<scene::Environment*>(values.components.edit(scene::Environment::descriptor));
  environment->shape=renderer::EnvironmentVolumeShape::Box;
  environment->boxSize[0]=environment->boxSize[1]=environment->boxSize[2]=2;
  environment->blendDistance=2;environment->overrideSky=false;environment->overrideFog=false;
  environment->values.exposureMode=renderer::ExposureMode::AutomaticHistogram;
  environment->values.exposureMetering=renderer::ExposureMetering::Spot;
  environment->values.exposureLimitMin=-4;environment->values.exposureLimitMax=4;
  environment->values.exposureSpeedDarkToLight=6;
  AE_EXPECT_TRUE(document.applyEntityValues(local,values),"volume local configurado");
  std::vector<renderer::SceneEnvironmentVolume> volumes;
  AE_EXPECT_TRUE(runtime::collectSceneEnvironmentVolumes(document,volumes),"instâncias extraídas");
  AE_EXPECT_EQ(volumes.size(),usize(2),"global e local preservados");
  const float inside[3]{0,0,0},halfway[3]{2,0,0},outside[3]{4,0,0};
  const auto a=renderer::resolveSceneEnvironment(volumes,inside);
  const auto b=renderer::resolveSceneEnvironment(volumes,halfway);
  const auto c=renderer::resolveSceneEnvironment(volumes,outside);
  const renderer::SceneEnvironment world{};
  AE_EXPECT_EQ(a.exposureCompensation,4.0f,"dentro recebe o perfil local inteiro");
  AE_EXPECT_TRUE(std::abs(b.exposureCompensation-2.0f)<.0001f,"faixa externa mistura continuamente");
  AE_EXPECT_EQ(c.exposureCompensation,0.0f,"fora conserva o ambiente global");
  AE_EXPECT_TRUE(a.exposureMode==renderer::ExposureMode::AutomaticHistogram&&
                 a.exposureMetering==renderer::ExposureMetering::Spot&&a.exposureLimitMin==-4,
                 "interior recebe a Exposure local completa");
  AE_EXPECT_TRUE(b.exposureMode==renderer::ExposureMode::AutomaticHistogram&&
                 std::abs(b.exposureLimitMin-(world.exposureLimitMin-4)*.5f)<.0001f&&
                 std::abs(b.exposureSpeedDarkToLight-(world.exposureSpeedDarkToLight+6)*.5f)<.0001f,
                 "faixa mistura escalares e escolhe modo discreto na metade");
  AE_EXPECT_TRUE(c.exposureMode==world.exposureMode&&c.exposureMetering==world.exposureMetering&&
                 c.exposureLimitMin==world.exposureLimitMin,
                 "fora conserva a Exposure global");
  (void)global;
}

AE_TEST(scene_environment_volume_layers_and_group_overrides_are_camera_scoped) {
  EditorDocument document;
  environmentAt(document,document.root(),"Mundo",0,0);
  const auto local=environmentAt(document,document.root(),"Pós da caverna",5,3);
  auto values=*document.find(local);
  auto *environment=static_cast<scene::Environment*>(values.components.edit(scene::Environment::descriptor));
  environment->shape=renderer::EnvironmentVolumeShape::Sphere;environment->sphereRadius=3;
  environment->layer=3;environment->overrideSky=false;environment->overrideFog=false;
  environment->values.skyZenith[0]=1;environment->values.fog=true;environment->values.fogDensity=.8f;
  AE_EXPECT_TRUE(document.applyEntityValues(local,values),"camada e overrides configurados");
  std::vector<renderer::SceneEnvironmentVolume> volumes;
  AE_EXPECT_TRUE(runtime::collectSceneEnvironmentVolumes(document,volumes),"volumes extraídos");
  const float camera[3]{};
  const auto ignored=renderer::resolveSceneEnvironment(volumes,camera,1u);
  const auto accepted=renderer::resolveSceneEnvironment(volumes,camera,1u<<3);
  AE_EXPECT_EQ(ignored.exposureCompensation,0.0f,"máscara da câmera exclui outra camada");
  AE_EXPECT_EQ(accepted.exposureCompensation,3.0f,"máscara inclui o pós local");
  AE_EXPECT_TRUE(accepted.skyZenith[0]!=1.0f && !accepted.fog,"grupos não sobrescritos herdam a base");
}

AE_TEST(scene_environment_singular_local_transform_is_rejected_without_partial_publication) {
  EditorDocument document;
  const auto id=environmentAt(document,document.root(),"Inválido",0,0);
  auto values=*document.find(id);
  auto *environment=static_cast<scene::Environment*>(values.components.edit(scene::Environment::descriptor));
  environment->shape=renderer::EnvironmentVolumeShape::Box;
  values.transform.scale[0]=0;
  AE_EXPECT_TRUE(document.applyEntityValues(id,values),"rascunho autoral preserva a intenção");
  std::vector<renderer::SceneEnvironmentVolume> sentinel(1);
  AE_EXPECT_TRUE(!runtime::collectSceneEnvironmentVolumes(document,sentinel),"publicação recusa inversa singular");
  AE_EXPECT_EQ(sentinel.size(),usize(1),"falha não publica coleção parcial");
}

AE_TEST(tonemapping_follows_unity_options_and_reinhard_migrates_to_neutral) {
  // URP/HDRP 17 Tonemapping: None, Neutral, ACES. AgX fica como extra.
  const auto *tone=[]{for(const auto &e:scene::environmentEnums) if(std::string_view(e.id)=="tone_mapper") return &e;return static_cast<const scene::ComponentEnum*>(nullptr);}();
  AE_EXPECT_TRUE(tone&&tone->options.size()==4&&tone->options[0].value==3&&tone->options[1].value==4&&
                 tone->options[2].value==1&&tone->options[3].value==2,"menu None, Neutral, ACES, AgX");
  AE_EXPECT_TRUE(renderer::SceneEnvironment{}.toneMapper==renderer::ToneMapper::Aces,"ACES do Default Volume Profile do HDRP");
  scene::Environment source;source.values.toneMapper=renderer::ToneMapper::Reinhard;
  std::ostringstream out;out.imbue(std::locale::classic());source.write(out);
  scene::Environment restored;
  std::istringstream input(environment_legacy::componentAsV16(out.str()));input.imbue(std::locale::classic());
  AE_EXPECT_TRUE(restored.read(input,16)&&restored.values.toneMapper==renderer::ToneMapper::Neutral,
                 "Reinhard antigo abre como Neutral");
}

AE_TEST(color_adjustments_and_bloom_detail_round_trip_blend_and_migrate) {
  const renderer::SceneEnvironment world{};
  AE_EXPECT_TRUE(world.postExposure==0&&world.hueShift==0&&world.colorFilter[0]==1&&world.colorFilter[2]==1&&
                 world.bloomScatter==.7f&&world.bloomClamp==65472.0f&&world.bloomTint[1]==1,
                 "padrões do Color Adjustments e do Bloom da Unity");
  scene::Environment authored;
  authored.values.postExposure=.75f;authored.values.hueShift=-40;
  authored.values.colorFilter[0]=.9f;authored.values.colorFilter[1]=.8f;authored.values.colorFilter[2]=.6f;
  authored.values.bloomScatter=.3f;authored.values.bloomClamp=20;
  authored.values.bloomTint[0]=1;authored.values.bloomTint[1]=.7f;authored.values.bloomTint[2]=.4f;
  authored.values.toneMapper=renderer::ToneMapper::None;
  std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(9);authored.write(out);
  scene::Environment back;std::istringstream in(out.str());in.imbue(std::locale::classic());
  AE_EXPECT_TRUE(back.read(in,scene::Environment::descriptor.version),"cena relida");
  AE_EXPECT_TRUE(back.values.postExposure==.75f&&back.values.hueShift==-40&&back.values.colorFilter[2]==.6f&&
                 back.values.bloomScatter==.3f&&back.values.bloomClamp==20&&back.values.bloomTint[2]==.4f&&
                 back.values.toneMapper==renderer::ToneMapper::None,"campos novos atravessam a cena");
  scene::Environment old;std::istringstream oldIn(environment_legacy::componentAsV16(out.str()));
  oldIn.imbue(std::locale::classic());
  AE_EXPECT_TRUE(old.read(oldIn,16)&&old.values.postExposure==0&&old.values.hueShift==0&&
                 old.values.colorFilter[0]==1&&old.values.bloomScatter==0&&old.values.bloomClamp==65472.0f,
                 "cena antiga: sem filtro, sem giro, bloom com o raio de antes");

  resources::EnvironmentProfile profile;profile.guid=resources::assetGuidFromSeed("perfil-ajustes");
  profile.name="Ajustes";profile.values=authored.values;profile.values.active=true;
  resources::EnvironmentProfile reread;
  AE_EXPECT_TRUE(resources::EnvironmentProfile::deserialize(profile.serialize(),reread)&&
                 reread.values.hueShift==-40&&reread.values.bloomTint[1]==.7f&&
                 reread.values.toneMapper==renderer::ToneMapper::None,"perfil v12 guarda os campos novos");
  resources::EnvironmentProfile legacy;profile.values.toneMapper=renderer::ToneMapper::Aces; // None não existia na v11
  AE_EXPECT_TRUE(resources::EnvironmentProfile::deserialize(environment_legacy::profileAsV11(profile.serialize()),legacy)&&
                 legacy.values.bloomScatter==0&&legacy.values.hueShift==0,"perfil v11 abre neutro");

  renderer::SceneEnvironmentVolume base;base.stableId=1;base.environment.active=true;
  auto look=base;look.stableId=2;look.environment.priority=1;look.weight=.5f;
  look.environment.postExposure=2;look.environment.hueShift=90;look.environment.colorFilter[0]=0;
  look.environment.bloomScatter=.1f;look.environment.bloomTint[2]=0;
  look.overrides=renderer::EnvironmentOverrideColorAdjustments;
  std::array volumes{base,look};const float camera[3]{};
  auto result=renderer::resolveSceneEnvironment(volumes,camera);
  AE_EXPECT_TRUE(result.postExposure==1&&result.hueShift==45&&result.colorFilter[0]==.5f&&
                 result.bloomScatter==world.bloomScatter&&result.bloomTint[2]==1,
                 "Color Adjustments mistura pelo peso; o Bloom desmarcado herda");
  volumes[1].overrides=renderer::EnvironmentOverrideBloom;
  result=renderer::resolveSceneEnvironment(volumes,camera);
  AE_EXPECT_TRUE(result.postExposure==0&&std::abs(result.bloomScatter-.4f)<1e-6f&&result.bloomTint[2]==.5f,
                 "Bloom mistura espalhamento e tom; os ajustes de cor herdam");
}

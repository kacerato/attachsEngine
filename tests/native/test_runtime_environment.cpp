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

#include <cmath>
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
  component->values.exposureEv=exposure;
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
  environment->values.autoExposure=true;
  environment->values.autoExposureMinEv=-3.0f;
  environment->values.autoExposureMaxEv=4.0f;
  environment->values.autoExposureLowPercent=.1f;
  environment->values.autoExposureHighPercent=.9f;
  environment->values.autoExposureTargetGrey=.2f;
  environment->values.autoExposureSpeedUp=1.5f;
  environment->values.autoExposureSpeedDown=2.5f;
  environment->values.autoExposureCenterWeighted=true;
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
  AE_EXPECT_EQ(back->values.exposureEv,1.25f,"exposição preservada");
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
  AE_EXPECT_TRUE(back->values.autoExposure&&back->values.autoExposureMinEv==-3.0f&&
                 back->values.autoExposureMaxEv==4.0f&&back->values.autoExposureLowPercent==.1f&&
                 back->values.autoExposureHighPercent==.9f&&back->values.autoExposureTargetGrey==.2f&&
                 back->values.autoExposureSpeedUp==1.5f&&back->values.autoExposureSpeedDown==2.5f&&
                 back->values.autoExposureCenterWeighted,
                 "medição e adaptação de exposição sobrevivem ao arquivo de cena");
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
  profile.values.ambientOcclusionIntensity=1.75f;profile.values.exposureEv=-.5f;
  profile.values.environmentMap=resources::assetGuidFromSeed("panorama-perfil");
  profile.values.hdriRotationDegrees=45;profile.values.hdriExposureEv=1.5f;
  profile.values.fog=true;profile.values.fogBaseHeight=-18.0f;
  profile.values.fogHeightFalloff=.04f;
  profile.values.fogLightEnergy=100.0f;
  profile.values.autoExposure=true;profile.values.autoExposureMinEv=-5;
  profile.values.autoExposureMaxEv=6;profile.values.autoExposureLowPercent=.12f;
  profile.values.autoExposureHighPercent=.88f;profile.values.autoExposureTargetGrey=.22f;
  profile.values.autoExposureSpeedUp=4;profile.values.autoExposureSpeedDown=1;
  profile.values.autoExposureCenterWeighted=true;
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
  AE_EXPECT_TRUE(restored.values.autoExposure&&restored.values.autoExposureMinEv==-5&&
                 restored.values.autoExposureMaxEv==6&&restored.values.autoExposureLowPercent==.12f&&
                 restored.values.autoExposureHighPercent==.88f&&restored.values.autoExposureTargetGrey==.22f&&
                 restored.values.autoExposureSpeedUp==4&&restored.values.autoExposureSpeedDown==1&&
                 restored.values.autoExposureCenterWeighted,
                 "perfil v7 preserva parâmetros de exposição automática");

  EditorDocument document;const auto id=environmentAt(document,document.root(),"Volume",9,profile.values.exposureEv);
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
  environment->values.exposureEv=1.25f;
  AE_EXPECT_TRUE(document.applyEntityValues(id,values),"override local editado");
  AE_EXPECT_TRUE(runtime::collectSceneEnvironmentVolumes(document,volumes,std::span{&profile,usize{1}})&&
                 volumes[0].environment.exposureEv==1.25f,"diferença local explícita vence o perfil");
}

AE_TEST(auto_exposure_properties_reject_invalid_pairs_without_mutation) {
  scene::Components components;
  AE_EXPECT_TRUE(components.add(scene::Environment::descriptor)!=nullptr,"ambiente anexado");
  const auto set=[&](std::string_view id,float value) {
    return scene::setComponentProperty(components,"astra.render.environment",id,value);
  };
  AE_EXPECT_TRUE(set("auto_exposure_min_ev",-3.0f)==scene::ComponentPropertyStatus::Applied,
                 "limite inferior editável pelo contrato comum");
  AE_EXPECT_TRUE(set("auto_exposure_max_ev",-4.0f)==scene::ComponentPropertyStatus::InvalidValue,
                 "limites invertidos recusados");
  AE_EXPECT_TRUE(set("auto_exposure_low_percent",.96f)==scene::ComponentPropertyStatus::InvalidValue,
                 "percentis invertidos recusados");
  const auto *environment=static_cast<const scene::Environment*>(components.find(scene::Environment::descriptor));
  AE_EXPECT_TRUE(environment&&environment->values.autoExposureMinEv==-3.0f&&
                 environment->values.autoExposureMaxEv==8.0f&&
                 environment->values.autoExposureLowPercent==.05f,
                 "candidato inválido não alterou o componente");
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
    if(property.id.starts_with("hdri_")||property.id.starts_with("auto_exposure_")||
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
  AE_EXPECT_TRUE(environment.values.sky==renderer::SkyModel::Atmosphere&&
      environment.values.skyZenith[2]>.6f&&environment.values.ground[2]>.4f&&
      environment.values.skyHorizon[0]>environment.values.skyZenith[0],"new procedural environments use a brighter blue daytime palette");
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
  AE_EXPECT_TRUE(saved&&saved->values.skyZenith[2]==.32f&&saved->values.ground[2]==.50f,"loading preserves old/custom colours rather than replacing with defaults");
  renderer::SceneEnvironmentVolume volume;volume.environment=saved->values;const float camera[]{0,0,0};
  const auto resolved=renderer::resolveSceneEnvironment(std::span(&volume,1),camera,~u32(0));
  AE_EXPECT_TRUE(resolved.active&&resolved.skyZenith[2]==.32f&&resolved.ground[2]==.50f,"runtime volume resolver consumes the persisted colours");
}
AE_TEST(scene_view_default_is_coherent_and_does_not_change_scene_defaults) {
  const renderer::SceneEnvironment authored{};
  const auto view=renderer::defaultSceneViewEnvironment();
  AE_EXPECT_TRUE(!authored.active&&view.active,"fallback só existe para a Scene View");
  AE_EXPECT_TRUE(view.post&&view.bloom&&view.vignette&&view.ambientOcclusion,
                 "look inicial tem uma cadeia visual completa");
  AE_EXPECT_TRUE(view.valid()&&view.skyHorizon[2]>view.skyHorizon[0],
                 "look inicial é válido e mantém horizonte azulado");
  AE_EXPECT_TRUE(view.ground[0]>=.10f&&view.ground[1]>=.10f&&view.ground[2]>=.10f,
                 "hemisfério inferior preserva leitura de superfícies verticais");
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
  AE_EXPECT_TRUE(result.environmentMap==base.environment.environmentMap&&result.hdriRotationDegrees==30,
                 "peso abaixo do limiar conserva o mapa anterior e sua orientação");
}

AE_TEST(environment_v7_migrates_hdri_without_consuming_old_numeric_fields) {
  scene::Environment source;source.values.exposureEv=2.25f;source.values.groundAlbedo=.3f;
  std::ostringstream legacy;
  legacy<<source.values.active<<' '<<source.values.fog<<' '<<source.values.post<<' '
        <<source.values.bloom<<' '<<source.values.vignette<<' '<<source.values.filmGrain<<' '
        <<static_cast<u32>(source.values.sky)<<' '<<static_cast<u32>(source.values.toneMapper)<<' '
        <<source.values.ambientOcclusion<<' '<<static_cast<u32>(source.shape)<<' '
        <<source.overrideSky<<' '<<source.overrideFog<<' '<<source.overridePost<<' '
        <<source.layer<<" - "<<source.overrideIndirect<<' '<<source.values.physicalAtmosphereHighQuality;
  for(const auto &property:scene::Environment::descriptor.numbers) {
    if(property.id.starts_with("hdri_")||property.id.starts_with("auto_exposure_")||
       property.id=="fog_base_height"||property.id=="fog_height_falloff"||
       property.id=="fog_light_energy") continue;
    legacy<<' '<<property.read(source);
  }
  scene::Environment restored;restored.values.hdriRotationDegrees=88;
  std::istringstream input(legacy.str());
  AE_EXPECT_TRUE(restored.read(input,7),"v7 reste legível");
  input>>std::ws;
  AE_EXPECT_TRUE(input.eof()&&!restored.values.environmentMap.valid()&&restored.values.hdriRotationDegrees==0&&
                 restored.values.exposureEv==2.25f&&restored.values.groundAlbedo==.3f,
                 "migração consome todos os campos legados e aplica HDRI neutro");
}

AE_TEST(auto_exposure_migrates_scene_v10_and_profile_v6_disabled) {
  scene::Environment source;source.values.exposureEv=1.25f;
  std::ostringstream old;
  old<<source.values.active<<' '<<source.values.fog<<' '<<source.values.post<<' '
     <<source.values.bloom<<' '<<source.values.vignette<<' '<<source.values.filmGrain<<' '
     <<static_cast<u32>(source.values.sky)<<' '<<static_cast<u32>(source.values.toneMapper)<<' '
     <<source.values.ambientOcclusion<<' '<<static_cast<u32>(source.shape)<<' '
     <<source.overrideSky<<' '<<source.overrideFog<<' '<<source.overridePost<<' '
     <<source.layer<<" - "<<source.overrideIndirect<<' '<<source.values.physicalAtmosphereHighQuality
     <<" -";
  for(const auto &property:scene::Environment::descriptor.numbers)
    if(!property.id.starts_with("auto_exposure_")&&property.id!="fog_light_energy")
      old<<' '<<property.read(source);
  scene::Environment restored;restored.values.autoExposure=true;
  restored.values.autoExposureMinEv=-12;restored.values.autoExposureCenterWeighted=true;
  std::istringstream input(old.str());
  AE_EXPECT_TRUE(restored.read(input,10),"cena v10 relida");
  input>>std::ws;
  AE_EXPECT_TRUE(input.eof()&&!restored.values.autoExposure&&
                 restored.values.autoExposureMinEv==-8&&restored.values.autoExposureMaxEv==8&&
                 restored.values.autoExposureLowPercent==.05f&&
                 !restored.values.autoExposureCenterWeighted&&restored.values.exposureEv==1.25f&&
                 restored.values.fogLightEnergy==1.0f,
                 "migração da cena mantém compensação e desliga medição nova");

  resources::EnvironmentProfile profile;
  profile.guid=resources::assetGuidFromSeed("perfil-exposicao-legado");
  profile.name="Legado";profile.values.autoExposure=true;
  std::istringstream profileTokens(profile.serialize());
  std::vector<std::string> fields;
  for(std::string field;profileTokens>>field;) fields.push_back(field);
  AE_EXPECT_TRUE(fields.size()>12,"perfil v7 gerado");
  fields[1]="6";fields.resize(fields.size()-10);
  std::ostringstream legacy;
  for(const auto &field:fields) legacy<<field<<' ';
  resources::EnvironmentProfile migrated;
  AE_EXPECT_TRUE(resources::EnvironmentProfile::deserialize(legacy.str(),migrated),"perfil v6 relido");
  AE_EXPECT_TRUE(!migrated.values.autoExposure&&migrated.values.fogLightEnergy==1.0f&&
                 migrated.values.autoExposureMinEv==-8&&
                 migrated.values.autoExposureMaxEv==8&&migrated.values.exposureEv==0,
                 "perfil antigo recebe exposição manual padrão");
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
  AE_EXPECT_TRUE(!result.physicalAtmosphereHighQuality,
                 "qualidade discreta não oscila antes da metade da mistura");
  volumes[1].weight=.75f;result=renderer::resolveSceneEnvironment(volumes,camera);
  AE_EXPECT_EQ(result.physicalSkyIntensity,2.5f,"intensidade acompanha a influência atualizada");
  AE_EXPECT_TRUE(result.physicalAtmosphereHighQuality,
                 "qualidade discreta troca a partir da metade da mistura");
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
  AE_EXPECT_EQ(selected.exposureEv,2.0f,"maior prioridade vence");

  auto groupValues=*document.find(group);
  groupValues.active=false;
  AE_EXPECT_TRUE(document.applyEntityValues(group,groupValues),"grupo desativado");
  AE_EXPECT_TRUE(runtime::collectSceneEnvironment(document,selected),"coleta refeita");
  AE_EXPECT_EQ(selected.exposureEv,-1.0f,"ancestral inativo exclui o ambiente filho");
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
  environment->values.autoExposure=true;environment->values.autoExposureCenterWeighted=true;
  environment->values.autoExposureMinEv=-4;environment->values.autoExposureMaxEv=4;
  environment->values.autoExposureSpeedUp=6;
  AE_EXPECT_TRUE(document.applyEntityValues(local,values),"volume local configurado");
  std::vector<renderer::SceneEnvironmentVolume> volumes;
  AE_EXPECT_TRUE(runtime::collectSceneEnvironmentVolumes(document,volumes),"instâncias extraídas");
  AE_EXPECT_EQ(volumes.size(),usize(2),"global e local preservados");
  const float inside[3]{0,0,0},halfway[3]{2,0,0},outside[3]{4,0,0};
  const auto a=renderer::resolveSceneEnvironment(volumes,inside);
  const auto b=renderer::resolveSceneEnvironment(volumes,halfway);
  const auto c=renderer::resolveSceneEnvironment(volumes,outside);
  AE_EXPECT_EQ(a.exposureEv,4.0f,"dentro recebe o perfil local inteiro");
  AE_EXPECT_TRUE(std::abs(b.exposureEv-2.0f)<.0001f,"faixa externa mistura continuamente");
  AE_EXPECT_EQ(c.exposureEv,0.0f,"fora conserva o ambiente global");
  AE_EXPECT_TRUE(a.autoExposure&&a.autoExposureCenterWeighted&&a.autoExposureMinEv==-4,
                 "interior recebe exposição automática completa");
  AE_EXPECT_TRUE(b.autoExposure&&std::abs(b.autoExposureMinEv+6.0f)<.0001f&&
                 std::abs(b.autoExposureSpeedUp-4.0f)<.0001f,
                 "faixa mistura escalares e escolhe modo discreto na metade");
  AE_EXPECT_TRUE(!c.autoExposure&&c.autoExposureMinEv==-8,
                 "fora conserva exposição manual global");
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
  AE_EXPECT_EQ(ignored.exposureEv,0.0f,"máscara da câmera exclui outra camada");
  AE_EXPECT_EQ(accepted.exposureEv,3.0f,"máscara inclui o pós local");
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

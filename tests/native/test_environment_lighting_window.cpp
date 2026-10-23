// PK-1/PK-2 da auditoria full Unity: janela Iluminação, Skybox, luz ambiente
// por fonte, neblina por modo e a migração das cenas antigas.
#include "harness.h"
#include "environment_legacy_lines.h"

#include "editor/editor_archive.h"
#include "editor/editor_creation_catalog.h"
#include "editor/editor_document.h"
#include "editor/editor_screen_internal.h"
#include "editor/editor_session.h"
#include "renderer/environment_ambient.h"
#include "renderer/procedural_sky.h"
#include "resources/environment_profile.h"
#include "runtime/environment_migration.h"
#include "runtime/scene_environment.h"
#include "scene/environment.h"
#include "scene/light.h"

#include <cmath>
#include <cstdio>
#include <iomanip>
#include <locale>
#include <sstream>

using namespace ae;
using namespace ae::editor;

#define AE_EXPECT_NEAR(a, b, eps, msg) AE_EXPECT_TRUE(std::fabs((a) - (b)) <= (eps), msg)

namespace {
scene::Environment *addEnvironment(EditorEntity &values) {
  return static_cast<scene::Environment *>(values.components.add(scene::Environment::descriptor));
}
// Texto v17 de um Ambiente (antes da janela Lighting), a partir do atual.
std::string asVersion17(const scene::Environment &environment) {
  std::ostringstream full;full.imbue(std::locale::classic());full<<std::setprecision(9);
  environment.write(full);
  return environment_legacy::componentAsV17(full.str());
}
float luminance(const float c[3]) {return .2126f*c[0]+.7152f*c[1]+.0722f*c[2];}
} // namespace

AE_TEST(environment_v18_round_trips_lighting_window_fields) {
  scene::Environment environment;
  auto &v=environment.values;
  v.skyExposure=2.5f;v.sunDisk=renderer::SunDiskMode::Simple;v.sunSize=.1f;v.sunSizeConvergence=7.0f;
  v.atmosphere=2.0f;v.ambientSource=renderer::AmbientSource::Gradient;
  v.ambientSkyColor[2]=.9f;v.ambientEquatorColor[0]=.3f;v.ambientGroundColor[1]=.2f;v.ambientColor[0]=.7f;
  v.fogMode=renderer::FogMode::Linear;v.fogColorMode=renderer::FogColorMode::SkyLight;v.fogEnd=420.0f;
  v.skybox=resources::assetGuidFromSeed("ceu-da-vila");
  AE_EXPECT_TRUE(environment.valid(),"valores da janela Lighting são válidos");
  std::ostringstream out;out.imbue(std::locale::classic());environment.write(out);
  scene::Environment back;std::istringstream in(out.str());
  AE_EXPECT_TRUE(back.read(in,scene::Environment::CurrentVersion),"v18 relido");
  AE_EXPECT_TRUE(back.values.skyExposure==2.5f&&back.values.sunDisk==renderer::SunDiskMode::Simple&&
                 back.values.sunSize==.1f&&back.values.sunSizeConvergence==7.0f&&back.values.atmosphere==2.0f,
                 "Exposure, Sun, Sun Size, Convergence e Atmosphere Thickness preservados");
  AE_EXPECT_TRUE(back.values.ambientSource==renderer::AmbientSource::Gradient&&back.values.ambientSkyColor[2]==.9f&&
                 back.values.ambientEquatorColor[0]==.3f&&back.values.ambientGroundColor[1]==.2f&&
                 back.values.ambientColor[0]==.7f,"Environment Lighting preservada");
  AE_EXPECT_TRUE(back.values.fogMode==renderer::FogMode::Linear&&back.values.fogEnd==420.0f&&
                 back.values.fogColorMode==renderer::FogColorMode::SkyLight,"Fog Mode, End e cor preservados");
  AE_EXPECT_TRUE(back.values.skybox==v.skybox,"Material do céu preservado");
  AE_EXPECT_EQ(back.loadedVersion,18u,"lido na versão atual");
}

AE_TEST(environment_v17_migrates_to_the_same_image) {
  scene::Environment environment;
  environment.values.fog=true;environment.values.fogDensity=.02f;environment.values.fogStart=5;
  environment.values.atmosphere=.6f;environment.values.sunDiskIntensity=8.0f;
  const auto text=asVersion17(environment);
  scene::Environment old;std::istringstream in(text);
  AE_EXPECT_TRUE(old.read(in,17),"texto v17 aceito");
  AE_EXPECT_TRUE(old.values.fogMode==renderer::FogMode::Exponential,"neblina antiga continua exponencial");
  AE_EXPECT_TRUE(old.values.fogColorMode==renderer::FogColorMode::SkyLight,"cor da neblina antiga acompanha o céu");
  AE_EXPECT_EQ(old.values.atmosphere,1.0f,"mistura antiga vira espessura 1, a de antes");
  AE_EXPECT_TRUE(old.values.sunDisk==renderer::SunDiskMode::HighQuality,"disco solar antigo vira High Quality");
  AE_EXPECT_TRUE(old.values.ambientSource==renderer::AmbientSource::Skybox,"ambiente antigo vem do céu");
  AE_EXPECT_EQ(old.values.skyExposure,1.3f,"Exposure padrão da Unity = brilho antigo sem sol fotométrico");
  AE_EXPECT_EQ(old.loadedVersion,17u,"versão de origem registrada para a migração de documento");
  AE_EXPECT_EQ(old.values.fogDensity,.02f,"números antigos lidos nos lugares certos");
}

AE_TEST(legacy_sky_scale_and_root_multipliers_migrate_to_owner_fields) {
  EditorDocument document;
  // Sol fotométrico de 100 000 lux: 146,4 unidades de shader; referência 2.
  const auto sun=document.createEntity(document.root(),EditorEntityKind::Light,"Sol");
  {
    auto values=*document.find(sun);
    auto *light=static_cast<scene::Light *>(values.components.add(scene::Light::descriptor));
    light->kind=scene::LightKind::Directional;light->unit=scene::LightUnit::LuxLumen;light->intensity=100000.0f;
    AE_EXPECT_TRUE(document.applyEntityValues(sun,values),"sol criado");
  }
  const auto ambient=document.createEntity(document.root(),EditorEntityKind::Folder,"Ambiente");
  {
    auto values=*document.find(ambient);auto *environment=addEnvironment(values);
    environment->values.hdriExposureEv=1.0f;environment->values.exposureMode=renderer::ExposureMode::Fixed;
    environment->values.fixedExposure=8.0f;environment->values.indirectDiffuse=1.0f;
    environment->loadedVersion=17;
    AE_EXPECT_TRUE(document.applyEntityValues(ambient,values),"ambiente v16");
  }
  {
    auto root=*document.find(document.root());
    root.environment[1]=.5f;root.environment[2]=2.0f;root.environment[3]=30.0f;
    AE_EXPECT_TRUE(document.applyEntityValues(document.root(),root),"multiplicadores legados");
  }
  const auto report=runtime::migrateLegacyEnvironmentLighting(document);
  const float scale=(100000.0f/683.0f)/2.0f;
  AE_EXPECT_NEAR(report.skyScale,scale,1e-3f,"escala antiga = sol / referência");
  const auto *environment=static_cast<const scene::Environment *>(
      document.find(ambient)->components.find(scene::Environment::descriptor));
  AE_EXPECT_NEAR(environment->values.skyExposure,1.3f*scale,1e-2f,"Exposure do céu absorve a escala");
  AE_EXPECT_NEAR(environment->values.hdriExposureEv,1.0f+std::log2(scale),1e-4f,"EV do HDRI absorve a escala");
  AE_EXPECT_NEAR(environment->values.fixedExposure,7.0f,1e-4f,"Exposição × 2 vira 1 EV100 a menos (clareia)");
  AE_EXPECT_NEAR(environment->values.indirectDiffuse,.5f,1e-6f,"Ambiente × vira Intensity Multiplier");
  AE_EXPECT_NEAR(environment->values.hdriRotationDegrees,30.0f,1e-4f,"rotação do céu vai para o HDRI");
  const auto *root=document.find(document.root());
  AE_EXPECT_TRUE(root->environment[0]==1&&root->environment[1]==1&&root->environment[2]==1&&root->environment[3]==0,
                 "a raiz volta ao neutro: só um dono para cada valor");
  const auto again=runtime::migrateLegacyEnvironmentLighting(document);
  AE_EXPECT_EQ(again.environments,0u,"migração não repete");
  const auto *after=static_cast<const scene::Environment *>(
      document.find(ambient)->components.find(scene::Environment::descriptor));
  AE_EXPECT_NEAR(after->values.skyExposure,1.3f*scale,1e-2f,"segunda passada não muda nada");
}

AE_TEST(engine_unit_sun_keeps_sky_exposure_neutral_and_archive_migrates) {
  EditorDocument document;
  const auto sun=document.createEntity(document.root(),EditorEntityKind::Light,"Sol");
  auto values=*document.find(sun);
  auto *light=static_cast<scene::Light *>(values.components.add(scene::Light::descriptor));
  light->kind=scene::LightKind::Directional;light->unit=scene::LightUnit::Engine;light->intensity=3.0f;
  AE_EXPECT_TRUE(document.applyEntityValues(sun,values),"sol relativo");
  const auto ambient=document.createEntity(document.root(),EditorEntityKind::Folder,"Ambiente");
  auto environmentValues=*document.find(ambient);addEnvironment(environmentValues);
  AE_EXPECT_TRUE(document.applyEntityValues(ambient,environmentValues),"ambiente");
  const auto archive=serializeEditorDocument(document,3);
  EditorDocument restored;AE_EXPECT_TRUE(deserializeEditorDocument(archive,3,restored),"cena v17 relida");
  const auto *environment=static_cast<const scene::Environment *>(
      restored.find(ambient)->components.find(scene::Environment::descriptor));
  AE_EXPECT_EQ(environment->values.skyExposure,1.3f,"sol na unidade relativa nunca escalou o céu");
}

AE_TEST(ambient_probe_sources_match_unity_environment_lighting) {
  const float up[3]{0,1,0},down[3]{0,-1,0},side[3]{1,0,0};
  renderer::DiffuseIrradianceSh9 probe{};
  const float color[3]{.2f,.3f,.4f};
  renderer::ambientProbeFromColor(color,probe);
  float e[3];
  renderer::evaluateDiffuseIrradianceSh(probe,side,e);
  AE_EXPECT_NEAR(e[1],3.14159265f*.3f,1e-4f,"Color: irradiância pi·L em qualquer normal");
  const float sky[3]{.2f,.4f,.9f},equator[3]{.3f,.3f,.3f},ground[3]{.05f,.04f,.03f};
  renderer::ambientProbeFromGradient(sky,equator,ground,probe);
  float eu[3],ed[3];
  renderer::evaluateDiffuseIrradianceSh(probe,up,eu);renderer::evaluateDiffuseIrradianceSh(probe,down,ed);
  AE_EXPECT_TRUE(eu[2]>ed[2]*4&&eu[2]>eu[0],"Gradient: céu azul em cima, chão escuro embaixo");
  // Skybox procedural: meio-dia azul vindo de cima; pôr do sol tinge o lado do sol.
  renderer::ProceduralSkyParameters parameters;
  const float noon[3]{0,1,0};
  renderer::ambientProbeFromProceduralSky(parameters,noon,0.0f,probe);
  renderer::evaluateDiffuseIrradianceSh(probe,up,eu);
  AE_EXPECT_TRUE(eu[2]>eu[0]&&luminance(eu)>0,"Skybox ao meio-dia: luz do céu azulada");
  const float sunset[3]{.995f,.0998f,0};
  renderer::ambientProbeFromProceduralSky(parameters,sunset,0.0f,probe);
  float toward[3],away[3];const float awayDir[3]{-1,0,0};
  renderer::evaluateDiffuseIrradianceSh(probe,side,toward);renderer::evaluateDiffuseIrradianceSh(probe,awayDir,away);
  std::printf("  [sonda] poente: rumo ao sol %.4f %.4f %.4f | oposto %.4f %.4f %.4f\n",
              toward[0],toward[1],toward[2],away[0],away[1],away[2]);
  AE_EXPECT_TRUE(luminance(toward)>luminance(away)*1.03f,
                 "pôr do sol: o lado do sol recebe mais luz (sonda direcional, não a média plana)");
  // Exposure escala a sonda junto com o céu.
  renderer::DiffuseIrradianceSh9 bright{};auto doubled=parameters;doubled.exposure*=2;
  renderer::ambientProbeFromProceduralSky(doubled,sunset,0.0f,bright);
  float brighter[3];renderer::evaluateDiffuseIrradianceSh(bright,side,brighter);
  AE_EXPECT_NEAR(luminance(brighter),2*luminance(toward),1e-3f*luminance(toward)+1e-6f,"Exposure dobra o ambiente do céu");
  renderer::ProceduralAmbientProbeCache cache;
  const auto &first=cache.resolve(parameters,sunset,0.0f);
  const auto copy=first;
  AE_EXPECT_TRUE(cache.resolve(parameters,sunset,0.0f)==copy,"cache devolve a mesma sonda sem recalcular");
}

AE_TEST(skybox_material_shares_only_the_sky) {
  resources::EnvironmentProfile material;material.skybox=true;material.name="Céu de teste";
  material.guid=resources::assetGuidFromSeed("material-ceu");
  renderer::SceneEnvironment source{};source.sky=renderer::SkyModel::Atmosphere;source.skyExposure=2.0f;
  source.sunSize=.08f;source.atmosphere=1.5f;source.fog=true;source.postExposure=3.0f;
  material.values=resources::skyboxMaterialValues(source);
  AE_EXPECT_TRUE(!material.values.fog&&material.values.postExposure==0.0f,"material guarda só o céu");
  const auto text=material.serialize();
  AE_EXPECT_TRUE(text.rfind("ASTRA_SKYBOX_MATERIAL 14 ",0)==0,"cabeçalho próprio do Material do céu");
  resources::EnvironmentProfile back;
  AE_EXPECT_TRUE(resources::EnvironmentProfile::deserialize(text,back)&&back.skybox,"material relido");
  AE_EXPECT_TRUE(back.assetType()==resources::AssetType::SkyboxMaterial,"tipo de recurso SkyboxMaterial");
  renderer::SceneEnvironment instance{};instance.fog=true;instance.fogDensity=.05f;instance.postExposure=-2.0f;
  const auto applied=resources::applySkyboxMaterial(instance,back);
  AE_EXPECT_TRUE(applied.skyExposure==2.0f&&applied.sunSize==.08f&&applied.atmosphere==1.5f,"céu vem do material");
  AE_EXPECT_TRUE(applied.fog&&applied.fogDensity==.05f&&applied.postExposure==-2.0f,"neblina e pós ficam na instância");
  AE_EXPECT_TRUE(applied.skybox==material.guid,"instância lembra o material");
  // O runtime lê o material ao coletar: editar o recurso muda o Ambiente.
  EditorDocument document;
  const auto id=document.createEntity(document.root(),EditorEntityKind::Folder,"Ambiente");
  auto values=*document.find(id);auto *environment=addEnvironment(values);environment->values.skybox=material.guid;
  AE_EXPECT_TRUE(document.applyEntityValues(id,values),"ambiente com material");
  back.values.skyExposure=4.0f;
  std::vector<resources::EnvironmentProfile> profiles{back};
  renderer::SceneEnvironment resolved{};
  AE_EXPECT_TRUE(runtime::collectSceneEnvironment(document,resolved,profiles),"coleta");
  AE_EXPECT_EQ(resolved.skyExposure,4.0f,"material editado vale sem copiar para a instância");
}

AE_TEST(scene_without_environment_uses_unity_defaults_everywhere) {
  const auto look=renderer::defaultSceneEnvironment();
  AE_EXPECT_TRUE(look.active&&look.sky==renderer::SkyModel::Atmosphere,"Default-Skybox procedural");
  AE_EXPECT_TRUE(look.ambientSource==renderer::AmbientSource::Skybox&&look.indirectDiffuse==1.0f,"luz ambiente do céu, 1×");
  AE_EXPECT_TRUE(!look.fog&&look.skyExposure==1.3f&&look.sunDisk==renderer::SunDiskMode::HighQuality,
                 "sem neblina; Exposure 1,3 e sol High Quality como o material padrão da Unity");
  AE_EXPECT_TRUE(look.sunSize==.04f&&look.sunSizeConvergence==5.0f,"Sun Size 0,04 e Convergence 5");
  // Só um volume local: fora dele vale o padrão; dentro, o padrão é a base.
  renderer::SceneEnvironmentVolume local;local.environment.active=true;local.shape=renderer::EnvironmentVolumeShape::Box;
  local.environment.exposureCompensation=2.0f;local.overrides=renderer::EnvironmentOverrideAll;
  const float inside[3]{};
  const auto resolved=renderer::resolveSceneEnvironment(std::span(&local,1),inside);
  AE_EXPECT_TRUE(resolved.active&&resolved.sky==renderer::SkyModel::Atmosphere&&resolved.exposureCompensation==2.0f,
                 "volume local se mistura sobre o ambiente padrão");
}

AE_TEST(lighting_window_targets_lowest_priority_global_environment) {
  EditorDocument document;
  u64 instance=0;
  AE_EXPECT_EQ(sceneLightingEnvironment(document,&instance),kInvalidEntity,"cena nova: padrão, sem componente");
  const auto add=[&](const char *name,float priority,renderer::EnvironmentVolumeShape shape) {
    const auto id=document.createEntity(document.root(),EditorEntityKind::Folder,name);
    auto values=*document.find(id);auto *environment=addEnvironment(values);
    environment->values.priority=priority;environment->shape=shape;document.applyEntityValues(id,values);
    return id;
  };
  add("Nuvens",1,renderer::EnvironmentVolumeShape::Global);
  const auto base=add("Ambiente",0,renderer::EnvironmentVolumeShape::Global);
  add("Interior",-5,renderer::EnvironmentVolumeShape::Box);
  AE_EXPECT_EQ(sceneLightingEnvironment(document,&instance),base,"janela edita o global de menor prioridade");
  AE_EXPECT_TRUE(instance!=0,"com a instância do componente");
  // Na janela, o Ambiente mostra só os grupos da janela Lighting, na ordem dela.
  const auto *component=document.find(base)->components.find(scene::Environment::descriptor);
  const auto groups=componentGroups(*component,true);
  AE_EXPECT_TRUE(!groups.empty()&&groups.front()=="Céu","começa pelo céu (Environment)");
  bool post=false,fog=false,ambient=false;
  for(auto group:groups) {post|=group=="Pós"||group=="Exposição";fog|=group=="Neblina";ambient|=group=="Luz ambiente";}
  AE_EXPECT_TRUE(!post&&fog&&ambient,"sem pós; com luz ambiente e neblina (Other Settings)");
}

AE_TEST(create_menu_has_volumes_and_environment) {
  u32 found=0;
  for(const auto &entry:editorCreationCatalog) {
    const std::string_view id=entry.id;
    if(id=="volume.global"||id=="volume.box"||id=="volume.sphere"||id=="volume.environment"||id=="light.clouds") {
      ++found;
      AE_EXPECT_TRUE(std::string_view(creationCategories[entry.category])=="Volume e céu","na categoria Volume e céu");
    }
  }
  AE_EXPECT_EQ(found,5u,"Criar › Volume (global, caixa, esfera), Ambiente e Nuvens");
}

AE_TEST(volume_recipes_create_empty_volumes_and_scene_environment) {
  editor::EditorSession session;
  const auto recipeOf=[](std::string_view id) {
    for(u32 i=0;i<editorCreationCatalog.size();++i) if(editorCreationCatalog[i].id==id) return i;
    return ~0u;
  };
  auto &graph=session.document();
  const auto box=session.createRecipe(recipeOf("volume.box"),graph.root());
  AE_EXPECT_TRUE(box!=0,"Volume em caixa criado");
  const auto *volume=static_cast<const scene::Environment *>(graph.find(box)->components.find(scene::Environment::descriptor));
  AE_EXPECT_TRUE(volume&&volume->shape==renderer::EnvironmentVolumeShape::Box&&volume->blendDistance==1.0f&&
                 !volume->overrideSky&&!volume->overrideFog&&!volume->overridePost&&!volume->overrideIndirect&&
                 !volume->overrideExposure&&!volume->overrideBloom&&!volume->overrideColorAdjustments&&
                 !volume->overrideClouds&&volume->values.priority==1.0f,
                 "Volume novo não muda nada até ligar um grupo (perfil vazio da Unity)");
  renderer::SceneEnvironment look;
  AE_EXPECT_TRUE(runtime::collectSceneEnvironment(graph,look),"cena resolvida");
  const auto defaults=renderer::defaultSceneEnvironment();
  AE_EXPECT_TRUE(look.exposureCompensation==defaults.exposureCompensation&&look.sky==defaults.sky,"imagem igual à do padrão");
  const auto ambient=session.createRecipe(recipeOf("volume.environment"),graph.root());
  const auto *environment=static_cast<const scene::Environment *>(graph.find(ambient)->components.find(scene::Environment::descriptor));
  AE_EXPECT_TRUE(environment&&environment->overrideSky&&environment->overrideFog&&environment->overridePost&&
                 environment->shape==renderer::EnvironmentVolumeShape::Global,"Criar › Ambiente: volume global completo");
  u64 instance=0;
  AE_EXPECT_EQ(sceneLightingEnvironment(graph,&instance),ambient,"a janela Iluminação passa a editar esse Ambiente");
}

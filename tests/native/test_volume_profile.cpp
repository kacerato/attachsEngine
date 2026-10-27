#include "harness.h"

#include "editor/editor_document.h"
#include "renderer/color_grading.h"
#include "renderer/volume_parameters.h"
#include "resources/environment_profile.h"
#include "runtime/environment_migration.h"
#include "runtime/scene_environment.h"
#include "scene/camera.h"
#include "scene/environment.h"
#include "environment_legacy_lines.h"

#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

// PK-4/PK-5: Volume Profile da Unity 6 (override por parâmetro, perfil
// compartilhado, Volume Mask por camada de objeto) e o pós completo da URP.

using namespace ae;
using namespace ae::editor;

namespace {
renderer::SceneEnvironmentVolume globalVolume(float priority,u64 id) {
  renderer::SceneEnvironmentVolume volume;
  volume.environment=renderer::defaultSceneEnvironment();volume.environment.priority=priority;
  volume.overrides=renderer::VolumeOverrides::empty();volume.stableId=id;
  return volume;
}
void overrideParameter(renderer::SceneEnvironmentVolume &volume,std::string_view id) {
  const u32 parameter=renderer::volumeParameterIndex(id);
  volume.overrides.addGroup(renderer::volumeParameters()[parameter].group);
  volume.overrides.setOverridden(parameter,true);
}
std::string written(const scene::ComponentValue &value) {
  std::ostringstream out;out.imbue(std::locale::classic());
  out<<std::setprecision(std::numeric_limits<float>::max_digits10);
  value.write(out);return out.str();
}
} // namespace

AE_TEST(volume_parameter_table_covers_every_volume_property_of_the_environment) {
  // Toda propriedade de aparência do Ambiente é um parâmetro de Volume com
  // grupo; as da instância (forma, peso, prioridade, perfil) não são.
  const auto &type=scene::Environment::descriptor;
  u32 missing=0;
  for(const auto &p:type.numbers) {
    const bool instance=p.id=="priority"||p.id=="weight"||p.id=="blend_distance"||p.id.starts_with("box_size")||p.id=="sphere_radius";
    if(!instance&&renderer::volumeParameterIndex(p.id)==~0u) ++missing;
    if(instance) AE_EXPECT_TRUE(renderer::volumeParameterIndex(p.id)==~0u,"campo da instância não é de Volume");
  }
  for(const auto &p:type.booleans)
    if(p.id!="enabled"&&!p.id.starts_with("override_")&&renderer::volumeParameterIndex(p.id)==~0u) ++missing;
  for(const auto &p:type.enums)
    if(p.id!="volume_shape"&&renderer::volumeParameterIndex(p.id)==~0u) ++missing;
  for(const auto &binding:type.resourceBindings)
    if(binding.id!="profile"&&renderer::volumeParameterIndex(binding.id)==~0u) ++missing;
  AE_EXPECT_EQ(missing,0u,"nenhuma propriedade sem parâmetro");
  AE_EXPECT_EQ(renderer::volumeParameterIndex("bloom_tint.g"),renderer::volumeParameterIndex("bloom_tint"),"canal de trio");
  AE_EXPECT_EQ(renderer::volumeParameterIndex("grading_lift_color"),renderer::volumeParameterIndex("grading_lift"),"trackball: cor e deslocamento");
  // Cada override tem o booleano "no perfil" correspondente no componente.
  for(const auto &group:renderer::volumeGroups()) {
    bool found=false;
    for(const auto &p:type.booleans) found|=p.id==group.addedProperty;
    AE_EXPECT_TRUE(found,std::string(group.addedProperty).c_str());
  }
}

AE_TEST(volume_override_is_per_parameter_and_unchecked_values_come_from_below) {
  // Unity: cada parâmetro tem a caixa; desmarcado, vale o volume de baixo.
  auto low=globalVolume(0,1),high=globalVolume(1,2);
  low.environment.bloomIntensity=.5f;low.environment.bloomThreshold=2.0f;
  overrideParameter(low,"bloom_intensity");overrideParameter(low,"bloom_threshold");
  high.environment.bloomIntensity=1.25f;high.environment.bloomThreshold=9.0f;
  overrideParameter(high,"bloom_intensity"); // o limiar fica desmarcado
  const renderer::SceneEnvironmentVolume volumes[]{low,high};
  const float origin[3]{};
  const auto result=renderer::resolveSceneEnvironment(volumes,origin);
  AE_EXPECT_TRUE(result.bloomIntensity==1.25f&&result.bloomThreshold==2.0f,"intensidade de cima, limiar de baixo");
  // NONE no de cima: tudo herdado.
  auto none=high;none.overrides.setGroupParameters(renderer::VolumeGroup::Bloom,false);
  const renderer::SceneEnvironmentVolume inherited[]{low,none};
  AE_EXPECT_TRUE(renderer::resolveSceneEnvironment(inherited,origin).bloomIntensity==.5f,"NONE herda");
  // Interruptor do cabeçalho desligado: o override inteiro sai da mistura.
  auto inactive=high;inactive.overrides.active&=~(1u<<static_cast<u32>(renderer::VolumeGroup::Bloom));
  const renderer::SceneEnvironmentVolume off[]{low,inactive};
  AE_EXPECT_TRUE(renderer::resolveSceneEnvironment(off,origin).bloomIntensity==.5f,"override desligado não entra");
  // Removido do perfil: idem, mesmo com as caixas marcadas antes.
  auto removed=high;removed.overrides.removeGroup(renderer::VolumeGroup::Bloom);
  AE_EXPECT_TRUE(!removed.overrides.contributes(renderer::volumeParameterIndex("bloom_intensity")),"removido não contribui");
}

AE_TEST(volume_discrete_parameters_switch_as_soon_as_weight_is_positive) {
  // VolumeParameter.Interp: discretos trocam quando t > 0; números misturam.
  auto base=globalVolume(0,1);
  auto local=globalVolume(1,2);local.weight=.1f;
  local.environment.toneMapper=renderer::ToneMapper::Neutral;local.environment.contrast=2.0f;
  overrideParameter(local,"tone_mapper");overrideParameter(local,"contrast");
  base.environment.toneMapper=renderer::ToneMapper::Aces;overrideParameter(base,"tone_mapper");
  const renderer::SceneEnvironmentVolume volumes[]{base,local};
  const float origin[3]{};
  const auto result=renderer::resolveSceneEnvironment(volumes,origin);
  AE_EXPECT_TRUE(result.toneMapper==renderer::ToneMapper::Neutral,"curva troca com peso 0,1");
  AE_EXPECT_TRUE(std::abs(result.contrast-1.1f)<1e-5f,"contraste mistura 10%");
}

AE_TEST(volume_effects_turn_on_by_intensity_like_unity) {
  // Bloom, vinheta, grão e oclusão ligam pela intensidade (zero desliga).
  auto volume=globalVolume(0,1);
  volume.environment.bloomIntensity=0;overrideParameter(volume,"bloom_intensity");
  volume.environment.vignetteIntensity=.4f;overrideParameter(volume,"vignette_intensity");
  const renderer::SceneEnvironmentVolume volumes[]{volume};
  const float origin[3]{};
  const auto result=renderer::resolveSceneEnvironment(volumes,origin);
  AE_EXPECT_TRUE(!result.bloom&&result.vignette&&!result.filmGrain&&!result.ambientOcclusion,"liga/desliga derivado");
}

AE_TEST(environment_v18_scene_migrates_group_overrides_and_effect_switches) {
  // Cena v18: caixa por grupo e bools de efeito. Bloom marcado e vinheta
  // desmarcada viram o override Bloom com todos os parâmetros e nenhuma
  // Vinheta; vinheta desligada a 0,3 vira intensidade zero.
  scene::Environment source;
  source.overrideVignette=true;source.values.vignetteIntensity=.3f;
  auto overrides=source.volumeOverrides();overrides.removeGroup(renderer::VolumeGroup::Vignette);
  overrides.addGroup(renderer::VolumeGroup::Vignette);overrides.setGroupParameters(renderer::VolumeGroup::Vignette,true);
  source.setVolumeOverrides(overrides);
  auto v18=environment_legacy::words(environment_legacy::componentAsV18(written(source)));
  v18[4]="0"; // vinheta desligada no arquivo antigo
  v18[23]="0"; // override de vinheta desmarcado
  scene::Environment restored;
  std::istringstream in(environment_legacy::join(v18));in.imbue(std::locale::classic());
  AE_EXPECT_TRUE(restored.read(in,18),"v18 legível");
  const auto migrated=restored.volumeOverrides();
  AE_EXPECT_TRUE(migrated.hasGroup(renderer::VolumeGroup::Bloom)&&
                 migrated.overriddenInGroup(renderer::VolumeGroup::Bloom)==migrated.parametersInGroup(renderer::VolumeGroup::Bloom),
                 "bloom: todos os parâmetros marcados");
  AE_EXPECT_TRUE(!migrated.hasGroup(renderer::VolumeGroup::Vignette),"vinheta fora do perfil");
  AE_EXPECT_TRUE(restored.values.vignetteIntensity==0&&!restored.values.vignette,"vinheta desligada vira intensidade 0");
  AE_EXPECT_TRUE(migrated.hasGroup(renderer::VolumeGroup::Celestial)&&migrated.hasGroup(renderer::VolumeGroup::PhysicalAtmosphere),
                 "o céu antigo vira Céu, Sol e lua e Atmosfera física");
  AE_EXPECT_TRUE(!migrated.hasGroup(renderer::VolumeGroup::DepthOfField)&&restored.values.depthOfField==renderer::DepthOfFieldMode::Off,
                 "pós novo fica fora e neutro");
  AE_EXPECT_EQ(restored.loadedVersion,18u,"versão lida");
  // Regravar e reler: a v19 guarda as caixas por parâmetro.
  scene::Environment again;std::istringstream v19(written(restored));v19.imbue(std::locale::classic());
  AE_EXPECT_TRUE(again.read(v19,scene::Environment::CurrentVersion)&&again.volumeOverrides()==migrated,"v19 ida e volta");
}

AE_TEST(environment_v19_roundtrips_every_post_effect_and_curve) {
  scene::Environment source;
  auto &v=source.values;
  v.depthOfField=renderer::DepthOfFieldMode::Bokeh;v.depthOfFieldQuality=renderer::EffectQuality::High;
  v.focusDistance=3.5f;v.focalLength=85;v.aperture=2.8f;v.bladeCount=7;v.bladeCurvature=.25f;v.bladeRotation=30;
  v.gaussianStart=4;v.gaussianEnd=40;v.gaussianMaxRadius=1.25f;v.gaussianHighQuality=true;
  v.motionBlurQuality=renderer::EffectQuality::Medium;v.motionBlurIntensity=.6f;v.motionBlurClamp=.08f;
  v.chromaticAberration=.4f;v.lensDistortion=-.35f;v.lensDistortionXMultiplier=.5f;v.lensDistortionCenter[0]=.4f;
  v.lensDistortionScale=1.2f;v.vignetteColor[0]=.2f;v.vignetteCenter[1]=.6f;v.vignetteSmoothness=.5f;v.vignetteRounded=true;
  v.vignetteIntensity=.45f;v.bloomHighQualityFiltering=true;v.bloomDirtIntensity=3;
  v.bloomDirt=resources::assetGuidFromSeed("sujeira");
  v.curves[0].count=3;v.curves[0].keys[1]={.5f,.7f,0,0,4,4,false};v.curves[0].keys[2]={1,1,1,1,0,0,false};
  v.curves[4].count=1;v.curves[4].keys[0]={.3f,.6f,0,0,4,4,false};
  renderer::updateColorCurveTangents(v.curves[0]);
  auto overrides=source.volumeOverrides();
  overrides.addGroup(renderer::VolumeGroup::DepthOfField);
  overrides.setOverridden(renderer::volumeParameterIndex("dof_focus_distance"),true);
  overrides.active&=~(1u<<static_cast<u32>(renderer::VolumeGroup::Fog));
  source.setVolumeOverrides(overrides);
  scene::Environment restored;std::istringstream in(written(source));in.imbue(std::locale::classic());
  AE_EXPECT_TRUE(restored.read(in,scene::Environment::CurrentVersion),"versão atual relida");
  in>>std::ws;AE_EXPECT_TRUE(in.eof(),"consumiu a linha inteira");
  bool same=true;
  for(const auto &parameter:renderer::volumeParameters())
    same&=renderer::sameVolumeParameter(parameter,source.values,restored.values);
  AE_EXPECT_TRUE(same,"todos os parâmetros de Volume iguais");
  AE_EXPECT_TRUE(restored.volumeOverrides()==source.volumeOverrides(),"overrides iguais");
}

AE_TEST(environment_profile_v14_keeps_overrides_and_v13_reads_as_full_groups) {
  resources::EnvironmentProfile profile;
  profile.guid=resources::assetGuidFromSeed("perfil-pk4");profile.name="Noite de festa";
  profile.values.lensDistortion=.2f;profile.values.motionBlurIntensity=.3f;
  profile.values.curves[5].count=2;profile.values.curves[5].keys[0]={.1f,.8f,0,0,4,4,false};
  profile.values.curves[5].keys[1]={.6f,.2f,0,0,4,4,false};
  profile.overrides=renderer::VolumeOverrides::empty();
  profile.overrides.addGroup(renderer::VolumeGroup::LensDistortion);
  profile.overrides.setOverridden(renderer::volumeParameterIndex("lens_distortion"),true);
  resources::EnvironmentProfile restored;
  AE_EXPECT_TRUE(resources::EnvironmentProfile::deserialize(profile.serialize(),restored),"v14 relido");
  AE_EXPECT_TRUE(restored.overrides==profile.overrides&&resources::sameEnvironmentAppearance(restored.values,profile),
                 "valores e overrides iguais");
  resources::EnvironmentProfile legacy;
  AE_EXPECT_TRUE(resources::EnvironmentProfile::deserialize(environment_legacy::profileAsV13(profile.serialize()),legacy),"v13 relido");
  AE_EXPECT_TRUE(legacy.overrides==renderer::volumeOverridesFromLegacyGroups(renderer::EnvironmentOverrideAll),
                 "perfil antigo cobre os grupos de antes");
  AE_EXPECT_TRUE(legacy.values.lensDistortion==0&&legacy.values.depthOfField==renderer::DepthOfFieldMode::Off,"pós novo neutro");
}

AE_TEST(volumes_sharing_a_profile_resolve_from_the_resource) {
  // V-01: a instância não guarda a aparência; o recurso manda, nos dois.
  EditorDocument document;
  resources::EnvironmentProfile profile;
  profile.guid=resources::assetGuidFromSeed("perfil-compartilhado");profile.name="Compartilhado";
  profile.values.bloomIntensity=.9f;profile.overrides=renderer::VolumeOverrides::empty();
  profile.overrides.addGroup(renderer::VolumeGroup::Bloom);
  profile.overrides.setOverridden(renderer::volumeParameterIndex("bloom_intensity"),true);
  std::vector<EditorEntityId> ids;
  for(const char *name:{"A","B"}) {
    const auto id=document.createEntity(document.root(),EditorEntityKind::Folder,name);
    auto value=*document.find(id);
    auto *environment=static_cast<scene::Environment*>(value.components.add(scene::Environment::descriptor));
    environment->profile=profile.guid;environment->values.bloomIntensity=.1f; // cópia velha
    document.applyEntityValues(id,value);ids.push_back(id);
  }
  std::vector<renderer::SceneEnvironmentVolume> volumes;
  AE_EXPECT_TRUE(runtime::collectSceneEnvironmentVolumes(document,volumes,std::span<const resources::EnvironmentProfile>(&profile,1)),"coleta");
  AE_EXPECT_EQ(volumes.size(),usize{2},"dois volumes");
  for(const auto &volume:volumes)
    AE_EXPECT_TRUE(volume.environment.bloomIntensity==.9f&&volume.overrides==profile.overrides,"valor e caixas do perfil");
  // Recurso ausente: vale a cópia gravada.
  AE_EXPECT_TRUE(runtime::collectSceneEnvironmentVolumes(document,volumes),"coleta sem recurso");
  AE_EXPECT_TRUE(volumes[0].environment.bloomIntensity==.1f,"cópia local quando o perfil some");
  // Cena antiga com cópia divergente: a cópia vale até o editor clonar.
  auto value=*document.find(ids[0]);
  static_cast<scene::Environment*>(value.components.editInstance(value.components.at(0)->instanceId()))->legacyProfileCopy=true;
  document.applyEntityValues(ids[0],value);
  AE_EXPECT_TRUE(runtime::collectSceneEnvironmentVolumes(document,volumes,std::span<const resources::EnvironmentProfile>(&profile,1)),"coleta");
  AE_EXPECT_TRUE(volumes[0].environment.bloomIntensity==.1f&&volumes[1].environment.bloomIntensity==.9f,
                 "a divergente guarda a imagem antiga, a outra segue o perfil");
}

AE_TEST(volume_mask_uses_object_layers_and_migrates_ambiente_layers) {
  // V-04: o Volume usa a camada do objeto; a Volume Mask da câmera filtra.
  EditorDocument document;
  const auto volume=document.createEntity(document.root(),EditorEntityKind::Folder,"Interior");
  {
    auto value=*document.find(volume);value.components.add(scene::Environment::descriptor);
    value.layer=5;document.applyEntityValues(volume,value);
  }
  std::vector<renderer::SceneEnvironmentVolume> volumes;
  AE_EXPECT_TRUE(runtime::collectSceneEnvironmentVolumes(document,volumes)&&volumes.size()==1&&volumes[0].layer==5,
                 "camada do objeto");
  const float origin[3]{};
  AE_EXPECT_TRUE(!renderer::resolveSceneEnvironment(volumes,origin,1u<<4).active&&
                 renderer::resolveSceneEnvironment(volumes,origin,1u<<5).active,"máscara por camada de objeto");
  // Cena v18 com "Ambiente 3" e câmera que só via essa camada.
  EditorDocument legacy;
  const auto old=legacy.createEntity(legacy.root(),EditorEntityKind::Folder,"Caverna");
  const auto camera=legacy.createEntity(legacy.root(),EditorEntityKind::Folder,"Câmera");
  {
    auto value=*legacy.find(old);
    auto *environment=static_cast<scene::Environment*>(value.components.add(scene::Environment::descriptor));
    environment->legacyLayer=3;legacy.applyEntityValues(old,value);
    auto cam=*legacy.find(camera);
    auto *component=static_cast<scene::Camera*>(cam.components.add(scene::Camera::descriptor));
    component->environmentMask=1u<<3;component->legacyEnvironmentMask=true;legacy.applyEntityValues(camera,cam);
  }
  AE_EXPECT_TRUE(runtime::migrateLegacyVolumeLayers(legacy)>=2,"volume e câmera migrados");
  const u32 layer=legacy.find(old)->layer;
  AE_EXPECT_TRUE(layer!=0&&legacy.layers().name(layer)=="Ambiente 3","camada de objeto Ambiente 3");
  const auto *mask=static_cast<const scene::Camera*>(legacy.find(camera)->components.find(scene::Camera::descriptor));
  AE_EXPECT_TRUE(mask->environmentMask==(1u<<layer)&&!mask->legacyEnvironmentMask,"a máscara aponta a camada nova");
}

AE_TEST(color_curves_are_neutral_by_default_and_reach_the_grading_lut) {
  renderer::SceneEnvironment environment{};
  AE_EXPECT_TRUE(environment.curvesNeutral(),"curvas padrão neutras");
  auto look=renderer::colorGradingLook(environment);
  AE_EXPECT_TRUE(!renderer::colorGradingUsesLut(look),"sem LUT");
  // Mestre em S: escurece as sombras e clareia os altos.
  auto &master=environment.curves[0];
  master.count=4;
  master.keys[0]={0,0,0,0,4,4,false};master.keys[1]={.25f,.15f,0,0,4,4,false};
  master.keys[2]={.75f,.85f,0,0,4,4,false};master.keys[3]={1,1,0,0,4,4,false};
  renderer::updateColorCurveTangents(master);
  look=renderer::colorGradingLook(environment);
  AE_EXPECT_TRUE(renderer::colorGradingUsesLut(look),"curva liga a LUT");
  const float dark[3]{.05f,.05f,.05f};float out[3];
  renderer::gradeDisplayColor(look,dark,out);
  AE_EXPECT_TRUE(out[0]<.05f,"sombra mais escura");
  // Hue vs Sat em zero dessatura (0,5 é neutro).
  renderer::SceneEnvironment gray{};
  gray.curves[5].count=2;gray.curves[5].keys[0]={0,0,0,0,4,4,false};gray.curves[5].keys[1]={.9f,0,0,0,4,4,false};
  const float red[3]{.6f,.1f,.1f};
  renderer::gradeDisplayColor(renderer::colorGradingLook(gray),red,out);
  AE_EXPECT_TRUE(std::abs(out[0]-out[1])<1e-3f&&std::abs(out[1]-out[2])<1e-3f,"sem saturação");
  // Mistura de volumes: curvas misturam amostra a amostra.
  auto a=globalVolume(0,1),b=globalVolume(1,2);
  b.environment.curves[0]=master;b.weight=.5f;overrideParameter(b,"curve_master");
  const renderer::SceneEnvironmentVolume volumes[]{a,b};
  const float origin[3]{};
  const auto mixed=renderer::resolveSceneEnvironment(volumes,origin);
  const float mid=renderer::evaluateColorCurve(0,mixed.curves[0],.25f);
  AE_EXPECT_TRUE(mid>.15f&&mid<.25f,"curva a meio caminho");
}

AE_TEST(camera_v4_volume_mask_is_marked_for_migration) {
  scene::Camera camera;camera.environmentMask=1u<<2;
  std::ostringstream out;out.imbue(std::locale::classic());camera.write(out);
  scene::Camera v4;std::istringstream in(out.str());in.imbue(std::locale::classic());
  AE_EXPECT_TRUE(v4.read(in,4)&&v4.legacyEnvironmentMask&&v4.environmentMask==(1u<<2),"v4 marcada");
  scene::Camera v5;std::istringstream again(out.str());again.imbue(std::locale::classic());
  AE_EXPECT_TRUE(v5.read(again,5)&&!v5.legacyEnvironmentMask,"v5 já é Volume Mask");
}

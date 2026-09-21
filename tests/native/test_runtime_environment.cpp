#include "harness.h"

#include "editor/editor_archive.h"
#include "editor/editor_component_catalog.h"
#include "editor/editor_document.h"
#include "runtime/scene_environment.h"
#include "resources/environment_profile.h"
#include "scene/component_schema.h"
#include "scene/environment.h"

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
  environment->profile=resources::assetGuidFromSeed("perfil-noite");
  AE_EXPECT_TRUE(document.applyEntityValues(id,values),"AO e perfil configurados");
  const auto archive=serializeEditorDocument(document,9);
  EditorDocument restored;
  AE_EXPECT_TRUE(deserializeEditorDocument(archive,9,restored),"cena relida");
  const auto *back=static_cast<const scene::Environment *>(
      restored.find(id)->components.find(scene::Environment::descriptor));
  AE_EXPECT_TRUE(back!=nullptr,"componente preservado");
  AE_EXPECT_EQ(back->values.exposureEv,1.25f,"exposição preservada");
  AE_EXPECT_EQ(back->values.priority,7.0f,"prioridade preservada");
  AE_EXPECT_TRUE(back->values.ambientOcclusion&&back->values.ambientOcclusionRadius==2.25f,
                 "parâmetros de AO preservados");
  AE_EXPECT_TRUE(back->values.filmGrain&&back->values.filmGrainIntensity==.22f,
                 "override de grão preservado");
  AE_EXPECT_TRUE(back->profile==environment->profile,"identidade do perfil preservada");
}

AE_TEST(environment_profile_round_trips_and_local_values_form_an_explicit_override) {
  resources::EnvironmentProfile profile;profile.guid=resources::assetGuidFromSeed("ambiente-compartilhado");
  profile.name="Caverna úmida";profile.values.active=true;profile.values.post=true;
  profile.values.ambientOcclusion=true;profile.values.ambientOcclusionRadius=2.5f;
  profile.values.filmGrain=true;profile.values.filmGrainIntensity=.17f;
  profile.values.ambientOcclusionIntensity=1.75f;profile.values.exposureEv=-.5f;
  resources::EnvironmentProfile restored;
  AE_EXPECT_TRUE(profile.valid()&&resources::EnvironmentProfile::deserialize(profile.serialize(),restored),
                 "perfil válido relê do arquivo próprio");
  AE_EXPECT_TRUE(restored.guid==profile.guid&&restored.values.ambientOcclusionRadius==2.5f&&
                 restored.values.filmGrain&&restored.values.filmGrainIntensity==.17f,
                 "identidade e aparência voltam iguais");

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

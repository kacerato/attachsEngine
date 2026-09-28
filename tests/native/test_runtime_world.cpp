#include "harness.h"
#include "editor/editor_play_scene.h"
#include "editor/editor_physics_body.h"
#include "runtime/game_world.h"
#include "runtime/scene_components.h"
#include "scene/component_schema.h"
#include "scene/environment.h"
#include "scene/mesh_renderer.h"
#include "scene/physics_body.h"
#include "scene/camera.h"
#include "scene/camera_look.h"
#include "scene/script_behavior.h"
#include <sstream>
#include <fstream>
#include "editor/editor_archive.h"

using namespace ae;
using namespace ae::editor;
using ae::runtime::GameWorld;
using ae::runtime::ObjectHandle;
using ae::runtime::ReparentPosePolicy;
using ae::runtime::TransformAuthority;
using ae::runtime::WorldStatus;
using ae::runtime::WorldOperationState;

namespace {
EditorEntityId named(EditorDocument &doc, EditorEntityId parent, const char *name) {
  return doc.createEntity(parent, EditorEntityKind::Folder, name);
}
}

AE_TEST(runtime_world_loads_scene_and_keeps_authoring_document_untouched) {
  EditorDocument doc;
  const auto child = named(doc, doc.root(), "Filho");
  auto values = *doc.find(child);
  values.transform.position[0] = 4;
  AE_EXPECT_TRUE(doc.applyEntityValues(child, values), "autoria inicial");
  const auto revision = doc.revision();

  GameWorld world;
  AE_EXPECT_TRUE(world.load(doc), "carga da cena");
  const auto handle = world.handle(child);
  AE_EXPECT_TRUE(handle.valid() && world.alive(handle), "handle do objeto carregado");

  runtime::Transform moved;
  AE_EXPECT_EQ(static_cast<u32>(world.localTransform(handle, moved)), 0u, "leitura de transform");
  moved.position[1] = 9;
  AE_EXPECT_EQ(static_cast<u32>(world.setLocalTransform(handle, moved)), 0u, "escrita no mundo");
  AE_EXPECT_EQ(world.graph().find(child)->transform.position[1], 9.f, "mundo recebeu a pose");
  AE_EXPECT_EQ(doc.find(child)->transform.position[1], 0.f, "documento autoral intacto");
  AE_EXPECT_EQ(doc.revision(), revision, "nenhuma revisão do documento durante a execução");
}

AE_TEST(runtime_world_rejects_handles_from_another_play_session) {
  EditorDocument doc;
  const auto child = named(doc, doc.root(), "Filho");
  GameWorld first, second;
  AE_EXPECT_TRUE(first.load(doc) && second.load(doc), "duas sessões");
  AE_EXPECT_TRUE(first.worldId() != second.worldId(), "sessões têm identidades distintas");
  const auto foreign = first.handle(child);
  AE_EXPECT_EQ(static_cast<u32>(second.validate(foreign)), static_cast<u32>(WorldStatus::ForeignWorld),
               "handle de outra execução é recusado, não reinterpretado");
}

AE_TEST(runtime_world_component_resources_keep_guid128_and_validate_asset_type) {
  EditorDocument doc;const auto id=named(doc,doc.root(),"Ambiente");auto values=*doc.find(id);
  auto *environment=static_cast<scene::Environment *>(values.components.add(scene::Environment::descriptor));
  AE_EXPECT_TRUE(environment&&doc.applyEntityValues(id,values),"componente autorado");
  GameWorld world;AE_EXPECT_TRUE(world.load(doc),"mundo");
  const auto component=world.findComponent(world.handle(id),scene::Environment::descriptor.id);
  resources::AssetRegistry assets;resources::AssetRecord record;
  const resources::AssetGuid hdri{0x1122334455667788ull,0x99aabbccddeeff00ull};
  record.guid=hdri;record.type=resources::AssetType::EnvironmentMap;record.path="Environment/studio.aeen";
  AE_EXPECT_TRUE(assets.add(record),"HDRI registrado");
  AE_EXPECT_EQ((u32)world.setResource(component,"environment_map",0,hdri,assets),(u32)WorldStatus::Ok,"recurso aplicado");
  resources::AssetGuid read;AE_EXPECT_EQ((u32)world.getResource(component,"environment_map",0,read),(u32)WorldStatus::Ok,"recurso lido");
  AE_EXPECT_TRUE(read==hdri,"128 bits preservados");
  record.guid={7,8};record.type=resources::AssetType::Mesh;record.path="Meshes/wrong.mesh";
  AE_EXPECT_TRUE(assets.add(record),"tipo divergente registrado");
  AE_EXPECT_EQ((u32)world.setResource(component,"environment_map",0,record.guid,assets),(u32)WorldStatus::ResourceTypeMismatch,"tipo recusado");
  AE_EXPECT_EQ((u32)world.setResource(component,"environment_map",1,{},assets),(u32)WorldStatus::InvalidArgument,"slot recusado");

  resources::EnvironmentProfile profile;profile.guid={0xabc,0xdef};profile.name="Noite";
  profile.values.hdriExposureEv=4.0f;
  record.guid=profile.guid;record.type=resources::AssetType::EnvironmentProfile;record.path="Environment/night.environment";
  AE_EXPECT_TRUE(assets.add(record)&&profile.valid(),"perfil e conteúdo registrados");
  AE_EXPECT_EQ((u32)world.setResource(component,"profile",0,profile.guid,assets),(u32)WorldStatus::UnknownResource,
               "identidade sem biblioteca de valores não finge aplicar perfil");
  const std::array profiles{profile};
  AE_EXPECT_EQ((u32)world.setResource(component,"profile",0,profile.guid,assets,profiles),(u32)WorldStatus::Ok,
               "perfil aplicado com conteúdo");
  const auto *runtimeEnvironment=static_cast<const scene::Environment*>(world.readComponent(component));
  AE_EXPECT_TRUE(runtimeEnvironment&&runtimeEnvironment->profile==profile.guid&&
                 runtimeEnvironment->values.hdriExposureEv==4.0f,"GUID e aparência mudam juntos");
  AE_EXPECT_EQ((u32)world.setResource(component,"profile",0,{},assets,profiles),(u32)WorldStatus::Ok,
               "limpeza do vínculo aceita GUID vazio");
  runtimeEnvironment=static_cast<const scene::Environment*>(world.readComponent(component));
  AE_EXPECT_TRUE(runtimeEnvironment&&!runtimeEnvironment->profile.valid()&&
                 runtimeEnvironment->values.hdriExposureEv==4.0f,"limpeza preserva fallback local visível");
}

AE_TEST(runtime_world_material_slots_are_typed_atomic_and_require_a_published_texture) {
  EditorDocument doc;const auto id=named(doc,doc.root(),"Malha");auto authored=*doc.find(id);
  auto *render=static_cast<scene::MeshRenderer*>(authored.components.add(scene::MeshRenderer::descriptor));
  AE_EXPECT_TRUE(render!=nullptr,"renderer criado");
  render->submeshes.emplace_back();
  AE_EXPECT_TRUE(doc.applyEntityValues(id,authored),"dois slots autorados");
  const auto authoringRevision=doc.revision();

  GameWorld world;AE_EXPECT_TRUE(world.load(doc),"mundo");
  const auto component=world.findComponent(world.handle(id),scene::MeshRenderer::descriptor.id);
  scene::ComponentPropertyValue value;
  AE_EXPECT_EQ((u32)world.setSlotProperty(component,"material.roughness",1,.25f),(u32)WorldStatus::Ok,
               "fator do segundo slot mutável em Play");
  AE_EXPECT_EQ((u32)world.getSlotProperty(component,"material.roughness",1,value),(u32)WorldStatus::Ok,"releitura");
  AE_EXPECT_EQ(std::get<float>(value),.25f,"slot correto alterado");
  AE_EXPECT_EQ((u32)world.setSlotProperty(component,"surface.alpha_mode",1,(u32)scene::MaterialAlphaMask),
               (u32)WorldStatus::Ok,"enum validado pelo schema");
  AE_EXPECT_EQ((u32)world.setSlotProperty(component,"sampling.wrap",1,(u32)scene::MaterialWrapClamp),
               (u32)WorldStatus::ComponentUnavailable,"sampler sem publicação não finge efeito");
  AE_EXPECT_EQ((u32)world.getSlotProperty(component,"sampling.wrap",1,value),(u32)WorldStatus::Ok,"sampler relido");
  AE_EXPECT_EQ(std::get<u32>(value),(u32)scene::MaterialWrapKeep,"recusa de sampler é atômica");
  AE_EXPECT_EQ((u32)world.setSlotProperty(component,"sampling.normal.scale_u",1,2.5f),(u32)WorldStatus::Ok,
               "escala individual da normal é alcançável");
  AE_EXPECT_EQ((u32)world.getSlotProperty(component,"sampling.normal.scale_u",1,value),(u32)WorldStatus::Ok,
               "escala individual relida");
  AE_EXPECT_EQ(std::get<float>(value),2.5f,"binding normal mudou");
  AE_EXPECT_EQ((u32)world.getSlotProperty(component,"sampling.base_color.scale_u",1,value),(u32)WorldStatus::Ok,
               "binding vizinho relido");
  AE_EXPECT_EQ(std::get<float>(value),1.0f,"binding vizinho não foi alterado");
  AE_EXPECT_EQ((u32)world.setSlotProperty(component,"material.roughness",1,2.f),(u32)WorldStatus::InvalidArgument,
               "faixa inválida recusada");
  AE_EXPECT_EQ((u32)world.setSlotProperty(component,"surface.alpha_mode",1,999u),(u32)WorldStatus::InvalidArgument,
               "opção inválida recusada");
  AE_EXPECT_EQ((u32)world.setSlotProperty(component,"material.roughness",2,.5f),(u32)WorldStatus::InvalidArgument,
               "slot inexistente recusado");

  resources::AssetRegistry assets;resources::AssetRecord texture;
  texture.guid={0x1111,0x2222};texture.type=resources::AssetType::Texture;texture.path="Textures/runtime.png";
  AE_EXPECT_TRUE(assets.add(texture),"textura registrada");
  AE_EXPECT_EQ((u32)world.setResource(component,"texture.base_color",1,texture.guid,assets),
               (u32)WorldStatus::ComponentUnavailable,"registro sem publicação não finge efeito visual");
  resources::AssetGuid read;
  AE_EXPECT_EQ((u32)world.getResource(component,"texture.base_color",1,read),(u32)WorldStatus::Ok,"binding ainda legível");
  AE_EXPECT_TRUE(!read.valid(),"recusa é atômica");

  bool sawCandidate=false;
  const auto published=[&](resources::AssetGuid guid,resources::AssetType kind,std::string_view property,u32 slot,
                           scene::ComponentValue &candidate) {
    const auto &mesh=static_cast<const scene::MeshRenderer&>(candidate);
    sawCandidate=guid==texture.guid&&kind==resources::AssetType::Texture&&property=="texture.base_color"&&slot==1&&
                 mesh.slotTextures(1)[0]==texture.guid;
    return sawCandidate;
  };
  AE_EXPECT_EQ((u32)world.setResource(component,"texture.base_color",1,texture.guid,assets,{},published),
               (u32)WorldStatus::Ok,"publicador confirma a variante efetiva antes do commit");
  AE_EXPECT_TRUE(sawCandidate,"callback recebeu binding, slot e componente candidato");
  bool sawSampler=false;
  const auto samplerPublished=[&](resources::AssetGuid guid,resources::AssetType kind,std::string_view property,u32 slot,
                                  scene::ComponentValue &candidate) {
    const auto &mesh=static_cast<const scene::MeshRenderer&>(candidate);
    sawSampler=!guid.valid()&&kind==resources::AssetType::Texture&&property=="sampling.wrap"&&slot==1&&
               mesh.slotSampling(1)[0].wrap==scene::MaterialWrapClamp;
    return sawSampler;
  };
  AE_EXPECT_EQ((u32)world.setSlotProperty(component,"sampling.wrap",1,(u32)scene::MaterialWrapClamp,samplerPublished),
               (u32)WorldStatus::Ok,"sampler entra após publicação das variantes candidatas");
  AE_EXPECT_TRUE(sawSampler,"publicador vê sampler candidato antes do commit");
  bool sawNormalSampler=false;
  const auto normalSamplerPublished=[&](resources::AssetGuid guid,resources::AssetType kind,std::string_view property,u32 slot,
                                        scene::ComponentValue &candidate) {
    const auto &mesh=static_cast<const scene::MeshRenderer&>(candidate);
    sawNormalSampler=!guid.valid()&&kind==resources::AssetType::Texture&&property=="sampling.normal.filter"&&slot==1&&
                     mesh.slotSampling(1)[1].filter==scene::MaterialFilterNearest&&
                     mesh.slotSampling(1)[0].filter==scene::MaterialFilterKeep;
    return sawNormalSampler;
  };
  AE_EXPECT_EQ((u32)world.setSlotProperty(component,"sampling.normal.filter",1,(u32)scene::MaterialFilterNearest,
                                          normalSamplerPublished),(u32)WorldStatus::Ok,
               "filtro individual passa pelo publicador antes do commit");
  AE_EXPECT_TRUE(sawNormalSampler,"publicador recebe somente o binding candidato alterado");
  AE_EXPECT_EQ((u32)world.getResource(component,"texture.base_color",1,read),(u32)WorldStatus::Ok,"textura relida");
  AE_EXPECT_TRUE(read==texture.guid,"GUID aplicado ao segundo slot");
  bool resolvedNone=false;
  const auto none=[&](resources::AssetGuid guid,resources::AssetType kind,std::string_view property,u32 slot,
                     scene::ComponentValue &candidate) {
    const auto &mesh=static_cast<const scene::MeshRenderer&>(candidate);
    resolvedNone=guid==scene::MaterialTextureNone&&kind==resources::AssetType::Texture&&
                 property=="texture.base_color"&&slot==1&&
                 mesh.slotTextures(1)[0]==scene::MaterialTextureNone;
    return resolvedNone;
  };
  AE_EXPECT_EQ((u32)world.setResource(component,"texture.base_color",1,scene::MaterialTextureNone,assets,{},none),
               (u32)WorldStatus::Ok,"None explícito é resolvido antes do commit");
  AE_EXPECT_TRUE(resolvedNone,"consumidor recebe binding removido");
  bool resolvedInheritance=false;
  const auto inherited=[&](resources::AssetGuid guid,resources::AssetType kind,std::string_view property,u32 slot,
                           scene::ComponentValue &candidate) {
    const auto &mesh=static_cast<const scene::MeshRenderer&>(candidate);
    resolvedInheritance=!guid.valid()&&kind==resources::AssetType::Texture&&property=="texture.base_color"&&slot==1&&
                        !mesh.slotTextures(1)[0].valid();
    return resolvedInheritance;
  };
  AE_EXPECT_EQ((u32)world.setResource(component,"texture.base_color",1,{},assets,{},inherited),(u32)WorldStatus::Ok,
               "limpar override também resolve o binding herdado candidato");
  AE_EXPECT_TRUE(resolvedInheritance,"herança não pula consumidor");
  AE_EXPECT_EQ(doc.revision(),authoringRevision,"mutação de Play não persiste no documento");
  const auto *authoredRender=static_cast<const scene::MeshRenderer*>(doc.find(id)->components.find(scene::MeshRenderer::descriptor));
  AE_EXPECT_TRUE(authoredRender&&!authoredRender->slotTextures(1)[0].valid(),"binding autoral preservado");
}

AE_TEST(runtime_world_reparent_obeys_pose_policy_at_safe_point) {
  EditorDocument doc;
  const auto left=named(doc,doc.root(),"Esquerda");
  const auto right=named(doc,doc.root(),"Direita");
  const auto child=named(doc,left,"Filho");
  auto setX=[&](EditorEntityId id,float x) {
    auto values=*doc.find(id);
    values.transform.position[0]=x;
    return doc.applyEntityValues(id,values);
  };
  AE_EXPECT_TRUE(setX(left,10.f)&&setX(right,20.f)&&setX(child,2.f),"poses autorais");
  const auto revision=doc.revision();
  GameWorld world;
  AE_EXPECT_TRUE(world.load(doc),"carga");
  const auto handle=world.handle(child);
  const auto target=world.handle(right);
  AE_EXPECT_EQ((u32)world.setParent(handle,target,0,ReparentPosePolicy::KeepWorld),(u32)WorldStatus::Ok,
               "troca enfileirada");
  AE_EXPECT_EQ(world.graph().find(child)->parent,left,"hierarquia só muda no ponto seguro");
  runtime::Transform movedParent;
  AE_EXPECT_EQ((u32)world.localTransform(target,movedParent),(u32)WorldStatus::Ok,"pai legível");
  movedParent.position[0]=30.f;
  AE_EXPECT_EQ((u32)world.setLocalTransform(target,movedParent),(u32)WorldStatus::Ok,
               "pai pode mudar antes do ponto seguro");
  AE_EXPECT_EQ(world.flush(),1u,"troca aplicada");
  runtime::Transform local;
  AE_EXPECT_EQ((u32)world.localTransform(handle,local),(u32)WorldStatus::Ok,"transform local");
  AE_EXPECT_TRUE(std::abs(local.position[0]+18.f)<.001f,"pose de mundo preservada na aplicação");
  AE_EXPECT_EQ(world.graph().find(child)->parent,right,"novo pai");
  AE_EXPECT_EQ((u32)world.setParent(handle,world.handle(left),0),(u32)WorldStatus::Ok,
               "política padrão mantém local");
  AE_EXPECT_EQ(world.flush(),1u,"segunda troca aplicada");
  AE_EXPECT_EQ((u32)world.localTransform(handle,local),(u32)WorldStatus::Ok,"transform relido");
  AE_EXPECT_TRUE(std::abs(local.position[0]+18.f)<.001f,"transform local intacto");
  AE_EXPECT_EQ((u32)world.setParent(world.handle(left),handle,0),(u32)WorldStatus::Rejected,
               "ciclo recusado");
  world.setAuthority(child,TransformAuthority::PhysicsBody);
  AE_EXPECT_EQ((u32)world.setParent(handle,target,0),(u32)WorldStatus::TransformOwnedByPhysics,
               "pose da física não pode mudar de referencial");
  AE_EXPECT_EQ(doc.revision(),revision,"documento autoral preservado");
}

AE_TEST(runtime_world_destroy_expires_handle_immediately_and_frees_at_safe_point) {
  EditorDocument doc;
  const auto parent = named(doc, doc.root(), "Pai");
  const auto child = named(doc, parent, "Filho");
  GameWorld world;
  AE_EXPECT_TRUE(world.load(doc), "carga");
  const auto parentHandle = world.handle(parent);
  const auto childHandle = world.handle(child);

  AE_EXPECT_EQ(static_cast<u32>(world.destroyObject(parentHandle)), 0u, "remoção enfileirada");
  // A referência vence AGORA, ainda dentro do mesmo callback.
  AE_EXPECT_EQ(static_cast<u32>(world.validate(parentHandle)), static_cast<u32>(WorldStatus::StaleHandle),
               "handle do removido é recusado imediatamente");
  AE_EXPECT_EQ(static_cast<u32>(world.validate(childHandle)), static_cast<u32>(WorldStatus::StaleHandle),
               "a subárvore inteira vence junto");
  runtime::Transform ignored;
  AE_EXPECT_EQ(static_cast<u32>(world.localTransform(parentHandle, ignored)), static_cast<u32>(WorldStatus::StaleHandle),
               "leitura por referência vencida é recusada");
  // O armazenamento ainda existe: a iteração em curso não é corrompida.
  AE_EXPECT_TRUE(world.graph().exists(parent), "remoção só toca o armazenamento no ponto seguro");
  AE_EXPECT_EQ(world.pendingCommandCount(), 1u, "um comando pendente");

  std::vector<runtime::ObjectId> destroyed;
  AE_EXPECT_EQ(world.flush(&destroyed), 1u, "ponto seguro aplica a fila");
  AE_EXPECT_EQ(static_cast<u32>(destroyed.size()), 2u, "pai e filho relatados ao consumidor");
  AE_EXPECT_TRUE(!world.graph().exists(parent) && !world.graph().exists(child), "armazenamento liberado");
  AE_EXPECT_EQ(world.pendingCommandCount(), 0u, "fila esvaziada");
}

AE_TEST(runtime_world_tracked_structural_operations_report_safe_point_result) {
  EditorDocument doc;
  const auto first = named(doc, doc.root(), "Primeiro");
  const auto second = named(doc, doc.root(), "Segundo");
  GameWorld world;
  AE_EXPECT_TRUE(world.load(doc), "carga");
  u64 firstTicket = 0, cycleTicket = 0, removeTicket = 0;
  AE_EXPECT_EQ((u32)world.setParent(world.handle(first), world.handle(second), 0,
                                   ReparentPosePolicy::KeepLocal, &firstTicket), (u32)WorldStatus::Ok,
               "primeira troca aceita");
  AE_EXPECT_EQ((u32)world.setParent(world.handle(second), world.handle(first), 0,
                                   ReparentPosePolicy::KeepLocal, &cycleTicket), (u32)WorldStatus::Ok,
               "segunda troca aceita contra o grafo ainda não aplicado");
  WorldOperationState state{};
  WorldStatus result{};
  AE_EXPECT_EQ((u32)world.operationResult(world.worldId(), cycleTicket, state, result), (u32)WorldStatus::Ok,
               "ticket consultável");
  AE_EXPECT_EQ((u32)state, (u32)WorldOperationState::Pending, "aguarda ponto seguro");
  AE_EXPECT_EQ(world.flush(), 1u, "só primeira troca aplicada");
  world.operationResult(world.worldId(), firstTicket, state, result);
  AE_EXPECT_EQ((u32)state, (u32)WorldOperationState::Applied, "primeira aplicada");
  world.operationResult(world.worldId(), cycleTicket, state, result);
  AE_EXPECT_EQ((u32)state, (u32)WorldOperationState::Failed, "ciclo relatado");
  AE_EXPECT_EQ((u32)result, (u32)WorldStatus::Rejected, "motivo do ciclo");

  WorldStatus status{};
  const auto camera = world.addComponent(world.handle(first), "astra.camera", status);
  AE_EXPECT_EQ((u32)status, (u32)WorldStatus::Ok, "câmera anexada");
  AE_EXPECT_EQ((u32)world.removeComponent(camera, &removeTicket), (u32)WorldStatus::Ok,
               "remoção aceita");
  world.addComponent(world.handle(first), "astra.camera.look", status);
  AE_EXPECT_EQ((u32)status, (u32)WorldStatus::Ok, "dependente adicionado antes do ponto seguro");
  AE_EXPECT_EQ(world.flush(), 0u, "dependência impede remoção");
  world.operationResult(world.worldId(), removeTicket, state, result);
  AE_EXPECT_EQ((u32)state, (u32)WorldOperationState::Failed, "remoção falhou");
  AE_EXPECT_EQ((u32)result, (u32)WorldStatus::ComponentInUse, "motivo da remoção");
  AE_EXPECT_EQ((u32)world.operationResult(world.worldId() + 1, removeTicket, state, result),
               (u32)WorldStatus::ForeignWorld, "ticket preso à sessão");
  u64 destroyTicket = 0;
  const auto doomed = world.handle(first);
  AE_EXPECT_EQ((u32)world.destroyObject(doomed, &destroyTicket), (u32)WorldStatus::Ok,
               "destruição rastreada aceita");
  AE_EXPECT_EQ((u32)world.validate(doomed), (u32)WorldStatus::StaleHandle,
               "objeto vence antes da aplicação");
  AE_EXPECT_EQ((u32)world.operationResult(world.worldId(), destroyTicket, state, result),
               (u32)WorldStatus::Ok, "ticket continua acessível após destruição");
  AE_EXPECT_EQ((u32)state, (u32)WorldOperationState::Pending, "destruição aguarda ponto seguro");
  AE_EXPECT_EQ(world.flush(), 1u, "destruição aplicada");
  world.operationResult(world.worldId(), destroyTicket, state, result);
  AE_EXPECT_EQ((u32)state, (u32)WorldOperationState::Applied, "destruição confirmada");
}

AE_TEST(runtime_world_full_queue_does_not_expire_rejected_destroy) {
  EditorDocument doc;
  const auto child = named(doc, doc.root(), "Filho");
  GameWorld world;
  AE_EXPECT_TRUE(world.load(doc), "carga");
  const auto handle = world.handle(child);
  for (u32 i = 0; i < GameWorld::kMaximumPendingCommands; ++i)
    AE_EXPECT_EQ((u32)world.setParent(handle, world.root(), 0), (u32)WorldStatus::Ok, "fila preenchida");
  AE_EXPECT_EQ((u32)world.destroyObject(handle), (u32)WorldStatus::LimitReached, "destruição recusada");
  AE_EXPECT_EQ((u32)world.validate(handle), (u32)WorldStatus::Ok, "handle continua válido");
  AE_EXPECT_TRUE(world.graph().exists(child), "objeto preservado");
}

AE_TEST(runtime_world_creates_objects_immediately_without_breaking_iteration) {
  EditorDocument doc;
  GameWorld world;
  AE_EXPECT_TRUE(world.load(doc), "carga");
  const auto root = world.root();
  WorldStatus status = WorldStatus::Rejected;

  // Percorre os filhos criando um objeto por iteração: o vetor de ids é copiado
  // por quem itera, então crescer a hierarquia aqui não invalida nada.
  const auto created = world.createObject(root, "Criado", status);
  AE_EXPECT_EQ(static_cast<u32>(status), 0u, "criação aceita");
  AE_EXPECT_TRUE(created.valid() && world.alive(created), "handle utilizável na mesma linha");
  AE_EXPECT_EQ(world.childCount(root), 1u, "criação é imediata na hierarquia");
  AE_EXPECT_EQ(static_cast<u32>(world.pendingCommandCount()), 0u, "criar não enfileira comando");

  const auto nested = world.createObject(created, "Neto", status);
  AE_EXPECT_TRUE(nested.valid(), "criação encadeada");
  AE_EXPECT_TRUE(world.parentOf(nested) == created, "pai correto");
  AE_EXPECT_TRUE(world.findChildByName(root, "Neto", true) == nested, "busca recursiva encontra o novo objeto");
  AE_EXPECT_TRUE(!world.findChildByName(root, "Neto", false).valid(), "busca direta não atravessa níveis");
}

AE_TEST(runtime_world_component_api_follows_the_shared_schema) {
  EditorDocument doc;
  const auto id = named(doc, doc.root(), "Objeto");
  GameWorld world;
  AE_EXPECT_TRUE(world.load(doc), "carga");
  const auto handle = world.handle(id);
  WorldStatus status = WorldStatus::Rejected;

  // Câmera é declarada mutável em Play; corpo físico não é.
  const auto camera = world.addComponent(handle, "astra.camera", status);
  AE_EXPECT_EQ(static_cast<u32>(status), 0u, "câmera anexável em Play");
  AE_EXPECT_TRUE(camera.valid(), "instância devolvida");
  world.addComponent(handle, "astra.physics.body", status);
  AE_EXPECT_EQ(static_cast<u32>(status), static_cast<u32>(WorldStatus::NotMutableInPlay),
               "corpo físico não entra durante a execução");
  world.addComponent(handle, "astra.inexistente", status);
  AE_EXPECT_EQ(static_cast<u32>(status), static_cast<u32>(WorldStatus::UnknownComponent), "tipo desconhecido");
  world.addComponent(handle, "astra.camera", status);
  AE_EXPECT_EQ(static_cast<u32>(status), static_cast<u32>(WorldStatus::ComponentUnavailable),
               "componente singular não duplica");

  // Olhar exige Câmera: a regra vem do schema, não de um caso especial aqui.
  const auto look = world.addComponent(handle, "astra.camera.look", status);
  AE_EXPECT_EQ(static_cast<u32>(status), 0u, "olhar com câmera presente");
  AE_EXPECT_EQ(static_cast<u32>(world.removeComponent(camera)), static_cast<u32>(WorldStatus::ComponentInUse),
               "remover a câmera quebraria a exigência do olhar");
  AE_EXPECT_EQ(static_cast<u32>(world.removeComponent(look)), 0u, "remoção do olhar enfileirada");
  AE_EXPECT_EQ(world.flush(), 1u, "ponto seguro remove o componente");
  AE_EXPECT_EQ(static_cast<u32>(world.removeComponent(camera)), 0u, "câmera liberada depois");
  world.flush();
  AE_EXPECT_EQ(world.componentCount(handle), 0u, "objeto sem componentes");
}

AE_TEST(runtime_world_property_access_is_typed_and_addresses_each_instance) {
  EditorDocument doc;
  const auto id = named(doc, doc.root(), "Objeto");
  auto values = *doc.find(id);
  auto *first = static_cast<scene::Collider *>(values.components.add(scene::Collider::descriptor));
  auto *second = static_cast<scene::Collider *>(values.components.add(scene::Collider::descriptor));
  AE_EXPECT_TRUE(first && second, "dois colisores autorados");
  second->halfX = 2;
  const auto secondInstance = second->instanceId();
  AE_EXPECT_TRUE(doc.applyEntityValues(id, values), "autoria aceita");

  GameWorld world;
  AE_EXPECT_TRUE(world.load(doc), "carga");
  const auto handle = world.handle(id);
  const auto a = world.findComponent(handle, "astra.physics.collider", 0);
  const auto b = world.findComponent(handle, "astra.physics.collider", 1);
  AE_EXPECT_TRUE(a.valid() && b.valid() && a.instance != b.instance, "instâncias distintas endereçáveis");
  AE_EXPECT_EQ(b.instance, secondInstance, "ordinal segue a ordem autorada");

  scene::ComponentPropertyValue value;
  AE_EXPECT_EQ(static_cast<u32>(world.getProperty(b, "half_x", value)), 0u, "leitura por instância");
  AE_EXPECT_EQ(std::get<float>(value), 2.f, "valor da segunda instância");
  AE_EXPECT_EQ(static_cast<u32>(world.setProperty(a, "half_x", 3.0f)), 0u, "escrita por instância");
  AE_EXPECT_EQ(static_cast<u32>(world.getProperty(a, "half_x", value)), 0u, "releitura");
  AE_EXPECT_EQ(std::get<float>(value), 3.f, "somente a primeira mudou");
  AE_EXPECT_EQ(static_cast<u32>(world.getProperty(b, "half_x", value)), 0u, "releitura da segunda");
  AE_EXPECT_EQ(std::get<float>(value), 2.f, "segunda preservada");

  AE_EXPECT_EQ(static_cast<u32>(world.setProperty(a, "half_x", true)), static_cast<u32>(WorldStatus::InvalidArgument),
               "tipo incompatível é recusado");
  AE_EXPECT_EQ(static_cast<u32>(world.setProperty(a, "half_x", 1e9f)), static_cast<u32>(WorldStatus::Rejected),
               "valor fora dos limites do descritor é recusado");
  AE_EXPECT_EQ(static_cast<u32>(world.setProperty(a, "nao_existe", 1.0f)), static_cast<u32>(WorldStatus::InvalidArgument),
               "propriedade desconhecida");
  AE_EXPECT_EQ(static_cast<u32>(world.setProperty(a, "owner", scene::ObjectReference{999})),
               static_cast<u32>(WorldStatus::UnknownObject), "referência para objeto inexistente é recusada");
}

AE_TEST(runtime_world_reference_writes_keep_full_identity_and_scope) {
  EditorDocument doc;
  const auto parent=named(doc,doc.root(),"Corpo pai");
  const auto child=named(doc,parent,"Forma filha");
  const auto other=named(doc,doc.root(),"Outro corpo");
  for(const auto id:{parent,other}) {
    auto values=*doc.find(id);
    AE_EXPECT_TRUE(values.components.add(scene::PhysicsBody::descriptor)!=nullptr,"corpo autorado");
    AE_EXPECT_TRUE(doc.applyEntityValues(id,values),"valores aceitos");
  }
  auto values=*doc.find(child);
  AE_EXPECT_TRUE(values.components.add(scene::Collider::descriptor)!=nullptr,"colisor autorado");
  AE_EXPECT_TRUE(doc.applyEntityValues(child,values),"colisor aceito");
  GameWorld world;
  AE_EXPECT_TRUE(world.load(doc),"carga");
  const auto collider=world.findComponent(world.handle(child),"astra.physics.collider");
  AE_EXPECT_TRUE(collider.valid(),"instância encontrada");
  AE_EXPECT_EQ((u32)world.setProperty(collider,"owner",scene::ObjectReference{parent}),(u32)WorldStatus::Ok,
               "ancestral compatível aceito");
  AE_EXPECT_EQ((u32)world.setProperty(collider,"owner",scene::ObjectReference{other}),(u32)WorldStatus::Rejected,
               "corpo fora da ancestralidade recusado");
  AE_EXPECT_EQ((u32)world.setProperty(collider,"owner",scene::ObjectReference{(1ull<<32)|parent}),
               (u32)WorldStatus::InvalidArgument,"ID de 64 bits não trunca para um alvo vivo");
  AE_EXPECT_EQ((u32)world.destroyObject(world.handle(other)),(u32)WorldStatus::Ok,"alvo marcado para remoção");
  AE_EXPECT_EQ((u32)world.setProperty(collider,"owner",scene::ObjectReference{other}),
               (u32)WorldStatus::StaleHandle,"alvo vencido é recusado antes do flush");
  scene::ComponentPropertyValue saved;
  AE_EXPECT_EQ((u32)world.getProperty(collider,"owner",saved),(u32)WorldStatus::Ok,"releitura");
  AE_EXPECT_EQ(std::get<scene::ObjectReference>(saved).id,(u64)parent,"recusas não alteram a referência");
}

AE_TEST(runtime_world_transform_authority_protects_simulated_poses) {
  EditorDocument doc;
  const auto id = named(doc, doc.root(), "Corpo");
  const auto child = named(doc, id, "Marca");
  GameWorld world;
  AE_EXPECT_TRUE(world.load(doc), "carga");
  const auto handle = world.handle(id);
  runtime::Transform value;
  AE_EXPECT_EQ(static_cast<u32>(world.localTransform(handle, value)), 0u, "leitura");
  AE_EXPECT_EQ(static_cast<u32>(world.setLocalTransform(handle, value)), 0u, "objeto livre aceita escrita");

  world.setAuthority(child, TransformAuthority::PhysicsBody);
  AE_EXPECT_EQ(static_cast<u32>(world.setLocalTransform(handle, value)),
               static_cast<u32>(WorldStatus::TransformOwnedByPhysics),
               "mover um ancestral de pose simulada é recusado com motivo");
  world.clearAuthorities();
  AE_EXPECT_EQ(static_cast<u32>(world.setLocalTransform(handle, value)), 0u, "autoridade liberada");
}

AE_TEST(runtime_world_world_transform_composes_and_decomposes_through_the_hierarchy) {
  EditorDocument doc;
  const auto parent = named(doc, doc.root(), "Pai");
  auto values = *doc.find(parent);
  values.transform.position[0] = 5;
  values.transform.rotationDegrees[1] = 90;
  AE_EXPECT_TRUE(doc.applyEntityValues(parent, values), "pai posicionado");
  const auto child = named(doc, parent, "Filho");
  values = *doc.find(child);
  values.transform.position[2] = 2;
  AE_EXPECT_TRUE(doc.applyEntityValues(child, values), "filho deslocado");

  GameWorld world;
  AE_EXPECT_TRUE(world.load(doc), "carga");
  const auto handle = world.handle(child);
  runtime::Transform world1;
  AE_EXPECT_EQ(static_cast<u32>(world.worldTransform(handle, world1)), 0u, "transform de mundo");
  AE_EXPECT_TRUE(std::abs(world1.position[0] - 7) < .001f, "rotação do pai aplicada ao deslocamento do filho");

  runtime::Transform target = world1;
  target.position[0] = 10;
  AE_EXPECT_EQ(static_cast<u32>(world.setWorldTransform(handle, target)), 0u, "escrita em espaço de mundo");
  AE_EXPECT_EQ(static_cast<u32>(world.worldTransform(handle, world1)), 0u, "releitura");
  AE_EXPECT_TRUE(std::abs(world1.position[0] - 10) < .001f, "ida e volta preserva o alvo de mundo");
}

AE_TEST(runtime_play_scene_runs_on_the_world_and_stop_preserves_authoring) {
  EditorDocument doc;
  const auto id = named(doc, doc.root(), "Caixa");
  auto values = *doc.find(id);
  values.transform.position[1] = 6;
  editPhysicsBody(values)->motion = scene::BodyMotion::Dynamic;
  editCollider(values);
  AE_EXPECT_TRUE(doc.applyEntityValues(id, values), "corpo dinâmico autorado");
  const auto revision = doc.revision();

  EditorMapScene resources;
  EditorPlayScene play;
  AE_EXPECT_TRUE(play.start(doc, resources), "Play inicia sobre o mundo de execução");
  AE_EXPECT_TRUE(play.world().running() && play.world().worldId() != 0, "mundo ativo");
  for (int step = 0; step < 30; ++step) AE_EXPECT_TRUE(play.advance(1. / 60), "passo de simulação");
  AE_EXPECT_TRUE(play.document().find(id)->transform.position[1] < 6.f, "a queda aparece no mundo");
  AE_EXPECT_EQ(doc.find(id)->transform.position[1], 6.f, "documento autoral intacto durante o Play");
  AE_EXPECT_EQ(doc.revision(), revision, "nenhuma revisão autoral durante o Play");

  const auto worldId = play.world().worldId();
  play.stop();
  AE_EXPECT_TRUE(!play.active() && !play.executionGraph(), "Stop encerra o mundo");
  AE_EXPECT_EQ(doc.find(id)->transform.position[1], 6.f, "autoria preservada depois do Stop");
  AE_EXPECT_TRUE(play.start(doc, resources), "novo Play");
  AE_EXPECT_TRUE(play.world().worldId() != worldId, "o segundo Play tem identidade própria");
}

AE_TEST(runtime_play_scene_releases_physics_bodies_destroyed_at_the_safe_point) {
  EditorDocument doc;
  const auto id = named(doc, doc.root(), "Caixa");
  auto values = *doc.find(id);
  values.transform.position[1] = 6;
  editPhysicsBody(values)->motion = scene::BodyMotion::Dynamic;
  editCollider(values);
  AE_EXPECT_TRUE(doc.applyEntityValues(id, values), "corpo dinâmico");

  EditorMapScene resources;
  EditorPlayScene play;
  AE_EXPECT_TRUE(play.start(doc, resources), "Play");
  auto &world = play.world();
  const auto handle = world.handle(id);
  AE_EXPECT_EQ(static_cast<u32>(world.destroyObject(handle)), 0u, "remoção durante a execução");
  AE_EXPECT_TRUE(play.advance(1. / 60), "o passo seguinte aplica o ponto seguro");
  AE_EXPECT_TRUE(!world.graph().exists(id), "objeto removido");
  for (int step = 0; step < 10; ++step)
    AE_EXPECT_TRUE(play.advance(1. / 60), "simulação continua sem o corpo removido");
}

AE_TEST(runtime_schema_matches_the_editor_catalog_rules) {
  // O catálogo do inspetor e a API C# leem o MESMO schema: se esta checagem
  // falhar, uma das duas listas voltou a existir em separado.
  for (const auto &schema : scene::componentSchemas) {
    AE_EXPECT_TRUE(schema.type && !schema.type->id.empty(), "entrada com descritor");
    AE_EXPECT_TRUE(scene::findComponentSchema(schema.type->id) == &schema, "busca por id devolve a própria entrada");
  }
  scene::Components components;
  const auto *look = scene::findComponentSchema("astra.camera.look");
  const auto *camera = scene::findComponentSchema("astra.camera");
  const auto *body = scene::findComponentSchema("astra.physics.body");
  const auto *character = scene::findComponentSchema("astra.physics.character");
  AE_EXPECT_TRUE(look && camera && body && character, "entradas nativas presentes");
  AE_EXPECT_TRUE(scene::componentUnavailableReason(*look, components), "olhar exige câmera");
  AE_EXPECT_TRUE(!scene::componentUnavailableReason(*camera, components), "câmera sempre anexável");
  AE_EXPECT_TRUE(components.add(*camera->type), "câmera adicionada");
  AE_EXPECT_TRUE(!scene::componentUnavailableReason(*look, components), "olhar liberado com câmera");
  AE_EXPECT_TRUE(components.add(*character->type), "personagem adicionado");
  AE_EXPECT_TRUE(scene::componentUnavailableReason(*body, components), "corpo é incompatível com personagem");
}

AE_TEST(each_direction_of_a_conflict_explains_the_object_in_front_of_you) {
  // A mensagem e lida por quem tentou anexar o componente RECUSADO, e descreve
  // o que fazer. Uma frase so, reusada nos dois sentidos, fala do objeto
  // errado: num objeto SEM personagem, recusar `Personagem` com "o personagem
  // ja possui capsula propria" explica uma situacao que nao existe.
  //
  // Encontrado montando a composicao de aceitacao no aparelho: a `Plataforma`
  // tinha um colisor e nenhum personagem.
  using namespace ae::scene;
  const auto *character = findComponentSchema(Character::descriptor);
  const auto *collider = findComponentSchema(Collider::descriptor);
  AE_EXPECT_TRUE(character && collider, "os dois schemas existem");

  Components comColisor;
  AE_EXPECT_TRUE(comColisor.add(Collider::descriptor) != nullptr, "objeto com colisor");
  const char *recusaPersonagem = componentUnavailableReason(*character, comColisor);
  AE_EXPECT_TRUE(recusaPersonagem != nullptr, "personagem e recusado");
  AE_EXPECT_TRUE(std::string(recusaPersonagem).find("remova") != std::string::npos,
                 "e diz o que fazer neste objeto, que nao tem personagem");

  Components comPersonagem;
  AE_EXPECT_TRUE(comPersonagem.add(Character::descriptor) != nullptr, "objeto com personagem");
  const char *recusaColisor = componentUnavailableReason(*collider, comPersonagem);
  AE_EXPECT_TRUE(recusaColisor != nullptr, "colisor e recusado");
  AE_EXPECT_TRUE(std::string(recusaColisor) != std::string(recusaPersonagem),
                 "as duas direcoes do mesmo conflito nao usam a mesma frase");
}

AE_TEST(runtime_world_enabled_controls_camera_look_and_script_property_without_unlocking_script_fields) {
  EditorDocument doc;const auto parent=named(doc,doc.root(),"Parent"),id=named(doc,parent,"Camera");
  auto values=*doc.find(id);values.components.add(scene::Camera::descriptor);
  values.components.add(scene::CameraLook::descriptor);
  auto *script=static_cast<scene::ScriptBehavior *>(values.components.add(scene::ScriptBehavior::descriptor));
  script->scriptType="test.enabled";script->source="Enabled.cs";
  AE_EXPECT_TRUE(doc.applyEntityValues(id,values),"dados autorados");
  GameWorld world;AE_EXPECT_TRUE(world.load(doc),"mundo");
  const auto camera=world.handle(id),owner=world.handle(parent);
  const auto look=world.findComponent(camera,"astra.camera.look"),behavior=world.findComponent(camera,"astra.script.behavior");
  AE_EXPECT_TRUE(world.setProperty(look,"enabled",false)==WorldStatus::Ok && world.applyCameraLook(camera,.1f,.1f)==WorldStatus::Ok &&
                 world.graph().find(id)->transform.rotationDegrees[1]==0,"desabilitado não gira");
  AE_EXPECT_TRUE(world.setProperty(look,"enabled",true)==WorldStatus::Ok && world.setActive(owner,false)==WorldStatus::Ok &&
                 world.applyCameraLook(camera,.1f,.1f)==WorldStatus::Ok && world.graph().find(id)->transform.rotationDegrees[1]==0,"pai inativo também suspende");
  world.setActive(owner,true);world.applyCameraLook(camera,.1f,.1f);
  AE_EXPECT_TRUE(world.graph().find(id)->transform.rotationDegrees[1]>0,"retoma controle");
  AE_EXPECT_TRUE(world.setProperty(behavior,"enabled",false)==WorldStatus::Ok,"estado de script aceito");
  scene::ComponentPropertyValue read;
  AE_EXPECT_TRUE(world.getProperty(behavior,"enabled",read)==WorldStatus::Ok && !std::get<bool>(read),"script e componente leem mesmo estado");
  AE_EXPECT_TRUE(world.setProperty(behavior,"other",true)==WorldStatus::NotMutableInPlay,"campos não foram liberados indiscriminadamente");
  scene::CameraLook authored;authored.enabled=false;std::stringstream saved;authored.write(saved);scene::CameraLook restored;
  AE_EXPECT_TRUE(restored.read(saved,scene::CameraLook::descriptor.version) && !restored.enabled,"estado salvo");
  std::stringstream legacy("300 195 83");AE_EXPECT_TRUE(restored.read(legacy,1) && restored.enabled,"v1 migra ativo");
}

// Fixture gerada com os serializers reais; evita manter payloads manualmente.
int writeEnabledFixture(const char *path) {
  EditorDocument doc;
  const auto driver=named(doc,doc.root(),"Driver"),worker=named(doc,doc.root(),"Worker"),
             body=named(doc,doc.root(),"Body"),camera=named(doc,doc.root(),"Camera");
  for(const auto id:{driver,worker}) {
    auto values=*doc.find(id);auto *script=static_cast<scene::ScriptBehavior *>(values.components.add(scene::ScriptBehavior::descriptor));
    script->scriptType=id==driver?"acceptance.enabled.driver":"acceptance.enabled.worker";
    script->source="EnabledProbe.cs";doc.applyEntityValues(id,values);
  }
  auto values=*doc.find(body);values.transform.position[0]=5;
  auto *physics=editPhysicsBody(values);physics->motion=scene::BodyMotion::Dynamic;physics->gravityFactor=0;
  editCollider(values);doc.applyEntityValues(body,values);
  values=*doc.find(camera);values.components.add(scene::Camera::descriptor);
  auto *look=static_cast<scene::CameraLook *>(values.components.add(scene::CameraLook::descriptor));look->enabled=false;
  auto *animation=static_cast<scene::Animation *>(values.components.add(scene::Animation::descriptor));animation->enabled=false;
  auto *lod=static_cast<scene::LodGroup *>(values.components.add(scene::LodGroup::descriptor));lod->enabled=false;
  doc.applyEntityValues(camera,values);
  std::ofstream out(path);out<<serializeEditorDocument(doc,0);return out.good()?0:1;
}

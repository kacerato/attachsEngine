#include "editor/editor_session.h"

#include "core/sha256.h"
#include "editor/editor_import_transaction.h"
#include "editor/editor_widgets.h"
#include "renderer/volume_parameters.h"
#include "resources/image_decode.h"
#include "scene/environment.h"
#include "scene/script_curve.h"

#include <algorithm>
#include <chrono>

namespace ae::editor {

// PK-4 (V-01): Volume Profile compartilhado como na Unity 6.
//
// Na Unity o Volume aponta para um Volume Profile (asset); editar um
// parâmetro no Inspector do Volume edita o asset na hora, e todo Volume que
// usa o mesmo perfil muda junto. "New" cria um perfil vazio e "Clone" uma
// cópia independente (docs.unity3d.com/6000.0/Documentation/Manual/urp/
// Volumes.html e set-up-a-volume.html).
//
// Aqui toda edição do Inspector continua sendo uma edição do componente (o
// mesmo caminho de teclado, cor, menu e curva); depois dela, a sessão vê que
// a cópia gravada na instância deixou de ser igual ao perfil e transforma o
// passo em edição do recurso: desfaz o passo do objeto e grava o perfil, que
// sincroniza a cópia de todos os volumes que o usam. Um Desfazer volta tudo.

namespace {
const scene::Environment *environmentOf(const EditorEntity &entity,u64 instance) {
  const auto *component=entity.components.findInstance(instance);
  return component&&&component->type()==&scene::Environment::descriptor?static_cast<const scene::Environment*>(component):nullptr;
}
}

u32 EditorSession::environmentProfileUsers(const resources::AssetGuid &guid) const {
  u32 count=0;std::vector<EditorEntityId> ids;document_.collectSubtree(document_.root(),ids);
  for(const auto id:ids) if(const auto *entity=document_.find(id))
    for(u32 i=0;i<entity->components.size();++i) {
      const auto *component=entity->components.at(i);
      if(&component->type()==&scene::Environment::descriptor&&static_cast<const scene::Environment&>(*component).profile==guid) ++count;
    }
  return count;
}

void EditorSession::reconcileEnvironmentProfiles() {
  if(isPlaying()||history_.isOpen()) return;
  if(document_.revision()==profileReconcileRevision_&&environmentProfiles_.size()==profileReconcileCount_) return;
  profileReconcileRevision_=document_.revision();profileReconcileCount_=environmentProfiles_.size();
  std::vector<EditorEntityId> ids;document_.collectSubtree(document_.root(),ids);
  std::vector<resources::EnvironmentProfile> candidates;
  EditorEntityId edited=kInvalidEntity;bool single=true;
  for(const auto id:ids) {
    const auto *entity=document_.find(id);if(!entity) continue;
    for(u32 i=0;i<entity->components.size();++i) {
      const auto *component=entity->components.at(i);
      if(&component->type()!=&scene::Environment::descriptor) continue;
      const auto &environment=static_cast<const scene::Environment&>(*component);
      if(!environment.profile.valid()) continue;
      const auto *profile=findEnvironmentProfile(environment.profile);
      if(!profile||profile->skybox) continue;
      const auto overrides=environment.volumeOverrides();
      if(environment.legacyProfileCopy) {
        migrateLegacyEnvironmentProfile(id,component->instanceId());
        continue;
      }
      if(resources::sameEnvironmentProfileData(environment.values,overrides,*profile)) continue;
      // Dois volumes do mesmo perfil editados ao mesmo tempo só acontecem fora
      // do Inspector (colar componente); vale o primeiro, o outro sincroniza.
      if(std::any_of(candidates.begin(),candidates.end(),[&](const auto &c){return c.guid==profile->guid;})) continue;
      auto candidate=*profile;
      candidate.values=environment.values;candidate.values.active=true;candidate.values.priority=0;
      candidate.overrides=overrides;++candidate.revision;
      candidates.push_back(std::move(candidate));
      if(edited==kInvalidEntity) edited=id;else if(edited!=id) single=false;
    }
  }
  if(candidates.empty()) return;
  // O passo que divergiu vira o passo do perfil: um Desfazer só.
  if(single&&history_.lastIsValuesOf(edited)) history_.undo(document_);
  std::string diagnostic;
  if(!commitProfileBatch(candidates,diagnostic,true)) state_.status=diagnostic;
  else {
    const u32 users=environmentProfileUsers(candidates.front().guid);
    state_.status="Perfil "+candidates.front().name+" · "+std::to_string(users)+(users==1?" volume":" volumes");
    if(state_.profileInspector==candidates.front().guid) refreshProfileInspector();
  }
  profileReconcileRevision_=document_.revision();
}

// Cena anterior à v19 com perfil cuja cópia divergia do recurso: a imagem que
// o autor via era a da cópia. Se a cópia é igual, só passa a seguir o perfil;
// se não, ganha um perfil novo "Clone de …" com aqueles valores (o mesmo
// clone para volumes do mesmo perfil com a mesma cópia).
void EditorSession::migrateLegacyEnvironmentProfile(EditorEntityId id,u64 instance) {
  const auto *entity=document_.find(id);const auto *environment=entity?environmentOf(*entity,instance):nullptr;
  if(!environment) return;
  const auto *profile=findEnvironmentProfile(environment->profile);
  auto values=*entity;
  auto *edit=static_cast<scene::Environment*>(values.components.editInstance(instance));
  if(!edit||!profile) return;
  edit->legacyProfileCopy=false;
  if(!resources::sameEnvironmentProfileData(environment->values,environment->volumeOverrides(),*profile)) {
    resources::AssetGuid clone{};
    for(const auto &candidate:environmentProfiles_)
      if(candidate.name=="Clone de "+profile->name&&
         resources::sameEnvironmentProfileData(environment->values,environment->volumeOverrides(),candidate)) clone=candidate.guid;
    if(!clone.valid()) {
      std::string diagnostic;
      clone=writeNewEnvironmentProfile("Clone de "+profile->name,environment->values,environment->volumeOverrides(),diagnostic);
      if(!clone.valid()) {reportProblem(EditorConsoleSeverity::Warning,"Perfil divergente não clonado: "+diagnostic);return;}
      reportProblem(EditorConsoleSeverity::Info,"Volume "+std::string(entity->name)+": a cópia local divergia de "+
                    profile->name+"; criado o perfil Clone de "+profile->name+" com a imagem que a cena mostrava.");
    }
    edit->profile=clone;
  }
  // Estado de abertura, não edição: sem histórico.
  document_.applyEntityValues(id,values);
}

resources::AssetGuid EditorSession::writeNewEnvironmentProfile(std::string name,const renderer::SceneEnvironment &values,
                                                               const renderer::VolumeOverrides &overrides,
                                                               std::string &diagnostic) {
  resources::EnvironmentProfile profile;profile.name=name.empty()?"Perfil":name;
  profile.values=values;profile.values.active=true;profile.values.priority=0;profile.overrides=overrides;
  std::string stem;for(unsigned char c:profile.name)
    stem.push_back(c<32||c==127||c=='/'||c=='\\'||c==':'||c=='"'?'_':static_cast<char>(c));
  if(stem.empty()||stem=="."||stem=="..") stem="Perfil";
  std::string path="Ambientes/"+stem+".environment";
  for(u32 n=2;assets_.findByPath(path)||files_.exists(path);++n) path="Ambientes/"+stem+" "+std::to_string(n)+".environment";
  profile.guid=resources::assetGuidFromSeed("environment:"+path+":"+
      std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+":"+std::to_string(++importInstanceCounter_));
  if(!profile.valid()) {diagnostic="Valores do perfil inválidos.";return {};}
  const auto serialized=profile.serialize();resources::AssetRecord record;
  record.guid=profile.guid;record.type=resources::AssetType::EnvironmentProfile;record.path=path;
  record.contentHash=Sha256::hex(std::span<const u8>(reinterpret_cast<const u8*>(serialized.data()),serialized.size()));
  if(profile.values.environmentMap.valid()) record.dependencies.push_back(profile.values.environmentMap);
  auto nextAssets=assets_;if(!nextAssets.add(record)) {diagnostic="O registro recusou o perfil.";return {};}
  if(!writeEnvironmentProfile(profile,path,diagnostic)) return {};
  assets_=std::move(nextAssets);assetRegistryDirty_=true;environmentProfiles_.push_back(profile);files_.rebuildTree();
  diagnostic="Perfil criado: "+path;
  return profile.guid;
}

// Unity: o botão New ao lado do Profile (perfil vazio) e Clone (cópia do
// perfil atual). Sem perfil, "Novo" guarda o que este Volume já tem.
resources::AssetGuid EditorSession::createVolumeProfile(EditorEntityId id,u64 instance,bool clone,std::string &diagnostic) {
  diagnostic.clear();
  const auto *entity=document_.find(id);const auto *environment=entity?environmentOf(*entity,instance):nullptr;
  if(isPlaying()||history_.isOpen()||!environment) {diagnostic="Volume indisponível.";return {};}
  const auto *current=environment->profile.valid()?findEnvironmentProfile(environment->profile):nullptr;
  if(clone&&!current) {diagnostic="Escolha um perfil antes de clonar.";return {};}
  renderer::SceneEnvironment values=environment->values;
  auto overrides=environment->volumeOverrides();
  std::string name=entity->name[0]?std::string(entity->name):"Ambiente";
  if(clone) name="Clone de "+current->name;
  else if(current) {
    // New da Unity: perfil vazio. O volume continua igual ao mundo sob ele.
    values=renderer::defaultSceneEnvironment();
    overrides=renderer::VolumeOverrides::empty();
    name=name+" Perfil";
  }
  const auto guid=writeNewEnvironmentProfile(name,values,overrides,diagnostic);
  if(!guid.valid()) return {};
  auto next=*entity;auto *edit=static_cast<scene::Environment*>(next.components.editInstance(instance));
  if(!edit) {diagnostic="O volume mudou durante a criação.";return {};}
  const auto *created=findEnvironmentProfile(guid);
  edit->profile=guid;edit->values=resources::applyEnvironmentProfile(edit->values,*created);
  edit->setVolumeOverrides(created->overrides);edit->legacyProfileCopy=false;
  if(!history_.applyValues(document_,id,next)) {diagnostic="O histórico recusou o vínculo do perfil.";return {};}
  profileReconcileRevision_=document_.revision();
  return guid;
}

// Caixa de override, ALL/NONE, interruptor do cabeçalho, Adicionar/Remover e
// Redefinir: mudam a instância (a reconciliação leva ao perfil, se houver).
bool EditorSession::editVolumeOverrides(EditorEntityId id,u64 instance,
                                        const std::function<bool(scene::Environment&)> &edit) {
  if(isPlaying()||history_.isOpen()) return false;
  const auto *entity=document_.find(id);if(!entity||!environmentOf(*entity,instance)) return false;
  auto values=*entity;auto *environment=static_cast<scene::Environment*>(values.components.editInstance(instance));
  if(!environment||!edit(*environment)||!environment->valid()) return false;
  return history_.applyValues(document_,id,values);
}

bool EditorSession::toggleVolumeParameter(EditorEntityId id,u64 instance,u32 parameter) {
  return editVolumeOverrides(id,instance,[&](scene::Environment &environment) {
    auto overrides=environment.volumeOverrides();
    if(parameter>=renderer::volumeParameters().size()) return false;
    overrides.setOverridden(parameter,!overrides.overridden(parameter));
    environment.setVolumeOverrides(overrides);return true;
  });
}

bool EditorSession::setVolumeGroupParameters(EditorEntityId id,u64 instance,renderer::VolumeGroup group,bool value) {
  return editVolumeOverrides(id,instance,[&](scene::Environment &environment) {
    auto overrides=environment.volumeOverrides();
    overrides.setGroupParameters(group,value);environment.setVolumeOverrides(overrides);return true;
  });
}

bool EditorSession::toggleVolumeGroupActive(EditorEntityId id,u64 instance,renderer::VolumeGroup group) {
  return editVolumeOverrides(id,instance,[&](scene::Environment &environment) {
    auto overrides=environment.volumeOverrides();
    overrides.active^=1u<<static_cast<u32>(group);environment.setVolumeOverrides(overrides);return true;
  });
}

bool EditorSession::addVolumeGroup(EditorEntityId id,u64 instance,renderer::VolumeGroup group) {
  return editVolumeOverrides(id,instance,[&](scene::Environment &environment) {
    auto overrides=environment.volumeOverrides();
    if(overrides.hasGroup(group)) return false;
    // Add Override da Unity: o efeito entra com os valores padrão e todas as
    // caixas desmarcadas; o autor marca o que este volume decide.
    overrides.addGroup(group);overrides.setGroupParameters(group,false);
    renderer::copyVolumeGroup(group,renderer::SceneEnvironment{},environment.values);
    environment.values.syncEffectSwitches();
    environment.setVolumeOverrides(overrides);return true;
  });
}

bool EditorSession::removeVolumeGroup(EditorEntityId id,u64 instance,renderer::VolumeGroup group) {
  return editVolumeOverrides(id,instance,[&](scene::Environment &environment) {
    auto overrides=environment.volumeOverrides();
    if(!overrides.hasGroup(group)) return false;
    overrides.removeGroup(group);environment.setVolumeOverrides(overrides);return true;
  });
}

bool EditorSession::resetVolumeGroup(EditorEntityId id,u64 instance,renderer::VolumeGroup group) {
  return editVolumeOverrides(id,instance,[&](scene::Environment &environment) {
    renderer::copyVolumeGroup(group,renderer::SceneEnvironment{},environment.values);
    environment.values.syncEffectSwitches();return true;
  });
}

bool EditorSession::setEnvironmentCurve(EditorEntityId id,u64 instance,u32 channel,const renderer::ColorCurve &curve) {
  if(channel>=renderer::ColorCurveCount||!curve.valid()) return false;
  return editVolumeOverrides(id,instance,[&](scene::Environment &environment) {
    environment.values.curves[channel]=curve;
    // Mexer na curva é decidi-la neste volume, como arrastar um controle com
    // a caixa marcada; a Unity exige marcar antes, aqui a edição já marca.
    auto overrides=environment.volumeOverrides();
    overrides.setOverridden(renderer::volumeParameterIndex(renderer::colorCurveParameterId(channel)),true);
    environment.setVolumeOverrides(overrides);return true;
  });
}

namespace {
// Color Curves no editor de curvas da Astra (o mesmo Keyframe da Unity). As de
// matiz repetem; sem chave, a curva começa no neutro dela para o dedo ter onde
// pegar (a diagonal, ou a reta em 0,5).
scene::ScriptCurve scriptCurveOf(u32 channel,const renderer::ColorCurve &curve) {
  scene::ScriptCurve result;
  result.pre=result.post=renderer::colorCurveLoops(channel)?scene::CurveWrapMode::Loop:scene::CurveWrapMode::Clamp;
  if(curve.count==0) {
    if(renderer::colorCurveIsDiagonal(channel)) {
      result.keys.push_back({0,0,1,1,scene::CurveTangentMode::Free,scene::CurveTangentMode::Free,false});
      result.keys.push_back({1,1,1,1,scene::CurveTangentMode::Free,scene::CurveTangentMode::Free,false});
    } else {
      result.keys.push_back({.25f,.5f,0,0,scene::CurveTangentMode::ClampedAuto,scene::CurveTangentMode::ClampedAuto,false});
      result.keys.push_back({.75f,.5f,0,0,scene::CurveTangentMode::ClampedAuto,scene::CurveTangentMode::ClampedAuto,false});
    }
    return result;
  }
  for(u32 i=0;i<curve.count;++i) {
    const auto &k=curve.keys[i];
    result.keys.push_back({k.time,k.value,k.in,k.out,static_cast<scene::CurveTangentMode>(k.left),
                           static_cast<scene::CurveTangentMode>(k.right),k.broken});
  }
  return result;
}
bool colorCurveOf(const scene::ScriptCurve &source,renderer::ColorCurve &out,std::string &diagnostic) {
  renderer::ColorCurve curve;
  for(const auto &k:source.keys) {
    if(k.time<0.0f||k.time>1.0f) continue; // fora do domínio 0..1 da TextureCurve
    if(curve.count==renderer::ColorCurve::MaximumKeys) {diagnostic="No máximo 16 chaves por curva de cor";return false;}
    curve.keys[curve.count++]={k.time,std::clamp(k.value,-4.0f,4.0f),k.in,k.out,static_cast<u32>(k.left),
                               static_cast<u32>(k.right),k.broken};
  }
  renderer::updateColorCurveTangents(curve);
  if(!curve.valid()) {diagnostic="Curva inválida: tempos repetidos";return false;}
  out=curve;return true;
}
}

void EditorSession::openVolumeCurve(u32 key,u32 channel,const renderer::ColorCurve &curve,bool profile) {
  openCurveEditor(key,scene::scriptCurveValue(scriptCurveOf(channel,curve)),"curve");
  state_.curveVolumeChannel=channel+1;state_.curveVolumeProfile=profile;
}

// Chamado pelo editor de curvas ao aplicar uma curva de cor.
bool EditorSession::commitVolumeCurve(const scene::ScriptCurve &source) {
  const u32 channel=state_.curveVolumeChannel-1;
  renderer::ColorCurve curve;std::string diagnostic;
  if(!colorCurveOf(source,curve,diagnostic)) {state_.status=diagnostic;return false;}
  bool applied=false;
  if(state_.curveVolumeProfile) {
    applied=editProfileVolume([&](scene::Environment &environment) {
      environment.values.curves[channel]=curve;
      auto overrides=environment.volumeOverrides();
      overrides.setOverridden(renderer::volumeParameterIndex(renderer::colorCurveParameterId(channel)),true);
      environment.setVolumeOverrides(overrides);return true;
    });
  } else applied=setEnvironmentCurve(state_.curveEntity,state_.curveInstance,channel,curve);
  if(applied) {state_.curveVolumeChannel=0;state_.curveField=0;state_.status=std::string("Curva ")+renderer::colorCurveName(channel);}
  else state_.status="Curva recusada";
  return applied;
}

// O mesmo que editVolumeOverrides, no recurso aberto no Inspector de Perfil.
bool EditorSession::editProfileVolume(const std::function<bool(scene::Environment&)> &edit) {
  const auto *profile=findEnvironmentProfile(state_.profileInspector);
  if(!profile||profile->skybox||isPlaying()||history_.isOpen()) return false;
  scene::Environment proxy;proxy.values=profile->values;proxy.setVolumeOverrides(profile->overrides);
  if(!edit(proxy)||!proxy.valid()) return false;
  auto candidate=*profile;candidate.values=proxy.values;candidate.values.active=true;candidate.values.priority=0;
  candidate.overrides=proxy.volumeOverrides();++candidate.revision;
  std::string diagnostic;
  if(!commitProfileBatch({candidate},diagnostic,true)) {state_.status=diagnostic;return false;}
  refreshProfileInspector();
  return true;
}

bool EditorSession::handleVolumeWidget(u32 key) {
  const auto in=[&](EditorWidget base){return key>=widgetId(base)&&key<widgetId(base)+0x10000u;};
  const auto *entity=document_.find(state_.selection);
  const auto instanceAt=[&](u32 index)->u64{
    return entity&&index<entity->components.size()?entity->components.at(index)->instanceId():0;};
  const auto low=[&](EditorWidget base){return (key-widgetId(base))&0xffu;};
  const auto high=[&](EditorWidget base){return (key-widgetId(base))>>8;};
  const auto groupName=[](u32 group){return std::string(renderer::volumeGroupInfo(static_cast<renderer::VolumeGroup>(group)).name);};
  if(isPlaying()) {state_.status="Edite o Volume fora do Play";return true;}
  if(in(EditorWidget::VolumeParameterBase)) {
    const u32 parameter=high(EditorWidget::VolumeParameterBase);
    if(parameter<renderer::volumeParameters().size()&&!toggleVolumeParameter(state_.selection,instanceAt(low(EditorWidget::VolumeParameterBase)),parameter))
      state_.status="Override recusado";
    return true;
  }
  if(in(EditorWidget::VolumeGroupActiveBase)) {
    const u32 group=high(EditorWidget::VolumeGroupActiveBase);
    if(group<renderer::VolumeGroupCount)
      toggleVolumeGroupActive(state_.selection,instanceAt(low(EditorWidget::VolumeGroupActiveBase)),static_cast<renderer::VolumeGroup>(group));
    return true;
  }
  if(in(EditorWidget::VolumeGroupAllBase)||in(EditorWidget::VolumeGroupNoneBase)) {
    const bool all=in(EditorWidget::VolumeGroupAllBase);
    const auto base=all?EditorWidget::VolumeGroupAllBase:EditorWidget::VolumeGroupNoneBase;
    const u32 group=high(base);
    if(group<renderer::VolumeGroupCount&&
       setVolumeGroupParameters(state_.selection,instanceAt(low(base)),static_cast<renderer::VolumeGroup>(group),all))
      state_.status=groupName(group)+(all?": todos os parâmetros decididos aqui":": todos herdados");
    return true;
  }
  if(in(EditorWidget::VolumeGroupMenuBase)) {
    const u32 menu=((low(EditorWidget::VolumeGroupMenuBase)+1)<<8)|high(EditorWidget::VolumeGroupMenuBase);
    state_.volumeMenu=state_.volumeMenu==menu?0:menu;return true;
  }
  if(key==widgetId(EditorWidget::VolumeGroupReset)||key==widgetId(EditorWidget::VolumeGroupRemove)) {
    const u32 menu=state_.volumeMenu;state_.volumeMenu=0;
    if(!menu) return true;
    const u32 index=(menu>>8)-1,group=menu&0xffu;
    if(group>=renderer::VolumeGroupCount) return true;
    const auto volumeGroup=static_cast<renderer::VolumeGroup>(group);
    if(key==widgetId(EditorWidget::VolumeGroupReset)) {
      if(resetVolumeGroup(state_.selection,instanceAt(index),volumeGroup)) state_.status=groupName(group)+": valores padrão";
    } else if(removeVolumeGroup(state_.selection,instanceAt(index),volumeGroup)) state_.status=groupName(group)+" removido do perfil";
    return true;
  }
  if(in(EditorWidget::VolumeAddOverrideBase)) {
    const u32 open=low(EditorWidget::VolumeAddOverrideBase)+1;
    state_.volumeAddMenu=state_.volumeAddMenu==open?0:open;return true;
  }
  if(in(EditorWidget::VolumeAddChoiceBase)) {
    const u32 group=key-widgetId(EditorWidget::VolumeAddChoiceBase),index=state_.volumeAddMenu?state_.volumeAddMenu-1:0;
    state_.volumeAddMenu=0;
    if(group>=renderer::VolumeGroupCount) return true;
    if(addVolumeGroup(state_.selection,instanceAt(index),static_cast<renderer::VolumeGroup>(group))) {
      // A seção nasce aberta, como o override recém-adicionado na Unity.
      std::erase(state_.inspectorSections,std::string(scene::Environment::descriptor.id)+"/"+groupName(group));
      state_.status=groupName(group)+" adicionado: marque o que este volume decide";
    }
    return true;
  }
  if(in(EditorWidget::VolumeProfileNewBase)||in(EditorWidget::VolumeProfileCloneBase)) {
    const bool clone=in(EditorWidget::VolumeProfileCloneBase);
    std::string diagnostic;
    createVolumeProfile(state_.selection,instanceAt(low(clone?EditorWidget::VolumeProfileCloneBase:EditorWidget::VolumeProfileNewBase)),clone,diagnostic);
    state_.status=diagnostic;return true;
  }
  if(in(EditorWidget::VolumeCurveBase)) {
    const u32 channel=high(EditorWidget::VolumeCurveBase);const u64 instance=instanceAt(low(EditorWidget::VolumeCurveBase));
    const auto *component=entity?entity->components.findInstance(instance):nullptr;
    if(channel>=renderer::ColorCurveCount||!component||&component->type()!=&scene::Environment::descriptor) return true;
    state_.curveEntity=entity->id;state_.curveInstance=instance;state_.curveProperty.clear();state_.curveElement=0;
    openVolumeCurve(key,channel,static_cast<const scene::Environment&>(*component).values.curves[channel],false);
    return true;
  }
  return false;
}

// Os mesmos controles no Inspector do recurso Perfil (V-06).
bool EditorSession::handleProfileVolumeWidget(u32 key) {
  const auto *profile=findEnvironmentProfile(state_.profileInspector);
  if(!profile) return false;
  const auto in=[&](EditorWidget base,u32 span){return key>=widgetId(base)&&key<widgetId(base)+span;};
  const auto groupAt=[&](EditorWidget base){return static_cast<renderer::VolumeGroup>(key-widgetId(base));};
  const auto groupName=[](renderer::VolumeGroup group){return std::string(renderer::volumeGroupInfo(group).name);};
  if(in(EditorWidget::ProfileSectionBase,renderer::VolumeGroupCount)) {
    state_.profileSectionsCollapsed^=1u<<(key-widgetId(EditorWidget::ProfileSectionBase));
    refreshProfileInspector();return true;
  }
  if(isPlaying()) {state_.status="Edite o perfil fora do Play";return true;}
  if(in(EditorWidget::ProfileOverrideBase,0x10000u)) {
    const u32 row=key-widgetId(EditorWidget::ProfileOverrideBase);
    if(row>=state_.profileRows.size()) return true;
    const u32 parameter=state_.profileRows[row].parameter;
    if(parameter==~0u) return true;
    editProfileVolume([&](scene::Environment &environment) {
      auto overrides=environment.volumeOverrides();overrides.setOverridden(parameter,!overrides.overridden(parameter));
      environment.setVolumeOverrides(overrides);return true;});
    return true;
  }
  if(in(EditorWidget::ProfileGroupActiveBase,renderer::VolumeGroupCount)) {
    const auto group=groupAt(EditorWidget::ProfileGroupActiveBase);
    editProfileVolume([&](scene::Environment &environment) {
      auto overrides=environment.volumeOverrides();overrides.active^=1u<<static_cast<u32>(group);
      environment.setVolumeOverrides(overrides);return true;});
    return true;
  }
  if(in(EditorWidget::ProfileGroupAllBase,renderer::VolumeGroupCount)||in(EditorWidget::ProfileGroupNoneBase,renderer::VolumeGroupCount)) {
    const bool all=in(EditorWidget::ProfileGroupAllBase,renderer::VolumeGroupCount);
    const auto group=groupAt(all?EditorWidget::ProfileGroupAllBase:EditorWidget::ProfileGroupNoneBase);
    if(editProfileVolume([&](scene::Environment &environment) {
      auto overrides=environment.volumeOverrides();overrides.setGroupParameters(group,all);
      environment.setVolumeOverrides(overrides);return true;}))
      state_.status=groupName(group)+(all?": todos os parâmetros decididos pelo perfil":": todos herdados");
    return true;
  }
  if(in(EditorWidget::ProfileGroupMenuBase,renderer::VolumeGroupCount)) {
    const u32 menu=key-widgetId(EditorWidget::ProfileGroupMenuBase)+1;
    state_.profileVolumeMenu=state_.profileVolumeMenu==menu?0:menu;refreshProfileInspector();return true;
  }
  if((key==widgetId(EditorWidget::VolumeGroupReset)||key==widgetId(EditorWidget::VolumeGroupRemove))&&state_.profileVolumeMenu) {
    const auto group=static_cast<renderer::VolumeGroup>(state_.profileVolumeMenu-1);state_.profileVolumeMenu=0;
    const bool remove=key==widgetId(EditorWidget::VolumeGroupRemove);
    if(editProfileVolume([&](scene::Environment &environment) {
      if(remove) {auto overrides=environment.volumeOverrides();overrides.removeGroup(group);environment.setVolumeOverrides(overrides);}
      else {renderer::copyVolumeGroup(group,renderer::SceneEnvironment{},environment.values);environment.values.syncEffectSwitches();}
      return true;}))
      state_.status=groupName(group)+(remove?" removido do perfil":": valores padrão");
    return true;
  }
  if(key==widgetId(EditorWidget::ProfileAddOverride)) {state_.profileAddMenu=!state_.profileAddMenu;refreshProfileInspector();return true;}
  if(in(EditorWidget::VolumeAddChoiceBase,renderer::VolumeGroupCount)&&state_.profileAddMenu) {
    const auto group=groupAt(EditorWidget::VolumeAddChoiceBase);state_.profileAddMenu=false;
    if(editProfileVolume([&](scene::Environment &environment) {
      auto overrides=environment.volumeOverrides();
      if(overrides.hasGroup(group)) return false;
      overrides.addGroup(group);overrides.setGroupParameters(group,false);
      renderer::copyVolumeGroup(group,renderer::SceneEnvironment{},environment.values);environment.values.syncEffectSwitches();
      environment.setVolumeOverrides(overrides);return true;})) {
      state_.profileSectionsCollapsed&=~(1u<<static_cast<u32>(group));refreshProfileInspector();
      state_.status=groupName(group)+" adicionado ao perfil";
    }
    return true;
  }
  if(in(EditorWidget::ProfileCurveBase,renderer::ColorCurveCount)) {
    const u32 channel=key-widgetId(EditorWidget::ProfileCurveBase);
    state_.curveEntity=kInvalidEntity;state_.curveInstance=0;state_.curveProperty.clear();state_.curveElement=0;
    openVolumeCurve(key,channel,profile->values.curves[channel],true);
    return true;
  }
  return false;
}

// PK-5: Bloom › Lens Dirt. As texturas que algum Ambiente ou perfil usa
// como sujeira, decodificadas da fonte do projeto e reamostradas para o
// quadrado que o pós lê. Ficam em cache pelo GUID e pelo conteúdo.
const std::vector<std::pair<resources::AssetGuid,renderer::SharedLensDirt>> &EditorSession::lensDirtTextures() {
  std::vector<resources::AssetGuid> wanted;
  const auto collect=[&](const runtime::SceneGraph &graph) {
    std::vector<EditorEntityId> ids;graph.collectSubtree(graph.root(),ids);
    for(const auto id:ids) if(const auto *entity=graph.find(id))
      for(u32 i=0;i<entity->components.size();++i) {
        const auto *component=entity->components.at(i);
        if(&component->type()!=&scene::Environment::descriptor) continue;
        const auto guid=static_cast<const scene::Environment&>(*component).values.bloomDirt;
        if(guid.valid()&&std::find(wanted.begin(),wanted.end(),guid)==wanted.end()) wanted.push_back(guid);
      }
  };
  collect(document_);
  if(isPlaying()&&playScene_.active()) collect(playScene_.document());
  for(const auto &profile:environmentProfiles_)
    if(profile.values.bloomDirt.valid()&&std::find(wanted.begin(),wanted.end(),profile.values.bloomDirt)==wanted.end())
      wanted.push_back(profile.values.bloomDirt);
  const auto root=files_.rootPath();
  for(const auto &guid:wanted) {
    const auto *record=assets_.find(guid);
    if(!record||record->type!=resources::AssetType::Texture) continue;
    const auto cached=std::find_if(lensDirtTextures_.begin(),lensDirtTextures_.end(),[&](const auto &entry){return entry.first==guid;});
    if(cached!=lensDirtTextures_.end()&&lensDirtHashes_[static_cast<usize>(cached-lensDirtTextures_.begin())]==record->contentHash) continue;
    std::filesystem::path absolute;std::vector<u8> bytes;resources::DecodedImage image;std::string diagnostic;
    auto texture=std::make_shared<renderer::LensDirtTexture>();
    if(root.empty()||!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),record->path,absolute)||
       !EditorImportTransaction::read(absolute,bytes,importLimits_.image.maximumEncodedBytes)||
       !resources::decodeImageRgba8(bytes,importLimits_.image,image,diagnostic)||
       !renderer::resampleLensDirt(image.rgba.data(),image.width,image.height,*texture)) {
      reportProblem(EditorConsoleSeverity::Warning,"Textura de sujeira ilegível: "+record->path+
                    (diagnostic.empty()?std::string():" ("+diagnostic+")"));
      continue;
    }
    if(cached!=lensDirtTextures_.end()) {
      cached->second=texture;lensDirtHashes_[static_cast<usize>(cached-lensDirtTextures_.begin())]=record->contentHash;
    } else {lensDirtTextures_.push_back({guid,texture});lensDirtHashes_.push_back(record->contentHash);}
  }
  return lensDirtTextures_;
}

} // namespace ae::editor

#include "scene/path.h"
#include "editor/editor_component_impact.h"
#include "editor/editor_component_visuals.h"
#include "editor/editor_color_picker.h"
#include "editor/editor_component_catalog.h"
#include "editor/editor_project_tags.h"
#include "editor/editor_collider_fit.h"
#include "editor/editor_lod_group.h"
#include "scene/script_behavior.h"
#include "scene/prefab_link.h"
#include "scene/collision_recipe.h"
#include "editor/editor_water_body_component.h"
#include "editor/editor_route_component.h"
#include "editor/editor_creation_catalog.h"
#include "renderer/primitive_geometry.h"
#include "runtime/primitive_object.h"
#include "editor/editor_session.h"
#include "editor/editor_property_tween.h"
#include "scene/property_tween.h"
#include "editor/editor_number_text.h"
#include "editor/editor_scene_template.h"
#include "scene/environment.h"
#include "resources/import_report.h"
#include "editor/editor_import_transaction.h"
#include "resources/texture_compression.h"
#include "resources/image_decode.h"
#include <fstream>
#include "resources/gltf_package.h"
#include "scene/import_link.h"
#include "core/sha256.h"
#include "editor/editor_script_templates.h"
#include "editor/editor_reference_picker.h"
#include "editor/editor_properties.h"
#include "editor/editor_theme.h"
#include "renderer/water_authoring_geometry.h"

#include <algorithm>
#include <map>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <cmath>
#include <cstdio>
#include <sstream>
#include "editor/editor_global_search.h"
#include "scene/component_schema.h"
#include <locale>
#include <cstring>
#include <cstdlib>
#include <limits>

namespace ae::editor {
namespace {

using namespace ae::ui;

// Pastas do projeto até o arquivo, para a trilha do Inspector de textura.
std::vector<std::string> folderTrail(const std::string &path) {
  std::vector<std::string> parts;
  usize begin=0;
  for(usize slash=path.find('/');slash!=std::string::npos;begin=slash+1,slash=path.find('/',begin))
    if(slash>begin) parts.push_back(path.substr(begin,slash-begin));
  return parts;
}
std::string baseName(const std::string &path) {
  const auto slash=path.find_last_of('/');
  return slash==std::string::npos?path:path.substr(slash+1);
}
std::string megabytesText(u64 bytes) {return decimalText(static_cast<double>(bytes)/1048576.0,bytes<(10ull<<20)?1:0)+" MB";}

float distanceBetween(UiPoint a, UiPoint b) noexcept {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  return std::sqrt(dx * dx + dy * dy);
}

} // namespace

u64 EditorSession::nextSceneEpoch() noexcept {
  static std::atomic<u64> sequence{1};
  return sequence.fetch_add(1,std::memory_order_relaxed);
}

void EditorSession::initialize(const UiFont *font, const UiIconAtlas *icons) {
  font_ = font;
  icons_ = icons;
  state_.creationAvailable=creationAlwaysAvailable();
  state_.document = &document_;
  state_.resources = &mapScene_;
  state_.assetRegistry = &assets_;

}

EditorActionResult EditorSession::dispatch(const EditorActionRequest &request) {
  auto result=[&](EditorActionStatus status,EditorEntityId entity=0) {
    return EditorActionResult{status,sceneVersion(),entity};
  };
  const auto version=sceneVersion();
  if(request.version.epoch!=version.epoch || request.version.revision!=version.revision)
    return result(EditorActionStatus::StaleScene);
  if(isPlaying() || history_.isOpen()) return result(EditorActionStatus::Busy);
  const auto *entity=document_.find(request.entity);
  if(request.action!=EditorAction::Undo && request.action!=EditorAction::Redo && !entity)
    return result(EditorActionStatus::InvalidTarget);
  bool applied=false;
  auto changed=request.entity;
  switch(request.action) {
    case EditorAction::Select: setSelection(request.entity);applied=true;break;
    case EditorAction::FrameSelection: setSelection(request.entity);frameSelection();applied=true;break;
    case EditorAction::Rename: {
      if(request.entity==document_.root() || request.name.empty() ||
         request.name.size()>=kEditorNameCapacity || request.name.find('\0')!=std::string::npos) break;
      auto values=*entity;assignEntityName(values,request.name);
      applied=history_.applyValues(document_,request.entity,values);break;
    }
    case EditorAction::Transform:
      applied=history_.setTransform(document_,request.entity,request.transform);break;
    case EditorAction::ComponentProperty: {
      if(const auto *target=std::get_if<scene::ObjectReference>(&request.componentValue)) {
        const auto *property=editorReferenceProperty(*entity,request.componentInstance,request.componentProperty);
        if(!property||!editorReferenceAccepts(document_,request.entity,*property,target->id)) break;
      }
      auto values=*entity;
      if(scene::setComponentProperty(values.components,request.componentType,request.componentProperty,
          request.componentValue,request.componentInstance)==scene::ComponentPropertyStatus::Applied)
        applied=history_.applyValues(document_,request.entity,values);
      break;
    }
    case EditorAction::ComponentSlotProperty: {
      auto values=*entity;
      if(scene::setComponentSlotProperty(values.components,request.componentType,request.componentProperty,
          request.componentSlot,request.componentValue,request.componentInstance)==scene::ComponentPropertyStatus::Applied)
        applied=history_.applyValues(document_,request.entity,values);
      break;
    }
    case EditorAction::ComponentResource: {
      const auto *source=entity->components.findInstance(request.componentInstance);
      if(!source) break;
      const scene::ComponentResourceBinding *binding=nullptr;
      for(const auto &candidate:source->type().resourceBindings) if(candidate.id==request.componentProperty) {
        if(binding) {binding=nullptr;break;}
        binding=&candidate;
      }
      if(!binding||!binding->write||request.componentResourceSlot>=binding->slotCount(*source) ||
         !binding->presentation.isEditable(*source)) break;
      if(request.componentResource.valid()) {
        const auto *record=assets_.find(request.componentResource);
        if(record&&record->type!=binding->kind) break;
        if(binding->kind==resources::AssetType::EnvironmentProfile&&
           (!record||record->type!=resources::AssetType::EnvironmentProfile)) break;
        if(binding->kind==resources::AssetType::PhysicsMaterial&&
           (!record||record->type!=resources::AssetType::PhysicsMaterial||!findPhysicsMaterial(request.componentResource))) break;
        if(binding->kind==resources::AssetType::AnimatorController&&
           (!record||!resources::findAnimatorController(animatorControllers_,request.componentResource))) {
          state_.status="Controller ausente ou inválido";break;
        }
        if(binding->kind==resources::AssetType::Mesh&&!mapScene_.assetSlot(request.componentResource)) break;
        if(binding->kind==resources::AssetType::Material&&!mapScene_.sharedMaterial(request.componentResource)) break;
        if(binding->kind==resources::AssetType::Texture&&(!record||record->type!=resources::AssetType::Texture)) break;
        if(binding->kind==resources::AssetType::EnvironmentMap&&
           (!record||record->type!=resources::AssetType::EnvironmentMap||
            !findEnvironmentMap(request.componentResource))) break;
        // Clipe: sub-recurso de uma fonte carregada nesta sessão.
        runtime::AnimationClipView clipView;
        if(binding->kind==resources::AssetType::AnimationClip&&!mapScene_.findClip(request.componentResource,clipView)) break;
      } else if(!binding->inheritable && !binding->none.valid() &&
                binding->kind!=resources::AssetType::AnimationClip && binding->kind!=resources::AssetType::AnimatorController && binding->id!="texture.lightmap") break;
      const auto authored=request.componentResource.valid()?request.componentResource:
          binding->inheritable?resources::AssetGuid{}:binding->none;
      auto values=*entity;auto *candidate=values.components.editInstance(request.componentInstance);
      if(!candidate||!binding->write(*candidate,request.componentResourceSlot,authored)) break;
      if(binding->kind==resources::AssetType::AnimatorController) {
        auto &instance=scene::animator(*candidate);
        if(authored.valid()) {
          const auto *asset=resources::findAnimatorController(animatorControllers_,authored);if(!asset) break;
          auto masks=scene::animator(*source).controller==authored?instance.layers:std::vector<scene::AnimatorLayer>{};instance.parameters=asset->graph.parameters;instance.layers=asset->graph.layers;instance.nextId=asset->graph.nextId;
          for(auto &layer:instance.layers) for(const auto &local:masks) if(local.id==layer.id) layer.mask=local.mask;
        } else if(scene::animator(*source).controller.valid()) {
          scene::Animator resolved;std::string diagnostic;
          if(!resources::resolveAnimatorController(scene::animator(*source),animatorControllers_,resolved,diagnostic)) break;
          resolved.controller={};resolved.clipOverrides.clear();scene::replaceAnimatorData(instance,resolved);
        }
      }
      // Escolher um material copia os valores dele para o corpo, na mesma transação.
      if(binding->kind==resources::AssetType::PhysicsMaterial&&authored.valid()) {
        const auto *material=findPhysicsMaterial(authored);
        if(!material||!applyPhysicsMaterialCopy(*candidate,*material)) break;
      }
      if(binding->kind==resources::AssetType::EnvironmentProfile&&authored.valid()) {
        auto *environment=&candidate->type()==&scene::Environment::descriptor?
            static_cast<scene::Environment*>(candidate):nullptr;
        const auto *profile=findEnvironmentProfile(authored);
        if(!environment||!profile) break;
        environment->values=resources::applyEnvironmentProfile(environment->values,*profile);
        if(environment->values.environmentMap.valid()&&
           !findEnvironmentMap(environment->values.environmentMap)) break;
      }
      if(candidate&&candidate->valid()) {
        if(binding->kind==resources::AssetType::Texture && authored.valid() &&
           !decodeProjectTexture(authored,binding->id!="texture.lightmap",
             binding->id=="texture.lightmap"?EditorMapScene::samplerFlags(scene::lightmapSampling()):EditorMapScene::DefaultTextureSampler)) break;
        applied=history_.applyValues(document_,request.entity,values);
        if(applied && binding->kind==resources::AssetType::Texture) {
          std::string diagnostic;
          if(!ensureTexturesPublished(diagnostic)) reportProblem(EditorConsoleSeverity::Warning,"Publicação da textura pendente: "+diagnostic);
        }
      }
      break;
    }
    case EditorAction::AddComponent: {
      const auto *entry=findEditorComponent(request.componentType);
      if(!entry) break;
      auto plan=scene::planComponentAddition(entity->components,request.componentType);if(!plan.ready) break;
      auto values=*entity;values.components=std::move(plan.candidate);
      auto *added=values.components.editInstance(plan.requestedInstance);if(!added) break;
      if(entry->type==&EditorCollider::descriptor && meshAsset(*entity)) {
        // Missing geometry leaves the explicit default shape available; a fit
        // only publishes a fully prepared conservative candidate.
        auto *collider=editCollider(values,added->instanceId());if(collider) fitEditorCollider(mapScene_,document_,request.entity,*collider);
      }
      applied=history_.applyValues(document_,request.entity,values);break;
    }
    case EditorAction::AddScript: {
      for(const auto &type:code_.scriptTypes()) if(type.id==request.scriptType) {
        auto values=*entity;auto *base=values.components.add(scene::ScriptBehavior::descriptor);if(!base) break;
        auto &script=static_cast<scene::ScriptBehavior &>(*base);script.scriptType=type.id;script.source=type.file;
        if(!script.valid()) break;
        applied=history_.applyValues(document_,request.entity,values);break;
      }
      break;
    }
    case EditorAction::RemoveComponent: {
      if(scene::componentInstanceRemovalBlockedBy(request.componentInstance,entity->components) ||
         runtime::componentRemovalReferenceUse(document_,request.entity,request.componentInstance).object) break;
      auto values=*entity;
      if(request.componentInstance&&values.components.removeInstance(request.componentInstance))
        applied=history_.applyValues(document_,request.entity,values);
      break;
    }
    case EditorAction::ScriptProperty:
    case EditorAction::ScriptEnabled: {
      const auto *script=scene::scriptBehavior(entity->components.findInstance(request.componentInstance));
      if(!script) break;
      auto replacement=*script;
      if(request.action==EditorAction::ScriptEnabled) replacement.enabled=request.enabled;
      else {
        bool declared=false;
        for(const auto &type:code_.scriptTypes()) if(type.id==script->scriptType)
          for(const auto &property:type.properties) if(property.id==request.componentProperty&&property.valueType==request.scriptPropertyType) declared=true;
        if(!declared||!replacement.setProperty(request.componentProperty,request.scriptPropertyType,request.scriptPropertyValue)) break;
      }
      auto values=*entity;
      if(values.components.replaceInstance(request.componentInstance,replacement)) applied=history_.applyValues(document_,request.entity,values);
      break;
    }
    case EditorAction::FitCollider: {
      auto values=*entity;auto *collider=editCollider(values,request.componentInstance);
      if(collider && fitEditorCollider(mapScene_,document_,request.entity,*collider)) applied=history_.applyValues(document_,request.entity,values);
      break;
    }
    case EditorAction::GenerateCollisionMesh: {
      std::string diagnostic;
      applied=generateCollisionMesh(request.entity,request.componentInstance,
                                    static_cast<u8>(request.property),request.number,diagnostic);
      state_.status=diagnostic;
      break;
    }
    case EditorAction::AssignMesh: {
      if(!meshRenderer(*entity) || entity->kind==EditorEntityKind::Water || request.property>mapScene_.assetCount()) break;
      if(request.property && (mapScene_.materialFlagsForAsset(request.property-1)&renderer::MapMaterialWater)) break;
      auto values=*entity;auto *mesh=editMeshRenderer(values);if(!mesh) break;
      mesh->mesh=request.property;
      if(!mesh->material.enabled) mesh->material=request.property?mapScene_.materialForAsset(request.property-1):scene::MaterialParameters{};
      applied=history_.applyValues(document_,request.entity,values);break;
    }
    case EditorAction::RestoreMaterial: {
      if(!meshRenderer(*entity)) break;
      auto values=*entity;auto *mesh=editMeshRenderer(values);if(!mesh) break;
      mesh->material=mesh->mesh?mapScene_.materialForAsset(mesh->mesh-1):scene::MaterialParameters{};
      applied=history_.applyValues(document_,request.entity,values);break;
    }
    case EditorAction::NumericProperty: {
      auto values=*entity;
      if(setEditorPropertyValue(values,request.property,request.number))
        applied=history_.applyValues(document_,request.entity,values);
      break;
    }
    case EditorAction::Duplicate: {
      // Vários selecionados: um duplicado por raiz, um passo de Desfazer, e a
      // seleção passa a ser as cópias (Unity: Duplicate na multisseleção).
      const auto roots=state_.isSelected(request.entity) && state_.selectionSet.size()>1?selectedRoots():
          std::vector<EditorEntityId>{request.entity};
      std::vector<EditorEntityId> copies;
      for(const auto id:roots) if(const auto copy=history_.duplicateEntity(document_,id)) copies.push_back(copy);
      if(copies.size()>1) history_.mergeLast(static_cast<u32>(copies.size()),"Duplicar · "+std::to_string(copies.size())+" objetos");
      applied=!copies.empty();
      if(applied) {changed=copies.back();setSelection(changed);state_.selectionSet=copies;}
      break;
    }
    case EditorAction::Remove: {
      const auto roots=state_.isSelected(request.entity) && state_.selectionSet.size()>1?selectedRoots():
          std::vector<EditorEntityId>{request.entity};
      u32 removed=0;
      for(const auto id:roots) removed+=history_.destroyEntity(document_,id);
      if(removed>1) history_.mergeLast(removed,"Excluir · "+std::to_string(removed)+" objetos");
      applied=removed>0;
      break;
    }
    case EditorAction::Reparent:
      applied=history_.reparentKeepingWorld(document_,request.entity,request.parent);break;
    case EditorAction::Undo: applied=history_.undo(document_);break;
    case EditorAction::Redo: applied=history_.redo(document_);break;
  }
  if(!document_.exists(state_.selection)) state_.selection=kInvalidEntity;
  return result(applied?EditorActionStatus::Applied:EditorActionStatus::InvalidValue,changed);
}

namespace {
// Perfil de importação de textura campo a campo, na ordem dos widgets
// TextureProfileInterpretation…StreamingPriority: rótulo, cópia e liga/desliga,
// comuns ao Inspector de uma textura e ao de várias.
constexpr u32 TextureProfileFields=10;
bool textureProfileToggle(u32 field) {return field==2||field==3||field==4||field==5||field==6||field==8;}
std::string textureProfileFieldText(const resources::TextureProfile &profile,u32 field) {
  static constexpr const char *interpretations[]{"Tipo: pelo uso","Tipo: cor (sRGB)","Tipo: dado (linear)","Tipo: mapa normal"};
  switch(field) {
    case 0: return interpretations[std::min<u32>(profile.interpretation,3u)];
    case 1: return profile.maximumDimension?"Tamanho: até "+std::to_string(profile.maximumDimension)+" px":std::string("Tamanho: teto do projeto");
    case 2: return profile.mipmaps?"Mipmaps: sim":"Mipmaps: não";
    case 3: return profile.dilateEdges?"Bordas: sem halo":"Bordas: do arquivo";
    case 4: return profile.anisotropy?"Anisotropia: da qualidade":"Anisotropia: desligada";
    case 5: return profile.invertNormalGreen?"Normal Y: inverter (DX)":"Normal Y: manter (GL)";
    case 6: return profile.preserveAlphaCoverage?"Cobertura alfa: preservar":"Cobertura alfa: desligada";
    case 7: return "Corte da cobertura: "+std::to_string(static_cast<u32>(std::lround(profile.alphaCoverageCutoff*100.0f)))+"%";
    case 8: return profile.streamingMipmaps?"Streaming de mips: sim":"Streaming de mips: não";
    default: return "Prioridade: "+std::to_string(profile.streamingPriority);
  }
}
void copyTextureProfileField(resources::TextureProfile &to,const resources::TextureProfile &from,u32 field) {
  switch(field) {
    case 0: to.interpretation=from.interpretation;break;
    case 1: to.maximumDimension=from.maximumDimension;break;
    case 2: to.mipmaps=from.mipmaps;break;
    case 3: to.dilateEdges=from.dilateEdges;break;
    case 4: to.anisotropy=from.anisotropy;break;
    case 5: to.invertNormalGreen=from.invertNormalGreen;break;
    case 6: to.preserveAlphaCoverage=from.preserveAlphaCoverage;break;
    case 7: to.alphaCoverageCutoff=from.alphaCoverageCutoff;break;
    case 8: to.streamingMipmaps=from.streamingMipmaps;break;
    default: to.streamingPriority=from.streamingPriority;break;
  }
}
void setTextureProfileToggle(resources::TextureProfile &profile,u32 field,bool on) {
  switch(field) {
    case 2: profile.mipmaps=on;break;
    case 3: profile.dilateEdges=on;break;
    case 4: profile.anisotropy=on;break;
    case 5: profile.invertNormalGreen=on;break;
    case 6: profile.preserveAlphaCoverage=on;break;
    case 8: profile.streamingMipmaps=on;break;
    default: break;
  }
}
// Campos de um material do projeto como o Inspector os mostra (chaves de
// buildMaterialSlots): quais diferem entre dois materiais, e a cópia do que
// mudou (antes × depois) para outro — números pelos mesmos descritores do
// MeshRenderer que o Inspector lê e grava.
void materialDifferences(const resources::MaterialAsset &a,const resources::MaterialAsset &b,std::vector<std::string> &out) {
  const auto add=[&](bool differ,std::string key) {if(differ && std::find(out.begin(),out.end(),key)==out.end()) out.push_back(std::move(key));};
  for(u32 i=0;i<scene::MaterialTextureCount;++i) {
    add(!(a.textures[i]==b.textures[i]),"tex"+std::to_string(i));
    const auto key="uv"+std::to_string(i)+".";const auto &x=a.sampling[i],&y=b.sampling[i];
    add(x.uvSet!=y.uvSet,key+"set");add(x.wrap!=y.wrap,key+"wrap");add(x.filter!=y.filter,key+"filter");
    for(u32 k=0;k<2;++k) {add(x.offset[k]!=y.offset[k],key+"offset"+std::to_string(k));add(x.scale[k]!=y.scale[k],key+"scale"+std::to_string(k));}
    add(x.rotation!=y.rotation,key+"rotation");
  }
  add(a.surface.alphaMode!=b.surface.alphaMode,"alpha");add(a.surface.sides!=b.surface.sides,"sides");
  add(a.surface.alphaCutoff!=b.surface.alphaCutoff,"cutoff");
  add(a.channels.occlusionSource!=b.channels.occlusionSource,"ch0");add(!(a.occlusionTexture==b.occlusionTexture),"ch1");
  add(a.channels.occlusionStrength!=b.channels.occlusionStrength,"ch2");
  add(a.channels.roughness!=b.channels.roughness,"ch3.0");add(a.channels.metallic!=b.channels.metallic,"ch3.1");
  add(a.channels.occlusion!=b.channels.occlusion,"ch3.2");
  add(a.channels.normalFlipY!=b.channels.normalFlipY,"ch4");add(a.channels.alphaSource!=b.channels.alphaSource,"ch5");
  scene::MeshRenderer pa,pb;pa.material=a.values;pb.material=b.values;
  for(u32 field=0;field<scene::meshRendererNumbers.size();++field)
    add(scene::meshRendererNumbers[field].read(pa)!=scene::meshRendererNumbers[field].read(pb),"num"+std::to_string(field));
}
bool copyMaterialChange(const resources::MaterialAsset &before,const resources::MaterialAsset &after,
                        resources::MaterialAsset &target,std::string_view field) {
  bool changed=false;
  const auto copy=[&](auto &to,const auto &from,const auto &was,const std::string &key) {
    if((field==key || !(from==was)) && !(to==from)) {to=from;changed=true;}
  };
  for(u32 i=0;i<scene::MaterialTextureCount;++i) {
    const auto key="tex"+std::to_string(i),uv="uv"+std::to_string(i)+".";
    copy(target.textures[i],after.textures[i],before.textures[i],key);
    auto &to=target.sampling[i];const auto &from=after.sampling[i],&was=before.sampling[i];
    copy(to.uvSet,from.uvSet,was.uvSet,uv+"set");copy(to.wrap,from.wrap,was.wrap,uv+"wrap");
    copy(to.filter,from.filter,was.filter,uv+"filter");
    for(u32 k=0;k<2;++k) {
      copy(to.offset[k],from.offset[k],was.offset[k],field==uv+"reset"?std::string(field):uv+"offset"+std::to_string(k));
      copy(to.scale[k],from.scale[k],was.scale[k],field==uv+"reset"?std::string(field):uv+"scale"+std::to_string(k));
    }
    copy(to.rotation,from.rotation,was.rotation,field==uv+"reset"?std::string(field):uv+"rotation");
  }
  copy(target.surface.alphaMode,after.surface.alphaMode,before.surface.alphaMode,"alpha");
  copy(target.surface.sides,after.surface.sides,before.surface.sides,"sides");
  copy(target.surface.alphaCutoff,after.surface.alphaCutoff,before.surface.alphaCutoff,"cutoff");
  auto &ch=target.channels;const auto &now=after.channels,&old=before.channels;
  copy(ch.roughness,now.roughness,old.roughness,"ch3.0");copy(ch.metallic,now.metallic,old.metallic,"ch3.1");
  copy(ch.occlusion,now.occlusion,old.occlusion,"ch3.2");copy(ch.occlusionSource,now.occlusionSource,old.occlusionSource,"ch0");
  copy(ch.occlusionStrength,now.occlusionStrength,old.occlusionStrength,"ch2");
  copy(ch.normalFlipY,now.normalFlipY,old.normalFlipY,"ch4");copy(ch.alphaSource,now.alphaSource,old.alphaSource,"ch5");
  copy(target.occlusionTexture,after.occlusionTexture,before.occlusionTexture,"ch1");
  copy(target.values.enabled,after.values.enabled,before.values.enabled,field.starts_with("num")?std::string(field):"enabled");
  scene::MeshRenderer was,now2,to;was.material=before.values;now2.material=after.values;to.material=target.values;
  for(u32 i=0;i<scene::meshRendererNumbers.size();++i) {
    const auto &number=scene::meshRendererNumbers[i];const float value=number.read(now2);
    if((field=="num"+std::to_string(i) || value!=number.read(was)) && number.write && value!=number.read(to)) {
      *number.write(to)=value;changed=true;
    }
  }
  target.values=to.material;
  return changed;
}
std::string materialWidgetField(u32 widget,u32 binding) {
  if(widget>=widgetId(EditorWidget::MaterialNumberBase) && widget-widgetId(EditorWidget::MaterialNumberBase)<scene::meshRendererNumbers.size())
    return "num"+std::to_string(widget-widgetId(EditorWidget::MaterialNumberBase));
  if(widget>=widgetId(EditorWidget::MaterialTextureBase) && widget-widgetId(EditorWidget::MaterialTextureBase)<scene::MaterialTextureCount)
    return "tex"+std::to_string(widget-widgetId(EditorWidget::MaterialTextureBase));
  const auto uv="uv"+std::to_string(binding)+".";
  if(widget>=widgetId(EditorWidget::TextureUvStepBase) && widget-widgetId(EditorWidget::TextureUvStepBase)<10) {
    static constexpr const char *keys[]{"offset0","offset1","scale0","scale1","rotation"};
    return uv+keys[(widget-widgetId(EditorWidget::TextureUvStepBase))/2];
  }
  switch(static_cast<EditorWidget>(widget)) {
    case EditorWidget::MaterialAlphaCycle:return "alpha";
    case EditorWidget::MaterialSidesCycle:return "sides";
    case EditorWidget::MaterialCutoffDown:case EditorWidget::MaterialCutoffUp:return "cutoff";
    case EditorWidget::MaterialOcclusionSourceCycle:return "ch0";
    case EditorWidget::MaterialOcclusionTexture:return "ch1";
    case EditorWidget::MaterialOcclusionStrengthDown:case EditorWidget::MaterialOcclusionStrengthUp:return "ch2";
    case EditorWidget::MaterialChannelRoughness:return "ch3.0";
    case EditorWidget::MaterialChannelMetallic:return "ch3.1";
    case EditorWidget::MaterialChannelOcclusion:return "ch3.2";
    case EditorWidget::MaterialNormalFlipCycle:return "ch4";
    case EditorWidget::MaterialAlphaSourceCycle:return "ch5";
    case EditorWidget::TextureSamplingUv:return uv+"set";
    case EditorWidget::TextureSamplingWrap:return uv+"wrap";
    case EditorWidget::TextureSamplingFilter:return uv+"filter";
    default:return {};
  }
}
std::string materialFieldText(const resources::MaterialAsset &material,std::string_view key) {
  const auto number=[](float value) {char text[32];std::snprintf(text,sizeof(text),"%.6g",static_cast<double>(value));return std::string(text);};
  if(key.starts_with("num")) {
    const auto i=static_cast<u32>(std::stoul(std::string(key.substr(3))));
    scene::MeshRenderer probe;probe.material=material.values;
    return i<scene::meshRendererNumbers.size()?number(scene::meshRendererNumbers[i].read(probe)):std::string();
  }
  if(key=="alpha") {static constexpr const char *labels[]{"Como a fonte","Opaco","Recorte","Transparente"};return labels[material.surface.alphaMode];}
  if(key=="sides") {static constexpr const char *labels[]{"Como a fonte","Uma face","Dupla face"};return labels[material.surface.sides];}
  if(key=="cutoff") return number(material.surface.alphaCutoff);
  if(key=="ch0") {static constexpr const char *labels[]{"Como a fonte","Sem oclusão","Metal/rugosidade","Textura própria"};return labels[material.channels.occlusionSource];}
  if(key=="ch2") return material.channels.occlusionStrength<0?"Como a fonte":number(material.channels.occlusionStrength);
  if(key.starts_with("ch3.")) {
    const u8 values[]{material.channels.roughness,material.channels.metallic,material.channels.occlusion};
    static constexpr const char *labels[]{"Como a fonte","R","G","B","A"};return labels[values[key.back()-'0']];
  }
  if(key=="ch4") {static constexpr const char *labels[]{"Como a fonte","OpenGL","DirectX"};return labels[material.channels.normalFlipY];}
  if(key=="ch5") {static constexpr const char *labels[]{"Como a fonte","Alfa da cor base","Ignorar alfa","Luminância"};return labels[material.channels.alphaSource];}
  if(key.starts_with("uv") && key.size()>4 && key[2]>='0' && static_cast<u32>(key[2]-'0')<scene::MaterialTextureCount) {
    const auto &uv=material.sampling[key[2]-'0'];const auto field=key.substr(4);
    if(field=="set") {static constexpr const char *labels[]{"Herdar","UV 0","UV 1","Mundo"};return labels[uv.uvSet];}
    if(field=="wrap") {static constexpr const char *labels[]{"Herdar","Repetir","Limitar","Espelhar"};return labels[uv.wrap];}
    if(field=="filter") {static constexpr const char *labels[]{"Herdar","Linear","Próximo"};return labels[uv.filter];}
    if(field.starts_with("offset")) return number(uv.offset[field.back()-'0']);
    if(field.starts_with("scale")) return number(uv.scale[field.back()-'0']);
    if(field=="rotation") return number(uv.rotation);
  }
  return {};
}
// Tipos de arquivo que o Inspector de vários recursos agrupa (a ordem é a dos
// grupos na tela).
struct FileKind {const char *label,*plural;ui::UiIcon icon;};
constexpr FileKind fileKinds[]{
  {"textura","Texturas",ui::UiIcon::AssetsTexture},{"material","Materiais",ui::UiIcon::AssetsMaterial},
  {"modelo","Modelos",ui::UiIcon::AssetsFileMesh},{"mapa HDRI","Mapas HDRI",ui::UiIcon::LightingSky},
  {"perfil","Perfis de ambiente",ui::UiIcon::LightingSceneEffects},{"script","Scripts",ui::UiIcon::ScriptingCode},
  {"cena","Cenas",ui::UiIcon::SceneObject},{"arquivo","Outros arquivos",ui::UiIcon::AssetsFile}};
// Tipo de componente para casar entre objetos: o id do tipo, ou o tipo do
// script (todos os comportamentos C# compartilham o mesmo tipo nativo).
std::string componentKind(const scene::ComponentValue &value) {
  if(const auto *script=scene::scriptBehavior(&value)) return "script:"+script->scriptType;
  return std::string(value.type().id);
}
u32 componentOccurrence(const scene::Components &components,const scene::ComponentValue &value) {
  const auto kind=componentKind(value);u32 occurrence=0;
  for(usize i=0;i<components.size();++i) {
    const auto *other=components.at(i);if(other==&value) break;
    if(componentKind(*other)==kind) ++occurrence;
  }
  return occurrence;
}
const scene::ComponentValue *matchComponent(const scene::Components &components,std::string_view kind,u32 occurrence) {
  u32 seen=0;
  for(usize i=0;i<components.size();++i) if(componentKind(*components.at(i))==kind) {if(seen==occurrence) return components.at(i);++seen;}
  return nullptr;
}
// Componentes cujo editor é por objeto (vínculo com a fonte, barra do LOD,
// blend shapes da malha, tipo ausente): a Unity também não os edita juntos.
bool multiEditable(const scene::ComponentValue &value) {
  const auto &type=value.type();
  return &type!=&scene::ImportLink::descriptor && &type!=&scene::PrefabLink::descriptor && &type!=&scene::LodGroup::descriptor &&
         &type!=&scene::SkinnedMesh::descriptor && !value.unresolved();
}
std::string transformChannel(const EditorTransform &t,u32 row,u32 axis) {
  const float v=row==0?t.position[axis]:row==1?t.rotationDegrees[axis]:t.scale[axis];
  char text[32];std::snprintf(text,sizeof text,"%.4g",static_cast<double>(v));return text;
}
std::string propertyText(const scene::ComponentValue &value,std::string_view property) {
  char text[48];
  for(const auto &p:value.type().numbers) if(p.id==property) {std::snprintf(text,sizeof text,"%.5g",static_cast<double>(p.read(value)));return text;}
  for(const auto &p:value.type().booleans) if(p.id==property) return p.read(value)?"ligado":"desligado";
  for(const auto &p:value.type().enums) if(p.id==property) {
    const u32 current=p.read(value);for(const auto &o:p.options) if(o.value==current) return o.name;
    return std::to_string(current);
  }
  for(const auto &p:value.type().references) if(p.id==property) {const auto target=p.read(value);return target?"#"+std::to_string(target):"nenhum";}
  if(const auto *script=scene::scriptBehavior(&value))
    for(const auto &p:script->properties) if(p.id==property) return p.value;
  return {};
}
}

void EditorSession::setSurface(const UiRect &surface, const UiInsets &safeArea) {
  state_.surface = surface;
  state_.safeArea = safeArea;
}

void EditorSession::setSelection(EditorEntityId entity) {
  if (!document_.exists(entity)) return;
  gui_.history().commit(gui_.document());gui_.cancelPointers();gui_.immediate().cancelInput();
  state_.guiSelection={};state_.guiInspector=false;
  // Escolher um objeto tira o material do projeto de Propriedades.
  if(materialAssetMode()) {state_.materialInspector={};state_.materialShared=false;state_.texturePicker=false;}
  state_.environmentInspector={};state_.profileInspector={};state_.physicsMaterialInspector={};
  if(state_.selection!=entity) {state_.routePoint=0;state_.propertyPage=0;state_.propertyQuery.clear();state_.componentPage=0;state_.componentPreview=0;state_.scriptPreviewType.clear();state_.expandedScript=0;state_.scriptMenu=0;state_.importLinkMenu=false;
    state_.impactInstance=0;state_.impactAsset={};state_.impactTrail.clear();state_.impactRemoval=false;
    state_.materialSlot=0;state_.materialShared=false;state_.materialPicker=false;state_.inspectorMenu=false;state_.transformMenu=false;}
  state_.selection=entity;
  if(!state_.multiSelect || !state_.isSelected(entity)) state_.selectionSet={entity};
  // Reveal the selected object through collapsed ancestors and long lists.
  for(auto parent=document_.find(entity);parent;parent=document_.find(parent->parent)) {
    auto &collapsed=state_.collapsedEntities;
    collapsed.erase(std::remove(collapsed.begin(),collapsed.end(),parent->parent),collapsed.end());
  }
  std::vector<EditorEntityId> stack;
  const auto roots=document_.childrenOf(document_.root());
  for(auto it=roots.rbegin();it!=roots.rend();++it) stack.push_back(*it);
  u32 row=0;
  while(!stack.empty()) {
    const auto id=stack.back();stack.pop_back();
    if(id==entity) {
      const auto visible=std::max(1u,layout_.hierarchyVisibleRows);
      if(row<state_.hierarchyScroll) state_.hierarchyScroll=row;
      else if(row>=state_.hierarchyScroll+visible) state_.hierarchyScroll=row-visible+1;
      break;
    }
    ++row;
    if(std::find(state_.collapsedEntities.begin(),state_.collapsedEntities.end(),id)!=state_.collapsedEntities.end()) continue;
    const auto children=document_.childrenOf(id);
    for(auto it=children.rbegin();it!=children.rend();++it) stack.push_back(*it);
  }
}

void EditorSession::setCameraPose(const float position[3], float yaw, float pitch) {
  EditorCamera camera;camera.yaw=yaw;camera.pitch=pitch;
  camera.target[0]=position[0]+std::cos(pitch)*std::sin(yaw)*camera.distance;
  camera.target[1]=position[1]-std::sin(pitch)*camera.distance;
  camera.target[2]=position[2]+std::cos(pitch)*std::cos(yaw)*camera.distance;
  if(isEditorCameraValid(camera)) camera_=camera;
}

void EditorSession::setProjectName(const char *name) {
  if (name != nullptr) state_.projectName = name;
}

bool EditorSession::setProjectDirectory(const char *path) {
  if(code_.dirty()) {state_.status="Salve os arquivos de código antes de trocar de projeto";return false;}
  if(!files_.rootPath().empty()&&gui_.dirty()) {state_.status="Salve o documento UI antes de trocar ou reabrir o projeto";return false;}
  if(path && *path) {
    std::string recovery;
    if(!EditorImportTransaction::recover(path,recovery)) {state_.status=recovery;return false;}
    if(!recovery.empty()) reportProblem(EditorConsoleSeverity::Warning,recovery);
    EditorImportTransaction::discardStaging(path);
  }
  code_.clear();state_.code=&code_;
  state_.files=&files_;state_.fileScroll=0;state_.fileScrollOffset=0;
  state_.console=&console_;
  closeImportPreview();state_.importAccept=false;state_.importCancel=false;
  state_.modelImportRequested=false;state_.environmentImportRequested=false;state_.textureImportRequested=false;
  state_.folderImportRequested=false;state_.waveImportRequested=false;
  reimportPath_.clear();environmentReimportPath_.clear();textureReimportPath_.clear();
  runtime::ObjectTags tags;std::string tagError;
  if(!path || !loadProjectTags(path,tags,tagError)) {state_.status=tagError;return false;}
  cancelPointers();gui_.immediate().cancelInput();
  if(!files_.setRoot(path)) return false;
  guiTree_.clear();state_.guiRows={};state_.guiSelection={};state_.guiInspector=false;state_.collapsedGui.clear();
  guiImages_.clear();gui_.setImages(&guiImages_);playScene_.gui().setImages(&guiImages_);playScene_.sceneGui().setImages(&guiImages_);
  gui_.setImageChoices([this](){
    std::vector<std::string> paths;std::error_code ec;const std::filesystem::path root(files_.rootPath());
    std::filesystem::recursive_directory_iterator it(root,std::filesystem::directory_options::skip_permission_denied,ec),end;
    for(u32 visited=0;it!=end&&!ec&&visited<8192;it.increment(ec),++visited) {
      if(it->is_symlink(ec) || it->path().filename()==".astra") {if(it->is_directory(ec))it.disable_recursion_pending();continue;}
      if(!it->is_regular_file(ec))continue;
      auto extension=it->path().extension().string();std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
      if(extension==".png" || extension==".jpg" || extension==".jpeg" || extension==".ktx2")paths.push_back(it->path().lexically_relative(root).generic_string());
      if(paths.size()==4096)break;
    }
    std::sort(paths.begin(),paths.end());return paths;
  });
  gui_.setInputActions([this](){
    std::vector<ui::GuiWorkbench::InputActionChoice> choices;
    for(const auto &a:document_.inputActions().actions())if(a.kind!=runtime::ActionKind::Axis1D)
      choices.push_back({a.id,a.kind==runtime::ActionKind::Button,a.interaction==runtime::InputInteraction::Press});
    return choices;
  });
  gui_.document()=ui::GuiDocument{};gui_.history().clear();gui_.select(0);gui_.setPreview(false);
  gui_.setResource("UI/main.aeui");gui_.setDiagnostic("");
  gui_.setStorage([this](ui::GuiDocument &document,std::string_view relative,bool save,std::string &error) {
    std::filesystem::path file;
    const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());
    if (!EditorImportTransaction::safePath(root,std::string(relative),file) || file.extension()!=".aeui") {
      error="Use um caminho .aeui dentro do projeto";return false;
    }
    const auto remember=[&]() {
      std::error_code ec;std::filesystem::create_directories(root/".astra",ec);
      if(ec || !EditorImportTransaction::writeText(root/".astra/gui-resource",std::string(relative))) {
        error="Interface carregada/salva, mas nao foi possivel registrar o recurso de inicializacao";return false;
      }
      return true;
    };
    if(save) {
      if(!document.validate(error)) return false;
      std::error_code filesystemError;std::filesystem::create_directories(file.parent_path(),filesystemError);
      if(filesystemError) {error="Nao foi possivel criar a pasta da interface";return false;}
      std::ostringstream content;document.write(content);
      if(!publishGuiDocument(relative,content.str(),error))return false;
      error.clear();return remember();
    }
    std::vector<u8> bytes;
    if(!EditorImportTransaction::read(file,bytes,8u*1024u*1024u)) {error="Interface ausente ou maior que 8 MiB";return false;}
    std::istringstream content(std::string(reinterpret_cast<const char*>(bytes.data()),bytes.size()));
    if(!document.read(content,error)) return false;
    return remember();
  });
  {
    const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());
    std::filesystem::path file=root/"UI/main.aeui";
    std::vector<u8> binding;
    if(EditorImportTransaction::read(root/".astra/gui-resource",binding,255)) {
      const std::string relative(binding.begin(),binding.end());std::filesystem::path bound;
      if(EditorImportTransaction::safePath(root,relative,bound) && bound.extension()==".aeui" && gui_.setResource(relative)) file=bound;
      else gui_.setDiagnostic("Recurso inicial invalido; usando UI/main.aeui");
    }
    std::error_code existsError;
    if(std::filesystem::exists(file,existsError)) {
      std::ifstream input(file);std::string error;
      if(!gui_.document().read(input,error)) gui_.setDiagnostic(error);
    }
  }
  gui_.markSaved();
  gui_.history().setCommit([this](const ui::GuiDocument &before,const ui::GuiDocument &after) {
    const auto source=std::string(gui_.resource()),project=files_.rootPath();
    auto old=std::make_shared<const ui::GuiDocument>(before),next=std::make_shared<const ui::GuiDocument>(after);
    return history_.recordResource("Editar documento UI",[this,source,project,old,next](bool forward) {
      if(files_.rootPath()!=project || isPlaying())return false;
      if(gui_.resource()!=source && !gui_.openResource(source)) {
        state_.status="Salve a UI atual antes de desfazer uma alteracao em outro documento";return false;
      }
      gui_.cancelPointers();gui_.immediate().cancelInput();gui_.document().restoreSnapshot(forward?*next:*old);gui_.history().clear();
      gui_.select(gui_.selected());state_.guiSelection.node=gui_.selected();return true;
    });
  });
  gui_.setHistoryActions([this](bool redo){return redo?history_.redo(document_):history_.undo(document_);},
                        [this](bool redo){return redo?history_.canRedo():history_.canUndo();});
  playScene_.configureSceneGui([this](resources::AssetGuid asset,ui::GuiDocument &doc,std::string &error){return loadGuiDocument(asset,doc,error);});
  audioClips_.clear();audioStreams_.clear();
  playScene_.configureAudio([this](resources::AssetGuid asset,std::string &error){return loadAudioClip(asset,error);},audioOutput_);
  playScene_.configureAudioStreams([this](resources::AssetGuid asset,std::string &path,std::string &error){return resolveAudioStream(asset,path,error);});
  projectTags_=std::move(tags);document_.setTags(projectTags_);
  state_.tagPicker=false;state_.editingTagName=false;state_.editingTagSearch=false;state_.tagPage=0;state_.tagQuery.clear();
  environmentBatchRequest_.clear();multiEnvironments_.clear();multiProfiles_.clear();
  environmentMaps_.clear();
  state_.presetPanel=false;state_.presetNaming=false;state_.presetChoices.clear();componentPresets_=EditorComponentPresets{};
  state_.codeRecoveryPending=code_.hasRecovery(files_);
  loadEditorPreferences();
  return true;
}

bool EditorSession::changeProjectTags(const runtime::ObjectTags &next) {
  if(isPlaying() || playMirrorOpen_ || history_.isOpen() || files_.rootPath().empty()) {
    state_.status="Edite o catálogo fora do Play, com um projeto aberto";return false;
  }
  const auto before=projectTags_;if(before==next) return true;
  const auto project=files_.rootPath();
  const auto apply=[this,project](const runtime::ObjectTags &expected,const runtime::ObjectTags &value) {
    if(files_.rootPath()!=project || projectTags_!=expected) {state_.status="Catálogo do histórico não corresponde ao projeto";return false;}
    std::string error;
    for(const auto &name:expected.names()) if(!value.contains(name) && !projectTagUnused(project,document_,name,error)) {
      state_.status=error;return false;
    }
    if(!saveProjectTags(project,expected,value,error)) {state_.status=error;return false;}
    projectTags_=value;document_.setTags(value);state_.tagPage=0;return true;
  };
  if(!apply(before,next)) return false;
  return history_.recordResource("Tags do projeto",[apply,before,next](bool forward) {
    return forward?apply(before,next):apply(next,before);
  });
}

bool EditorSession::assignTag(std::string_view name) {
  if(!document_.tags().contains(name) || history_.isOpen()) return false;
  const auto target=state_.inspectorTarget?state_.inspectorTarget:state_.selection;
  if(!document_.find(target) || target==document_.root()) return false;
  std::vector<EditorEntityId> targets{target};
  if(target==state_.selection && state_.selectionSet.size()>1) targets=state_.selectionSet;
  if(!history_.begin("Tag")) return false;
  for(const auto id:targets) {
    const auto *object=document_.find(id);if(!object || id==document_.root()) continue;
    if(object->tag==name) continue;
    auto values=*object;values.tag=name;
    if(!history_.applyValues(document_,id,values)) {history_.cancel(document_);return false;}
  }
  history_.end();state_.tagPicker=false;state_.status="Tag atribuída";refreshMultiEdit();return true;
}

void EditorSession::loadEditorPreferences() {
  state_.pickerAdvanced=false;
  state_.focusedInspectors.clear();focusedNames_.clear();focusedValidated_=false;state_.focusedActive=0;
  state_.focusedCollapsed=false;state_.focusedMenu=false;state_.undoNewestFirst=true;
  state_.inspectorSurface=EditorInspectorSurface::Inspection;state_.componentOverviewScroll=0;
  state_.hiddenLayers=0;state_.unpickableLayers=0;appearanceChanged_=true;state_.userLayouts.clear();state_.layoutsPanel=false;
  state_.sceneHidden.clear();state_.scenePickOff.clear();sceneHiddenNames_.clear();scenePickOffNames_.clear();
  std::filesystem::path file;
  if(files_.rootPath().empty() ||
     !EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),".astra/editor-preferences.astra",file)) return;
  std::vector<u8> bytes;
  std::error_code ec;
  if(!std::filesystem::exists(file,ec) || !EditorImportTransaction::read(file,bytes,64u*1024u)) return;
  std::istringstream in(std::string(bytes.begin(),bytes.end()));
  std::string magic,key,value;
  if(!(in>>magic) || magic!="ASTRA_EDITOR_PREFERENCES_1") return;
  // Chaves desconhecidas são ignoradas: uma versão futura pode acrescentar.
  for(std::string line;std::getline(in,line);) {
    std::istringstream fields(line);
    if(!(fields>>key)) continue;
    if(key=="object_picker" && fields>>value) state_.pickerAdvanced=value=="advanced";
    else if(key=="undo_order" && fields>>value) state_.undoNewestFirst=value!="oldest";
    else if(key=="inspector_surface" && fields>>value)
      state_.inspectorSurface=value=="components"?EditorInspectorSurface::Components:EditorInspectorSurface::Inspection;
    else if(key=="focused_asset") {
      u32 kind=0;std::string text,name;resources::AssetGuid guid;
      if(fields>>kind>>text>>std::quoted(name) && kind>=1 && kind<=3 && resources::AssetGuid::parse(text,guid) &&
         state_.focusedInspectors.size()<8) {
        EditorScreenState::FocusedInspector focused;focused.kind=static_cast<EditorScreenState::FocusedAsset>(kind);
        focused.asset=guid;focused.name=name;
        state_.focusedInspectors.push_back(focused);focusedNames_.push_back(name);
      }
    }
    else if(key=="layout" || key=="layout_active") {
      // Layout salvo (com nome) ou o arranjo atual quando o projeto fechou.
      EditorLayout layout;int hv=1,iv=1,fc=0,dd=0;
      if(key=="layout" && !(fields>>std::quoted(layout.name))) continue;
      if(!(fields>>layout.hierarchyWidth>>layout.inspectorWidth>>hv>>iv>>fc>>dd)) continue;
      if(!std::isfinite(layout.hierarchyWidth) || !std::isfinite(layout.inspectorWidth) ||
         layout.hierarchyWidth<0 || layout.inspectorWidth<0) continue;
      layout.hierarchyVisible=hv;layout.inspectorVisible=iv;layout.filesCollapsed=fc;layout.diagnosticDock=dd;
      if(key=="layout") {if(state_.userLayouts.size()<12 && !layout.name.empty()) state_.userLayouts.push_back(layout);}
      else {
        state_.hierarchyWidth=layout.hierarchyWidth;state_.inspectorWidth=layout.inspectorWidth;
        state_.hierarchyVisible=layout.hierarchyVisible;state_.inspectorVisible=layout.inspectorVisible;
        state_.filesCollapsed=layout.filesCollapsed;state_.diagnosticDockOpen=layout.diagnosticDock;
      }
    }
    else if(key=="scene_hidden" || key=="scene_pick_off") {
      EditorEntityId id=0;std::string name;
      if(fields>>id>>std::quoted(name)) (key=="scene_hidden"?sceneHiddenNames_:scenePickOffNames_).push_back({id,name});
    }
    else if(key=="scene_layers") {u32 hidden=0,locked=0;if(fields>>hidden>>locked) {state_.hiddenLayers=hidden;state_.unpickableLayers=locked;}}
    else if(key=="focused") {
      // Inspector focado aberto quando o projeto fechou (Unity os restaura).
      EditorEntityId entity=0;u64 component=0;std::string name;
      if(fields>>entity>>component>>std::quoted(name) && state_.focusedInspectors.size()<8) {
        state_.focusedInspectors.push_back({entity,component,EditorScreenState::FocusedAsset::None,{},{}});focusedNames_.push_back(name);
      }
    }
  }
  state_.focusedActive=state_.focusedInspectors.empty()?0:1;
}

void EditorSession::saveEditorPreferences() {
  std::filesystem::path file;
  if(files_.rootPath().empty() ||
     !EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),".astra/editor-preferences.astra",file)) return;
  std::error_code ec;std::filesystem::create_directories(file.parent_path(),ec);
  std::ostringstream out;
  out<<"ASTRA_EDITOR_PREFERENCES_1\nobject_picker "<<(state_.pickerAdvanced?"advanced":"classic")<<'\n';
  out<<"undo_order "<<(state_.undoNewestFirst?"newest":"oldest")<<'\n';
  out<<"inspector_surface "<<(state_.inspectorSurface==EditorInspectorSurface::Components?"components":"inspection")<<'\n';
  out<<"scene_layers "<<state_.hiddenLayers<<' '<<state_.unpickableLayers<<'\n';
  // Visibilidade e seleção por objeto: o id e o nome (a cena reaberta confere).
  for(const auto id:state_.sceneHidden) if(const auto *e=document_.find(id)) out<<"scene_hidden "<<id<<' '<<std::quoted(std::string(e->name))<<'\n';
  for(const auto id:state_.scenePickOff) if(const auto *e=document_.find(id)) out<<"scene_pick_off "<<id<<' '<<std::quoted(std::string(e->name))<<'\n';
  const auto writeLayout=[&](const EditorLayout &l) {
    out<<l.hierarchyWidth<<' '<<l.inspectorWidth<<' '<<l.hierarchyVisible<<' '<<l.inspectorVisible<<' '
       <<l.filesCollapsed<<' '<<l.diagnosticDock<<'\n';
  };
  for(const auto &layout:state_.userLayouts) {out<<"layout "<<std::quoted(layout.name)<<' ';writeLayout(layout);}
  EditorLayout active;
  active.hierarchyWidth=state_.hierarchyWidth;active.inspectorWidth=state_.inspectorWidth;
  active.hierarchyVisible=state_.hierarchyVisible;active.inspectorVisible=state_.inspectorVisible;
  active.filesCollapsed=state_.filesCollapsed;active.diagnosticDock=state_.diagnosticDockOpen;
  out<<"layout_active ";writeLayout(active);
  for(u32 i=0;i<state_.focusedInspectors.size();++i) {
    const auto &focused=state_.focusedInspectors[i];
    if(focused.kind!=EditorScreenState::FocusedAsset::None)
      out<<"focused_asset "<<static_cast<u32>(focused.kind)<<' '<<focused.asset.text()<<' '<<std::quoted(focused.name)<<'\n';
    else out<<"focused "<<focused.entity<<' '<<focused.component<<' '
            <<std::quoted(i<focusedNames_.size()?focusedNames_[i]:std::string())<<'\n';
  }
  const std::string text=out.str();
  if(ec || !EditorImportTransaction::writeText(file,text)) state_.status="Não foi possível guardar a preferência";
}

void EditorSession::setScriptRuntime(scene::ScriptRuntimeApi api) {
  // Android republishes the available scripting API while polling compilation.
  // Audio ownership belongs to the project/output configuration, not that API:
  // reconfiguring here stopped miniaudio and reset every cursor each frame.
  playScene_.setScriptRuntime(api,files_.rootPath());
  playScene_.setScriptResourceAvailability(runtimeResourceResolver());
  playScene_.setPrefabLoader([this](resources::AssetGuid asset,runtime::Prefab &prefab,std::string &error) {
    return preparePrefab(asset,prefab,error);
  });
  playScene_.setSceneSource([this]{return projectScenes();},
    [this](std::string_view request,runtime::SceneGraph &out,std::string &name,std::string &error) {
      return loadPlayScene(request,out,name,error);
    });
}

// Cenas do projeto como os scripts as nomeiam: caminho relativo, sem extensão,
// com '/'. Ordenado para que índices sejam estáveis entre chamadas.
std::vector<std::string> EditorSession::projectScenes() const {
  std::vector<std::string> scenes;
  if(files_.rootPath().empty()) return scenes;
  std::error_code ec;const std::filesystem::path root(EditorImportTransaction::fromUtf8(files_.rootPath()));
  std::filesystem::recursive_directory_iterator it(root,std::filesystem::directory_options::skip_permission_denied,ec),end;
  for(u32 visited=0;it!=end&&!ec&&visited<8192;it.increment(ec),++visited) {
    if(it->is_symlink(ec) || it->path().filename()==".astra") {if(it->is_directory(ec))it.disable_recursion_pending();continue;}
    if(!it->is_regular_file(ec) || it->path().extension()!=".aescene") continue;
    auto relative=it->path().lexically_relative(root);relative.replace_extension();
    scenes.push_back(relative.generic_string());
    if(scenes.size()==4096) break;
  }
  std::sort(scenes.begin(),scenes.end());
  return scenes;
}

// Resolve o pedido do script e prepara o grafo como a abertura de cena faz:
// tags do projeto, reconciliação de recursos e materiais, e extração conferida.
// O documento autoral não é tocado.
bool EditorSession::loadPlayScene(std::string_view request,runtime::SceneGraph &out,std::string &name,std::string &error) {
  const auto scenes=projectScenes();
  std::string resolved;
  for(const auto &scene:scenes) if(scene==request) {resolved=scene;break;}
  if(resolved.empty()) {
    u32 matches=0;
    for(const auto &scene:scenes) {
      const auto slash=scene.rfind('/');
      if(std::string_view(scene).substr(slash==std::string::npos?0:slash+1)==request) {resolved=scene;++matches;}
    }
    if(matches>1) {error="Nome de cena ambíguo; use o caminho: "+std::string(request);return false;}
  }
  if(resolved.empty()) {error="Cena não encontrada no projeto: "+std::string(request);return false;}
  std::filesystem::path file;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),resolved+".aescene",file)) {
    error="Caminho de cena inválido";return false;
  }
  EditorDocument candidate;
  const auto full=files_.resolveFile(resolved+".aescene");
  if(full.empty() || !loadEditorDocument(full.c_str(),sceneFingerprint_,candidate)) {
    error="A cena não pôde ser lida: "+resolved;return false;
  }
  candidate.setTags(projectTags_);
  mapScene_.reconcileAssets(candidate);
  std::vector<renderer::MapDrawState> check;
  if(!mapScene_.extract(candidate,check)) {error="A cena referencia recursos indisponíveis: "+resolved;return false;}
  mapScene_.hydrateMaterials(candidate);
  if(EditorPlayScene::unresolvedEntity(candidate)!=kInvalidEntity) {error="A cena tem componentes de tipo ausente: "+resolved;return false;}
  const auto slash=resolved.rfind('/');
  name=resolved.substr(slash==std::string::npos?0:slash+1);
  out=std::move(static_cast<runtime::SceneGraph&>(candidate));
  return true;
}

// Quem publica sob demanda um recurso trocado com o Play rodando: o mesmo
// caminho para a API de scripts e para o Inspector em Play.
runtime::ComponentResourceResolver EditorSession::runtimeResourceResolver() {
  return [this](resources::AssetGuid guid,resources::AssetType type,std::string_view propertyId,u32 slot,
             scene::ComponentValue &candidate) {
    if(type==resources::AssetType::AnimatorController) {
      if(&candidate.type()!=&scene::Animator::descriptor) return false;
      auto value=scene::animator(candidate);
      if(guid.valid()) {
        if(value.controller!=guid) {value.controller=guid;value.clipOverrides.clear();for(auto &layer:value.layers) layer.mask=0;}
      }
      scene::Animator resolved;std::string diagnostic;
      if(!playScene_.animatorGraphs().resolve(value,resolved,diagnostic)) return false;
      if(!guid.valid()) {resolved.controller={};resolved.clipOverrides.clear();}
      scene::replaceAnimatorData(scene::animator(candidate),resolved);return true;
    }
    // Clipe: vale se a fonte dele está carregada nesta sessão.
    if(type==resources::AssetType::AnimationClip) {
      runtime::AnimationClipView view;
      return !guid.valid() || mapScene_.findClip(guid,view);
    }
    if(type==resources::AssetType::AudioClip) {std::string error;return !guid.valid() || audioClipAvailable(guid,error);}
    if(type==resources::AssetType::UiDocument) {ui::GuiDocument doc;std::string error;return !guid.valid()||loadGuiDocument(guid,doc,error);}
    auto *render=&candidate.type()==&scene::MeshRenderer::descriptor?static_cast<scene::MeshRenderer *>(&candidate):nullptr;
    if(!render||slot>=render->slotCount()) return false;
    const auto ensure=[&](const std::vector<UsedTexture> &required) {
      const auto exact=[&](const UsedTexture &item) {
        const auto &published=mapScene_.textureLibrary();
        return std::any_of(published.begin(),published.end(),[&](const auto &entry) {
          return entry.guid==item.guid&&entry.srgb==item.srgb&&entry.normal==item.normal&&entry.sampler==item.sampler;
        });
      };
      std::vector<UsedTexture> inserted;
      bool complete=true;
      for(const auto &item:required) {
        const auto *record=assets_.find(item.guid);
        if(!record||record->type!=resources::AssetType::Texture) return false;
      }
      for(const auto &item:required) {
        if(exact(item)) continue;
        complete=false;
        if(std::find(runtimeTextures_.begin(),runtimeTextures_.end(),item)==runtimeTextures_.end()) {
          runtimeTextures_.push_back(item);inserted.push_back(item);
        }
      }
      std::string diagnostic;
      const bool available=complete||publishAndAdopt(flattenSources(importedSources_),diagnostic);
      const bool all=available&&std::all_of(required.begin(),required.end(),exact);
      if(!all) for(const auto &item:inserted) std::erase(runtimeTextures_,item);
      if(!all) reportProblem(EditorConsoleSeverity::Warning,"Textura pedida pelo script não publicada: "+diagnostic);
      return all;
    };
    const auto slotTextures=[&] {
      std::vector<UsedTexture> required;
      for(u32 binding=0;binding<scene::MaterialTextureCount;++binding) {
        const auto texture=mapScene_.slotTexture(*render,slot,binding);
        if(texture.valid()&&texture!=scene::MaterialTextureNone)
          required.push_back({texture,EditorMapScene::bindingIsSrgb(binding),binding==1,
                              EditorMapScene::samplerFlags(mapScene_.slotSampling(*render,slot,binding))});
      }
      const auto occlusion=mapScene_.slotOcclusionTexture(*render,slot);
      if(occlusion.valid()&&occlusion!=scene::MaterialTextureNone)
        required.push_back({occlusion,false,false,EditorMapScene::samplerFlags(mapScene_.slotSampling(*render,slot,2))});
      return required;
    };
    u32 samplerBindingMask=0;
    if(type==resources::AssetType::Texture&&
       (propertyId=="sampling.wrap"||propertyId=="sampling.filter")) samplerBindingMask=0xf;
    else if(type==resources::AssetType::Texture&&
            (propertyId.ends_with(".wrap")||propertyId.ends_with(".filter"))) {
      if(propertyId.starts_with("sampling.base_color.")) samplerBindingMask=1u<<0;
      else if(propertyId.starts_with("sampling.normal.")) samplerBindingMask=1u<<1;
      else if(propertyId.starts_with("sampling.metallic_roughness.")) samplerBindingMask=1u<<2;
      else if(propertyId.starts_with("sampling.emissive.")) samplerBindingMask=1u<<3;
    }
    if(samplerBindingMask) {
      std::vector<UsedTexture> required;
      for(u32 binding=0;binding<scene::MaterialTextureCount;++binding) {
        if(!(samplerBindingMask&(1u<<binding))) continue;
        const auto texture=mapScene_.slotTexture(*render,slot,binding);
        if(texture.valid()&&texture!=scene::MaterialTextureNone)
          required.push_back({texture,EditorMapScene::bindingIsSrgb(binding),binding==1,
                              EditorMapScene::samplerFlags(mapScene_.slotSampling(*render,slot,binding))});
      }
      if(samplerBindingMask&(1u<<2)) {
        const auto occlusion=mapScene_.slotOcclusionTexture(*render,slot);
        if(occlusion.valid()&&occlusion!=scene::MaterialTextureNone)
          required.push_back({occlusion,false,false,EditorMapScene::samplerFlags(mapScene_.slotSampling(*render,slot,2))});
      }
      // Samplers das texturas embutidas na fonte são parte do pacote e ainda
      // não podem virar uma variante em Play. Recuse em vez de aceitar sem efeito.
      const auto source=render->slotMesh(slot)?mapScene_.materialForAsset(render->slotMesh(slot)-1):renderer::MaterialOverride{};
      for(u32 binding=0;binding<scene::MaterialTextureCount;++binding)
        if((samplerBindingMask&(1u<<binding))&&!mapScene_.slotTexture(*render,slot,binding).valid()&&
           source.textures[binding]!=renderer::InvalidMapTexture&&source.textures[binding]!=scene::MaterialTextureKeep)
          return false;
      if((samplerBindingMask&(1u<<2))&&!mapScene_.slotOcclusionTexture(*render,slot).valid()&&
         source.occlusionTexture!=renderer::InvalidMapTexture&&source.occlusionTexture!=scene::MaterialTextureKeep)
        return false;
      if(required.empty()) return false;
      return ensure(required);
    }
    if(type==resources::AssetType::Texture && propertyId=="texture.lightmap") {
      if(!guid.valid()) return true;
      return ensure({{guid,false,false,EditorMapScene::samplerFlags(scene::lightmapSampling())}});
    }
    if(type==resources::AssetType::Material&&(guid==scene::MaterialTextureNone||!guid.valid()))
      return ensure(slotTextures());
    if(type==resources::AssetType::Mesh&&(guid==scene::MaterialTextureNone||!guid.valid())) {
      if(auto *target=render->editSlotMesh(slot)) {*target=0;return true;}
      return false;
    }
    if(type==resources::AssetType::Texture&&(guid==scene::MaterialTextureNone||!guid.valid())) {
      if(guid==scene::MaterialTextureNone) return true;
      u32 binding=0;
      if(propertyId=="texture.normal") binding=1;
      else if(propertyId=="texture.metallic_roughness"||propertyId=="texture.occlusion") binding=2;
      else if(propertyId=="texture.emissive") binding=3;
      else if(propertyId!="texture.base_color") return false;
      const auto inherited=propertyId=="texture.occlusion"?mapScene_.slotOcclusionTexture(*render,slot):
                                                          mapScene_.slotTexture(*render,slot,binding);
      if(!inherited.valid()||inherited==scene::MaterialTextureNone) return true;
      return ensure({{inherited,EditorMapScene::bindingIsSrgb(binding),binding==1,
                      EditorMapScene::samplerFlags(mapScene_.slotSampling(*render,slot,binding))}});
    }
    const auto *record=assets_.find(guid);
    if(!record||record->type!=type) return false;
    if(type==resources::AssetType::Material)
      return mapScene_.sharedMaterial(guid)!=nullptr&&ensure(slotTextures());
    if(type==resources::AssetType::Mesh) {
      const u32 resolved=mapScene_.assetSlot(guid);
      auto *target=render->editSlotMesh(slot);
      if(!resolved||!target) return false;
      *target=resolved;return true;
    }
    if(type!=resources::AssetType::Texture) return true;
    u32 binding=0;
    if(propertyId=="texture.normal") binding=1;
    else if(propertyId=="texture.metallic_roughness"||propertyId=="texture.occlusion") binding=2;
    else if(propertyId=="texture.emissive") binding=3;
    else if(propertyId!="texture.base_color") return false;
    const auto sampling=mapScene_.slotSampling(*render,slot,binding);
    const UsedTexture required{guid,EditorMapScene::bindingIsSrgb(binding),binding==1,EditorMapScene::samplerFlags(sampling)};
    return ensure({required});
  };
}

EditorSession::ViewportPointer *EditorSession::findViewportPointer(u32 id) noexcept {
  for (ViewportPointer &pointer : viewportPointers_)
    if (pointer.id == id) return &pointer;
  return nullptr;
}

void EditorSession::buildPickCandidates(const runtime::SceneGraph *source,bool occlusion) {
  const auto &document=source?*source:static_cast<const runtime::SceneGraph &>(document_);
  // Editor hiding/locks must not leak into world-UI depth testing in Play.
  const bool editorVisibility=!source || source==&document_;
  candidates_.clear();
  std::vector<EditorEntityId> subtree;
  document.collectSubtree(document.root(), subtree);
  for (const EditorEntityId id : subtree) {
    if (id == document.root()) continue;
    const EditorEntity *entity = document.find(id);
    if (entity == nullptr) continue;
    // Selection follows the visible capability, including meshes on logical objects.
    const auto *mesh=meshRenderer(*entity);if(!mesh || !mesh->enabled || !mesh->mesh) continue;
    EditorPickCandidate candidate{};
    candidate.id = id;
    // Missing resources have no viewport silhouette. Keep their document row
    // available for repair, but never invent an invisible pick sphere.
    if(!mapScene_.bounds(document, id, candidate.center, candidate.radius)) continue;
    candidate.selectable = entity->visible && entity->active &&
        (!editorVisibility || (!(entity->layer<32 && (state_.hiddenLayers&(1u<<entity->layer))) && !state_.sceneHiddenHas(id))) &&
        (occlusion || (!(entity->layer<32 && (state_.unpickableLayers&(1u<<entity->layer))) && !state_.scenePickOffHas(id)));
    for(auto parent=document.find(entity->parent);parent;parent=document.find(parent->parent))
      candidate.selectable &= parent->visible && parent->active;
    mapScene_.pickGeometry(document,id,candidate);
    const auto base=candidate;
    candidates_.push_back(std::move(candidate));
    // Um candidato por slot adicional, com o MESMO objeto: tocar qualquer
    // primitiva seleciona o objeto inteiro.
    for(u32 slot=1;slot<mesh->slotCount();++slot) {
      const auto meshSlot=mesh->slotMesh(slot);if(!meshSlot) continue;
      auto extra=base;extra.mesh={};extra.resolve={};
      if(!mapScene_.slotBounds(document,id,meshSlot-1,extra.center,extra.radius) ||
         !mapScene_.pickSlotGeometry(document,id,slot,extra)) continue;
      candidates_.push_back(std::move(extra));
    }
  }
}

void EditorSession::handleGizmoPointer(const UiPointerRouting &routing, u32 axis) {
  const EditorEntity *entity = document_.find(state_.selection);
  if (entity == nullptr || axis > 5) return;
  const bool plane = axis >= 3;
  if(plane && state_.tool != EditorGizmoMode::Translate) return;

  if (!gizmoDrag_.active) {
    EditorGizmoSettings settings{};
    settings.screenLengthPixels = 72.0f;
    float world[16];if(!editorWorldMatrix(document_,entity->id,world)) return;
    float parent[16];editorTransformMatrix(EditorTransform{},parent);
    if(entity->parent && !editorWorldMatrix(document_,entity->parent,parent)) return;
    if(!renderer::buildNormalMatrix(parent,parentInverseTranspose_)) return;
    EditorGizmoFrame frame = buildGizmoFrame(view_, world+12, settings);
    if(plane) {
      if(!gizmoPlanePoint(view_,world+12,axis-3,routing.start,planeStart_)) return;
      rotationView_=view_;
    }
    if(state_.tool==EditorGizmoMode::Rotate) {
      if(!gizmoRingAngle(view_,world+12,axis,routing.start,rotationLastAngle_)) return;
      rotationTotalAngle_=0;rotationView_=view_;
      std::copy(world,world+16,rotationWorld_);std::copy(parent,parent+16,rotationParent_);
      frame.axisUsable[axis]=true;
    }
    const EditorGizmoHandle handle = static_cast<EditorGizmoHandle>(axis + 1);
    if (!beginGizmoDrag(frame, handle, entity->transform, settings, gizmoDrag_)) return;
    state_.activeGizmoAxis = handle;
    dragMode_ = state_.tool;
    dragEntity_ = state_.selection;
    std::copy(world,world+16,dragInitialWorld_);
    multiDrag_.clear();
    if(state_.selectionSet.size()>1) for(const auto id:selectedRoots()) {
      if(id==dragEntity_) continue;
      const auto *other=document_.find(id);if(!other) continue;
      MultiDragTarget target;target.id=id;target.initial=other->transform;
      editorTransformMatrix(EditorTransform{},target.parent);
      if(!editorWorldMatrix(document_,id,target.world) || (other->parent && !editorWorldMatrix(document_,other->parent,target.parent))) continue;
      multiDrag_.push_back(target);
    }
    // Uma transação por gesto: o arraste inteiro vira UM passo de desfazer, e
    // não um por frame. É a razão de o histórico ter transações.
    gizmoTransactionOpen_ = history_.begin(state_.tool == EditorGizmoMode::Rotate ? "Rotate" :
                                            state_.tool == EditorGizmoMode::Scale ? "Escala" : "Move");
  }

  EditorTransform moved{};
  bool resolved=false;
  if(plane) {
    float hit[3];
    if(gizmoPlanePoint(rotationView_,gizmoDrag_.frame.origin,axis-3,routing.position,hit)) {
      moved=gizmoDrag_.initial;
      for(u32 i=0;i<3;++i) moved.position[i]+=hit[i]-planeStart_[i];
      resolved=isTransformValid(moved);
    }
  } else if(dragMode_==EditorGizmoMode::Rotate) {
    float angle;
    if(gizmoRingAngle(rotationView_,rotationWorld_+12,axis,routing.position,angle)) {
      rotationTotalAngle_+=std::remainder(angle-rotationLastAngle_,6.28318530718f);
      rotationLastAngle_=angle;
      float rotated[16];std::copy(rotationWorld_,rotationWorld_+16,rotated);
      const u32 u=(axis+1)%3,v=(axis+2)%3;
      const float c=std::cos(rotationTotalAngle_),s=std::sin(rotationTotalAngle_);
      for(u32 column=0;column<3;++column) {
        rotated[column*4+u]=c*rotationWorld_[column*4+u]-s*rotationWorld_[column*4+v];
        rotated[column*4+v]=s*rotationWorld_[column*4+u]+c*rotationWorld_[column*4+v];
      }
      resolved=editorLocalTransformForWorld(rotated,rotationParent_,moved);
      if(!resolved) state_.status="Rotacao exige transformacao sem shear";
    }
  } else resolved=resolveGizmoTransform(gizmoDrag_,dragMode_,routing.totalDelta,moved);
  if (resolved) {
    // O token de fusão é o eixo: comandos consecutivos do mesmo arraste viram
    // um só dentro da transação.
    if(dragMode_==EditorGizmoMode::Translate) {
      float delta[3];for(u32 a=0;a<3;++a) delta[a]=moved.position[a]-gizmoDrag_.initial.position[a];
      for(u32 a=0;a<3;++a) {
        moved.position[a]=gizmoDrag_.initial.position[a];
        for(u32 b=0;b<3;++b) moved.position[a]+=parentInverseTranspose_[a*4+b]*delta[b];
      }
    }
    history_.setTransform(document_, dragEntity_, moved, 0x6000u + axis);
    if(!multiDrag_.empty()) applyMultiDrag(moved,axis);
  }

  if (routing.released) {
    if (gizmoTransactionOpen_) history_.end();
    gizmoTransactionOpen_ = false;
    multiDrag_.clear();
    gizmoDrag_ = EditorGizmoDrag{};
    state_.activeGizmoAxis = EditorGizmoHandle::None;
  }
}

void EditorSession::applyMultiDrag(const EditorTransform &moved,u32 axis) {
  float primary[16];
  if(!editorWorldMatrix(document_,dragEntity_,primary)) return;
  for(const auto &target:multiDrag_) {
    EditorTransform next=target.initial;
    bool resolved=true;
    // Delta nulo (o toque que pega a alça) mantém a pose exata: recompor a
    // local pela matriz de mundo traria erro de arredondamento como edição.
    if(dragMode_==EditorGizmoMode::Translate) {
      float world[16];std::copy(target.world,target.world+16,world);
      float moved2=0;
      for(u32 a=0;a<3;++a) {const float d=primary[12+a]-dragInitialWorld_[12+a];world[12+a]+=d;moved2+=d*d;}
      resolved=moved2>1e-14f?editorLocalTransformForWorld(world,target.parent,next):true;
    } else if(dragMode_==EditorGizmoMode::Rotate && std::abs(rotationTotalAngle_)<1e-7f) {
      next=target.initial;
    } else if(dragMode_==EditorGizmoMode::Rotate) {
      // O mesmo giro do ativo, no eixo do mundo, em torno do pivô de cada um.
      float rotated[16];std::copy(target.world,target.world+16,rotated);
      const u32 u=(axis+1)%3,v=(axis+2)%3;
      const float c=std::cos(rotationTotalAngle_),s=std::sin(rotationTotalAngle_);
      for(u32 column=0;column<3;++column) {
        rotated[column*4+u]=c*target.world[column*4+u]-s*target.world[column*4+v];
        rotated[column*4+v]=s*target.world[column*4+u]+c*target.world[column*4+v];
      }
      resolved=editorLocalTransformForWorld(rotated,target.parent,next);
    } else {
      for(u32 a=0;a<3;++a) {
        const float ratio=gizmoDrag_.initial.scale[a]!=0?moved.scale[a]/gizmoDrag_.initial.scale[a]:1;
        next.scale[a]=target.initial.scale[a]*ratio;
      }
      resolved=isTransformValid(next);
    }
    if(resolved) history_.setTransform(document_,target.id,next,0x6000u+axis);
  }
}

void EditorSession::finishCameraGesture(bool cancel) {
  if(!cameraGestureOpen_) return;
  if(cancel) {
    if(!history_.cancel(document_)) {history_.end();state_.status="Não foi possível cancelar a pilotagem; use desfazer";}
  } else history_.end();
  cameraGestureOpen_=false;cameraGestureEntity_=0;cameraGestureInstance_=0;
}

void EditorSession::pilotCamera(UiPoint delta,float dolly,bool translate) {
  if(!state_.cameraPiloting || isPlaying() || !state_.cameraViewEntity || !isViewportValid(view_)) return;
  const auto *entity=document_.find(state_.cameraViewEntity);
  const auto *component=entity?cameraComponent(*entity):nullptr;
  if(!component) {finishCameraGesture(true);return;}
  if(cameraGestureOpen_ && (cameraGestureEntity_!=entity->id || cameraGestureInstance_!=component->instanceId())) {
    finishCameraGesture(true);return;
  }
  auto pose=resolveSceneCamera(document_,entity->id,true);if(!pose.entity) return;
  if(!translate) {
    pose.yaw=std::remainder(pose.yaw-delta.x*.006f,6.28318530718f);
    pose.pitch=std::clamp(pose.pitch+delta.y*.006f,-1.553343f,1.553343f);
  }
  const auto basis=renderer::buildCameraViewBasis(pose.yaw,pose.pitch,pose.roll);
  const bool orthographic=component->projection==scene::CameraProjection::Orthographic;
  const float scale=2*(orthographic?component->orthographicHalfHeight:std::max(.1f,camera_.distance)*view_.frustum.tangentHalfVertical)/std::max(1.f,view_.rect.height);
  float world[16]{},parent[16];editorTransformMatrix(EditorTransform{},world);editorTransformMatrix(EditorTransform{},parent);
  for(u32 k=0;k<3;++k) {
    world[k]=basis.row0[k];world[4+k]=basis.row1[k];world[8+k]=basis.row2[k];
    world[12+k]=pose.position[k]+basis.row2[k]*(orthographic?0.f:dolly)+
        (translate?scale*(-delta.x*basis.row0[k]+delta.y*basis.row1[k]):0);
  }
  EditorTransform local;
  if(orthographic && translate && delta.x==0 && delta.y==0) local=entity->transform;
  else if((entity->parent && !editorWorldMatrix(document_,entity->parent,parent)) || !editorLocalTransformForWorld(world,parent,local)) {
    state_.status="A hierarquia não permite esta pose sem shear";return;
  }
  std::copy(entity->transform.scale,entity->transform.scale+3,local.scale);
  if(!cameraGestureOpen_) {
    if(!history_.begin("Pilotar câmera")) return;
    cameraGestureOpen_=true;cameraGestureEntity_=entity->id;cameraGestureInstance_=component->instanceId();
  }
  EditorEntity changed=*entity;changed.transform=local;
  if(orthographic && dolly!=0) {
    auto *lens=static_cast<scene::Camera*>(changed.components.editInstance(component->instanceId()));
    lens->orthographicHalfHeight=std::clamp(lens->orthographicHalfHeight*std::exp(-dolly/std::max(.1f,camera_.distance)),.001f,100000.f);
  }
  history_.applyValues(document_,entity->id,changed,0xCA01u);
}

bool EditorSession::handleViewportPointer(const UiPointerEvent &event,
                                          const UiPointerRouting &routing) {
  if (event.phase == UiPointerPhase::Down) {
    viewportPointers_.erase(std::remove_if(viewportPointers_.begin(), viewportPointers_.end(),
        [&](const ViewportPointer &p) { return p.id == event.pointerId; }), viewportPointers_.end());
    viewportPointers_.push_back({event.pointerId, event.position, false});
    if (viewportPointers_.size() > 1)
      for (auto &p : viewportPointers_) p.moved = true;
    pinchDistance_ = viewportPointers_.size() == 2
        ? distanceBetween(viewportPointers_[0].position, viewportPointers_[1].position)
        : 0.0f;
    return true;
  }

  ViewportPointer *pointer = findViewportPointer(event.pointerId);
  if (pointer == nullptr) return false;

  if (event.phase == UiPointerPhase::Move) {
    const UiPoint previous = pointer->position;
    pointer->position = event.position;
    if (routing.dragging) pointer->moved = true;
    // Preserve normal tap selection while inspecting through the camera;
    // gestures must not silently alter either authored pose or editor orbit.
    if(state_.cameraViewEntity) {
      if(state_.cameraPiloting) {
        const UiPoint delta{event.position.x-previous.x,event.position.y-previous.y};
        const float speed=std::max(.1f,camera_.distance);
        if(viewportPointers_.size()>=2) {
          const float distance=distanceBetween(viewportPointers_[0].position,viewportPointers_[1].position);
          const float dolly=pinchDistance_>1 && distance>1?std::clamp(std::log(distance/pinchDistance_),-.5f,.5f)*speed:0;
          pinchDistance_=distance;pilotCamera({delta.x*.5f,delta.y*.5f},dolly,true);
        } else if(routing.dragging) {
          if(state_.navigation==EditorNavigationMode::Zoom) pilotCamera({},-delta.y/std::max(1.f,view_.rect.height)*speed*4,true);
          else pilotCamera(delta,0,state_.navigation==EditorNavigationMode::Pan);
        }
      }
      return true;
    }

    if (viewportPointers_.size() >= 2) {
      // Dois dedos: o deslocamento do ponto médio desloca o alvo, e a variação
      // da distância entre eles afasta ou aproxima. As duas coisas ao mesmo
      // tempo, porque é assim que a mão faz.
      const UiPoint first = viewportPointers_[0].position;
      const UiPoint second = viewportPointers_[1].position;
      const float distance = distanceBetween(first, second);
      if (pinchDistance_ > 1.0f && distance > 1.0f)
        zoomEditorCamera(camera_, pinchDistance_ / distance);
      pinchDistance_ = distance;
      const UiPoint delta{(event.position.x - previous.x) * 0.5f,
                          (event.position.y - previous.y) * 0.5f};
      panEditorCamera(camera_, delta, layout_.viewport, projection_);
    } else if (routing.dragging) {
      const UiPoint delta{event.position.x-previous.x,event.position.y-previous.y};
      switch(state_.navigation) {
        case EditorNavigationMode::Orbit: orbitEditorCamera(camera_,delta);break;
        case EditorNavigationMode::Pan: panEditorCamera(camera_,delta,layout_.viewport,projection_);break;
        case EditorNavigationMode::Zoom:
          zoomEditorCamera(camera_,std::exp(std::clamp(delta.y/std::max(1.0f,layout_.viewport.height)*4.0f,-2.0f,2.0f)));break;
      }
    }
    return true;
  }

  // Levantou. Um toque curto que não virou arraste é uma seleção.
  const bool wasTap = !pointer->moved && event.phase == UiPointerPhase::Up;
  const UiPoint at = event.position;
  viewportPointers_.erase(viewportPointers_.begin() +
                          (pointer - viewportPointers_.data()));
  if(event.phase==UiPointerPhase::Cancel) {
    finishCameraGesture(true);viewportPointers_.clear();pinchDistance_=0;return true;
  }
  if(viewportPointers_.empty()) finishCameraGesture(false);
  pinchDistance_ = viewportPointers_.size() == 2
      ? distanceBetween(viewportPointers_[0].position, viewportPointers_[1].position)
      : 0.0f;

  if (wasTap && viewportPointers_.empty()) {
    if(state_.motorSetupTarget==state_.selection&&state_.motorSetupPolicy==3&&state_.motorBakeSourcesOpen)
      return pickMotorBakeSource(at);
    if(colliderTopology_.active())return pickColliderTopology(at);
    const auto *object=document_.find(state_.selection);
    const auto *focused=object?object->components.findInstance(state_.expandedNative):nullptr;
    if(state_.workspace==EditorWorkspace::Scene && state_.showComponentVisuals &&
       !state_.cameraViewEntity && !state_.multiSelect && state_.selectionSet.size()<=1 && !state_.guiSelection.valid() &&
       !history_.isOpen() && focused && (&focused->type()==&scene::Collider::descriptor || &focused->type()==&scene::PhysicsBody::descriptor) &&
       componentVisualSelectable(document_,state_.selection,state_.hiddenLayers,
           state_.unpickableLayers,state_.sceneHidden,state_.scenePickOff)) {
      buildPickCandidates(nullptr,true);
      const auto hit=pickInspectedCollider(document_,state_.selection,state_.expandedNative,view_,&mapScene_,at,
          state_.hiddenLayers,state_.unpickableLayers,state_.sceneHidden,state_.scenePickOff,candidates_);
      if(hit.entity) {
        if(hit.entity!=state_.selection)setSelection(hit.entity);
        state_.componentSelection=hit.entity;state_.expandedNative=hit.instance;
        state_.expandedComponent.clear();state_.expandedScript=0;state_.nativeMenu=0;state_.scriptMenu=0;
        state_.propertyQuery.clear();state_.propertyPage=0;state_.componentGroup.clear();
        state_.status="Colisor "+std::to_string(hit.instance)+(hit.surface?" selecionado pela superfície":" selecionado pelo contorno");
        return true;
      }
      const auto visible=pickNearest(candidates_,screenPointToRay(view_,at),0,true);
      if(visible.hit) {
        if(componentVisualSelectable(document_,visible.id,state_.hiddenLayers,state_.unpickableLayers,
            state_.sceneHidden,state_.scenePickOff))setSelection(visible.id);
        else state_.status="Objeto visível travado; use a Hierarquia ou oculte-o para acessar a forma atrás";
        return true;
      }
    }
    buildPickCandidates();
    const EditorPickResult hit = pickNearest(candidates_, screenPointToRay(view_, at));
    // Tocar no vazio LIMPA a seleção. É o gesto que todo editor tem, e sem ele
    // não há como desmarcar sem selecionar outra coisa.
    if(state_.multiSelect) {if(hit.hit) toggleSelection(hit.id);}
    else if(hit.hit) setSelection(hit.id); else state_.selection=kInvalidEntity;
  }
  return true;
}

EditorTextEdit EditorSession::pendingTextEdit() const {
  EditorTextEdit edit;edit.version=sceneVersion();
  if(guiActive()) {
    const auto &immediate=immediateGui();
    if(const auto id=immediate.inputId()) {
      edit.purpose=EditorTextPurpose::Gui;edit.elementId=id;edit.text=immediate.inputText();
      edit.entity=state_.guiInspector?state_.guiSelection.owner:0;
      edit.componentInstance=state_.guiInspector?state_.guiSelection.component:0;
      edit.field=gui_.selected();edit.propertyId=std::string(gui_.resource());
    }
    return edit;
  }
  // The Play HUD hides authoring fields. Returning no request also closes the
  // platform IME and rejects late replies instead of editing an invisible draft.
  // Inspecionando o Play, o campo aberto é o do espelho, com a época dele.
  if(isPlaying() && !playMirrorOpen_) {
    if(!playInspecting() || !playMirrorValid_) return edit;
    return const_cast<EditorSession *>(this)->inPlayMirror([this] {return pendingTextEdit();});
  }
  if(state_.curveField && state_.curveText && !state_.numericField) {
    // Nome de preset ou de biblioteca do editor de curvas.
    edit.purpose=EditorTextPurpose::ColorText;edit.field=20+state_.curveText;
    const auto *library=curveLibraries_.currentOrNull();
    if(state_.curveText==2 && library && state_.curvePresetMenu && state_.curvePresetMenu<=library->entries.size())
      edit.text=library->entries[state_.curvePresetMenu-1].name;
    return edit;
  }
  if(state_.gradientField && state_.gradientText && !state_.colorField) {
    // Nome de preset ou de biblioteca do editor de gradiente.
    edit.purpose=EditorTextPurpose::ColorText;edit.field=10+state_.gradientText;
    const auto *library=gradientLibraries_.currentOrNull();
    if(state_.gradientText==2 && library && state_.gradientPresetMenu && state_.gradientPresetMenu<=library->entries.size())
      edit.text=library->entries[state_.gradientPresetMenu-1].name;
    return edit;
  }
  if(state_.colorField && state_.colorText) {
    // Hexadecimal, nome de amostra ou nome de biblioteca da janela de cor.
    edit.purpose=EditorTextPurpose::ColorText;edit.field=state_.colorText;
    if(state_.colorText==1) {
      float srgb[3];pickerRgb(state_.colorHue,state_.colorSaturation,state_.colorValue,srgb);
      edit.text=formatColorHex(srgb,state_.colorAlpha,state_.colorHasAlpha);
    } else if(state_.colorText==2) {
      const auto *library=colorLibraries_.currentOrNull();
      if(library && state_.colorSwatchMenu && state_.colorSwatchMenu<=library->entries.size())
        edit.text=library->entries[state_.colorSwatchMenu-1].name;
    }
    return edit;
  }
  if(state_.presetNaming) {
    edit.purpose=EditorTextPurpose::ComponentPresetName;edit.entity=state_.presetEntity;
    edit.componentInstance=state_.presetInstance;edit.text=state_.presetName;return edit;
  }
  if(state_.viewNaming) {
    edit.purpose=EditorTextPurpose::SceneViewName;edit.field=state_.viewSelected;edit.text=state_.viewName;return edit;
  }
  if(state_.editingCode && !state_.platformCodeView) {
    if(const auto *buffer=code_.active()) {
      edit.purpose=EditorTextPurpose::Code;edit.text=buffer->text;
      edit.bufferId=buffer->id;edit.bufferRevision=buffer->revision;
    }
    return edit;
  }
  if(state_.editingScriptInstance) {
    edit.purpose=EditorTextPurpose::ScriptProperty;edit.entity=state_.editingScriptEntity;
    edit.componentInstance=state_.editingScriptInstance;edit.propertyId=state_.editingScriptProperty;
    edit.propertyType=state_.editingScriptType;
    if(const auto *entity=document_.find(edit.entity))
      if(const auto *script=scene::scriptBehavior(entity->components.findInstance(edit.componentInstance))) {
        if(state_.editingScriptArraySize||state_.editingScriptElement) {
          // Tamanho ou elemento da lista: o campo mostra só aquele valor.
          const auto items=scriptArrayItems(*script,edit.propertyId);
          if(state_.editingScriptArraySize) edit.text=std::to_string(items.size());
          else if(state_.editingScriptElement<=items.size()) edit.text=items[state_.editingScriptElement-1];
        } else for(const auto &p:script->properties) if(p.id==edit.propertyId) edit.text=p.value;
      }
    return edit;
  }
  if(state_.renamingResource && !state_.selectedFile.empty()) {
    edit.purpose=EditorTextPurpose::ResourceName;
    const auto slash=state_.selectedFile.find_last_of('/');
    edit.text=slash==std::string::npos?state_.selectedFile:state_.selectedFile.substr(slash+1);
    return edit;
  }
  if(state_.creatingScript) {edit.purpose=EditorTextPurpose::ScriptName;return edit;}
  if(state_.searchingCode) {edit.purpose=EditorTextPurpose::CodeSearch;edit.text=state_.codeQuery;return edit;}
  if(state_.searchingConsole) {edit.purpose=EditorTextPurpose::ConsoleSearch;edit.text=state_.consoleQuery;return edit;}
  if(state_.searchingTextures) {edit.purpose=EditorTextPurpose::TextureSearch;edit.text=state_.textureQuery;return edit;}
  if(state_.goingToLine) {edit.purpose=EditorTextPurpose::CodeLine;return edit;}
  if(state_.creatingCodeFolder) {edit.purpose=EditorTextPurpose::CodeFolder;return edit;}
  if(state_.editingComponentSearch) {edit.purpose=EditorTextPurpose::ComponentSearch;edit.text=state_.renameText;return edit;}
  if(state_.editingPropertySearch) {edit.purpose=EditorTextPurpose::PropertySearch;edit.entity=state_.selection;edit.componentInstance=state_.expandedNative;edit.text=state_.renameText;return edit;}
  if(state_.editingReferenceSearch) {edit.purpose=EditorTextPurpose::ReferenceSearch;edit.text=state_.renameText;return edit;}
  if(state_.editingGlobalSearch) {edit.purpose=EditorTextPurpose::GlobalSearch;edit.text=state_.renameText;return edit;}
  if(state_.namingLayout) {edit.purpose=EditorTextPurpose::LayoutName;edit.text=state_.renameText;return edit;}
  if(state_.editingTagName) {edit.purpose=EditorTextPurpose::TagName;edit.text=state_.renameText;return edit;}
  if(state_.editingGroupName) {
    edit.purpose=EditorTextPurpose::GroupName;edit.entity=state_.groupEntity;
    edit.field=state_.groupEditSlot;edit.text=state_.renameText;return edit;
  }
  if(state_.editingTagSearch) {edit.purpose=EditorTextPurpose::TagSearch;edit.text=state_.tagQuery;return edit;}
  if(state_.editingAnimatorName) {edit.purpose=EditorTextPurpose::AnimatorName;edit.field=state_.animatorNameField;edit.entity=state_.animatorEntity;edit.text=state_.renameText;return edit;}
  if(state_.editingPhysicsLayerName) {
    edit.purpose=EditorTextPurpose::PhysicsLayerName;edit.field=state_.physicsLayer;
    edit.text=document_.layers().name(state_.physicsLayer);return edit;
  }
  if(state_.editingInputActionName || state_.editingInputContext || state_.inputEditField) {
    const auto &actions=document_.inputActions().actions();
    if(state_.inputActionIndex>=actions.size()) return edit;
    const auto &action=actions[state_.inputActionIndex];
    edit.entity=state_.inputActionIndex;edit.componentInstance=state_.inputBindingIndex;
    if(state_.editingInputActionName) {edit.purpose=EditorTextPurpose::InputActionName;edit.text=action.id;}
    else if(state_.editingInputContext) {edit.purpose=EditorTextPurpose::InputContext;edit.text=action.context;}
    else {
      edit.purpose=EditorTextPurpose::InputNumber;edit.field=state_.inputEditField;
      if(edit.field==widgetId(EditorWidget::InputDuration)) edit.text=std::to_string(action.duration);
      else if(edit.field==widgetId(EditorWidget::InputDeadzone)) edit.text=std::to_string(action.deadzone);
      else if(edit.field==widgetId(EditorWidget::InputSensitivity)) edit.text=std::to_string(action.sensitivity);
      else if(state_.inputBindingIndex<action.bindings.size()) {
        const auto &binding=action.bindings[state_.inputBindingIndex];
        if(edit.field==widgetId(EditorWidget::InputBindingCode)) edit.text=std::to_string(binding.code);
        else if(edit.field==widgetId(EditorWidget::InputBindingNegativeCode)) edit.text=std::to_string(binding.negativeCode);
        else if(edit.field==widgetId(EditorWidget::InputBindingScale)) edit.text=std::to_string(binding.scale);
      }
    }
    return edit;
  }
  if(state_.editingMeshSearch) {edit.purpose=EditorTextPurpose::MeshSearch;edit.text=state_.renameText;return edit;}
  if(state_.numericField) {
    edit.purpose=EditorTextPurpose::Number;edit.entity=state_.numericEntity;
    edit.field=state_.numericField;edit.text=state_.numericText;
    edit.componentInstance=state_.numericInstance;edit.propertyId=state_.numericProperty;
    if(numericPathPointId_ && (edit.field&0xff000000u)==widgetId(EditorWidget::ComponentSlotNumberBase)){edit.elementId=numericPathPointId_;edit.version=numericPathVersion_;}
    if((edit.field&0xff000000u)==widgetId(EditorWidget::ComponentTripleBase)) edit.propertyType="triple";
    else if(state_.numericExpression) edit.propertyType="expression";
  } else {
    if(!state_.editingCreationSearch && !state_.editingHierarchySearch && !state_.renameEntity) return edit;
    edit.entity=state_.renameEntity;edit.text=state_.renameText;
    if(state_.editingCreationSearch) edit.purpose=EditorTextPurpose::CreationSearch;
    else if(state_.editingHierarchySearch) edit.purpose=EditorTextPurpose::HierarchySearch;
    else if(state_.renameEntity) edit.purpose=EditorTextPurpose::Rename;
  }
  return edit;
}

bool EditorSession::updateTextDraft(const EditorTextEdit &edit,std::string_view text,u32 caret) {
  if(playInspecting() && playMirrorValid_ && !playMirrorOpen_ && edit.version.epoch==playMirrorEpoch_)
    return inPlayMirror([&] {return updateTextDraftNow(edit,text,caret);});
  return updateTextDraftNow(edit,text,caret);
}

bool EditorSession::updateTextDraftNow(const EditorTextEdit &edit,std::string_view text,u32 caret) {
  const auto current=pendingTextEdit();
  if(edit.purpose==EditorTextPurpose::None || current.purpose!=edit.purpose ||
     current.entity!=edit.entity || current.componentInstance!=edit.componentInstance ||
     current.field!=edit.field || current.elementId!=edit.elementId || current.propertyType!=edit.propertyType || edit.version.epoch!=sceneEpoch_) return false;
  if(edit.purpose==EditorTextPurpose::Gui) return current.propertyId==edit.propertyId && gui_.immediate().replaceInput(static_cast<u32>(edit.elementId),text);
  // O mesmo teto que a ponte aplica. Um rascunho maior que o campo aceita não é
  // rascunho: é um commit que vai ser recusado no fim, depois de o usuário ter
  // digitado tudo.
  if(text.size()>EditorCodeWorkspace::MaximumFileBytes) return false;
  state_.platformDraft.assign(text);
  state_.platformCaret=static_cast<u32>(std::min<usize>(caret,text.size()));
  // Codigo nao tem "aplicar": o texto digitado E o buffer. Ele entra ja, por
  // `type`, que agrupa a sessao inteira de digitacao em UMA entrada de
  // desfazer -- `replace` guarda uma copia do arquivo por chamada, e uma
  // chamada por tecla faria desfazer voltar caractere a caractere.
  if(edit.purpose==EditorTextPurpose::Code) {
    if(!typeCode(text)) return false;
    followCodeCaret();
    return true;
  }
  // Busca filtra enquanto se digita. É o único efeito permitido antes de
  // confirmar, e ele não toca no documento nem no histórico.
  switch(edit.purpose) {
    case EditorTextPurpose::HierarchySearch:
    case EditorTextPurpose::CreationSearch:
    case EditorTextPurpose::ComponentSearch:
    case EditorTextPurpose::PropertySearch:
    case EditorTextPurpose::MeshSearch:
    case EditorTextPurpose::ReferenceSearch: {
      const auto size=std::min(text.size(),sizeof(state_.renameText)-1);
      std::memcpy(state_.renameText,text.data(),size);
      state_.renameText[size]='\0';
      if(edit.purpose==EditorTextPurpose::PropertySearch) state_.propertyPage=0;
      break;
    }
    default: break;
  }
  return true;
}

// Rolagem acompanha o cursor. Sem isto, digitar no fim de um arquivo longo
// escreveria fora da tela.
// Tocar numa linha do console LEVA ate o lugar dela. Sem isso o console e um
// mural: diz que algo aconteceu e deixa o usuario procurar onde.
void EditorSession::reportScriptSchemaChanges() {
  std::vector<EditorEntityId> ids;document_.collectSubtree(document_.root(),ids);
  for(const auto id:ids) {
    const auto *object=document_.find(id);
    for(usize i=0;i<object->components.size();++i) if(const auto *script=scene::scriptBehavior(object->components.at(i))) {
      auto emit=[&](std::string message) {
        EditorConsoleEntry entry;entry.origin=EditorConsoleOrigin::Editor;entry.severity=EditorConsoleSeverity::Warning;
        entry.message=std::move(message);entry.object=id;entry.component=script->instanceId();
        entry.project=files_.rootPath();entry.buildGeneration=code_.publishedGeneration();console_.add(std::move(entry));
      };
      const auto schema=std::find_if(code_.scriptTypes().begin(),code_.scriptTypes().end(),[&](const auto &type){return type.id==script->scriptType;});
      if(schema==code_.scriptTypes().end()) {emit("Tipo não resolvido: "+script->scriptType+". Instância e valores preservados.");continue;}
      for(const auto &value:script->properties) {
        const auto property=std::find_if(schema->properties.begin(),schema->properties.end(),[&](const auto &field){return field.id==value.id;});
        if(property==schema->properties.end()) emit("Campo removido da fonte: "+value.id+". Valor autorado preservado para recuperação.");
        else if(property->valueType!=value.valueType) emit("Tipo do campo "+value.id+" mudou de "+value.valueType+" para "+property->valueType+". Revise o valor antes de Play.");
      }
    }
  }
}
void EditorSession::jumpToConsoleEntry(u32 index) {
  const auto *entry=console_.at(index);
  if(!entry) return;
  if(!entry->project.empty() && entry->project!=files_.rootPath()) {state_.status="Evento de outro projeto";return;}
  if(entry->playSession && entry->playSession!=playScene_.world().worldId() && entry->object) {
    state_.status="Objeto de uma sessão Play encerrada; evento preservado no console";return;
  }
  if(!entry->file.empty()) {
    if(!code_.open(files_,entry->file)) { state_.status=code_.error(); return; }
    state_.code=&code_;state_.workspace=EditorWorkspace::Code;
    auto *buffer=code_.active();
    if(!buffer) return;
    if(entry->buildGeneration && entry->buildGeneration!=code_.generation()) {
      state_.codeFiles=false;state_.status="Fonte alterada; consulte o trecho original no detalhe do evento";return;
    }
    // A linha do diagnostico e 1-based; a do editor conta do zero.
    const u32 target=entry->line>0?entry->line-1:0;
    const u32 visible=layout_.codeVisibleLines>2?layout_.codeVisibleLines:12;
    buffer->firstLine=target>visible/2?target-visible/2:0;
    usize start=0;u32 line=0;
    while(line<target) {
      const auto end=buffer->text.find(char(10),start);
      if(end==std::string::npos) { start=buffer->text.size(); break; }
      start=end+1;++line;
    }
    code_.locate(entry->line,entry->column);
    state_.platformCaret=buffer->selectionEnd;
    state_.codeFiles=false;
    state_.platformDraft=buffer->text;
    state_.status="Linha "+std::to_string(entry->line)+" de "+entry->file;
    return;
  }
  if(entry->object!=0 && document_.find(static_cast<EditorEntityId>(entry->object))) {
    setSelection(static_cast<EditorEntityId>(entry->object));
    state_.workspace=EditorWorkspace::Scene;
    return;
  }
  state_.status="Esta mensagem não aponta para um lugar";
}

// Colagem e UMA transacao, e nao a continuacao da digitacao.
//
// Uma insercao de varios caracteres num evento so nao veio de teclas: veio da
// area de transferencia ou de uma correcao inteira. Junta-la a sessao de
// digitacao faria um desfazer levar embora as duas coisas de uma vez.
bool EditorSession::typeCode(std::string_view text) {
  auto *buffer=code_.active();
  if(!buffer) { state_.status="Nenhum arquivo de código aberto"; return false; }
  const auto before=buffer->text.size();
  const bool bulk=text.size()>before+2 || before>text.size()+2;
  if(bulk) code_.endTypingRun();
  if(!code_.type(buffer->id,text)) { state_.status=code_.error(); return false; }
  if(bulk) code_.endTypingRun();
  return true;
}

void EditorSession::followCodeCaret() {
  auto *buffer=code_.active();
  if(!buffer) return;
  const auto caret=std::min<usize>(state_.platformCaret,buffer->text.size());
  const auto line=static_cast<u32>(std::count(buffer->text.begin(),buffer->text.begin()+static_cast<long>(caret),10));
  const u32 visible=layout_.codeVisibleLines>2?layout_.codeVisibleLines:12;
  if(line<buffer->firstLine) buffer->firstLine=line;
  else if(line>=buffer->firstLine+visible) buffer->firstLine=line-visible+1;
}

// O toque escolhe a LINHA e o cursor vai para o fim dela.
//
// Coluna exata exigiria medir o texto, que so acontece na construcao das
// instancias; aproximar por largura media poria o cursor no lugar errado em
// cada linha com indentacao, que e toda linha de codigo. Fim de linha e
// previsivel e util, e a coluna exata e o que falta do M05.3.
void EditorSession::placeCodeCaret(ui::UiPoint position) {
  auto *buffer=code_.active();
  if(!buffer || layout_.codeBody.height<=0 || layout_.codeLineHeight<=0) return;
  const float offset=(position.y-layout_.codeBody.y)/layout_.codeLineHeight;
  const u32 row=buffer->firstLine+static_cast<u32>(std::max(0.0f,offset));
  usize start=0;u32 line=0;
  while(line<row) {
    const auto end=buffer->text.find(10,start);
    if(end==std::string::npos) { start=buffer->text.size();break; }
    start=end+1;++line;
  }
  const auto end=buffer->text.find(10,start);
  state_.platformCaret=static_cast<u32>(end==std::string::npos?buffer->text.size():end);
  state_.platformDraft=buffer->text;
  followCodeCaret();
}

void EditorSession::setPlatformImeFraction(float fraction) {
  state_.platformImeFraction=std::isfinite(fraction)?std::clamp(fraction,0.0f,0.9f):0.0f;
}

void EditorSession::setCodeViewState(u64 id,u64 revision,u32 start,u32 end,float x,float y,bool focus) {
  if(!code_.setSelection(id,revision,start,end,std::isfinite(x)?std::max(0.0f,x):0,
                          std::isfinite(y)?std::max(0.0f,y):0)) return;
  state_.platformCaret=end;state_.editingCode=focus;
  if(!focus) {code_.endTypingRun();state_.codeComposing=false;}
}
void EditorSession::codeHistoryAction(bool redo) {
  if(redo) code_.redo();else code_.undo();
}
void EditorSession::recoverCodeDraft(std::string_view text) {
  if(text.size()>EditorCodeWorkspace::MaximumFileBytes) return;
  const std::string folder=".astra/recovery";
  if(!files_.createDirectory(folder)) {state_.status=files_.error();return;}
  std::string path;
  for(u64 i=1;;++i) {path=folder+"/Codigo-recuperado-"+std::to_string(i)+".txt";if(!files_.exists(path)) break;}
  if(!files_.createTextFile(path,text)) {state_.status=files_.error();return;}
  EditorConsoleEntry entry;entry.severity=EditorConsoleSeverity::Warning;
  entry.message="Edição concorrente: rascunho preservado. Toque para abrir a recuperação.";
  entry.file=path;entry.line=1;console_.add(std::move(entry));
  state_.consoleCollapsed=false;state_.status="Rascunho preservado em "+path;
}

namespace {
// Espaco e quebra de linha nas bordas de um NOME nao sao conteudo.
//
// O teclado do sistema acrescenta os dois sem o usuario ver: a sugestao vem com
// espaco no fim, e a tecla de confirmar pode chegar como quebra de linha antes
// do evento de acao. Recusar o nome por causa deles devolve "use letras,
// numeros e sublinhado" para quem digitou exatamente isso -- foi o que
// aconteceu ao criar um script no aparelho.
std::string_view trimmedName(std::string_view text) {
  const auto blank=[](char c) { return c==' '||c==char(9)||c==char(10)||c==char(13); };
  while(!text.empty() && blank(text.front())) text.remove_prefix(1);
  while(!text.empty() && blank(text.back())) text.remove_suffix(1);
  return text;
}
} // namespace

bool EditorSession::completeTextEdit(const EditorTextEdit &edit,std::string_view text,bool accept) {
  if(playInspecting() && playMirrorValid_ && !playMirrorOpen_ && edit.version.epoch==playMirrorEpoch_)
    return inPlayMirror([&] {return completeTextEditNow(edit,text,accept);});
  // O teclado aberto por um campo da janela focada de recurso edita o recurso dela.
  if(modalInFocusedAsset_) {
    const bool done=inFocusedAssetScope([&] {return completeTextEditNow(edit,text,accept);});
    if(!inspectorModalOpen()) modalInFocusedAsset_=false;
    return done;
  }
  if(edit.elementId || edit.purpose==EditorTextPurpose::GroupName) return completeTextEditNow(edit,text,accept);
  if(!state_.inspectorTarget || state_.inspectorTarget==state_.selection)
    return withMultiEdit([&] {return completeTextEditNow(edit,text,accept);});
  return completeTextEditNow(edit,text,accept);
}

bool EditorSession::completeTextEditNow(const EditorTextEdit &edit,std::string_view text,bool accept) {
  const auto current=pendingTextEdit();
  if(edit.purpose==EditorTextPurpose::None || current.purpose!=edit.purpose ||
     current.entity!=edit.entity || current.componentInstance!=edit.componentInstance || current.field!=edit.field || current.elementId!=edit.elementId || edit.version.epoch!=sceneEpoch_) return false;
  if(edit.purpose==EditorTextPurpose::Gui) {
    if(current.propertyId!=edit.propertyId)return false;
    if(accept && !gui_.immediate().replaceInput(static_cast<u32>(edit.elementId),text)) return false;
    gui_.immediate().finishInput(accept);return true;
  }
  const auto close=[&] {
    state_.presetNaming=false;
    state_.viewNaming=false;state_.viewRenaming=false;
    state_.editingPhysicsLayerName=false;state_.editingTagName=false;state_.editingTagSearch=false;
    state_.editingGroupName=false;state_.editingAnimatorName=false;
    state_.editingInputActionName=false;state_.editingInputContext=false;state_.inputEditField=0;
    code_.endTypingRun();
    state_.editingCode=false;state_.creatingScript=false;state_.searchingCode=false;
    state_.goingToLine=false;state_.creatingCodeFolder=false;state_.codeComposing=false;
    state_.searchingConsole=false;
    state_.searchingTextures=false;
    state_.renamingResource=false;
    state_.choosingTemplate=false;
    state_.editingScriptInstance=0;state_.editingScriptEntity=0;state_.editingScriptProperty.clear();state_.editingScriptType.clear();
    state_.editingScriptElement=0;state_.editingScriptArraySize=false;state_.colorText=0;state_.gradientText=0;state_.curveText=0;
    state_.numericField=0;state_.numericInstance=0;state_.numericProperty.clear();numericPathPointId_=0;state_.renameEntity=0;
    state_.editingComponentSearch=false;state_.editingPropertySearch=false;state_.editingMeshSearch=false;state_.editingReferenceSearch=false;
    state_.editingHierarchySearch=false;state_.editingCreationSearch=false;state_.editingGlobalSearch=false;state_.namingLayout=false;
    cancelPointers();
  };
  if(!accept) {close();return true;}
  if(edit.purpose==EditorTextPurpose::AnimatorName) {const bool ok=applyAnimatorName(edit.field,std::string(text));close();return ok;}
  if(edit.purpose==EditorTextPurpose::Number&&animator_widget::owns(edit.field)) {
    NumericExpressionContext context;context.current=state_.numericCurrent;double number=0;std::string reason;
    if(!evaluateNumericExpression(text,context,number,&reason)) {state_.numericError=true;state_.status=reason.empty()?"Valor inválido":reason;return false;}
    const bool ok=applyAnimatorNumber(animator_widget::code(edit.field),number);close();return ok;
  }
  if(edit.elementId && playMirrorOpen_){close();state_.status="Edição de pontos indisponível em Play";return false;}
  if(edit.purpose==EditorTextPurpose::InputActionName || edit.purpose==EditorTextPurpose::InputContext ||
     edit.purpose==EditorTextPurpose::InputNumber) {
    if(edit.version.revision!=document_.revision() || isPlaying() || history_.isOpen() ||
       edit.entity!=state_.inputActionIndex || edit.componentInstance!=state_.inputBindingIndex) {
      close();state_.status="Mapa alterado; reabra o campo";return false;
    }
    auto map=document_.inputActions();
    if(edit.entity>=map.actions().size()) {close();return false;}
    auto action=map.actions()[edit.entity];const auto oldId=action.id;
    const auto value=trimmedName(text);
    bool ok=false;
    if(edit.purpose==EditorTextPurpose::InputActionName) ok=map.rename(oldId,value);
    else if(edit.purpose==EditorTextPurpose::InputContext) {
      action.context=std::string(value);ok=map.replace(oldId,action);
    } else {
      const std::string buffer(value);char *end=nullptr;
      if(edit.field==widgetId(EditorWidget::InputBindingCode) ||
         edit.field==widgetId(EditorWidget::InputBindingNegativeCode)) {
        if(!buffer.empty() && buffer[0]!='-') {
          const auto parsed=std::strtoull(buffer.c_str(),&end,10);
          if(end!=buffer.c_str() && *end=='\0' && parsed<=std::numeric_limits<u32>::max() &&
             edit.componentInstance<action.bindings.size()) {
            auto &binding=action.bindings[edit.componentInstance];
            if(edit.field==widgetId(EditorWidget::InputBindingCode)) binding.code=static_cast<u32>(parsed);
            else binding.negativeCode=static_cast<u32>(parsed);
            ok=map.replace(oldId,action);
          }
        }
      } else {
        const float parsed=std::strtof(buffer.c_str(),&end);
        if(end!=buffer.c_str() && *end=='\0' && std::isfinite(parsed)) {
          if(edit.field==widgetId(EditorWidget::InputDuration)) action.duration=parsed;
          else if(edit.field==widgetId(EditorWidget::InputDeadzone)) action.deadzone=parsed;
          else if(edit.field==widgetId(EditorWidget::InputSensitivity)) action.sensitivity=parsed;
          else if(edit.field==widgetId(EditorWidget::InputBindingScale) && edit.componentInstance<action.bindings.size())
            action.bindings[edit.componentInstance].scale=parsed;
          ok=map.replace(oldId,action);
        }
      }
    }
    if(!ok || !history_.setInputActions(document_,map)) {state_.status="Valor ou nome inválido para a ação";return false;}
    close();return true;
  }
  if(edit.purpose==EditorTextPurpose::GroupName) {
    const auto *object=document_.find(edit.entity);
    if(!object || edit.version.revision!=document_.revision() || history_.isOpen() ||
       !state_.groupPicker || state_.groupEntity!=edit.entity) {
      close();state_.status="Objeto mudou; reabra o grupo";return false;
    }
    const auto name=trimmedName(text);auto value=*object;
    if(edit.field) {
      if(edit.field>value.groups.names().size()) {close();return false;}
      const auto previous=value.groups.names()[edit.field-1];
      if(previous!=name && value.groups.contains(name)) {state_.status="Este objeto já pertence ao grupo";return false;}
      value.groups.remove(previous);
    }
    if(!value.groups.add(name)) {state_.status="Nome inválido, acima de 63 bytes ou limite de 32 grupos";return false;}
    if(value.groups!=object->groups && !history_.applyValues(document_,edit.entity,value)) return false;
    close();state_.status="Grupos atualizados";return true;
  }
  if(edit.purpose==EditorTextPurpose::TagSearch) {
    state_.tagQuery=std::string(text.substr(0,127));state_.tagPage=0;close();return true;
  }
  if(edit.purpose==EditorTextPurpose::TagName) {
    if(edit.version.revision!=document_.revision()) {close();state_.status="Cena mudou; reabra a criação de tag";return false;}
    auto tags=projectTags_;const auto name=trimmedName(text);
    if(!tags.add(name)) {state_.status="Nome inválido, duplicado, acima de 63 bytes ou catálogo cheio";return false;}
    if(!changeProjectTags(tags)) return false;
    state_.tagQuery=std::string(name);state_.tagSelected=std::string(name);state_.tagPage=0;
    state_.status="Tag criada no projeto; escolha-a para atribuir";close();return true;
  }
  if(edit.purpose==EditorTextPurpose::PhysicsLayerName) {
    if(edit.version.revision!=document_.revision() || isPlaying() || history_.isOpen() ||
       edit.field!=state_.physicsLayer) {close();state_.status="Camadas mudaram; reabra o nome";return false;}
    auto layers=document_.layers();const auto name=trimmedName(text);
    bool printable=true;
    for(const unsigned char c:name) if(c<32 || c==127) printable=false;
    if(name.empty() || !printable || name.find('\0')!=std::string_view::npos ||
       !layers.setName(edit.field,name)) {
      state_.status="Nome vazio, duplicado ou acima de 32 caracteres";return false;
    }
    if(!history_.setLayers(document_,layers)) return false;
    close();return true;
  }
  if(edit.purpose==EditorTextPurpose::ComponentPresetName) {
    if(edit.componentInstance!=state_.presetInstance || edit.version.revision!=document_.revision()) {state_.status="Cena alterada; reabra os presets";close();return false;}
    std::string error;const bool saved=state_.presetRenaming?
      componentPresets_.rename(state_.presetSelected,std::string(text),error):
      state_.presetRecipeNaming?saveComponentRecipe(edit.entity,std::string(text),error):
      saveComponentPreset(edit.entity,edit.componentInstance,std::string(text),error);
    state_.status=saved?(state_.presetRecipeNaming?"Receita salva no projeto":"Preset salvo no projeto"):error;
    if(saved) {refreshComponentPresets();close();}return saved;
  }
  if(edit.purpose==EditorTextPurpose::LayoutName) {
    const auto name=trimmedName(text);
    if(name.empty() || name.size()>32 || name.find('"')!=std::string::npos || name.find('\n')!=std::string::npos) {
      state_.status="O nome do layout precisa ter de 1 a 32 caracteres, sem aspas";return false;
    }
    EditorLayout layout;layout.name=std::string(name);
    layout.hierarchyWidth=state_.hierarchyWidth;layout.inspectorWidth=state_.inspectorWidth;
    layout.hierarchyVisible=state_.hierarchyVisible;layout.inspectorVisible=state_.inspectorVisible;
    layout.filesCollapsed=state_.filesCollapsed;layout.diagnosticDock=state_.diagnosticDockOpen;
    // Mesmo nome substitui (a Unity pergunta; aqui o nome na lista já avisa).
    bool replaced=false;
    for(auto &existing:state_.userLayouts) if(existing.name==layout.name) {existing=layout;replaced=true;}
    if(!replaced) {
      if(state_.userLayouts.size()>=12) {state_.status="Limite de 12 layouts salvos";return false;}
      state_.userLayouts.push_back(layout);
    }
    saveEditorPreferences();
    state_.status=replaced?"Layout substituído: "+layout.name:"Layout salvo: "+layout.name;
    close();return true;
  }
  if(edit.purpose==EditorTextPurpose::SceneViewName) {
    const auto name=trimmedName(text);
    if(!runtime::SceneViews::validName(name)) {
      state_.status="O nome da vista não pode ficar vazio nem passar de 48 caracteres";return false;
    }
    const bool renaming=state_.viewRenaming;
    const bool ok=renaming?renameSceneView(state_.viewSelected,name):saveSceneView(name);
    if(ok && !renaming)
      for(u32 i=0;i<document_.views().count();++i)
        if(document_.views().at(i)->name==name) state_.viewSelected=i;
    state_.status=ok?(renaming?"Vista renomeada":"Vista salva com o enquadramento atual"):
        "Já existe uma vista com esse nome";
    if(ok) close();
    return ok;
  }
  if(edit.purpose==EditorTextPurpose::ConsoleSearch) {
    state_.consoleQuery=std::string(text);state_.consoleScroll=0;state_.consoleAnchor=0;close();return true;
  }
  if(edit.purpose==EditorTextPurpose::TextureSearch) {
    state_.textureQuery=std::string(text);state_.textureManagerPage=0;close();return true;
  }
  if(edit.purpose==EditorTextPurpose::CodeLine) {
    u32 line=0;std::istringstream value{std::string(text)};
    if(!(value>>line) || line==0) {state_.status="Informe um número de linha positivo";return false;}
    value>>std::ws;if(!value.eof()) return false;
    code_.locate(line);close();return true;
  }
  if(edit.purpose==EditorTextPurpose::CodeFolder) {
    const auto name=trimmedName(text);
    if(name.empty() || name=="." || name==".." || name.find_first_of("/\\")!=std::string_view::npos) {
      state_.status="Informe apenas o nome da nova pasta";return false;
    }
    std::string parent;
    for(const auto &entry:files_.tree()) if(entry.relativePath==state_.selectedFile) {
      if(entry.directory) parent=entry.relativePath;
      else {const auto slash=entry.relativePath.find_last_of('/');if(slash!=std::string::npos) parent=entry.relativePath.substr(0,slash);}
    }
    const auto path=(parent.empty()?"":parent+"/")+std::string(name);
    if(files_.exists(path)) {state_.status="Já existe um recurso com esse nome";return false;}
    if(!files_.createDirectory(path)) {state_.status=files_.error();return false;}
    files_.rebuildTree();state_.selectedFile=path;state_.status="Pasta criada: "+path;close();return true;
  }
  if(edit.purpose==EditorTextPurpose::ColorText && edit.field>=20) {
    if(!state_.curveField || edit.field!=20u+state_.curveText) return false;
    if(state_.curveText==2) {
      if(!state_.curvePresetMenu || !curveLibraries_.rename(state_.curvePresetMenu-1,trimmedName(text))) {state_.status="Nome inválido";return false;}
      state_.curvePresetMenu=0;
    } else if(!curveLibraries_.createLibrary(trimmedName(text))) {state_.status="Nome de biblioteca inválido ou repetido";return false;}
    state_.curveLibraryMenu=false;saveCurveLibraries();
    state_.curveText=0;close();return true;
  }
  if(edit.purpose==EditorTextPurpose::ColorText && edit.field>=10) {
    if(!state_.gradientField || edit.field!=10u+state_.gradientText) return false;
    if(state_.gradientText==2) {
      if(!state_.gradientPresetMenu || !gradientLibraries_.rename(state_.gradientPresetMenu-1,trimmedName(text))) {state_.status="Nome inválido";return false;}
      state_.gradientPresetMenu=0;
    } else if(!gradientLibraries_.createLibrary(trimmedName(text))) {state_.status="Nome de biblioteca inválido ou repetido";return false;}
    state_.gradientLibraryMenu=false;saveGradientLibraries();
    state_.gradientText=0;close();return true;
  }
  if(edit.purpose==EditorTextPurpose::ColorText) {
    if(!state_.colorField || edit.field!=state_.colorText) return false;
    if(state_.colorText==1) {
      float srgb[3],alpha=state_.colorAlpha;
      if(!parseColorHex(text,srgb,alpha,state_.colorHasAlpha)) {state_.status="Hexadecimal inválido: use RRGGBB";return false;}
      float h=state_.colorHue,s=0,v=0;srgbToHsv(srgb,h,s,v);
      state_.colorHue=h;state_.colorSaturation=s;state_.colorValue=v;state_.colorAlpha=alpha;
    } else if(state_.colorText==2) {
      if(!state_.colorSwatchMenu || !colorLibraries_.rename(state_.colorSwatchMenu-1,trimmedName(text))) {
        state_.status="Nome inválido";return false;
      }
      state_.colorSwatchMenu=0;saveColorLibraries();
    } else {
      if(!colorLibraries_.createLibrary(trimmedName(text))) {state_.status="Nome de biblioteca inválido ou repetido";return false;}
      state_.colorLibraryMenu=false;saveColorLibraries();
    }
    close();return true;
  }
  if(edit.purpose==EditorTextPurpose::ScriptProperty) {
    const auto *entity=document_.find(edit.entity);
    if(!entity || isPlaying() || edit.version.revision!=document_.revision() ||
       current.componentInstance!=edit.componentInstance || current.propertyId!=edit.propertyId || current.propertyType!=edit.propertyType) return false;
    const auto *script=scene::scriptBehavior(entity->components.findInstance(edit.componentInstance));
    if(!script) return false;
    if(state_.editingScriptArraySize || state_.editingScriptElement) {
      const auto *declared=scriptProperty(script->scriptType,edit.propertyId);
      const auto element=declared?scene::scriptArrayElementType(declared->valueType):std::string_view{};
      if(element.empty()) return false;
      auto items=scriptArrayItems(*script,edit.propertyId);
      if(state_.editingScriptArraySize) {
        // Unity: aumentar o Tamanho repete o último elemento; diminuir corta o fim.
        u32 size=0;std::istringstream in{std::string(text)};
        if(!(in>>size) || !(in>>std::ws).eof() || size>scene::kScriptArrayMaximum) {
          state_.status="Tamanho entre 0 e "+std::to_string(scene::kScriptArrayMaximum);return false;
        }
        const auto fill=items.empty()?scene::scriptElementDefault(element):items.back();
        items.resize(size,fill);
        if(state_.scriptArraySelected>size) state_.scriptArraySelected=0;
      } else {
        if(state_.editingScriptElement>items.size() || !scene::validScriptPropertyValue(element,text)) {
          state_.status="Valor incompatível com o tipo do elemento";return false;
        }
        items[state_.editingScriptElement-1]=std::string(text);
      }
      if(!setScriptArray(edit.entity,edit.componentInstance,edit.propertyId,declared->valueType,items)) return false;
      close();return true;
    }
    auto replacement=*script;
    if(!replacement.setProperty(edit.propertyId,edit.propertyType,text)) {state_.status="Valor incompatível com o tipo do campo";return false;}
    auto value=*entity;
    if(!value.components.replaceInstance(edit.componentInstance,replacement) || !history_.applyValues(document_,edit.entity,value)) return false;
    close();return true;
  }
  if(edit.purpose==EditorTextPurpose::Code) {
    // Digitando, o texto ja entrou tecla a tecla e a revisao do instantaneo que
    // abriu o campo ficou para tras de proposito. Confirmar so fecha.
    const auto *buffer=code_.active();
    if(buffer && buffer->id==edit.bufferId && buffer->text==text) { close(); return true; }
    const bool accepted=code_.replace(edit.bufferId,
        buffer && buffer->id==edit.bufferId?buffer->revision:edit.bufferRevision,text);
    if(accepted) close();else state_.status=code_.error();
    return accepted;
  }
  if(edit.purpose==EditorTextPurpose::ResourceName) {
    if(state_.selectedFile.empty()) return false;
    // Nome, nao caminho: barra e ".." aqui virariam mover disfarcado de
    // renomear, e a checagem de destino do sistema de arquivos passaria a ser a
    // unica barreira.
    const auto name=trimmedName(text);
    if(name.empty() || name.find('/')!=std::string_view::npos ||
       name.find(char(92))!=std::string_view::npos || name==".." || name==".") {
      state_.status="Nome invalido para um arquivo";return false;
    }
    const auto slash=state_.selectedFile.find_last_of('/');
    const std::string destination=
        (slash==std::string::npos?std::string():state_.selectedFile.substr(0,slash+1))+std::string(name);
    ResourceChangeReport report;
    if(!moveResource(state_.selectedFile,destination,report)) {
      state_.status=report.diagnostic;return false;
    }
    state_.selectedFile=destination;
    close();return true;
  }
  if(edit.purpose==EditorTextPurpose::ScriptName) {
    std::string directory="Scripts";
    for(const auto &entry:files_.tree()) if(!state_.selectedFile.empty() && entry.relativePath==state_.selectedFile) {
      if(entry.directory) directory=entry.relativePath;
      else {const auto slash=entry.relativePath.find_last_of('/');directory=slash==std::string::npos?"":entry.relativePath.substr(0,slash);}
    }
    if(!code_.createScript(files_,trimmedName(text),state_.scriptTemplate,directory)) {state_.status=code_.error();return false;}
    state_.scriptTemplate=~0u;
    files_.rebuildTree();state_.codeFiles=false;state_.codeFirstTab=static_cast<u32>(code_.buffers().size()-1);
    state_.workspace=EditorWorkspace::Code;close();return true;
  }
  if(edit.purpose==EditorTextPurpose::CodeSearch) {
    state_.codeQuery=text;const auto matches=code_.find(text);
    if(auto *buffer=code_.active();buffer && !matches.empty()) {
      buffer->selectionStart=static_cast<u32>(matches.front().offset);
      buffer->selectionEnd=static_cast<u32>(matches.front().offset+matches.front().length);
      buffer->firstLine=matches.front().line-1;++buffer->viewRevision;
    }
    state_.status=std::to_string(matches.size())+" ocorrências";close();return true;
  }
  if(edit.purpose==EditorTextPurpose::ComponentSearch || edit.purpose==EditorTextPurpose::PropertySearch ||
     edit.purpose==EditorTextPurpose::MeshSearch || edit.purpose==EditorTextPurpose::ReferenceSearch ||
     edit.purpose==EditorTextPurpose::GlobalSearch) {
    if(text.size()>63 || text.find('\0')!=std::string_view::npos) return false;
    if(edit.purpose==EditorTextPurpose::GlobalSearch) {state_.globalQuery=text;state_.globalPage=0;close();return true;}
    if(edit.purpose==EditorTextPurpose::ComponentSearch) {state_.componentQuery=text;state_.componentPage=0;state_.addScroll=0;}
    else if(edit.purpose==EditorTextPurpose::PropertySearch) {state_.propertyQuery=text;state_.propertyPage=0;}
    else if(edit.purpose==EditorTextPurpose::ReferenceSearch) {state_.referenceQuery=text;state_.referencePage=0;}
    else {state_.meshQuery=text;state_.meshPage=0;}
    close();return true;
  }
  if(edit.version.revision!=document_.revision() || history_.isOpen() || isPlaying()) {
    close();state_.status="Edição cancelada: a cena mudou";return false;
  }
  const usize capacity=edit.purpose==EditorTextPurpose::Number?sizeof(state_.numericText):sizeof(state_.renameText);
  if(text.size()>=capacity || text.find('\0')!=std::string_view::npos) {
    state_.status="Texto excede o limite do campo";return false;
  }
  std::string value(text);
  if(edit.purpose==EditorTextPurpose::Number) {
    const auto bakeBase=widgetId(EditorWidget::MotorBakeSettingsNumberBase);
    if(edit.field>=bakeBase&&edit.field<bakeBase+6) {
      NumericExpressionContext context;context.current=state_.numericCurrent;double number=0;std::string reason;
      if(!state_.motorBakeSettingsOpen||state_.motorSetupTarget!=edit.entity||isPlaying()||
         !evaluateNumericExpression(value,context,number,&reason)||number<0||number>400000||
         (edit.field!=bakeBase+3&&edit.field!=bakeBase+5&&number!=std::floor(number))) {
        state_.numericError=true;state_.status=reason.empty()?"Use um inteiro neste ajuste; erro de volume e instante aceitam decimais":reason;return false;
      }
      auto settings=state_.motorBakeDraft;
      switch(edit.field-bakeBase) {
        case 0:settings.maximumParts=static_cast<u32>(number);break;
        case 1:settings.voxelResolution=static_cast<u32>(number);break;
        case 2:settings.maximumVertices=static_cast<u32>(number);break;
        case 3:settings.volumeErrorPercent=static_cast<float>(number);break;
        case 4:settings.timeBudgetSeconds=static_cast<u32>(number);break;
        case 5:settings.animationTime=static_cast<float>(number);break;
      }
      if(!resources::validConvexBakeSettings(settings)) {state_.numericError=true;state_.status="Valor fora do limite mostrado no ajuste";return false;}
      cancelMotorDecomposition();state_.motorBakeDraft=settings;state_.motorBakeBudget=3;
      state_.motorSetupError.clear();state_.motorSetupSummary="Rascunho alterado; gere e revise antes de aplicar.";
      state_.status=state_.motorSetupSummary;
      close();return true;
    }
    const auto topologyBase=widgetId(EditorWidget::ColliderGeometryCoordinateX);
    if(edit.field>=topologyBase&&edit.field<topologyBase+3){
      NumericExpressionContext context;context.current=state_.numericCurrent;double evaluated=0;std::string reason;
      if(!colliderTopology_.active()||edit.entity!=colliderTopology_.object||edit.componentInstance!=colliderTopology_.instance||
         !evaluateNumericExpression(value,context,evaluated,&reason)||std::abs(evaluated)>1000000){state_.numericError=true;state_.status=reason.empty()?"Prévia desatualizada ou coordenada inválida":reason;return false;}
      const bool applied=editColliderTopologyCoordinate(edit.field-topologyBase,static_cast<float>(evaluated),reason);
      close();if(!applied)state_.status=reason;return applied;
    }
    if(edit.propertyType=="triple") {
      state_.numericError=true;
      std::replace(value.begin(),value.end(),';',' ');
      // Commas are decimal separators; channels are separated by spaces or semicolons.
      std::replace(value.begin(),value.end(),',','.');
      float channels[3]{};std::istringstream input(value);input.imbue(std::locale::classic());
      const auto *entity=document_.find(edit.entity);
      if(!entity||current.componentInstance!=edit.componentInstance||current.propertyId!=edit.propertyId) return false;
      auto values=*entity;const auto *component=values.components.findInstance(edit.componentInstance);
      const scene::ComponentTriple *tuple=nullptr;if(component)for(const auto &t:component->type().triples)if(t.id==edit.propertyId)tuple=&t;
      if(!tuple)return false;
      for(u32 axis=0;axis<tuple->dimensions();++axis)if(!(input>>channels[axis]))return false;
      input>>std::ws;if(!input.eof())return false;
      if(!component || scene::setComponentTriple(values.components,component->type().id,edit.propertyId,channels,edit.componentInstance)!=scene::ComponentPropertyStatus::Applied) return false;
      if(!history_.applyValues(document_,edit.entity,values)) return false;
      close();return true;
    }
    // Expressão da Unity (editor/editor_numeric_expression.h): conta,
    // relativo ao valor aberto (+=) e L/R. A vírgula decimal do teclado do
    // Android continua valendo fora de parênteses.
    NumericExpressionContext context;context.current=state_.numericCurrent;
    context.seed=static_cast<u64>(std::chrono::steady_clock::now().time_since_epoch().count());
    double evaluated=0;std::string reason;
    const auto *entity=document_.find(edit.entity);
    state_.numericError=true;
    const bool assetNumber=materialAssetMode() && edit.field>=widgetId(EditorWidget::MaterialNumberBase) &&
        edit.field-widgetId(EditorWidget::MaterialNumberBase)<scene::meshRendererNumbers.size();
    // Perfil de ambiente: número ou trio ("r g b") da linha aberta.
    if(state_.profileInspector.valid() && edit.field>=widgetId(EditorWidget::ProfileRowBase) &&
       edit.field<widgetId(EditorWidget::ProfileRowBase)+state_.profileRows.size()) {
      const auto row=state_.profileRows[edit.field-widgetId(EditorWidget::ProfileRowBase)];
      std::string diagnostic;bool applied=false;
      if(row.id!=state_.numericProperty) return false;
      if(row.kind==EditorScreenState::ProfileRow::Kind::Triple) {
        std::istringstream input{std::string(value)};float rgb[3]{};
        if(!(input>>rgb[0]>>rgb[1]>>rgb[2])) {state_.status="Digite três valores: r g b";return false;}
        applied=editProfileProperty(row,false,rgb,nullptr,diagnostic);
      } else {
        double evaluated=0;std::string reason;
        if(!evaluateNumericExpression(value,context,evaluated,&reason) || std::abs(evaluated)>3.4e38) {
          state_.status="Expressão recusada: "+(reason.empty()?std::string("fora do alcance de um número"):reason);return false;
        }
        applied=editProfileProperty(row,static_cast<float>(evaluated),nullptr,nullptr,diagnostic);
      }
      state_.status=diagnostic;
      if(!applied) return false;
      state_.numericError=false;close();return true;
    }
    if(!entity && !assetNumber) return false;
    if(!evaluateNumericExpression(value,context,evaluated,&reason) || std::abs(evaluated)>3.4e38) {
      state_.status="Expressão recusada: "+(reason.empty()?std::string("fora do alcance de um número"):reason);return false;
    }
    const float number=static_cast<float>(evaluated);
    // Tempo ou valor da chave escolhida no editor de curvas (rascunho).
    if(edit.field==widgetId(EditorWidget::CurveKeyTime) || edit.field==widgetId(EditorWidget::CurveKeyValue)) {
      if(!state_.curveField || !state_.curveSelected || state_.curveSelected>curveEdit_.keys.size()) return false;
      auto &key=curveEdit_.keys[state_.curveSelected-1];
      if(edit.field==widgetId(EditorWidget::CurveKeyValue)) key.value=number;
      else {
        for(u32 i=0;i<curveEdit_.keys.size();++i)
          if(i+1!=state_.curveSelected && curveEdit_.keys[i].time==number) {state_.status="Já existe chave nesse tempo";return false;}
        key.time=number;
      }
      state_.curveSelected=settleCurveSelection(state_.curveSelected);publishCurveDraft();
      state_.numericError=false;close();return true;
    }
    // Campo de material de slot: vale no alcance escolhido no inspetor.
    if(edit.field>=widgetId(EditorWidget::MaterialNumberBase) &&
       edit.field-widgetId(EditorWidget::MaterialNumberBase)<scene::meshRendererNumbers.size()) {
      std::string diagnostic;
      if(!setSlotMaterialValue(edit.entity,state_.materialSlot,state_.materialShared?MaterialScope::Shared:MaterialScope::Instance,
                               edit.field-widgetId(EditorWidget::MaterialNumberBase),number,diagnostic)) {
        state_.status=diagnostic;return false;
      }
      state_.numericError=false;close();return true;
    }
    if(edit.field>=widgetId(EditorWidget::ComponentSlotNumberBase) &&
       edit.field<widgetId(EditorWidget::ComponentSlotNumberBase)+0x01000000u) {
      const auto *component=entity->components.findInstance(edit.componentInstance);
      if(current.componentInstance!=edit.componentInstance || current.propertyId!=edit.propertyId || !component) return false;
      u32 slot=((edit.field-widgetId(EditorWidget::ComponentSlotNumberBase))>>16)&0xffu;
      if(component->type().id=="astra.path"){
        if(!edit.elementId||current.elementId!=edit.elementId)return false;
        const auto&points=static_cast<const scene::Path*>(component)->curve.points;
        const auto found=std::find_if(points.begin(),points.end(),[&](const auto&p){return p.id==edit.elementId;});
        if(found==points.end())return false;
        slot=static_cast<u32>(found-points.begin());
      }
      auto values=*entity;
      if(scene::setComponentSlotProperty(values.components,component->type().id,edit.propertyId,slot,number,
                                         edit.componentInstance)!=scene::ComponentPropertyStatus::Applied ||
         !history_.applyValues(document_,edit.entity,values)) return false;
      close();return true;
    }
    auto values=*entity;
    bool changed=false;
    if(edit.componentInstance) {
      const auto *component=values.components.findInstance(edit.componentInstance);
      if(current.componentInstance!=edit.componentInstance || current.propertyId!=edit.propertyId || !component) return false;
      changed=scene::setComponentProperty(values.components,component->type().id,edit.propertyId,number,edit.componentInstance)==scene::ComponentPropertyStatus::Applied;
    } else changed=setEditorPropertyValue(values,edit.field-widgetId(EditorWidget::TransformFieldBase),number);
    if(!changed || !history_.applyValues(document_,edit.entity,values)) return false;
  } else if(edit.purpose==EditorTextPurpose::Rename) {
    const auto first=value.find_first_not_of(' '),last=value.find_last_not_of(' ');
    if(first==std::string::npos) {state_.status="Nome não pode ficar vazio";return false;}
    for(unsigned char c:value) if(c<32 || c==127) return false;
    const auto *entity=document_.find(edit.entity);if(!entity) {close();return false;}
    auto values=*entity;assignEntityName(values,value.substr(first,last-first+1));
    if(!history_.applyValues(document_,edit.entity,values)) return false;
  } else if(edit.purpose==EditorTextPurpose::HierarchySearch) {
    std::snprintf(state_.hierarchySearch,sizeof(state_.hierarchySearch),"%s",value.c_str());
    state_.hierarchyScroll=0;
  } else {
    std::snprintf(state_.creationSearch,sizeof(state_.creationSearch),"%s",value.c_str());
    state_.creationScroll=0;const auto query=editorSearchKey(value);
    for(u32 i=0;i<editorCreationCatalog.size();++i)
      if(creationAvailable(state_,i) && creationListed(editorCreationCatalog[i],query,state_.creationCategory)) {state_.creationSelection=i;break;}
  }
  close();return true;
}

void EditorSession::refreshScriptInspection(bool force) {
  if(!playInspecting()) {
    scriptInspectionObject_=0;scriptInspectionTime_=-1;
    state_.scriptInspectionEntity=0;state_.scriptFieldIssues.clear();state_.scriptInspectionError.clear();return;
  }
  const auto id=state_.inspectorLocked?state_.inspectorLocked:state_.selection;
  if(!force && (playMirrorBusy_ || (id==scriptInspectionObject_ && lastWallSeconds_-scriptInspectionTime_<.25))) return;
  scriptInspectionObject_=id;scriptInspectionTime_=lastWallSeconds_;
  state_.scriptInspectionEntity=id;
  state_.scriptFieldIssues.clear();state_.scriptInspectionError.clear();
  const auto *object=playScene_.document().find(id);if(!object) return;
  bool hasScript=false;
  for(usize i=0;i<object->components.size();++i) hasScript|=scene::scriptBehavior(object->components.at(i))!=nullptr;
  if(hasScript && !playScene_.inspectFields(id,state_.scriptFieldIssues)) {
    state_.scriptFieldIssues.clear();
    state_.scriptInspectionError="Leitura de campos indisponível · valores não atualizados";
  }
}

void EditorSession::refreshPlayMirror() {
  refreshScriptInspection(true);
  static_cast<runtime::SceneGraph &>(playMirror_)=playScene_.document();
  playMirrorBase_=playScene_.document();
  playHistory_.clear();
  playMirrorEpoch_=nextSceneEpoch();
  playMirrorValid_=true;playMirrorBusy_=false;
}

void EditorSession::enterPlayMirror() {
  std::swap(document_,playMirror_);std::swap(history_,playHistory_);std::swap(sceneEpoch_,playMirrorEpoch_);
  playMirrorWorkspace_=state_.workspace;
  state_.workspace=EditorWorkspace::Scene;state_.document=&document_;
  playMirrorRevision_=document_.revision();playMirrorOpen_=true;
}

void EditorSession::leavePlayMirror() {
  const bool changed=document_.revision()!=playMirrorRevision_;
  playMirrorBusy_=history_.isOpen()||componentDragOpen_||colliderDragOpen_||lensDragOpen_||fieldWidget_!=0||gizmoTransactionOpen_||
                  state_.draggingEntity!=kInvalidEntity||state_.draggingAsset||state_.componentReorder!=0||
                  pendingTextEdit().purpose!=EditorTextPurpose::None;
  const auto requested=state_.workspace;
  std::swap(document_,playMirror_);std::swap(history_,playHistory_);std::swap(sceneEpoch_,playMirrorEpoch_);
  playMirrorOpen_=false;
  // Trocar de área (código, recursos) no meio do Play encerraria a execução
  // sem o usuário pedir; o botão de parar continua sendo o único caminho.
  state_.workspace=playMirrorWorkspace_;
  if(requested!=EditorWorkspace::Scene && requested!=EditorWorkspace::Play)
    state_.status="Pare o Play para trocar de área";
  if(changed) {
    const PlayEditContext context{playScene_,assets_,environmentProfiles_,runtimeResourceResolver()};
    const auto result=applyPlayEdits(playMirrorBase_,playMirror_,context);
    playMirrorBase_=playMirror_;
    // Um objeto criado recebe o id do mundo; o espelho é refeito no próximo
    // toque livre e a seleção já aponta para o objeto real.
    for(const auto &[mirror,world]:result.created) {
      if(state_.selection==mirror) state_.selection=world;
      if(state_.renameEntity==mirror) state_.renameEntity=world;
      if(mirror!=world) playMirrorValid_=false;
    }
    state_.status=result.refused.empty()?"Alterado em Play · volta ao parar":"Não aplicado em Play: "+result.refused;
    state_.playEditRefused=!result.refused.empty();
    state_.playEditNote=result.refused.empty()?state_.status:"Recusado em Play · "+result.refusedField+" · ver console";
    if(!result.refused.empty()) reportProblem(EditorConsoleSeverity::Warning,state_.status);
  }
  state_.document=playInspecting()?&playScene_.document():&document_;
}

bool EditorSession::routeToPlayMirror(const UiPointerEvent &event) {
  if(!playInspecting()) {playEditPointers_.clear();return false;}
  const auto found=std::find(playEditPointers_.begin(),playEditPointers_.end(),event.pointerId);
  if(event.phase==UiPointerPhase::Down) {
    // O viewport, os botões do jogo e a barra superior continuam do Play.
    const auto hit=router_.hitTest(event.position);
    const bool game=hit.target==UiPointerTarget::Viewport || layout_.topBar.contains(event.position) ||
        (hit.target==UiPointerTarget::Widget && (animator_widget::owns(hit.widgetId) || hit.widgetId==widgetId(EditorWidget::JumpCharacter) ||
                                                 hit.widgetId==widgetId(EditorWidget::PlaySecondaryAction) || hit.widgetId==widgetId(EditorWidget::PathFollowRestart) || hit.widgetId==widgetId(EditorWidget::PathFollowStop) || (hit.widgetId>=widgetId(EditorWidget::TweenRestart)&&hit.widgetId<widgetId(EditorWidget::TweenRestart)+0x10000u) ||
                                                 (hit.widgetId>=widgetId(EditorWidget::TimerControlBase)&&hit.widgetId<widgetId(EditorWidget::TimerControlBase)+0x10000u)));
    if(game) return false;
    if(found==playEditPointers_.end()) playEditPointers_.push_back(event.pointerId);
    if(!playMirrorValid_ || !playMirrorBusy_) refreshPlayMirror();
    return true;
  }
  if(found==playEditPointers_.end()) return false;
  if(event.phase==UiPointerPhase::Up || event.phase==UiPointerPhase::Cancel) playEditPointers_.erase(found);
  return true;
}

// Sair do Play (ou fechar a inspeção) descarta o espelho inteiro. O documento
// autoral nunca recebeu nada, então não há o que desfazer nele.
void EditorSession::endPlayInspect() {
  if(playMirrorValid_) {
    inPlayMirror([this] {
      const auto edit=pendingTextEdit();
      if(edit.purpose!=EditorTextPurpose::None) completeTextEditNow(edit,{},false);
      cancelPointers();
    });
  }
  playMirror_=EditorDocument{};playMirrorBase_=runtime::SceneGraph{};playHistory_.clear();
  playMirrorValid_=false;playMirrorBusy_=false;playEditPointers_.clear();
  state_.playEditRefused=false;state_.playEditNote.clear();
  state_.document=&document_;
  if(!document_.exists(state_.selection)) state_.selection=kInvalidEntity;
}

bool EditorSession::inspectorModalOpen() const noexcept {
  return state_.enumPicker || state_.addingComponent || state_.referenceInstance || state_.numericField || state_.colorField ||
         state_.gradientField || state_.curveField || state_.editingScriptInstance || state_.tagPicker;
}

EditorEntityId EditorSession::inspectorScopeFor(const UiPointerEvent &event) {
  auto found=std::find_if(inspectorPointers_.begin(),inspectorPointers_.end(),[&](const auto &p){return p.first==event.pointerId;});
  if(event.phase==UiPointerPhase::Down) {
    EditorEntityId target=0;
    if(inspectorModalOpen()) target=state_.inspectorTarget;
    else if(!state_.focusedCollapsed && !state_.focusedInspectors.empty() && layout_.focusedWindow.contains(event.position)) {
      const u32 active=std::min(std::max(state_.focusedActive,1u),static_cast<u32>(state_.focusedInspectors.size()))-1;
      target=state_.focusedInspectors[active].entity;
    } else if(state_.inspectorLocked && layout_.inspectorPanel.contains(event.position)) target=state_.inspectorLocked;
    if(found!=inspectorPointers_.end()) found->second=target;
    else inspectorPointers_.push_back({event.pointerId,target});
    return target;
  }
  if(found==inspectorPointers_.end()) return 0;
  const auto target=found->second;
  if(event.phase==UiPointerPhase::Up || event.phase==UiPointerPhase::Cancel) inspectorPointers_.erase(found);
  return target;
}

bool EditorSession::handlePointer(const UiPointerEvent &event) {
  if(guiActive() && !state_.workspaceMenu && gui_.pointer(event)) return true;
  if(guiAuthorPointer(event))return true;
  if(isPlaying() && playScene_.active() && !state_.playPaused && !state_.workspaceMenu) {
    const auto hit=router_.hitTest(event.position);
    // Native toolbar/inspection keeps priority. A captured game UI drag retains
    // ownership outside its rectangle through Up/Cancel.
    if((playScene_.gui().captures(event.pointerId,event.device) || playScene_.sceneGui().captures(event.pointerId,event.device) || hit.target!=UiPointerTarget::Widget) && guiPlayPointer(event)) return true;
  }
  // Conta-gotas: todos os toques pertencem à amostragem. Cancelar pela faixa;
  // qualquer outro ponto vira o pedido de amostra ao soltar (o quadro seguinte,
  // sem a janela de cor, é o que se lê).
  if(state_.colorField && state_.colorPicking) {
    const auto routed=router_.route(event);
    if(event.phase==UiPointerPhase::Down) {
      if(routed.target==UiPointerTarget::Widget && routed.widgetId==widgetId(EditorWidget::ColorPickCancel)) {
        state_.colorPicking=state_.colorSampling=false;state_.status="Amostragem cancelada";return true;
      }
      pixelSamplePointer_=event.pointerId+1;
    } else if(event.phase==UiPointerPhase::Up && pixelSamplePointer_==event.pointerId+1 && !state_.colorSampling) {
      pixelSamplePoint_[0]=event.position.x;pixelSamplePoint_[1]=event.position.y;
      pixelSampleRequest_=true;state_.colorSampling=true;pixelSamplePointer_=0;
    }
    return true;
  }
  // Toques na janela focada com aba de recurso (e nos modais que ela abriu)
  // rodam com o contexto de recurso dela.
  {
    auto found=std::find(focusedAssetPointers_.begin(),focusedAssetPointers_.end(),event.pointerId);
    bool assetScope=found!=focusedAssetPointers_.end();
    if(event.phase==UiPointerPhase::Down) {
      assetScope=inspectorModalOpen()?modalInFocusedAsset_:
          (activeFocusedAsset() && layout_.focusedWindow.contains(event.position));
      if(assetScope && found==focusedAssetPointers_.end()) focusedAssetPointers_.push_back(event.pointerId);
      if(!assetScope && found!=focusedAssetPointers_.end()) {focusedAssetPointers_.erase(found);found=focusedAssetPointers_.end();}
    }
    if(event.phase==UiPointerPhase::Up || event.phase==UiPointerPhase::Cancel)
      if(found!=focusedAssetPointers_.end()) focusedAssetPointers_.erase(found);
    if(!inspectorModalOpen()) modalInFocusedAsset_=false;
    if(assetScope) {
      const bool consumed=inFocusedAssetScope([&] {return handlePointerNow(event);});
      modalInFocusedAsset_=inspectorModalOpen();
      return consumed;
    }
  }
  const auto target=inspectorScopeFor(event);
  const auto hit=router_.hitTest(event.position);
  const u32 widget=hit.target==UiPointerTarget::Widget?hit.widgetId:0;
  // O histórico já restaura o valor próprio de cada objeto. Replicar a
  // diferença do primário depois de Undo destrói os valores mistos e o Redo.
  const bool historyReplay=widget==widgetId(EditorWidget::Undo) || widget==widgetId(EditorWidget::Redo) ||
      (widget>=widgetId(EditorWidget::UndoHistoryRowBase) &&
       widget<=widgetId(EditorWidget::UndoHistoryRowBase)+history_.entryCount());
  bool pathSlot=false;
  if(widget>=widgetId(EditorWidget::ComponentSlotNumberBase)&&widget<widgetId(EditorWidget::ComponentSlotNumberBase)+0x01000000u){
    const auto*object=document_.find(target?target:state_.selection);const u32 type=(widget-widgetId(EditorWidget::ComponentSlotNumberBase))&0xffu;
    pathSlot=object&&type<object->components.size()&&object->components.at(type)->type().id=="astra.path";
  }
  const auto run=[&] {
    if(animator_widget::owns(widget)||(widget>=widgetId(EditorWidget::TimerControlBase)&&widget<widgetId(EditorWidget::TimerControlBase)+0x10000u)||(widget>=widgetId(EditorWidget::TweenRestart)&&widget<widgetId(EditorWidget::TweenRestart)+0x10000u)) {
      // Runtime commands bypass the authored-value mirror, but retain the touched
      // Inspector's owner even for objects created only in the running world.
      const auto previous=state_.inspectorTarget;state_.inspectorTarget=target?target:state_.selection;
      const bool consumed=handlePointerNow(event);state_.inspectorTarget=previous;return consumed;
    }
    const bool consumed=inInspectorScope(target,[&] {
      // Sem alvo próprio (travado/focado), o Inspector é o da seleção: a
      // edição do ativo vale para todos os selecionados.
      if(historyReplay || pathSlot || (widget>=widgetId(EditorWidget::PathEditorOpen)&&widget<widgetId(EditorWidget::PathPointSelectBase)+128u) || (target && target!=state_.selection)) return handlePointerNow(event);
      return withMultiEdit([&] {return handlePointerNow(event);});
    });
    // O modal que ficou aberto pertence ao Inspector que o abriu.
    if(!inspectorModalOpen()) state_.inspectorTarget=0;
    else if(target) state_.inspectorTarget=target;
    else if(!state_.inspectorTarget) state_.inspectorTarget=state_.inspectorLocked?state_.inspectorLocked:state_.selection;
    return consumed;
  };
  if(routeToPlayMirror(event)) return inPlayMirror(run);
  return run();
}

// A vista de edição múltipla: quais componentes do ativo existem em todos,
// quantos tipos ficaram de fora, os sem edição múltipla e os campos com
// valores diferentes.
void EditorSession::refreshMultiEdit() {
  auto &view=state_.multi;view={};
  if(state_.selectionSet.size()<2) return;
  const auto *primary=document_.find(state_.selection);if(!primary) return;
  view.count=static_cast<u32>(state_.selectionSet.size());
  std::vector<const EditorEntity *> others;
  for(const auto id:state_.selectionSet) if(id!=state_.selection) if(const auto *other=document_.find(id)) others.push_back(other);
  const auto mixedIf=[&](bool differs,std::string key){if(differs) view.mixed.push_back(std::move(key));};
  bool name=false,visible=false,shadow=false,layer=false,active=false,tag=false;
  bool channel[3][3]{};
  for(const auto *other:others) {
    tag|=other->tag!=primary->tag;
    name|=std::string_view(other->name)!=primary->name;visible|=other->visible!=primary->visible;
    shadow|=other->castShadow!=primary->castShadow;layer|=other->layer!=primary->layer;active|=other->active!=primary->active;
    for(u32 row=0;row<3;++row) for(u32 axis=0;axis<3;++axis)
      channel[row][axis]|=transformChannel(other->transform,row,axis)!=transformChannel(primary->transform,row,axis);
  }
  mixedIf(tag,"tag");mixedIf(name,"name");mixedIf(visible,"visible");mixedIf(shadow,"castShadow");mixedIf(layer,"layer");mixedIf(active,"active");
  for(u32 row=0;row<3;++row) for(u32 axis=0;axis<3;++axis) mixedIf(channel[row][axis],"t."+std::to_string(row)+std::to_string(axis));
  // Componentes: em comum quando todos têm o mesmo tipo na mesma ocorrência.
  std::vector<std::string> kindsOutside;
  for(usize c=0;c<primary->components.size();++c) {
    const auto &value=*primary->components.at(c);
    const auto kind=componentKind(value);const u32 occurrence=componentOccurrence(primary->components,value);
    bool common=true;
    for(const auto *other:others) common&=matchComponent(other->components,kind,occurrence)!=nullptr;
    if(!common) {if(std::find(kindsOutside.begin(),kindsOutside.end(),kind)==kindsOutside.end()) kindsOutside.push_back(kind);continue;}
    view.common.push_back(value.instanceId());
    if(!multiEditable(value)) {view.unsupported.push_back(value.instanceId());continue;}
    const auto compare=[&](std::string_view property) {
      const auto mine=propertyText(value,property);
      for(const auto *other:others) if(propertyText(*matchComponent(other->components,kind,occurrence),property)!=mine) {
        view.mixed.push_back(multiKey(value.instanceId(),property));return;
      }
    };
    for(const auto &p:value.type().numbers) compare(p.id);
    for(const auto &p:value.type().booleans) compare(p.id);
    for(const auto &p:value.type().enums) compare(p.id);
    for(const auto &p:value.type().references) compare(p.id);
    const auto compareSlots=[&](const auto &properties) {
      for(const auto &p:properties) if(p.read) for(u32 slot=0;slot<p.slotCount(value);++slot) {
        for(const auto *other:others) {
          const auto *target=matchComponent(other->components,kind,occurrence);
          if(slot>=p.slotCount(*target)||p.read(*target,slot)!=p.read(value,slot)) {
            view.mixed.push_back(multiKey(value.instanceId(),std::string(p.id)+"@"+std::to_string(slot)));break;
          }
        }
      }
    };
    compareSlots(value.type().slotNumbers);compareSlots(value.type().slotEnums);compareSlots(value.type().resourceBindings);

    if(const auto *script=scene::scriptBehavior(&value)) {
      for(const auto &p:script->properties) compare(p.id);
      bool enabled=false;
      for(const auto *other:others) enabled|=scene::scriptBehavior(matchComponent(other->components,kind,occurrence))->enabled!=script->enabled;
      mixedIf(enabled,multiKey(value.instanceId(),"enabled"));
    }
  }
  for(const auto *other:others) for(usize c=0;c<other->components.size();++c) {
    const auto kind=componentKind(*other->components.at(c));
    if(!matchComponent(primary->components,kind,componentOccurrence(other->components,*other->components.at(c))) &&
       std::find(kindsOutside.begin(),kindsOutside.end(),kind)==kindsOutside.end()) kindsOutside.push_back(kind);
  }
  view.hidden=static_cast<u32>(kindsOutside.size());
}

// Repete nos outros selecionados o que mudou no ativo: campos do objeto,
// canais da transformação (o gizmo tem o próprio caminho, relativo), e cada
// propriedade alterada dos componentes em comum; componente acrescentado ou
// removido no ativo é acrescentado ou removido nos outros. Tudo no mesmo passo
// de Desfazer do gesto.
void EditorSession::replicateEdit(EditorEntityId id,const EditorEntity &before,u32 depth) {
  const auto *afterPtr=document_.find(id);
  if(!afterPtr || state_.selectionSet.size()<2) return;
  const EditorEntity after=*afterPtr;
  const bool gizmo=gizmoTransactionOpen_ || !multiDrag_.empty();
  u32 applied=0;
  const bool open=history_.isOpen();
  const std::string label=std::string(history_.undoLabel());
  for(const auto otherId:std::vector<EditorEntityId>(state_.selectionSet)) {
    if(otherId==id) continue;
    const auto *other=document_.find(otherId);if(!other) continue;
    EditorEntity values=*other;bool changed=false;
    if(std::string_view(before.name)!=after.name) {assignEntityName(values,after.name);changed=true;}
    const auto flag=[&](bool EditorEntity::*field){if(before.*field!=after.*field) {values.*field=after.*field;changed=true;}};
    flag(&EditorEntity::active);flag(&EditorEntity::visible);flag(&EditorEntity::castShadow);flag(&EditorEntity::receiveShadow);flag(&EditorEntity::isStatic);
    if(before.layer!=after.layer) {values.layer=after.layer;changed=true;}
    if(before.tag!=after.tag && values.tag!=after.tag) {values.tag=after.tag;changed=true;}
    if(!gizmo) for(u32 a=0;a<3;++a) {
      if(before.transform.position[a]!=after.transform.position[a]) {values.transform.position[a]=after.transform.position[a];changed=true;}
      if(before.transform.rotationDegrees[a]!=after.transform.rotationDegrees[a]) {values.transform.rotationDegrees[a]=after.transform.rotationDegrees[a];changed=true;}
      if(before.transform.scale[a]!=after.transform.scale[a]) {values.transform.scale[a]=after.transform.scale[a];changed=true;}
    }
    // Componentes alterados e acrescentados.
    for(usize c=0;c<after.components.size();++c) {
      const auto &mine=*after.components.at(c);
      const auto kind=componentKind(mine);const u32 occurrence=componentOccurrence(after.components,mine);
      const auto *old=before.components.findInstance(mine.instanceId());
      const auto *target=matchComponent(values.components,kind,occurrence);
      if(!old) {
        if(target || scene::scriptBehavior(&mine)) continue;
        auto plan=scene::planComponentAddition(values.components,mine.type().id,false,false);
        if(plan.ready) {values.components=std::move(plan.candidate);changed=true;}
        continue;
      }
      if(!target || !multiEditable(mine)) continue;
      auto *edit=values.components.editInstance(target->instanceId());if(!edit) continue;
      for(const auto &p:mine.type().numbers) if(p.write && p.read(*old)!=p.read(mine)) {*p.write(*edit)=p.read(mine);changed=true;}
      for(const auto &p:mine.type().booleans) if(p.write && p.read(*old)!=p.read(mine)) {p.write(*edit,p.read(mine));changed=true;}
      for(const auto &p:mine.type().enums) if(p.write && p.read(*old)!=p.read(mine)) {p.write(*edit,p.read(mine));changed=true;}
      for(const auto &p:mine.type().references) if(p.write && p.read(*old)!=p.read(mine)) {p.write(*edit,p.read(mine));changed=true;}
      const auto copySlots=[&](const auto &properties) {
        for(const auto &p:properties) if(p.read&&p.write) {
          const u32 count=std::min({p.slotCount(*old),p.slotCount(mine),p.slotCount(*edit)});
          for(u32 slot=0;slot<count;++slot) if(p.read(*old,slot)!=p.read(mine,slot))
            changed|=p.write(*edit,slot,p.read(mine,slot));
        }
      };
      copySlots(mine.type().slotNumbers);copySlots(mine.type().slotEnums);copySlots(mine.type().resourceBindings);

      if(const auto *script=scene::scriptBehavior(&mine)) {
        const auto *oldScript=scene::scriptBehavior(old);
        auto replacement=*scene::scriptBehavior(target);
        bool scriptChanged=false;
        if(oldScript->enabled!=script->enabled) {replacement.enabled=script->enabled;scriptChanged=true;}
        for(const auto &p:script->properties) {
          const auto previous=std::find_if(oldScript->properties.begin(),oldScript->properties.end(),[&](const auto &q){return q.id==p.id;});
          if(previous!=oldScript->properties.end() && previous->value==p.value) continue;
          scriptChanged|=replacement.setProperty(p.id,p.valueType,p.value);
        }
        if(scriptChanged && values.components.replaceInstance(target->instanceId(),replacement)) changed=true;
      }
    }
    // Removidos no ativo.
    for(usize c=0;c<before.components.size();++c) {
      const auto &old=*before.components.at(c);
      if(after.components.findInstance(old.instanceId())) continue;
      if(const auto *target=matchComponent(values.components,componentKind(old),componentOccurrence(before.components,old)))
        if(values.components.removeInstance(target->instanceId())) changed=true;
    }
    if(!changed) continue;
    // Durante um gesto contínuo (arraste de campo), os comandos ficam na
    // transação dele e se fundem por objeto; senão, um passo por objeto,
    // juntados ao do ativo logo abaixo.
    if(history_.applyValues(document_,otherId,values,open?0x7A000000u:0u)) ++applied;
  }
  if(!open && applied) {
    const u32 steps=history_.undoDepth()-depth;
    if(steps>1) history_.mergeLast(steps,(label.empty()?std::string("Editar"):label)+" · "+std::to_string(applied+1)+" objetos");
  }
}

void EditorSession::stepTextureProfileField(resources::TextureProfile &profile,u32 field) {
  switch(field) {
    case 0: profile.interpretation=static_cast<u8>((profile.interpretation+1u)%4u);break;
    case 1: {
      const auto &steps=resources::TextureDimensionSteps;
      const auto current=std::find(steps.begin(),steps.end(),profile.maximumDimension);
      profile.maximumDimension=current==steps.end()||current+1==steps.end()?steps.front():*(current+1);
      break;
    }
    case 2: profile.mipmaps=!profile.mipmaps;break;
    case 3: profile.dilateEdges=!profile.dilateEdges;break;
    case 4: profile.anisotropy=!profile.anisotropy;break;
    case 5: profile.invertNormalGreen=!profile.invertNormalGreen;break;
    case 6: profile.preserveAlphaCoverage=!profile.preserveAlphaCoverage;break;
    case 7:
      profile.alphaCoverageCutoff=std::round((profile.alphaCoverageCutoff+.05f)*20.0f)/20.0f;
      if(profile.alphaCoverageCutoff>1.0f) profile.alphaCoverageCutoff=0;
      break;
    case 8: profile.streamingMipmaps=!profile.streamingMipmaps;break;
    default: profile.streamingPriority=nextStreamingPriority(profile.streamingPriority);break;
  }
}

void EditorSession::selectFiles(std::vector<std::string> paths) {
  // Sem repetidos, na ordem do toque: o último é o ativo.
  std::vector<std::string> unique;
  for(auto &path:paths) if(std::find(unique.begin(),unique.end(),path)==unique.end()) unique.push_back(std::move(path));
  state_.pendingResourceDelete.clear();
  state_.selectedFiles=unique;
  const auto single=[&](const std::string &path) {
    // No modo de vários, sobrar um script ou uma cena não troca de área.
    const bool leaves=path.ends_with(".cs")||path.ends_with(".json")||path.ends_with(".md")||path.ends_with(".aescene");
    if(state_.filesMultiSelect && leaves) return false;
    for(const auto &entry:files_.tree()) if(entry.relativePath==path) {openProjectFile(entry);return true;}
    return false;
  };
  if(unique.size()==1 && single(unique.front())) return;
  // Nenhum ou vários: os Inspectors de um recurso fecham e Propriedades mostra
  // o conjunto (ou nada).
  state_.materialInspector={};state_.materialShared=false;state_.texturePicker=false;
  state_.environmentInspector={};state_.profileInspector={};state_.physicsMaterialInspector={};
  if(state_.textureInspector) {closeTextureViewer();state_.textureInspector=false;}
  state_.textureManager=false;
  state_.selectedFile=unique.empty()?std::string():unique.back();
}

void EditorSession::refreshMultiAsset() {
  auto &paths=state_.selectedFiles;auto &view=state_.multiAsset;
  // Coerente com o ativo, que muitas rotinas escrevem direto.
  std::erase_if(paths,[&](const std::string &path){return !files_.exists(path);});
  if(state_.selectedFile.empty()) paths.clear();
  else if(!state_.isFileSelected(state_.selectedFile)) paths={state_.selectedFile};
  if(paths.size()<2) {view={};multiProfiles_.clear();multiEnvironments_.clear();state_.environmentMixed=0;state_.environmentPending=0;multiTextures_.clear();multiAssetGroups_.clear();multiMaterials_.clear();state_.materialMixed.clear();return;}
  multiMaterials_.clear();multiProfiles_.clear();state_.materialMixed.clear();state_.environmentMixed=0;state_.environmentPending=0;
  view.items.clear();view.groups.clear();multiAssetGroups_.clear();
  std::array<u32,std::size(fileKinds)> counts{};
  std::vector<resources::AssetGuid> textures;
  for(const auto &path:paths) {
    const auto *record=assets_.findByPath(path);
    const auto type=record?record->type:resources::AssetType{};
    const u32 kind=record&&type==resources::AssetType::Texture?0u:record&&type==resources::AssetType::Material?1u:
        record&&type==resources::AssetType::Mesh?2u:record&&type==resources::AssetType::EnvironmentMap?3u:
        record&&type==resources::AssetType::EnvironmentProfile?4u:path.ends_with(".cs")?5u:path.ends_with(".aescene")?6u:
        (path.ends_with(".glb")||path.ends_with(".gltf"))?2u:7u;
    ++counts[kind];multiAssetGroups_.push_back(kind);
    EditorScreenState::MultiAssetView::Item item;
    item.name=baseName(path);item.path=path;item.icon=static_cast<u32>(fileKinds[kind].icon);item.active=path==state_.selectedFile;
    const auto slash=path.find_last_of('/');
    item.detail=slash==std::string::npos?std::string("Projeto"):path.substr(0,slash);
    if(kind==0) {
      textures.push_back(record->guid);
      if(const auto *texture=findProjectTexture(record->guid);texture && texture->width)
        item.detail=std::to_string(texture->width)+"×"+std::to_string(texture->height)+"  ·  "+item.detail;
    }
    view.items.push_back(std::move(item));
  }
  u32 kinds=0,only=0;
  for(u32 kind=0;kind<counts.size();++kind) if(counts[kind]) {
    view.groups.push_back({fileKinds[kind].plural,counts[kind],static_cast<u32>(fileKinds[kind].icon)});
    ++kinds;only=kind;
  }
  const u32 count=static_cast<u32>(paths.size());
  view.fields={};view.mixed=0;view.pending=0;
  if(kinds==1 && (only==3 || only==4)) {multiTextures_.clear();refreshMultiEnvironmentAssets(only==4);return;}
  multiEnvironments_.clear();
  if(kinds>1) {
    view.kind=EditorScreenState::MultiAssetView::Kind::Mixed;
    view.title=std::to_string(count)+" recursos";
    view.note="Tipos diferentes: só o comum aparece. Toque num tipo para estreitar.";
    if(state_.materialInspector.valid()) {state_.materialInspector={};state_.materialShared=false;state_.texturePicker=false;}
    multiTextures_.clear();return;
  }
  std::string plural=fileKinds[only].plural;
  plural[0]=static_cast<char>(std::tolower(static_cast<unsigned char>(plural[0])));
  view.title=std::to_string(count)+" "+(only==7?std::string("arquivos"):plural);
  if(only==1) {
    // Materiais: o Inspector do ativo edita todos; o que difere mostra "—".
    view.kind=EditorScreenState::MultiAssetView::Kind::Materials;view.note.clear();multiTextures_.clear();
    resources::AssetGuid active{};
    for(const auto &path:paths) if(const auto *record=assets_.findByPath(path)) {
      multiMaterials_.push_back(record->guid);
      if(path==state_.selectedFile) active=record->guid;
    }
    if(multiMaterials_.empty() || std::any_of(multiMaterials_.begin(),multiMaterials_.end(),
        [&](const auto &guid){return !findMaterialAsset(guid);})) {
      view.kind=EditorScreenState::MultiAssetView::Kind::Other;
      view.note="Um material da seleção está ilegível. Abra-o sozinho para verificar.";
      state_.materialInspector={};multiMaterials_.clear();return;
    }
    if(!active.valid()) active=multiMaterials_.back();
    if(!(state_.materialInspector==active)) openMaterialInspector(active);
    if(const auto *reference=findMaterialAsset(active))
      for(const auto &guid:multiMaterials_) if(!(guid==active)) if(const auto *other=findMaterialAsset(guid))
        materialDifferences(*reference,*other,state_.materialMixed);
    return;
  }
  if(state_.materialInspector.valid()) {state_.materialInspector={};state_.materialShared=false;state_.texturePicker=false;}
  if(only!=0) {
    // Unity: "Multi-object editing not supported" para o tipo sem editor comum.
    view.kind=EditorScreenState::MultiAssetView::Kind::Other;
    view.note="Sem edição em conjunto para "+plural+". Toque num item para abri-lo sozinho.";
    multiTextures_.clear();return;
  }
  view.kind=EditorScreenState::MultiAssetView::Kind::Textures;
  view.note.clear();
  // Rascunhos: quem não mudou acompanha o aplicado (Aplicar, Desfazer, outro Inspector).
  std::vector<MultiTextureDraft> drafts;
  for(const auto &guid:textures) {
    const auto saved=textureProfileFor(guid);
    MultiTextureDraft entry{guid,saved,saved};
    for(const auto &previous:multiTextures_) if(previous.guid==guid) {
      entry.draft=resources::sameTextureProfile(previous.draft,previous.saved)?saved:previous.draft;
    }
    drafts.push_back(entry);
  }
  multiTextures_=std::move(drafts);
  for(u32 field=0;field<TextureProfileFields;++field) {
    const auto first=textureProfileFieldText(multiTextures_.front().draft,field);
    bool same=true;
    for(const auto &texture:multiTextures_) if(textureProfileFieldText(texture.draft,field)!=first) {same=false;break;}
    if(same) view.fields[field]=first;
    else {view.fields[field]=first.substr(0,first.find(": "))+": \xE2\x80\x94";view.mixed|=static_cast<u16>(1u<<field);}
  }
  for(const auto &texture:multiTextures_) if(!resources::sameTextureProfile(texture.draft,texture.saved)) ++view.pending;
}

bool EditorSession::applyMultiTextureProfiles() {
  std::vector<MultiTextureDraft> changes;
  for(const auto &texture:multiTextures_) if(!resources::sameTextureProfile(texture.draft,texture.saved)) changes.push_back(texture);
  if(changes.empty()) return true;
  // Todas ou nenhuma: uma recusa devolve as já publicadas ao perfil anterior.
  const auto publish=[this](const std::vector<MultiTextureDraft> &list,bool forward,std::string &diagnostic) {
    for(usize i=0;i<list.size();++i) {
      if(setTextureProfile(list[i].guid,forward?list[i].draft:list[i].saved,diagnostic,false)) continue;
      const auto *record=assets_.find(list[i].guid);
      if(record) diagnostic=baseName(record->path)+": "+diagnostic;
      for(usize k=i;k-->0;) {std::string ignored;setTextureProfile(list[k].guid,forward?list[k].saved:list[k].draft,ignored,false);}
      return false;
    }
    return true;
  };
  std::string diagnostic;
  if(!publish(changes,true,diagnostic)) {state_.status="Nenhum perfil aplicado. "+diagnostic;return false;}
  const auto project=files_.rootPath();
  const u32 count=static_cast<u32>(changes.size());
  history_.recordResource("Perfil de "+std::to_string(count)+(count==1?" textura":" texturas"),
                          [this,changes,project,publish](bool forward) {
    if(files_.rootPath()!=project) {state_.status="Recurso do histórico indisponível neste projeto.";return false;}
    for(const auto &texture:changes)
      if(!assets_.find(texture.guid) || !resources::sameTextureProfile(textureProfileFor(texture.guid),forward?texture.saved:texture.draft)) {
        state_.status="Perfil de textura mudou; histórico preservado sem sobrescrever.";return false;
      }
    std::string error;
    if(!publish(changes,forward,error)) {state_.status=error;return false;}
    state_.status=forward?"Perfis de textura refeitos":"Perfis de textura desfeitos";
    return true;
  });
  state_.status="Perfil aplicado a "+std::to_string(count)+(count==1?" textura":" texturas")+" e republicado";
  return true;
}

bool EditorSession::applySetValue(u32 row) {
  const auto &menu=state_.setValueMenu;
  if(row>=menu.rows.size()) return false;
  // Campo do perfil comum a várias texturas: copia para o rascunho de todas.
  if(menu.key.starts_with("asset.tex.")) {
    const u32 field=static_cast<u32>(std::stoul(menu.key.substr(10)));const u32 index=menu.rows[row].first;
    if(index>=multiTextures_.size() || field>=TextureProfileFields) return false;
    const auto source=multiTextures_[index].draft;
    for(auto &texture:multiTextures_) copyTextureProfileField(texture.draft,source,field);
    state_.status="Valor copiado para todas; Aplicar publica";
    return true;
  }
  if(menu.key.starts_with("asset.mat.")) {
    const u32 index=menu.rows[row].first;
    if(index>=multiMaterials_.size()) return false;
    const auto *source=findMaterialAsset(multiMaterials_[index]);
    const auto *active=findMaterialAsset(state_.materialInspector);
    if(!source || !active) return false;
    const auto field=std::string_view(menu.key).substr(10);auto candidate=*active;
    copyMaterialChange(*source,*source,candidate,field);++candidate.revision;
    std::string diagnostic;
    if(!commitSharedMaterial(candidate,diagnostic,true,field)) {state_.status=diagnostic;return false;}
    if(!ensureTexturesPublished(diagnostic)) state_.status="Materiais atualizados; publicação pendente: "+diagnostic;
    else state_.status="Valor copiado para os materiais selecionados";
    return true;
  }
  if(menu.key.starts_with("asset.hdri.")) return applyEnvironmentValue(row);
  if(menu.key.starts_with("asset.profile.")) return applyProfileValue(row);
  const auto *source=document_.find(menu.rows[row].first);if(!source) return false;
  const u32 depth=history_.undoDepth();u32 applied=0;
  for(const auto id:std::vector<EditorEntityId>(state_.selectionSet)) {
    const auto *object=document_.find(id);if(!object || id==source->id) continue;
    EditorEntity values=*object;bool changed=false;
    if(menu.key.starts_with("t.") && menu.key.size()==4) {
      const u32 r=menu.key[2]-'0',a=menu.key[3]-'0';
      float *mine=r==0?values.transform.position:r==1?values.transform.rotationDegrees:values.transform.scale;
      const float *theirs=r==0?source->transform.position:r==1?source->transform.rotationDegrees:source->transform.scale;
      if(mine[a]!=theirs[a]) {mine[a]=theirs[a];changed=true;}
    } else {
      const auto slash=menu.key.find('/');
      const auto instance=std::stoull(menu.key.substr(0,slash));const auto property=menu.key.substr(slash+1);
      const auto *primary=document_.find(state_.selection);
      const auto *reference=primary?primary->components.findInstance(instance):nullptr;if(!reference) return false;
      const auto kind=componentKind(*reference);const u32 occurrence=componentOccurrence(primary->components,*reference);
      const auto *from=matchComponent(source->components,kind,occurrence);
      const auto *to=matchComponent(values.components,kind,occurrence);
      auto *edit=to?values.components.editInstance(to->instanceId()):nullptr;
      if(!from || !edit) continue;
      for(const auto &p:from->type().numbers) if(p.id==property && p.write) {*p.write(*edit)=p.read(*from);changed=true;}
      for(const auto &p:from->type().booleans) if(p.id==property && p.write) {p.write(*edit,p.read(*from));changed=true;}
      for(const auto &p:from->type().enums) if(p.id==property && p.write) {p.write(*edit,p.read(*from));changed=true;}
      for(const auto &p:from->type().references) if(p.id==property && p.write) {p.write(*edit,p.read(*from));changed=true;}
    }
    if(changed && history_.applyValues(document_,id,values)) ++applied;
  }
  const u32 steps=history_.undoDepth()-depth;
  if(steps>1) history_.mergeLast(steps,"Definir valor · "+std::to_string(applied)+" objetos");
  state_.status=applied?"Valor de "+std::string(source->name)+" aplicado aos selecionados":"Nada mudou";
  return applied>0;
}

void EditorSession::toggleSelection(EditorEntityId entity) {
  if(!document_.exists(entity) || entity==document_.root()) return;
  auto &set=state_.selectionSet;
  const auto found=std::find(set.begin(),set.end(),entity);
  if(found!=set.end()) {
    set.erase(found);
    // O ativo passa a ser o último que ficou (Unity: activeObject).
    if(state_.selection==entity) state_.selection=set.empty()?kInvalidEntity:set.back();
  } else {
    if(set.empty() && document_.exists(state_.selection)) set.push_back(state_.selection);
    set.push_back(entity);state_.selection=entity;
  }
  state_.propertyPage=0;
  state_.status=set.size()>1?std::to_string(set.size())+" objetos selecionados":set.empty()?"Nada selecionado":"1 objeto selecionado";
}

void EditorSession::selectEntities(std::vector<EditorEntityId> entities) {
  entities.erase(std::remove_if(entities.begin(),entities.end(),[&](EditorEntityId id){return !document_.exists(id) || id==document_.root();}),
                 entities.end());
  if(entities.empty()) {state_.selection=kInvalidEntity;state_.selectionSet.clear();return;}
  const auto active=std::find(entities.begin(),entities.end(),state_.selection)!=entities.end()?state_.selection:entities.back();
  state_.selection=active;state_.selectionSet=std::move(entities);state_.propertyPage=0;
  if(state_.selectionSet.size()>1) state_.multiSelect=true;
  state_.status=std::to_string(state_.selectionSet.size())+(state_.selectionSet.size()==1?" objeto selecionado":" objetos selecionados");
}

std::vector<EditorEntityId> EditorSession::selectedRoots() const {
  std::vector<EditorEntityId> roots;
  for(const auto id:state_.selectionSet) {
    bool nested=false;
    for(auto parent=document_.find(id)?document_.find(id)->parent:kInvalidEntity;parent && !nested;parent=document_.find(parent)?document_.find(parent)->parent:kInvalidEntity)
      nested=state_.isSelected(parent);
    if(!nested && document_.exists(id)) roots.push_back(id);
  }
  return roots;
}

const EditorScreenState::FocusedInspector *EditorSession::activeFocusedAsset() const {
  if(state_.focusedCollapsed || state_.focusedInspectors.empty()) return nullptr;
  const u32 active=std::min(std::max(state_.focusedActive,1u),static_cast<u32>(state_.focusedInspectors.size()))-1;
  const auto &focused=state_.focusedInspectors[active];
  return focused.kind!=EditorScreenState::FocusedAsset::None?&focused:nullptr;
}

bool EditorSession::openFocusedAsset(EditorScreenState::FocusedAsset kind,const resources::AssetGuid &guid) {
  using Kind=EditorScreenState::FocusedAsset;
  const auto *record=assets_.find(guid);
  const bool ok=record && ((kind==Kind::Material && findMaterialAsset(guid)) ||
      (kind==Kind::EnvironmentMap && record->type==resources::AssetType::EnvironmentMap) ||
      (kind==Kind::EnvironmentProfile && findEnvironmentProfile(guid)));
  if(!ok) return false;
  for(u32 i=0;i<state_.focusedInspectors.size();++i)
    if(state_.focusedInspectors[i].kind==kind && state_.focusedInspectors[i].asset==guid) {
      state_.focusedActive=i+1;state_.focusedCollapsed=false;return true;
    }
  if(state_.focusedInspectors.size()>=8) {state_.focusedInspectors.erase(state_.focusedInspectors.begin());if(!focusedNames_.empty()) focusedNames_.erase(focusedNames_.begin());}
  EditorScreenState::FocusedInspector focused;focused.kind=kind;focused.asset=guid;
  focused.name=record->path.substr(record->path.rfind('/')+1);
  state_.focusedInspectors.push_back(focused);focusedNames_.push_back(focused.name);
  state_.focusedActive=static_cast<u32>(state_.focusedInspectors.size());
  state_.focusedCollapsed=false;state_.focusedMenu=false;focusedValidated_=true;
  state_.status="Propriedades de "+focused.name;
  saveEditorPreferences();
  return true;
}

// A aba de recurso ativa é atualizada no contexto dela: o guid do tipo certo
// (os outros vazios) e a mesma rotina do Inspector principal. Recurso apagado,
// ou "<" tocado dentro da janela, fecha a aba.
void EditorSession::refreshFocusedAsset() {
  const auto *focused=activeFocusedAsset();
  if(!focused) return;
  using Kind=EditorScreenState::FocusedAsset;
  const auto kind=focused->kind;const auto guid=focused->asset;
  bool closed=false;
  inFocusedAssetScope([&] {
    auto &s=state_;
    const auto &mine=kind==Kind::Material?s.materialInspector:kind==Kind::EnvironmentMap?s.environmentInspector:s.profileInspector;
    if(!(mine==guid) && focusedAssetLoaded_==guid) {closed=true;return;}
    const bool fresh=!(mine==guid);
    if(fresh) {
      focusedAssetLoaded_=guid;
      s.materialInspector={};s.environmentInspector={};s.profileInspector={};s.texturePicker=false;s.propertyPage=0;
      if(kind==Kind::Material) {s.materialInspector=guid;s.materialShared=true;}
      else if(kind==Kind::EnvironmentMap) {
        resources::EnvironmentMapImportSettings saved;const auto *record=assets_.find(guid);
        if(record && resources::readEnvironmentMapImportSettings(record->importerParameters,saved)) s.environmentDraft=s.environmentSaved=saved;
        s.environmentInspector=guid;environmentPreviewSource_=nullptr;
      } else {s.profileInspector=guid;s.profileGroup=0;}
    }
    if(kind==Kind::Material) refreshMaterialSlotView();
    else if(kind==Kind::EnvironmentMap) refreshEnvironmentInspector();
    else refreshProfileInspector();
    const auto &after=kind==Kind::Material?s.materialInspector:kind==Kind::EnvironmentMap?s.environmentInspector:s.profileInspector;
    closed=!(after==guid);
  });
  if(closed) {
    focusedAssetLoaded_={};
    const u32 index=std::min(std::max(state_.focusedActive,1u),static_cast<u32>(state_.focusedInspectors.size()))-1;
    state_.focusedInspectors.erase(state_.focusedInspectors.begin()+index);
    if(index<focusedNames_.size()) focusedNames_.erase(focusedNames_.begin()+index);
    state_.focusedActive=state_.focusedInspectors.empty()?0:std::min<u32>(state_.focusedActive,static_cast<u32>(state_.focusedInspectors.size()));
    saveEditorPreferences();
  }
}

void EditorSession::openFocusedInspector(EditorEntityId entity,u64 component) {
  const auto *object=state_.document?state_.document->find(entity):nullptr;
  if(!object || entity==state_.document->root()) return;
  for(u32 i=0;i<state_.focusedInspectors.size();++i)
    if(state_.focusedInspectors[i].entity==entity && state_.focusedInspectors[i].component==component) {
      state_.focusedActive=i+1;state_.focusedCollapsed=false;return;
    }
  // Até oito: cada aba é uma janela da Unity, mas a tela do telefone é uma só.
  if(state_.focusedInspectors.size()>=8) {state_.focusedInspectors.erase(state_.focusedInspectors.begin());if(!focusedNames_.empty()) focusedNames_.erase(focusedNames_.begin());}
  state_.focusedInspectors.push_back({entity,component,EditorScreenState::FocusedAsset::None,{},{}});
  focusedNames_.push_back(object->name);
  state_.focusedActive=static_cast<u32>(state_.focusedInspectors.size());
  state_.focusedCollapsed=false;state_.focusedMenu=false;focusedValidated_=true;
  state_.status=std::string("Propriedades de ")+object->name;
  saveEditorPreferences();
}

void EditorSession::applyLayout(const EditorLayout &layout) {
  state_.hierarchyWidth=layout.hierarchyWidth;state_.inspectorWidth=layout.inspectorWidth;
  state_.hierarchyVisible=layout.hierarchyVisible;state_.inspectorVisible=layout.inspectorVisible;
  state_.filesCollapsed=layout.filesCollapsed;state_.diagnosticDockOpen=layout.diagnosticDock;
  state_.compactPanel=EditorScreenState::CompactPanel::Viewport;
  saveEditorPreferences();
}

void EditorSession::openGlobalSearch() {
  state_.globalSearch=true;state_.globalPage=0;
  // Índice dos arquivos do projeto, uma vez por abertura: nenhuma leitura de
  // disco por quadro. `.astra` e pastas ocultas ficam de fora, como no painel.
  searchFiles_.clear();
  const auto root=files_.rootPath();
  if(root.empty()) return;
  std::error_code error;
  const std::filesystem::path base=EditorImportTransaction::fromUtf8(root);
  for(std::filesystem::recursive_directory_iterator it(base,std::filesystem::directory_options::skip_permission_denied,error),end;
      !error && it!=end && searchFiles_.size()<EditorFileSystem::MaximumEntries;it.increment(error)) {
    const auto name=it->path().filename().u8string();
    const std::string leaf(name.begin(),name.end());
    if(leaf.empty() || leaf[0]=='.') {if(it->is_directory(error)) it.disable_recursion_pending();continue;}
    const auto relative=std::filesystem::relative(it->path(),base,error).generic_u8string();
    EditorFileEntry entry;entry.name=leaf;entry.relativePath.assign(relative.begin(),relative.end());
    entry.directory=it->is_directory(error);entry.depth=static_cast<unsigned>(it.depth());
    searchFiles_.push_back(std::move(entry));
  }
}

bool EditorSession::openSearchResult(u32 index) {
  if(index>=state_.globalResults.size()) return false;
  const auto result=state_.globalResults[index];
  state_.globalSearch=false;
  switch(result.provider) {
    case EditorSearchProvider::Scene: {
      const auto id=static_cast<EditorEntityId>(result.key);
      if(!document_.exists(id)) {state_.status="Objeto não existe mais";return false;}
      setSelection(id);pingEntity(id);
      state_.status="Selecionado: "+result.title;
      return true;
    }
    case EditorSearchProvider::Project: {
      if(result.key>=searchFiles_.size()) return false;
      const auto entry=searchFiles_[result.key];
      if(!files_.exists(entry.relativePath)) {state_.status="Arquivo não existe mais";return false;}
      state_.filesCollapsed=false;
      revealProjectPath(entry.relativePath);
      openProjectFile(entry);
      return true;
    }
    case EditorSearchProvider::Create: {
      const u32 recipe=static_cast<u32>(result.key);
      if(recipe>=editorCreationCatalog.size() || !creationAvailable(state_,recipe) || isPlaying()) return false;
      // Abre Criar na receita: o toque em Criar é o mesmo caminho do menu.
      state_.creationMenu=true;state_.creationCategory=editorCreationCatalog[recipe].category;
      state_.creationSelection=recipe;state_.creationScroll=0;state_.creationSearch[0]=0;
      return true;
    }
    case EditorSearchProvider::All: break;
  }
  return false;
}

void EditorSession::revealProjectPath(const std::string &relative) {
  // Cada pasta ancestral fechada é aberta pelo mesmo `toggle` do painel.
  for(usize slash=relative.find('/');slash!=std::string::npos;slash=relative.find('/',slash+1)) {
    const auto folder=relative.substr(0,slash);
    const auto &tree=files_.tree();
    for(unsigned i=0;i<tree.size();++i)
      if(tree[i].directory && tree[i].relativePath==folder) {if(!tree[i].expanded) files_.toggle(i);break;}
  }
}

void EditorSession::openProjectFile(const EditorFileEntry &entry) {
  // Escolher e a acao mais barata do toque, e vale para pasta e arquivo:
  // as acoes de recurso precisam de um alvo, e abrir um arquivo nem sempre
  // e possivel.
  state_.selectedFile=entry.relativePath;
  state_.pendingResourceDelete.clear();
  // R4: a pasta Texturas (ou uma subpasta) abre o gerenciador em Propriedades;
  // uma textura do projeto abre a própria textura, logo abaixo.
  state_.textureManager=false;state_.textureInspector=false;
  if(entry.directory && (entry.relativePath=="Texturas" || entry.relativePath.starts_with("Texturas/")))
    openTextureManager(entry.relativePath=="Texturas"?std::string():entry.relativePath);
  if(entry.directory) return;
  if(entry.name.ends_with(".wav")) {
    resources::AssetGuid asset;std::string error;
    if(!importWaveClip(entry.relativePath,asset,error))state_.status=error;
    return;
  }
  if(entry.name.ends_with(".cs") || entry.name.ends_with(".json") || entry.name.ends_with(".md")) {
    state_.code=&code_;
    if(code_.open(files_,entry.relativePath)) {state_.workspace=EditorWorkspace::Code;state_.codeFiles=false;}
    else state_.status=code_.error();
  }
  else if(entry.name.ends_with(".aescene"))
    requestedScenePath_=files_.resolveFile(entry.relativePath);
  else if(const auto *record=assets_.findByPath(entry.relativePath);record && record->type==resources::AssetType::Prefab) {
    runtime::Prefab prefab;std::string error;
    state_.status=loadPrefab(record->guid,prefab,error)?
      "Prefab · "+std::to_string(prefab.graph().entityCount()-1)+" objetos · Instanciar adiciona uma cópia vinculada à cena":error;
  }
  else if(const auto *record=assets_.findByPath(entry.relativePath);record && record->type==resources::AssetType::Material) {
    if(!openMaterialInspector(record->guid)) state_.status="Material registrado, mas o arquivo não pôde ser lido";
  }
  else if(const auto *record=assets_.findByPath(entry.relativePath);record && record->type==resources::AssetType::EnvironmentProfile) {
    if(!openProfileInspector(record->guid)) state_.status="Perfil registrado, mas o arquivo não pôde ser lido";
  }
  else if(const auto *record=assets_.findByPath(entry.relativePath);record && record->type==resources::AssetType::PhysicsMaterial) {
    if(!openPhysicsMaterialInspector(record->guid)) state_.status="Material físico registrado, mas o arquivo não pôde ser lido";
  }
  else if(const auto *record=assets_.findByPath(entry.relativePath);record && record->type==resources::AssetType::AnimatorController) {
    const auto guid=record->guid;
    loadAnimatorControllers();
    std::vector<EditorEntityId> owners;document_.collectSubtree(document_.root(),owners);
    for(const auto id:owners) if(const auto *object=document_.find(id))
      if(const auto *component=object->components.find(scene::Animator::descriptor);
          component&&scene::animator(*component).controller==guid) {
        state_.selection=state_.inspectorTarget=id;openAnimatorEditor();state_.animatorEditShared=true;state_.animatorDrawer=3;return;
      }
    // Unassigned resources stay editable without manufacturing a scene object.
    state_.code=&code_;
    if(code_.open(files_,entry.relativePath)) {state_.workspace=EditorWorkspace::Code;state_.codeFiles=false;}
    else state_.status=code_.error();
  }
  else if(const auto *record=assets_.findByPath(entry.relativePath);record && record->type==resources::AssetType::EnvironmentMap) {
    if(!openEnvironmentInspector(record->guid)) state_.status="Receita HDRI salva é inválida; Reimportar recria a partir da fonte";
  }
  else if(const auto *record=assets_.findByPath(entry.relativePath);record && record->type==resources::AssetType::Texture) {
    const auto *texture=findProjectTexture(record->guid);
    for(u32 index=0;index<textures_.size();++index)
      if(textures_[index].guid==record->guid) {openTextureInspector(index);break;}
    const auto users=textureUsersOf(record->guid);
    state_.status=std::string("Textura do projeto")+
        (texture && texture->width?" · "+std::to_string(texture->width)+"×"+std::to_string(texture->height):std::string())+
        " · "+std::to_string(users)+(users==1?" uso":" usos");
  }
  else if(const i32 source=sourceTextureForFile(entry.relativePath);source>=0) {
    // Bloco F: a imagem de uma fonte abre a textura que o modelo usa dela.
    openSourceTextureInspector(static_cast<u32>(source));
  }
  else {
    const auto *selectedAsset=assets_.findByPath(entry.relativePath);
    state_.status=selectedAsset&&selectedAsset->type==resources::AssetType::EnvironmentMap?
        "Mapa HDRI · Reimportar atualiza céu, irradiância e reflexões":
        entry.name.ends_with(".glb")?"Recurso GLB · Instanciar adiciona à cena; Reimportar atualiza a fonte; Texturas extrai as imagens":
        selectedAsset&&selectedAsset->type==resources::AssetType::Mesh?
        "Modelo em pasta · Instanciar adiciona à cena; Reimportar relê a pasta com o perfil da fonte":"Arquivo de origem";
  }
}

bool EditorSession::moveHistoryTo(u32 applied) {
  if(isPlaying() || applied>history_.entryCount()) return false;
  bool changed=false;
  while(history_.undoDepth()!=applied) {
    EditorActionRequest command;
    command.version=sceneVersion();command.entity=state_.selection;
    command.action=history_.undoDepth()>applied?EditorAction::Undo:EditorAction::Redo;
    if(dispatch(command).status!=EditorActionStatus::Applied) break;
    changed=true;
  }
  if(changed && state_.meshPicker && state_.resourceProperty=="collision_mesh") refreshCollisionMeshDraft();
  return history_.undoDepth()==applied;
}

void EditorSession::restoreSceneVisibility() {
  // Só voltam os ids que ainda são o mesmo objeto (mesmo nome) nesta cena.
  const auto restore=[&](const std::vector<std::pair<EditorEntityId,std::string>> &saved,std::vector<EditorEntityId> &out) {
    out.clear();
    for(const auto &[id,name]:saved) if(const auto *e=document_.find(id); e && std::string_view(e->name)==name) out.push_back(id);
  };
  restore(sceneHiddenNames_,state_.sceneHidden);restore(scenePickOffNames_,state_.scenePickOff);
  appearanceChanged_=true;
}

void EditorSession::validateFocusedInspectors() {
  // Só voltam as abas cujo id ainda é o mesmo objeto (mesmo nome) e, se de
  // componente, cuja instância ainda existe nele.
  std::vector<EditorScreenState::FocusedInspector> kept;std::vector<std::string> names;
  for(u32 i=0;i<state_.focusedInspectors.size();++i) {
    const auto &focused=state_.focusedInspectors[i];
    if(focused.kind!=EditorScreenState::FocusedAsset::None) {
      if(assets_.find(focused.asset)) {kept.push_back(focused);names.push_back(focused.name);}
      continue;
    }
    const auto *object=document_.find(focused.entity);
    if(!object || (i<focusedNames_.size() && focusedNames_[i]!=object->name)) continue;
    if(focused.component && !object->components.findInstance(focused.component)) continue;
    kept.push_back(focused);names.push_back(object->name);
  }
  state_.focusedInspectors=std::move(kept);focusedNames_=std::move(names);
  state_.focusedActive=state_.focusedInspectors.empty()?0:std::min<u32>(std::max(state_.focusedActive,1u),static_cast<u32>(state_.focusedInspectors.size()));
  focusedValidated_=true;
  // O arquivo só muda quando o usuário abre ou fecha abas: outra cena do
  // projeto não apaga as abas da cena onde foram abertas.
}

void EditorSession::pingEntity(EditorEntityId id) {
  const auto *entity=document_.find(id);
  if(!entity || id==document_.root()) return;
  auto &collapsed=state_.collapsedEntities;
  for(const auto *up=document_.find(entity->parent);up;up=document_.find(up->parent))
    collapsed.erase(std::remove(collapsed.begin(),collapsed.end(),up->id),collapsed.end());
  // A busca da Hierarquia esconderia o objeto: sai dela.
  if(state_.hierarchySearch[0] && editorSearchKey(entity->name).find(editorSearchKey(state_.hierarchySearch))==std::string::npos)
    state_.hierarchySearch[0]=0;
  // Posição na mesma travessia que desenha a Hierarquia.
  u32 row=0,found=0;bool located=false;
  std::vector<EditorEntityId> stack;
  const auto roots=document_.childrenOf(document_.root());
  for(usize i=roots.size();i>0;--i) stack.push_back(roots[i-1]);
  while(!stack.empty() && !located) {
    const auto current=stack.back();stack.pop_back();
    if(current==id) {found=row;located=true;break;}
    ++row;
    if(std::find(collapsed.begin(),collapsed.end(),current)!=collapsed.end()) continue;
    const auto children=document_.childrenOf(current);
    for(usize i=children.size();i>0;--i) stack.push_back(children[i-1]);
  }
  const u32 visible=std::max(1u,layout_.hierarchyVisibleRows);
  state_.hierarchyScroll=found>visible/2?found-visible/2:0;
  state_.pingEntity=id;state_.pingUntil=state_.uiTime+1.6;
  if(state_.compactPanel!=EditorScreenState::CompactPanel::Hierarchy &&
     layout_.hierarchyPanel.isEmpty()) state_.compactPanel=EditorScreenState::CompactPanel::Hierarchy;
  state_.status=std::string("Ping: ")+entity->name;
}

bool EditorSession::handlePointerNow(const UiPointerEvent &event) {
  if(event.phase==UiPointerPhase::Cancel) {playTouches_.cancel();cancelPlayButtons();}
  if(state_.platformTextInput && pendingTextEdit().purpose!=EditorTextPurpose::None) {
    // O campo do sistema tem o foco; só o botão "Expressão" da barra responde,
    // trocando o teclado numérico pelo de texto sem perder o que foi digitado.
    const auto routed=router_.route(event);
    if(routed.tapped && routed.widgetId==widgetId(EditorWidget::NumericExpressionToggle) && state_.numericField) {
      if(!state_.platformDraft.empty() && state_.platformDraft.size()<sizeof(state_.numericText))
        std::snprintf(state_.numericText,sizeof(state_.numericText),"%s",state_.platformDraft.c_str());
      state_.numericExpression=!state_.numericExpression;state_.numericReplace=false;
    }
    return true;
  }
  UiPointerRouting routing = router_.route(event);
  if(handleColliderAuthoringInput(event,routing))return true;
  if(routing.target==UiPointerTarget::Widget &&
     ((routing.widgetId>=widgetId(EditorWidget::GuiHierarchyRowBase)&&routing.widgetId<widgetId(EditorWidget::GuiHierarchyRowBase)+0x00100000u) ||
      (routing.widgetId>=widgetId(EditorWidget::GuiHierarchyCollapseBase)&&routing.widgetId<widgetId(EditorWidget::GuiHierarchyCollapseBase)+0x00100000u))) {
    if(routing.tapped) {
      const bool fold=routing.widgetId>=widgetId(EditorWidget::GuiHierarchyCollapseBase);
      const auto *row=guiTree_.find(routing.widgetId-widgetId(fold?EditorWidget::GuiHierarchyCollapseBase:EditorWidget::GuiHierarchyRowBase));
      if(row) {
        if(fold) {
          auto &closed=state_.collapsedGui;const auto at=std::find(closed.begin(),closed.end(),row->target);
          if(at==closed.end())closed.push_back(row->target);else closed.erase(at);
        } else if(!row->error.empty())state_.status=row->error;
        else selectGuiElement(row->target);
      }
    }
    return true;
  }
  if(routing.tapped && routing.widgetId>=widgetId(EditorWidget::HierarchyRowBase) &&
     routing.widgetId<widgetId(EditorWidget::HierarchyRowBase)+EditorDocument::kMaximumEntities) {
    gui_.history().commit(gui_.document());gui_.cancelPointers();state_.guiSelection={};state_.guiInspector=false;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::GuiCanvasEdit)) {
    const auto *object=document_.find(state_.selection);
    const auto *value=object?object->components.findInstance(state_.expandedNative):nullptr;
    if(value&&value->type().id==scene::UiCanvas::descriptor.id) {
      const auto asset=static_cast<const scene::UiCanvas&>(*value).document;const auto *record=assets_.find(asset);
      if(record&&record->type==resources::AssetType::UiDocument&&gui_.openResource(record->path))openGui();
      else state_.status="Documento UI ausente, inválido ou documento atual ainda não salvo";
    }
    return true;
  }
  if(guiActive() && routing.tapped) {
    if(routing.widgetId==widgetId(EditorWidget::Undo)) {gui_.history().commit(gui_.document());history_.undo(document_);return true;}
    if(routing.widgetId==widgetId(EditorWidget::Redo)) {history_.redo(document_);return true;}
    if(routing.widgetId==widgetId(EditorWidget::SaveDocument)) {gui_.save();return true;}
  }
  if(event.phase==UiPointerPhase::Down) {
    const auto old=std::find_if(playButtonPointers_.begin(),playButtonPointers_.end(),[&](const auto&p){return p.first==event.pointerId;});
    if(old!=playButtonPointers_.end()) {playButtonPointers_.erase(old);playButtonCanceled_|=runtime::InputTouch;}
  }
  if(routing.target==ui::UiPointerTarget::Widget &&
     (routing.widgetId==widgetId(EditorWidget::JumpCharacter)||routing.widgetId==widgetId(EditorWidget::PlaySecondaryAction))) {
    const u32 mask=routing.widgetId==widgetId(EditorWidget::JumpCharacter)?1u:2u;
    if(event.phase==UiPointerPhase::Down && isPlaying()&&!state_.playPaused&&gameplayInputFocused()) {
      playButtonPointers_.erase(std::remove_if(playButtonPointers_.begin(),playButtonPointers_.end(),
          [&](const auto&p){return p.first==event.pointerId;}),playButtonPointers_.end());
      playButtonPointers_.emplace_back(event.pointerId,mask);
      if(mask==1)jumpPressed_=true;else secondaryPressed_=true;
    }
    if(routing.released) {
      playButtonPointers_.erase(std::remove_if(playButtonPointers_.begin(),playButtonPointers_.end(),
          [&](const auto&p){return p.first==event.pointerId;}),playButtonPointers_.end());
      if(event.phase==UiPointerPhase::Cancel){playButtonCanceled_|=runtime::InputTouch;jumpPressed_=secondaryPressed_=false;}
    }
    return true;
  }
  if(inputCapture_ && !validateInputCapture()) cancelInputBindingCapture();
  if(state_.inputCapturing) {
    if(!routing.tapped) return true;
    cancelInputBindingCapture();
    if(routing.widgetId==widgetId(EditorWidget::InputBindingCaptureCancel)) return true;
  }
  if(routing.tapped) {
    const auto key=routing.widgetId;
    if(key>=widgetId(EditorWidget::TimerControlBase)&&key<widgetId(EditorWidget::TimerControlBase)+0x10000u) {
      if(!playScene_.active()||!playInspecting())return true;
      const auto target=state_.inspectorTarget?state_.inspectorTarget:state_.selection;
      const auto *entity=playScene_.document().find(target);const auto offset=key-widgetId(EditorWidget::TimerControlBase);
      const auto *component=entity?entity->components.at(offset/4):nullptr;
      if(!component||&component->type()!=&scene::Timer::descriptor||offset%4>2)return true;
      const auto *current=playScene_.timers().state(target,component->instanceId());
      const u32 operation=offset%4==0?1u:offset%4==2?2u:(current&&current->paused?4u:3u);
      runtime::SceneTimers::State result;const auto status=playScene_.timerCommand(target,component->instanceId(),operation,0,result);
      state_.status=status==runtime::WorldStatus::Ok?(operation==1?"Timer iniciado":operation==2?"Timer parado":operation==3?"Timer pausado":"Timer retomado"):"Comando de timer recusado";
      return true;
    }
    if(key==widgetId(EditorWidget::InputBindingCode) && state_.workspace==EditorWorkspace::Project && state_.projectSection==EditorProjectSection::Input) {
      auto map=document_.inputActions();const auto &actions=map.actions();
      if(state_.inputActionIndex<actions.size()) {
        auto action=actions[state_.inputActionIndex];
        if(state_.inputBindingIndex<action.bindings.size()) {
          auto &binding=action.bindings[state_.inputBindingIndex];
          const u32 count=binding.source==runtime::InputSource::MouseButton?5u:binding.source==runtime::InputSource::MouseAxis?4u:0u;
          if(count) {
            if(history_.isOpen())return true;
            binding.code=(binding.code+1)%count;
            if(map.replace(action.id,action) && history_.setInputActions(document_,map))state_.status="Canal do mouse atualizado";
            return true;
          }
        }
      }
    }
    if(key==widgetId(EditorWidget::InputBindingCapture) || key==widgetId(EditorWidget::InputBindingCaptureNegative)) {
      beginInputBindingCapture(key==widgetId(EditorWidget::InputBindingCaptureNegative));return true;
    }
    if(key==widgetId(EditorWidget::PlayTimeScale)) {
      constexpr float scales[]{0,.25f,.5f,1,2,4};
      const float current=playScene_.active()?playScene_.world().clock().scale():1.f;
      float next=0;for(float scale:scales) if(scale>current){next=scale;break;}
      if(!setPlayTimeScale(next)) state_.status="Tempo indisponível: inicie a execução da cena";
      return true;
    }
    if(key==widgetId(EditorWidget::ObjectTagOpen)) {state_.tagPicker=true;state_.tagPage=0;state_.tagQuery.clear();return true;}
    if(key==widgetId(EditorWidget::ObjectGroupsOpen)) {
      const auto target=state_.inspectorTarget?state_.inspectorTarget:state_.selection;
      if(document_.find(target)) {state_.groupPicker=true;state_.groupEntity=target;state_.groupPage=0;state_.tagPicker=false;}
      return true;
    }
    if(key==widgetId(EditorWidget::GroupsClose)) {state_.groupPicker=false;state_.editingGroupName=false;return true;}
    if(key==widgetId(EditorWidget::GroupsPrevious)) {if(state_.groupPage) --state_.groupPage;return true;}
    if(key==widgetId(EditorWidget::GroupsNext)) {++state_.groupPage;return true;}
    if(key==widgetId(EditorWidget::GroupNew)) {
      if(document_.find(state_.groupEntity)) {state_.editingGroupName=true;state_.groupEditSlot=0;state_.renameText[0]=0;}
      return true;
    }
    if(key>=widgetId(EditorWidget::GroupEditBase) && key<widgetId(EditorWidget::GroupEditBase)+runtime::ObjectGroups::MaximumCount) {
      const auto index=key-widgetId(EditorWidget::GroupEditBase);const auto *object=document_.find(state_.groupEntity);
      if(state_.groupPicker && object && index<object->groups.names().size()) {
        state_.groupEditSlot=index+1;state_.editingGroupName=true;
        std::snprintf(state_.renameText,sizeof(state_.renameText),"%s",object->groups.names()[index].c_str());
      }
      return true;
    }
    if(key>=widgetId(EditorWidget::GroupRemoveBase) && key<widgetId(EditorWidget::GroupRemoveBase)+runtime::ObjectGroups::MaximumCount) {
      const auto index=key-widgetId(EditorWidget::GroupRemoveBase);const auto *object=document_.find(state_.groupEntity);
      if(state_.groupPicker && object && index<object->groups.names().size() && !history_.isOpen()) {
        auto value=*object;const auto name=value.groups.names()[index];value.groups.remove(name);
        if(history_.applyValues(document_,object->id,value)) state_.status="Associação removida: "+name;
      }
      return true;
    }
    if(key==widgetId(EditorWidget::TagClose)) {state_.tagPicker=false;return true;}
    if(key==widgetId(EditorWidget::TagNew)) {state_.editingTagName=true;state_.renameText[0]=0;return true;}
    if(key==widgetId(EditorWidget::TagSearch)) {
      state_.editingTagSearch=true;std::snprintf(state_.renameText,sizeof(state_.renameText),"%s",state_.tagQuery.c_str());return true;
    }
    if(key==widgetId(EditorWidget::TagPrevious)) {if(state_.tagPage) --state_.tagPage;return true;}
    if(key==widgetId(EditorWidget::TagNext)) {++state_.tagPage;return true;}
    if(key==widgetId(EditorWidget::TagDelete)) {
      auto tags=projectTags_;if(tags.remove(state_.tagSelected) && changeProjectTags(tags)) {
        state_.tagSelected="Untagged";state_.status="Tag excluída do catálogo";
      }
      return true;
    }
    if(key>=widgetId(EditorWidget::TagRowBase) && key<widgetId(EditorWidget::TagRowBase)+runtime::ObjectTags::MaximumCount) {
      const auto index=key-widgetId(EditorWidget::TagRowBase);const auto &names=document_.tags().names();
      if(index<names.size()) {if(state_.tagPicker) assignTag(names[index]);else state_.tagSelected=names[index];}
      return true;
    }
  }
  if(multiAssetEditing_ && state_.materialInspector.valid() && routing.tapped &&
     routing.heldSeconds>=ui::kUiLongPressSeconds && state_.setValueMenu.key.empty()) {
    const auto field=materialWidgetField(routing.widgetId,state_.textureBinding);
    if(!field.empty() && state_.materialMixedHas(field)) {
      state_.setValueMenu.key="asset.mat."+field;state_.setValueMenu.label="Valor do material";
      if(field.starts_with("num")) state_.setValueMenu.label=scene::meshRendererNumbers[std::stoul(field.substr(3))].name;
      state_.setValueMenu.rows.clear();
      for(u32 i=0;i<multiMaterials_.size();++i) if(const auto *material=findMaterialAsset(multiMaterials_[i])) {
        auto value=materialFieldText(*material,field);
        if(field.starts_with("tex") || field=="ch1") {
          const auto texture=field=="ch1"?material->occlusionTexture:material->textures[std::stoul(field.substr(3))];
          const auto *record=assets_.find(texture);
          value=texture==scene::MaterialTextureNone?"Sem textura":!texture.valid()?"Como a fonte":record?baseName(record->path):"Textura ausente";
        }
        state_.setValueMenu.rows.push_back({i,material->name+" · "+value});
      }
      return true;
    }
  }

  if(multiAssetEditing_ && state_.profileInspector.valid() && routing.tapped &&
     routing.heldSeconds>=ui::kUiLongPressSeconds && state_.setValueMenu.key.empty() &&
     routing.widgetId>=widgetId(EditorWidget::ProfileRowBase) &&
     routing.widgetId<widgetId(EditorWidget::ProfileRowBase)+state_.profileRows.size())
    if(showProfileValueMenu(routing.widgetId-widgetId(EditorWidget::ProfileRowBase))) return true;

  if(multiAssetEditing_ && state_.environmentInspector.valid() && routing.tapped &&
     routing.heldSeconds>=ui::kUiLongPressSeconds && state_.setValueMenu.key.empty()) {
    const u32 key=routing.widgetId;
    const bool down=key>=widgetId(EditorWidget::EnvironmentRecipeDownBase) && key<widgetId(EditorWidget::EnvironmentRecipeDownBase)+5;
    const bool up=key>=widgetId(EditorWidget::EnvironmentRecipeUpBase) && key<widgetId(EditorWidget::EnvironmentRecipeUpBase)+5;
    if((down||up) && showEnvironmentValueMenu(key-widgetId(down?EditorWidget::EnvironmentRecipeDownBase:EditorWidget::EnvironmentRecipeUpBase))) return true;
  }
  // "Definir como o valor de…": modal pequeno com um objeto por linha.
  if(!state_.setValueMenu.key.empty() && routing.tapped) {
    const u32 key=routing.widgetId;
    if(key>=widgetId(EditorWidget::SetValueRowBase) && key<widgetId(EditorWidget::SetValueRowBase)+state_.setValueMenu.rows.size())
      applySetValue(key-widgetId(EditorWidget::SetValueRowBase));
    state_.setValueMenu={};
    return true;
  }
  // Toque longo num campo com valores diferentes abre o menu (Unity: clique
  // direito › Set to Value of).
  if(routing.tapped && routing.heldSeconds>=ui::kUiLongPressSeconds && state_.selectionSet.size()>1 && !isPlaying()) {
    const u32 key=routing.widgetId,operation=key&0xff000000u,low=key&0x00ffffffu;
    const auto *entity=document_.find(state_.selection);
    std::string field,label;
    if(key>=widgetId(EditorWidget::TransformFieldBase) && key<widgetId(EditorWidget::TransformFieldBase)+9) {
      const u32 row=(key-widgetId(EditorWidget::TransformFieldBase))/3,axis=(key-widgetId(EditorWidget::TransformFieldBase))%3;
      field="t."+std::to_string(row)+std::to_string(axis);
      label=std::string(row==0?"Posição":row==1?"Rotação":"Escala")+" "+"XYZ"[axis];
    } else if(entity && (operation==widgetId(EditorWidget::ComponentNumberBase) || operation==widgetId(EditorWidget::ComponentBooleanBase) ||
              operation==widgetId(EditorWidget::ComponentEnumBase) || operation==widgetId(EditorWidget::ComponentReferenceBase))) {
      const auto *component=entity->components.at(low&0xffu);const u32 property=low>>8;
      if(component) {
        const auto &type=component->type();
        const auto id=operation==widgetId(EditorWidget::ComponentNumberBase)?(property<type.numbers.size()?type.numbers[property].id:std::string_view{}):
            operation==widgetId(EditorWidget::ComponentBooleanBase)?(property<type.booleans.size()?type.booleans[property].id:std::string_view{}):
            operation==widgetId(EditorWidget::ComponentEnumBase)?(property<type.enums.size()?type.enums[property].id:std::string_view{}):
            (property<type.references.size()?type.references[property].id:std::string_view{});
        const char *name=operation==widgetId(EditorWidget::ComponentNumberBase)?(property<type.numbers.size()?type.numbers[property].name:""):
            operation==widgetId(EditorWidget::ComponentBooleanBase)?(property<type.booleans.size()?type.booleans[property].name:""):
            operation==widgetId(EditorWidget::ComponentEnumBase)?(property<type.enums.size()?type.enums[property].name:""):
            (property<type.references.size()?type.references[property].name:"");
        if(!id.empty()) {field=multiKey(component->instanceId(),id);label=name;}
      }
    }
    if(!field.empty()) {
      // O Down do campo da Transformação já abriu a transação do arraste: o
      // toque longo não edita, então ela é descartada.
      if(fieldWidget_==key) {history_.cancel(document_);fieldWidget_=0;}
      if(!state_.multi.isMixed(field)) {state_.status="Todos os selecionados já têm o mesmo valor";return true;}
      state_.setValueMenu.key=field;state_.setValueMenu.label=label;state_.setValueMenu.rows.clear();
      // Cada objeto com o valor que tem hoje, para escolher de quem copiar.
      const auto slash=field.find('/');
      const scene::ComponentValue *reference=slash==std::string::npos?nullptr:entity->components.findInstance(std::stoull(field.substr(0,slash)));
      for(const auto id:state_.selectionSet) if(const auto *object=document_.find(id)) {
        std::string value;
        if(field.starts_with("t.")) value=transformChannel(object->transform,field[2]-'0',field[3]-'0');
        else if(reference) if(const auto *mine=matchComponent(object->components,componentKind(*reference),componentOccurrence(entity->components,*reference)))
          value=propertyText(*mine,field.substr(slash+1));
        state_.setValueMenu.rows.push_back({id,std::string(object->name)+"  \xC2\xB7  "+value});
      }
      return true;
    }
  }
  // Toque longo no cabeçalho de um componente abre o menu dele, como o clique
  // direito da Unity (Manual/UsingComponents).
  // Unity "PropertyEditor/OpenMouseOver": toque longo numa linha da Hierarquia
  // abre o Inspector focado daquele objeto sem trocar a seleção.
  if(routing.tapped && routing.heldSeconds>=ui::kUiLongPressSeconds &&
     routing.widgetId>=widgetId(EditorWidget::HierarchyRowBase) &&
     routing.widgetId<widgetId(EditorWidget::HierarchyRowBase)+EditorDocument::kMaximumEntities && !isPlaying()) {
    openFocusedInspector(routing.widgetId-widgetId(EditorWidget::HierarchyRowBase));
    return true;
  }
  if(routing.tapped && routing.heldSeconds>=ui::kUiLongPressSeconds) {
    const u32 operation=routing.widgetId&0xff000000u,index=routing.widgetId&0x00ffffffu;
    if(operation==widgetId(EditorWidget::ComponentFoldBase)) routing.widgetId=widgetId(EditorWidget::ComponentMenuBase)+index;
    else if(operation==widgetId(EditorWidget::ScriptFoldBase)) routing.widgetId=widgetId(EditorWidget::ScriptMenuBase)+index;
  }
  const u32 colliderHandleBase=widgetId(EditorWidget::ColliderHandleBase);
  if(colliderDragOpen_ || (routing.widgetId>=colliderHandleBase &&
      routing.widgetId<colliderHandleBase+static_cast<u32>(ColliderHandleKind::Count))) {
    if(!colliderDragOpen_ && event.phase==UiPointerPhase::Down && !isPlaying() &&
       viewportPointers_.empty() && !history_.isOpen() && state_.showComponentVisuals &&
       !state_.cameraViewEntity && !state_.multiSelect && state_.selectionSet.size()<=1 &&
       !state_.guiSelection.valid() && componentVisualSelectable(document_,state_.selection,
           state_.hiddenLayers,state_.unpickableLayers,state_.sceneHidden,state_.scenePickOff)) {
      const auto *entity=document_.find(state_.selection);
      const auto kind=static_cast<ColliderHandleKind>(routing.widgetId-colliderHandleBase);
      if(entity && colliderHandleMatchesTool(kind,state_.tool) && colliderHandleGeometry(document_,entity->id,
          state_.expandedNative,kind,view_,colliderDragHandle_)) {
        const bool resolved=colliderDragHandle_.rotation?
            colliderRingAngle(view_,colliderDragHandle_,event.position,colliderDragStart_):
            cameraHandleRayParameter(view_,colliderDragHandle_,event.position,colliderDragStart_);
        if(resolved && history_.begin("Editar Colisor")) {
          colliderDragOpen_=true;colliderDragPointer_=event.pointerId;
          colliderDragInitial_=*entity;colliderDragView_=view_;
          colliderDragLastAngle_=colliderDragStart_;colliderDragAngle_=0;
          state_.activeColliderHandle=static_cast<u32>(kind)+1;
        }
      }
    }
    if(colliderDragOpen_ && event.pointerId==colliderDragPointer_) {
      if(event.phase==UiPointerPhase::Cancel || state_.selection!=colliderDragInitial_.id ||
         state_.expandedNative!=colliderDragHandle_.instance || !document_.exists(colliderDragInitial_.id)) {
        history_.cancel(document_);colliderDragOpen_=false;state_.activeColliderHandle=0;
      } else {
        if(routing.dragging) {
          float parameter;
          const bool resolved=colliderDragHandle_.rotation?
              colliderRingAngle(colliderDragView_,colliderDragHandle_,event.position,parameter):
              cameraHandleRayParameter(colliderDragView_,colliderDragHandle_,event.position,parameter);
          if(resolved) {
            float delta=parameter-colliderDragStart_;
            if(colliderDragHandle_.rotation) {
              colliderDragAngle_+=std::remainder(parameter-colliderDragLastAngle_,6.28318530718f);
              colliderDragLastAngle_=parameter;delta=colliderDragAngle_;
            }
            auto changed=colliderDragInitial_;
            if(applyColliderHandleDelta(changed,colliderDragHandle_,delta) &&
               history_.applyValues(document_,changed.id,changed,0xCA04u)) {
              state_.status="Colisor "+std::to_string(colliderDragHandle_.instance)+" · "+
                  std::string(colliderDragHandle_.property)+" alterado";
            }
          }
        }
        if(routing.released) {history_.end();colliderDragOpen_=false;state_.activeColliderHandle=0;}
      }
    }
    // Another finger cannot switch selection, tool or camera during the gesture.
    return true;
  }
  const u32 lensHandleBase=widgetId(EditorWidget::CameraHandleBase);
  if(lensDragOpen_ || (routing.widgetId>=lensHandleBase && routing.widgetId<lensHandleBase+3)) {
    if(!lensDragOpen_ && event.phase==UiPointerPhase::Down && !isPlaying() &&
       viewportPointers_.empty() && !history_.isOpen()) {
      const auto *entity=document_.find(state_.selection);
      const u32 kind=routing.widgetId-lensHandleBase;
      if(entity && cameraHandleGeometry(document_,entity->id,kind,lensDragHandle_) &&
         cameraHandleRayParameter(view_,lensDragHandle_,event.position,lensDragStart_) &&
         history_.begin(kind==0?"Ajustar lente":"Ajustar recorte")) {
        lensDragOpen_=true;lensDragPointer_=event.pointerId;lensDragKind_=kind;
        lensDragInitial_=*entity;lensDragView_=view_;
      }
    }
    if(lensDragOpen_ && event.pointerId==lensDragPointer_) {
      if(event.phase==UiPointerPhase::Cancel) {history_.cancel(document_);lensDragOpen_=false;}
      else {
        if(routing.dragging) {
          float parameter;
          if(cameraHandleRayParameter(lensDragView_,lensDragHandle_,event.position,parameter)) {
            auto changed=lensDragInitial_;auto *camera=editCamera(changed);
            if(camera && applyCameraHandleDelta(*camera,lensDragKind_,lensDragHandle_.depth,parameter-lensDragStart_))
              history_.applyValues(document_,changed.id,changed,0xCA02u);
          }
        }
        if(routing.released) {history_.end();lensDragOpen_=false;}
      }
    }
    return true;
  }
  const u32 componentHandleBase=widgetId(EditorWidget::ComponentHandleBase);
  if(componentDragOpen_ || (routing.widgetId>=componentHandleBase &&
                            routing.widgetId<componentHandleBase+static_cast<u32>(EditorComponentHandleKind::Count))) {
    if(!componentDragOpen_ && event.phase==UiPointerPhase::Down && !isPlaying() &&
       viewportPointers_.empty() && !history_.isOpen()) {
      const auto *entity=document_.find(state_.selection);
      const auto kind=static_cast<EditorComponentHandleKind>(routing.widgetId-componentHandleBase);
      if(entity && componentHandleGeometry(document_,entity->id,state_.expandedNative,kind,componentDragHandle_) &&
         cameraHandleRayParameter(view_,componentDragHandle_,event.position,componentDragStart_) &&
         history_.begin("Ajustar componente")) {
        componentDragOpen_=true;componentDragPointer_=event.pointerId;
        componentDragInitial_=*entity;componentDragView_=view_;
      }
    }
    if(componentDragOpen_ && event.pointerId==componentDragPointer_) {
      if(event.phase==UiPointerPhase::Cancel) {history_.cancel(document_);componentDragOpen_=false;}
      else {
        if(routing.dragging) {
          float parameter;
          if(cameraHandleRayParameter(componentDragView_,componentDragHandle_,event.position,parameter)) {
            auto changed=componentDragInitial_;
            if(applyComponentHandleDelta(changed,componentDragHandle_,parameter-componentDragStart_))
              history_.applyValues(document_,changed.id,changed,0xCA03u);
          }
        }
        if(routing.released) {history_.end();componentDragOpen_=false;}
      }
    }
    return true;
  }
  if(cameraGestureOpen_ && routing.target==UiPointerTarget::Widget) finishCameraGesture(false);
  if(!state_.numericField && handlePathEditorInput(event,routing)) return true;
  if(state_.colorField) return handleColorWindow(event,routing);
  if(state_.gradientField) return handleGradientEditor(event,routing);
  if(routing.tapped&&routing.widgetId==animator_widget::id(animator_widget::Open)) {openAnimatorEditor();return true;}
  if(state_.animatorOpen&&!state_.numericField&&!state_.editingAnimatorName) return handleAnimatorEditor(event,routing);
  if(state_.curveField && !state_.numericField) return handleCurveEditor(event,routing);
  if(state_.codeRecoveryPending) {
    if(routing.tapped && routing.widgetId==widgetId(EditorWidget::CodeRecover)) {
      if(code_.restoreRecovery(files_)) {
        state_.codeRecoveryPending=false;state_.workspace=EditorWorkspace::Code;state_.codeFiles=false;
        reportProblem(EditorConsoleSeverity::Info,"Rascunhos recuperados. Arquivos alterados externamente continuam protegidos contra sobrescrita.");
      } else reportProblem(EditorConsoleSeverity::Error,code_.error());
    }
    if(routing.tapped && routing.widgetId==widgetId(EditorWidget::CodeDiscardRecovery)) {
      if(code_.discardRecovery(files_)) state_.codeRecoveryPending=false;
      else reportProblem(EditorConsoleSeverity::Error,code_.error());
    }
    return true;
  }
  // R3: o importador é um painel, não uma janela. Os toques nos controles dele
  // são tratados aqui; o resto do editor (viewport, hierarquia, arquivos)
  // continua respondendo enquanto a prévia está aberta.
  if(state_.importPanel && routing.tapped) {
    const auto is=[&routing](EditorWidget widget) {return routing.widgetId==widgetId(widget);};
    if(state_.importBatch) {
      if(is(EditorWidget::ImportCancel)) {state_.importCancel=true;closeImportPreview();}
      return true;
    }
    if(state_.importTexture) {
      if(is(EditorWidget::ImportCancel)) {state_.importCancel=true;closeImportPreview();}
      else if(is(EditorWidget::ImportPreviousPage)) {if(state_.importPage) --state_.importPage;}
      else if(is(EditorWidget::ImportNextPage)) state_.importPage=std::min(3u,state_.importPage+1);
      else if(is(EditorWidget::ImportAccept) && state_.importReady &&
              resources::sameTexturePreparation(state_.textureImportSettings,state_.textureImportPreparedSettings)) {
        state_.importAccept=true;state_.importReady=false;state_.importStatus="Publicando textura…";
      } else {
        auto &profile=state_.textureImportSettings;
        bool changed=true;
        if(is(EditorWidget::TextureProfileInterpretation)) profile.interpretation=static_cast<u8>((profile.interpretation+1u)%4u);
        else if(is(EditorWidget::TextureProfileDimension)) {
          const auto &steps=resources::TextureDimensionSteps;
          const auto current=std::find(steps.begin(),steps.end(),profile.maximumDimension);
          profile.maximumDimension=current==steps.end()||current+1==steps.end()?steps.front():*(current+1);
        } else if(is(EditorWidget::TextureProfileMipmaps)) profile.mipmaps=!profile.mipmaps;
        else if(is(EditorWidget::TextureProfileEdges)) profile.dilateEdges=!profile.dilateEdges;
        else if(is(EditorWidget::TextureProfileAnisotropy)) profile.anisotropy=!profile.anisotropy;
        else if(is(EditorWidget::TextureProfileNormalGreen)) profile.invertNormalGreen=!profile.invertNormalGreen;
        else if(is(EditorWidget::TextureProfileCoverage)) profile.preserveAlphaCoverage=!profile.preserveAlphaCoverage;
        else if(is(EditorWidget::TextureProfileCoverageCutoff)) {
          profile.alphaCoverageCutoff=std::round((profile.alphaCoverageCutoff+.05f)*20.0f)/20.0f;
          if(profile.alphaCoverageCutoff>1.0f) profile.alphaCoverageCutoff=0;
        }
        // Streaming não muda os bytes preparados: vale na publicação, sem repreparar.
        else if(is(EditorWidget::TextureProfileStreaming)) {profile.streamingMipmaps=!profile.streamingMipmaps;changed=false;}
        else if(is(EditorWidget::TextureProfileStreamingPriority)) {
          profile.streamingPriority=nextStreamingPriority(profile.streamingPriority);changed=false;
        }
        else changed=false;
        if(changed) {
          state_.textureImportReprepare=true;state_.importReady=false;state_.importError=false;
          state_.importStatus="Repreparando textura com a receita…";
        }
      }
      return true;
    }
    if(state_.importEnvironment) {
      if(is(EditorWidget::ImportCancel)) {state_.importCancel=true;closeImportPreview();}
      else if(is(EditorWidget::ImportAccept) && state_.importReady) {
        state_.importAccept=true;state_.importReady=false;state_.importStatus="Publicando mapa HDRI…";
      } else {
        const auto step=[&](u32 &value,std::span<const u32> choices,EditorWidget down,EditorWidget up) {
          if(!is(down)&&!is(up)) return false;
          usize index=0;while(index+1<choices.size()&&choices[index]<value) ++index;
          if(is(down)&&index) --index;
          if(is(up)&&index+1<choices.size()) ++index;
          if(value==choices[index]) return true;
          value=choices[index];state_.environmentImportReprepare=true;state_.importReady=false;
          state_.importError=false;state_.importStatus="Repreparando iluminação HDRI…";return true;
        };
        const auto &panorama=resources::EnvironmentPanoramaSteps;
        const auto &specular=resources::EnvironmentSpecularSizeSteps;
        const auto &brdf=resources::EnvironmentBrdfSizeSteps;
        const auto &specularSamples=resources::EnvironmentSpecularSampleSteps;
        const auto &brdfSamples=resources::EnvironmentBrdfSampleSteps;
        const bool handled=step(state_.environmentImportSettings.panoramaWidth,panorama,
             EditorWidget::EnvironmentPanoramaDown,EditorWidget::EnvironmentPanoramaUp)||
        step(state_.environmentImportSettings.specularSize,specular,
             EditorWidget::EnvironmentSpecularDown,EditorWidget::EnvironmentSpecularUp)||
        step(state_.environmentImportSettings.brdfSize,brdf,
             EditorWidget::EnvironmentBrdfDown,EditorWidget::EnvironmentBrdfUp)||
        step(state_.environmentImportSettings.specularSamples,specularSamples,
             EditorWidget::EnvironmentSpecularSamplesDown,EditorWidget::EnvironmentSpecularSamplesUp)||
        step(state_.environmentImportSettings.brdfSamples,brdfSamples,
             EditorWidget::EnvironmentBrdfSamplesDown,EditorWidget::EnvironmentBrdfSamplesUp);
        (void)handled;
      }
      return true;
    }
    using Tab=EditorScreenState::ImportTab;
    // Só o que muda a SAÍDA do importador pede nova preparação: desmarcar um nó
    // na Estrutura não relê o arquivo.
    const bool profileApplied=resources::sameImportPreparation(importProfileDraft(),importProfilePrepared());
    bool handled=true;
    if(is(EditorWidget::ImportTabSummary)) {state_.importTab=Tab::Summary;state_.importPage=0;}
    else if(is(EditorWidget::ImportTabStructure)) {state_.importTab=Tab::Structure;state_.importPage=0;}
    else if(is(EditorWidget::ImportTabMeshes)) {state_.importTab=Tab::Meshes;state_.importPage=0;state_.importMeshDetail=false;}
    else if(is(EditorWidget::ImportTabTextures)) {state_.importTab=Tab::Textures;state_.importPage=0;}
    else if(is(EditorWidget::ImportTabProfile)) {state_.importTab=Tab::Profile;state_.importPage=0;}
    else if(is(EditorWidget::ImportMeshClose)) state_.importMeshDetail=false;
    else if(routing.widgetId>=widgetId(EditorWidget::ImportMeshRowBase) &&
            routing.widgetId<widgetId(EditorWidget::ImportMeshRowBase)+state_.importMeshes.size()) {
      state_.importMeshSelected=routing.widgetId-widgetId(EditorWidget::ImportMeshRowBase);
      state_.importMeshDetail=true;
    }
    else if(is(EditorWidget::ImportPreviousPage)) {if(state_.importPage) --state_.importPage;}
    else if(is(EditorWidget::ImportNextPage)) ++state_.importPage;
    else if(is(EditorWidget::ImportCancel)) {state_.importCancel=!state_.importError;closeImportPreview();}
    else if(is(EditorWidget::ImportMatchInOrder)) state_.importAmbiguityChoice=1;
    else if(is(EditorWidget::ImportTreatAsNew)) state_.importAmbiguityChoice=2;
    else if(is(EditorWidget::ImportScaleDown) || is(EditorWidget::ImportScaleUp)) {
      auto step=resources::nearestImportScaleStep(state_.importScale);
      if(is(EditorWidget::ImportScaleDown) && step>0) --step;
      if(is(EditorWidget::ImportScaleUp) && step+1<resources::ImportScaleSteps.size()) ++step;
      state_.importScale=resources::ImportScaleSteps[step];
    }
    else if(is(EditorWidget::ImportTextureDimension256)) state_.importTextureDimension=256;
    else if(is(EditorWidget::ImportTextureDimension512)) state_.importTextureDimension=512;
    else if(is(EditorWidget::ImportTextureDimension1024)) state_.importTextureDimension=1024;
    else if(is(EditorWidget::ImportTextureDimension2048)) state_.importTextureDimension=2048;
    else if(is(EditorWidget::ImportNormalsCycle))
      state_.importNormals=state_.importNormals==resources::GltfNormalsImport?resources::GltfNormalsCalculate:resources::GltfNormalsImport;
    else if(is(EditorWidget::ImportNormalWeightingCycle))
      state_.importNormalWeighting=state_.importNormalWeighting==resources::GltfNormalWeightArea?
          resources::GltfNormalWeightAngle:resources::GltfNormalWeightArea;
    else if(is(EditorWidget::ImportTangentsCycle))
      state_.importTangents=state_.importTangents==resources::GltfTangentsImport?resources::GltfTangentsCalculate:resources::GltfTangentsImport;
    else if(is(EditorWidget::ImportCamerasToggle)) state_.importCameras=!state_.importCameras;
    else if(is(EditorWidget::ImportLightsToggle)) state_.importLights=!state_.importLights;
    else if(is(EditorWidget::ImportTextureCompressionCycle)) {
      // 6x6 -> 4x4 -> 8x8 -> sem -> 6x6: começa no padrão e vai para qualidade, economia, desligado.
      const u8 value=state_.importTextureCompression;
      state_.importTextureCompression=value==6?4:value==4?8:value==8?0:6;
    }
    else if(is(EditorWidget::ImportTextureStreamingToggle)) state_.importTextureStreaming=!state_.importTextureStreaming;
    else if(is(EditorWidget::ImportGenerateLodsToggle)) state_.importGenerateLods=!state_.importGenerateLods;
    else if(is(EditorWidget::ImportLodLevelsCycle) && state_.importGenerateLods)
      state_.importLodLevels=static_cast<u8>(state_.importLodLevels>=4?2:state_.importLodLevels+1);
    else if(is(EditorWidget::ImportOptimizeOrderToggle)) state_.importOptimizeOrder=!state_.importOptimizeOrder;
    else if(is(EditorWidget::ImportTextureStreamingPriorityCycle))
      state_.importTextureStreamingPriority=nextStreamingPriority(state_.importTextureStreamingPriority);
    // Passos de 15°: o controle deslizante da Unity em toque, sem arrasto fino.
    else if(is(EditorWidget::ImportSmoothingDown)) state_.importSmoothingAngle=state_.importSmoothingAngle>=15?state_.importSmoothingAngle-15:0;
    else if(is(EditorWidget::ImportSmoothingUp)) state_.importSmoothingAngle=std::min(180u,state_.importSmoothingAngle+15);
    else if(routing.widgetId>=widgetId(EditorWidget::ImportNodeToggleBase) &&
            routing.widgetId<widgetId(EditorWidget::ImportNodeToggleBase)+65536)
      toggleImportNodeExclusion(routing.widgetId-widgetId(EditorWidget::ImportNodeToggleBase));
    else if(is(EditorWidget::ImportApplyProfile)) {
      if(state_.importReady && !profileApplied) {
        state_.importReprepare=true;state_.importReady=false;state_.importStatus="Preparando com o perfil…";
      }
    }
    else if(is(EditorWidget::ImportSaveDefaultProfile))
      setImportStatus(saveProjectImportProfile(importProfileDraft())?"Perfil salvo como padrão do projeto":
                      "Não foi possível salvar o perfil padrão do projeto",
                      EditorConsoleSeverity::Info);
    else if(is(EditorWidget::ImportAccept) || is(EditorWidget::ImportIntoScene)) {
      // Publicar só com a escolha explícita das ambiguidades e com a prévia do
      // perfil que está na tela.
      if(state_.importReady && profileApplied && (!state_.importAmbiguities || state_.importAmbiguityChoice)) {
        state_.importIntoScene=is(EditorWidget::ImportIntoScene);
        state_.importAccept=true;state_.importReady=false;state_.importStatus="Publicando recurso…";
      }
    }
    else handled=false;
    if(handled) return true;
  }
  // O console responde PRIMEIRO.
  //
  // Ele e desenhado por cima do editor de codigo e da cena, e a cadeia abaixo
  // tem varios blocos que consomem o toque por workspace. Um painel que aparece
  // em cima e decidido embaixo e um painel que so funciona por acidente.
  if(state_.console) {
    const auto widget=routing.widgetId;
    if(widget==widgetId(EditorWidget::ConsoleResize) && routing.dragging) {
      state_.consoleCollapsed=false;state_.consoleExpanded=false;
      state_.consoleFraction=std::clamp(state_.consoleFraction-routing.stepDelta.y/std::max(1.0f,layout_.consolePanel.height/state_.consoleFraction),.25f,.82f);
      return true;
    }
    if(event.phase==UiPointerPhase::Down) state_.consoleDragRemainder=0;
    if(routing.dragging && widget>=widgetId(EditorWidget::ConsoleRowBase) &&
        widget-widgetId(EditorWidget::ConsoleRowBase)<EditorConsole::Capacity) {
      // Rolar anda a partir do FIM: zero e a linha mais nova.
      const auto rows=layout_.consoleRowCount,visible=layout_.consoleVisibleRows;
      const auto maximum=rows>visible?rows-visible:0u;
      state_.consoleDragRemainder+=routing.stepDelta.y;
      const int steps=static_cast<int>(state_.consoleDragRemainder/56);
      if(!steps) return true;
      state_.consoleDragRemainder-=steps*56;
      const int next=static_cast<int>(state_.consoleScroll)+steps;
      state_.consoleScroll=static_cast<u32>(std::clamp(next,0,static_cast<int>(maximum)));
      state_.consoleFollow=state_.consoleScroll==0;
      const auto order=console_.filtered(state_.consoleProblems?1:0,state_.consoleOrigin,state_.consoleQuery);
      const auto first=maximum>state_.consoleScroll?maximum-state_.consoleScroll:0;
      if(first<order.size()) state_.consoleAnchor=console_.at(order[first])->eventId;
      return true;
    }
    if(routing.tapped) {
      if(widget==widgetId(EditorWidget::ConsoleInfo)) {console_.toggle(EditorConsoleSeverity::Info);return true;}
      if(widget==widgetId(EditorWidget::ConsoleWarning)) {console_.toggle(EditorConsoleSeverity::Warning);return true;}
      if(widget==widgetId(EditorWidget::ConsoleError)) {console_.toggle(EditorConsoleSeverity::Error);return true;}
      if(widget==widgetId(EditorWidget::ConsoleClear)) {console_.clearLogs();state_.consoleScroll=0;return true;}
      if(widget==widgetId(EditorWidget::ConsoleCollapse)) {state_.consoleCollapsed=!state_.consoleCollapsed;return true;}
      if(widget==widgetId(EditorWidget::ConsoleExpand)) {state_.consoleExpanded=!state_.consoleExpanded;return true;}
      if(widget==widgetId(EditorWidget::ConsoleProblems) || widget==widgetId(EditorWidget::ConsoleLogs)) {
        state_.consoleProblems=widget==widgetId(EditorWidget::ConsoleProblems);state_.consoleOrigin=-1;
        state_.consoleScroll=0;state_.consoleAnchor=0;state_.consoleSelected=0;return true;
      }
      if(widget==widgetId(EditorWidget::ConsoleSource)) {
        state_.consoleOrigin=state_.consoleOrigin==3?-1:state_.consoleOrigin+1;
        state_.consoleScroll=0;state_.consoleAnchor=0;return true;
      }
      if(widget==widgetId(EditorWidget::ConsoleSearch)) {state_.searchingConsole=true;return true;}
      if(widget==widgetId(EditorWidget::ConsoleFollow)) {
        state_.consoleFollow=!state_.consoleFollow;
        if(state_.consoleFollow) {state_.consoleScroll=0;state_.consoleAnchor=0;}
        else {
          const auto order=console_.filtered(state_.consoleProblems?1:0,state_.consoleOrigin,state_.consoleQuery);
          const auto first=order.size()>layout_.consoleVisibleRows?order.size()-layout_.consoleVisibleRows:0;
          if(first<order.size()) state_.consoleAnchor=console_.at(order[first])->eventId;
        }
        return true;
      }
      if(widget==widgetId(EditorWidget::ConsoleDetailClose)) {state_.consoleSelected=0;return true;}
      if(widget==widgetId(EditorWidget::ConsoleDetailPrevious)) {
        if(state_.consoleDetailPage) --state_.consoleDetailPage;
        return true;
      }
      if(widget==widgetId(EditorWidget::ConsoleDetailNext)) {++state_.consoleDetailPage;return true;}
      if(widget==widgetId(EditorWidget::ConsoleCopy)) {
        if(const auto *entry=console_.find(state_.consoleSelected)) consoleCopy_=EditorConsole::describe(*entry);
        return true;
      }
      if(widget==widgetId(EditorWidget::ConsoleOpenSource)) {
        for(u32 i=0;i<console_.entries().size();++i) if(console_.at(i)->eventId==state_.consoleSelected) {
          jumpToConsoleEntry(i);state_.consoleExpanded=false;break;
        }
        return true;
      }
      if(widget==widgetId(EditorWidget::ConsoleExport)) {
        std::string text="ASTRA · Console\nTempos relativos ao início desta sessão. Repetições consecutivas agrupadas.\n";
        for(const auto i:console_.filtered(state_.consoleProblems?1:0,state_.consoleOrigin,state_.consoleQuery))
          text+='\n'+EditorConsole::describe(*console_.at(i))+"\n";
        const std::string directory="Logs";
        const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        const auto path=directory+"/console-"+std::to_string(now)+".txt";
        if((files_.exists(directory)||files_.createDirectory(directory)) && files_.createTextFile(path,text)) {
          files_.rebuildTree();state_.selectedFile=path;state_.status="Console exportado: "+path;
          reportProblem(EditorConsoleSeverity::Info,state_.status);
        } else reportProblem(EditorConsoleSeverity::Error,"Não foi possível exportar: "+files_.error());
        return true;
      }
      const auto base=widgetId(EditorWidget::ConsoleRowBase);
      if(widget>=base && widget-base<console_.entries().size()) {
        state_.consoleSelected=console_.at(widget-base)->eventId;state_.consoleDetailPage=0;return true;
      }
    }
  }
  if(state_.workspace==EditorWorkspace::Code && routing.widgetId==widgetId(EditorWidget::CodeBody)) {
    if(routing.dragging) if(auto *buffer=code_.active()) {
      const auto count=static_cast<u32>(std::count(buffer->text.begin(),buffer->text.end(),'\n'));
      const int next=static_cast<int>(buffer->firstLine)-static_cast<int>(routing.stepDelta.y/12);
      buffer->firstLine=std::min(count,static_cast<u32>(std::max(0,next)));
    }
    if(routing.tapped) {
      state_.editingCode=code_.active()!=nullptr;
      if(state_.editingCode) placeCodeCaret(routing.position);
    }
    return true;
  }
  if(routing.dragging && state_.texturePicker) {
    const u32 key=routing.widgetId;
    const bool control=key==widgetId(EditorWidget::TexturePickerScroll) || key==widgetId(EditorWidget::TextureUseInherited) ||
        key==widgetId(EditorWidget::TextureUseNone) || key==widgetId(EditorWidget::TextureSamplingUv) ||
        key==widgetId(EditorWidget::TextureSamplingWrap) || key==widgetId(EditorWidget::TextureSamplingFilter) ||
        key==widgetId(EditorWidget::TextureUvReset) ||
        (key>=widgetId(EditorWidget::TextureUvStepBase) && key<widgetId(EditorWidget::TextureUvStepBase)+10) ||
        (key>=widgetId(EditorWidget::TextureChoiceBase) && key<widgetId(EditorWidget::TextureChoiceBase)+state_.projectTextureNames.size()) ||
        (key>=widgetId(EditorWidget::TextureViewBase) && key<widgetId(EditorWidget::TextureViewBase)+state_.projectTextureNames.size());
    if(control) {
      const u32 scope=previewSecondary_?1u:0u;
      const float limit=std::max(0.f,layout_.texturePickerContent[scope]-layout_.texturePickerWindow[scope]);
      state_.texturePickerScroll=std::clamp(state_.texturePickerScroll-routing.stepDelta.y,0.f,limit);return true;
    }
  }
  // A aba de componentes usa arraste para navegar, não para reordenar por
  // acidente. A ordem continua disponível pelo menu de cada componente.
  if(routing.dragging && state_.inspectorSurface==EditorInspectorSurface::Components &&
     layout_.componentOverviewWindow.contains(routing.start)) {
    const float limit=std::max(0.f,layout_.componentOverviewContent-layout_.componentOverviewWindow.height);
    state_.componentOverviewScroll=std::clamp(state_.componentOverviewScroll-routing.stepDelta.y,0.f,limit);
    return true;
  }
  // Inspector de vários recursos: arrastar em qualquer ponto dele rola.
  if(routing.dragging && state_.multiAsset.items.size()>1 && routing.widgetId>=widgetId(EditorWidget::MultiAssetApply) &&
     routing.widgetId<widgetId(EditorWidget::FilesMultiToggle)+detail::kRange) {
    const float limit=std::max(0.0f,layout_.multiAssetContent-layout_.multiAssetWindow);
    state_.multiAssetScroll=std::clamp(state_.multiAssetScroll-routing.stepDelta.y,0.0f,limit);
    return true;
  }
  // Inspector de textura: arrastar sobre os cartões (fundo ou controle) rola a
  // janela entre o cabeçalho e o rodapé; o toque curto continua sendo toque.
  if(routing.dragging && state_.textureViewer && (state_.textureInspector||state_.selection!=kInvalidEntity)) {
    const u32 key=routing.widgetId;
    const auto within=[&](EditorWidget first,EditorWidget last) {return key>=widgetId(first) && key<=widgetId(last);};
    const bool card=(key>=widgetId(EditorWidget::TextureInspectorBase) && key<widgetId(EditorWidget::TextureInspectorBase)+0x100u &&
                     key!=widgetId(EditorWidget::TextureInspectorBase)+detail::TextureInspectorExpandClose) ||
                    (key>=widgetId(EditorWidget::TextureUserBase) && key<widgetId(EditorWidget::TextureUserBase)+0x10000u) ||
                    within(EditorWidget::TextureViewerChannel,EditorWidget::TextureViewerBackground) ||
                    within(EditorWidget::TextureProfileInterpretation,EditorWidget::TextureProfileStreamingPriority);
    if(card && !state_.textureViewerExpanded) {
      const float limit=std::max(0.0f,layout_.textureInspectorContent-layout_.textureInspectorWindow);
      state_.textureInspectorScroll=std::clamp(state_.textureInspectorScroll-routing.stepDelta.y,0.0f,limit);
      return true;
    }
  }
  if(routing.tapped && !isPlaying()) {
    const auto presetKey=routing.widgetId;
    if(presetKey==widgetId(EditorWidget::PresetOpen)) {
      // Vindo da folha do Add, o painel de presets toma o lugar dela.
      const bool fromAdd=state_.addingComponent;
      if(openComponentPresets(state_.selection,fromAdd?0:state_.nativeMenu) && fromAdd) state_.addingComponent=false;
      return true;
    }
    if(state_.presetPanel && ((presetKey>=widgetId(EditorWidget::PresetClose)&&presetKey<=widgetId(EditorWidget::PresetRecipeNext)) ||
        (presetKey>=widgetId(EditorWidget::PresetChoiceBase)&&presetKey<widgetId(EditorWidget::PresetChoiceBase)+256) ||
        (presetKey>=widgetId(EditorWidget::PresetFieldBase)&&presetKey<widgetId(EditorWidget::PresetFieldBase)+4096) ||
        (presetKey>=widgetId(EditorWidget::PresetInputBase)&&presetKey<widgetId(EditorWidget::PresetInputBase)+256))) {
      if(state_.selection!=state_.presetEntity || state_.presetEpoch!=sceneEpoch_) {state_.presetPanel=false;return true;}
      if(presetKey==widgetId(EditorWidget::PresetClose)) {state_.presetPanel=false;state_.presetNaming=false;return true;}
      if(presetKey==widgetId(EditorWidget::PresetLibrary)) {state_.presetSelected=0;state_.presetInputValues.clear();refreshComponentPresets();return true;}
      if(presetKey==widgetId(EditorWidget::PresetRecipeDetails)) {state_.presetRecipeDetails=!state_.presetRecipeDetails;state_.presetRecipePage=0;return true;}
      if(presetKey==widgetId(EditorWidget::PresetRecipePrevious)) {if(state_.presetRecipePage)--state_.presetRecipePage;return true;}
      if(presetKey==widgetId(EditorWidget::PresetRecipeNext)) {++state_.presetRecipePage;return true;}
      if(presetKey==widgetId(EditorWidget::PresetPrevious)) {if(state_.presetPage) --state_.presetPage;return true;}
      if(presetKey==widgetId(EditorWidget::PresetNext)) {if((state_.presetPage+1)*4<state_.presetChoices.size()) ++state_.presetPage;return true;}
      if(presetKey>=widgetId(EditorWidget::PresetInputBase)&&presetKey<widgetId(EditorWidget::PresetInputBase)+256) {
        openRecipeInput(presetKey-widgetId(EditorWidget::PresetInputBase));return true;
      }
      // Escolha por campo: marcar, desmarcar e paginar o diff não tocam a cena.
      if(presetKey>=widgetId(EditorWidget::PresetFieldBase)) {
        const auto index=presetKey-widgetId(EditorWidget::PresetFieldBase);
        if(index<state_.presetFields.size()&&state_.presetFields[index].applicable)
          state_.presetFields[index].selected=!state_.presetFields[index].selected;
        return true;
      }
      if(presetKey==widgetId(EditorWidget::PresetSelectAll)||presetKey==widgetId(EditorWidget::PresetSelectNone)) {
        const bool value=presetKey==widgetId(EditorWidget::PresetSelectAll);
        for(auto &field:state_.presetFields) if(field.applicable) field.selected=value;
        return true;
      }
      if(presetKey==widgetId(EditorWidget::PresetFieldsPrevious)) {if(state_.presetFieldPage) --state_.presetFieldPage;return true;}
      if(presetKey==widgetId(EditorWidget::PresetFieldsNext)) {
        // Quantas linhas cabem por página depende da altura do painel, que é da
        // tela; aqui só se garante que a página nunca passa do número de linhas
        // (uma por página no pior caso). A tela limita a página ao total real.
        if(state_.presetFieldPage+1<state_.presetFields.size()) ++state_.presetFieldPage;
        return true;
      }
      if(presetKey==widgetId(EditorWidget::PresetSave)||presetKey==widgetId(EditorWidget::PresetRename)||
         presetKey==widgetId(EditorWidget::PresetSaveRecipe)) {
        state_.presetRenaming=presetKey==widgetId(EditorWidget::PresetRename);
        state_.presetRecipeNaming=presetKey==widgetId(EditorWidget::PresetSaveRecipe);
        const auto *entry=componentPresets_.find(state_.presetSelected);
        state_.presetName=state_.presetRenaming&&entry?entry->name:document_.find(state_.presetEntity)->name;
        std::snprintf(state_.renameText,sizeof(state_.renameText),"%s",state_.presetName.c_str());
        state_.presetNaming=true;return true;
      }
      if(presetKey>=widgetId(EditorWidget::PresetChoiceBase)) {
        const auto index=presetKey-widgetId(EditorWidget::PresetChoiceBase);
        if(index<state_.presetChoices.size()) {state_.presetSelected=state_.presetChoices[index].first;state_.presetDeleteConfirm=false;state_.presetInputValues.clear();state_.presetRecipeDetails=false;state_.presetRecipePage=0;refreshComponentPresets();}
        return true;
      }
      std::string error;
      if(presetKey==widgetId(EditorWidget::PresetDelete)) {
        if(!state_.presetDeleteConfirm) {state_.presetDeleteConfirm=true;return true;}
        const bool erased=componentPresets_.erase(state_.presetSelected,error);state_.status=erased?"Preset excluído":error;
        if(erased) {state_.presetSelected=0;refreshComponentPresets();}state_.presetDeleteConfirm=false;return true;
      }
      // Uma receita não tem "aplicar valores" e "adicionar" separados: ela
      // resolve os dois de uma vez, componente a componente, na transação.
      const bool applied=state_.presetSelectedIsRecipe?
        applyComponentRecipe(state_.presetSelected,state_.presetEntity,{state_.presetEpoch,state_.presetRevision},state_.presetInputValues,error):
        applyComponentPreset(state_.presetSelected,state_.presetEntity,state_.presetInstance,
          {state_.presetEpoch,state_.presetRevision},presetKey==widgetId(EditorWidget::PresetAdd),error);
      state_.status=error;if(applied) {state_.presetPanel=false;state_.nativeMenu=0;state_.addingComponent=false;}return true;
    }
    const auto impactKey=routing.widgetId;
    if(impactKey==widgetId(EditorWidget::ImpactClose)) {
      if(state_.impactRepair) {state_.impactRepair=false;state_.impactReplacement={};state_.impactRepairMaterial={};state_.impactPage=0;return true;}
      if(!state_.impactTrail.empty()) {
        const auto previous=state_.impactTrail.back();state_.impactTrail.pop_back();
        state_.impactAsset=previous.first;state_.impactPage=previous.second;
      } else {state_.impactInstance=0;state_.impactAsset={};state_.impactRemoval=false;}
      return true;
    }
    if(impactKey==widgetId(EditorWidget::ImpactPrevious)) {if(state_.impactPage)--state_.impactPage;return true;}
    if(impactKey==widgetId(EditorWidget::ImpactNext)) {++state_.impactPage;return true;}
    if(impactKey==widgetId(EditorWidget::ImpactRemoveConfirm)) {
      if(!state_.impactRemoval || state_.impactAsset.valid() || state_.selection!=state_.impactEntity) return true;
      const auto version=EditorSceneVersion{state_.impactRemovalEpoch,state_.impactRemovalRevision};
      if(sceneVersion().epoch!=version.epoch || sceneVersion().revision!=version.revision) {
        state_.status="Cena alterada; reabra a remoção";return true;
      }
      EditorActionRequest request;request.action=EditorAction::RemoveComponent;
      request.entity=state_.impactEntity;request.componentInstance=state_.impactInstance;request.version=version;
      if(dispatch(request).status==EditorActionStatus::Applied) {
        if(state_.expandedNative==request.componentInstance) state_.expandedNative=0;
        if(state_.expandedScript==request.componentInstance) state_.expandedScript=0;
        state_.nativeMenu=0;state_.scriptMenu=0;state_.impactInstance=0;state_.impactAsset={};state_.impactTrail.clear();state_.impactRemoval=false;
        state_.status="Componente removido";
      } else state_.status="Remoção bloqueada; revise as dependências";
      return true;
    }
    if(impactKey==widgetId(EditorWidget::ImpactRepairScope)&&state_.impactRepair&&!state_.impactRepairMaterial.valid()) {
      state_.impactRepairScene=!state_.impactRepairScene;state_.impactPage=0;return true;
    }
    if(impactKey==widgetId(EditorWidget::ImpactRepair)||impactKey==widgetId(EditorWidget::ImpactRepairShared)) {
      state_.impactRepairMaterial=impactKey==widgetId(EditorWidget::ImpactRepairShared)?repairMaterialContext(state_.impactTrail,state_.impactAsset,&mapScene_):resources::AssetGuid{};
      const auto *material=findMaterialAsset(state_.impactRepairMaterial);
      if(impactKey==widgetId(EditorWidget::ImpactRepairShared)&&!material) return true;
      state_.impactRepairMaterialRevision=material?material->revision:0;
      state_.impactRepairScene=false;
      state_.impactRepair=true;state_.impactReplacement={};state_.impactRepairRevision=document_.revision();state_.impactRepairEpoch=sceneEpoch_;state_.impactPage=0;return true;
    }
    if(impactKey==widgetId(EditorWidget::ImpactRepairApply)) {
      std::string diagnostic;
      const auto version=EditorSceneVersion{state_.impactRepairEpoch,state_.impactRepairRevision};
      const bool applied=state_.impactRepairMaterial.valid()?
          repairSharedTexture(state_.impactRepairMaterial,state_.impactAsset,state_.impactReplacement,state_.impactRepairMaterialRevision,version,diagnostic):
          state_.impactRepairScene?repairSceneResource(state_.impactAsset,state_.impactReplacement,version,diagnostic):
          repairComponentResource(state_.impactEntity,state_.impactInstance,state_.impactAsset,state_.impactReplacement,version,diagnostic);
      if(applied) {
        state_.impactAsset=state_.impactReplacement;state_.impactRepair=false;state_.impactReplacement={};state_.impactRepairMaterial={};state_.impactPage=0;
      }
      state_.status=diagnostic;return true;
    }
    if(impactKey>=widgetId(EditorWidget::ImpactOpenBase)&&impactKey<widgetId(EditorWidget::ImpactOpenBase)+0x01000000u) {
      const auto *entity=document_.find(state_.selection);const auto index=impactKey-widgetId(EditorWidget::ImpactOpenBase);
      if(entity&&index<entity->components.size()) {state_.impactEntity=entity->id;state_.impactInstance=entity->components.at(index)->instanceId();state_.impactPage=0;state_.impactAsset={};state_.impactTrail.clear();state_.impactRepair=false;state_.impactRemoval=false;state_.impactReplacement={};state_.impactRepairMaterial={};}
      return true;
    }
    const bool removeNative=impactKey>=widgetId(EditorWidget::ComponentRemoveBase)&&impactKey<widgetId(EditorWidget::ComponentRemoveBase)+0x01000000u;
    const bool removeScript=impactKey>=widgetId(EditorWidget::ScriptRemoveBase)&&impactKey<widgetId(EditorWidget::ScriptRemoveBase)+0x01000000u;
    if(removeNative || removeScript) {
      const auto *entity=document_.find(state_.selection);
      const auto index=impactKey-widgetId(removeNative?EditorWidget::ComponentRemoveBase:EditorWidget::ScriptRemoveBase);
      const auto *component=entity?entity->components.at(index):nullptr;
      if(component && (removeNative || scene::scriptBehavior(component))) {
        state_.impactEntity=entity->id;state_.impactInstance=component->instanceId();
        const auto version=sceneVersion();state_.impactRemovalEpoch=version.epoch;state_.impactRemovalRevision=version.revision;
        state_.impactPage=0;state_.impactAsset={};state_.impactTrail.clear();state_.impactRepair=false;
        state_.impactRemoval=true;state_.impactReplacement={};state_.impactRepairMaterial={};state_.nativeMenu=0;state_.scriptMenu=0;
      }
      return true;
    }
    if(impactKey>=widgetId(EditorWidget::ImpactRowBase)&&impactKey<widgetId(EditorWidget::ImpactRowBase)+0x01000000u) {
      if(state_.impactRepair) {
        if(state_.impactReplacement.valid()) return true;
        const auto *object=document_.find(state_.impactEntity);
        const auto shared=sharedTextureBindings(state_.impactRepairMaterial,&mapScene_);
        const auto choices=resourceRepairChoices(state_.impactRepairMaterial.valid()?&shared:object?object->components.findInstance(state_.impactInstance):nullptr,state_.impactAsset,&assets_,&mapScene_);
        const auto index=impactKey-widgetId(EditorWidget::ImpactRowBase);
        if(index<choices.size()) {state_.impactReplacement=choices[index].asset;state_.impactPage=0;}
        return true;
      }
      const auto entries=state_.impactAsset.valid()?resourceImpact(document_,state_.impactAsset,&assets_,&mapScene_):
          componentImpact(document_,state_.impactEntity,state_.impactInstance,&assets_,&mapScene_);const auto index=impactKey-widgetId(EditorWidget::ImpactRowBase);
      if(index<entries.size()&&entries[index].asset.valid()) {
        if(state_.impactTrail.size()>=64) {state_.status="Volte para abrir outro recurso";return true;}
        state_.impactTrail.push_back({state_.impactAsset,state_.impactPage});state_.impactAsset=entries[index].asset;state_.impactPage=0;
        return true;
      }
      if(index<entries.size()&&document_.find(entries[index].object)) {
        setSelection(entries[index].object);state_.componentSelection=entries[index].object;
        state_.expandedNative=entries[index].instance;state_.nativeMenu=0;state_.propertyPage=0;state_.impactInstance=0;state_.impactAsset={};state_.impactTrail.clear();state_.impactRepair=false;state_.impactReplacement={};state_.impactRepairMaterial={};
      }
      return true;
    }

    if(routing.widgetId==widgetId(EditorWidget::CameraPreviewPin)) {
      cameraPreview_.pin(document_,sceneVersion(),state_.selection);return true;
    }
    if(routing.widgetId==widgetId(EditorWidget::CameraPreviewClose)) {cameraPreview_.close();return true;}
    if(routing.widgetId==widgetId(EditorWidget::CameraPreviewRetry)) {cameraPreview_.invalidateTarget();return true;}
    if(routing.widgetId==widgetId(EditorWidget::CameraPreviewResolution) ||
       routing.widgetId==widgetId(EditorWidget::CameraPreviewFrequency)) {
      auto width=cameraPreview_.width();auto height=cameraPreview_.height();
      float frequency=cameraPreview_.frequency();
      if(routing.widgetId==widgetId(EditorWidget::CameraPreviewResolution)) {
        width=width<640?640:width<960?960:320;height=width*9/16;
      } else frequency=frequency<15?15:frequency<30?30:5;
      renderer::PreviewViewBudget budget;
      budget.maximumWidth=width;budget.maximumHeight=height;
      budget.maximumPixels=static_cast<u64>(width)*height;budget.updatesPerSecond=frequency;
      cameraPreview_.configure(width,height,budget);return true;
    }
    // R4: textura em Propriedades e gerenciador de texturas.
    {
      const auto key=routing.widgetId;
      if(key==widgetId(EditorWidget::TextureManagerClose)) {state_.textureManager=false;return true;}
      if(key==widgetId(EditorWidget::TextureSearch) && state_.textureManager) {state_.searchingTextures=true;return true;}
      if(key>=widgetId(EditorWidget::TextureFilterBase) && key<widgetId(EditorWidget::TextureFilterBase)+TextureFilterCount) {
        setTextureFilter(static_cast<u8>(key-widgetId(EditorWidget::TextureFilterBase)));return true;
      }
      if(key==widgetId(EditorWidget::TextureManagerPrevious)) {if(state_.textureManagerPage) --state_.textureManagerPage;return true;}
      if(key==widgetId(EditorWidget::TextureManagerNext)) {++state_.textureManagerPage;return true;}
      // Bloco F: texturas das fontes no mesmo gerenciador.
      if(key==widgetId(EditorWidget::TextureManagerShowProject) || key==widgetId(EditorWidget::TextureManagerShowSources)) {
        state_.textureManagerSources=key==widgetId(EditorWidget::TextureManagerShowSources);
        state_.textureManagerPage=0;return true;
      }
      if(key>=widgetId(EditorWidget::TextureManagerRowBase)+detail::TextureManagerSourceRowOffset &&
         key-widgetId(EditorWidget::TextureManagerRowBase)-detail::TextureManagerSourceRowOffset<sourceTextures_.size()) {
        if(!openSourceTextureInspector(key-widgetId(EditorWidget::TextureManagerRowBase)-detail::TextureManagerSourceRowOffset))
          state_.status=state_.textureViewerInfo;
        return true;
      }
      if(key==widgetId(EditorWidget::TextureSourceShowOrigin) && state_.textureViewerSource &&
         state_.textureViewerSourceIndex<sourceTextures_.size()) {
        // A fonte é onde ficam perfil (compressão, LOD, streaming) e Reimportar.
        const auto path=sourceTextures_[state_.textureViewerSourceIndex].sourcePath;
        closeTextureViewer();state_.textureInspector=false;state_.textureManager=false;
        state_.selectedFile=path;
        state_.status="Fonte selecionada em Arquivos · Reimportar abre o perfil de importação";
        return true;
      }
      if(key==widgetId(EditorWidget::TextureSourceShowAll)) {
        const bool fromSource=state_.textureViewerSource;
        closeTextureViewer();
        openTextureManager();
        state_.textureManagerSources=fromSource;
        return true;
      }
      // Inspector de textura em cartões.
      if(key>=widgetId(EditorWidget::TextureInspectorBase) && key<widgetId(EditorWidget::TextureInspectorBase)+0x100u && state_.textureViewer) {
        const u32 sub=key-widgetId(EditorWidget::TextureInspectorBase);
        if(sub<detail::TextureInspectorCopy) {state_.textureCardsClosed^=1u<<sub;return true;}
        if(sub<detail::TextureInspectorCrumb) {
          const u32 row=sub-detail::TextureInspectorCopy;
          if(row<state_.textureOrigin.size() && !state_.textureOrigin[row].copy.empty()) {
            consoleCopy_=state_.textureOrigin[row].copy;
            state_.status=state_.textureOrigin[row].label+" copiado: "+consoleCopy_;
          }
          return true;
        }
        if(sub<detail::TextureInspectorScroll) {
          const u32 depth=sub-detail::TextureInspectorCrumb;
          std::string folder;
          for(u32 i=0;i<=depth && i<state_.textureBreadcrumb.size();++i) folder+=(i?"/":"")+state_.textureBreadcrumb[i];
          if(!folder.empty() && revealInFiles(folder)) state_.status="Pasta aberta em Arquivos: "+folder;
          return true;
        }
        if(sub==detail::TextureInspectorExpand) {state_.textureViewerExpanded=!state_.textureViewerImage.isEmpty();return true;}
        if(sub==detail::TextureInspectorExpandClose) {state_.textureViewerExpanded=false;return true;}
        if(sub==detail::TextureInspectorUsersMore) {state_.textureUsersExpanded=!state_.textureUsersExpanded;return true;}
        if(sub==detail::TextureInspectorGpuInfo) {state_.status=state_.textureGpuNote;return true;}
        if(sub==detail::TextureInspectorLocate) {
          // A imagem em Arquivos (Ping da Unity): pastas abertas até ela e linha à vista.
          const std::string file=state_.textureOrigin.size()>1?state_.textureOrigin[1].copy:std::string();
          if(!file.empty() && revealInFiles(file)) state_.status="Localizado em Arquivos: "+file;
          else state_.status="Esta textura vem de dentro do modelo; não há arquivo próprio para localizar.";
          return true;
        }
        if(sub==detail::TextureInspectorReimport) {
          if(state_.textureViewerSource && state_.textureViewerSourceIndex<sourceTextures_.size()) {
            reimportPath_=sourceTextures_[state_.textureViewerSourceIndex].sourcePath;
            state_.status="Reimportando a fonte "+baseName(reimportPath_)+" com o perfil dela";
          } else if(!state_.textureViewerSource && state_.textureViewerIndex<textures_.size()) {
            textureReimportPath_=textures_[state_.textureViewerIndex].path;
            state_.status="Reimportando "+textures_[state_.textureViewerIndex].name;
          }
          return true;
        }
        return true;
      }
      if(key>=widgetId(EditorWidget::TextureManagerRowBase) && key-widgetId(EditorWidget::TextureManagerRowBase)<textures_.size()) {
        openTextureInspector(key-widgetId(EditorWidget::TextureManagerRowBase));return true;
      }
      if(key>=widgetId(EditorWidget::TextureUserBase) && key-widgetId(EditorWidget::TextureUserBase)<state_.textureUserEntities.size()) {
        const auto id=state_.textureUserEntities[key-widgetId(EditorWidget::TextureUserBase)];
        if(id!=kInvalidEntity && document_.find(id)) {
          state_.textureInspector=false;state_.textureViewer=false;
          setSelection(id);
          state_.status="Objeto que usa a textura selecionado";
        }
        return true;
      }
    }
    if(state_.componentSelection!=state_.selection) {
      state_.componentSelection=state_.selection;state_.referenceInstance=0;state_.expandedComponent.clear();state_.expandedNative=0;state_.nativeMenu=0;
      state_.expandedScript=0;state_.scriptMenu=0;state_.meshPicker=false;state_.resourceInstance=0;state_.resourceProperty.clear();
      state_.componentPage=0;state_.componentPreview=0;state_.scriptPreviewType.clear();state_.propertyPage=0;state_.propertyQuery.clear();state_.addingComponent=false;
    }
    const u32 key=routing.widgetId;
    if(key==widgetId(EditorWidget::CameraViewClose)) {cancelPointers();state_.cameraViewEntity=0;state_.cameraPiloting=false;return true;}
    if(key==widgetId(EditorWidget::CameraView) || key==widgetId(EditorWidget::CameraPilot)) {
      cancelPointers();
      const auto pose=resolveSceneCamera(document_,state_.selection,true);
      if(pose.entity) {state_.cameraViewEntity=pose.entity;state_.cameraPiloting=key==widgetId(EditorWidget::CameraPilot);}
      else state_.status="A câmera precisa de uma transformação válida";
      return true;
    }
    if(key==widgetId(EditorWidget::CameraAlignView)) {
      const auto *entity=document_.find(state_.selection);
      if(!entity || !cameraComponent(*entity) || history_.isOpen()) return true;
      if(!isViewportValid(view_)) {state_.status="A vista precisa de uma projeção válida";return true;}
      float world[16]{},parent[16];editorTransformMatrix(EditorTransform{},world);editorTransformMatrix(EditorTransform{},parent);
      const auto basis=renderer::buildCameraViewBasis(view_.frustum.yaw,view_.frustum.pitch,view_.frustum.roll);
      for(u32 k=0;k<3;++k) {world[k]=basis.row0[k];world[4+k]=basis.row1[k];world[8+k]=basis.row2[k];world[12+k]=view_.frustum.cameraPosition[k];}
      EditorTransform pose;
      if((!entity->parent || editorWorldMatrix(document_,entity->parent,parent)) && editorLocalTransformForWorld(world,parent,pose)) {
        std::copy(entity->transform.scale,entity->transform.scale+3,pose.scale);
        if(history_.setTransform(document_,entity->id,pose)) state_.status="Câmera alinhada à vista";
      } else state_.status="A hierarquia não permite alinhar esta pose sem shear";
      return true;
    }
    if(key==widgetId(EditorWidget::ReferenceClose)) {state_.referenceInstance=0;state_.presetInputIndex=0;state_.presetInputProperty=nullptr;return true;}
    if(key==widgetId(EditorWidget::ReferencePrevious)) {if(state_.referencePage) --state_.referencePage;return true;}
    if(key==widgetId(EditorWidget::ReferenceNext)) {++state_.referencePage;return true;}
    if(key==widgetId(EditorWidget::ReferenceModeToggle)) {
      state_.pickerAdvanced=!state_.pickerAdvanced;state_.referenceHighlight=0;state_.referencePage=0;
      saveEditorPreferences();return true;
    }
    if(key==widgetId(EditorWidget::ReferenceTypeFilter)) {
      state_.referenceTypeFilter=!state_.referenceTypeFilter;state_.referencePage=0;return true;
    }
    if(key>=widgetId(EditorWidget::ReferenceViewBase) && key<widgetId(EditorWidget::ReferenceViewBase)+3) {
      state_.pickerView=static_cast<u8>(key-widgetId(EditorWidget::ReferenceViewBase));state_.referencePage=0;return true;
    }
    if(key==widgetId(EditorWidget::ReferenceSearch)) {
      state_.editingReferenceSearch=true;std::snprintf(state_.renameText,sizeof(state_.renameText),"%s",state_.referenceQuery.c_str());return true;
    }
    if(key>=widgetId(EditorWidget::ComponentReferenceBase)&&key<widgetId(EditorWidget::ComponentReferenceBase)+0x01000000u) {
      const auto *entity=document_.find(state_.selection);if(!entity) return true;
      const auto *component=entity->components.at(key&0xffu);const auto field=(key&0x00ffffffu)>>8;
      if(!component||field>=component->type().references.size()) return true;
      state_.referenceInstance=component->instanceId();state_.referenceProperty=component->type().references[field].id;
      state_.referenceScript=false;state_.referenceQuery.clear();state_.referencePage=0;return true;
    }
    if(key==widgetId(EditorWidget::ReferenceClear)||(key>=widgetId(EditorWidget::ReferenceChoiceBase)&&key<widgetId(EditorWidget::ReferenceChoiceBase)+0x01000000u)||
       key==widgetId(EditorWidget::ReferenceAssign)||(key>=widgetId(EditorWidget::ReferenceResultBase)&&key<widgetId(EditorWidget::ReferenceResultBase)+0x10000u)) {
      const auto *entity=document_.find(state_.presetInputIndex?state_.presetEntity:state_.selection);if(!entity||!state_.referenceInstance) return true;
      const auto scriptReference=editorScriptReference(state_.referenceScriptType);
      const auto *property=state_.presetInputIndex?state_.presetInputProperty:state_.referenceScript?&scriptReference:editorReferenceProperty(*entity,state_.referenceInstance,state_.referenceProperty);
      if(!property) {state_.referenceInstance=0;return true;}
      u64 target=0;
      if(key==widgetId(EditorWidget::ReferenceAssign) ||
         (key>=widgetId(EditorWidget::ReferenceResultBase)&&key<widgetId(EditorWidget::ReferenceResultBase)+0x10000u)) {
        // Avançado: o primeiro toque destaca (painel de inspeção); o segundo
        // toque ou "Escolher" atribui — só quando o campo aceita o objeto.
        const auto results=state_.presetInputIndex?editorRecipeReferenceResults(document_,entity->id,state_.presetInputChoices,state_.referenceQuery,state_.referenceTypeFilter):editorAdvancedReferenceResults(document_,entity->id,*property,state_.referenceQuery,state_.referenceTypeFilter);
        const EditorReferenceResult *chosen=nullptr;
        if(key==widgetId(EditorWidget::ReferenceAssign)) {
          for(const auto &result:results) if(result.id==state_.referenceHighlight) chosen=&result;
        } else {
          const u32 index=key-widgetId(EditorWidget::ReferenceResultBase);
          if(index>=results.size()) return true;
          if(results[index].id!=state_.referenceHighlight) {state_.referenceHighlight=results[index].id;return true;}
          chosen=&results[index];
        }
        if(!chosen) return true;
        if(!chosen->compatible) {state_.status="O campo recusa este objeto";return true;}
        target=chosen->id;state_.referenceHighlight=0;
      } else if(key!=widgetId(EditorWidget::ReferenceClear)) {
        auto choices=state_.presetInputIndex?editorRecipeReferenceChoices(document_,state_.presetInputChoices,state_.referenceQuery):editorReferenceChoices(document_,entity->id,*property,state_.referenceQuery);
        const auto index=key-widgetId(EditorWidget::ReferenceChoiceBase);
        if(index>=choices.size()) return true;
        target=choices[index];
      }
      if(state_.presetInputIndex) {
        const auto index=state_.presetInputIndex-1;
        if(state_.presetEpoch!=sceneEpoch_ || state_.presetRevision!=document_.revision() || index>=state_.presetInputValues.size() ||
           (target && std::find(state_.presetInputChoices.begin(),state_.presetInputChoices.end(),target)==state_.presetInputChoices.end())) {
          state_.status="Entrada de receita incompatível ou cena alterada";return true;
        }
        state_.presetInputValues[index]=target;state_.referenceInstance=0;state_.presetInputIndex=0;state_.presetInputProperty=nullptr;
        refreshComponentPresets();state_.status="Entrada da receita preparada; cena preservada";return true;
      }
      // Como na Unity, pôr objetos no LOD 0 recalcula os limites do grupo. Vai
      // num comando só com a referência: um Desfazer volta os dois.
      if(!state_.referenceScript && state_.referenceProperty=="level_0" && !history_.isOpen() &&
         &entity->components.findInstance(state_.referenceInstance)->type()==&scene::LodGroup::descriptor &&
         editorReferenceAccepts(document_,entity->id,*property,target)) {
        auto value=*entity;
        auto *group=static_cast<scene::LodGroup *>(value.components.editInstance(state_.referenceInstance));
        if(group) {
          group->levels[0]=target;
          const bool measured=target && fitLodGroupSize(mapScene_,document_,entity->id,*group);
          if(group->valid() && history_.applyValues(document_,entity->id,value)) {
            state_.referenceInstance=0;
            state_.status=measured?"LOD 0 atribuído; tamanho medido pela malha":"Referência atualizada";
            return true;
          }
        }
      }
      EditorActionRequest request;request.version=sceneVersion();request.entity=entity->id;request.componentInstance=state_.referenceInstance;request.componentProperty=state_.referenceProperty;
      if(state_.referenceScript && state_.referenceScriptElement) {
        // Elemento de uma lista de referências: troca só aquele elemento.
        const auto *script=scene::scriptBehavior(entity->components.findInstance(state_.referenceInstance));
        const auto *declared=script?scriptProperty(script->scriptType,state_.referenceProperty):nullptr;
        const auto value=editorScriptReferenceValue(document_,state_.referenceScriptType,target);
        auto items=script?scriptArrayItems(*script,state_.referenceProperty):std::vector<std::string>{};
        if(!declared || value.empty() || state_.referenceScriptElement>items.size()) {state_.status="A referência não é compatível com este campo";return true;}
        items[state_.referenceScriptElement-1]=value;
        if(setScriptArray(entity->id,state_.referenceInstance,state_.referenceProperty,declared->valueType,items)) {
          state_.referenceInstance=0;state_.referenceScriptElement=0;state_.status="Referência atualizada";
        }
        return true;
      }
      if(state_.referenceScript) {
        request.action=EditorAction::ScriptProperty;request.scriptPropertyType=state_.referenceScriptType;
        request.scriptPropertyValue=editorScriptReferenceValue(document_,state_.referenceScriptType,target);
      }
      else {request.action=EditorAction::ComponentProperty;request.componentType=entity->components.findInstance(state_.referenceInstance)->type().id;request.componentValue=scene::ObjectReference{target};}
      if(dispatch(request).status==EditorActionStatus::Applied) {state_.referenceInstance=0;state_.status="Referência atualizada";}
      else state_.status="A referência não é compatível com este campo";
      return true;
    }
    if(key>=widgetId(EditorWidget::CameraLens) && key<=widgetId(EditorWidget::CameraFar)) {
      const auto *entity=document_.find(state_.cameraViewEntity);
      const auto *component=entity?cameraComponent(*entity):nullptr;
      if(!component || history_.isOpen()) return true;
      const u32 index=key==widgetId(EditorWidget::CameraLens)?
        (component->projection==scene::CameraProjection::Orthographic?4u:0u):
        (key==widgetId(EditorWidget::CameraNear)?1u:2u);
      const auto &property=component->type().numbers[index];
      state_.numericField=key;state_.numericEntity=entity->id;
      state_.numericInstance=component->instanceId();state_.numericProperty=property.id;
      std::snprintf(state_.numericText,sizeof(state_.numericText),"%.9g",static_cast<double>(property.read(*component)));
      state_.numericReplace=true;state_.numericError=false;return true;
    }
    if(key>=widgetId(EditorWidget::ComponentColorBase)&&key<widgetId(EditorWidget::ComponentColorBase)+0x01000000u) {
      const auto *entity=document_.find(state_.selection);const u32 index=key&0xffu,field=(key&0x00ffffffu)>>8;
      if(!entity||index>=entity->components.size()||history_.isOpen()) return true;
      const auto *component=entity->components.at(index);
      if(field>=component->type().triples.size()) return true;
      const auto &triple=component->type().triples[field];if(triple.kind!=scene::ComponentTripleKind::LinearColor) return true;
      float rgb[3]{};
      for(u32 axis=0;axis<3;++axis) {
        bool found=false;
        for(const auto &p:component->type().numbers) if(p.id==triple.channels[axis]) {
          if(!p.presentation.isEditable(*component)) return true;
          rgb[axis]=p.read(*component);found=true;
        }
        if(!found) return true;
      }
      state_.colorEntity=entity->id;state_.colorInstance=component->instanceId();
      state_.colorProperty=triple.id;state_.colorTarget=0;
      openColorWindow(key,{rgb[0],rgb[1],rgb[2],1},false,false);return true;
    }
    if(key>=widgetId(EditorWidget::ComponentTripleBase) && key<widgetId(EditorWidget::ComponentTripleBase)+0x01000000u) {
      const auto *entity=document_.find(state_.selection);const u32 type=key&0xffu,field=(key&0x00ffffffu)>>8;
      if(!entity||type>=entity->components.size()||history_.isOpen()) return true;
      const auto *component=entity->components.at(type);
      if(field>=component->type().triples.size()) return true;
      const auto &triple=component->type().triples[field];float values[3]{};
      for(u32 axis=0;axis<triple.dimensions();++axis) {
        bool found=false;
        for(const auto &p:component->type().numbers) if(p.id==triple.channels[axis]) {
          if(!p.presentation.isEditable(*component)) return true;
          values[axis]=p.read(*component);found=true;
        }
        if(!found) return true;
      }
      state_.numericField=key;state_.numericEntity=entity->id;state_.numericInstance=component->instanceId();state_.numericProperty=triple.id;
      if(triple.dimensions()==2)std::snprintf(state_.numericText,sizeof(state_.numericText),"%.9g %.9g",static_cast<double>(values[0]),static_cast<double>(values[1]));
      else std::snprintf(state_.numericText,sizeof(state_.numericText),"%.9g %.9g %.9g",static_cast<double>(values[0]),static_cast<double>(values[1]),static_cast<double>(values[2]));
      state_.numericReplace=true;state_.numericError=false;return true;
    }
    if(key>=widgetId(EditorWidget::ComponentNumberBase) && key<widgetId(EditorWidget::ComponentNumberBase)+0x01000000u) {
      const auto *entity=document_.find(state_.selection);const u32 type=key&0xffu,field=(key&0x00ffffffu)>>8;
      if(!entity || type>=entity->components.size() || history_.isOpen()) return true;
      const auto *component=entity->components.at(type);
      if(!component || field>=component->type().numbers.size()) return true;
      const auto &property=component->type().numbers[field];
      state_.numericField=key;state_.numericEntity=entity->id;state_.numericInstance=component->instanceId();state_.numericProperty=property.id;
      std::snprintf(state_.numericText,sizeof(state_.numericText),"%.9g",static_cast<double>(property.read(*component)));
      state_.numericReplace=true;state_.numericError=false;return true;
    }
    if(key>=widgetId(EditorWidget::ComponentSlotNumberBase) && key<widgetId(EditorWidget::ComponentSlotNumberBase)+0x01000000u) {
      const auto *entity=document_.find(state_.selection);const u32 encoded=key-widgetId(EditorWidget::ComponentSlotNumberBase);
      const u32 type=encoded&0xffu,field=(encoded>>8)&0xffu,slot=(encoded>>16)&0xffu;
      if(!entity||type>=entity->components.size()||history_.isOpen()) return true;
      const auto *component=entity->components.at(type);
      if(!component||field>=component->type().slotNumbers.size()) return true;
      const auto &property=component->type().slotNumbers[field];
      if(!property.read||slot>=property.slotCount(*component)) return true;
      // O endereço viaja no próprio campo; o commit decodifica e confere de novo.
      state_.numericField=key;state_.numericEntity=entity->id;state_.numericInstance=component->instanceId();state_.numericProperty=property.id;
      std::snprintf(state_.numericText,sizeof(state_.numericText),"%.9g",static_cast<double>(property.read(*component,slot)));
      state_.numericReplace=true;state_.numericError=false;return true;
    }
    if(key>=widgetId(EditorWidget::ComponentResourceBase) && key<widgetId(EditorWidget::ComponentResourceBase)+0x01000000u) {
      const auto *entity=document_.find(state_.selection);const u32 encoded=key-widgetId(EditorWidget::ComponentResourceBase);
      const u32 type=encoded&0xffu,bindingIndex=(encoded>>8)&0xffu,slot=(encoded>>16)&0xffu;
      if(!entity||type>=entity->components.size()||history_.isOpen()) return true;
      const auto *component=entity->components.at(type);
      if(bindingIndex>=component->type().resourceBindings.size()) return true;
      const auto &binding=component->type().resourceBindings[bindingIndex];
      if((binding.kind!=resources::AssetType::Mesh&&binding.kind!=resources::AssetType::EnvironmentProfile&&
          binding.kind!=resources::AssetType::EnvironmentMap&&binding.kind!=resources::AssetType::AnimationClip&&binding.kind!=resources::AssetType::Texture&&binding.kind!=resources::AssetType::UiDocument&&
          binding.kind!=resources::AssetType::PhysicsMaterial&&binding.kind!=resources::AssetType::AnimatorController)||
         slot>=binding.slotCount(*component)||
         !binding.presentation.isEditable(*component)) return true;
      state_.resourceInstance=component->instanceId();state_.resourceProperty=std::string(binding.id);state_.resourceSlot=slot;
      if(binding.id=="collision_mesh") refreshCollisionMeshDraft();
      state_.meshPicker=true;state_.meshPage=0;return true;
    }
    if(key>=widgetId(EditorWidget::ComponentFieldResetBase) && key<widgetId(EditorWidget::ComponentFieldResetBase)+0x10000000u) {
      const u32 encoded=key-widgetId(EditorWidget::ComponentFieldResetBase);
      const u32 index=encoded&0xffu,kind=(encoded>>8)&0xfu,field=(encoded>>12)&0xffu,slot=(encoded>>20)&0xffu;
      const auto *entity=document_.find(state_.selection);
      const auto *component=entity&&index<entity->components.size()?entity->components.at(index):nullptr;
      if(!component || component->instanceId()!=state_.expandedNative || history_.isOpen()) return true;
      const auto &type=component->type();
      const auto defaults=type.create();if(!defaults) return true;
      const auto changes=scene::componentDelta(*component,*defaults);
      std::vector<scene::FieldAddress> fields;
      const auto add=[&](std::string_view id,scene::FieldKind fieldKind,u32 fieldSlot=0) {
        for(const auto &change:changes) if(change.address==scene::FieldAddress{id,fieldSlot,fieldKind} && change.differs && change.applicable)
          fields.push_back(change.address);
      };
      switch(kind) {
      case 0:if(field<type.booleans.size()) add(type.booleans[field].id,scene::FieldKind::Boolean);break;
      case 1:if(field<type.enums.size()) add(type.enums[field].id,scene::FieldKind::Enum);break;
      case 2:if(field<type.numbers.size()) add(type.numbers[field].id,scene::FieldKind::Number);break;
      case 4:if(field<type.references.size()) add(type.references[field].id,scene::FieldKind::Reference);break;
      case 5:if(field<type.triples.size()) for(const auto channel:type.triples[field].channels) add(channel,scene::FieldKind::Number);break;
      case 9:if(field<type.slotNumbers.size()) add(type.slotNumbers[field].id,scene::FieldKind::SlotNumber,slot);break;
      case 10:if(field<type.slotEnums.size()) add(type.slotEnums[field].id,scene::FieldKind::SlotEnum,slot);break;
      default:return true;
      }
      if(fields.empty()) return true;
      auto candidate=*entity;
      const auto result=scene::applyComponentFields(candidate.components,*defaults,fields,component->instanceId());
      const auto *applied=candidate.components.findInstance(component->instanceId());
      if(!result.ok() || result.rejected || !applied || !editorReferencesAccept(document_,entity->id,*applied)) {
        state_.status=result.error?result.error:"Valor padrão incompatível com este objeto";return true;
      }
      if(history_.applyValues(document_,entity->id,candidate)) state_.status="Propriedade restaurada; disponível em Desfazer";
      return true;
    }
    if(key==widgetId(EditorWidget::ComponentPropertySearch)) {
      if(!state_.expandedNative || !document_.find(state_.selection)) return true;
      state_.editingPropertySearch=true;
      std::snprintf(state_.renameText,sizeof(state_.renameText),"%s",state_.propertyQuery.c_str());
      return true;
    }
    if(key==widgetId(EditorWidget::ComponentPropertySearchClear)) {state_.propertyQuery.clear();state_.propertyPage=0;return true;}
    if(key==widgetId(EditorWidget::ComponentSearch) || key==widgetId(EditorWidget::MeshSearch)) {
      state_.editingComponentSearch=key==widgetId(EditorWidget::ComponentSearch);state_.editingMeshSearch=!state_.editingComponentSearch;
      const auto &query=state_.editingComponentSearch?state_.componentQuery:state_.meshQuery;
      std::snprintf(state_.renameText,sizeof(state_.renameText),"%s",query.c_str());return true;
    }
    if(key==widgetId(EditorWidget::ComponentSearchClear)) {state_.componentQuery.clear();state_.componentPage=0;state_.addScroll=0;return true;}
    if(key>=widgetId(EditorWidget::ComponentFamilyBase) &&
       key<=widgetId(EditorWidget::ComponentFamilyBase)+static_cast<u32>(scene::ComponentFamily::Count)) {
      state_.componentCategory=key-widgetId(EditorWidget::ComponentFamilyBase);state_.addScroll=0;return true;
    }
    if(key==widgetId(EditorWidget::ObjectFold)) {
      state_.componentSelection=state_.selection;state_.expandedComponent=state_.expandedComponent=="astra.object"?"":"astra.object";
      state_.expandedNative=0;state_.expandedScript=0;state_.nativeMenu=0;state_.scriptMenu=0;state_.meshPicker=false;
      state_.transformMenu=false;state_.propertyPage=0;state_.propertyQuery.clear();return true;
    }
    if(key==widgetId(EditorWidget::TransformFold)) {
      state_.componentSelection=state_.selection;state_.expandedComponent=state_.expandedComponent=="astra.transform"?"":"astra.transform";
      state_.expandedNative=0;state_.expandedScript=0;state_.nativeMenu=0;state_.scriptMenu=0;state_.meshPicker=false;state_.propertyPage=0;state_.propertyQuery.clear();return true;
    }
    if(key==widgetId(EditorWidget::MeshChoose)) {
      state_.resourceInstance=0;state_.resourceProperty.clear();state_.meshPicker=true;state_.meshPage=0;return true;
    }
    // Vínculo com a fonte importada (M08.2).
    if(key==widgetId(EditorWidget::ImportLinkMenu)) {state_.importLinkMenu=!state_.importLinkMenu;return true;}
    if(key>=widgetId(EditorWidget::ImportLinkRevertBase) && key<widgetId(EditorWidget::ImportLinkRevertBase)+128u) {
      state_.status=revertImportLink(state_.selection,key-widgetId(EditorWidget::ImportLinkRevertBase))?"Revertido para a fonte":"Nada a reverter";
      state_.importLinkMenu=false;return true;
    }
    if(key==widgetId(EditorWidget::ImportLinkUnlink)) {
      const auto count=unlinkImport(state_.selection);
      state_.status=count?std::to_string(count)+" objeto(s) desvinculado(s); a fonte não altera mais esta instância":"Sem vínculo";
      state_.importLinkMenu=false;return true;
    }
    if(key==widgetId(EditorWidget::ImportLinkKeep) || key==widgetId(EditorWidget::ImportLinkDelete)) {
      const bool keep=key==widgetId(EditorWidget::ImportLinkKeep);
      state_.status=resolveImportOrphan(state_.selection,keep)?(keep?"Órfão mantido como objeto independente":"Órfão apagado"):"Nada a resolver";
      state_.importLinkMenu=false;return true;
    }
    // Recurso aberto em Propriedades → janela focada.
    if(key==widgetId(EditorWidget::AssetInspectorFocus)) {
      using Kind=EditorScreenState::FocusedAsset;
      const bool ok=state_.materialInspector.valid()?openFocusedAsset(Kind::Material,state_.materialInspector):
          state_.environmentInspector.valid()?openFocusedAsset(Kind::EnvironmentMap,state_.environmentInspector):
          state_.profileInspector.valid()?openFocusedAsset(Kind::EnvironmentProfile,state_.profileInspector):false;
      if(!ok) state_.status="Nada para abrir numa janela";
      return true;
    }
    // Material físico do projeto em Propriedades.
    if(state_.physicsMaterialInspector.valid()) {
      if(key==widgetId(EditorWidget::PhysicsMaterialInspectorClose)) {state_.physicsMaterialInspector={};state_.selectedFile.clear();state_.selectedFiles.clear();return true;}
      if(key>=widgetId(EditorWidget::PhysicsMaterialStepBase)&&key<widgetId(EditorWidget::PhysicsMaterialStepBase)+10) {
        const u32 code=key-widgetId(EditorWidget::PhysicsMaterialStepBase),field=code/2;const bool up=code%2;
        stepPhysicsMaterial(field,up);return true;
      }
    }
    // Perfil de ambiente do projeto em Propriedades.
    if(state_.profileInspector.valid()) {
      if(key==widgetId(EditorWidget::ProfileInspectorClose)) {state_.profileInspector={};state_.selectedFile.clear();state_.selectedFiles.clear();return true;}
      if(key==widgetId(EditorWidget::ProfileInspectorUses)) {
        const auto &users=state_.profileObjects;
        if(users.empty()) {state_.status="Nenhum componente Ambiente da cena usa este perfil";return true;}
        const u32 index=state_.profileUse%static_cast<u32>(users.size());
        pingEntity(users[index]);state_.profileUse=index+1;
        state_.status="Uso "+std::to_string(index+1)+" de "+std::to_string(users.size())+": "+document_.find(users[index])->name;
        return true;
      }
      if(key>=widgetId(EditorWidget::ProfileGroupBase) && key<widgetId(EditorWidget::ProfileGroupBase)+state_.profileGroups.size()) {
        state_.profileGroup=key-widgetId(EditorWidget::ProfileGroupBase);state_.propertyPage=0;refreshProfileInspector();return true;
      }
      if(key>=widgetId(EditorWidget::ProfileRowBase) && key<widgetId(EditorWidget::ProfileRowBase)+state_.profileRows.size()) {
        const auto row=state_.profileRows[key-widgetId(EditorWidget::ProfileRowBase)];
        if(!row.editable) {state_.status="Sem consumidor neste aparelho ou dependente de outra opção";return true;}
        using Kind=EditorScreenState::ProfileRow::Kind;
        std::string diagnostic;bool applied=false;
        if(row.kind==Kind::Boolean) applied=editProfileProperty(row,row.mixed?true:!row.on,nullptr,nullptr,diagnostic);
        else if(row.kind==Kind::Enum) {
          for(const auto &p:scene::Environment::descriptor.enums) if(p.id==row.id) {
            const auto *profile=findEnvironmentProfile(state_.profileInspector);
            scene::Environment proxy;proxy.values=profile->values;
            const u32 current=p.read(proxy);usize next=0;
            for(usize i=0;i<p.options.size();++i) if(p.options[i].value==current) next=(i+1)%p.options.size();
            applied=editProfileProperty(row,p.options[next].value,nullptr,nullptr,diagnostic);
          }
        } else if(row.kind==Kind::EnvironmentMap) {
          // Percorre os mapas HDRI do projeto e o ambiente padrão (sem mapa).
          const auto *profile=findEnvironmentProfile(state_.profileInspector);
          std::vector<resources::AssetGuid> maps{resources::AssetGuid{}};
          for(const auto &record:assets_.records()) if(record.type==resources::AssetType::EnvironmentMap) maps.push_back(record.guid);
          usize next=0;
          for(usize i=0;i<maps.size();++i) if(maps[i]==profile->values.environmentMap) next=(i+1)%maps.size();
          applied=editProfileProperty(row,false,nullptr,&maps[next],diagnostic);
        } else {
          // Número ou trio: o teclado numérico (trio como "r g b").
          if(history_.isOpen()) return true;
          state_.numericField=key;state_.numericEntity=0;state_.numericInstance=0;state_.numericProperty=std::string(row.id);
          std::snprintf(state_.numericText,sizeof(state_.numericText),"%s",row.kind==Kind::Triple?row.value.c_str():
                        row.value.substr(0,row.value.find(' ')).c_str());
          state_.numericReplace=true;state_.numericError=false;return true;
        }
        state_.status=applied?diagnostic:diagnostic;
        return true;
      }
    }
    // Mapa HDRI do projeto em Propriedades.
    if(state_.environmentInspector.valid()) {
      auto &draft=state_.environmentDraft;
      if(key==widgetId(EditorWidget::EnvironmentInspectorClose)) {state_.environmentInspector={};state_.selectedFile.clear();state_.selectedFiles.clear();return true;}
      if(key==widgetId(EditorWidget::EnvironmentExposureDown) || key==widgetId(EditorWidget::EnvironmentExposureUp)) {
        state_.environmentExposure=std::clamp(state_.environmentExposure+(key==widgetId(EditorWidget::EnvironmentExposureUp)?.5f:-.5f),-6.0f,6.0f);
        writeEnvironmentPreview();return true;
      }
      if(key==widgetId(EditorWidget::EnvironmentInspectorUses)) {
        const auto &users=state_.environmentObjects;
        if(users.empty()) {state_.status="Nenhum componente Ambiente da cena usa este HDRI";return true;}
        const u32 index=state_.environmentUse%static_cast<u32>(users.size());
        pingEntity(users[index]);state_.environmentUse=index+1;
        state_.status="Uso "+std::to_string(index+1)+" de "+std::to_string(users.size())+": "+document_.find(users[index])->name;
        return true;
      }
      // Receita: os mesmos degraus do painel de importação.
      const bool down=key>=widgetId(EditorWidget::EnvironmentRecipeDownBase) && key<widgetId(EditorWidget::EnvironmentRecipeDownBase)+5;
      const bool up=key>=widgetId(EditorWidget::EnvironmentRecipeUpBase) && key<widgetId(EditorWidget::EnvironmentRecipeUpBase)+5;
      if(down || up) {
        const u32 row=key-widgetId(down?EditorWidget::EnvironmentRecipeDownBase:EditorWidget::EnvironmentRecipeUpBase);
        editEnvironmentRecipe(row,up);
        return true;
      }
      if(key==widgetId(EditorWidget::EnvironmentRecipeRevert)) {revertEnvironmentRecipes();state_.status="Receita HDRI revertida";return true;}
      if(key==widgetId(EditorWidget::EnvironmentRecipeApply)) {
        if(multiAssetEditing_ && multiEnvironments_.size()>1) {requestEnvironmentBatch();return true;}
        // O shell reimporta pela mesma trilha do botão Reimportar, com esta receita.
        environmentReimportPath_=state_.environmentInspectorPath;environmentReimportOverride_=draft;
        state_.status="Reimportando o HDRI com a receita nova";return true;
      }
    }
    // Material do projeto em Propriedades.
    if(key==widgetId(EditorWidget::MaterialInspectorClose)) {
      state_.materialInspector={};state_.materialShared=false;state_.texturePicker=false;state_.selectedFile.clear();return true;
    }
    if(key==widgetId(EditorWidget::MaterialInspectorUses)) {
      // Cada toque revela o próximo objeto que usa o material (Ping), sem tirar o
      // material de Propriedades.
      const auto &users=state_.materialInspectorObjects;
      if(users.empty()) {state_.status="Nenhum objeto da cena usa este material";return true;}
      const u32 index=state_.materialInspectorUse%static_cast<u32>(users.size());
      pingEntity(users[index]);state_.materialInspectorUse=index+1;
      state_.status="Uso "+std::to_string(index+1)+" de "+std::to_string(users.size())+": "+document_.find(users[index])->name;
      return true;
    }
    // Material por slot (Entrega 2).
    if(key==widgetId(EditorWidget::MaterialSlotPrevious)) {if(state_.materialSlot) --state_.materialSlot;state_.propertyPage=0;return true;}
    if(key==widgetId(EditorWidget::MaterialSlotNext)) {++state_.materialSlot;state_.propertyPage=0;return true;}
    if(key==widgetId(EditorWidget::MaterialScopeInstance)) {state_.materialShared=false;return true;}
    if(key==widgetId(EditorWidget::MaterialScopeShared)) {
      if(state_.materialSlotView.shared) state_.materialShared=true;
      else state_.status="Este slot usa o material da fonte: crie um material do projeto para editar todos os usos";
      return true;
    }
    if(key==widgetId(EditorWidget::MaterialChoose)) {state_.materialPicker=true;state_.meshPage=0;return true;}
    if(key==widgetId(EditorWidget::MaterialPickerClose)) {state_.materialPicker=false;return true;}
    // R4: textura por binding, no alcance em edição.
    if(key>=widgetId(EditorWidget::MaterialTextureBase) && key<widgetId(EditorWidget::MaterialTextureBase)+scene::MaterialTextureCount) {
      state_.textureBinding=key-widgetId(EditorWidget::MaterialTextureBase);
      state_.texturePicker=true;state_.texturePickerScroll=0;state_.materialPicker=false;state_.meshPage=0;return true;
    }
    if(key==widgetId(EditorWidget::TexturePickerClose)) {state_.texturePicker=false;return true;}
    // R4: amostragem do binding aberto no seletor, no alcance em edição.
    if(key==widgetId(EditorWidget::TextureSamplingUv) || key==widgetId(EditorWidget::TextureSamplingWrap) ||
       key==widgetId(EditorWidget::TextureSamplingFilter) || key==widgetId(EditorWidget::TextureUvReset) ||
       (key>=widgetId(EditorWidget::TextureUvStepBase) && key<widgetId(EditorWidget::TextureUvStepBase)+10)) {
      const auto *entity=materialAssetMode()?nullptr:document_.find(state_.selection);
      const auto *render=entity?meshRenderer(*entity):nullptr;
      if((!render && !materialAssetMode()) || state_.textureBinding>=scene::MaterialTextureCount) return true;
      const bool shared=materialAssetMode() || state_.materialShared;
      const auto *asset=findMaterialAsset(sharedMaterialGuid(render));
      if(shared && !asset) {state_.status="Este slot usa o material da fonte: crie um material do projeto para editar todos os usos";return true;}
      auto sampling=shared?asset->sampling[state_.textureBinding]:render->slotSampling(state_.materialSlot)[state_.textureBinding];
      if(key==widgetId(EditorWidget::TextureSamplingUv)) sampling.uvSet=static_cast<std::uint8_t>((sampling.uvSet+1)%4);
      else if(key==widgetId(EditorWidget::TextureSamplingWrap)) sampling.wrap=static_cast<std::uint8_t>((sampling.wrap+1)%4);
      else if(key==widgetId(EditorWidget::TextureSamplingFilter)) sampling.filter=static_cast<std::uint8_t>((sampling.filter+1)%3);
      else if(key==widgetId(EditorWidget::TextureUvReset)) {
        sampling.offset[0]=sampling.offset[1]=0;sampling.scale[0]=sampling.scale[1]=1;sampling.rotation=0;
      } else {
        // Passos fixos e arredondados: somar floats repetidamente deixaria 0.30000001 no arquivo.
        const u32 step=key-widgetId(EditorWidget::TextureUvStepBase);
        const float sign=(step&1u)?1.0f:-1.0f;
        const auto rounded=[](float value){return std::round(value*100.0f)/100.0f;};
        switch(step/2) {
        case 0: sampling.offset[0]=std::clamp(rounded(sampling.offset[0]+.05f*sign),-100.0f,100.0f); break;
        case 1: sampling.offset[1]=std::clamp(rounded(sampling.offset[1]+.05f*sign),-100.0f,100.0f); break;
        case 2: sampling.scale[0]=std::clamp(rounded(sampling.scale[0]+.1f*sign),.1f,100.0f); break;
        case 3: sampling.scale[1]=std::clamp(rounded(sampling.scale[1]+.1f*sign),.1f,100.0f); break;
        default: sampling.rotation=std::fmod(sampling.rotation+15.0f*sign,360.0f); break;
        }
      }
      std::string diagnostic;
      state_.status=setSlotSampling(state_.selection,state_.materialSlot,state_.textureBinding,
                                    shared?MaterialScope::Shared:MaterialScope::Instance,sampling,diagnostic,
          "uv"+std::to_string(state_.textureBinding)+"."+
          (key==widgetId(EditorWidget::TextureSamplingUv)?"set":key==widgetId(EditorWidget::TextureSamplingWrap)?"wrap":
           key==widgetId(EditorWidget::TextureSamplingFilter)?"filter":key==widgetId(EditorWidget::TextureUvReset)?"reset":
           (key-widgetId(EditorWidget::TextureUvStepBase))/2==0?"offset0":(key-widgetId(EditorWidget::TextureUvStepBase))/2==1?"offset1":
           (key-widgetId(EditorWidget::TextureUvStepBase))/2==2?"scale0":(key-widgetId(EditorWidget::TextureUvStepBase))/2==3?"scale1":"rotation"))?
          (shared?"Amostragem compartilhada atualizada em todos os usos":"Amostragem desta instância atualizada"):diagnostic;
      return true;
    }
    // R4: visualizador de textura.
    if(key>=widgetId(EditorWidget::TextureViewBase) && key-widgetId(EditorWidget::TextureViewBase)<textures_.size()) {
      if(!openTextureViewer(key-widgetId(EditorWidget::TextureViewBase))) state_.status=state_.textureViewerInfo;
      return true;
    }
    if(key==widgetId(EditorWidget::TextureViewerClose)) {
      closeTextureViewer();
      // Em Propriedades, voltar leva ao gerenciador quando a textura veio dele.
      if(state_.textureInspector) {state_.textureInspector=false;state_.textureManager=textureInspectorFromManager_;}
      return true;
    }
    if(key==widgetId(EditorWidget::TextureViewerChannel)) {cycleTextureViewerChannel();return true;}
    if(key==widgetId(EditorWidget::TextureViewerZoom)) {cycleTextureViewerZoom();return true;}
    if(key==widgetId(EditorWidget::TextureProfilePrevious)) {
      if(state_.textureProfilePage) --state_.textureProfilePage;
      return true;
    }
    if(key==widgetId(EditorWidget::TextureProfileNext)) {
      state_.textureProfilePage=std::min(2u,state_.textureProfilePage+1);
      return true;
    }
    // R4: perfil da textura aberta no visualizador.
    if(key>=widgetId(EditorWidget::TextureProfileInterpretation) && key<=widgetId(EditorWidget::TextureProfileStreamingPriority) &&
       state_.textureViewer && state_.textureViewerIndex<textures_.size()) {
      auto &profile=state_.textureProfileDraft;
      stepTextureProfileField(profile,key-widgetId(EditorWidget::TextureProfileInterpretation));
      state_.textureProfileDirty=!resources::sameTextureProfile(profile,state_.textureProfileSaved);
      refreshTextureViewerImage();
      return true;
    }
    if((key==widgetId(EditorWidget::TextureProfileApply) || key==widgetId(EditorWidget::TextureProfileRevert)) &&
       state_.textureViewer && state_.textureViewerIndex<textures_.size()) {
      const auto texture=textures_[state_.textureViewerIndex];
      if(key==widgetId(EditorWidget::TextureProfileRevert)) {
        state_.textureProfileDraft=state_.textureProfileSaved;state_.textureProfileDirty=false;
        state_.status="Alterações do perfil descartadas";
      } else if(state_.textureProfileDirty) {
        std::string diagnostic;
        if(setTextureProfile(texture.guid,state_.textureProfileDraft,diagnostic)) {
          state_.textureProfileSaved=state_.textureProfileDraft;state_.textureProfileDirty=false;
          state_.status=diagnostic.empty()?"Perfil aplicado a todos os usos de "+texture.name:diagnostic;
        } else state_.status=std::move(diagnostic);
      }
      refreshTextureViewerImage();return true;
    }
    if(key==widgetId(EditorWidget::TextureViewerBackground)) {cycleTextureViewerBackground();return true;}
    if(key==widgetId(EditorWidget::TextureViewerMipDown) || key==widgetId(EditorWidget::TextureViewerMipUp)) {
      stepTextureViewerLevel(key==widgetId(EditorWidget::TextureViewerMipUp)?1:-1);
      return true;
    }
    // R4: modo de alfa, corte e faces, no alcance em edição.
    if(key==widgetId(EditorWidget::MaterialAlphaCycle) || key==widgetId(EditorWidget::MaterialSidesCycle) ||
       key==widgetId(EditorWidget::MaterialCutoffDown) || key==widgetId(EditorWidget::MaterialCutoffUp)) {
      const auto *entity=materialAssetMode()?nullptr:document_.find(state_.selection);
      const auto *render=entity?meshRenderer(*entity):nullptr;
      if(!render && !materialAssetMode()) return true;
      const bool shared=materialAssetMode() || state_.materialShared;
      const auto *asset=findMaterialAsset(sharedMaterialGuid(render));
      if(shared && !asset) {state_.status="Este slot usa o material da fonte: crie um material do projeto para editar todos os usos";return true;}
      auto surface=shared?asset->surface:render->slotSurface(state_.materialSlot);
      if(key==widgetId(EditorWidget::MaterialAlphaCycle)) surface.alphaMode=static_cast<std::uint8_t>((surface.alphaMode+1)%4);
      else if(key==widgetId(EditorWidget::MaterialSidesCycle)) surface.sides=static_cast<std::uint8_t>((surface.sides+1)%3);
      else if(surface.alphaMode==scene::MaterialAlphaMask)
        surface.alphaCutoff=std::clamp(std::round((surface.alphaCutoff+(key==widgetId(EditorWidget::MaterialCutoffUp)?.05f:-.05f))*100.0f)/100.0f,0.0f,1.0f);
      std::string diagnostic;
      state_.status=setSlotSurface(state_.selection,state_.materialSlot,shared?MaterialScope::Shared:MaterialScope::Instance,surface,diagnostic,
          key==widgetId(EditorWidget::MaterialAlphaCycle)?"alpha":key==widgetId(EditorWidget::MaterialSidesCycle)?"sides":"cutoff")?
          (shared?"Material compartilhado atualizado em todos os usos":"Material desta instância atualizado"):diagnostic;
      return true;
    }
    // R4: oclusão, canais, normal e origem do alfa, no alcance em edição.
    if(key==widgetId(EditorWidget::MaterialOcclusionSourceCycle) || key==widgetId(EditorWidget::MaterialOcclusionStrengthDown) ||
       key==widgetId(EditorWidget::MaterialOcclusionStrengthUp) || key==widgetId(EditorWidget::MaterialChannelRoughness) ||
       key==widgetId(EditorWidget::MaterialChannelMetallic) || key==widgetId(EditorWidget::MaterialChannelOcclusion) ||
       key==widgetId(EditorWidget::MaterialNormalFlipCycle) || key==widgetId(EditorWidget::MaterialAlphaSourceCycle)) {
      const auto *entity=materialAssetMode()?nullptr:document_.find(state_.selection);
      const auto *render=entity?meshRenderer(*entity):nullptr;
      if(!render && !materialAssetMode()) return true;
      const bool shared=materialAssetMode() || state_.materialShared;
      const auto *asset=findMaterialAsset(sharedMaterialGuid(render));
      if(shared && !asset) {state_.status="Este slot usa o material da fonte: crie um material do projeto para editar todos os usos";return true;}
      auto channels=shared?asset->channels:render->slotChannels(state_.materialSlot);
      const auto cycle=[](std::uint8_t value,unsigned count) {return static_cast<std::uint8_t>((value+1u)%count);};
      if(key==widgetId(EditorWidget::MaterialOcclusionSourceCycle)) channels.occlusionSource=cycle(channels.occlusionSource,4);
      else if(key==widgetId(EditorWidget::MaterialOcclusionStrengthDown) || key==widgetId(EditorWidget::MaterialOcclusionStrengthUp)) {
        const float current=channels.occlusionStrength<0?1.0f:channels.occlusionStrength;
        const float step=key==widgetId(EditorWidget::MaterialOcclusionStrengthUp)?.05f:-.05f;
        channels.occlusionStrength=std::clamp(std::round((current+step)*100.0f)/100.0f,0.0f,1.0f);
      }
      else if(key==widgetId(EditorWidget::MaterialChannelRoughness)) channels.roughness=cycle(channels.roughness,5);
      else if(key==widgetId(EditorWidget::MaterialChannelMetallic)) channels.metallic=cycle(channels.metallic,5);
      else if(key==widgetId(EditorWidget::MaterialChannelOcclusion)) channels.occlusion=cycle(channels.occlusion,5);
      else if(key==widgetId(EditorWidget::MaterialNormalFlipCycle)) channels.normalFlipY=cycle(channels.normalFlipY,3);
      else channels.alphaSource=cycle(channels.alphaSource,4);
      std::string diagnostic;
      state_.status=setSlotChannels(state_.selection,state_.materialSlot,shared?MaterialScope::Shared:MaterialScope::Instance,channels,diagnostic,
          key==widgetId(EditorWidget::MaterialOcclusionSourceCycle)?"ch0":
          key==widgetId(EditorWidget::MaterialChannelRoughness)?"ch3.0":
          key==widgetId(EditorWidget::MaterialChannelMetallic)?"ch3.1":
          key==widgetId(EditorWidget::MaterialChannelOcclusion)?"ch3.2":
          key==widgetId(EditorWidget::MaterialNormalFlipCycle)?"ch4":
          key==widgetId(EditorWidget::MaterialAlphaSourceCycle)?"ch5":"ch2")?
          (shared?"Material compartilhado atualizado em todos os usos":"Material desta instância atualizado"):diagnostic;
      return true;
    }
    if(key==widgetId(EditorWidget::MaterialIsolateCycle)) {
      state_.materialIsolate=static_cast<u8>((state_.materialIsolate+1u)%6u);
      state_.status=state_.materialIsolate?"Prévia isolando um dado do material (não é salvo)":"Prévia normal";
      return true;
    }
    if(key==widgetId(EditorWidget::MaterialOcclusionTexture)) {
      state_.textureBinding=scene::MaterialOcclusionTextureBinding;
      state_.texturePicker=true;state_.texturePickerScroll=0;state_.materialPicker=false;state_.meshPage=0;return true;
    }
    const auto applyTexture=[&](const resources::AssetGuid &texture,const std::string &done) {
      std::string diagnostic;
      const bool applied=setSlotTexture(state_.selection,state_.materialSlot,state_.textureBinding,
                                        state_.materialShared?MaterialScope::Shared:MaterialScope::Instance,texture,diagnostic);
      state_.status=applied?done:diagnostic;
      state_.texturePicker=false;
      return true;
    };
    if(key==widgetId(EditorWidget::TextureUseInherited)) return applyTexture({},"O binding voltou a herdar a textura");
    if(key==widgetId(EditorWidget::TextureUseNone)) return applyTexture(scene::MaterialTextureNone,"Binding sem textura");
    if(key>=widgetId(EditorWidget::TextureChoiceBase) && key-widgetId(EditorWidget::TextureChoiceBase)<textures_.size()) {
      const auto texture=textures_[key-widgetId(EditorWidget::TextureChoiceBase)];
      return applyTexture(texture.guid,"O binding usa "+texture.name);
    }
    if(key==widgetId(EditorWidget::MaterialClearOverride)) {
      state_.status=clearSlotMaterialOverride(state_.selection,state_.materialSlot)?"Substituição local removida":"Sem substituição local";
      return true;
    }
    if(key==widgetId(EditorWidget::MaterialCreateShared)) {
      std::string diagnostic;
      if(createMaterialFromSlot(state_.selection,state_.materialSlot,diagnostic).valid()) state_.materialShared=true;
      else state_.status=diagnostic;
      state_.materialPicker=false;return true;
    }
    if(key==widgetId(EditorWidget::MaterialUseSource)) {
      state_.status=assignSlotMaterial(state_.selection,state_.materialSlot,{})?"O slot voltou ao material da fonte":"Não foi possível trocar o material";
      state_.materialShared=false;state_.materialPicker=false;return true;
    }
    if(key>=widgetId(EditorWidget::MaterialChoiceBase) && key-widgetId(EditorWidget::MaterialChoiceBase)<materials_.size()) {
      const auto &material=materials_[key-widgetId(EditorWidget::MaterialChoiceBase)];
      state_.status=assignSlotMaterial(state_.selection,state_.materialSlot,material.guid)?"O slot usa "+material.name:"Não foi possível trocar o material";
      state_.materialPicker=false;return true;
    }
    if(key>=widgetId(EditorWidget::MaterialNumberBase) && key-widgetId(EditorWidget::MaterialNumberBase)<scene::meshRendererNumbers.size()) {
      if(history_.isOpen()) return true;
      const auto field=key-widgetId(EditorWidget::MaterialNumberBase);
      state_.numericField=key;state_.numericEntity=state_.selection;state_.numericInstance=0;state_.numericProperty.clear();
      std::snprintf(state_.numericText,sizeof(state_.numericText),"%.9g",static_cast<double>(state_.materialSlotView.values[field]));
      state_.numericReplace=true;state_.numericError=false;return true;
    }
    if(key==widgetId(EditorWidget::MeshPickerClose)) {state_.meshPicker=false;state_.resourceInstance=0;state_.resourceProperty.clear();return true;}
    if(key==widgetId(EditorWidget::MeshPrevious)) {if(state_.meshPage) --state_.meshPage;return true;}
    if(key==widgetId(EditorWidget::MeshNext)) {++state_.meshPage;return true;}
    // Tween de propriedade: abrir o seletor, fechar e aplicar a escolha. A
    // escolha grava tipo e PropertyId e usa o valor atual como destino inicial.
    if(key>=widgetId(EditorWidget::PropertyTweenPick)&&key<widgetId(EditorWidget::PropertyTweenPick)+0x01000000u) {
      const auto *entity=document_.find(state_.selection);const u32 index=key-widgetId(EditorWidget::PropertyTweenPick);
      if(!entity||index>=entity->components.size()||isPlaying()||history_.isOpen()) return true;
      const auto *component=entity->components.at(index);
      if(&component->type()!=&scene::PropertyTween::descriptor) return true;
      state_.propertyTweenPicker=component->instanceId();state_.meshPage=0;return true;
    }
    if(key==widgetId(EditorWidget::PropertyTweenPickerClose)) {state_.propertyTweenPicker=0;return true;}
    if(key>=widgetId(EditorWidget::PropertyTweenChoiceBase)&&key<widgetId(EditorWidget::PropertyTweenChoiceBase)+0x01000000u) {
      const u32 choice=key-widgetId(EditorWidget::PropertyTweenChoiceBase);
      const auto *entity=document_.find(state_.selection);
      const auto *component=entity?entity->components.findInstance(state_.propertyTweenPicker):nullptr;
      if(!component||&component->type()!=&scene::PropertyTween::descriptor||isPlaying()||history_.isOpen()) {state_.propertyTweenPicker=0;return true;}
      const auto &tween=scene::propertyTween(*component);
      const auto *target=tween.target?document_.find(static_cast<EditorEntityId>(tween.target)):entity;
      const auto options=target?tweenablePropertyOptions(target->components):std::vector<TweenablePropertyOption>{};
      if(choice>=options.size()) return true;
      auto values=*entity;auto &edited=scene::propertyTween(*values.components.editInstance(state_.propertyTweenPicker));
      edited.componentType=options[choice].type;edited.property=options[choice].property;
      edited.destination=std::clamp(options[choice].value,-100000.f,100000.f);edited.relative=false;
      if(!edited.valid()||!history_.applyValues(document_,state_.selection,values)) {state_.status="Propriedade recusada";return true;}
      state_.propertyTweenPicker=0;state_.status="Propriedade do tween: "+options[choice].componentName+" · "+options[choice].propertyName;
      return true;
    }
    if(key==widgetId(EditorWidget::PhysicsMaterialCreate)||key==widgetId(EditorWidget::PhysicsMaterialUpdate)) {
      std::string diagnostic;
      const bool created=key==widgetId(EditorWidget::PhysicsMaterialCreate);
      const bool ok=created?createPhysicsMaterial(state_.selection,state_.resourceInstance,diagnostic).valid():
                            updatePhysicsMaterial(state_.selection,state_.resourceInstance,diagnostic);
      state_.status=diagnostic;if(ok) {state_.meshPicker=false;state_.resourceInstance=0;state_.resourceProperty.clear();}
      return true;
    }
    if(key==widgetId(EditorWidget::EnvironmentProfileCreate)||key==widgetId(EditorWidget::EnvironmentProfileUpdate)) {
      std::string diagnostic;
      const bool created=key==widgetId(EditorWidget::EnvironmentProfileCreate);
      const bool ok=created?createEnvironmentProfile(state_.selection,state_.resourceInstance,diagnostic).valid():
                            updateEnvironmentProfile(state_.selection,state_.resourceInstance,diagnostic);
      state_.status=diagnostic;if(ok) {state_.meshPicker=false;state_.resourceInstance=0;state_.resourceProperty.clear();}
      return true;
    }
    constexpr u8 collisionQualitySteps[]{5,10,25,50,75,90};
    constexpr float collisionErrorSteps[]{.001f,.005f,.01f,.02f,.05f,.1f,.25f};
    const auto stepIndex=[](const auto &steps,auto value) {
      u32 closest=0;auto distance=std::abs(steps[0]-value);
      for(u32 i=1;i<std::size(steps);++i) if(const auto next=std::abs(steps[i]-value);next<distance) {closest=i;distance=next;}
      return closest;
    };
    if(key==widgetId(EditorWidget::MeshCollisionQualityDown)||key==widgetId(EditorWidget::MeshCollisionQualityUp)) {
      auto step=stepIndex(collisionQualitySteps,state_.collisionTrianglePercent);
      if(key==widgetId(EditorWidget::MeshCollisionQualityDown)&&step) --step;
      if(key==widgetId(EditorWidget::MeshCollisionQualityUp)&&step+1<std::size(collisionQualitySteps)) ++step;
      state_.collisionTrianglePercent=collisionQualitySteps[step];return true;
    }
    if(key==widgetId(EditorWidget::MeshCollisionErrorDown)||key==widgetId(EditorWidget::MeshCollisionErrorUp)) {
      auto step=stepIndex(collisionErrorSteps,state_.collisionMaximumError);
      if(key==widgetId(EditorWidget::MeshCollisionErrorDown)&&step) --step;
      if(key==widgetId(EditorWidget::MeshCollisionErrorUp)&&step+1<std::size(collisionErrorSteps)) ++step;
      state_.collisionMaximumError=collisionErrorSteps[step];return true;
    }
    if(key==widgetId(EditorWidget::MeshGenerateCollision)) {
      EditorActionRequest request;request.version=sceneVersion();request.entity=state_.selection;
      request.action=EditorAction::GenerateCollisionMesh;request.componentInstance=state_.resourceInstance;
      request.property=state_.collisionTrianglePercent;request.number=state_.collisionMaximumError;
      dispatch(request);
      return true;
    }
    if(key==widgetId(EditorWidget::MeshGeometryTab)||key==widgetId(EditorWidget::MeshMaterialTab)) {state_.meshTab=key==widgetId(EditorWidget::MeshMaterialTab);state_.propertyPage=0;state_.meshPicker=false;return true;}
    if((key>=widgetId(EditorWidget::MeshChoiceBase) && key<widgetId(EditorWidget::MeshChoiceBase)+0x01000000u) || key==widgetId(EditorWidget::MeshClear) || key==widgetId(EditorWidget::MaterialRestore)) {
      EditorActionRequest request;request.version=sceneVersion();request.entity=state_.selection;
      request.property=key>=widgetId(EditorWidget::MeshChoiceBase)?key-widgetId(EditorWidget::MeshChoiceBase)+1:0;
      if(state_.resourceInstance&&key!=widgetId(EditorWidget::MaterialRestore)) {
        request.action=EditorAction::ComponentResource;request.componentInstance=state_.resourceInstance;
        request.componentProperty=state_.resourceProperty;request.componentResourceSlot=state_.resourceSlot;
        if(request.property) {
          const auto *object=document_.find(state_.selection);
          const auto *component=object?object->components.findInstance(state_.resourceInstance):nullptr;
          const scene::ComponentResourceBinding *binding=nullptr;
          if(component) for(const auto &candidate:component->type().resourceBindings)
            if(candidate.id==state_.resourceProperty) {binding=&candidate;break;}
          if(binding&&(binding->kind==resources::AssetType::EnvironmentProfile||
                      binding->kind==resources::AssetType::EnvironmentMap||binding->kind==resources::AssetType::Texture||binding->kind==resources::AssetType::AudioClip||binding->kind==resources::AssetType::UiDocument||
                      binding->kind==resources::AssetType::PhysicsMaterial||binding->kind==resources::AssetType::AnimatorController)) {
            const auto index=request.property-1;
            if(index>=assets_.records().size()) return true;
            request.componentResource=assets_.records()[index].guid;
          } else if(binding&&binding->kind==resources::AssetType::AnimationClip) {
            // A lista do seletor é o catálogo de clipes carregados, na mesma ordem.
            const auto catalog=mapScene_.clipCatalog();
            const auto index=request.property-1;
            if(index>=catalog.size()) return true;
            request.componentResource=catalog[index].clip;
          } else request.componentResource=mapScene_.assetGuid(request.property-1);
        }
      } else request.action=key==widgetId(EditorWidget::MaterialRestore)?EditorAction::RestoreMaterial:EditorAction::AssignMesh;
      if(dispatch(request).status==EditorActionStatus::Applied) {
        state_.meshPicker=false;state_.resourceInstance=0;state_.resourceProperty.clear();
        state_.status=request.action==EditorAction::RestoreMaterial?"Material da origem restaurado":
                      request.action==EditorAction::ComponentResource?"Recurso do componente atualizado":"Referência de malha atualizada";
      }
      else state_.status="Referência incompatível com este objeto";
      return true;
    }
    const bool confirming=key==widgetId(EditorWidget::ComponentPreviewConfirm);
    if((confirming && state_.componentPreview) || (key>=widgetId(EditorWidget::ComponentAddBase)&&key-widgetId(EditorWidget::ComponentAddBase)<editorComponentCatalog.size())) {
      const u32 index=confirming?state_.componentPreview-1:key-widgetId(EditorWidget::ComponentAddBase);
      if(index>=editorComponentCatalog.size() || (confirming &&
         (!state_.componentPreview || !state_.addingComponent || state_.componentSelection!=state_.selection))) return true;
      const auto &entry=editorComponentCatalog[index];
      const auto *entity=document_.find(state_.selection);
      if(!entity) return true;
      const auto plan=scene::planComponentAddition(entity->components,entry.type->id,false,false);
      if(!confirming) {
        state_.componentPreview=index+1;state_.componentPreviewValues=false;state_.componentPreviewPage=0;
        state_.scriptPreviewType.clear();return true;
      }
      if(!plan.ready) {state_.status=plan.error?plan.error:"Não foi possível compor o objeto";return true;}
      EditorActionRequest request;request.version=sceneVersion();request.action=EditorAction::AddComponent;
      request.entity=state_.selection;request.componentType=entry.type->id;
      const auto result=dispatch(request);
      if(result.status==EditorActionStatus::Applied) {
        state_.componentSelection=state_.selection;state_.expandedComponent.clear();state_.expandedNative=0;state_.expandedScript=0;
        if(const auto *entity=document_.find(state_.selection))
          for(usize i=entity->components.size();i>0;--i)
            if(const auto *component=entity->components.at(i-1);component && &component->type()==entry.type) {
              state_.expandedNative=component->instanceId();break;
            }
        // Recentes: o tipo sobe para o topo, sem repetição, e a lista fica curta.
        auto &recent=state_.recentComponents;
        recent.erase(std::remove(recent.begin(),recent.end(),entry.type->id),recent.end());
        recent.insert(recent.begin(),std::string(entry.type->id));
        if(recent.size()>4) recent.resize(4);
        state_.addingComponent=false;state_.componentPreview=0;state_.componentPage=0;state_.componentGroup.clear();state_.propertyPage=0;
        state_.nativeMenu=0;state_.scriptMenu=0;
        state_.status="Componente adicionado";
      } else if(const auto *entity=document_.find(state_.selection)) {
        const auto *reason=entry.unavailable(*entity);state_.status=reason?reason:"Não foi possível adicionar o componente";
      }
      return true;
    }
    if(confirming && !state_.scriptPreviewType.empty()) {
      if(!state_.addingComponent || state_.componentSelection!=state_.selection ||
         code_.publishedGeneration()!=state_.scriptPreviewGeneration) {
        state_.status="Catálogo alterado; volte e escolha novamente";return true;
      }
      const auto type=std::find_if(code_.scriptTypes().begin(),code_.scriptTypes().end(),
          [&](const auto &candidate){return candidate.id==state_.scriptPreviewType;});
      if(type==code_.scriptTypes().end()) {state_.status="Tipo indisponível; volte e escolha novamente";return true;}
      EditorActionRequest request;request.version=sceneVersion();request.action=EditorAction::AddScript;
      request.entity=state_.selection;request.scriptType=type->id;
      if(dispatch(request).status==EditorActionStatus::Applied) {
        state_.expandedComponent.clear();state_.expandedNative=0;state_.expandedScript=0;
        if(const auto *entity=document_.find(state_.selection))
          if(const auto *script=scene::scriptBehavior(entity->components.at(entity->components.size()-1)))
            state_.expandedScript=script->instanceId();
        state_.addingComponent=false;state_.scriptPreviewType.clear();state_.componentPage=0;
        state_.componentGroup.clear();state_.scriptPropertyPage=0;state_.status="Comportamento anexado";
      } else state_.status="Não foi possível anexar o comportamento";
      return true;
    }
    if(key==widgetId(EditorWidget::ColliderFit)) {
      const auto *entity=document_.find(state_.selection);
      if(entity && colliderComponent(*entity) && !history_.isOpen()) {
        auto value=*entity;auto *collider=editCollider(value,state_.expandedNative);
        if(collider && fitEditorCollider(mapScene_,document_,entity->id,*collider) && history_.applyValues(document_,entity->id,value))
          state_.status="Colisor ajustado pela geometria; forma e centro continuam editáveis";
        else state_.status="Não foi possível ajustar: geometria ausente, escala ou dimensões incompatíveis";
      }
      return true;
    }
    if(key==widgetId(EditorWidget::LodGroupFit)) {
      const auto *entity=document_.find(state_.selection);
      const auto *current=entity?static_cast<const scene::LodGroup *>(entity->components.find(scene::LodGroup::descriptor)):nullptr;
      if(current && !history_.isOpen()) {
        auto value=*entity;auto *group=static_cast<scene::LodGroup *>(value.components.edit(scene::LodGroup::descriptor));
        if(group && fitLodGroupSize(mapScene_,document_,entity->id,*group) && history_.applyValues(document_,entity->id,value))
          state_.status="Tamanho do LOD Group medido pela malha do LOD 0";
        else state_.status="Não foi possível medir: o LOD 0 não tem malha";
      }
      return true;
    }
    if(key==widgetId(EditorWidget::ComponentPrevious)||key==widgetId(EditorWidget::ComponentNext)) {
      state_.componentPage=layout_.componentPage;
      if(key==widgetId(EditorWidget::ComponentPrevious)) {if(state_.componentPage) --state_.componentPage;}
      else ++state_.componentPage;
      state_.componentSelection=state_.selection;return true;
    }
    if(key==widgetId(EditorWidget::ScriptFieldsPrevious)||key==widgetId(EditorWidget::ScriptFieldsNext)) {
      if(key==widgetId(EditorWidget::ScriptFieldsPrevious)) {if(state_.scriptPropertyPage) --state_.scriptPropertyPage;}
      else ++state_.scriptPropertyPage;
      return true;
    }
    const u32 keyOperation=key&0xff000000u;
    const bool arrayWidget=keyOperation>=widgetId(EditorWidget::ScriptArraySizeBase) && keyOperation<=widgetId(EditorWidget::ScriptArrayRemoveBase);
    if((key>=widgetId(EditorWidget::ScriptAddBase)&&key<widgetId(EditorWidget::ScriptFieldBase)+0x01000000u) || arrayWidget) {
      const auto *entity=document_.find(state_.selection);if(!entity||state_.workspace!=EditorWorkspace::Scene||history_.isOpen()) return true;
      const u32 operation=key&0xff000000u;
      const u32 index=key&(operation==widgetId(EditorWidget::ScriptFieldBase)||arrayWidget?0xffu:0x00ffffffu);
      state_.componentSelection=state_.selection;
      if(operation==widgetId(EditorWidget::ScriptAddBase)) {
        if(index>=code_.scriptTypes().size() || !state_.addingComponent) return true;
        state_.scriptPreviewType=code_.scriptTypes()[index].id;
        state_.scriptPreviewGeneration=code_.publishedGeneration();state_.componentPreview=0;
        return true;
      }
      const auto *script=scene::scriptBehavior(entity->components.at(index));if(!script) return true;
      if(operation==widgetId(EditorWidget::ScriptFoldBase)) {
        state_.expandedScript=state_.expandedScript==script->instanceId()?0:script->instanceId();state_.expandedComponent.clear();state_.expandedNative=0;state_.nativeMenu=0;state_.scriptMenu=0;state_.meshPicker=false;state_.scriptPropertyPage=0;state_.propertyQuery.clear();
      } else if(operation==widgetId(EditorWidget::ScriptMenuBase)) {
        state_.scriptMenu=state_.scriptMenu==script->instanceId()?0:script->instanceId();state_.expandedScript=0;state_.expandedComponent.clear();state_.expandedNative=0;state_.propertyQuery.clear();
      } else if(operation==widgetId(EditorWidget::ScriptSourceBase)) {
        if(code_.open(files_,script->source)) {state_.code=&code_;state_.workspace=EditorWorkspace::Code;} else state_.status=code_.error();
      } else if(operation==widgetId(EditorWidget::ScriptArraySizeBase) || operation==widgetId(EditorWidget::ScriptArrayElementBase) ||
                operation==widgetId(EditorWidget::ScriptArrayAddBase) || operation==widgetId(EditorWidget::ScriptArrayRemoveBase)) {
        const auto *declared=scriptProperty(script->scriptType,state_.expandedScriptArray);
        const auto element=declared?scene::scriptArrayElementType(declared->valueType):std::string_view{};
        if(element.empty()) return true;
        auto items=scriptArrayItems(*script,declared->id);
        const u32 at=(key&0x00ffffffu)>>8;
        if(operation==widgetId(EditorWidget::ScriptArraySizeBase)) {
          state_.editingScriptInstance=script->instanceId();state_.editingScriptEntity=entity->id;
          state_.editingScriptProperty=declared->id;state_.editingScriptType="int32";state_.editingScriptArraySize=true;
        } else if(operation==widgetId(EditorWidget::ScriptArrayElementBase)) {
          if(at>=items.size()) return true;
          state_.scriptArraySelected=at+1;
          if(scene::scriptCurveType(element)) {
            state_.curveEntity=entity->id;state_.curveInstance=script->instanceId();state_.curveProperty=declared->id;
            state_.curveElement=at+1;
            openCurveEditor(key,items[at],declared->valueType);
          } else if(scene::scriptGradientType(element)) {
            state_.gradientEntity=entity->id;state_.gradientInstance=script->instanceId();state_.gradientProperty=declared->id;
            state_.gradientElement=at+1;
            openGradientEditor(key,items[at],element);
            state_.gradientType=declared->valueType;
          } else if(bool hdr=false,alpha=true;scene::scriptColorType(element,&hdr,&alpha)) {
            float rgba[4]{1,1,1,1};scene::parseScriptColor(items[at],rgba);
            state_.colorEntity=entity->id;state_.colorInstance=script->instanceId();state_.colorProperty=declared->id;
            state_.colorTarget=2;state_.colorScriptType=declared->valueType;state_.colorScriptElement=at+1;
            openColorWindow(key,rgba,alpha,hdr);
          } else if(element=="bool") {
            items[at]=items[at]=="true"?"false":"true";
            setScriptArray(entity->id,script->instanceId(),declared->id,declared->valueType,items);
          } else if(element=="object" || !scene::scriptComponentTypeId(element).empty()) {
            state_.referenceInstance=script->instanceId();state_.referenceProperty=declared->id;state_.referenceScript=true;
            state_.referenceScriptType=std::string(element);state_.referenceScriptElement=at+1;
            state_.referenceQuery.clear();state_.referencePage=0;
          } else {
            state_.editingScriptInstance=script->instanceId();state_.editingScriptEntity=entity->id;
            state_.editingScriptProperty=declared->id;state_.editingScriptType=std::string(element);state_.editingScriptElement=at+1;
          }
        } else if(operation==widgetId(EditorWidget::ScriptArrayAddBase)) {
          // Unity: o elemento novo copia o anterior.
          items.push_back(items.empty()?scene::scriptElementDefault(element):items.back());
          if(items.size()>scene::kScriptArrayMaximum) {state_.status="Limite de 1024 elementos";return true;}
          if(setScriptArray(entity->id,script->instanceId(),declared->id,declared->valueType,items)) state_.scriptArraySelected=items.size();
        } else if(!items.empty()) {
          const usize removed=state_.scriptArraySelected&&state_.scriptArraySelected<=items.size()?state_.scriptArraySelected-1:items.size()-1;
          items.erase(items.begin()+static_cast<std::ptrdiff_t>(removed));
          if(setScriptArray(entity->id,script->instanceId(),declared->id,declared->valueType,items)) state_.scriptArraySelected=0;
        }
      } else if(operation==widgetId(EditorWidget::ScriptFieldBase)) {
        const u32 field=(key&0x00ffffffu)>>8;
        for(const auto &type:code_.scriptTypes()) if(type.id==script->scriptType && field<type.properties.size()) {
          const auto &property=type.properties[field];
          if(!scene::scriptArrayElementType(property.valueType).empty()) {
            for(const auto &issue:state_.scriptFieldIssues) if(issue.instance==script->instanceId() && issue.property==property.id && issue.isNull) {
              if(setScriptArray(entity->id,script->instanceId(),property.id,property.valueType,{})) state_.expandedScriptArray=property.id;
              return true;
            }
            // Lista: o toque abre e fecha os elementos logo abaixo do campo.
            state_.expandedScriptArray=state_.expandedScriptArray==property.id?std::string():property.id;
            state_.scriptArraySelected=0;return true;
          }
          if(scene::scriptCurveType(property.valueType)) {
            std::string value=scene::scriptElementDefault(property.valueType);
            for(const auto &p:script->properties) if(p.id==property.id&&p.valueType==property.valueType) value=p.value;
            state_.curveEntity=entity->id;state_.curveInstance=script->instanceId();state_.curveProperty=property.id;
            state_.curveElement=0;
            openCurveEditor(key,value,property.valueType);return true;
          }
          if(scene::scriptGradientType(property.valueType)) {
            std::string value=scene::scriptElementDefault(property.valueType);
            for(const auto &p:script->properties) if(p.id==property.id&&p.valueType==property.valueType) value=p.value;
            state_.gradientEntity=entity->id;state_.gradientInstance=script->instanceId();state_.gradientProperty=property.id;
            state_.gradientElement=0;
            openGradientEditor(key,value,property.valueType);return true;
          }
          if(bool hdr=false,alpha=true;scene::scriptColorType(property.valueType,&hdr,&alpha)) {
            float rgba[4]{1,1,1,1};
            for(const auto &p:script->properties) if(p.id==property.id&&p.valueType==property.valueType) scene::parseScriptColor(p.value,rgba);
            state_.colorEntity=entity->id;state_.colorInstance=script->instanceId();state_.colorProperty=property.id;
            state_.colorTarget=1;state_.colorScriptType=property.valueType;state_.colorScriptElement=0;
            openColorWindow(key,rgba,alpha,hdr);return true;
          }
          if(property.valueType=="bool") {
            // Interruptor: grava direto, como a caixa de seleção da Unity.
            bool on=false;for(const auto &p:script->properties) if(p.id==property.id&&p.valueType=="bool") on=p.value=="true";
            EditorActionRequest request;request.version=sceneVersion();request.entity=entity->id;
            request.componentInstance=script->instanceId();request.action=EditorAction::ScriptProperty;
            request.componentProperty=property.id;request.scriptPropertyType="bool";request.scriptPropertyValue=on?"false":"true";
            dispatch(request);return true;
          }
          if(property.valueType=="object" || !scene::scriptComponentTypeId(property.valueType).empty()) {
            state_.referenceInstance=script->instanceId();state_.referenceProperty=property.id;state_.referenceScript=true;
            state_.referenceScriptType=property.valueType;state_.referenceScriptElement=0;
            state_.referenceQuery.clear();state_.referencePage=0;return true;
          }
          state_.editingScriptInstance=script->instanceId();state_.editingScriptEntity=entity->id;
          state_.editingScriptProperty=property.id;state_.editingScriptType=property.valueType;
        }
      } else {
        auto value=*entity;
        if(operation==widgetId(EditorWidget::ScriptEnabledBase)) {
          auto replacement=*script;replacement.enabled=!replacement.enabled;
          if(!value.components.replaceInstance(script->instanceId(),replacement)) return true;
        } else return true;
        history_.applyValues(document_,state_.selection,value);
      }
      return true;
    }
    if(key==widgetId(EditorWidget::CodeOpen)) {state_.code=&code_;state_.workspace=EditorWorkspace::Code;return true;}
    if(state_.workspace==EditorWorkspace::Code) {
      if(key>=widgetId(EditorWidget::CodeTabBase) && key-widgetId(EditorWidget::CodeTabBase)<code_.buffers().size()) {
        code_.select(code_.buffers()[key-widgetId(EditorWidget::CodeTabBase)].id);return true;
      }
      switch(static_cast<EditorWidget>(key)) {
      case EditorWidget::CodeApply:
        state_.codeMenu=false;
        if(state_.codeCompilerAvailable && !state_.codeBuildBusy) {
          if(files_.rootPath().empty()) state_.status="Abra um projeto antes de aplicar código";
          else if(!code_.saveAll(files_)) state_.status=code_.error();
          else {codeBuildGeneration_=code_.generation();codeBuildRequest_=files_.rootPath();state_.codeBuildBusy=true;state_.status="Compilando código do projeto";}
        }
        return true;
      case EditorWidget::CodeMenu:state_.codeMenu=!state_.codeMenu;return true;
      case EditorWidget::CodeFiles:state_.codeFiles=!state_.codeFiles;state_.codeMenu=false;return true;
      case EditorWidget::CodeConsole:state_.codeFiles=false;state_.codeMenu=false;state_.consoleCollapsed=false;state_.consoleExpanded=true;return true;
      case EditorWidget::CodeTabsPrevious:if(state_.codeFirstTab) --state_.codeFirstTab;return true;
      case EditorWidget::CodeTabsNext:if(state_.codeFirstTab+1<code_.buffers().size()) ++state_.codeFirstTab;return true;
      case EditorWidget::CodeNewFolder:state_.creatingCodeFolder=true;return true;
      case EditorWidget::CodeGoLine:state_.goingToLine=true;state_.codeMenu=false;return true;
      case EditorWidget::CodeFindPrevious:
      case EditorWidget::CodeFindNext: {
        auto *buffer=code_.active();const auto matches=code_.find(state_.codeQuery);
        if(buffer && !matches.empty()) {
          usize index=0;
          if(key==widgetId(EditorWidget::CodeFindNext)) {
            while(index<matches.size() && matches[index].offset<=buffer->selectionStart) ++index;
            if(index==matches.size()) index=0;
          } else {
            index=matches.size()-1;
            for(usize i=0;i<matches.size();++i) if(matches[i].offset<buffer->selectionStart) index=i;
          }
          buffer->selectionStart=static_cast<u32>(matches[index].offset);
          buffer->selectionEnd=static_cast<u32>(matches[index].offset+matches[index].length);
          buffer->firstLine=matches[index].line-1;++buffer->viewRevision;
          state_.status=std::to_string(index+1)+" / "+std::to_string(matches.size());
        }
        return true;
      }
      case EditorWidget::CodeSaveAll:
        state_.codeMenu=false;
        if(!code_.saveAll(files_)) state_.status=code_.error();
        else state_.status="Arquivos de código salvos";
        return true;
      case EditorWidget::CodeScene:state_.codeMenu=false;state_.workspace=EditorWorkspace::Scene;return true;
      case EditorWidget::CodeNew:state_.creatingScript=true;state_.scriptTemplate=~0u;state_.codeMenu=false;return true;
      case EditorWidget::CodeNewHelper:state_.creatingScript=true;state_.scriptTemplate=EditorCodeWorkspace::HelperTemplate;state_.codeMenu=false;return true;
      case EditorWidget::CodeTemplates:state_.choosingTemplate=true;state_.codeMenu=false;return true;
      case EditorWidget::CodeTemplateClose:state_.choosingTemplate=false;return true;
      case EditorWidget::CodeEdit:state_.editingCode=code_.active()!=nullptr;return true;
      case EditorWidget::CodeSave:state_.status=code_.save(files_)?"Código salvo":code_.error();return true;
      case EditorWidget::CodeUndo:state_.codeMenu=false;codeHistoryAction(false);return true;
      case EditorWidget::CodeRedo:state_.codeMenu=false;codeHistoryAction(true);return true;
      case EditorWidget::CodeSearch:
        state_.codeMenu=false;
        if(state_.platformCodeView) ++state_.codeSearchRequest;
        else state_.searchingCode=true;
        return true;
      case EditorWidget::CodeClose:
        state_.codeMenu=false;
        if(const auto *buffer=code_.active()) if(!code_.close(buffer->id)) state_.status=code_.error();
        return true;
      default:break;
      }
    }
  }
  if(routing.tapped && (routing.widgetId&0xff000000u)==widgetId(EditorWidget::CodeTemplateBase) &&
     state_.choosingTemplate) {
    const u32 choice=routing.widgetId-widgetId(EditorWidget::CodeTemplateBase);
    if(choice<=editorScriptTemplates.size()) {
      // Zero é o arquivo vazio; os demais deslocam um por causa dele.
      state_.scriptTemplate=choice?choice-1:~0u;
      state_.choosingTemplate=false;
      state_.creatingScript=true;
    }
    return true;
  }
  if(routing.widgetId==widgetId(EditorWidget::FilesSplitter)) {
    if(routing.dragging && !state_.filesCollapsed) {
      const float height=layout_.hierarchyPanel.height+layout_.filesPanel.height;
      if(height>0) state_.filePanelRatio=std::clamp(state_.filePanelRatio-routing.stepDelta.y/height,.28f,.58f);
    }
    return true;
  }
  // Toque longo no olho ou na mão: só o objeto, sem os descendentes (Unity:
  // Alt+clique).
  if(routing.tapped && routing.heldSeconds>=ui::kUiLongPressSeconds) {
    const u32 key=routing.widgetId;
    const bool eye=key>=widgetId(EditorWidget::HierarchyEyeBase) && key<widgetId(EditorWidget::HierarchyEyeBase)+EditorDocument::kMaximumEntities;
    const bool hand=key>=widgetId(EditorWidget::HierarchyPickBase) && key<widgetId(EditorWidget::HierarchyPickBase)+EditorDocument::kMaximumEntities;
    if(eye || hand) {
      const EditorEntityId id=key-widgetId(eye?EditorWidget::HierarchyEyeBase:EditorWidget::HierarchyPickBase);
      auto &list=eye?state_.sceneHidden:state_.scenePickOff;
      auto found=std::find(list.begin(),list.end(),id);
      if(found!=list.end()) list.erase(found); else if(document_.exists(id)) list.push_back(id);
      state_.sceneVisibilityChanged=true;
      return true;
    }
  }
  // Toque longo num recurso em Arquivos: abre-o numa janela focada (Unity:
  // Properties), sem trocar o que Propriedades mostra.
  if(state_.files && routing.tapped && routing.heldSeconds>=ui::kUiLongPressSeconds &&
     routing.widgetId>=widgetId(EditorWidget::FileRowBase) && routing.widgetId-widgetId(EditorWidget::FileRowBase)<files_.tree().size()) {
    const auto &entry=files_.tree()[routing.widgetId-widgetId(EditorWidget::FileRowBase)];
    using Kind=EditorScreenState::FocusedAsset;
    if(const auto *record=assets_.findByPath(entry.relativePath)) {
      const Kind kind=record->type==resources::AssetType::Material?Kind::Material:
          record->type==resources::AssetType::EnvironmentMap?Kind::EnvironmentMap:
          record->type==resources::AssetType::EnvironmentProfile?Kind::EnvironmentProfile:Kind::None;
      if(kind!=Kind::None && openFocusedAsset(kind,record->guid)) return true;
    }
  }
  if(state_.files && routing.dragging && routing.widgetId>=widgetId(EditorWidget::FileRowBase)
      && routing.widgetId-widgetId(EditorWidget::FileRowBase)<files_.tree().size()) {
    state_.fileScrollOffset=std::max(0.0f,state_.fileScrollOffset-routing.stepDelta.y);
    const float row=state_.workspace==EditorWorkspace::Code?34.0f:24.0f;
    const auto visible=static_cast<u32>(std::max(row,layout_.filesPanel.height-(state_.workspace==EditorWorkspace::Code?126:66))/row);
    const auto maximum=files_.tree().size()>visible?files_.tree().size()-visible:0;
    state_.fileScrollOffset=std::min(state_.fileScrollOffset,static_cast<float>(maximum)*row);
    state_.fileScroll=static_cast<u32>(state_.fileScrollOffset/row);
    return true;
  }
  if(routing.tapped && (routing.widgetId>=widgetId(EditorWidget::TweenRestart)&&routing.widgetId<widgetId(EditorWidget::TweenRestart)+0x10000u)) {
    if(!isPlaying()||!playScene_.active()) return false;
    auto &world=playScene_.world();
    const auto target=state_.inspectorTarget?state_.inspectorTarget:state_.selection;
    const u32 key=routing.widgetId-widgetId(EditorWidget::TweenRestart);
    const auto *object=world.find(world.handle(target));
    if(!object||key/4>=object->components.size()||key%4==3)return false;
    const auto component=runtime::ComponentHandle{world.handle(target),object->components.at(key/4)->instanceId()};
    const auto *value=world.readComponent(component);
    if(!value||value->type().id!=scene::TransformTween::descriptor.id) return false;
    const bool restart=key%4==0;
    const auto *live=playScene_.tweens().state(target,component.instance);
    const u32 operation=restart?1:key%4==1?2:live&&live->paused?4:3;
    runtime::SceneTweens::State snapshot;
    const bool accepted=playScene_.tweens().command(world,component,operation,snapshot)==runtime::WorldStatus::Ok;
    state_.status=accepted?(operation==3?"Tween pausado":operation==4?"Tween retomado":restart?"Tween reiniciado no mundo Play":"Tween cancelado; pose atual preservada"):"Tween indisponível no mundo Play";
    return accepted;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::ImportWave)) {
    if(isPlaying() || history_.isOpen()) return false;
    state_.waveImportRequested=true;state_.codeFiles=false;state_.creationMenu=false;
    return true;
  }
  if(routing.tapped && state_.files && !history_.isOpen() && !isPlaying()) {
    const auto key=routing.widgetId;
    if(key==widgetId(EditorWidget::FilesCollapse)) {state_.filesCollapsed=!state_.filesCollapsed;return true;}
    const u32 base=widgetId(EditorWidget::FileRowBase);
    // Vários arquivos (Unity: Ctrl/Shift+clique no Project): um modo no toque.
    if(key==widgetId(EditorWidget::FilesMultiToggle)) {
      state_.filesMultiSelect=!state_.filesMultiSelect;
      if(!state_.filesMultiSelect && state_.selectedFiles.size()>1)
        selectFiles(state_.selectedFile.empty()?std::vector<std::string>{}:std::vector<std::string>{state_.selectedFile});
      state_.status=state_.filesMultiSelect?"Selecionar vários: toque nos arquivos para somar ou tirar":"Seleção única em Arquivos";
      return true;
    }
    if(key==widgetId(EditorWidget::FilesSelectNone)) {selectFiles({});return true;}
    // Inspector de vários recursos.
    const auto &view=state_.multiAsset;
    if(key>=widgetId(EditorWidget::MultiAssetNarrowBase) && key<widgetId(EditorWidget::MultiAssetNarrowBase)+view.groups.size()) {
      // Unity: "Narrow the Selection" — só os arquivos daquele tipo.
      const auto &group=view.groups[key-widgetId(EditorWidget::MultiAssetNarrowBase)];
      std::vector<std::string> kept;
      for(usize i=0;i<state_.selectedFiles.size() && i<multiAssetGroups_.size();++i)
        if(fileKinds[multiAssetGroups_[i]].plural==group.label) kept.push_back(state_.selectedFiles[i]);
      selectFiles(std::move(kept));return true;
    }
    if(key>=widgetId(EditorWidget::MultiAssetItemBase) && key<widgetId(EditorWidget::MultiAssetItemBase)+view.items.size()) {
      selectFiles({view.items[key-widgetId(EditorWidget::MultiAssetItemBase)].path});return true;
    }
    if(key>=widgetId(EditorWidget::MultiAssetRemoveBase) && key<widgetId(EditorWidget::MultiAssetRemoveBase)+view.items.size()) {
      auto paths=state_.selectedFiles;const auto removed=view.items[key-widgetId(EditorWidget::MultiAssetRemoveBase)].path;
      std::erase(paths,removed);
      // O ativo continua o ativo; tirado ele, o último da lista assume.
      if(removed!=state_.selectedFile) if(auto found=std::find(paths.begin(),paths.end(),state_.selectedFile);found!=paths.end()) {
        paths.erase(found);paths.push_back(state_.selectedFile);
      }
      selectFiles(std::move(paths));return true;
    }
    if(view.kind==EditorScreenState::MultiAssetView::Kind::Textures && !multiTextures_.empty()) {
      if(key>=widgetId(EditorWidget::MultiAssetFieldBase) && key<widgetId(EditorWidget::MultiAssetFieldBase)+TextureProfileFields) {
        const u32 field=key-widgetId(EditorWidget::MultiAssetFieldBase);
        const bool mixed=(view.mixed>>field)&1u;
        if(routing.heldSeconds>=ui::kUiLongPressSeconds) {
          // Unity: clique direito › Set to Value of — de qual textura copiar.
          if(!mixed) {state_.status="Todas já têm o mesmo valor";return true;}
          const auto text=textureProfileFieldText(multiTextures_.front().draft,field);
          state_.setValueMenu.key="asset.tex."+std::to_string(field);state_.setValueMenu.label=text.substr(0,text.find(": "));
          state_.setValueMenu.rows.clear();
          for(u32 i=0;i<multiTextures_.size();++i) {
            const auto *record=assets_.find(multiTextures_[i].guid);
            const auto value=textureProfileFieldText(multiTextures_[i].draft,field);
            state_.setValueMenu.rows.push_back({i,(record?baseName(record->path):std::string("?"))+"  \xC2\xB7  "+value.substr(value.find(": ")+2)});
          }
          return true;
        }
        // O passo parte do valor da textura ativa; liga/desliga misturado liga
        // todas (como o toggle "—" da Unity).
        const auto *active=assets_.findByPath(state_.selectedFile);
        auto reference=multiTextures_.front().draft;
        for(const auto &texture:multiTextures_) if(active && texture.guid==active->guid) reference=texture.draft;
        if(mixed && textureProfileToggle(field)) setTextureProfileToggle(reference,field,true);
        else stepTextureProfileField(reference,field);
        for(auto &texture:multiTextures_) copyTextureProfileField(texture.draft,reference,field);
        return true;
      }
      if(key==widgetId(EditorWidget::MultiAssetRevert)) {
        for(auto &texture:multiTextures_) texture.draft=texture.saved;
        state_.status="Rascunho descartado";return true;
      }
      if(key==widgetId(EditorWidget::MultiAssetApply)) {applyMultiTextureProfiles();return true;}
    }
    if(state_.filesMultiSelect && key>=base && key-base<files_.tree().size()) {
      // Pasta no modo: só abre ou fecha, sem mexer no que está escolhido.
      if(files_.tree()[key-base].directory) {files_.toggle(key-base);return true;}
      auto paths=state_.selectedFiles;const auto path=files_.tree()[key-base].relativePath;
      if(const auto found=std::find(paths.begin(),paths.end(),path);found!=paths.end()) paths.erase(found);
      else paths.push_back(path);
      selectFiles(std::move(paths));
      return true;
    }
    if(key==widgetId(EditorWidget::ImportModel)) {state_.modelImportRequested=true;state_.codeFiles=false;return true;}
    if(key==widgetId(EditorWidget::ImportEnvironment)) {state_.environmentImportRequested=true;state_.codeFiles=false;return true;}
    if(key==widgetId(EditorWidget::ImportTexture)) {state_.textureImportRequested=true;state_.codeFiles=false;return true;}
    if(key==widgetId(EditorWidget::AssetInstantiate)) {
      const auto *record=assets_.findByPath(state_.selectedFile);
      if(record && record->type==resources::AssetType::Prefab) {
        std::string error;
        if(!instantiatePrefab(record->guid,document_.root(),error)) state_.status=error;
        return true;
      }
      ModelImportReport report;
      if(!record) setImportStatus("Registre o recurso com Reimportar antes de instanciar.",EditorConsoleSeverity::Warning);
      else if(!instantiateModel(record->guid,report)) setImportStatus(report.diagnostic,EditorConsoleSeverity::Error);
      else setImportStatus("Instância criada: "+std::to_string(report.objects)+" objetos. Desfazer remove somente esta instância.");
      return true;
    }
    if(key==widgetId(EditorWidget::AssetReimport)) {
      const auto *record=assets_.findByPath(state_.selectedFile);
      if(record&&record->type==resources::AssetType::EnvironmentMap)
        environmentReimportPath_=state_.selectedFile;
      else if(record&&record->type==resources::AssetType::Texture)
        textureReimportPath_=state_.selectedFile;
      else reimportPath_=state_.selectedFile;
      return true;
    }
    if(key==widgetId(EditorWidget::AssetExtractTextures)) {
      TextureExtraction report;std::string diagnostic;
      if(extractSourceTextures(state_.selectedFile,report,diagnostic)) setImportStatus(state_.status);
      else setImportStatus(diagnostic.empty()?std::string("Não foi possível extrair as texturas."):diagnostic,EditorConsoleSeverity::Warning);
      return true;
    }
    if(key==widgetId(EditorWidget::FilesRename)) {
      if(!state_.selectedFile.empty()) state_.renamingResource=true;
      return true;
    }
    if(key==widgetId(EditorWidget::FilesDelete)) {
      if(state_.selectedFile.empty()) return true;
      // Dois toques, nao um dialogo. O primeiro confronta quem usa e conta; o
      // segundo, no MESMO arquivo, confirma. Mudar de arquivo cancela.
      const bool force=state_.pendingResourceDelete==state_.selectedFile;
      ResourceChangeReport report;
      if(deleteResource(state_.selectedFile,force,report)) {
        state_.pendingResourceDelete.clear();
        state_.selectedFile.clear();
      } else if(!force && (report.sceneUsers>0 || report.registryDependents>0)) {
        state_.pendingResourceDelete=state_.selectedFile;
        state_.status=report.diagnostic+" Toque de novo para apagar mesmo assim.";
      } else {
        state_.pendingResourceDelete.clear();
        state_.status=report.diagnostic;
      }
      return true;
    }
    if(key>=base && key-base<files_.tree().size()) {
      const auto entry=files_.tree()[key-base];
      if(entry.directory) files_.toggle(key-base);
      else state_.selectedFiles={entry.relativePath};
      openProjectFile(entry);
      return true;
    }
  }
  if(state_.draggingAsset && event.pointerId!=assetPointer_) return true;
  if(state_.draggingEntity && event.pointerId!=hierarchyPointer_) return true;
  if(isPlaying() && routing.target==UiPointerTarget::Viewport) {
    if(event.device==UiPointerDevice::Mouse) return false;
    const auto *controlled=document_.find(state_.selection);
    const auto view=sceneCameraPose();const auto *cameraEntity=document_.find(view.entity);
    const bool canLook=cameraEntity&&cameraLook(*cameraEntity);
    if((!state_.playHasScripts && (!controlled || !characterComponent(*controlled)) && !canLook) || state_.playPaused) return true;
    const float x=event.position.x-layout_.viewport.x,y=event.position.y-layout_.viewport.y;
    if(event.phase==UiPointerPhase::Down) playTouches_.pointerDown(event.pointerId,x,y,layout_.viewport.width,layout_.viewport.height);
    else if(event.phase==UiPointerPhase::Move) playTouches_.pointerMove(event.pointerId,x,y,layout_.viewport.width,layout_.viewport.height);
    else if(event.phase==UiPointerPhase::Up) playTouches_.pointerUp(event.pointerId);
    else playTouches_.cancel();
    return true;
  }
  if(routing.tapped) for(u32 i=0;i<editorCreationCatalog.size();++i)
    if(routing.widgetId==creationWidget(i) && !creationAvailable(state_,i)) return true;
  if(routing.tapped && state_.creationMenu) {
    const auto key=routing.widgetId;
    if(key>=widgetId(EditorWidget::CreationCategoryBase)&&key<widgetId(EditorWidget::CreationCategoryBase)+std::size(creationCategories)) {
      state_.creationCategory=key-widgetId(EditorWidget::CreationCategoryBase);state_.creationScroll=0;state_.creationSearch[0]=0;
      for(u32 i=0;i<editorCreationCatalog.size();++i) if(creationAvailable(state_,i) && editorCreationCatalog[i].category==state_.creationCategory) {state_.creationSelection=i;break;}
      return true;
    }
    if(key>=widgetId(EditorWidget::CreationRowBase)&&key<widgetId(EditorWidget::CreationRowBase)+editorCreationCatalog.size()) {
      state_.creationSelection=key-widgetId(EditorWidget::CreationRowBase);return true;
    }
  }
  if (state_.renameEntity != kInvalidEntity || state_.editingHierarchySearch || state_.editingCreationSearch || state_.editingComponentSearch || state_.editingPropertySearch || state_.editingMeshSearch || state_.editingReferenceSearch || state_.editingGlobalSearch || state_.namingLayout || state_.presetNaming || state_.viewNaming || state_.editingInputActionName || state_.editingInputContext || state_.editingTagName || state_.editingTagSearch || state_.editingGroupName || state_.editingPhysicsLayerName || state_.editingAnimatorName) {
    if(routing.tapped) {
      const auto key=routing.widgetId;
      auto n=std::strlen(state_.renameText);
      if(key==widgetId(EditorWidget::NameCancel)) {completeTextEdit(pendingTextEdit(),{},false);}
      else if(key==widgetId(EditorWidget::NameClear)) state_.renameText[0]=0;
      else if(key==widgetId(EditorWidget::NameShift)) state_.renameUppercase=!state_.renameUppercase;
      else if(key==widgetId(EditorWidget::NameBackspace) && n) {
        do {--n;} while(n && (static_cast<unsigned char>(state_.renameText[n])&0xc0)==0x80);
        state_.renameText[n]=0;
      } else if(key>=widgetId(EditorWidget::NameKeyBase) && key<widgetId(EditorWidget::NameKeyBase)+40 && n+1<sizeof(state_.renameText)) {
        const char *keys=state_.renameUppercase?"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-. ":"abcdefghijklmnopqrstuvwxyz0123456789_-. ";
        state_.renameText[n]=keys[key-widgetId(EditorWidget::NameKeyBase)];state_.renameText[n+1]=0;
      } else if(key==widgetId(EditorWidget::NameApply)) {
        completeTextEdit(pendingTextEdit(),state_.renameText,true);
      }
    }
    return true;
  }
  if (state_.numericField != 0) {
    if (routing.tapped) {
      const u32 keyBase = widgetId(EditorWidget::NumericKeyBase);
      const u32 key = routing.widgetId;
      // Teclas de expressão: o texto inserido é o da sintaxe (`*`, `pi`), o
      // rótulo é o da tela (`×`, `pi`). As relativas trocam o prefixo.
      static constexpr const char *inserts[]{"1","2","3","4","5","6","7","8","9",".","0","-"," ",
                                            "+","*","/","(",")","^","pi",",","+=","-=","*=","/=","L(","R(","sqrt("};
      if (key == widgetId(EditorWidget::NumericCancel)) completeTextEdit(pendingTextEdit(),{},false);
      else if (key == widgetId(EditorWidget::NumericClear)) {state_.numericText[0] = 0;state_.numericError=false;}
      else if (key == widgetId(EditorWidget::NumericBackspace)) {
        const auto n=std::strlen(state_.numericText);if(n) state_.numericText[n-1]=0;
        state_.numericReplace=false;state_.numericError=false;
      } else if (key >= keyBase && key < keyBase+std::size(inserts)) {
        const u32 index=key-keyBase;
        if(state_.numericReplace) {state_.numericText[0]=0;state_.numericReplace=false;}
        std::string text=state_.numericText;
        if(index>=21 && index<=24) {
          if(text.size()>=2 && text[1]=='=' && std::strchr("+-*/",text[0])) text.erase(0,2);
          text.insert(0,inserts[index]);
        } else text+=inserts[index];
        if(text.size()<sizeof(state_.numericText)) std::snprintf(state_.numericText,sizeof(state_.numericText),"%s",text.c_str());
        state_.numericError=false;
      } else if (key == widgetId(EditorWidget::NumericApply)) {
        completeTextEdit(pendingTextEdit(),state_.numericText,true);
      }
    }
    return true;
  }
  if(routing.tapped && (routing.widgetId==widgetId(EditorWidget::CreateGround) || routing.widgetId==widgetId(EditorWidget::CreateCube))) {
    const bool ground=routing.widgetId==widgetId(EditorWidget::CreateGround);
    const auto parent=state_.creationMenu && state_.creationAsChild && document_.exists(state_.selection) && state_.selection!=document_.root()?state_.selection:document_.root();
    for(u32 i=0;i<mapScene_.assetCount();++i) if(mapScene_.materialFlagsForAsset(i)&renderer::BoxAuthoringResource) {
      const float position[]{camera_.target[0],ground?-.1f:.5f,camera_.target[2]};
      EditorAssetInstantiation options;options.name=ground?"Chão":"Cubo";
      if(ground) {options.scale[0]=20;options.scale[1]=.2f;options.scale[2]=20;}
      if(instantiateAsset(i,parent,position,&options)) {state_.creationMenu=false;frameSelection();}
      break;
    }
    return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::QualityOpen)) {
    state_.qualityPanel=!state_.qualityPanel;return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::QualityTemporalDebugQuick)) {
    if(state_.qualityTemporalAvailable && state_.workspace==EditorWorkspace::Scene)
      state_.qualityTemporalDebug=nextTemporalDebugView();
    return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::QualityTemporalModeQuick)) {
    // Controle rápido: aplica na hora, sobre o que está salvo, e pula o que o
    // aparelho recusa dizendo por quê.
    using Mode=renderer::TemporalReconstruction;
    auto next=renderingSettings_;
    auto mode=renderer::temporalReconstruction(next);
    std::string skipped;
    for(u32 attempt=0;attempt<4;++attempt) {
      mode=static_cast<Mode>((static_cast<u32>(mode)+1u)%4u);
      const auto availability=mode==Mode::ArmAsr?state_.qualityArmAsr:mode==Mode::Fsr2?state_.qualityFsr2:
                              renderer::TemporalUpscalerAvailability::Available;
      if(availability==renderer::TemporalUpscalerAvailability::Available) break;
      if(!skipped.empty()) skipped+="; ";
      skipped+=std::string(renderer::temporalReconstructionLabel(mode))+" indisponível: "+
               renderer::temporalUpscalerAvailabilityLabel(availability);
    }
    if(renderingSettingsRequestInFlight_ || renderingSettingsRequested_) {
      state_.status="Aguarde a aplicação anterior da qualidade";return true;
    }
    renderer::setTemporalReconstruction(next,mode);
    requestedRenderingSettings_=next;
    renderingSettingsRequestRevision_=qualityRevision_;
    renderingSettingsRequested_=true;
    if(!state_.qualityDirty) state_.qualityDraft=next;
    state_.status=std::string("Ampliação temporal: ")+renderer::temporalReconstructionLabel(mode)+
                  (skipped.empty()?std::string():" ("+skipped+")");
    return true;
  }
  if(state_.lightExplorer && routing.tapped) {
    const auto key=routing.widgetId;
    const auto is=[&](EditorWidget widget) {return key==widgetId(widget);};
    // Degraus da intensidade em lote: cobrem lux de interior a sol e lúmen de
    // vela a holofote, na unidade de cada luz.
    static constexpr float steps[]{0,1,5,10,50,100,250,500,1000,2500,5000,10000,25000,50000,100000};
    if(is(EditorWidget::LightExplorerClose)) {state_.lightExplorer=false;return true;}
    if(is(EditorWidget::LightExplorerFilter)) {
      state_.lightExplorerDarkOnly=!state_.lightExplorerDarkOnly;state_.lightExplorerPage=0;refreshLightExplorer();return true;
    }
    if(is(EditorWidget::LightExplorerPrevious)) {
      if(state_.lightExplorerPage) --state_.lightExplorerPage;
      refreshLightExplorer();return true;
    }
    if(is(EditorWidget::LightExplorerNext)) {++state_.lightExplorerPage;refreshLightExplorer();return true;}
    if(is(EditorWidget::LightExplorerIntensityDown) || is(EditorWidget::LightExplorerIntensityUp)) {
      usize index=0;
      for(usize i=0;i<std::size(steps);++i) if(steps[i]<=state_.lightExplorerIntensity) index=i;
      if(is(EditorWidget::LightExplorerIntensityUp)) index=std::min(index+1,std::size(steps)-1);
      else if(index && steps[index]==state_.lightExplorerIntensity) --index;
      state_.lightExplorerIntensity=steps[index];return true;
    }
    if(is(EditorWidget::LightExplorerApply)) {
      const u32 changed=applyLightExplorerIntensity();
      state_.status=changed?std::to_string(changed)+" luz(es) com a intensidade nova":std::string("Nenhuma luz mudou");
      return true;
    }
    if(key>=widgetId(EditorWidget::LightExplorerRow0) && key<=widgetId(EditorWidget::LightExplorerRowLast)) {
      const u32 row=key-widgetId(EditorWidget::LightExplorerRow0);
      if(row<state_.lightExplorerRows.size()) setSelection(state_.lightExplorerRows[row].entity);
      return true;
    }
    if(key>=widgetId(EditorWidget::LightExplorerToggle0) && key<=widgetId(EditorWidget::LightExplorerToggleLast)) {
      const u32 row=key-widgetId(EditorWidget::LightExplorerToggle0);
      if(row>=state_.lightExplorerRows.size() || isPlaying() || history_.isOpen()) return true;
      const auto &target=state_.lightExplorerRows[row];
      if(const auto *entity=document_.find(target.entity)) {
        auto values=*entity;
        if(scene::setComponentProperty(values.components,scene::Light::descriptor.id,"enabled",!target.enabled,
                                       target.instance)==scene::ComponentPropertyStatus::Applied)
          history_.applyValues(document_,target.entity,values);
      }
      refreshLightExplorer();return true;
    }
  }
  if(state_.qualityPanel && routing.tapped) {
    auto &draft=state_.qualityDraft;
    const auto key=routing.widgetId;
    const auto is=[&](EditorWidget widget) {return key==widgetId(widget);};
    const auto cycleOverride=[](renderer::FeatureOverride value) {
      using Override=renderer::FeatureOverride;
      return value==Override::Inherit?Override::Disabled:value==Override::Disabled?Override::Enabled:Override::Inherit;
    };
    // Próximo/anterior numa escada fixa; valor fora dela volta ao degrau mais perto abaixo.
    const auto stepList=[](const auto &steps,u32 value,bool up) {
      usize index=0;
      for(usize i=0;i<std::size(steps);++i) if(steps[i]<=value) index=i;
      if(up) return steps[std::min(index+1,std::size(steps)-1)];
      return steps[index==0||steps[index]!=value?index:index-1];
    };
    const auto step=[](float value,float inherited,float initial,float delta,float minimum,float maximum,bool up) {
      const float current=value==inherited?initial:value;
      return std::round(std::clamp(current+(up?delta:-delta),minimum,maximum)/delta)*delta;
    };
    bool changed=true;
    if(is(EditorWidget::QualityClose)) {state_.qualityPanel=false;return true;}
    else if(is(EditorWidget::QualityTabGeneral)) {state_.qualityTab=0;state_.qualityPage=0;return true;}
    else if(is(EditorWidget::QualityTabShadows)) {state_.qualityTab=1;state_.qualityPage=0;return true;}
    else if(is(EditorWidget::QualityTabLighting)) {state_.qualityTab=2;state_.qualityPage=0;return true;}
    else if(is(EditorWidget::QualityTabPerformance)) {state_.qualityTab=3;state_.qualityPage=0;return true;}
    else if(is(EditorWidget::QualityTabTextures)) {state_.qualityTab=4;state_.qualityPage=0;return true;}
    else if(is(EditorWidget::LightExplorerOpen)) {
      state_.lightExplorer=true;state_.qualityPanel=false;state_.lightExplorerPage=0;refreshLightExplorer();return true;
    }
    else if(is(EditorWidget::QualitySceneStatistics)) {
      state_.sceneStatisticsVisible=!state_.sceneStatisticsVisible;return true;
    }
    else if(is(EditorWidget::QualityStreamingDebugView)) {
      state_.qualityTextureStreamingDebug=!state_.qualityTextureStreamingDebug;return true;
    }
    else if(is(EditorWidget::QualityPagePrevious)) {if(state_.qualityPage) --state_.qualityPage;return true;}
    else if(is(EditorWidget::QualityPageNext)) {++state_.qualityPage;return true;}
    else if(is(EditorWidget::QualityTemporalDebug)) {
      if(state_.qualityTemporalAvailable) state_.qualityTemporalDebug=nextTemporalDebugView();
      return true;
    }
    else if(is(EditorWidget::QualityTemporalMode)) {
      using Mode=renderer::TemporalReconstruction;
      const auto mode=renderer::temporalReconstruction(draft);
      renderer::setTemporalReconstruction(draft,static_cast<Mode>((static_cast<u32>(mode)+1u)%4u));
    } else if(is(EditorWidget::QualityTemporalQuality)) {
      using Quality=renderer::TemporalUpscalerQuality;
      draft.temporalUpscalerQuality=draft.temporalUpscalerQuality==Quality::Inherit?Quality::Quality:
          draft.temporalUpscalerQuality==Quality::Quality?Quality::Balanced:
          draft.temporalUpscalerQuality==Quality::Balanced?Quality::Performance:
          draft.temporalUpscalerQuality==Quality::Performance?Quality::UltraPerformance:Quality::Inherit;
    }
    else if(is(EditorWidget::QualityLevel)) {
      // Automático, Baixo, Médio, Alto, Ultra: a ordem dos níveis da Unity.
      using Preset=renderer::QualityPreset;
      draft.preset=draft.preset==Preset::Auto?Preset::C:draft.preset==Preset::C?Preset::B:
                   draft.preset==Preset::B?Preset::A:draft.preset==Preset::A?Preset::S:Preset::Auto;
    } else if(is(EditorWidget::QualityScaleDown) || is(EditorWidget::QualityScaleUp)) {
      // Passos de 5%, entre 50% e 100%, como o Render Scale do URP Asset.
      const float current=draft.resolutionScale>0?draft.resolutionScale:1.0f;
      if(is(EditorWidget::QualityScaleUp)&&current>=1.0f) draft.resolutionScale=0.0f;
      else {
        const float next=std::clamp(current+(is(EditorWidget::QualityScaleUp)?.05f:-.05f),.5f,1.0f);
        draft.resolutionScale=std::round(next*20.0f)/20.0f;
      }
    } else if(is(EditorWidget::QualityDynamic)) {
      draft.dynamicResolution=cycleOverride(draft.dynamicResolution);
    } else if(is(EditorWidget::QualityUpscaling)) {
      using Filter=renderer::UpscalingFilter;
      if(renderer::isTemporalUpscaler(draft.upscalingFilter)) return true; // o temporal substitui o espacial
      draft.upscalingFilter=draft.upscalingFilter==Filter::Inherit?Filter::Bilinear:
                            draft.upscalingFilter==Filter::Bilinear?Filter::CatmullRom:
                            draft.upscalingFilter==Filter::CatmullRom?Filter::Fsr1:Filter::Inherit;
    } else if(is(EditorWidget::QualityTextures)) {
      using Quality=renderer::TextureQuality;
      draft.textures=draft.textures==Quality::Inherit?Quality::Half:
                     draft.textures==Quality::Half?Quality::Full:Quality::Inherit;
    } else if(is(EditorWidget::QualityAntiAliasing)) {
      using Mode=renderer::AntiAliasingMode;
      if(renderer::isTemporalUpscaler(draft.upscalingFilter)) return true; // AA pertence ao ampliador
      draft.antiAliasing=draft.antiAliasing==Mode::Inherit?Mode::Off:draft.antiAliasing==Mode::Off?Mode::Fxaa:
                         draft.antiAliasing==Mode::Fxaa?Mode::Temporal:Mode::Inherit;
    } else if(is(EditorWidget::QualitySharpenDown) || is(EditorWidget::QualitySharpenUp)) {
      const float current=draft.postSharpen>=0?draft.postSharpen:0.0f;
      if(is(EditorWidget::QualitySharpenDown)&&draft.postSharpen==0.0f) draft.postSharpen=-1.0f;
      else draft.postSharpen=std::round(std::clamp(current+(is(EditorWidget::QualitySharpenUp)?.1f:-.1f),0.0f,1.0f)*10.0f)/10.0f;
    } else if(is(EditorWidget::QualityRate)) {
      draft.maximumRenderHz=draft.maximumRenderHz==30?60:draft.maximumRenderHz==60?90:
                            draft.maximumRenderHz==90?120:draft.maximumRenderHz==120?0:30;
    } else if(is(EditorWidget::QualityShadows)) {
      using Quality=renderer::ShadowQuality;
      draft.shadows=draft.shadows==Quality::Inherit?Quality::Off:draft.shadows==Quality::Off?Quality::Hard:
                    draft.shadows==Quality::Hard?Quality::Soft:draft.shadows==Quality::Soft?Quality::UltraSoft:Quality::Inherit;
    } else if(is(EditorWidget::QualityShadowCascades)) {
      draft.shadowCascadeCount=draft.shadowCascadeCount>=4?0:draft.shadowCascadeCount+1;
    } else if(is(EditorWidget::QualityShadowResolution)) {
      draft.shadowCascadeResolution=draft.shadowCascadeResolution==0?512:draft.shadowCascadeResolution==512?1024:
                                    draft.shadowCascadeResolution==1024?2048:draft.shadowCascadeResolution==2048?4096:0;
    } else if(is(EditorWidget::QualityShadowDistanceDown)||is(EditorWidget::QualityShadowDistanceUp)) {
      draft.shadowMaximumDistance=step(draft.shadowMaximumDistance,0.0f,160.0f,20.0f,20.0f,1000.0f,is(EditorWidget::QualityShadowDistanceUp));
    } else if(is(EditorWidget::QualityShadowBiasDown)||is(EditorWidget::QualityShadowBiasUp)) {
      draft.shadowDepthBiasConstant=step(draft.shadowDepthBiasConstant,-1.0f,1.0f,.1f,0.0f,10.0f,is(EditorWidget::QualityShadowBiasUp));
    } else if(is(EditorWidget::QualityShadowSlopeDown)||is(EditorWidget::QualityShadowSlopeUp)) {
      draft.shadowDepthBiasSlope=step(draft.shadowDepthBiasSlope,-1.0f,1.5f,.1f,0.0f,10.0f,is(EditorWidget::QualityShadowSlopeUp));
    } else if(is(EditorWidget::QualityShadowNormalDown)||is(EditorWidget::QualityShadowNormalUp)) {
      draft.shadowNormalOffsetTexels=step(draft.shadowNormalOffsetTexels,-1.0f,1.0f,.1f,0.0f,10.0f,is(EditorWidget::QualityShadowNormalUp));
    } else if(is(EditorWidget::QualityShadowCache)) {
      draft.staticShadowCache=cycleOverride(draft.staticShadowCache);
    } else if(is(EditorWidget::QualityAmbient)) {
      using Quality=renderer::AmbientQuality;
      draft.ambient=draft.ambient==Quality::Inherit?Quality::Constant:draft.ambient==Quality::Constant?Quality::Hemispheric:
                    draft.ambient==Quality::Hemispheric?Quality::HemisphericSpecular:Quality::Inherit;
    } else if(is(EditorWidget::QualityEnvironmentBrdf)) {
      draft.environmentSplitSumBrdf=cycleOverride(draft.environmentSplitSumBrdf);
    } else if(is(EditorWidget::QualityPost)) {
      using Quality=renderer::PostQuality;
      draft.post=draft.post==Quality::Inherit?Quality::None:draft.post==Quality::None?Quality::Tonemap:
                 draft.post==Quality::Tonemap?Quality::Bloom:Quality::Inherit;
    } else if(is(EditorWidget::QualityBloomThresholdDown)||is(EditorWidget::QualityBloomThresholdUp)) {
      draft.bloomThreshold=step(draft.bloomThreshold,-1.0f,1.0f,.1f,0.0f,8.0f,is(EditorWidget::QualityBloomThresholdUp));
    } else if(is(EditorWidget::QualityBloomIntensityDown)||is(EditorWidget::QualityBloomIntensityUp)) {
      draft.bloomIntensity=step(draft.bloomIntensity,-1.0f,.25f,.05f,0.0f,2.0f,is(EditorWidget::QualityBloomIntensityUp));
    } else if(is(EditorWidget::QualityTemporalWeightDown)||is(EditorWidget::QualityTemporalWeightUp)) {
      draft.temporalHistoryWeight=step(draft.temporalHistoryWeight,-1.0f,.88f,.01f,0.0f,.97f,is(EditorWidget::QualityTemporalWeightUp));
    } else if(is(EditorWidget::QualityVignette)) {
      draft.postVignette=cycleOverride(draft.postVignette);
    } else if(is(EditorWidget::QualityDynamicMinimumDown)||is(EditorWidget::QualityDynamicMinimumUp)) {
      draft.dynamicResolutionMinimumScale=step(draft.dynamicResolutionMinimumScale,0.0f,.70f,.05f,.50f,1.0f,is(EditorWidget::QualityDynamicMinimumUp));
    } else if(is(EditorWidget::QualityLodSelection)) {
      draft.lodSelection=cycleOverride(draft.lodSelection);
    } else if(is(EditorWidget::QualityLodErrorDown)||is(EditorWidget::QualityLodErrorUp)) {
      draft.lodPixelErrorBudget=step(draft.lodPixelErrorBudget,0.0f,1.0f,.25f,.25f,16.0f,is(EditorWidget::QualityLodErrorUp));
    } else if(is(EditorWidget::QualityLodHysteresisDown)||is(EditorWidget::QualityLodHysteresisUp)) {
      draft.lodHysteresisBandRatio=step(draft.lodHysteresisBandRatio,0.0f,.75f,.05f,.10f,.95f,is(EditorWidget::QualityLodHysteresisUp));
    } else if(is(EditorWidget::QualityMaterialVariants)) {
      draft.materialShaderVariants=cycleOverride(draft.materialShaderVariants);
    } else if(is(EditorWidget::QualityTextureStreaming)) {
      draft.textureStreaming=cycleOverride(draft.textureStreaming);
    } else if(is(EditorWidget::QualityStreamingBudgetDown)||is(EditorWidget::QualityStreamingBudgetUp)) {
      // Passos do Memory Budget; abaixo do menor volta a "Do nível".
      static constexpr u32 steps[]{0,32,64,128,256,384,512,768,1024,1536,2048,3072,4096};
      draft.textureStreamingBudgetMegabytes=stepList(steps,draft.textureStreamingBudgetMegabytes,is(EditorWidget::QualityStreamingBudgetUp));
    } else if(is(EditorWidget::QualityStreamingReductionDown)||is(EditorWidget::QualityStreamingReductionUp)) {
      static constexpr u32 steps[]{0,1,2,3,4,5,6,7};
      draft.textureStreamingMaxLevelReduction=stepList(steps,draft.textureStreamingMaxLevelReduction,is(EditorWidget::QualityStreamingReductionUp));
    } else if(is(EditorWidget::QualityStreamingUploadDown)||is(EditorWidget::QualityStreamingUploadUp)) {
      static constexpr u32 steps[]{0,512,1024,2048,4096,8192,16384,32768};
      draft.textureStreamingUploadKilobytesPerFrame=stepList(steps,draft.textureStreamingUploadKilobytesPerFrame,is(EditorWidget::QualityStreamingUploadUp));
    } else if(is(EditorWidget::QualityApply)) {
      // Um ampliador temporal que o aparelho recusa não é salvo como se fosse
      // funcionar: o motivo aparece e o rascunho continua pendente.
      if(renderer::isTemporalUpscaler(draft.upscalingFilter)) {
        const auto availability=draft.upscalingFilter==renderer::UpscalingFilter::ArmAsr?state_.qualityArmAsr:state_.qualityFsr2;
        if(availability!=renderer::TemporalUpscalerAvailability::Available) {
          state_.status=std::string(renderer::upscalingFilterLabel(draft.upscalingFilter))+" indisponível: "+
                        renderer::temporalUpscalerAvailabilityLabel(availability);
          return true;
        }
      }
      requestedRenderingSettings_=draft;
      renderingSettingsRequestRevision_=qualityRevision_;
      renderingSettingsRequested_=true;
      state_.status="Salvando qualidade do projeto";
      return true;
    } else changed=false;
    if(changed) {
      ++qualityRevision_;
      state_.qualityDirty=true;
      return true;
    }
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::CreateSceneTemplate)) {
    state_.templatePanel=true;state_.creationMenu=false;return true;
  }
  if(state_.templatePanel && routing.tapped) {
    if(routing.widgetId==widgetId(EditorWidget::SceneTemplateClose)) {state_.templatePanel=false;return true;}
    if(routing.widgetId>=widgetId(EditorWidget::SceneTemplateRowBase) &&
       routing.widgetId<widgetId(EditorWidget::SceneTemplateRowBase)+sceneTemplates().size()) {
      createSceneTemplate(routing.widgetId-widgetId(EditorWidget::SceneTemplateRowBase));
      state_.templatePanel=false;return true;
    }
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::ImportFolder)) {
    // Pasta inteira: o shell lista a pasta, e só os arquivos que o glTF
    // referencia são copiados para o projeto.
    state_.folderImportRequested=true;state_.creationMenu=false;state_.codeFiles=false;
    state_.status="Escolha a pasta do modelo (.gltf com .bin e texturas)";
    return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::ImportModel)) {
    // O editor não conhece Android nem o seletor de arquivos: levanta o pedido
    // e quem tem o sistema na mão abre o diálogo e devolve os bytes.
    state_.modelImportRequested=true;state_.creationMenu=false;
    state_.status="Escolha um arquivo .glb";
    return true;
  }
  if(routing.tapped && routing.widgetId>=widgetId(EditorWidget::CreationRecipeBase) &&
     routing.widgetId<widgetId(EditorWidget::CreationRecipeBase)+editorCreationCatalog.size()) {
    const auto parent=state_.creationMenu && state_.creationAsChild && document_.exists(state_.selection) &&
        state_.selection!=document_.root()?state_.selection:document_.root();
    if(createRecipe(routing.widgetId-widgetId(EditorWidget::CreationRecipeBase),parent)!=kInvalidEntity)
      state_.creationMenu=false;
    return true;
  }
  if(routing.tapped && (routing.widgetId==widgetId(EditorWidget::CreateFiniteWater) ||
                        routing.widgetId==widgetId(EditorWidget::CreateOceanWater))) {
    const auto parent=state_.creationMenu && state_.creationAsChild && document_.exists(state_.selection) && state_.selection!=document_.root()?state_.selection:document_.root();
    state_.creationMenu=false;
    createWaterSurface(routing.widgetId==widgetId(EditorWidget::CreateOceanWater),parent);
    return true;
  }
  if(routing.tapped && (routing.widgetId==widgetId(EditorWidget::CreateRiverWater) || routing.widgetId==widgetId(EditorWidget::CreateBuoyantBox))) {
    const bool box=routing.widgetId==widgetId(EditorWidget::CreateBuoyantBox);
    const auto parent=state_.creationMenu && state_.creationAsChild && document_.exists(state_.selection) && state_.selection!=document_.root()?state_.selection:document_.root();
    for(u32 i=0;i<mapScene_.assetCount();++i) if(mapScene_.materialFlagsForAsset(i)&(box?renderer::BoxAuthoringResource:renderer::WaterRouteResource)) {
      const float position[]{camera_.target[0],box?3.0f:0.0f,camera_.target[2]};
      if(instantiateAsset(i,parent,position)) {
        state_.workspace=EditorWorkspace::Scene;state_.waterTab=1;state_.routePoint=0;state_.creationMenu=false;frameSelection();
      }
      break;
    }
    return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::CreateGroup)) state_.creationMenu=false;
  if(routing.tapped && routing.widgetId>=widgetId(EditorWidget::RoutePointAdd) && routing.widgetId<=widgetId(EditorWidget::RoutePointNext)) {
    const auto *selected=document_.find(state_.selection);
    if(selected && waterRoute(*selected).count) {
      auto value=*selected;auto *editableRoute=editWaterRoute(value);if(!editableRoute) return false;auto &route=*editableRoute;state_.routePoint=std::min(state_.routePoint,route.count-1);
      const auto widget=static_cast<EditorWidget>(routing.widgetId);
      if(widget==EditorWidget::RoutePointPrevious) state_.routePoint=state_.routePoint?state_.routePoint-1:0;
      if(widget==EditorWidget::RoutePointNext) state_.routePoint=std::min(state_.routePoint+1,route.count-1);
      if(widget==EditorWidget::RoutePointAdd && route.count<renderer::MaximumWaterRoutePoints) {
        const u32 point=state_.routePoint;
        for(u32 i=route.count;i>point+1;--i) route.points[i]=route.points[i-1];
        route.points[point+1]=route.points[point];
        if(point+1<route.count) {
          for(u32 axis=0;axis<3;++axis) route.points[point+1].position[axis]=(route.points[point].position[axis]+route.points[point+2].position[axis])*.5f;
        } else {
          for(u32 axis=0;axis<3;++axis) route.points[point+1].position[axis]+=route.points[point].position[axis]-route.points[point-1].position[axis];
        }
        ++route.count;if(history_.applyValues(document_,selected->id,value)) ++state_.routePoint;
      }
      if(widget==EditorWidget::RoutePointRemove && route.count>2) {
        for(u32 i=state_.routePoint;i+1<route.count;++i) route.points[i]=route.points[i+1];
        --route.count;if(history_.applyValues(document_,selected->id,value)) state_.routePoint=std::min(state_.routePoint,route.count-1);
      }
      state_.propertyPage=0;
    }
    return true;
  }
  if(routing.widgetId>=widgetId(EditorWidget::RoutePointBase) && routing.widgetId<widgetId(EditorWidget::RoutePointBase)+renderer::MaximumWaterRoutePoints) {
    const auto *selected=document_.find(state_.selection);const u32 point=routing.widgetId-widgetId(EditorWidget::RoutePointBase);
    if(selected && point<waterRoute(*selected).count) {
      state_.routePoint=point;
      if(event.phase==ui::UiPointerPhase::Down && !history_.isOpen()) {history_.begin("Mover ponto do rio");fieldWidget_=routing.widgetId;fieldPointer_=event.pointerId;fieldInitial_=*selected;dragEntity_=selected->id;}
      if(routing.dragging && event.phase==UiPointerPhase::Move && fieldWidget_==routing.widgetId && fieldPointer_==event.pointerId) {
        float world[16],normal[12];
        const auto ray=screenPointToRay(view_,event.position);
        if(editorWorldMatrix(document_,selected->id,world) && renderer::buildNormalMatrix(world,normal) && ray.valid && std::abs(ray.direction[1])>1e-5f) {
          const auto &p=waterRoute(fieldInitial_).points[point];const float height=world[13]+world[1]*p.position[0]+world[5]*p.position[1]+world[9]*p.position[2];
          const float distance=(height-ray.origin[1])/ray.direction[1];
          if(distance>0) {
            float offset[3];for(u32 axis=0;axis<3;++axis) offset[axis]=ray.origin[axis]+distance*ray.direction[axis]-world[12+axis];
            auto value=*selected;
            auto *route=editWaterRoute(value);if(!route) return false;
            for(u32 axis=0;axis<3;++axis) route->points[point].position[axis]=normal[axis*4]*offset[0]+normal[axis*4+1]*offset[1]+normal[axis*4+2]*offset[2];
            history_.applyValues(document_,selected->id,value,routing.widgetId);
          }
        }
      }
      if(routing.released && fieldWidget_==routing.widgetId && fieldPointer_==event.pointerId) {history_.end();fieldWidget_=0;}
    }
    return true;
  }
  const u32 assetBase=widgetId(EditorWidget::AssetRowBase);
  if(routing.target==UiPointerTarget::Widget && routing.widgetId>=assetBase &&
      routing.widgetId<assetBase+mapScene_.assetCount()) {
    const u32 index=routing.widgetId-assetBase;
    if(routing.dragging && event.phase==UiPointerPhase::Move && !history_.isOpen() &&
       (!state_.draggingAsset || assetPointer_==event.pointerId)) {
      assetPointer_=event.pointerId;state_.draggingAsset=true;
      state_.assetDragPosition=event.position;state_.workspace=EditorWorkspace::Scene;
      state_.status="Solte na cena ou sobre um pai na hierarquia";
    }
    if(routing.tapped && !state_.draggingAsset) {
      if(instantiateAsset(index,document_.root(),camera_.target)) {
        state_.workspace=EditorWorkspace::Scene;frameSelection();
      }
    } else if(routing.released && state_.draggingAsset && assetPointer_==event.pointerId) {
      state_.draggingAsset=false;
      state_.status="Arraste cancelado";
      if(event.phase==UiPointerPhase::Up && !history_.isOpen()) {
        const auto target=router_.hitTest(event.position);
        const u32 rows=widgetId(EditorWidget::HierarchyRowBase);
        if(target.target==UiPointerTarget::Widget && dropAssetOnField(target.widgetId,index)) {
          state_.status="Recurso atribuído pelo arraste";return true;
        }
        if(target.target==UiPointerTarget::Widget && target.widgetId>=rows &&
           target.widgetId<rows+EditorDocument::kMaximumEntities) {
          const auto parent=target.widgetId-rows;
          float world[16];
          if(editorWorldMatrix(document_,parent,world)) instantiateAsset(index,parent,world+12);
        } else if(target.target==UiPointerTarget::Viewport && layout_.viewport.contains(event.position)) {
          const auto ray=screenPointToRay(view_,event.position);
          if(ray.valid && std::abs(ray.direction[1])>1e-5f) {
            const float distance=(camera_.target[1]-ray.origin[1])/ray.direction[1];
            if(distance>view_.frustum.nearPlane && distance<view_.frustum.farPlane) {
              float point[3];for(u32 a=0;a<3;++a) point[a]=ray.origin[a]+distance*ray.direction[a];
              instantiateAsset(index,document_.root(),point);
            }
          }
        }
      }
    }
    return true;
  }  const u32 fieldBase = widgetId(EditorWidget::TransformFieldBase);
  if (routing.target == UiPointerTarget::Widget && routing.widgetId >= fieldBase &&
      routing.widgetId < fieldBase + editorNumericProperties.size()) {
    if (event.phase == UiPointerPhase::Down && fieldWidget_ == 0 && !history_.isOpen()) {
      const u32 index=routing.widgetId-fieldBase;
      const auto *entity = document_.find((editorNumericProperties[index].group==EditorPropertyGroup::Environment || editorNumericProperties[index].group==EditorPropertyGroup::Water)?document_.root():state_.selection);
      if (entity && history_.begin(editorNumericProperties[index].name)) {
        fieldInitial_ = *entity;
        dragEntity_ = entity->id;
        fieldPointer_ = event.pointerId;
        fieldWidget_ = routing.widgetId;
      }
    }
    if (fieldWidget_ == routing.widgetId && fieldPointer_ == event.pointerId) {
      const u32 field = fieldWidget_ - fieldBase;
      if (routing.dragging) {
        auto value = fieldInitial_;
        const auto &property=editorNumericProperties[field];
        const float number=std::clamp(editorPropertyValue(fieldInitial_,field)+routing.totalDelta.x*property.dragStep,
                                      property.minimum,property.maximum);
        if(setEditorPropertyValue(value,field,number)) history_.applyValues(document_,dragEntity_,value,fieldWidget_);
      }
      if (routing.released) {
        history_.end();
        if(routing.tapped) {
          state_.numericField=fieldWidget_;state_.numericEntity=dragEntity_;state_.numericInstance=0;state_.numericProperty.clear();
          const float value=editorPropertyValue(fieldInitial_,field);
          std::snprintf(state_.numericText,sizeof(state_.numericText),"%.9g",static_cast<double>(value));
          state_.numericReplace=true;state_.numericError=false;
        }
        fieldWidget_=0;
      }
    }
    return true;
  }
  // A second finger must not switch tools or history during an active edit.
  if (history_.isOpen() && (fieldWidget_ != 0 || gizmoTransactionOpen_) &&
      routing.target == UiPointerTarget::Widget &&
      routing.widgetId < widgetId(EditorWidget::GizmoAxisBase)) return true;
  if (routing.tapped && routing.widgetId == widgetId(EditorWidget::ViewsOpen)) {
    state_.viewsPanel=!state_.viewsPanel;state_.sceneLayersPanel=false;return true;
  }
  if (routing.tapped && routing.widgetId == widgetId(EditorWidget::SceneLayersOpen)) {
    state_.sceneLayersPanel=!state_.sceneLayersPanel;state_.viewsPanel=false;state_.sceneLayersPage=0;return true;
  }
  if (state_.sceneLayersPanel && routing.tapped) {
    const u32 key=routing.widgetId;
    const u32 hidden=state_.hiddenLayers,locked=state_.unpickableLayers;
    if(key==widgetId(EditorWidget::SceneLayersClose)) {state_.sceneLayersPanel=false;return true;}
    if(key==widgetId(EditorWidget::SceneLayersPrevious)) {if(state_.sceneLayersPage) --state_.sceneLayersPage;return true;}
    if(key==widgetId(EditorWidget::SceneLayersNext)) {++state_.sceneLayersPage;return true;}
    if(key==widgetId(EditorWidget::SceneLayersShowAll)) state_.hiddenLayers=0;
    else if(key==widgetId(EditorWidget::SceneLayersHideAll)) state_.hiddenLayers=0xffffffffu;
    else if(key==widgetId(EditorWidget::SceneLayersPickAll)) state_.unpickableLayers=0;
    else if(key>=widgetId(EditorWidget::SceneLayerVisibleBase) && key<widgetId(EditorWidget::SceneLayerVisibleBase)+32)
      state_.hiddenLayers^=1u<<(key-widgetId(EditorWidget::SceneLayerVisibleBase));
    else if(key>=widgetId(EditorWidget::SceneLayerPickBase) && key<widgetId(EditorWidget::SceneLayerPickBase)+32)
      state_.unpickableLayers^=1u<<(key-widgetId(EditorWidget::SceneLayerPickBase));
    if(hidden!=state_.hiddenLayers || locked!=state_.unpickableLayers) {
      // O desenho muda sem a cena mudar de revisão: o shell republica.
      if(hidden!=state_.hiddenLayers) appearanceChanged_=true;
      // O que ficou escondido ou sem seleção sai da seleção da vista.
      if(const auto *selected=document_.find(state_.selection);
         selected && selected->layer<32 && ((state_.hiddenLayers|state_.unpickableLayers)&(1u<<selected->layer)) &&
         !((hidden|locked)&(1u<<selected->layer))) state_.status="O selecionado está numa camada escondida ou sem seleção";
      saveEditorPreferences();
      return true;
    }
  }
  if (state_.viewsPanel && routing.tapped) {
    const auto key=routing.widgetId;
    if(key==widgetId(EditorWidget::ViewsClose)) {state_.viewsPanel=false;return true;}
    if(key>=widgetId(EditorWidget::SceneViewRowBase) &&
       key<widgetId(EditorWidget::SceneViewRowBase)+runtime::SceneViews::kMaximum) {
      if(!applySceneView(key-widgetId(EditorWidget::SceneViewRowBase))) state_.status="Vista indisponível";
      return true;
    }
    if(key==widgetId(EditorWidget::ViewSave) || key==widgetId(EditorWidget::ViewRename)) {
      if(isPlaying() || history_.isOpen()) {state_.status="Finalize a edição antes de mexer nas vistas.";return true;}
      state_.viewRenaming=key==widgetId(EditorWidget::ViewRename);
      const auto *chosen=document_.views().at(state_.viewSelected);
      state_.viewName=state_.viewRenaming&&chosen?chosen->name:
          "Vista "+std::to_string(document_.views().count()+1);
      std::snprintf(state_.renameText,sizeof(state_.renameText),"%s",state_.viewName.c_str());
      state_.viewNaming=true;return true;
    }
    if(key==widgetId(EditorWidget::ViewUpdate)) {
      const auto *chosen=document_.views().at(state_.viewSelected);
      // Atualizar guarda a vista ATUAL com o nome que já existe: é o gesto de
      // "esta é a comparação daqui em diante".
      state_.status=chosen&&saveSceneView(chosen->name)?"Vista atualizada com o enquadramento atual":
          "Não foi possível atualizar a vista";
      return true;
    }
    if(key==widgetId(EditorWidget::ViewDelete)) {
      state_.status=deleteSceneView(state_.viewSelected)?"Vista excluída":"Não foi possível excluir a vista";
      return true;
    }
  }
  if (routing.tapped && routing.widgetId == widgetId(EditorWidget::FrameSelection)) {
    state_.inspectorMenu=false;
    frameSelection(); return true;
  }
  if (routing.tapped && routing.widgetId == widgetId(EditorWidget::FrameAll)) {
    frameAll(); return true;
  }
  if (routing.target == UiPointerTarget::Viewport) {
    if(gizmoTransactionOpen_ || fieldWidget_) return true;
    return handleViewportPointer(event, routing);
  }
  if (routing.target == UiPointerTarget::None) return true;

  const u32 gizmoBase = widgetId(EditorWidget::GizmoAxisBase);
  if (routing.widgetId >= gizmoBase && routing.widgetId < gizmoBase + 6) {
    if(!viewportPointers_.empty() || fieldWidget_) return true;
    if(event.phase==UiPointerPhase::Down && !gizmoDrag_.active) gizmoPointer_=event.pointerId;
    if(event.pointerId!=gizmoPointer_) return true;
    handleGizmoPointer(routing, routing.widgetId - gizmoBase);
    return true;
  }

  // Inspectors focados. No escopo do Inspector, "a seleção" é o alvo dele.
  if(routing.tapped) {
    const u32 key=routing.widgetId;
    if(key==widgetId(EditorWidget::MotorSetupOpen)||key==widgetId(EditorWidget::MotorSetupPreserve)||key==widgetId(EditorWidget::MotorSetupFit)||key==widgetId(EditorWidget::MotorSetupConvex)||key==widgetId(EditorWidget::MotorSetupDecompose)) {
      if(key==widgetId(EditorWidget::MotorSetupOpen)){
        state_.motorSetupTarget=state_.selection;state_.motorSetupPolicy=0;
        if(const auto *e=document_.find(state_.selection))if(const auto *recipe=scene::collisionRecipe(e->components);recipe&&!recipe->parts.empty()) {
          state_.motorSetupPolicy=3;motorBakeSettings_=recipe->settings;state_.motorBakeDraft=recipe->settings;
          state_.motorBakeBudget=3;
          for(u32 budget=0;budget<3;++budget)if(recipe->settings.maximumParts==(budget==0?8u:budget==1?16u:32u)&&recipe->settings.voxelResolution==(budget==0?50000u:budget==1?100000u:400000u)&&recipe->settings.maximumVertices==(budget==2?64u:32u)&&recipe->settings.volumeErrorPercent==1&&recipe->settings.timeBudgetSeconds==(budget==2?120.f:60.f))state_.motorBakeBudget=budget;
        }
      }
      else if(state_.motorSetupTarget!=state_.selection){state_.status="Seleção mudou; abra a configuração novamente";return true;}
      else state_.motorSetupPolicy=key==widgetId(EditorWidget::MotorSetupPreserve)?0:key==widgetId(EditorWidget::MotorSetupFit)?1:key==widgetId(EditorWidget::MotorSetupConvex)?2:3;
      cancelMotorDecomposition();
      EditorEntity candidate;
      state_.motorBakeSourcesOpen=false;state_.motorBakeSettingsOpen=false;
      if(state_.motorSetupPolicy==3){refreshMotorBakeSources(state_.selection);state_.motorSetupError.clear();state_.motorSetupSummary="Escolha as fontes e gere a prévia. Requer sólidos fechados; hierarquia e visual preservados.";}
      else previewDynamicMotor(state_.selection,static_cast<MotorCollisionPolicy>(state_.motorSetupPolicy),candidate,state_.motorSetupSummary,state_.motorSetupError);
      state_.characterConversionTarget=0;state_.entityMenu=false;state_.inspectorMenu=true;state_.propertyPage=0;
      state_.compactPanel=EditorScreenState::CompactPanel::Inspector;return true;
    }
    if(key==widgetId(EditorWidget::MotorSetupBack)){if(state_.motorBakeSourcesOpen){state_.motorBakeSourcesOpen=false;return true;}if(state_.motorBakeSettingsOpen){state_.motorBakeSettingsOpen=false;return true;}cancelMotorDecomposition();state_.motorSetupTarget=0;state_.propertyPage=0;return true;}
    if(key==widgetId(EditorWidget::MotorBakeSettingsOpen)){state_.motorBakeSettingsOpen=true;state_.motorBakeSettingsPage=0;return true;}
    if(key==widgetId(EditorWidget::MotorBakeSettingsDone)){state_.motorBakeSettingsOpen=false;return true;}
    if(key==widgetId(EditorWidget::MotorBakeSettingsPrevious)){if(state_.motorBakeSettingsPage)--state_.motorBakeSettingsPage;return true;}
    if(key==widgetId(EditorWidget::MotorBakeSettingsNext)){if(state_.motorBakeSettingsPage<2)++state_.motorBakeSettingsPage;return true;}
    if(key==widgetId(EditorWidget::MotorBakePoseRest)||key==widgetId(EditorWidget::MotorBakePoseAuthored)||key==widgetId(EditorWidget::MotorBakePoseAnimation)) {
      cancelMotorDecomposition();state_.motorBakeDraft.pose=key==widgetId(EditorWidget::MotorBakePoseRest)?resources::ConvexBakePose::Rest:key==widgetId(EditorWidget::MotorBakePoseAuthored)?resources::ConvexBakePose::Authored:resources::ConvexBakePose::Animation;
      state_.motorBakeBudget=3;state_.motorSetupError.clear();return true;
    }
    if(key>=widgetId(EditorWidget::MotorBakeSettingsNumberBase)&&key<widgetId(EditorWidget::MotorBakeSettingsNumberBase)+6) {
      if(!state_.motorBakeSettingsOpen||isPlaying()||history_.isOpen())return true;
      const auto &s=state_.motorBakeDraft;const double numbers[]{double(s.maximumParts),double(s.voxelResolution),double(s.maximumVertices),s.volumeErrorPercent,double(s.timeBudgetSeconds),s.animationTime};
      state_.numericField=key;state_.numericEntity=state_.motorSetupTarget;state_.numericInstance=0;state_.numericProperty.clear();
      state_.numericCurrent=numbers[key-widgetId(EditorWidget::MotorBakeSettingsNumberBase)];
      std::snprintf(state_.numericText,sizeof(state_.numericText),"%.7g",state_.numericCurrent);
      state_.numericReplace=true;state_.numericError=false;return true;
    }
    if(key==widgetId(EditorWidget::MotorBakeSourcesOpen)){refreshMotorBakeSources(state_.selection);state_.motorBakeSourcesOpen=true;return true;}
    if(key==widgetId(EditorWidget::MotorBakeSourcesDone)){state_.motorBakeSourcesOpen=false;return true;}
    if(key==widgetId(EditorWidget::MotorBakeSourcesObject)||key==widgetId(EditorWidget::MotorBakeSourcesHierarchy)) {
      const auto options=motorDecompositionSources(state_.selection);std::vector<MotorBakeSource> sources;
      for(const auto &option:options)if(key==widgetId(EditorWidget::MotorBakeSourcesObject)?option.source.object==state_.selection:option.error.empty())sources.push_back(option.source);
      if(key==widgetId(EditorWidget::MotorBakeSourcesHierarchy)&&!options.empty()&&!options.back().source.object){state_.motorSetupError=options.back().error;return true;}
      selectMotorDecompositionSources(state_.selection,sources,state_.motorSetupError);return true;
    }
    if(key==widgetId(EditorWidget::MotorBakeSourcePrevious)){if(state_.motorBakeSourcePage)--state_.motorBakeSourcePage;return true;}
    if(key==widgetId(EditorWidget::MotorBakeSourceNext)){if((state_.motorBakeSourcePage+1)*state_.motorBakePartsPerPage()<state_.motorBakeSourceRows.size())++state_.motorBakeSourcePage;return true;}
    if(key>=widgetId(EditorWidget::MotorBakeSourceBase)&&key<widgetId(EditorWidget::MotorBakeSourceBase)+256) {
      const auto options=motorDecompositionSources(state_.selection);const auto row=key-widgetId(EditorWidget::MotorBakeSourceBase);
      if(row<options.size()&&(options[row].error.empty()||std::find(motorBakeSources_.begin(),motorBakeSources_.end(),options[row].source)!=motorBakeSources_.end())) {
        auto sources=motorBakeSources_;const auto found=std::find(sources.begin(),sources.end(),options[row].source);
        if(found==sources.end())sources.push_back(options[row].source);else sources.erase(found);
        selectMotorDecompositionSources(state_.selection,sources,state_.motorSetupError);
      }
      return true;
    }
    if(key==widgetId(EditorWidget::MotorBakeStart)||key==widgetId(EditorWidget::MotorBakeRegenerate)) {
      beginMotorDecomposition(state_.selection,state_.motorBakeDraft,state_.motorSetupError);return true;
    }
    if(key==widgetId(EditorWidget::MotorBakeConfirmMapping)){confirmMotorDecompositionMapping(state_.motorSetupError);return true;}
    if(key>=widgetId(EditorWidget::MotorBakeMappingBase)&&key<widgetId(EditorWidget::MotorBakeMappingBase)+32) {
      const auto part=key-widgetId(EditorWidget::MotorBakeMappingBase);
      if(part<motorBakePartMapping_.size()) {
        std::vector<u64> choices{0};choices.insert(choices.end(),state_.motorBakePreviousParts.begin(),state_.motorBakePreviousParts.end());
        const auto position=std::find(choices.begin(),choices.end(),motorBakePartMapping_[part]);
        const auto start=position==choices.end()?0:static_cast<usize>(position-choices.begin());
        for(usize offset=1;offset<=choices.size();++offset) {
          const auto candidate=choices[(start+offset)%choices.size()];bool taken=false;
          for(usize i=0;i<motorBakePartMapping_.size();++i)if(i!=part&&candidate&&motorBakePartMapping_[i]==candidate)taken=true;
          if(!taken){setMotorDecompositionPartMapping(part,candidate,state_.motorSetupError);break;}
        }
      }
      return true;
    }
    if(key==widgetId(EditorWidget::MotorBakeCancel)){cancelMotorDecomposition();state_.motorSetupSummary="Geração cancelada; a cena permanece intacta.";return true;}
    if(key>=widgetId(EditorWidget::MotorBakeBudgetLow)&&key<=widgetId(EditorWidget::MotorBakeBudgetHigh)) {
      cancelMotorDecomposition();state_.motorBakeBudget=key-widgetId(EditorWidget::MotorBakeBudgetLow);
      motorBakeSettings_.maximumParts=state_.motorBakeBudget==0?8:state_.motorBakeBudget==1?16:32;
      motorBakeSettings_.voxelResolution=state_.motorBakeBudget==0?50000:state_.motorBakeBudget==1?100000:400000;
      motorBakeSettings_.maximumVertices=state_.motorBakeBudget==2?64:32;motorBakeSettings_.volumeErrorPercent=1;motorBakeSettings_.timeBudgetSeconds=state_.motorBakeBudget==2?120:60;
      motorBakeSettings_.pose=state_.motorBakeDraft.pose;motorBakeSettings_.animationTime=state_.motorBakeDraft.animationTime;state_.motorBakeDraft=motorBakeSettings_;
      state_.motorSetupError.clear();state_.motorSetupSummary="Orçamento alterado; gere uma nova prévia.";return true;
    }
    if(key==widgetId(EditorWidget::MotorBakePrevious)){if(state_.motorBakePage)--state_.motorBakePage;return true;}
    if(key==widgetId(EditorWidget::MotorBakeNext)){if((state_.motorBakePage+1)*state_.motorBakePartsPerPage()<state_.motorBakeEnabled.size())++state_.motorBakePage;return true;}
    if(key>=widgetId(EditorWidget::MotorBakePartBase)&&key<widgetId(EditorWidget::MotorBakePartBase)+32) {
      const auto part=key-widgetId(EditorWidget::MotorBakePartBase);
      if(state_.motorBakeReady&&part<state_.motorBakeEnabled.size()) {
        if(motorBakeRegenerating_&&motorBakePartMapping_[part]) {
          const auto *e=document_.find(motorBakeObject_);
          if(!e||!e->components.findInstance(motorBakePartMapping_[part])){state_.motorSetupError="Remoção local preservada; restaurar o Colisor é uma edição separada";return true;}
        }
        state_.motorBakeEnabled[part]=!state_.motorBakeEnabled[part];
        refreshMotorBakeCandidate();
        if(std::none_of(state_.motorBakeEnabled.begin(),state_.motorBakeEnabled.end(),[](bool v){return v;}))state_.motorSetupError="Ative pelo menos uma parte";
      }
      return true;
    }
    if(key==widgetId(EditorWidget::MotorSetupApply)) {
      if(state_.motorSetupTarget!=state_.selection){state_.status="Seleção mudou; abra a configuração novamente";return true;}
      configureDynamicMotor(state_.selection,static_cast<MotorCollisionPolicy>(state_.motorSetupPolicy),state_.motorSetupError);return true;
    }
    if(key==widgetId(EditorWidget::CharacterConversionOpen)) {
      state_.motorSetupTarget=0;
      EditorEntity root,child;std::string error;
      state_.characterConversionTarget=state_.selection;
      const bool ready=prepareCharacterConversion(state_.selection,root,child,error);
      state_.characterConversionError=std::move(error);
      if(ready) {const auto *c=runtime::characterComponent(root);state_.characterConversionRadius=c->radius;state_.characterConversionHeight=2*(c->halfHeight+c->radius);}
      state_.entityMenu=false;state_.inspectorMenu=true;state_.propertyPage=0;
      state_.compactPanel=EditorScreenState::CompactPanel::Inspector;
      return true;
    }
    if(key==widgetId(EditorWidget::CharacterConversionBack)) {state_.characterConversionTarget=0;state_.propertyPage=0;return true;}
    if(key==widgetId(EditorWidget::CharacterConversionApply)) {
      if(state_.characterConversionTarget!=state_.selection) {state_.status="Seleção mudou; abra a conversão novamente";return true;}
      const auto result=convertToCharacter(state_.selection);
      if(!result) state_.characterConversionError=state_.status;
      return true;
    }
    if(key==widgetId(EditorWidget::InspectorOpenFocused)) {state_.inspectorMenu=false;openFocusedInspector(state_.selection);return true;}
    if(key==widgetId(EditorWidget::HierarchyProperties)) {state_.entityMenu=false;openFocusedInspector(state_.selection);return true;}
    if(key>=widgetId(EditorWidget::ComponentPropertiesBase) && key<widgetId(EditorWidget::ComponentPropertiesBase)+0x100u) {
      const auto *entity=document_.find(state_.selection);
      const u32 index=key-widgetId(EditorWidget::ComponentPropertiesBase);
      if(entity && index<entity->components.size()) {
        state_.nativeMenu=0;state_.scriptMenu=0;
        openFocusedInspector(entity->id,entity->components.at(index)->instanceId());
      }
      return true;
    }
    const u32 count=static_cast<u32>(state_.focusedInspectors.size());
    if(key>=widgetId(EditorWidget::FocusedTabBase) && key<widgetId(EditorWidget::FocusedTabBase)+count) {
      state_.focusedActive=key-widgetId(EditorWidget::FocusedTabBase)+1;state_.focusedMenu=false;return true;
    }
    if(key>=widgetId(EditorWidget::FocusedCloseBase) && key<widgetId(EditorWidget::FocusedCloseBase)+count) {
      const u32 index=key-widgetId(EditorWidget::FocusedCloseBase);
      state_.focusedInspectors.erase(state_.focusedInspectors.begin()+index);
      if(index<focusedNames_.size()) focusedNames_.erase(focusedNames_.begin()+index);
      state_.focusedActive=state_.focusedInspectors.empty()?0:std::min<u32>(std::max(state_.focusedActive,1u),static_cast<u32>(state_.focusedInspectors.size()));
      state_.focusedMenu=false;saveEditorPreferences();return true;
    }
    if(key==widgetId(EditorWidget::FocusedMenu)) {state_.focusedMenu=!state_.focusedMenu;return true;}
    if(key==widgetId(EditorWidget::FocusedPing) && count) {
      state_.focusedMenu=false;
      const auto &focused=state_.focusedInspectors[std::min(std::max(state_.focusedActive,1u),count)-1];
      // Recurso: Ping no painel Arquivos (abre as pastas e marca o arquivo).
      if(focused.kind!=EditorScreenState::FocusedAsset::None) {
        if(const auto *record=assets_.find(focused.asset)) {
          state_.filesCollapsed=false;revealProjectPath(record->path);state_.selectedFile=record->path;
        }
      } else pingEntity(focused.entity);
      return true;
    }
    if(key==widgetId(EditorWidget::FocusedCloseAll)) {
      state_.focusedInspectors.clear();focusedNames_.clear();state_.focusedActive=0;state_.focusedMenu=false;saveEditorPreferences();return true;
    }
    if(key==widgetId(EditorWidget::FocusedCollapse)) {state_.focusedCollapsed=true;state_.focusedMenu=false;return true;}
    if(key==widgetId(EditorWidget::FocusedChip)) {state_.focusedCollapsed=false;return true;}
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::InspectorLock)) {
    // Dentro do escopo a "seleção" é o alvo do Inspector: travar prende nele.
    state_.inspectorLocked=state_.inspectorLocked?0:state_.selection;
    state_.status=state_.inspectorLocked?"Inspector travado neste objeto":"Inspector segue a seleção";
    return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::InspectorDebugToggle)) {
    state_.inspectorDebug=!state_.inspectorDebug;state_.inspectorMenu=false;state_.propertyPage=0;return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::InspectorPing)) {
    state_.inspectorMenu=false;pingEntity(state_.selection);return true;
  }
  if(handleComponentReorder(event,routing)) return true;
  if(handleScriptArrayDrag(event,routing)) return true;
  if(handleLodBar(event,routing)) return true;

  const u32 hierarchyRows=widgetId(EditorWidget::HierarchyRowBase);
  if(routing.widgetId>=hierarchyRows && routing.widgetId<hierarchyRows+EditorDocument::kMaximumEntities) {
    // Horizontal movement starts a hierarchy drag; vertical movement retains
    // the established touch-scroll behaviour.
    if(routing.dragging && event.phase==UiPointerPhase::Move && !history_.isOpen() &&
       (state_.draggingEntity || std::abs(routing.totalDelta.x)>std::max(24.0f,std::abs(routing.totalDelta.y)*1.5f))) {
      state_.draggingEntity=routing.widgetId-hierarchyRows;
      hierarchyPointer_=event.pointerId;state_.assetDragPosition=event.position;
      state_.status="Solte sobre o novo pai; arraste vertical para rolar";
    }
    if(state_.draggingEntity) {
      if(routing.released) {
        const auto entity=state_.draggingEntity;state_.draggingEntity=kInvalidEntity;
        bool accepted=false;
        if(event.phase==UiPointerPhase::Up) {
          const auto target=router_.hitTest(event.position);
          // Sobre um campo de referência o gesto é atribuir, nunca reparentear:
          // a recusa fica com o motivo do campo.
          const u32 operation=target.widgetId&0xff000000u;
          if(target.target==UiPointerTarget::Widget && (operation==widgetId(EditorWidget::ComponentReferenceBase) ||
                                                        operation==widgetId(EditorWidget::ScriptFieldBase))) {
            if(dropObjectOnField(target.widgetId,entity)) state_.status="Referência atribuída pelo arraste";
            return true;
          }
          if(target.target==UiPointerTarget::Widget && target.widgetId>=hierarchyRows &&
             target.widgetId<hierarchyRows+EditorDocument::kMaximumEntities)
            accepted=history_.reparentKeepingWorld(document_,entity,target.widgetId-hierarchyRows);
          else if(target.target==UiPointerTarget::None && layout_.hierarchyPanel.contains(event.position))
            accepted=history_.reparentKeepingWorld(document_,entity,document_.root());
        }
        state_.status=accepted?"Pai alterado; transformacao mundial preservada":"Mudanca de pai cancelada ou incompativel";
        if(accepted) setSelection(entity);
      }
      return true;
    }
  }
  // Barra de status: console e trabalhos.
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::StatusConsole)) {
    if(state_.workspace==EditorWorkspace::Code) {
      state_.codeFiles=false;state_.codeMenu=false;state_.consoleCollapsed=false;state_.consoleExpanded=true;
    } else if(!isPlaying()) {state_.workspace=EditorWorkspace::Scene;state_.diagnosticDockOpen=true;}
    return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::StatusTasks)) {state_.backgroundPanel=!state_.backgroundPanel;return true;}
  if(state_.backgroundPanel && routing.tapped) {
    const u32 key=routing.widgetId;
    if(key==widgetId(EditorWidget::StatusTasksClose)) {state_.backgroundPanel=false;return true;}
    if(key>=widgetId(EditorWidget::StatusTaskCancelBase) && key<widgetId(EditorWidget::StatusTaskCancelBase)+state_.backgroundTasks.size()) {
      const auto &task=state_.backgroundTasks[key-widgetId(EditorWidget::StatusTaskCancelBase)];
      if(task.cancelable) {
        // Importações usam o mesmo cancelamento do botão da preparação.
        if(task.kind==EditorBackgroundTask::Kind::ModelImport || task.kind==EditorBackgroundTask::Kind::EnvironmentImport ||
           task.kind==EditorBackgroundTask::Kind::TextureImport) state_.importCancel=true;
        else backgroundCancel_=task.kind;
        state_.status="Cancelando: "+task.label;
      }
      return true;
    }
  }
  // Layouts dos painéis.
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::LayoutsOpen)) {
    state_.workspaceMenu=false;state_.layoutsPanel=true;return true;
  }
  if(state_.layoutsPanel && routing.target==UiPointerTarget::Widget) {
    if(!routing.tapped) return true;
    const u32 key=routing.widgetId;
    const auto builtins=editorBuiltinLayouts(state_.surface.width);
    if(key==widgetId(EditorWidget::LayoutsClose)) state_.layoutsPanel=false;
    else if(key==widgetId(EditorWidget::LayoutReset)) {applyLayout(builtins[0]);state_.status="Layout padrão restaurado";}
    else if(key==widgetId(EditorWidget::LayoutSave)) {
      state_.namingLayout=true;std::snprintf(state_.renameText,sizeof(state_.renameText),"Layout %u",static_cast<unsigned>(state_.userLayouts.size()+1));
    } else if(key>=widgetId(EditorWidget::LayoutBuiltinBase) && key<widgetId(EditorWidget::LayoutBuiltinBase)+builtins.size()) {
      const auto &layout=builtins[key-widgetId(EditorWidget::LayoutBuiltinBase)];
      applyLayout(layout);state_.status="Layout: "+layout.name;
    } else if(key>=widgetId(EditorWidget::LayoutDeleteBase) && key<widgetId(EditorWidget::LayoutDeleteBase)+state_.userLayouts.size()) {
      const u32 index=key-widgetId(EditorWidget::LayoutDeleteBase);
      state_.status="Layout apagado: "+state_.userLayouts[index].name;
      state_.userLayouts.erase(state_.userLayouts.begin()+index);saveEditorPreferences();
    } else if(key>=widgetId(EditorWidget::LayoutUserBase) && key<widgetId(EditorWidget::LayoutUserBase)+state_.userLayouts.size()) {
      const auto layout=state_.userLayouts[key-widgetId(EditorWidget::LayoutUserBase)];
      applyLayout(layout);state_.status="Layout: "+layout.name;
    }
    return true;
  }
  // Multisseleção: modo, ações de conjunto e linhas da Hierarquia no modo.
  if(routing.tapped) {
    const u32 key=routing.widgetId;
    if(key==widgetId(EditorWidget::HierarchyMultiToggle)) {
      state_.multiSelect=!state_.multiSelect;
      if(!state_.multiSelect && state_.selectionSet.size()>1) state_.selectionSet={state_.selection};
      state_.status=state_.multiSelect?"Selecionar vários: toque nos objetos para somar ou tirar":"Seleção única";
      return true;
    }
    std::vector<EditorEntityId> all;document_.collectSubtree(document_.root(),all);
    all.erase(std::remove(all.begin(),all.end(),document_.root()),all.end());
    if(key==widgetId(EditorWidget::SelectAll)) {selectEntities(all);return true;}
    if(key==widgetId(EditorWidget::SelectNone)) {selectEntities({});return true;}
    if(key==widgetId(EditorWidget::SelectInvert)) {
      std::vector<EditorEntityId> inverted;
      for(const auto id:all) if(!state_.isSelected(id)) inverted.push_back(id);
      selectEntities(inverted);return true;
    }
    if(key==widgetId(EditorWidget::SelectChildren)) {
      // Unity: Select Children — os selecionados e todos os descendentes.
      std::vector<EditorEntityId> result;
      for(const auto id:selectedRoots()) {std::vector<EditorEntityId> subtree;document_.collectSubtree(id,subtree);
        for(const auto member:subtree) if(std::find(result.begin(),result.end(),member)==result.end()) result.push_back(member);}
      selectEntities(result);return true;
    }
    if(state_.multiSelect && !state_.reparentEntity && key>=widgetId(EditorWidget::HierarchyRowBase) &&
       key<widgetId(EditorWidget::HierarchyRowBase)+EditorDocument::kMaximumEntities && routing.heldSeconds<ui::kUiLongPressSeconds) {
      toggleSelection(key-widgetId(EditorWidget::HierarchyRowBase));return true;
    }
  }
  // Busca global: modal sobre tudo; toques fora dela não chegam ao editor.
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::GlobalSearchOpen)) {openGlobalSearch();return true;}
  if(state_.globalSearch && routing.target==UiPointerTarget::Widget) {
    if(!routing.tapped) return true;
    const u32 key=routing.widgetId;
    if(key==widgetId(EditorWidget::GlobalSearchClose)) {state_.globalSearch=false;return true;}
    if(key==widgetId(EditorWidget::GlobalSearchField)) {
      state_.editingGlobalSearch=true;std::snprintf(state_.renameText,sizeof(state_.renameText),"%s",state_.globalQuery.c_str());return true;
    }
    if(key==widgetId(EditorWidget::GlobalSearchPrevious)) {if(state_.globalPage) --state_.globalPage;return true;}
    if(key==widgetId(EditorWidget::GlobalSearchNext)) {++state_.globalPage;return true;}
    if(key>=widgetId(EditorWidget::GlobalSearchProviderBase) && key<widgetId(EditorWidget::GlobalSearchProviderBase)+4) {
      state_.globalProvider=static_cast<EditorSearchProvider>(key-widgetId(EditorWidget::GlobalSearchProviderBase));
      state_.globalPage=0;return true;
    }
    if(key>=widgetId(EditorWidget::GlobalSearchResultBase) && key<widgetId(EditorWidget::GlobalSearchResultBase)+state_.globalResults.size()) {
      openSearchResult(key-widgetId(EditorWidget::GlobalSearchResultBase));return true;
    }
    return true;
  }
  // Unity 6000.0 Manual/UndoWindow: o histórico abre pelo ícone de Desfazer;
  // aqui, pelo toque longo em Desfazer ou Refazer (o toque curto continua
  // desfazendo um passo).
  if(routing.tapped && routing.heldSeconds>=ui::kUiLongPressSeconds &&
     (routing.widgetId==widgetId(EditorWidget::Undo) || routing.widgetId==widgetId(EditorWidget::Redo))) {
    state_.undoHistory=true;state_.undoHistoryPage=0;
    return true;
  }
  if(routing.tapped && state_.undoHistory) {
    const u32 key=routing.widgetId;
    if(key==widgetId(EditorWidget::UndoHistoryClose)) {state_.undoHistory=false;return true;}
    if(key==widgetId(EditorWidget::UndoHistoryOrder)) {
      state_.undoNewestFirst=!state_.undoNewestFirst;state_.undoHistoryPage=0;saveEditorPreferences();return true;
    }
    if(key==widgetId(EditorWidget::UndoHistoryPrevious)) {if(state_.undoHistoryPage) --state_.undoHistoryPage;return true;}
    if(key==widgetId(EditorWidget::UndoHistoryNext)) {++state_.undoHistoryPage;return true;}
    if(key>=widgetId(EditorWidget::UndoHistoryRowBase) && key<=widgetId(EditorWidget::UndoHistoryRowBase)+history_.entryCount()) {
      const u32 point=key-widgetId(EditorWidget::UndoHistoryRowBase);
      const bool reached=moveHistoryTo(point);
      state_.status=!reached?"Histórico: um passo recusou; a cena parou no último ponto possível":
                    point?"Histórico: "+history_.describe(point-1):std::string("Histórico: cena como abriu");
      return true;
    }
  }
  if(routing.tapped && (routing.widgetId==widgetId(EditorWidget::Undo) ||
      routing.widgetId==widgetId(EditorWidget::Redo) ||
      routing.widgetId==widgetId(EditorWidget::DuplicateSelection) ||
      routing.widgetId==widgetId(EditorWidget::DeleteSelection))) {
    EditorActionRequest command;
    command.version=sceneVersion();command.entity=state_.selection;
    command.action=routing.widgetId==widgetId(EditorWidget::Undo)?EditorAction::Undo:
      routing.widgetId==widgetId(EditorWidget::Redo)?EditorAction::Redo:
      routing.widgetId==widgetId(EditorWidget::DuplicateSelection)?EditorAction::Duplicate:EditorAction::Remove;
    const auto result=dispatch(command);
    if(result.status==EditorActionStatus::Applied &&
       (command.action==EditorAction::Undo||command.action==EditorAction::Redo) &&
       state_.meshPicker&&state_.resourceProperty=="collision_mesh") refreshCollisionMeshDraft();
    state_.entityMenu=false;
    if(result.status!=EditorActionStatus::Applied) state_.status="Operação indisponível";
    return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::PrefabCreate)) {
    std::string error;
    if(state_.selectionSet.size()>1) state_.status="Selecione uma raiz para criar o prefab da subárvore";
    else if(!createPrefab(state_.inspectorTarget?state_.inspectorTarget:state_.selection,error).valid()) state_.status=error;
    state_.entityMenu=false;return true;
  }
  if(routing.tapped && (routing.widgetId==widgetId(EditorWidget::PrefabOverrides) ||
      routing.widgetId==widgetId(EditorWidget::PrefabOverridesRefresh))) {
    if(state_.selectionSet.size()>1) {state_.status="Selecione um objeto para comparar com a fonte";return true;}
    const auto target=state_.inspectorTarget?state_.inspectorTarget:state_.selection;
    state_.prefabOperationStatus.clear();
    std::string error;inspectPrefabOverrides(target,state_.prefabOverrides,error);
    state_.prefabOverridesOpen=true;state_.inspectorMenu=false;state_.inspectorDebug=false;
    state_.propertyPage=0;if(!error.empty()) state_.status=error;return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::PrefabOverridesClose)) {
    state_.prefabOverridesOpen=false;state_.propertyPage=0;return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::PrefabReceiveSource)) {
    std::string error;
    if(!refreshPrefabInstance(state_.prefabOverrides,error)) state_.status=error;
    else {const auto target=state_.prefabOverrides.object;inspectPrefabOverrides(target,state_.prefabOverrides,error);state_.propertyPage=0;}
    return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::PrefabOverridesRevertAll)) {
    std::vector<usize> rows;
    for(usize i=0;i<state_.prefabOverrides.rows.size();++i) rows.push_back(i);
    std::string error;const auto target=state_.prefabOverrides.object;
    if(!revertPrefabOverrides(state_.prefabOverrides,rows,error)) {
      state_.status=error;state_.prefabOperationStatus=error;
    } else {
      inspectPrefabOverrides(target,state_.prefabOverrides,error);state_.propertyPage=0;
      state_.prefabOperationStatus=error.empty()?"Diferenças revertidas neste objeto · Desfazer restaura o lote":error;
    }
    return true;
  }
  if(routing.tapped && (routing.widgetId&0xff000000u)==widgetId(EditorWidget::PrefabOverrideRevertBase)) {
    std::string error;
    if(!revertPrefabOverride(state_.prefabOverrides,routing.widgetId&0x00ffffffu,error)) state_.status=error;
    else {const auto target=state_.prefabOverrides.object;inspectPrefabOverrides(target,state_.prefabOverrides,error);}
    return true;
  }
  if(routing.tapped && (routing.widgetId&0xff000000u)==widgetId(EditorWidget::PrefabOverrideApplyBase)) {
    const auto row=routing.widgetId&0x00ffffffu;
    std::string error;PrefabApplyReport report;
    const bool conflict=row<state_.prefabOverrides.rows.size() &&
      state_.prefabOverrides.rows[row].origin==PrefabOverrideOrigin::Conflict;
    const auto target=state_.prefabOverrides.object;
    if(!applyPrefabOverride(state_.prefabOverrides,row,conflict,error,&report)) {
      state_.prefabOperationStatus=error;state_.status=error;
    } else {
      state_.prefabOperationStatus="Fonte atualizada · "+std::to_string(report.instances)+" instâncias · "+
        std::to_string(report.propagatedFields)+" campos herdados · "+std::to_string(report.conflicts)+" conflitos preservados";
      const auto published=state_.prefabOperationStatus;
      inspectPrefabOverrides(target,state_.prefabOverrides,error);
      state_.prefabOperationStatus=error.empty()?published:published+" · "+error;
    }
    return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::PrefabUnpack)) {
    std::string error;
    if(state_.selectionSet.size()>1) state_.status="Selecione uma instância para desvincular";
    else if(!unpackPrefab(state_.inspectorTarget?state_.inspectorTarget:state_.selection,error)) state_.status=error;
    state_.entityMenu=false;state_.prefabOverridesOpen=false;return true;
  }
  if(routing.tapped && routing.widgetId==widgetId(EditorWidget::InspectorMenu)) state_.prefabOverridesOpen=false;
  const auto previousSurface=state_.inspectorSurface;
  const EditorPointerOutcome outcome =
      applyEditorPointer(state_, layout_, routing, document_, history_);
  if(previousSurface!=state_.inspectorSurface) saveEditorPreferences();
  if (outcome.requestPlay) preparePlay();
  if(!isPlaying()) {jumpPressed_=false;secondaryPressed_=false;}
  return outcome.consumed;
}

bool EditorSession::inputBindingCaptureActive() const noexcept {
  return inputCapture_.has_value() && state_.inputCapturing &&
      state_.workspace==EditorWorkspace::Project && state_.projectSection==EditorProjectSection::Input;
}

void EditorSession::cancelInputBindingCapture() {
  inputCapture_.reset();state_.inputCapturing=false;state_.inputCaptureNegative=false;state_.inputCapturePrompt.clear();state_.inputCaptureFeedback.clear();
}

bool EditorSession::validateInputCapture() {
  if(!inputBindingCaptureActive()) return false;
  const auto &capture=*inputCapture_;const auto version=sceneVersion();
  const auto &actions=document_.inputActions().actions();
  return version.epoch==capture.version.epoch && version.revision==capture.version.revision &&
      files_.rootPath()==capture.project && document_.inputActions()==capture.before &&
      state_.inputActionIndex<actions.size() && actions[state_.inputActionIndex].id==capture.action &&
      state_.inputBindingIndex==capture.binding && !history_.isOpen();
}

bool EditorSession::beginInputBindingCapture(bool negative) {
  cancelInputBindingCapture();
  if(state_.workspace!=EditorWorkspace::Project || state_.projectSection!=EditorProjectSection::Input ||
     history_.isOpen() || pendingTextEdit().purpose!=EditorTextPurpose::None) {
    state_.status="Finalize o campo atual e abra o mapa de entrada para capturar";return false;
  }
  const auto &actions=document_.inputActions().actions();
  if(state_.inputActionIndex>=actions.size()) return false;
  const auto &action=actions[state_.inputActionIndex];
  if(state_.inputBindingIndex>=action.bindings.size()) return false;
  const auto source=action.bindings[state_.inputBindingIndex].source;
  if((source!=runtime::InputSource::Key && source!=runtime::InputSource::GamepadButton && source!=runtime::InputSource::GamepadAxis && source!=runtime::InputSource::MouseButton) ||
     (negative && (source!=runtime::InputSource::Key || action.kind==runtime::ActionKind::Button))) {
    state_.status="A captura exige Tecla, Botão mouse, Botão gamepad ou Eixo gamepad";return false;
  }
  InputCapture capture;capture.version=sceneVersion();capture.before=document_.inputActions();
  capture.project=files_.rootPath();capture.action=action.id;capture.binding=state_.inputBindingIndex;capture.negative=negative;
  inputCapture_=std::move(capture);state_.inputCapturing=true;state_.inputCaptureNegative=negative;
  state_.inputCapturePrompt=source==runtime::InputSource::Key?
      negative?"Pressione a tecla negativa":"Pressione uma tecla":
      source==runtime::InputSource::GamepadButton?"Pressione um botão":source==runtime::InputSource::MouseButton?"Clique um botão do mouse":"Mova o eixo do gamepad";
  state_.inputCaptureFeedback=source==runtime::InputSource::GamepadAxis?"Solte o eixo antes de mover":"Aguardando nova pressão";
  state_.status="Capturando · "+action.id;return true;
}

bool EditorSession::commitInputCapture(u32 code,bool invert) {
  if(!validateInputCapture()) {cancelInputBindingCapture();state_.status="Captura cancelada: a autoria mudou";return false;}
  auto map=inputCapture_->before;auto action=*map.find(inputCapture_->action);auto &binding=action.bindings[inputCapture_->binding];
  if(binding.source==runtime::InputSource::Key) {
    if(code==(inputCapture_->negative?binding.code:binding.negativeCode) && code) {
      state_.status="Use teclas diferentes para as duas direções";state_.inputCaptureFeedback=state_.status;return false;
    }
    if(inputCapture_->negative) binding.negativeCode=code;else binding.code=code;
  } else {
    binding.code=code;if(binding.source==runtime::InputSource::GamepadAxis) binding.invert=invert;
  }
  const auto name=action.id;
  if(!map.replace(name,action) || !history_.setInputActions(document_,map)) {
    cancelInputBindingCapture();state_.status="O vínculo capturado foi recusado";return false;
  }
  cancelInputBindingCapture();state_.status="Vínculo capturado · "+name+" · código "+std::to_string(code);return true;
}

bool EditorSession::captureInputKey(u32 code,bool gamepad,bool down,bool repeated) {
  if(!inputCapture_) return false;
  if(!validateInputCapture()) {cancelInputBindingCapture();return false;}
  if(down && !gamepad && (code==4 || code==111)) {cancelInputBindingCapture();state_.status="Captura cancelada";return true;}
  if(repeated) {state_.inputCaptureFeedback="Solte a tecla antes de pressionar";return true;}
  if(!down || !code || code>65535 || code==3 || code==24 || code==25 || code==26) return true;
  const auto &action=*inputCapture_->before.find(inputCapture_->action);
  const auto source=action.bindings[inputCapture_->binding].source;
  if(source==(gamepad?runtime::InputSource::GamepadButton:runtime::InputSource::Key)) commitInputCapture(code,false);
  else state_.inputCaptureFeedback="Entrada de outra fonte ignorada";
  return true;
}

bool EditorSession::captureInputMouseButton(u32 code,bool down,UiPoint position) {
  if(!inputCapture_) return false;
  if(!validateInputCapture()) {cancelInputBindingCapture();return false;}
  if(!down || code>=5) return true;
  const auto hit=router_.hitTest(position);
  if(code==0 && hit.target==UiPointerTarget::Widget && hit.widgetId==widgetId(EditorWidget::InputBindingCaptureCancel)) {
    cancelInputBindingCapture();state_.status="Captura cancelada";return true;
  }
  const auto &action=*inputCapture_->before.find(inputCapture_->action);
  if(action.bindings[inputCapture_->binding].source==runtime::InputSource::MouseButton)commitInputCapture(code,false);
  else state_.inputCaptureFeedback="Entrada de outra fonte ignorada";
  return true;
}

bool EditorSession::captureInputAxes(const std::array<float,8> &axes) {
  if(!inputCapture_) return false;
  if(!validateInputCapture()) {cancelInputBindingCapture();return false;}
  const auto &action=*inputCapture_->before.find(inputCapture_->action);
  if(action.bindings[inputCapture_->binding].source!=runtime::InputSource::GamepadAxis) return true;
  u32 selected=8;float magnitude=.65f;bool heldBeforeArm=false;
  for(u32 i=0;i<axes.size();++i) {
    if(!std::isfinite(axes[i]) || std::abs(axes[i])>1.01f) continue;
    if(std::abs(axes[i])<.2f) inputCapture_->neutralAxes|=1u<<i;
    if(!(inputCapture_->neutralAxes&(1u<<i)) && std::abs(axes[i])>.65f) heldBeforeArm=true;
    if((inputCapture_->neutralAxes&(1u<<i)) && std::abs(axes[i])>magnitude) {selected=i;magnitude=std::abs(axes[i]);}
  }
  if(selected<axes.size()) commitInputCapture(selected,axes[selected]<0);
  else state_.inputCaptureFeedback=heldBeforeArm || !inputCapture_->neutralAxes?"Solte o eixo antes de mover":"Mova o eixo com mais força";
  return true;
}

bool EditorSession::startPlay() {
  cancelInputBindingCapture();
  if(isPlaying()) return true;
  // As mesmas travas do botão Play: fonte compilando ou não publicada.
  if(state_.codeBuildBusy) return false;
  if(state_.code && (state_.code->catalogState()==EditorCodeCatalogState::Failed ||
      state_.code->catalogState()==EditorCodeCatalogState::Stale || state_.code->dirty())) return false;
  state_.workspace=EditorWorkspace::Play;state_.playPaused=false;state_.playStepRequested=false;
  preparePlay();
  return playRequested_;
}

void EditorSession::preparePlay() {
  if(playScene_.active()) {endPlayInspect();playScene_.stop();runtimeTextures_.clear();}
  state_.playTimeScale=1;
  std::vector<EditorEntityId> tagObjects;document_.collectSubtree(document_.root(),tagObjects);
  for(const auto id:tagObjects) if(!document_.tags().contains(document_.find(id)->tag)) {
    state_.status="Tag ausente no catálogo: "+document_.find(id)->tag;
    state_.workspace=EditorWorkspace::Scene;playRequested_=false;return;
  }
  state_.playHasScripts=runtime::ScriptBridge::hasScripts(document_);
  // O primeiro frame do projeto antecede a compilação automática. Um catálogo
  // vazio não é um assembly pronto: a mesma barreira vale para botão e autostart.
  if(state_.playHasScripts && code_.catalogState()!=EditorCodeCatalogState::Current) {
    state_.status="Aguarde a publicação dos scripts antes de Play";
    state_.workspace=EditorWorkspace::Scene;playRequested_=false;return;
  }
  state_.playSecondaryActionLabel.clear();state_.playHudMessage.clear();
  for(const auto &action:document_.inputActions().actions()) {
    if(action.kind!=runtime::ActionKind::Button) continue;
    for(const auto &binding:action.bindings)
      if(binding.source==runtime::InputSource::TouchButton && binding.code==1) {
        state_.playSecondaryActionLabel=action.id;break;
      }
    if(!state_.playSecondaryActionLabel.empty()) break;
  }
  const auto view=resolveSceneCamera(document_);
  const auto *camera=document_.find(view.entity);
  state_.playFirstPerson=camera&&cameraLook(*camera);
  state_.playHasCharacter=false;
  for(auto *ancestor=camera;ancestor;ancestor=document_.find(ancestor->parent))
    if(characterComponent(*ancestor)) {state_.playHasCharacter=true;break;}
  playRequested_ = true;
  cancelPointers();
}

std::vector<std::string> EditorSession::scriptArrayItems(const scene::ScriptBehavior &script,std::string_view id) const {
  std::vector<std::string> items;
  const auto *declared=scriptProperty(script.scriptType,id);
  if(!declared) return items;
  for(const auto &p:script.properties) if(p.id==id && p.valueType==declared->valueType) scene::parseScriptArray(p.value,items);
  return items;
}

bool EditorSession::setScriptArray(EditorEntityId entity,u64 instance,std::string_view id,std::string_view declaredType,
                                   const std::vector<std::string> &items) {
  EditorActionRequest request;request.version=sceneVersion();request.entity=entity;
  request.componentInstance=instance;request.action=EditorAction::ScriptProperty;
  request.componentProperty=std::string(id);request.scriptPropertyType=std::string(declaredType);
  request.scriptPropertyValue=scene::scriptArrayValue(items);
  if(dispatch(request).status==EditorActionStatus::Applied) return true;
  state_.status="Lista recusada pelo tipo do campo";
  return false;
}

const EditorScriptProperty *EditorSession::scriptProperty(std::string_view scriptType,std::string_view id) const {
  for(const auto &type:code_.scriptTypes()) if(type.id==scriptType)
    for(const auto &property:type.properties) if(property.id==id) return &property;
  return nullptr;
}

// Unity Manual/InspectorArray: arrastar a alça de um elemento muda a posição
// dele na lista. Solto sobre outro elemento, vai para aquele lugar; fora,
// nada muda. A lista inteira é gravada num passo de Desfazer.
bool EditorSession::handleScriptArrayDrag(const UiPointerEvent &event,const UiPointerRouting &routing) {
  const u32 handleBase=widgetId(EditorWidget::ScriptArrayHandleBase);
  const auto handle=[&](u32 widget) {return (widget&0xff000000u)==handleBase || (widget&0xff000000u)==widgetId(EditorWidget::ScriptArrayElementBase);};
  if(!state_.scriptArrayDrag) {
    if((routing.widgetId&0xff000000u)!=handleBase || !routing.dragging || event.phase!=UiPointerPhase::Move ||
       std::abs(routing.totalDelta.y)<12.0f || history_.isOpen()) return false;
    state_.scriptArrayDrag=((routing.widgetId&0x00ffffffu)>>8)+1;state_.scriptArrayDragTarget=0;
    scriptArrayPointer_=event.pointerId;scriptArrayPagerHover_=0;
  }
  if(event.pointerId!=scriptArrayPointer_) return true;
  state_.scriptArrayDragPoint=event.position;
  const auto hover=router_.hitTest(event.position);
  const u32 over=hover.target==UiPointerTarget::Widget?hover.widgetId:0;
  state_.scriptArrayDragTarget=handle(over)?((over&0x00ffffffu)>>8)+1:0;
  // Os campos paginam: passar pela seta durante o arraste vira a página.
  if(event.phase==UiPointerPhase::Move && over!=scriptArrayPagerHover_) {
    scriptArrayPagerHover_=over;
    if(over==widgetId(EditorWidget::ScriptFieldsNext)) ++state_.scriptPropertyPage;
    else if(over==widgetId(EditorWidget::ScriptFieldsPrevious) && state_.scriptPropertyPage) --state_.scriptPropertyPage;
  }
  if(event.phase!=UiPointerPhase::Up && event.phase!=UiPointerPhase::Cancel) return true;
  const u32 from=state_.scriptArrayDrag-1,to=state_.scriptArrayDragTarget;
  state_.scriptArrayDrag=0;state_.scriptArrayDragTarget=0;scriptArrayPointer_=0;scriptArrayPagerHover_=0;
  if(event.phase==UiPointerPhase::Cancel || !to || to-1==from) return true;
  const auto *entity=document_.find(state_.selection);
  const auto *script=entity?scene::scriptBehavior(entity->components.findInstance(state_.expandedScript)):nullptr;
  const auto *declared=script?scriptProperty(script->scriptType,state_.expandedScriptArray):nullptr;
  if(!declared) return true;
  auto items=scriptArrayItems(*script,declared->id);
  if(from>=items.size() || to-1>=items.size()) return true;
  auto moved=std::move(items[from]);items.erase(items.begin()+from);
  items.insert(items.begin()+(to-1),std::move(moved));
  if(setScriptArray(entity->id,script->instanceId(),declared->id,declared->valueType,items)) {
    state_.scriptArraySelected=to;state_.status="Elemento movido";
  }
  return true;
}

// A cor exibida (sRGB de 8 bits) volta como cor linear; alfa e intensidade HDR
// do campo ficam como estavam (a tela não tem alfa nem faixa HDR).
void EditorSession::applyPixelSample(const u8 (&rgba)[4]) {
  if(!state_.colorField || !state_.colorPicking) return;
  float linear[4]{};
  for(u32 i=0;i<3;++i) linear[i]=colorToLinear(rgba[i]/255.0f);
  const float alpha=state_.colorAlpha,intensity=state_.colorIntensity;
  linear[3]=alpha;
  pickerFromLinear(linear);
  state_.colorAlpha=alpha;state_.colorIntensity=intensity;
  state_.colorPicking=state_.colorSampling=false;
  char hex[16];std::snprintf(hex,sizeof hex,"#%02X%02X%02X",rgba[0],rgba[1],rgba[2]);
  state_.status=std::string("Cor amostrada: ")+hex;
}

void EditorSession::pickerFromLinear(const float (&linearRgba)[4]) {
  float base[3],intensity=0;splitHdr({linearRgba[0],linearRgba[1],linearRgba[2]},base,intensity);
  float srgb[3];for(u32 i=0;i<3;++i) srgb[i]=colorToSrgb(base[i]);
  float h=state_.colorHue,s=0,v=0;srgbToHsv(srgb,h,s,v);
  state_.colorHue=h;state_.colorSaturation=s;state_.colorValue=v;
  state_.colorAlpha=state_.colorHasAlpha?std::clamp(linearRgba[3],0.f,1.f):1;
  state_.colorIntensity=state_.colorHdr?std::clamp(intensity,-10.f,10.f):0;
}

void EditorSession::pickerLinear(float (&rgba)[4]) const {
  float srgb[3];pickerRgb(state_.colorHue,state_.colorSaturation,state_.colorValue,srgb);
  const float scale=state_.colorHdr?std::exp2(state_.colorIntensity):1;
  for(u32 i=0;i<3;++i) rgba[i]=colorToLinear(srgb[i])*scale;
  rgba[3]=state_.colorHasAlpha?state_.colorAlpha:1;
}

void EditorSession::openColorWindow(u32 key,const float (&linearRgba)[4],bool alpha,bool hdr) {
  state_.colorField=key;state_.colorRevision=document_.revision();
  state_.colorHasAlpha=alpha;state_.colorHdr=hdr;state_.colorMode=0;
  state_.colorSwatchMenu=0;state_.colorLibraryMenu=false;state_.colorText=0;
  std::copy(linearRgba,linearRgba+4,state_.colorOriginal);
  pickerFromLinear(linearRgba);
  // Amostras do projeto: sem projeto aberto a biblioteca existe só nesta sessão.
  std::string error;
  if(!files_.rootPath().empty() && !colorLibraries_.load(files_.rootPath(),EditorLibraryKind::Color,error)) state_.status=error;
  state_.colorLibraries=&colorLibraries_;
}

void EditorSession::saveColorLibraries() {
  std::string error;
  if(!files_.rootPath().empty() && !colorLibraries_.save(error)) state_.status=error;
}

bool EditorSession::commitColorWindow() {
  if(state_.colorTarget==3) {
    // Parada de cor do gradiente: volta ao rascunho do editor, sem gravar.
    float rgba[4];pickerLinear(rgba);
    if(state_.gradientField && state_.gradientSelected && !state_.gradientSelectedAlpha &&
       state_.gradientSelected<=gradientEdit_.colors.size()) {
      std::copy(rgba,rgba+3,gradientEdit_.colors[state_.gradientSelected-1].rgb);
      publishGradientDraft();
    }
    state_.colorField=0;return true;
  }
  const auto *entity=document_.find(state_.colorEntity);
  if(!entity||document_.revision()!=state_.colorRevision||isPlaying()||history_.isOpen()) {
    state_.colorField=0;state_.status="Cor cancelada: a cena mudou";return true;
  }
  float rgba[4];pickerLinear(rgba);
  if(state_.colorTarget==0) {
    auto values=*entity;const auto *component=values.components.findInstance(state_.colorInstance);
    const float rgb[3]{rgba[0],rgba[1],rgba[2]};
    if(component && scene::setComponentTriple(values.components,component->type().id,state_.colorProperty,rgb,state_.colorInstance)==scene::ComponentPropertyStatus::Applied &&
       history_.applyValues(document_,entity->id,values)) {state_.colorField=0;return true;}
    state_.status="Cor recusada pelo componente";return false;
  }
  const auto value=scene::scriptColorValue(rgba);
  if(state_.colorTarget==2) {
    const auto *script=scene::scriptBehavior(entity->components.findInstance(state_.colorInstance));
    auto items=script?scriptArrayItems(*script,state_.colorProperty):std::vector<std::string>{};
    if(!script || !state_.colorScriptElement || state_.colorScriptElement>items.size()) {state_.colorField=0;return false;}
    items[state_.colorScriptElement-1]=value;
    if(setScriptArray(entity->id,state_.colorInstance,state_.colorProperty,state_.colorScriptType,items)) {state_.colorField=0;return true;}
    return false;
  }
  EditorActionRequest request;request.version=sceneVersion();request.entity=entity->id;
  request.componentInstance=state_.colorInstance;request.action=EditorAction::ScriptProperty;
  request.componentProperty=state_.colorProperty;request.scriptPropertyType=state_.colorScriptType;request.scriptPropertyValue=value;
  if(dispatch(request).status==EditorActionStatus::Applied) {state_.colorField=0;return true;}
  state_.status="Cor recusada pelo campo";return false;
}

// Unity 6000.0 Manual/InspectorColorPicker. Quadrado, matiz e barras seguem o
// dedo enquanto ele se move (não só no toque); abas, hexadecimal, original e
// amostras são toques; toque longo numa amostra abre as ações dela.
bool EditorSession::handleColorWindow(const UiPointerEvent &event,const UiPointerRouting &routing) {
  const u32 key=routing.widgetId;
  const bool tracking=event.phase==UiPointerPhase::Down || (event.phase==UiPointerPhase::Move && routing.dragging) ||
                      event.phase==UiPointerPhase::Up;
  const auto unit=[](float value,float start,float size) {return size>0?std::clamp((value-start)/size,0.f,1.f):0.f;};
  if(tracking && key==widgetId(EditorWidget::ColorSquare)) {
    state_.colorSaturation=unit(event.position.x,layout_.colorSquare.x,layout_.colorSquare.width);
    state_.colorValue=1-unit(event.position.y,layout_.colorSquare.y,layout_.colorSquare.height);
    return true;
  }
  if(tracking && key==widgetId(EditorWidget::ColorHueStrip)) {
    state_.colorHue=std::min(unit(event.position.y,layout_.colorHue.y,layout_.colorHue.height),.9999f);
    return true;
  }
  if(tracking && key>=widgetId(EditorWidget::ColorSliderBase) && key<widgetId(EditorWidget::ColorSliderBase)+5) {
    const u32 slot=key-widgetId(EditorWidget::ColorSliderBase);
    const auto &bar=layout_.colorSliders[slot];
    const float t=unit(event.position.x,bar.x,bar.width);
    // A ordem das barras é a mesma da tela: três do modo, alfa, intensidade.
    if(slot<3) {
      if(state_.colorMode==2) {
        if(slot==0) state_.colorHue=std::min(t,.9999f);
        else if(slot==1) state_.colorSaturation=t;
        else state_.colorValue=t;
      } else {
        float srgb[3];pickerRgb(state_.colorHue,state_.colorSaturation,state_.colorValue,srgb);srgb[slot]=t;
        float h=state_.colorHue,s=0,v=0;srgbToHsv(srgb,h,s,v);
        state_.colorHue=h;state_.colorSaturation=s;state_.colorValue=v;
      }
    } else if(slot==3 && state_.colorHasAlpha) state_.colorAlpha=t;
    else if(state_.colorHdr) state_.colorIntensity=-10+20*t;
    return true;
  }
  if(!routing.tapped) return true;
  const bool held=routing.heldSeconds>=ui::kUiLongPressSeconds;
  const auto *library=colorLibraries_.currentOrNull();
  if(key==widgetId(EditorWidget::ColorCancel)) {state_.colorField=0;state_.colorSwatchMenu=0;state_.colorLibraryMenu=false;}
  else if(key==widgetId(EditorWidget::ColorApply)) commitColorWindow();
  else if(key>=widgetId(EditorWidget::ColorModeBase) && key<widgetId(EditorWidget::ColorModeBase)+3)
    state_.colorMode=static_cast<u8>(key-widgetId(EditorWidget::ColorModeBase));
  else if(key==widgetId(EditorWidget::ColorOriginal)) pickerFromLinear(state_.colorOriginal);
  else if(key==widgetId(EditorWidget::ColorEyedropper)) {
    state_.colorPicking=true;state_.colorSampling=false;pixelSampleRequest_=false;state_.colorSwatchMenu=0;state_.colorLibraryMenu=false;
    state_.status="Toque em qualquer ponto da tela para amostrar a cor";
  }
  else if(key==widgetId(EditorWidget::ColorHex)) state_.colorText=1;
  else if(key==widgetId(EditorWidget::ColorLibraryToggle)) {state_.colorLibraryMenu=!state_.colorLibraryMenu;state_.colorSwatchMenu=0;}
  else if(key==widgetId(EditorWidget::ColorLibraryNew)) state_.colorText=3;
  else if(key>=widgetId(EditorWidget::ColorLibraryBase) && key<widgetId(EditorWidget::ColorLibraryBase)+EditorValueLibraries::kMaximumLibraries) {
    if(colorLibraries_.select(key-widgetId(EditorWidget::ColorLibraryBase))) saveColorLibraries();
    state_.colorLibraryMenu=false;
  } else if(key==widgetId(EditorWidget::ColorSwatchAdd)) {
    float rgba[4];pickerLinear(rgba);
    if(colorLibraries_.add(colorLibraries_.nextName("Cor"),scene::scriptColorValue(rgba))) {saveColorLibraries();state_.status="Cor guardada na biblioteca";}
    else state_.status="Biblioteca cheia";
  } else if(key>=widgetId(EditorWidget::ColorSwatchBase) && key<widgetId(EditorWidget::ColorSwatchBase)+EditorValueLibraries::kMaximumEntries) {
    const u32 index=key-widgetId(EditorWidget::ColorSwatchBase);
    if(!library || index>=library->entries.size()) return true;
    state_.colorLibraryMenu=false;
    if(held) {state_.colorSwatchMenu=index+1;return true;}
    float rgba[4]{1,1,1,1};scene::parseScriptColor(library->entries[index].value,rgba);
    pickerFromLinear(rgba);state_.colorSwatchMenu=0;
  } else if(key>=widgetId(EditorWidget::ColorSwatchActionBase) && key<widgetId(EditorWidget::ColorSwatchActionBase)+5 && state_.colorSwatchMenu) {
    const u32 index=state_.colorSwatchMenu-1,action=key-widgetId(EditorWidget::ColorSwatchActionBase);
    float rgba[4];pickerLinear(rgba);
    bool changed=false;
    if(action==0) changed=colorLibraries_.replace(index,scene::scriptColorValue(rgba));
    else if(action==1) changed=index>0 && colorLibraries_.move(index,index-1);
    else if(action==2) changed=colorLibraries_.move(index,index+1);
    else if(action==3) {state_.colorText=2;return true;}
    else changed=colorLibraries_.remove(index);
    if(changed) saveColorLibraries();
    state_.colorSwatchMenu=0;
  } else {state_.colorSwatchMenu=0;state_.colorLibraryMenu=false;}
  return true;
}

void EditorSession::publishGradientDraft() {
  state_.gradientDraft=scene::scriptGradientValue(gradientEdit_);
}

void EditorSession::openGradientEditor(u32 key,std::string_view value,std::string_view type) {
  gradientEdit_={};
  scene::parseScriptGradient(value,gradientEdit_);
  state_.gradientField=key;state_.gradientType=std::string(type);state_.gradientSelected=0;state_.gradientSelectedAlpha=false;
  state_.gradientRemoving=false;state_.gradientPresetMenu=0;state_.gradientLibraryMenu=false;state_.gradientText=0;
  state_.colorRevision=document_.revision();
  publishGradientDraft();
  std::string error;
  if(!files_.rootPath().empty() && !gradientLibraries_.load(files_.rootPath(),EditorLibraryKind::Gradient,error)) state_.status=error;
  state_.gradientLibraries=&gradientLibraries_;
}

void EditorSession::saveGradientLibraries() {
  std::string error;
  if(!files_.rootPath().empty() && !gradientLibraries_.save(error)) state_.status=error;
}

bool EditorSession::commitGradientEditor() {
  const auto *entity=document_.find(state_.gradientEntity);
  bool hdr=false;scene::scriptGradientType(scene::scriptArrayElementType(state_.gradientType).empty()?
      std::string_view(state_.gradientType):scene::scriptArrayElementType(state_.gradientType),&hdr);
  if(!gradientEdit_.valid(hdr)) {state_.status="Gradiente fora do alcance do campo (cor HDR num campo comum)";return false;}
  if(!entity||document_.revision()!=state_.colorRevision||isPlaying()||history_.isOpen()) {
    state_.gradientField=0;state_.status="Gradiente cancelado: a cena mudou";return true;
  }
  const auto value=scene::scriptGradientValue(gradientEdit_);
  if(state_.gradientElement) {
    const auto *script=scene::scriptBehavior(entity->components.findInstance(state_.gradientInstance));
    auto items=script?scriptArrayItems(*script,state_.gradientProperty):std::vector<std::string>{};
    if(!script || state_.gradientElement>items.size()) {state_.gradientField=0;return false;}
    items[state_.gradientElement-1]=value;
    if(setScriptArray(entity->id,state_.gradientInstance,state_.gradientProperty,state_.gradientType,items)) {state_.gradientField=0;return true;}
    return false;
  }
  EditorActionRequest request;request.version=sceneVersion();request.entity=entity->id;
  request.componentInstance=state_.gradientInstance;request.action=EditorAction::ScriptProperty;
  request.componentProperty=state_.gradientProperty;request.scriptPropertyType=state_.gradientType;request.scriptPropertyValue=value;
  if(dispatch(request).status==EditorActionStatus::Applied) {state_.gradientField=0;return true;}
  state_.status="Gradiente recusado pelo campo";return false;
}

// Unity Gradient Editor. A parada escolhida segue o dedo em x; afastada da
// faixa mais de 48 dp ela fica marcada e some ao soltar (se não for a única).
// Reordenar por tempo mantém a escolha na mesma parada.
bool EditorSession::handleGradientEditor(const UiPointerEvent &event,const UiPointerRouting &routing) {
  const u32 key=routing.widgetId;
  const auto &bar=layout_.gradientBar;
  const auto timeAt=[&](float x) {return bar.width>0?std::clamp((x-bar.x)/bar.width,0.f,1.f):0.f;};
  const auto settle=[&](bool alpha,u32 selected) {
    // Ordena por tempo e devolve a nova posição da parada escolhida.
    std::vector<u32> order(alpha?gradientEdit_.alphas.size():gradientEdit_.colors.size());
    for(u32 i=0;i<order.size();++i) order[i]=i;
    if(alpha) {
      std::stable_sort(order.begin(),order.end(),[&](u32 a,u32 b){return gradientEdit_.alphas[a].time<gradientEdit_.alphas[b].time;});
      auto keys=gradientEdit_.alphas;for(u32 i=0;i<order.size();++i) gradientEdit_.alphas[i]=keys[order[i]];
    } else {
      std::stable_sort(order.begin(),order.end(),[&](u32 a,u32 b){return gradientEdit_.colors[a].time<gradientEdit_.colors[b].time;});
      auto keys=gradientEdit_.colors;for(u32 i=0;i<order.size();++i) gradientEdit_.colors[i]=keys[order[i]];
    }
    for(u32 i=0;i<order.size();++i) if(order[i]+1==selected) return i+1;
    return 0u;
  };
  const bool alphaStop=key>=widgetId(EditorWidget::GradientAlphaStopBase) && key<widgetId(EditorWidget::GradientAlphaStopBase)+8;
  const bool colorStop=key>=widgetId(EditorWidget::GradientColorStopBase) && key<widgetId(EditorWidget::GradientColorStopBase)+8;
  if(alphaStop || colorStop) {
    const u32 index=key-widgetId(alphaStop?EditorWidget::GradientAlphaStopBase:EditorWidget::GradientColorStopBase);
    if(event.phase==UiPointerPhase::Down) {
      state_.gradientSelected=index+1;state_.gradientSelectedAlpha=alphaStop;state_.gradientRemoving=false;
      gradientPointer_=event.pointerId;gradientPressY_=event.position.y;
      state_.gradientPresetMenu=0;state_.gradientLibraryMenu=false;
      return true;
    }
    if(event.pointerId!=gradientPointer_ || !state_.gradientSelected) return true;
    const usize count=alphaStop?gradientEdit_.alphas.size():gradientEdit_.colors.size();
    if(event.phase==UiPointerPhase::Move && routing.dragging) {
      const u32 i=state_.gradientSelected-1;
      if(i>=count) return true;
      state_.gradientRemoving=count>1 && std::abs(event.position.y-gradientPressY_)>48.f;
      if(alphaStop) gradientEdit_.alphas[i].time=timeAt(event.position.x);
      else gradientEdit_.colors[i].time=timeAt(event.position.x);
      state_.gradientSelected=settle(alphaStop,state_.gradientSelected);
      publishGradientDraft();
    } else if(event.phase==UiPointerPhase::Up || event.phase==UiPointerPhase::Cancel) {
      if(state_.gradientRemoving && event.phase==UiPointerPhase::Up && count>1) {
        const u32 i=state_.gradientSelected-1;
        if(alphaStop) gradientEdit_.alphas.erase(gradientEdit_.alphas.begin()+i);
        else gradientEdit_.colors.erase(gradientEdit_.colors.begin()+i);
        state_.gradientSelected=0;publishGradientDraft();state_.status="Parada apagada";
      }
      state_.gradientRemoving=false;gradientPointer_=0;
    }
    return true;
  }
  const bool dragging=event.phase==UiPointerPhase::Down || (event.phase==UiPointerPhase::Move && routing.dragging) || event.phase==UiPointerPhase::Up;
  if(dragging && key==widgetId(EditorWidget::GradientLocation) && state_.gradientSelected) {
    const auto &slider=layout_.gradientLocation;
    const float t=slider.width>0?std::clamp((event.position.x-slider.x)/slider.width,0.f,1.f):0.f;
    const bool alpha=state_.gradientSelectedAlpha;const u32 i=state_.gradientSelected-1;
    if(alpha && i<gradientEdit_.alphas.size()) gradientEdit_.alphas[i].time=t;
    else if(!alpha && i<gradientEdit_.colors.size()) gradientEdit_.colors[i].time=t;
    state_.gradientSelected=settle(alpha,state_.gradientSelected);publishGradientDraft();
    return true;
  }
  if(dragging && key==widgetId(EditorWidget::GradientAlphaValue) && state_.gradientSelected && state_.gradientSelectedAlpha) {
    const auto &slider=layout_.gradientAlpha;
    if(state_.gradientSelected<=gradientEdit_.alphas.size())
      gradientEdit_.alphas[state_.gradientSelected-1].alpha=slider.width>0?std::clamp((event.position.x-slider.x)/slider.width,0.f,1.f):0.f;
    publishGradientDraft();
    return true;
  }
  if(!routing.tapped) return true;
  const bool held=routing.heldSeconds>=ui::kUiLongPressSeconds;
  const auto *library=gradientLibraries_.currentOrNull();
  if(key==widgetId(EditorWidget::GradientCancel)) state_.gradientField=0;
  else if(key==widgetId(EditorWidget::GradientApply)) commitGradientEditor();
  else if(key>=widgetId(EditorWidget::GradientModeBase) && key<widgetId(EditorWidget::GradientModeBase)+3) {
    gradientEdit_.mode=static_cast<scene::GradientMode>(key-widgetId(EditorWidget::GradientModeBase));publishGradientDraft();
  } else if(key==widgetId(EditorWidget::GradientAlphaLane) || key==widgetId(EditorWidget::GradientColorLane)) {
    // Tocar no vazio da faixa acrescenta uma parada com o valor daquele ponto.
    const bool alpha=key==widgetId(EditorWidget::GradientAlphaLane);
    const float t=timeAt(event.position.x);
    float rgba[4];scene::evaluateScriptGradient(gradientEdit_,t,rgba);
    if((alpha?gradientEdit_.alphas.size():gradientEdit_.colors.size())>=scene::ScriptGradient::kMaximumKeys) {
      state_.status="No máximo 8 paradas de cada tipo";return true;
    }
    if(alpha) gradientEdit_.alphas.push_back({t,rgba[3]});
    else gradientEdit_.colors.push_back({t,{rgba[0],rgba[1],rgba[2]}});
    const u32 added=static_cast<u32>(alpha?gradientEdit_.alphas.size():gradientEdit_.colors.size());
    state_.gradientSelectedAlpha=alpha;state_.gradientSelected=settle(alpha,added);publishGradientDraft();
  } else if(key==widgetId(EditorWidget::GradientColorSwatch) && state_.gradientSelected && !state_.gradientSelectedAlpha &&
            state_.gradientSelected<=gradientEdit_.colors.size()) {
    const auto &color=gradientEdit_.colors[state_.gradientSelected-1];
    state_.colorTarget=3;
    openColorWindow(key,{color.rgb[0],color.rgb[1],color.rgb[2],1},false,state_.gradientType.ends_with(":hdr"));
  } else if(key==widgetId(EditorWidget::GradientDeleteStop) && state_.gradientSelected) {
    const u32 i=state_.gradientSelected-1;
    if(state_.gradientSelectedAlpha && gradientEdit_.alphas.size()>1 && i<gradientEdit_.alphas.size()) gradientEdit_.alphas.erase(gradientEdit_.alphas.begin()+i);
    else if(!state_.gradientSelectedAlpha && gradientEdit_.colors.size()>1 && i<gradientEdit_.colors.size()) gradientEdit_.colors.erase(gradientEdit_.colors.begin()+i);
    state_.gradientSelected=0;publishGradientDraft();
  } else if(key==widgetId(EditorWidget::GradientLibraryToggle)) {state_.gradientLibraryMenu=!state_.gradientLibraryMenu;state_.gradientPresetMenu=0;}
  else if(key==widgetId(EditorWidget::GradientLibraryNew)) state_.gradientText=3;
  else if(key>=widgetId(EditorWidget::GradientLibraryBase) && key<widgetId(EditorWidget::GradientLibraryBase)+EditorValueLibraries::kMaximumLibraries) {
    if(gradientLibraries_.select(key-widgetId(EditorWidget::GradientLibraryBase))) saveGradientLibraries();
    state_.gradientLibraryMenu=false;
  } else if(key==widgetId(EditorWidget::GradientPresetAdd)) {
    if(gradientLibraries_.add(gradientLibraries_.nextName("Gradiente"),scene::scriptGradientValue(gradientEdit_))) {
      saveGradientLibraries();state_.status="Gradiente guardado na biblioteca";
    } else state_.status="Biblioteca cheia";
  } else if(key>=widgetId(EditorWidget::GradientPresetBase) && key<widgetId(EditorWidget::GradientPresetBase)+EditorValueLibraries::kMaximumEntries) {
    const u32 index=key-widgetId(EditorWidget::GradientPresetBase);
    if(!library || index>=library->entries.size()) return true;
    state_.gradientLibraryMenu=false;
    if(held) {state_.gradientPresetMenu=index+1;return true;}
    scene::ScriptGradient preset;
    if(scene::parseScriptGradient(library->entries[index].value,preset)) {gradientEdit_=preset;state_.gradientSelected=0;publishGradientDraft();}
    state_.gradientPresetMenu=0;
  } else if(key>=widgetId(EditorWidget::GradientPresetActionBase) && key<widgetId(EditorWidget::GradientPresetActionBase)+5 && state_.gradientPresetMenu) {
    const u32 index=state_.gradientPresetMenu-1,action=key-widgetId(EditorWidget::GradientPresetActionBase);
    bool changed=false;
    if(action==0) changed=gradientLibraries_.replace(index,scene::scriptGradientValue(gradientEdit_));
    else if(action==1) changed=index>0 && gradientLibraries_.move(index,index-1);
    else if(action==2) changed=gradientLibraries_.move(index,index+1);
    else if(action==3) {state_.gradientText=2;return true;}
    else changed=gradientLibraries_.remove(index);
    if(changed) saveGradientLibraries();
    state_.gradientPresetMenu=0;
  } else {state_.gradientPresetMenu=0;state_.gradientLibraryMenu=false;}
  return true;
}

void EditorSession::publishCurveDraft() {
  curveEdit_.updateTangents();
  state_.curveDraft=scene::scriptCurveValue(curveEdit_);
}

u32 EditorSession::settleCurveSelection(u32 selected) {
  std::vector<u32> order(curveEdit_.keys.size());
  for(u32 i=0;i<order.size();++i) order[i]=i;
  std::stable_sort(order.begin(),order.end(),[&](u32 a,u32 b){return curveEdit_.keys[a].time<curveEdit_.keys[b].time;});
  const auto keys=curveEdit_.keys;
  for(u32 i=0;i<order.size();++i) curveEdit_.keys[i]=keys[order[i]];
  // Dois tempos iguais tornariam a curva ambígua: o recém-movido cede 1e-4.
  for(u32 i=1;i<curveEdit_.keys.size();++i)
    if(curveEdit_.keys[i].time<=curveEdit_.keys[i-1].time) curveEdit_.keys[i].time=curveEdit_.keys[i-1].time+1e-4f;
  for(u32 i=0;i<order.size();++i) if(order[i]+1==selected) return i+1;
  return 0;
}

void EditorSession::openCurveEditor(u32 key,std::string_view value,std::string_view type) {
  curveEdit_={};
  scene::parseScriptCurve(value,curveEdit_);
  state_.curveField=key;state_.curveType=std::string(type);state_.curveSelected=0;
  state_.curvePresetMenu=0;state_.curveLibraryMenu=false;state_.curveText=0;
  state_.colorRevision=document_.revision();curveDrag_=CurveDrag::None;curveLastTap_=-1;
  frameCurve(curveEdit_,state_.curveView);
  publishCurveDraft();
  std::string error;
  if(!files_.rootPath().empty() && !curveLibraries_.load(files_.rootPath(),EditorLibraryKind::Curve,error)) state_.status=error;
  state_.curveLibraries=&curveLibraries_;
}

void EditorSession::saveCurveLibraries() {
  std::string error;
  if(!files_.rootPath().empty() && !curveLibraries_.save(error)) state_.status=error;
}

bool EditorSession::commitCurveEditor() {
  const auto *entity=document_.find(state_.curveEntity);
  if(!entity||document_.revision()!=state_.colorRevision||isPlaying()||history_.isOpen()) {
    state_.curveField=0;state_.status="Curva cancelada: a cena mudou";return true;
  }
  const auto value=scene::scriptCurveValue(curveEdit_);
  if(state_.curveElement) {
    const auto *script=scene::scriptBehavior(entity->components.findInstance(state_.curveInstance));
    auto items=script?scriptArrayItems(*script,state_.curveProperty):std::vector<std::string>{};
    if(!script || state_.curveElement>items.size()) {state_.curveField=0;return false;}
    items[state_.curveElement-1]=value;
    if(setScriptArray(entity->id,state_.curveInstance,state_.curveProperty,state_.curveType,items)) {state_.curveField=0;return true;}
    return false;
  }
  EditorActionRequest request;request.version=sceneVersion();request.entity=entity->id;
  request.componentInstance=state_.curveInstance;request.action=EditorAction::ScriptProperty;
  request.componentProperty=state_.curveProperty;request.scriptPropertyType=state_.curveType;request.scriptPropertyValue=value;
  if(dispatch(request).status==EditorActionStatus::Applied) {state_.curveField=0;return true;}
  state_.status="Curva recusada pelo campo";return false;
}

// Unity Manual/EditingCurves adaptado ao toque: toque duplo acrescenta chave
// (o clique duplo da Unity), arrastar chave/alça edita, arrastar o vazio
// desloca a vista. Tangente arrastada vira Free (alinhada, ou só aquele lado
// quando a chave está quebrada).
bool EditorSession::handleCurveEditor(const UiPointerEvent &event,const UiPointerRouting &routing) {
  const u32 key=routing.widgetId;
  const auto &graph=layout_.curveGraph;
  if(key==widgetId(EditorWidget::CurveGraph)) {
    const auto distance=[](UiPoint a,UiPoint b){return std::hypot(a.x-b.x,a.y-b.y);};
    if(event.phase==UiPointerPhase::Down) {
      curvePointer_=event.pointerId;curvePress_=event.position;curveDrag_=CurveDrag::Pan;
      std::copy(state_.curveView,state_.curveView+4,curvePressView_);
      state_.curvePresetMenu=0;state_.curveLibraryMenu=false;
      // Alças da chave escolhida têm prioridade: ficam perto dela.
      if(state_.curveSelected && state_.curveSelected<=curveEdit_.keys.size()) {
        const auto &selected=curveEdit_.keys[state_.curveSelected-1];
        const bool first=state_.curveSelected==1,last=state_.curveSelected==curveEdit_.keys.size();
        if(!first && distance(event.position,curveHandle(graph,state_.curveView,selected,true))<22) curveDrag_=CurveDrag::HandleIn;
        else if(!last && distance(event.position,curveHandle(graph,state_.curveView,selected,false))<22) curveDrag_=CurveDrag::HandleOut;
      }
      if(curveDrag_==CurveDrag::Pan) {
        float best=24;
        for(u32 i=0;i<curveEdit_.keys.size();++i) {
          const float d=distance(event.position,curveToScreen(graph,state_.curveView,curveEdit_.keys[i].time,curveEdit_.keys[i].value));
          if(d<best) {
            best=d;state_.curveSelected=i+1;curveDrag_=CurveDrag::Key;
            curvePressKey_[0]=curveEdit_.keys[i].time;curvePressKey_[1]=curveEdit_.keys[i].value;
          }
        }
      }
      return true;
    }
    if(event.pointerId!=curvePointer_) return true;
    if(event.phase==UiPointerPhase::Move && routing.dragging) {
      float time=0,value=0;screenToCurve(graph,state_.curveView,event.position,time,value);
      if(curveDrag_==CurveDrag::Key && state_.curveSelected && graph.width>0 && graph.height>0) {
        // Deslocamento relativo ao toque: arrastar só na vertical não mexe no tempo.
        auto &moved=curveEdit_.keys[state_.curveSelected-1];
        moved.time=curvePressKey_[0]+(event.position.x-curvePress_.x)/graph.width*(state_.curveView[2]-state_.curveView[0]);
        moved.value=curvePressKey_[1]-(event.position.y-curvePress_.y)/graph.height*(state_.curveView[3]-state_.curveView[1]);
        state_.curveSelected=settleCurveSelection(state_.curveSelected);publishCurveDraft();
      } else if((curveDrag_==CurveDrag::HandleIn || curveDrag_==CurveDrag::HandleOut) && state_.curveSelected) {
        auto &edited=curveEdit_.keys[state_.curveSelected-1];
        const float dt=time-edited.time;
        const float slope=std::clamp(std::abs(dt)<1e-5f?(value>edited.value?1e4f:-1e4f):(value-edited.value)/dt,-1e4f,1e4f);
        const bool incoming=curveDrag_==CurveDrag::HandleIn;
        if(edited.broken) {
          (incoming?edited.left:edited.right)=scene::CurveTangentMode::Free;
          (incoming?edited.in:edited.out)=slope;
        } else {
          edited.left=edited.right=scene::CurveTangentMode::Free;edited.in=edited.out=slope;
        }
        publishCurveDraft();
      } else if(curveDrag_==CurveDrag::Pan && graph.width>0 && graph.height>0) {
        const float dt=(event.position.x-curvePress_.x)/graph.width*(curvePressView_[2]-curvePressView_[0]);
        const float dv=(event.position.y-curvePress_.y)/graph.height*(curvePressView_[3]-curvePressView_[1]);
        state_.curveView[0]=curvePressView_[0]-dt;state_.curveView[2]=curvePressView_[2]-dt;
        state_.curveView[1]=curvePressView_[1]+dv;state_.curveView[3]=curvePressView_[3]+dv;
      }
      return true;
    }
    if(event.phase==UiPointerPhase::Up) {
      if(routing.tapped && curveDrag_==CurveDrag::Pan) {
        // Toque no vazio: o segundo toque perto do primeiro, em até 0,4 s, acrescenta.
        const bool twice=curveLastTap_>=0 && event.timeSeconds-curveLastTap_<=.4 && distance(event.position,curveLastTapPoint_)<24;
        if(twice) {
          if(curveEdit_.keys.size()>=scene::ScriptCurve::kMaximumKeys) state_.status="No máximo 256 chaves";
          else {
            float time=0,value=0;screenToCurve(graph,state_.curveView,event.position,time,value);
            // Sobre a curva quando ela existe; senão onde o dedo tocou.
            if(!curveEdit_.keys.empty()) value=scene::evaluateScriptCurve(curveEdit_,time);
            curveEdit_.keys.push_back({time,value});
            state_.curveSelected=settleCurveSelection(static_cast<u32>(curveEdit_.keys.size()));publishCurveDraft();
          }
          curveLastTap_=-1;
        } else {state_.curveSelected=0;curveLastTap_=event.timeSeconds;curveLastTapPoint_=event.position;}
      }
      curveDrag_=CurveDrag::None;curvePointer_=0;
    }
    return true;
  }
  if(!routing.tapped) return true;
  const bool held=routing.heldSeconds>=ui::kUiLongPressSeconds;
  const auto *library=curveLibraries_.currentOrNull();
  const auto zoom=[&](float factor) {
    const float ct=(state_.curveView[0]+state_.curveView[2])*.5f,cv=(state_.curveView[1]+state_.curveView[3])*.5f;
    const float ht=(state_.curveView[2]-state_.curveView[0])*.5f*factor,hv=(state_.curveView[3]-state_.curveView[1])*.5f*factor;
    state_.curveView[0]=ct-ht;state_.curveView[2]=ct+ht;state_.curveView[1]=cv-hv;state_.curveView[3]=cv+hv;
  };
  auto *selected=state_.curveSelected && state_.curveSelected<=curveEdit_.keys.size()?&curveEdit_.keys[state_.curveSelected-1]:nullptr;
  if(key==widgetId(EditorWidget::CurveCancel)) state_.curveField=0;
  else if(key==widgetId(EditorWidget::CurveApply)) commitCurveEditor();
  else if(key==widgetId(EditorWidget::CurveFrame)) frameCurve(curveEdit_,state_.curveView);
  else if(key==widgetId(EditorWidget::CurveZoomIn)) zoom(.5f);
  else if(key==widgetId(EditorWidget::CurveZoomOut)) zoom(2.f);
  else if(key>=widgetId(EditorWidget::CurvePreWrapBase) && key<widgetId(EditorWidget::CurvePreWrapBase)+3) {
    curveEdit_.pre=static_cast<scene::CurveWrapMode>(key-widgetId(EditorWidget::CurvePreWrapBase));publishCurveDraft();
  } else if(key>=widgetId(EditorWidget::CurvePostWrapBase) && key<widgetId(EditorWidget::CurvePostWrapBase)+3) {
    curveEdit_.post=static_cast<scene::CurveWrapMode>(key-widgetId(EditorWidget::CurvePostWrapBase));publishCurveDraft();
  } else if(selected && (key==widgetId(EditorWidget::CurveKeyTime) || key==widgetId(EditorWidget::CurveKeyValue))) {
    // Teclado numérico (com expressões) sobre o rascunho.
    state_.numericField=key;state_.numericEntity=state_.curveEntity;state_.numericInstance=0;state_.numericProperty.clear();
    std::snprintf(state_.numericText,sizeof(state_.numericText),"%.9g",
                  static_cast<double>(key==widgetId(EditorWidget::CurveKeyTime)?selected->time:selected->value));
    state_.numericReplace=true;state_.numericError=false;
  } else if(selected && key==widgetId(EditorWidget::CurveKeyDelete)) {
    curveEdit_.keys.erase(curveEdit_.keys.begin()+(state_.curveSelected-1));state_.curveSelected=0;publishCurveDraft();
  } else if(selected && key>=widgetId(EditorWidget::CurveTangentBase) && key<widgetId(EditorWidget::CurveTangentBase)+5) {
    const u32 mode=key-widgetId(EditorWidget::CurveTangentBase);
    if(mode==4) {
      selected->broken=!selected->broken;
      if(!selected->broken) {selected->left=selected->right=scene::CurveTangentMode::Free;selected->out=selected->in;}
    } else {
      selected->broken=false;
      selected->left=selected->right=mode==0?scene::CurveTangentMode::ClampedAuto:mode==1?scene::CurveTangentMode::Auto:scene::CurveTangentMode::Free;
      if(mode==2) selected->out=selected->in;
      if(mode==3) selected->in=selected->out=0;
    }
    publishCurveDraft();
  } else if(selected && selected->broken && ((key>=widgetId(EditorWidget::CurveLeftModeBase) && key<widgetId(EditorWidget::CurveLeftModeBase)+3) ||
                                           (key>=widgetId(EditorWidget::CurveRightModeBase) && key<widgetId(EditorWidget::CurveRightModeBase)+3))) {
    const bool left=key<widgetId(EditorWidget::CurveRightModeBase);
    const u32 choice=key-widgetId(left?EditorWidget::CurveLeftModeBase:EditorWidget::CurveRightModeBase);
    (left?selected->left:selected->right)=choice==1?scene::CurveTangentMode::Linear:choice==2?scene::CurveTangentMode::Constant:scene::CurveTangentMode::Free;
    publishCurveDraft();
  } else if(key==widgetId(EditorWidget::CurveLibraryToggle)) {state_.curveLibraryMenu=!state_.curveLibraryMenu;state_.curvePresetMenu=0;}
  else if(key==widgetId(EditorWidget::CurveLibraryNew)) state_.curveText=3;
  else if(key==widgetId(EditorWidget::CurveLibraryFactory)) {
    // Unity: "Add Factory Presets To Current Library".
    u32 added=0;
    for(const auto &preset:scene::curveFactoryPresets) added+=curveLibraries_.add(preset.name,preset.value);
    if(added) saveCurveLibraries();
    state_.status=std::to_string(added)+" presets de fábrica na biblioteca";state_.curveLibraryMenu=false;
  } else if(key>=widgetId(EditorWidget::CurveLibraryBase) && key<widgetId(EditorWidget::CurveLibraryBase)+EditorValueLibraries::kMaximumLibraries) {
    if(curveLibraries_.select(key-widgetId(EditorWidget::CurveLibraryBase))) saveCurveLibraries();
    state_.curveLibraryMenu=false;
  } else if(key==widgetId(EditorWidget::CurvePresetAdd)) {
    if(curveLibraries_.add(curveLibraries_.nextName("Curva"),scene::scriptCurveValue(curveEdit_))) {
      saveCurveLibraries();state_.status="Curva guardada na biblioteca";
    } else state_.status="Biblioteca cheia";
  } else if(key>=widgetId(EditorWidget::CurvePresetBase) && key<widgetId(EditorWidget::CurvePresetBase)+EditorValueLibraries::kMaximumEntries) {
    const u32 index=key-widgetId(EditorWidget::CurvePresetBase);
    if(!library || index>=library->entries.size()) return true;
    state_.curveLibraryMenu=false;
    if(held) {state_.curvePresetMenu=index+1;return true;}
    scene::ScriptCurve preset;
    if(scene::parseScriptCurve(library->entries[index].value,preset)) {
      curveEdit_=preset;state_.curveSelected=0;publishCurveDraft();frameCurve(curveEdit_,state_.curveView);
    }
    state_.curvePresetMenu=0;
  } else if(key>=widgetId(EditorWidget::CurvePresetActionBase) && key<widgetId(EditorWidget::CurvePresetActionBase)+5 && state_.curvePresetMenu) {
    const u32 index=state_.curvePresetMenu-1,action=key-widgetId(EditorWidget::CurvePresetActionBase);
    bool changed=false;
    if(action==0) changed=curveLibraries_.replace(index,scene::scriptCurveValue(curveEdit_));
    else if(action==1) changed=index>0 && curveLibraries_.move(index,index-1);
    else if(action==2) changed=curveLibraries_.move(index,index+1);
    else if(action==3) {state_.curveText=2;return true;}
    else changed=curveLibraries_.remove(index);
    if(changed) saveCurveLibraries();
    state_.curvePresetMenu=0;
  } else {state_.curvePresetMenu=0;state_.curveLibraryMenu=false;}
  return true;
}

// Unity Manual/InspectorBarSliders e LOD Group. O divisor i é a transição do
// LOD i: arrastar muda só ela, presa entre as vizinhas (a ordem estritamente
// decrescente é o que o componente exige). Toque escolhe o nível; toque longo
// abre Inserir antes/Apagar, que deslocam níveis, referências e larguras de
// fade juntos.
bool EditorSession::handleLodBar(const UiPointerEvent &event,const UiPointerRouting &routing) {
  const u32 key=routing.widgetId;
  const u32 dividers=widgetId(EditorWidget::LodBarDividerBase),segments=widgetId(EditorWidget::LodBarSegmentBase);
  const bool divider=key>=dividers && key<dividers+scene::LodGroupMaximumLevels;
  const bool segment=key>=segments && key<=segments+scene::LodGroupMaximumLevels;
  const bool menu=key==widgetId(EditorWidget::LodBarInsert) || key==widgetId(EditorWidget::LodBarDelete);
  if(!divider && !segment && !menu && !lodDivider_) return false;
  const auto *entity=document_.find(state_.selection);
  const auto *group=entity?static_cast<const scene::LodGroup *>(entity->components.find(scene::LodGroup::descriptor)):nullptr;
  if(!group) return true;
  if(divider || lodDivider_) {
    if(event.phase==UiPointerPhase::Down && !lodDivider_) {
      if(history_.isOpen() || !history_.begin("Transição de LOD")) return true;
      lodDivider_=key-dividers+1;lodPointer_=event.pointerId;state_.lodMenu=0;
    }
    if(event.pointerId!=lodPointer_) return true;
    const u32 i=lodDivider_-1;
    if(event.phase==UiPointerPhase::Cancel) {history_.cancel(document_);lodDivider_=0;return true;}
    const auto &bar=layout_.lodBar;
    if(bar.width>0 && i<group->levelCount) {
      const float upper=i?group->transitions[i-1]-.1f:100.f,lower=i+1<group->levelCount?group->transitions[i+1]+.1f:.1f;
      const float percent=std::clamp(100.f*(1-(event.position.x-bar.x)/bar.width),lower,upper);
      auto value=*entity;
      if(scene::setComponentProperty(value.components,scene::LodGroup::descriptor.id,"transition_"+std::to_string(i),percent,
                                     group->instanceId())==scene::ComponentPropertyStatus::Applied)
        history_.applyValues(document_,entity->id,value);
    }
    if(event.phase==UiPointerPhase::Up) {history_.end();lodDivider_=0;lodPointer_=0;state_.status="Transição de LOD ajustada";}
    return true;
  }
  if(!routing.tapped) return true;
  if(segment) {
    const u32 i=key-segments;
    if(routing.heldSeconds>=ui::kUiLongPressSeconds) state_.lodMenu=i+1;
    else {state_.lodSelected=i+1;state_.lodMenu=0;}
    return true;
  }
  if(!state_.lodMenu || history_.isOpen()) return true;
  const u32 at=state_.lodMenu-1;state_.lodMenu=0;
  auto value=*entity;
  auto *edited=static_cast<scene::LodGroup *>(value.components.editInstance(group->instanceId()));
  if(!edited) return true;
  if(key==widgetId(EditorWidget::LodBarInsert) && edited->levelCount<scene::LodGroupMaximumLevels) {
    // O nível novo entra antes do escolhido (antes do Culled = no fim), no
    // meio da faixa que aquele segmento ocupava.
    const u32 i=std::min(at,edited->levelCount);
    const float upper=i?edited->transitions[i-1]:100.f,lower=i<edited->levelCount?edited->transitions[i]:0.f;
    for(u32 k=edited->levelCount;k>i;--k) {
      edited->transitions[k]=edited->transitions[k-1];edited->levels[k]=edited->levels[k-1];edited->fadeWidths[k]=edited->fadeWidths[k-1];
    }
    edited->transitions[i]=(upper+lower)*.5f;edited->levels[i]=0;edited->fadeWidths[i]=.2f;++edited->levelCount;
  } else if(key==widgetId(EditorWidget::LodBarDelete) && at<edited->levelCount && edited->levelCount>1) {
    for(u32 k=at;k+1<edited->levelCount;++k) {
      edited->transitions[k]=edited->transitions[k+1];edited->levels[k]=edited->levels[k+1];edited->fadeWidths[k]=edited->fadeWidths[k+1];
    }
    --edited->levelCount;edited->levels[edited->levelCount]=0;
  } else return true;
  if(!edited->valid() || !history_.applyValues(document_,entity->id,value)) {state_.status="Níveis de LOD recusados";return true;}
  state_.lodSelected=0;state_.status="Níveis de LOD alterados";
  return true;
}

// Unity Manual/UsingComponents: arrastar o cabeçalho de um componente muda a
// ordem dele no objeto. Aqui o gesto nasce de um arraste VERTICAL no cabeçalho
// (o toque curto continua abrindo o cartão e o longo, o menu), o destino é o
// cabeçalho sob o dedo e as setas de página viram ao passar por elas, porque o
// Inspector pagina em vez de rolar. O movimento é o mesmo `moveInstance` de
// Subir/Descer: identidade preservada e um passo de Desfazer.
bool EditorSession::handleComponentReorder(const UiPointerEvent &event,const UiPointerRouting &routing) {
  const auto header=[&](u32 widget) {
    const u32 operation=widget&0xff000000u;
    return operation==widgetId(EditorWidget::ComponentFoldBase) || operation==widgetId(EditorWidget::ScriptFoldBase);
  };
  if(!state_.componentReorder) {
    if(!header(routing.widgetId) || !routing.dragging || event.phase!=UiPointerPhase::Move || history_.isOpen() ||
       std::abs(routing.totalDelta.y)<18.0f || std::abs(routing.totalDelta.y)<std::abs(routing.totalDelta.x)*1.5f) return false;
    const auto *entity=document_.find(state_.selection);
    const u32 index=routing.widgetId&0x00ffffffu;
    if(!entity || index>=entity->components.size()) return false;
    state_.componentReorder=index+1;state_.componentReorderTarget=0;reorderPointer_=event.pointerId;reorderPagerHover_=0;
    state_.nativeMenu=0;state_.scriptMenu=0;
    state_.status="Solte sobre outro cabeçalho para mudar a ordem";
  }
  if(event.pointerId!=reorderPointer_) return true;
  state_.componentReorderPoint=event.position;
  const auto hover=router_.hitTest(event.position);
  const u32 over=hover.target==UiPointerTarget::Widget?hover.widgetId:0;
  state_.componentReorderTarget=header(over)?(over&0x00ffffffu)+1:0;
  if(event.phase==UiPointerPhase::Move && over!=reorderPagerHover_) {
    reorderPagerHover_=over;
    // Mesma aritmética do toque nas setas: parte da página desenhada.
    if(over==widgetId(EditorWidget::ComponentNext) || over==widgetId(EditorWidget::ComponentPrevious)) {
      state_.componentPage=layout_.componentPage;
      if(over==widgetId(EditorWidget::ComponentNext)) ++state_.componentPage;
      else if(state_.componentPage) --state_.componentPage;
      state_.componentSelection=state_.selection;
    }
  }
  if(event.phase!=UiPointerPhase::Up && event.phase!=UiPointerPhase::Cancel) return true;
  const u32 from=state_.componentReorder-1,to=state_.componentReorderTarget;
  state_.componentReorder=0;state_.componentReorderTarget=0;reorderPointer_=0;reorderPagerHover_=0;
  if(event.phase==UiPointerPhase::Cancel || !to || to-1==from) {state_.status="Ordem mantida";return true;}
  const auto *entity=document_.find(state_.selection);
  if(!entity || from>=entity->components.size() || history_.isOpen()) return true;
  auto value=*entity;
  if(!value.components.moveInstance(entity->components.at(from)->instanceId(),to-1) ||
     !history_.applyValues(document_,entity->id,value)) {state_.status="Não foi possível mudar a ordem";return true;}
  state_.status="Componente movido";
  return true;
}

void EditorSession::cancelPointers() {
  if(topologyDragOpen_){colliderTopology_.vertices=topologyDragBase_;const auto *c=inspectedCollider(document_,colliderTopology_.object,colliderTopology_.instance);if(c)colliderTopology_.validate(c->hullTolerance);topologyDragOpen_=false;}
  guiAuthorPointers_.clear();guiAuthorPrepared_=false;
  gui_.cancelPointers();playScene_.gui().cancelPointers();playScene_.sceneGui().cancelPointers();
  state_.componentReorder=0;state_.componentReorderTarget=0;reorderPointer_=0;
  if(lodDivider_) {history_.cancel(document_);lodDivider_=0;lodPointer_=0;}
  if(lensDragOpen_) {history_.cancel(document_);lensDragOpen_=false;}
  if(componentDragOpen_) {history_.cancel(document_);componentDragOpen_=false;}
  if(colliderDragOpen_) {history_.cancel(document_);colliderDragOpen_=false;state_.activeColliderHandle=0;}
  finishCameraGesture(true);
  playTouches_.cancel();
  jumpPressed_=false;secondaryPressed_=false;
  cancelPlayButtons();
  state_.draggingAsset=false;
  state_.draggingEntity=kInvalidEntity;
  router_.cancelAllPointers();
  viewportPointers_.clear();
  pinchDistance_ = 0.0f;
  if (gizmoTransactionOpen_ || fieldWidget_ != 0) history_.end();
  fieldWidget_ = 0;
  gizmoTransactionOpen_ = false;
  gizmoDrag_ = EditorGizmoDrag{};
  state_.activeGizmoAxis = EditorGizmoHandle::None;
}

void EditorSession::advanceClock(float wallSeconds) noexcept {
  if(!isPlaying() && playScene_.active()) {endPlayInspect();playScene_.stop();runtimeTextures_.clear();}
  else if(isPlaying() && !state_.playInspect && playMirrorValid_) endPlayInspect();
  if (!std::isfinite(wallSeconds)) return;
  if (!clockPrimed_) {
    lastWallSeconds_ = wallSeconds;
    clockPrimed_ = true;
    return;
  }
  const float delta = wallSeconds - lastWallSeconds_;
  lastWallSeconds_ = wallSeconds;
  guiDeltaSeconds_=delta>0 && delta<=1?delta:0;
  // Passo negativo ou absurdo é o relógio de parede saltando (retomada do app,
  // mudança de fonte de tempo). Descartar é melhor do que teleportar a
  // simulação para um futuro que ninguém viu acontecer.
  if (!isPlaying() || state_.playPaused || delta <= 0.0f || delta > 1.0f) return;
  sceneTime_ += delta;
}

void EditorSession::frameSelection() {
  finishCameraGesture(false);state_.cameraViewEntity=0;state_.cameraPiloting=false;
  const EditorEntity *entity = document_.find(state_.selection);
  if (entity == nullptr) return;
  if(!meshAsset(*entity)) {frameSubtree(entity->id);return;}
  float center[3]{entity->transform.position[0],entity->transform.position[1],entity->transform.position[2]};
  float radius=0;
  if(!mapScene_.bounds(document_, entity->id, center, radius)) {
    float world[16];if(editorWorldMatrix(document_,entity->id,world)) std::copy(world+12,world+15,camera_.target);
    return;
  }
  renderer::PerspectiveVisibilitySettings fit=projection_;
  if(layout_.viewport.height>0 && layout_.viewport.width<layout_.viewport.height)
    fit.verticalFieldOfViewRadians=2*std::atan(std::tan(fit.verticalFieldOfViewRadians*0.5f)*
      layout_.viewport.width/layout_.viewport.height);
  frameEditorCamera(camera_, center, std::max(radius, 0.01f), fit);
}

bool EditorSession::save(const char *path, u64 fingerprint) {
  state_.saveRequested=false;
  const bool ok=saveEditorDocument(path,document_,fingerprint);
  if(ok && path) {scenePath_=path;sceneFingerprint_=fingerprint;}
  state_.status=ok?"Salvo":"Erro ao salvar";
  return ok;
}

bool EditorSession::load(const char *path, u64 fingerprint) {
  if(isPlaying()) return false;
  EditorDocument candidate;
  if(!loadEditorDocument(path,fingerprint,candidate)) return false;
  candidate.setTags(projectTags_);
  // Reconciliar ANTES de conferir a extração: a cena pode trazer identidades
  // cujos slots mudaram, e a checagem tem de valer para o documento que vai
  // realmente ser adotado.
  mapScene_.reconcileAssets(candidate);
  // Vínculos de importação (M08.2): a cena pode ter sido salva antes da última
  // reimportação, e objetos de cenas antigas ganham vínculo quando há prova.
  // Sem histórico: é o estado de abertura, não uma edição a desfazer.
  ImportReconcileReport reconcile;
  for(const auto &block:importedSources_) if(block.map.revision) {
    adoptLegacyImportInstances(candidate,nullptr,block.guid,block.map,reconcile);
    reconcileImportInstances(candidate,nullptr,block.guid,block.map,reconcile,
                             [this](const resources::AssetGuid &guid){return mapScene_.assetSlot(guid);});
  }
  if(reconcile.changed()) mapScene_.reconcileAssets(candidate);
  std::vector<renderer::MapDrawState> check;
  if(!mapScene_.extract(candidate,check)) return false;
  mapScene_.hydrateMaterials(candidate);
  cancelPointers();document_=std::move(candidate);history_.clear();
  scenePath_=path;sceneFingerprint_=fingerprint;
  guiTree_.clear();state_.guiRows={};state_.guiSelection={};state_.guiInspector=false;state_.collapsedGui.clear();
  state_.cameraViewEntity=0;state_.cameraPiloting=false;state_.componentGroup.clear();
  sceneEpoch_=nextSceneEpoch();state_.groupPicker=false;state_.editingGroupName=false;state_.groupEntity=0;cameraPreview_.close();state_.colorField=0;state_.impactInstance=0;state_.impactAsset={};state_.impactTrail.clear();state_.impactRepair=false;state_.impactReplacement={};state_.impactRepairMaterial={};
  // R4: a cena aberta pode usar texturas do projeto que a biblioteca atual ainda
  // não publicou.
  anticipatedTextures_.clear();
  if(std::string texturePublish;!ensureTexturesPublished(texturePublish))
    reportProblem(EditorConsoleSeverity::Warning,"Texturas do projeto não publicadas: "+texturePublish);
  reportImportReconcile(reconcile,"Cena aberta");
  state_.selection=kInvalidEntity;state_.status="Cena restaurada";
  state_.collapsedEntities.clear();state_.hierarchyScroll=0;state_.renameEntity=kInvalidEntity;
  validateFocusedInspectors();
  restoreSceneVisibility();
  return true;
}

EditorSession::ImportedLibrary EditorSession::flattenSources(const std::vector<ImportedSource> &sources) {
  ImportedLibrary library;
  for(const auto &source:sources) {
    const auto vertexBase=static_cast<u32>(library.vertices.size()/renderer::MapVertexStride);
    const auto indexBase=static_cast<u32>(library.indices.size());
    const auto materialBase=static_cast<u32>(library.materials.size());
    library.vertices.insert(library.vertices.end(),source.vertices.begin(),source.vertices.end());
    library.indices.insert(library.indices.end(),source.indices.begin(),source.indices.end());
    // Cada fonte numera as próprias texturas a partir de zero; na biblioteca
    // achatada, os índices dos materiais andam junto com o bloco.
    const auto textureBase=static_cast<u32>(library.textures.size());
    for(auto material:source.materials) {
      for(auto &texture:material.textureIndices) if(texture!=renderer::InvalidMapTexture) texture+=textureBase;
      library.materials.push_back(material);
    }
    library.textures.insert(library.textures.end(),source.textures.begin(),source.textures.end());
    library.textureSources.insert(library.textureSources.end(),source.textures.size(),source.guid);
    const auto drawBase=static_cast<u32>(library.draws.size());
    for(const auto &draw:source.draws) {
      auto moved=draw;
      moved.firstIndex+=indexBase;moved.vertexOffset+=vertexBase;moved.materialIndex+=materialBase;
      library.draws.push_back(moved);
    }
    for(auto lod:source.meshLods) {
      if(lod.draw>=source.draws.size()) continue;
      lod.draw+=drawBase;lod.firstIndex+=indexBase;
      library.meshLods.push_back(lod);
    }
    library.identities.insert(library.identities.end(),source.identities.begin(),source.identities.end());
    library.names.insert(library.names.end(),source.names.begin(),source.names.end());
    // G6-B: skin e blend shapes viajam por desenho. Derivados (colisão) são
    // estáticos: são geometria de física, não o corpo deformado.
    std::vector<std::shared_ptr<const resources::SkinDefinition>> skins;
    for(const auto &skin:source.skins)
      skins.push_back(skin.joints.empty()?nullptr:std::make_shared<const resources::SkinDefinition>(skin));
    std::vector<std::shared_ptr<const resources::MorphTargetSet>> morphs;
    std::vector<u32> morphOffsets;
    for(const auto &set:source.morphs) {
      morphOffsets.push_back(static_cast<u32>(library.morphDeltas.size()));
      library.morphDeltas.insert(library.morphDeltas.end(),set.deltas.begin(),set.deltas.end());
      morphs.push_back(std::make_shared<const resources::MorphTargetSet>(set));
    }
    const bool influences=!source.skinInfluences.empty() &&
        source.skinInfluences.size()==source.vertices.size()/renderer::MapVertexStride*resources::SkinInfluenceStride;
    for(usize i=0;i<source.draws.size();++i) {
      const bool authored=i<source.sourceDrawCount;
      const i32 skin=authored && i<source.drawSkins.size()?source.drawSkins[i]:-1;
      const i32 morph=authored && i<source.drawMorphs.size()?source.drawMorphs[i]:-1;
      auto deformation=std::make_shared<EditorMapScene::DrawDeformation>();
      deformation->skin=influences && skin>=0 && static_cast<usize>(skin)<skins.size()?skins[static_cast<usize>(skin)]:nullptr;
      deformation->morph=morph>=0 && static_cast<usize>(morph)<morphs.size()?morphs[static_cast<usize>(morph)]:nullptr;
      library.drawJoints.push_back(deformation->skin?static_cast<u32>(deformation->skin->joints.size()):0u);
      library.drawMorphOffsets.push_back(deformation->morph?morphOffsets[static_cast<usize>(morph)]:~0u);
      library.drawMorphTargets.push_back(deformation->morph?deformation->morph->targetCount:0u);
      if(!deformation->skin && !deformation->morph) {library.deformations.push_back(nullptr);continue;}
      // Cópia de CPU da faixa do desenho, para seleção e limites deformados.
      const auto &draw=source.draws[i];
      u32 highest=0;
      for(u32 k=0;k<draw.indexCount;++k) highest=std::max(highest,source.indices[draw.firstIndex+k]);
      deformation->localIndices.assign(source.indices.begin()+draw.firstIndex,source.indices.begin()+draw.firstIndex+draw.indexCount);
      deformation->restPositions.resize(usize(highest+1)*3);
      for(u32 v=0;v<=highest;++v)
        std::memcpy(deformation->restPositions.data()+v*3,
                    source.vertices.data()+(usize(draw.vertexOffset)+v)*renderer::MapVertexStride,12);
      if(deformation->skin)
        deformation->influences.assign(source.skinInfluences.begin()+usize(draw.vertexOffset)*resources::SkinInfluenceStride,
                                       source.skinInfluences.begin()+(usize(draw.vertexOffset)+highest+1)*resources::SkinInfluenceStride);
      library.deformations.push_back(std::move(deformation));
    }
    if(influences || !library.skinInfluences.empty()) {
      library.skinInfluences.resize(static_cast<usize>(vertexBase)*resources::SkinInfluenceStride,0);
      if(influences) library.skinInfluences.insert(library.skinInfluences.end(),source.skinInfluences.begin(),source.skinInfluences.end());
      else library.skinInfluences.resize(library.vertices.size()/renderer::MapVertexStride*resources::SkinInfluenceStride,0);
    }
    if(!source.animations.empty() && source.map.nodes.size()==source.nodes.size()) {
      auto animations=std::make_shared<runtime::SourceAnimations>();
      animations->source=source.guid;
      animations->clips=source.animations;
      // Identidade de clipe: fonte + nome + ordem entre clipes de mesmo nome.
      animations->clipIds=resources::animationClipGuids(source.guid,source.animations);
      for(usize n=0;n<source.map.nodes.size();++n) {
        animations->nodes.push_back(source.map.nodes[n].id);
        animations->nodeNames.push_back(importEntityName(source.nodes[n].name));
      }
      library.animations.push_back(std::move(animations));
    }
    // Pivô na origem do nó: a geometria importada já vive no espaço dele.
    for(usize i=0;i<source.draws.size();++i) {library.pivots.push_back(0);library.pivots.push_back(0);library.pivots.push_back(0);}
  }
  return library;
}

// Sobe a biblioteca para o consumidor gráfico e faz o editor adotar o pacote
// que voltou. Um caminho só, usado pela importação e pela reidratação: eram
// duas cópias desta sequência que deixavam uma delas para trás.
bool EditorSession::publishAndAdopt(const ImportedLibrary &library, std::string &diagnostic,
                                    usize *outPrimitives) {
  if(!publishGeometry_) { diagnostic="Este ambiente não publica geometria importada."; return false; }
  PublishedGeometry published;
  // R2: o orçamento agregado reduz a residência antes de subir. As fontes (e o
  // derivado em cache) guardam as cadeias completas; subir o teto e republicar
  // devolve a resolução sem reimportar nada.
  auto textures=library.textures;
  // R4: texturas do projeto usadas por slots e materiais entram depois das da
  // biblioteca das fontes; a tabela de bindings diz onde cada uma ficou.
  std::vector<EditorMapScene::TextureBinding> bindings;
  {
    std::vector<UsedTexture> used;
    collectUsedTextures(used);
    for(const auto &item:used)
      if(auto decoded=decodeProjectTexture(item.guid,item.srgb,item.sampler,item.normal)) {
        bindings.push_back({item.guid,item.srgb,item.sampler,static_cast<u32>(textures.size()),item.normal});
        textures.push_back(std::move(decoded));
      }
  }
  const auto residency=resources::applyTextureBudget(textures,importTextureBudget_,importLimits_.minimumTextureDimension);
  // R4 (T16): o que cada textura do projeto ocupa de fato na GPU, depois do perfil e do orçamento.
  std::vector<std::pair<EditorMapScene::TextureBinding,TextureResidency>> publishedResidency;
  for(const auto &binding:bindings)
    if(binding.index<textures.size() && textures[binding.index]) {
      const auto &texture=*textures[binding.index];
      publishedResidency.push_back({binding,{texture.width,texture.height,texture.levels,texture.expectedBytes(),texture.srgb,texture.samplerFlags}});
    }
  // S2: o que cada textura publicada pede ao streaming, pela mesma origem que
  // decidiu os bytes: o perfil de importação da fonte ou o perfil da textura.
  std::vector<renderer::TextureStreamingParameters> streaming(textures.size());
  {
    resources::AssetGuid lastSource{};resources::ImportProfile sourceProfile{};bool haveSource=false;
    for(usize i=0;i<library.textureSources.size() && i<streaming.size();++i) {
      if(!haveSource || !(library.textureSources[i]==lastSource)) {
        lastSource=library.textureSources[i];sourceProfile=importProfileFor(lastSource);haveSource=true;
      }
      streaming[i].streamable=sourceProfile.textureStreaming;streaming[i].priority=sourceProfile.textureStreamingPriority;
    }
    for(const auto &binding:bindings)
      if(binding.index<streaming.size()) {
        const auto profile=textureProfileFor(binding.guid);
        streaming[binding.index].streamable=profile.streamingMipmaps;streaming[binding.index].priority=profile.streamingPriority;
      }
  }
  skinningPublication_={library.skinInfluences,library.drawJoints,library.morphDeltas,library.drawMorphOffsets,
                        library.drawMorphTargets,library.meshLods};
  const bool accepted=publishGeometry_(library.vertices,library.indices,library.draws,library.materials,textures,published);
  if(accepted) {
    textureStreamingParameters_=std::move(streaming);++textureStreamingRevision_;
    textureStreamingDesired_.clear();textureStreamingLoaded_.clear();
  }
  skinningPublication_={};
  if(!accepted) {
    diagnostic="O consumidor gráfico recusou a geometria importada.";return false;
  }
  const bool residencyChanged=residency.residentBytes!=textureResidency_.residentBytes ||
                              residency.reducedTextures!=textureResidency_.reducedTextures ||
                              residency.withinBudget()!=textureResidency_.withinBudget();
  textureResidency_=residency;
  if(residencyChanged && (residency.reducedTextures || !residency.withinBudget())) {
    const auto mb=[](u64 bytes){return std::to_string((bytes+(u64{1}<<19))>>20);};
    reportProblem(residency.withinBudget()?EditorConsoleSeverity::Info:EditorConsoleSeverity::Warning,
        "Texturas do projeto: "+mb(residency.requestedBytes)+" MB pedidos, "+mb(residency.residentBytes)+
        " MB residentes (teto "+mb(residency.budgetBytes)+" MB); "+std::to_string(residency.reducedTextures)+
        " textura(s) com resolução reduzida"+(residency.withinBudget()?".":"; todas no piso e ainda acima do teto."));
  }
  // O pacote publicado é primitivas internas + biblioteca, nessa ordem. As
  // O cubo mantém a identidade antiga; as demais têm GUID por tipo e versão.
  if(published.draws.size()<library.draws.size()) { diagnostic="Pacote publicado inconsistente."; return false; }
  const auto primitives=published.draws.size()-library.draws.size();
  if(outPrimitives) *outPrimitives=primitives;
  std::vector<resources::AssetGuid> identities(primitives);
  identities.insert(identities.end(),library.identities.begin(),library.identities.end());
  std::vector<std::string> names;names.reserve(published.draws.size());
  for(usize i=0;i<primitives;++i) {
    const auto material=published.draws[i].materialIndex;
    const auto type=renderer::primitiveFromFlags(material<published.materials.size()?published.materials[material].flags:0);
    names.push_back(scene::validPrimitive(type)?scene::primitiveNames[static_cast<u32>(type)]:"Primitiva "+std::to_string(i+1));
  }
  names.insert(names.end(),library.names.begin(),library.names.end());
  // Pivô: as primitivas internas mantêm a convenção do pacote (centro dos
  // limites), a geometria importada usa a origem do nó.
  std::vector<float> pivots;
  pivots.reserve(published.draws.size()*3);
  for(usize i=0;i<primitives;++i)
    for(u32 axis=0;axis<3;++axis) pivots.push_back(published.draws[i].boundsCenter[axis]);
  pivots.insert(pivots.end(),library.pivots.begin(),library.pivots.end());
  if(pivots.size()!=published.draws.size()*3) { diagnostic="Pacote publicado inconsistente."; return false; }
  cancelPointers();
  if(!mapScene_.adoptPackage(document_,published.draws,published.materials,published.vertices,
                             published.indices,identities,packageFingerprint_,pivots,names)) {
    diagnostic="O editor recusou o pacote publicado.";return false;
  }
  publishedTextures_=std::move(publishedResidency);
  mapScene_.setTextureLibrary(std::move(bindings));
  std::vector<std::shared_ptr<const EditorMapScene::DrawDeformation>> deformations(primitives);
  deformations.insert(deformations.end(),library.deformations.begin(),library.deformations.end());
  mapScene_.setDeformation(std::move(deformations),library.animations);
  return true;
}

// Quantos objetos da cena usam este recurso. E a pergunta que apagar precisa
// responder ANTES de apagar.
namespace {
bool readProjectImage(const std::string &root,const std::string &relative,const resources::ImageDecodeLimits &limits,
                      resources::DecodedImage &out) {
  std::filesystem::path absolute;std::vector<u8> bytes;std::string diagnostic;
  return !root.empty() && EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),relative,absolute) &&
         EditorImportTransaction::read(absolute,bytes,limits.maximumEncodedBytes) &&
         resources::decodeImageRgba8(bytes,limits,out,diagnostic);
}
} // namespace

void EditorSession::collectSourceTextures() {
  sourceTextures_.clear();
  u32 library=0;
  for(const auto &source:importedSources_) {
    const auto *record=assets_.find(source.guid);
    for(u32 t=0;t<source.textures.size();++t,++library) {
      SourceTexture entry;
      entry.library=library;entry.source=source.guid;entry.texture=source.textures[t];
      entry.sourcePath=record?record->path:std::string();
      entry.image=t<source.textureImages.size()?source.textureImages[t]:std::string();
      const auto slash=entry.image.find_last_of('/');
      entry.name=entry.image.empty()?"Textura "+std::to_string(t+1):entry.image.substr(slash==std::string::npos?0:slash+1);
      // Quem usa: as malhas da fonte cujo material amostra esta textura.
      for(usize d=0;d<source.draws.size() && d<source.identities.size() && d<source.sourceDrawCount;++d) {
        const auto material=source.draws[d].materialIndex;
        if(material>=source.materials.size()) continue;
        const auto &indices=source.materials[material].textureIndices;
        if(std::find(std::begin(indices),std::end(indices),t)!=std::end(indices)) entry.meshes.push_back(source.identities[d]);
      }
      sourceTextures_.push_back(std::move(entry));
    }
  }
}

bool EditorSession::revealInFiles(const std::string &relativePath) {
  if(!state_.files || relativePath.empty()) return false;
  // Abre cada pasta do caminho que ainda estiver fechada; a árvore cresce a cada toggle.
  for(usize slash=relativePath.find('/');;slash=relativePath.find('/',slash+1)) {
    const std::string folder=relativePath.substr(0,slash);
    const auto &tree=files_.tree();
    for(u32 i=0;i<tree.size();++i)
      if(tree[i].directory && tree[i].relativePath==folder) {if(!tree[i].expanded) files_.toggle(i);break;}
    if(slash==std::string::npos) break;
  }
  const auto &tree=files_.tree();
  for(u32 i=0;i<tree.size();++i) if(tree[i].relativePath==relativePath) {
    state_.selectedFile=relativePath;state_.filesCollapsed=false;
    const float row=state_.workspace==EditorWorkspace::Code?34.0f:24.0f;
    const auto visible=static_cast<u32>(std::max(row,layout_.filesPanel.height-(state_.workspace==EditorWorkspace::Code?126:66))/row);
    const u32 first=i>visible/2?i-visible/2:0u;
    const u32 maximum=tree.size()>visible?static_cast<u32>(tree.size())-visible:0u;
    state_.fileScroll=std::min(first,maximum);state_.fileScrollOffset=static_cast<float>(state_.fileScroll)*row;
    return true;
  }
  return false;
}

i32 EditorSession::sourceTextureForFile(std::string_view relativePath) {
  collectSourceTextures();
  for(usize i=0;i<sourceTextures_.size();++i) {
    const auto &entry=sourceTextures_[i];
    if(entry.image.empty() || entry.sourcePath.empty()) continue;
    // A URI do glTF é relativa à pasta do arquivo principal da fonte.
    const auto slash=entry.sourcePath.find_last_of('/');
    const std::string folder=slash==std::string::npos?std::string():entry.sourcePath.substr(0,slash+1);
    std::string relative,refusal;
    if(resources::gltfRelativeUri(entry.image,relative,refusal) && folder+relative==relativePath) return static_cast<i32>(i);
  }
  return -1;
}

bool EditorSession::openSourceTextureInspector(u32 row) {
  if(row>=sourceTextures_.size()) collectSourceTextures();
  if(row>=sourceTextures_.size()) return false;
  textureInspectorFromManager_=state_.textureManager;
  state_.textureManager=false;state_.textureInspector=true;state_.textureViewer=true;
  state_.textureViewerSource=true;state_.textureViewerSourceIndex=row;
  state_.textureInspectorScroll=0;state_.textureUsersExpanded=false;
  state_.textureViewerLevel=0;state_.textureViewerChannel=0;state_.textureViewerZoom=0;
  texturePanelSelection_=state_.selection;
  // Usuários: objetos cuja malha (por slot) é uma das malhas que usam a textura.
  state_.textureUserLabels.clear();state_.textureUserEntities.clear();
  const auto &meshes=sourceTextures_[row].meshes;
  std::vector<EditorEntityId> ids;document_.collectSubtree(document_.root(),ids);
  for(const auto id:ids) {
    const auto *entity=document_.find(id);
    const auto *render=entity?meshRenderer(*entity):nullptr;
    if(!render) continue;
    for(u32 slot=0;slot<render->slotCount();++slot)
      if(std::find(meshes.begin(),meshes.end(),render->slotAsset(slot))!=meshes.end()) {
        state_.textureUserLabels.push_back(std::string("Objeto: ")+entity->name);state_.textureUserEntities.push_back(id);break;
      }
  }
  state_.status="Textura da fonte · "+std::to_string(state_.textureUserLabels.size())+" objeto(s) usam";
  return refreshSourceTextureViewer();
}

bool EditorSession::refreshSourceTextureViewer() {
  const u32 row=state_.textureViewerSourceIndex;
  if(row>=sourceTextures_.size() || !sourceTextures_[row].texture) return false;
  const auto &entry=sourceTextures_[row];
  const auto &texture=*entry.texture;
  state_.textureViewerTitle=entry.name;
  std::string diagnostic;
  if(sourceViewer_.texture!=entry.texture.get()) {
    sourceViewer_={};
    // A prévia é do que foi preparado (ASTC decodificado), limitada ao visualizador.
    if(!resources::previewTexture(texture,TextureViewerSize*2,sourceViewer_.rgba,sourceViewer_.firstLevel,diagnostic)) {
      state_.textureViewerImage={};state_.textureViewerLevels=0;state_.textureViewerInfo=diagnostic;
      state_.textureProperties={{"Prévia",diagnostic}};
      return false;
    }
    sourceViewer_.texture=entry.texture.get();
  }
  const auto &image=sourceViewer_.rgba;
  state_.textureViewerLevels=image.levels;
  state_.textureViewerLevel=std::min(state_.textureViewerLevel,image.levels-1);
  usize offset=0;u32 width=image.width,height=image.height;
  for(u32 level=0;level<state_.textureViewerLevel;++level) {
    offset+=static_cast<usize>(width)*height*4;
    width=std::max<u32>(1,width/2);height=std::max<u32>(1,height/2);
  }
  const auto channel=static_cast<TexturePreviewChannel>(state_.textureViewerChannel);
  const auto background=static_cast<TexturePreviewBackground>(state_.textureViewerBackground);
  state_.textureViewerImage=preview_.writeViewer(std::span<const u8>(image.mipChain).subspan(offset,static_cast<usize>(width)*height*4),
                                                 width,height,channel,state_.textureViewerZoom,background);
  state_.textureViewerZoomLabel=std::to_string(1u<<state_.textureViewerZoom)+"× no centro";
  state_.textureViewerBackgroundLabel=texturePreviewBackgroundName(background);
  state_.textureViewerChannelLabel=texturePreviewChannelName(channel);
  // O nível mostrado é contado na textura inteira, não na prévia.
  const u32 level=sourceViewer_.firstLevel+state_.textureViewerLevel;
  state_.textureViewerLevelLabel="Nível "+std::to_string(level)+" de "+std::to_string(texture.levels-1)+" · "+
                                 std::to_string(width)+"×"+std::to_string(height);
  state_.textureMipValue=std::to_string(level)+" de "+std::to_string(texture.levels-1);
  const char *format=texture.format==renderer::AuthoringTextureAstc4x4?"ASTC 4×4":texture.format==renderer::AuthoringTextureAstc6x6?"ASTC 6×6":
                     texture.format==renderer::AuthoringTextureAstc8x8?"ASTC 8×8":"RGBA8";
  state_.textureDimensions=std::to_string(texture.width)+" × "+std::to_string(texture.height);
  auto &fields=state_.textureProperties;
  fields={{"Cor",texture.srgb?"sRGB":"Linear (dados)"},{"Formato",format},{"Níveis",std::to_string(texture.levels)},
          {"Memória",texture.partial()?megabytesText(texture.mipChain.size())+" de "+megabytesText(texture.expectedBytes()):
                                       megabytesText(texture.expectedBytes())}};
  if(entry.library<textureStreamingLoaded_.size() && entry.library<textureStreamingDesired_.size()) {
    const u32 loaded=textureStreamingLoaded_[entry.library],desired=textureStreamingDesired_[entry.library];
    const u32 side=std::max(1u,texture.width>>std::min(loaded,31u));
    fields.push_back({"Na GPU",std::to_string(side)+" px (mip "+std::to_string(loaded)+")"});
    state_.textureGpuNote="Na GPU está o mip "+std::to_string(loaded)+" ("+std::to_string(side)+" px); a tela pede o "+
                          std::to_string(desired)+". O streaming troca o nível pela distância e pelo orçamento de Qualidade.";
  } else {
    fields.push_back({"Na GPU","sem relatório"});
    state_.textureGpuNote="Sem relatório do renderer neste aparelho: o nível na GPU aparece quando a cena é desenhada.";
  }
  if(texture.partial()) state_.textureGpuNote+=" Na RAM ficam só os níveis pequenos; os de cima são lidos do derivado em disco.";
  // Origem: a imagem dentro do projeto e a fonte que a trouxe.
  const auto slash=entry.sourcePath.find_last_of('/');
  const std::string folder=slash==std::string::npos?std::string():entry.sourcePath.substr(0,slash+1);
  // O rótulo da imagem é a URI numa fonte em pasta, ou o nome dela dentro do
  // GLB; só é arquivo quando existe no projeto.
  std::string file,relative,refusal;
  if(!entry.image.empty() && resources::gltfRelativeUri(entry.image,relative,refusal) && files_.exists(folder+relative)) file=folder+relative;
  state_.textureOrigin.clear();
  state_.textureOrigin.push_back({"Arquivo",file.empty()?entry.name+" (dentro do modelo)":entry.name,file.empty()?std::string():entry.name});
  if(!file.empty()) state_.textureOrigin.push_back({"Caminho",file,file});
  state_.textureOrigin.push_back({"Fonte",baseName(entry.sourcePath),entry.sourcePath});
  state_.textureBreadcrumb=folderTrail(file.empty()?entry.sourcePath:file);
  state_.textureViewerInfo=std::to_string(state_.textureUserLabels.size())+" objeto(s) usam esta textura";
  state_.textureResidencyLabel.clear();
  return true;
}

bool EditorSession::generatePendingSourceThumbnail() {
  for(const u32 row:layout_.visibleSourceTextureRows) {
    if(row>=sourceTextures_.size() || !sourceTextures_[row].texture) continue;
    const u32 cell=row%TextureThumbnailCapacity;
    if(sourceThumbCells_[cell]==sourceTextures_[row].texture.get()) continue;
    renderer::AuthoringTexture preview;u32 first=0;std::string diagnostic;
    sourceThumbCells_[cell]=sourceTextures_[row].texture.get();
    sourceThumbContent_[cell]={};
    // A miniatura é o nível que cabe na célula: decodificação pequena, uma por quadro.
    if(resources::previewTexture(*sourceTextures_[row].texture,TextureThumbnailSize,preview,first,diagnostic))
      sourceThumbContent_[cell]=preview_.writeThumbnail(cell,preview.mipChain,preview.width,preview.height);
    // A célula agora é desta textura: a miniatura do projeto que estava ali é refeita quando voltar.
    if(cell<thumbnails_.size()) thumbnails_[cell]={};
    return true;
  }
  return false;
}

bool EditorSession::generatePendingTextureThumbnail() {
  const u32 count=std::min<u32>(static_cast<u32>(textures_.size()),TextureThumbnailCapacity);
  if(thumbnails_.size()>count) thumbnails_.resize(count);
  for(u32 index=0;index<count;++index) {
    const auto *record=assets_.find(textures_[index].guid);
    const std::string hash=record?record->contentHash:std::string();
    if(index<thumbnails_.size() && thumbnails_[index].guid==textures_[index].guid && thumbnails_[index].contentHash==hash) continue;
    TextureThumbnail thumbnail{textures_[index].guid,hash,{}};
    resources::DecodedImage image;
    // Arquivo ilegível fica sem miniatura, mas registrado: não é tentado de novo a cada quadro.
    if(readProjectImage(files_.rootPath(),textures_[index].path,importLimits_.image,image)) {
      thumbnail.content=preview_.writeThumbnail(index,image.rgba,image.width,image.height);
      // Mesma decodificação serve ao filtro "sem alfa" do gerenciador.
      thumbnail.opaque=true;
      for(usize at=3;at<image.rgba.size();at+=4) if(image.rgba[at]!=255) {thumbnail.opaque=false;break;}
    }
    if(index<thumbnails_.size()) thumbnails_[index]=std::move(thumbnail);
    else thumbnails_.push_back(std::move(thumbnail));
    // Bloco F: a célula volta a ser do projeto.
    sourceThumbCells_[index]=nullptr;
    return true;
  }
  return false;
}

bool EditorSession::openTextureViewer(u32 projectTextureIndex) {
  if(projectTextureIndex>=textures_.size()) return false;
  state_.textureViewer=true;state_.textureViewerIndex=projectTextureIndex;state_.textureViewerSource=false;
  state_.textureViewerLevel=0;state_.textureViewerChannel=0;state_.textureViewerZoom=0;
  state_.textureInspectorScroll=0;state_.textureUsersExpanded=false;
  state_.textureProfileSaved=state_.textureProfileDraft=textureProfileFor(textures_[projectTextureIndex].guid);
  state_.textureProfileDirty=false;
  state_.textureProfilePage=0;
  // O fundo escolhido vale entre texturas: é preferência de leitura, não da imagem.
  return refreshTextureViewerImage();
}

bool EditorSession::cycleTextureViewerZoom() {
  if(!state_.textureViewer) return false;
  state_.textureViewerZoom=static_cast<u8>((state_.textureViewerZoom+1)%TexturePreviewZoomSteps);
  return refreshTextureViewerImage();
}

bool EditorSession::cycleTextureViewerBackground() {
  if(!state_.textureViewer) return false;
  state_.textureViewerBackground=static_cast<u8>((state_.textureViewerBackground+1)%TexturePreviewBackgroundCount);
  return refreshTextureViewerImage();
}

bool EditorSession::stepTextureViewerLevel(int delta) {
  if(!state_.textureViewer) return false;
  const int next=static_cast<int>(state_.textureViewerLevel)+delta;
  if(next<0 || next>=static_cast<int>(state_.textureViewerLevels)) return false;
  state_.textureViewerLevel=static_cast<u32>(next);
  return refreshTextureViewerImage();
}

bool EditorSession::cycleTextureViewerChannel() {
  if(!state_.textureViewer) return false;
  state_.textureViewerChannel=static_cast<u8>((state_.textureViewerChannel+1)%TexturePreviewChannelCount);
  return refreshTextureViewerImage();
}

bool EditorSession::refreshTextureViewerImage() {
  if(state_.textureViewer && state_.textureViewerSource) return refreshSourceTextureViewer();
  if(!state_.textureViewer || state_.textureViewerIndex>=textures_.size()) return false;
  const auto &texture=textures_[state_.textureViewerIndex];
  const auto *record=assets_.find(texture.guid);
  const std::string hash=record?record->contentHash:std::string();
  const auto savedProfile=textureProfileFor(texture.guid);
  const auto recipe=resources::serializeTextureProfile(savedProfile);
  state_.textureViewerTitle=texture.name;
  if(viewerChain_.guid!=texture.guid || viewerChain_.contentHash!=hash ||
     viewerChain_.recipe!=recipe || !viewerChain_.texture) {
    viewerChain_={};
    auto prepared=decodeProjectTexture(texture.guid,true,EditorMapScene::DefaultTextureSampler,
        savedProfile.interpretation==resources::TextureInterpretationNormal);
    if(!prepared || !prepared->valid()) {
      viewerChain_={};
      state_.textureViewerImage={};state_.textureViewerLevels=0;
      state_.textureViewerLevelLabel.clear();state_.textureViewerChannelLabel.clear();
      state_.textureViewerInfo="Arquivo ilegível: "+texture.path;
      return false;
    }
    viewerChain_.guid=texture.guid;viewerChain_.contentHash=hash;
    viewerChain_.recipe=recipe;viewerChain_.texture=std::move(prepared);
  }
  const auto &image=*viewerChain_.texture;
  state_.textureViewerLevels=image.levels;
  state_.textureViewerLevel=std::min(state_.textureViewerLevel,image.levels-1);
  usize offset=0;u32 width=image.width,height=image.height;
  for(u32 level=0;level<state_.textureViewerLevel;++level) {
    offset+=static_cast<usize>(width)*height*4;
    width=std::max<u32>(1,width/2);height=std::max<u32>(1,height/2);
  }
  const auto channel=static_cast<TexturePreviewChannel>(state_.textureViewerChannel);
  const auto background=static_cast<TexturePreviewBackground>(state_.textureViewerBackground);
  state_.textureViewerImage=preview_.writeViewer(std::span<const u8>(image.mipChain).subspan(offset,static_cast<usize>(width)*height*4),
                                                 width,height,channel,state_.textureViewerZoom,background);
  state_.textureViewerZoomLabel=std::to_string(1u<<state_.textureViewerZoom)+"× no centro";
  // R4: perfil da textura (todos os usos) e o que está na GPU.
  {
    for(u32 field=0;field<TextureProfileFields;++field) state_.textureProfileLabels[field]=textureProfileFieldText(state_.textureProfileDraft,field);
    // O que o renderer do aparelho tem desta textura agora (S2); no host, sem GPU, diz isso.
    const auto streaming=textureStreamingLevelsOf(texture.guid);
    if(!streaming.known) {
      state_.textureProfileLabels[10]="Carregado: sem renderer";
      state_.textureProfileLabels[11]="Tela pede: —";
    } else {
      state_.textureProfileLabels[10]="Carregado: mip "+std::to_string(streaming.loaded)+" · "+std::to_string(streaming.loadedWidth)+" px";
      state_.textureProfileLabels[11]="Tela pede: mip "+std::to_string(streaming.desired);
    }
    const auto residency=textureResidencyOf(texture.guid);
    if(residency.empty()) state_.textureResidencyLabel="Na GPU: não usada";
    else {
      char text[96];
      std::snprintf(text,sizeof(text),"Na GPU: %u×%u · %u níveis · %.1f MB%s",residency.front().width,residency.front().height,
                    residency.front().levels,static_cast<double>(residency.front().bytes)/1048576.0,
                    residency.size()>1?" · +usos":"");
      state_.textureResidencyLabel=text;
    }
  }
  state_.textureViewerBackgroundLabel=texturePreviewBackgroundName(background);
  state_.textureViewerLevelLabel="Nível "+std::to_string(state_.textureViewerLevel)+" de "+std::to_string(image.levels-1)+
                                 " · "+std::to_string(width)+"×"+std::to_string(height);
  // Inspector em cartões: o que o perfil preparou e o que a GPU tem.
  state_.textureMipValue=std::to_string(state_.textureViewerLevel)+" de "+std::to_string(image.levels-1);
  state_.textureDimensions=std::to_string(image.width)+" × "+std::to_string(image.height);
  {
    const auto residency=textureResidencyOf(texture.guid);
    const auto streaming=textureStreamingLevelsOf(texture.guid);
    state_.textureProperties={{"Cor",image.srgb?"sRGB":"Linear (dados)"},{"Níveis",std::to_string(image.levels)},
                              {"Memória",megabytesText(image.mipChain.size())},
                              {"Na GPU",residency.empty()?std::string("não usada"):
                                        std::to_string(residency.front().width)+" px · "+megabytesText(residency.front().bytes)}};
    state_.textureGpuNote=streaming.known?"Carregado o mip "+std::to_string(streaming.loaded)+" ("+std::to_string(streaming.loadedWidth)+
                                          " px); a tela pede o "+std::to_string(streaming.desired)+"."
                                        :std::string("Sem relatório do renderer: o nível na GPU aparece quando a cena é desenhada.");
    if(residency.size()>1) state_.textureGpuNote+=" Há mais de uma cópia na GPU (usos com amostragem diferente).";
    state_.textureOrigin={{"Arquivo",texture.name,texture.name},{"Caminho",texture.path,texture.path}};
    state_.textureBreadcrumb=folderTrail(texture.path);
  }
  state_.textureViewerChannelLabel=texturePreviewChannelName(channel);
  const auto users=textureUsersOf(texture.guid);
  const double megabytes=static_cast<double>(image.mipChain.size())/1048576.0;
  char size[32];std::snprintf(size,sizeof(size),"%.1f",megabytes);
  state_.textureViewerInfo="Perfil aplicado · "+std::to_string(image.width)+"×"+std::to_string(image.height)+" · "+
      std::to_string(image.levels)+" níveis · "+size+" MB · "+std::to_string(users)+(users==1?" uso":" usos")+
      (savedProfile.interpretation==resources::TextureInterpretationUse?" · prévia como cor":std::string())+" · "+texture.path;
  return true;
}

u32 EditorSession::textureUsersOf(const resources::AssetGuid &guid) const {
  if(!guid.valid() || guid==scene::MaterialTextureNone) return 0;
  u32 users=sceneUsersOf(guid);
  for(const auto &material:materials_)
    if(std::find(material.textures.begin(),material.textures.end(),guid)!=material.textures.end() || material.occlusionTexture==guid) ++users;
  return users;
}

void EditorSession::anticipateSceneTextures(const char *path,u64 fingerprint) {
  anticipatedTextures_.clear();
  EditorDocument scene;
  if(!path || !loadEditorDocument(path,fingerprint,scene)) return;
  std::vector<EditorEntityId> ids;scene.collectSubtree(scene.root(),ids);
  for(const auto id:ids) {
    const auto *entity=scene.find(id);
    const auto *render=entity?meshRenderer(*entity):nullptr;
    if(!render) continue;
    for(u32 slot=0;slot<render->slotCount();++slot) {
      const auto guid=render->slotLightmap(slot).texture;
      if(guid.valid()) {
        const UsedTexture item{guid,false,false,EditorMapScene::samplerFlags(scene::lightmapSampling())};
        if(std::find(anticipatedTextures_.begin(),anticipatedTextures_.end(),item)==anticipatedTextures_.end())
          anticipatedTextures_.push_back(item);
      }
    }
    for(u32 slot=0;slot<render->slotCount();++slot)
      for(u32 binding=0;binding<=scene::MaterialOcclusionTextureBinding;++binding) {
        // Mesma resolução da extração: textura e amostragem da instância, senão do
        // material. O binding extra é a oclusão própria (linear, amostragem do binding 2).
        const bool occlusion=binding==scene::MaterialOcclusionTextureBinding;
        const auto guid=occlusion?mapScene_.slotOcclusionTexture(*render,slot):mapScene_.slotTexture(*render,slot,binding);
        if(!guid.valid() || guid==scene::MaterialTextureNone) continue;
        const UsedTexture item{guid,!occlusion && EditorMapScene::bindingIsSrgb(binding),!occlusion&&binding==1,
                               EditorMapScene::samplerFlags(mapScene_.slotSampling(*render,slot,occlusion?2u:binding))};
        if(std::find(anticipatedTextures_.begin(),anticipatedTextures_.end(),item)==anticipatedTextures_.end())
          anticipatedTextures_.push_back(item);
      }
  }
}

u32 EditorSession::sceneUsersOf(const resources::AssetGuid &guid) const {
  u32 users=0;
  std::vector<EditorEntityId> subtree;
  document_.collectSubtree(document_.root(),subtree);
  for(const auto id:subtree) {
    const auto *entity=document_.find(id);
    if(!entity) continue;
    bool used=false;
    const auto *render=meshRenderer(*entity);
    // Um slot usa o recurso pela malha ou pelo material do projeto.
    if(render) for(u32 slot=0;slot<render->slotCount();++slot)
      if(render->slotAsset(slot)==guid || render->slotMaterialAsset(slot)==guid ||
         std::find(render->slotTextures(slot).begin(),render->slotTextures(slot).end(),guid)!=render->slotTextures(slot).end() ||
         render->slotOcclusionTexture(slot)==guid) {used=true;break;}
    // Bindings declarados pelo schema cobrem recursos fora do MeshRenderer,
    // incluindo o mapa HDRI autorado diretamente no volume de ambiente.
    if(!used) for(usize componentIndex=0;componentIndex<entity->components.size()&&!used;++componentIndex) {
      const auto *component=entity->components.at(componentIndex);if(!component) continue;
      for(const auto &binding:component->type().resourceBindings) {
        if(!binding.read) continue;
        for(u32 slot=0;slot<binding.slotCount(*component);++slot)
          if(binding.read(*component,slot)==guid) {used=true;break;}
        if(used) break;
      }
    }
    if(used) ++users;
  }
  return users;
}

bool EditorSession::moveResource(const std::string &relative,const std::string &destination,
                                 ResourceChangeReport &report) {
  report={};
  if(relative.empty() || destination.empty() || relative==destination) {
    report.diagnostic="Origem e destino precisam ser caminhos diferentes.";return false;
  }
  // O registro primeiro, porque ele e o unico que sabe dizer "nao" sem deixar
  // rastro. Se o arquivo fosse antes, uma colisao de caminho descobriria o
  // problema com o arquivo ja no lugar novo.
  const int retargeted=assets_.retargetPrefix(relative,destination);
  if(retargeted<0) {
    report.diagnostic="Ja existe um recurso registrado nesse caminho.";return false;
  }
  if(!files_.movePath(relative,destination)) {
    // Desfaz o reapontamento: o disco nao mudou, entao o registro tambem nao
    // pode ter mudado.
    assets_.retargetPrefix(destination,relative);
    report.diagnostic=files_.error();
    return false;
  }
  report.retargeted=static_cast<u32>(retargeted);
  assetRegistryDirty_=true;
  // A cena NAO e tocada, de proposito: os objetos guardam o GUID, nao o
  // caminho. Renomear um arquivo nao pode quebrar um objeto.
  state_.status=report.retargeted>0
      ? std::to_string(report.retargeted)+" recurso(s) reapontado(s)"
      : "Movido";
  return true;
}

bool EditorSession::deleteResource(const std::string &relative,bool force,
                                   ResourceChangeReport &report) {
  report={};
  if(relative.empty()) { report.diagnostic="Caminho vazio."; return false; }
  u32 texturesDoomed=0;
  u32 meshesDoomed=0;
  bool geometryLibraryChanged=false;
  // Tudo que vive sob este caminho: apagar uma pasta apaga os recursos dela.
  std::vector<resources::AssetGuid> doomed;
  for(const auto &record:assets_.records()) {
    const auto &path=record.path;
    if(path.size()<relative.size() || path.compare(0,relative.size(),relative)!=0) continue;
    if(path.size()!=relative.size() && path[relative.size()]!='/') continue;
    doomed.push_back(record.guid);
    if(record.type==resources::AssetType::Texture) ++texturesDoomed;
    if(record.type==resources::AssetType::Mesh) ++meshesDoomed;
    geometryLibraryChanged|=record.type==resources::AssetType::Mesh||
        record.type==resources::AssetType::Material||record.type==resources::AssetType::Texture;
  }
  for(const auto &guid:doomed) {
    report.sceneUsers+=sceneUsersOf(guid);
    // A cena nao aponta para o ARQUIVO, e sim para cada malha que saiu dele.
    // Um GLB de cinco nos vira cinco identidades, e sao elas que os objetos
    // guardam. Contar so o recurso da fonte diria "ninguem usa" com a cena
    // inteira montada em cima dele.
    for(const auto &source:importedSources_)
      if(source.guid==guid)
        for(const auto &identity:source.identities) report.sceneUsers+=sceneUsersOf(identity);
    // R4: materiais do projeto que usam esta textura dependem dela.
    for(const auto &material:materials_) {
      if(std::find(material.textures.begin(),material.textures.end(),guid)==material.textures.end() && material.occlusionTexture!=guid) continue;
      if(std::find(doomed.begin(),doomed.end(),material.guid)==doomed.end()) ++report.registryDependents;
    }
    for(const auto &dependent:assets_.dependents(guid)) {
      bool alsoDoomed=false;
      for(const auto &other:doomed) if(other==dependent) {alsoDoomed=true;break;}
      if(!alsoDoomed) ++report.registryDependents;
    }
  }
  if(!force && (report.sceneUsers>0 || report.registryDependents>0)) {
    report.diagnostic="Em uso: "+std::to_string(report.sceneUsers)+" objeto(s) da cena e "+
        std::to_string(report.registryDependents)+" recurso(s) dependem deste arquivo.";
    return false;
  }
  if(geometryLibraryChanged && !publishGeometry_) {
    report.diagnostic="Este ambiente não publica geometria importada.";
    return false;
  }
  if(!files_.removePath(relative)) { report.diagnostic=files_.error(); return false; }

  bool environmentLibraryChanged=false;
  // O registro e a biblioteca so mudam DEPOIS que o arquivo saiu: ate aqui o
  // projeto ainda podia ser recuperado do disco.
  for(const auto &guid:doomed) {
    for(auto source=importedSources_.begin();source!=importedSources_.end();++source)
      if(source->guid==guid) { importedSources_.erase(source); break; }
    removeImportMapFile(guid);
    // Material apagado: os slots que o usavam voltam ao da fonte, visivelmente
    // marcados como referência sem arquivo no inspetor.
    std::erase_if(materials_,[&](const auto &material){return material.guid==guid;});
    std::erase_if(decodedTextures_,[&](const auto &entry){return entry.guid==guid;});
    environmentLibraryChanged|=std::erase_if(environmentMaps_,[&](const auto &entry){return entry.first==guid;})!=0;
    std::erase_if(environmentProfiles_,[&](const auto &profile){return profile.guid==guid;});
    assets_.remove(guid);
    ++report.retargeted;
  }
  assetRegistryDirty_=true;
  // A biblioteca HDRI não pertence à revisão do documento. Use o sinal de
  // publicação compartilhada para o shell substituir a lista uma única vez;
  // isso também faz o renderer abandonar um shared_ptr removido da GPU.
  if(environmentLibraryChanged) appearanceChanged_=true;
  if(!doomed.empty()) {loadTextureAssets();publishMaterialLibrary();}
  std::string diagnostic;
  if(geometryLibraryChanged && !republishGeometry(diagnostic)) {
    report.diagnostic=diagnostic;
    return false;
  }
  // Objeto que apontava para o recurso apagado fica SEM malha, visivelmente. A
  // reconciliacao ja faz isso: identidade ausente vira slot zero, em vez de
  // apontar para a malha que por acaso ocupar o indice antigo.
  mapScene_.reconcileAssets(document_);
  // Textura apagada não tira malha de ninguém: o binding volta à textura da fonte.
  state_.status=report.sceneUsers==0 ? std::string("Apagado")
      : texturesDoomed==doomed.size() ? std::to_string(report.sceneUsers)+" objeto(s) voltaram à textura da fonte"
      : meshesDoomed ? std::to_string(report.sceneUsers)+" objeto(s) ficaram sem malha"
      : std::to_string(report.sceneUsers)+" objeto(s) preservam referência ao recurso removido";
  return true;
}

bool EditorSession::republishGeometry(std::string &diagnostic) {
  // Mesmo sem fontes GLB, materiais das primitivas podem usar texturas do
  // projeto. O renderer novo ainda não tem essas imagens nem seus bindings.
  return publishAndAdopt(flattenSources(importedSources_),diagnostic);
}

bool EditorSession::importModel(std::span<const u8> bytes, std::string_view sourceName,
                                const resources::GltfImportProgress &progress, ModelImportReport &report) {
  report = {};
  if(isPlaying()) { report.diagnostic="Pare a execução antes de importar."; return false; }
  if(!publishGeometry_) { report.diagnostic="Este ambiente não publica geometria importada."; return false; }
  if(sourceName.empty()) { report.diagnostic="Nome de arquivo vazio."; return false; }

  resources::GltfImport model;
  if(!resources::importGlb(bytes,importLimits_,progress,model)) {
    report.diagnostic=model.diagnostic;report.cancelled=model.cancelled;return false;
  }
  // Mesmo contrato da reabertura: a exclusão que vale é a salva com a fonte.
  // Publicar por esta porta sem ela traria de volta, em silêncio, o nó que o
  // autor tirou da cena.
  const auto *known=assets_.findByPath(std::string(sourceName));
  const auto profile=importProfileFor(known?known->guid:resources::assetGuidFromSeed("fonte:"+std::string(sourceName)));
  if(!publishModel(model,Sha256::hex(bytes),sourceName,report,resources::ImportAmbiguityPolicy::Refuse,
                   profile.excludedNodes)) return false;
  // Reabertura: o mapa acompanha a fonte no projeto. Mesmo conteúdo produz o
  // mesmo mapa, e nada é regravado; um mapa novo (fonte trocada fora do editor,
  // ou projeto anterior ao mapa) é gravado para a próxima revisão partir dele.
  persistImportMap(report.source);
  // Compatibility entry point for existing loaders; already registered sources
  // only rehydrate geometry. Interactive import uses resource-only publication.
  if(!report.reimported) {
    ModelImportReport instance;
    // Preserve scene shape for the legacy API; interactive resource instances
    // use a movable wrapper when the source scene contains independent roots.
    if(!instantiateModel(report.source,instance,false)) {report.diagnostic=instance.diagnostic;return false;}
    report.objects=instance.objects;report.groups=instance.groups;
    report.lodGroups=instance.lodGroups;report.lodNotes=std::move(instance.lodNotes);
    report.skinnedMeshes=instance.skinnedMeshes;report.animations=instance.animations;
  }
  return true;
}

bool EditorSession::publishModel(const resources::GltfImport &model, std::string_view hash,
                                std::string_view sourceName, ModelImportReport &report,
                                resources::ImportAmbiguityPolicy policy,
                                std::span<const resources::AssetGuid> excludedNodes) {
  report={};
  if(isPlaying() || history_.isOpen()) {report.diagnostic="Finalize a edição antes de publicar o recurso.";return false;}
  // Candidato completo antes de publicar: a versão anterior continua valendo
  // até a nova estar inteira na GPU e aceita pelo editor.
  auto candidateSources=importedSources_;
  auto nextAssets=assets_;
  StagedSource staged;
  if(!stageSource(model,hash,sourceName,policy,candidateSources,nextAssets,report,staged,excludedNodes)) return false;
  const auto previousDocument=document_;
  const auto previousMap=mapScene_;
  if(!publishAndAdopt(flattenSources(candidateSources),report.diagnostic)) {
    std::string rollback;
    if(!publishAndAdopt(flattenSources(importedSources_),rollback))
      report.diagnostic+=" Falha ao restaurar a GPU: "+rollback;
    document_=previousDocument;mapScene_=previousMap;return false;
  }
  importedSources_=std::move(candidateSources);
  assets_=std::move(nextAssets);assetRegistryDirty_=true;
  state_.status=report.reimported?"Recurso reimportado; instâncias preservadas":"Recurso registrado; pronto para instanciar";
  reconcileStagedSource(staged,report);
  return true;
}

bool EditorSession::reopenSources(std::vector<ReopenedSource> &sources, std::vector<ModelImportReport> &reports,
                                  std::string &diagnostic) {
  reports.assign(sources.size(),ModelImportReport{});
  diagnostic.clear();
  if(isPlaying() || history_.isOpen()) {diagnostic="Finalize a edição antes de reabrir as fontes.";return false;}
  if(!publishGeometry_) {diagnostic="Este ambiente não publica geometria importada.";return false;}
  auto candidateSources=importedSources_;
  auto nextAssets=assets_;
  std::vector<StagedSource> staged(sources.size());
  std::vector<bool> accepted(sources.size(),false);
  bool any=false;
  for(usize i=0;i<sources.size();++i) {
    if(sources[i].sourceName.empty()) {reports[i].diagnostic="Nome de arquivo vazio.";continue;}
    // Reabrir usa a exclusão com que a fonte foi PUBLICADA — o perfil salvo com
    // ela. Sem isso, reabrir o projeto traria de volta à cena o nó que o autor
    // tirou na importação.
    const auto *known=nextAssets.findByPath(sources[i].sourceName);
    const auto source=known?known->guid:resources::assetGuidFromSeed("fonte:"+sources[i].sourceName);
    const auto profile=importProfileFor(source);
    accepted[i]=stageSource(sources[i].model,sources[i].hash,sources[i].sourceName,resources::ImportAmbiguityPolicy::Refuse,
                            candidateSources,nextAssets,reports[i],staged[i],profile.excludedNodes);
    any=any||accepted[i];
  }
  if(!any) return true;
  const auto previousDocument=document_;
  const auto previousMap=mapScene_;
  if(!publishAndAdopt(flattenSources(candidateSources),diagnostic)) {
    std::string rollback;
    if(!publishAndAdopt(flattenSources(importedSources_),rollback)) diagnostic+=" Falha ao restaurar a GPU: "+rollback;
    document_=previousDocument;mapScene_=previousMap;
    for(usize i=0;i<sources.size();++i) if(accepted[i]) reports[i].diagnostic=diagnostic;
    return false;
  }
  importedSources_=std::move(candidateSources);
  assets_=std::move(nextAssets);assetRegistryDirty_=true;
  for(usize i=0;i<sources.size();++i) {
    if(!accepted[i]) continue;
    reconcileStagedSource(staged[i],reports[i]);
    persistImportMap(reports[i].source);
    // Mesmo contrato de `importModel`: fonte que ainda não estava no registro
    // instancia seus nós; fonte registrada só reidrata a geometria.
    if(!reports[i].reimported) {
      ModelImportReport instance;
      if(!instantiateModel(reports[i].source,instance,false)) {reports[i].diagnostic=instance.diagnostic;continue;}
      reports[i].objects=instance.objects;reports[i].groups=instance.groups;
    }
  }
  state_.status="Recursos do projeto reabertos";
  return true;
}

// Valida a fonte, monta o mapa de nós e põe o bloco no candidato e o registro em
// `nextAssets`. Nada é publicado aqui.
bool EditorSession::stageSource(const resources::GltfImport &model, std::string_view hash, std::string_view sourceName,
                                resources::ImportAmbiguityPolicy policy, std::vector<ImportedSource> &candidateSources,
                                resources::AssetRegistry &nextAssets, ModelImportReport &report, StagedSource &staged,
                                std::span<const resources::AssetGuid> excludedNodes) {
  report={};
  staged={};
  if(model.draws.empty() || model.nodes.empty()) {report.diagnostic="Modelo sem geometria utilizável.";return false;}
  const float identity[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
  for(usize n=0;n<model.nodes.size();++n) {
    EditorTransform local;
    if(model.nodes[n].parent>=static_cast<i32>(n) || model.nodes[n].parent < -1 ||
       !runtime::localTransformForWorld(model.nodes[n].localMatrix,identity,local)) {
      report.diagnostic="Hierarquia ou matriz local incompatível com TRS; fonte anterior preservada.";return false;
    }
  }
  if(model.drawNodes.size()!=model.draws.size()) {report.diagnostic="Mapa de desenhos incompleto.";return false;}
  for(const auto node:model.drawNodes) if(node>=model.nodes.size()) {report.diagnostic="Desenho sem nó.";return false;}
  report.skippedTextures=model.skippedTextures;
  report.skippedAnimations=model.skippedAnimations;
  report.skippedSkins=model.skippedSkins;

  // Identidade da FONTE: derivada do caminho dentro do projeto, não do conteúdo.
  // Reimportar o arquivo editado precisa cair no mesmo recurso; é o hash que
  // muda, não a identidade.
  const std::string path(sourceName);
  // `nextAssets`, não `assets_`: numa reabertura em lote, uma fonte já posta no
  // candidato conta como existente para a seguinte.
  const auto *existing=nextAssets.findByPath(path);
  const resources::AssetGuid source=existing?existing->guid:resources::assetGuidFromSeed("fonte:"+path);
  report.source=source;
  staged.source=source;

  // Mapa de nós (M08.2): a revisão anterior, em memória ou no projeto, é o ponto
  // de partida da correspondência. Sem ela as identidades nascem da fonte e das
  // chaves legadas — as mesmas que cenas anteriores ao mapa já gravaram.
  auto &previousNodeMap=staged.previousNodeMap;
  staged.hasPrevious=existing && previousImportMap(source,previousNodeMap);
  const bool hasPrevious=staged.hasPrevious;
  resources::ImportNodeMap nodeMap;
  std::string mapDiagnostic;
  if(!resources::buildImportNodeMap(model,source,hash,hasPrevious?&previousNodeMap:nullptr,policy,nodeMap,report.match,mapDiagnostic,
                                    excludedNodes)) {
    report.diagnostic=mapDiagnostic.empty()?"Mapa de nós recusado; fonte anterior preservada.":mapDiagnostic;
    return false;
  }

  ImportedSource block;
  block.guid=source;
  block.vertices.assign(model.vertices.begin(),model.vertices.end());
  block.indices.assign(model.indices.begin(),model.indices.end());
  block.materials.assign(model.materials.begin(),model.materials.end());
  block.draws.assign(model.draws.begin(),model.draws.end());
  block.nodes=model.nodes;
  block.drawNodes=model.drawNodes;
  std::vector<usize> primitiveOfNode(model.nodes.size(),0);
  for(usize i=0;i<model.draws.size();++i) {
    // A identidade de cada malha vem do mapa: um nó reconhecido na revisão nova
    // mantém a identidade das suas primitivas, mesmo renomeado ou movido. O mapa
    // já recusa identidades repetidas.
    const auto node=model.drawNodes[i];
    const auto primitive=primitiveOfNode[node]++;
    if(primitive>=nodeMap.nodes[node].draws.size()) {report.diagnostic="Mapa de nós incompleto; fonte anterior preservada.";return false;}
    block.identities.push_back(nodeMap.nodes[node].draws[primitive]);
    block.names.push_back(i<model.names.size()?model.names[i]:std::string("Malha"));
  }
  block.map=std::move(nodeMap);
  block.materialNames=model.materialNames;
  block.textures=model.textures;
  block.textureImages=model.textureImages;
  block.skins=model.skins;
  block.drawSkins=model.drawSkins;
  block.skinInfluences=model.skinInfluences;
  block.animations=model.animations;
  block.morphs=model.morphs;
  block.drawMorphs=model.drawMorphs;
  block.meshLods=model.meshLods;
  block.sourceIndexCount=block.indices.size();
  block.sourceDrawCount=block.draws.size();
  auto profile=importProfileFor(source);
  profile.excludedNodes.assign(excludedNodes.begin(),excludedNodes.end());
  if(!applyCollisionMeshRecipes(block,profile,report.diagnostic)) return false;

  usize slot=candidateSources.size();
  for(usize i=0;i<candidateSources.size();++i) if(candidateSources[i].guid==source) {slot=i;break;}
  // Reimportação é decidida pelo REGISTRO, não pela biblioteca deste processo.
  // Ao reabrir o projeto, a fonte está no registro e a biblioteca está vazia --
  // se a decisão fosse pela biblioteca, cada abertura criaria os objetos de
  // novo e a cena cresceria sozinha a cada vez.
  report.reimported=existing!=nullptr;
  staged.reimported=report.reimported;
  // Onde o bloco entra é decidido pela BIBLIOTECA (substituir o que já está
  // carregado); se objetos são criados, pelo REGISTRO. São perguntas diferentes:
  // ao reabrir o projeto a fonte está no registro e não na biblioteca.
  if(slot<candidateSources.size()) candidateSources[slot]=std::move(block);
  else candidateSources.push_back(std::move(block));

  resources::AssetRecord record=existing?*existing:resources::AssetRecord{};
  record.guid=source;record.type=resources::AssetType::Mesh;record.path=path;record.source=path;
  record.contentHash=std::string(hash);record.importerVersion=1;record.importerParameters="glb";
  record.derived.clear();
  const bool registered=existing
      ?nextAssets.publishImport(source,record.contentHash,1,"glb",{},record.dependencies)
      :nextAssets.add(record);
  if(!registered) {report.diagnostic="Registro recusou o recurso; publicação cancelada.";return false;}
  return true;
}

bool EditorSession::applyCollisionMeshRecipes(ImportedSource &source,const resources::ImportProfile &profile,
                                               std::string &diagnostic) const {
  if(source.sourceDrawCount>source.draws.size() || source.sourceDrawCount>source.identities.size() ||
     source.sourceDrawCount>source.names.size() || source.sourceIndexCount>source.indices.size()) {
    diagnostic="Biblioteca fonte inconsistente; derivados não foram gerados.";return false;
  }
  source.draws.resize(source.sourceDrawCount);source.identities.resize(source.sourceDrawCount);
  source.names.resize(source.sourceDrawCount);source.indices.resize(source.sourceIndexCount);
  for(const auto &recipe:profile.collisionMeshes) {
    const auto found=std::find(source.identities.begin(),source.identities.end(),recipe.source);
    if(found==source.identities.end()) continue; // referência ausente fica preservada no perfil
    const usize index=static_cast<usize>(found-source.identities.begin());
    if(index>=source.sourceDrawCount) continue;
    resources::CollisionMeshBuild build;
    if(!resources::buildCollisionMesh(source.vertices,source.indices,source.draws[index],recipe,build,diagnostic)) return false;
    auto draw=source.draws[index];draw.firstIndex=static_cast<u32>(source.indices.size());
    draw.indexCount=static_cast<u32>(build.indices.size());draw.lodLevel=0;draw.geometricError=build.resultingError;
    draw.lodGroupId=static_cast<u32>(source.draws.size());
    source.indices.insert(source.indices.end(),build.indices.begin(),build.indices.end());
    source.draws.push_back(draw);source.identities.push_back(resources::collisionMeshGuid(recipe));
    source.names.push_back(source.names[index]+" · Colisão "+std::to_string(recipe.trianglePercent)+"%");
  }
  return true;
}

bool EditorSession::generateCollisionMesh(EditorEntityId id,u64 componentInstance,u8 trianglePercent,
                                           float maximumError,std::string &diagnostic) {
  diagnostic.clear();
  const auto *entity=document_.find(id);const auto *render=entity?meshRenderer(*entity):nullptr;
  const auto *component=entity?entity->components.findInstance(componentInstance):nullptr;
  const scene::ComponentResourceBinding *binding=nullptr;
  if(component) for(const auto &candidate:component->type().resourceBindings)
    if(candidate.id=="collision_mesh") {binding=&candidate;break;}
  if(!entity||!render||!component||!binding||!binding->write||!binding->presentation.isEditable(*component)) {
    diagnostic="A malha visual ou o vínculo da malha física não está disponível.";return false;
  }
  auto visual=render->slotAsset(0);
  if(!visual.valid()&&render->slotMesh(0)) visual=mapScene_.assetGuid(render->slotMesh(0)-1);
  resources::CollisionMeshRecipe recipe{visual,trianglePercent,maximumError};
  if(!resources::validCollisionMeshRecipe(recipe)) {diagnostic="Escolha uma malha visual importada e parâmetros válidos.";return false;}
  usize sourceSlot=importedSources_.size();
  for(usize i=0;i<importedSources_.size();++i)
    if(std::find(importedSources_[i].identities.begin(),
                 importedSources_[i].identities.begin()+static_cast<std::ptrdiff_t>(importedSources_[i].sourceDrawCount),visual)!=
       importedSources_[i].identities.begin()+static_cast<std::ptrdiff_t>(importedSources_[i].sourceDrawCount)) {sourceSlot=i;break;}
  if(sourceSlot==importedSources_.size()) {diagnostic="A malha visual não pertence a uma fonte importada carregada.";return false;}
  const auto sourceGuid=importedSources_[sourceSlot].guid;
  auto oldProfile=importProfileFor(sourceGuid),profile=oldProfile;
  auto existing=std::find_if(profile.collisionMeshes.begin(),profile.collisionMeshes.end(),
                             [&](const auto &entry){return entry.source==visual;});
  // Parâmetros regeneram o conteúdo, mas a identidade autoral do recurso não
  // muda. Perfis schema 6 chegam aqui com seu GUID legado já materializado.
  if(existing==profile.collisionMeshes.end()) {
    recipe.identity=resources::collisionMeshGuid(recipe);
    profile.collisionMeshes.push_back(recipe);
  } else {
    recipe.identity=resources::collisionMeshGuid(*existing);
    *existing=recipe;
  }
  const auto derived=resources::collisionMeshGuid(recipe);
  auto values=*entity;auto *editable=values.components.editInstance(componentInstance);
  if(!editable||!binding->write(*editable,0,derived)||!editable->valid()) {
    diagnostic="O componente recusou a malha física derivada.";return false;
  }
  const auto previous=*entity;
  if(!applyCollisionMeshEdit(sourceGuid,profile,id,values,diagnostic)) return false;
  if(!history_.recordResource("Gerar malha física",
      [this,sourceGuid,oldProfile,profile,id,previous,values](bool forward) {
        std::string ignored;
        return applyCollisionMeshEdit(sourceGuid,forward?profile:oldProfile,id,forward?values:previous,ignored);
      })) {
    std::string rollback;applyCollisionMeshEdit(sourceGuid,oldProfile,id,previous,rollback);
    diagnostic="O histórico estava ocupado; a geração foi revertida.";return false;
  }
  const auto slot=mapScene_.assetSlot(derived);
  const auto *draw=slot?mapScene_.asset(slot-1):nullptr;
  diagnostic="Malha física gerada: "+std::to_string(draw?draw->indexCount/3:0)+" triângulos (alvo "+
             std::to_string(trianglePercent)+"%) · disponível em Desfazer";
  return true;
}

bool EditorSession::applyCollisionMeshEdit(const resources::AssetGuid &sourceGuid,
    const resources::ImportProfile &profile,EditorEntityId id,const EditorEntity &values,std::string &diagnostic) {
  usize sourceSlot=importedSources_.size();
  for(usize i=0;i<importedSources_.size();++i) if(importedSources_[i].guid==sourceGuid) {sourceSlot=i;break;}
  if(sourceSlot==importedSources_.size() || !document_.find(id) || !resources::validImportProfile(profile)) {
    diagnostic="A fonte ou o objeto da malha física não está mais disponível.";return false;
  }
  auto candidateSources=importedSources_;
  if(!applyCollisionMeshRecipes(candidateSources[sourceSlot],profile,diagnostic)) return false;
  const auto previousDocument=document_;
  const auto previousMap=mapScene_;
  const auto previousProfile=importProfileFor(sourceGuid);
  if(!publishAndAdopt(flattenSources(candidateSources),diagnostic)) {
    std::string rollback;publishAndAdopt(flattenSources(importedSources_),rollback);
    document_=previousDocument;mapScene_=previousMap;return false;
  }
  if(!saveImportProfile(sourceGuid,profile) || !document_.applyEntityValues(id,values)) {
    saveImportProfile(sourceGuid,previousProfile);
    std::string rollback;publishAndAdopt(flattenSources(importedSources_),rollback);
    document_=previousDocument;mapScene_=previousMap;
    diagnostic="Não foi possível persistir a receita e a alteração foi revertida.";return false;
  }
  importedSources_=std::move(candidateSources);
  return true;
}

void EditorSession::refreshCollisionMeshDraft() {
  state_.collisionTrianglePercent=25;state_.collisionMaximumError=.02f;
  const auto *entity=document_.find(state_.selection);const auto *render=entity?meshRenderer(*entity):nullptr;
  auto visual=render?render->slotAsset(0):resources::AssetGuid{};
  if(!visual.valid()&&render&&render->slotMesh(0)) visual=mapScene_.assetGuid(render->slotMesh(0)-1);
  for(const auto &source:importedSources_) {
    const auto end=source.identities.begin()+static_cast<std::ptrdiff_t>(source.sourceDrawCount);
    if(std::find(source.identities.begin(),end,visual)==end) continue;
    const auto profile=importProfileFor(source.guid);
    const auto recipe=std::find_if(profile.collisionMeshes.begin(),profile.collisionMeshes.end(),
                                   [&](const auto &candidate){return candidate.source==visual;});
    if(recipe!=profile.collisionMeshes.end()) {
      state_.collisionTrianglePercent=recipe->trianglePercent;
      state_.collisionMaximumError=recipe->maximumError;
    }
    break;
  }
}

// Depois da biblioteca adotada: a reconciliação resolve slots pelo pacote novo.
bool EditorSession::bindImportedDeformation(const ImportedSource &tree,const std::vector<EditorEntityId> &objectOfNode,
                                            std::span<const EditorEntityId> roots,u32 &meshes,u32 &animations) {
  const auto objectOf=[&](u32 node)->EditorEntityId {
    const auto id=node<objectOfNode.size()?objectOfNode[node]:kInvalidEntity;
    return id && document_.find(id)?id:kInvalidEntity;
  };
  std::vector<u8> done(tree.nodes.size(),0);
  for(usize i=0;i<tree.sourceDrawCount && i<tree.drawNodes.size();++i) {
    const u32 node=tree.drawNodes[i];
    if(node>=done.size() || done[node]) continue;
    done[node]=1;
    const i32 skinIndex=i<tree.drawSkins.size()?tree.drawSkins[i]:-1;
    const i32 morphIndex=i<tree.drawMorphs.size()?tree.drawMorphs[i]:-1;
    const auto *skin=skinIndex>=0 && static_cast<usize>(skinIndex)<tree.skins.size() && !tree.skinInfluences.empty() &&
                     !tree.skins[static_cast<usize>(skinIndex)].joints.empty()?&tree.skins[static_cast<usize>(skinIndex)]:nullptr;
    const auto *morph=morphIndex>=0 && static_cast<usize>(morphIndex)<tree.morphs.size()?&tree.morphs[static_cast<usize>(morphIndex)]:nullptr;
    const auto target=objectOf(node);
    if((!skin && !morph) || !target) continue;
    auto value=*document_.find(target);
    // `edit` cria quando falta: "criado" sai de `find`, antes.
    const bool created=!value.components.find(scene::SkinnedMesh::descriptor);
    auto *mesh=static_cast<scene::SkinnedMesh *>(value.components.edit(scene::SkinnedMesh::descriptor));
    if(!mesh) return false;
    bool changed=created;
    if(skin) {
      std::vector<u64> bones;
      for(const auto joint:skin->joints) bones.push_back(objectOf(joint));
      // Religa quando a fonte mudou as juntas ou um osso deixou de existir;
      // uma troca de osso feita pelo autor, para outro objeto vivo, fica.
      bool stale=mesh->bones.size()!=bones.size();
      for(const auto bone:mesh->bones) stale=stale || !bone || !document_.find(static_cast<EditorEntityId>(bone));
      if(created || stale) {mesh->bones=std::move(bones);changed=true;}
    }
    if(morph && mesh->blendShapeWeights.size()!=morph->targetCount) {
      // Pesos iniciais: os do nó quando declarados, senão os da malha (0..1 → 0..100).
      const auto &initial=tree.nodes[node].morphWeights.size()==morph->targetCount?tree.nodes[node].morphWeights:morph->defaultWeights;
      const usize kept=mesh->blendShapeWeights.size();
      mesh->blendShapeWeights.resize(morph->targetCount);
      for(usize t=kept;t<morph->targetCount;++t) mesh->blendShapeWeights[t]=std::clamp(initial[t]*100.0f,
          -scene::SkinnedMesh::MaximumBlendShapeWeight,scene::SkinnedMesh::MaximumBlendShapeWeight);
      changed=true;
    }
    if(changed && !history_.applyValues(document_,target,value)) return false;
    if(created) ++meshes;
  }
  if(tree.animations.empty()) return true;
  const auto clipIds=resources::animationClipGuids(tree.guid,tree.animations);
  for(const auto root:roots) {
    if(!document_.find(root)) continue;
    auto value=*document_.find(root);
    const bool created=!value.components.find(scene::Animation::descriptor);
    auto *animation=static_cast<scene::Animation *>(value.components.edit(scene::Animation::descriptor));
    if(!animation) return false;
    bool changed=created;
    // v1 guardava o índice do clipe; a fonte carregada o traduz para identidade.
    if(animation->legacyClipIndex!=~0u) {
      if(animation->legacyClipIndex<clipIds.size()) animation->clip=clipIds[animation->legacyClipIndex];
      animation->legacyClipIndex=~0u;changed=true;
    }
    // Clipes novos da fonte entram na lista, como no Model Importer.
    for(const auto &id:clipIds)
      if(!animation->contains(id) && animation->appendClip(id)) changed=true;
    if(!animation->clip.valid()) {animation->clip=clipIds.front();changed=true;}
    if(changed && !history_.applyValues(document_,root,value)) return false;
    if(created) ++animations;
  }
  return true;
}

void EditorSession::reconcileStagedSource(const StagedSource &staged, ModelImportReport &report) {
  if(!staged.reimported) return;
  if(const auto *published=importNodeMap(staged.source)) {
    // Um passo de desfazer para tudo o que a reimportação fez na cena: vínculo
    // de objetos legados comprovados e reconciliação das instâncias.
    history_.begin("Reimportar recurso");
    // Objetos legados são provados contra a revisão que a cena USOU — a
    // anterior. Provar contra a nova faria os nós recém-chegados parecerem
    // conhecidos e apagados pelo usuário, e eles nunca entrariam.
    adoptLegacyImportInstances(document_,&history_,staged.source,staged.hasPrevious?staged.previousNodeMap:*published,
                               report.reconcile);
    reconcileImportInstances(document_,&history_,staged.source,*published,report.reconcile,
                             [this](const resources::AssetGuid &guid){return mapScene_.assetSlot(guid);});
    // Nós `_LOD<n>` que chegaram agora: grupo novo num pai novo, ou níveis
    // novos num grupo que já existia. Mesmo passo de Desfazer.
    report.lodGroups=addImportedLodGroups(document_,history_,mapScene_,report.reconcile.createdObjects,report.lodNotes);
    // Deformação e clipes de cada instância: objetos novos ganham os
    // componentes; os existentes têm ossos, blend shapes e clipes religados.
    for(const auto &tree:importedSources_) if(tree.guid==staged.source) {
      std::map<resources::AssetGuid,std::pair<std::vector<EditorEntityId>,std::vector<EditorEntityId>>> instances;
      std::vector<EditorEntityId> ids;document_.collectSubtree(document_.root(),ids);
      for(const auto id:ids) {
        const auto *link=scene::importLink(document_.find(id)->components);
        if(!link || link->source!=tree.guid || link->orphan || link->unlinked) continue;
        auto &[nodes,roots]=instances[link->instance];
        nodes.resize(tree.map.nodes.size(),kInvalidEntity);
        if(link->root) roots.push_back(id);
        if(link->node.valid() && link->primitive<0) {
          const auto index=tree.map.indexOf(link->node);
          if(index>=0 && static_cast<usize>(index)<nodes.size()) nodes[static_cast<usize>(index)]=id;
        }
      }
      for(auto &[instance,entry]:instances)
        bindImportedDeformation(tree,entry.first,entry.second,report.skinnedMeshes,report.animations);
    }
    history_.end();
    for(const auto &note:report.lodNotes) reportProblem(EditorConsoleSeverity::Info,note);
    if(report.reconcile.changed()) mapScene_.hydrateMaterials(document_);
    reportImportReconcile(report.reconcile,"Reimportação");
  }
}

bool EditorSession::instantiateModel(resources::AssetGuid source, ModelImportReport &report,bool wrapMultipleRoots) {
  report={};report.source=source;
  if(isPlaying() || history_.isOpen()) {report.diagnostic="Finalize a edição antes de instanciar.";return false;}
  usize slot=0,firstNew=0,total=0;
  for(;slot<importedSources_.size() && importedSources_[slot].guid!=source;++slot)
    firstNew+=importedSources_[slot].draws.size();
  if(slot==importedSources_.size()) {report.diagnostic="Recurso não carregado; reimporte a fonte.";return false;}
  for(const auto &block:importedSources_) total+=block.draws.size();
  if(mapScene_.assetCount()<total) {report.diagnostic="Biblioteca gráfica incompleta.";return false;}
  const usize primitives=mapScene_.assetCount()-total;
  const auto newDrawCount=importedSources_[slot].draws.size();
  const auto previousDocument=document_;const auto previousHistory=history_;
  std::vector<EditorEntityId> newRoots;
  const auto rollback=[&](const char *message) {
    document_=previousDocument;history_=previousHistory;report.objects=0;report.groups=0;
    report.diagnostic=message;return false;
  };
  // Reimportar NÃO cria objetos: os que já existem apontam para as mesmas
  // identidades e acabaram de ser reconciliados com a geometria nova.
  {
    // A árvore do arquivo vira árvore de objetos. Um nó por objeto, INCLUSIVE
    // os sem malha: é o grupo vazio que segura a porta no lugar quando a
    // carroceria se move, e descartá-lo é exatamente o que achata a hierarquia.
    const auto &tree=importedSources_[slot];
    // Vínculo de cada objeto com o nó da fonte (M08.2). A identidade da
    // instância precisa ser única entre sessões: relógio e contador.
    const auto &nodeMap=tree.map;
    const bool linkable=nodeMap.revision && nodeMap.nodes.size()==tree.nodes.size();
    const auto instance=resources::assetGuidFromSeed("instancia:"+source.text()+":"+
        std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+":"+std::to_string(++importInstanceCounter_));
    history_.begin("Instanciar recurso");
    auto instanceParent=document_.root();
    const auto rootCount=std::count_if(tree.nodes.begin(),tree.nodes.end(),[](const auto &node) {return node.parent<0;});
    if(wrapMultipleRoots && rootCount>1) {
      std::string name="Modelo importado";
      if(const auto *record=assets_.find(source)) {
        name=record->path;const auto slash=name.find_last_of("/\\");if(slash!=std::string::npos) name.erase(0,slash+1);
        const auto dot=name.find_last_of('.');if(dot!=std::string::npos) name.resize(dot);
      }
      instanceParent=history_.createEntity(document_,document_.root(),EditorEntityKind::Folder,name.c_str());
      if(!instanceParent) return rollback("Não foi possível criar o grupo da importação.");
      if(linkable) {
        auto wrapper=*document_.find(instanceParent);
        if(auto *link=scene::editImportLink(wrapper.components)) {
          link->source=source;link->instance=instance;link->root=true;link->revision=nodeMap.revision;link->baseName=wrapper.name;
          history_.applyValues(document_,instanceParent,wrapper);
        }
      }
      newRoots.push_back(instanceParent);++report.objects;++report.groups;
    }
    std::vector<EditorEntityId> created(tree.nodes.size(),kInvalidEntity);
    const float identity[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    for(usize n=0;n<tree.nodes.size();++n) {
      const auto &node=tree.nodes[n];
      const auto parent=node.parent>=0 && static_cast<usize>(node.parent)<created.size()
          ? created[static_cast<usize>(node.parent)] : instanceParent;
      if(parent==kInvalidEntity) { return rollback("Hierarquia do modelo fora de ordem."); }
      const auto id=history_.createEntity(document_,parent,EditorEntityKind::Mesh,
                                          node.name.empty()?"Objeto":node.name.c_str());
      if(!id) { return rollback("Não foi possível criar os objetos do modelo."); }
      created[n]=id;
      if(node.parent<0 && instanceParent==document_.root()) newRoots.push_back(id);
      auto value=*document_.find(id);
      // A pose local precisa caber em TRS; nunca substituir shear por identidade.
      EditorTransform local;
      if(runtime::localTransformForWorld(node.localMatrix,identity,local)) value.transform=local;
      else return rollback("Matriz do nó não pode ser instanciada sem perder sua transformação.");
      if(linkable) if(auto *link=scene::editImportLink(value.components)) {
        link->source=source;link->instance=instance;
        setImportLinkBase(*link,nodeMap.nodes[n],local,-1,nodeMap.revision);
        link->root=node.parent<0 && instanceParent==document_.root();
      }
      if(linkable) applyImportedNodeComponents(value,nodeMap.nodes[n]);
      history_.applyValues(document_,id,value);
      ++report.objects;
    }
    // Várias primitivas no mesmo nó são SLOTS do mesmo objeto (Entrega 2): um
    // nó autoral continua sendo um objeto, sem filhos inventados por desenho.
    std::vector<u32> placed(tree.nodes.size(),0);
    for(usize i=0;i<newDrawCount;++i) {
      const auto drawSlot=static_cast<u32>(primitives+firstNew+i)+1;
      const auto nodeIndex=i<tree.drawNodes.size()?tree.drawNodes[i]:0u;
      if(nodeIndex>=created.size()) { return rollback("Desenho sem nó no modelo."); }
      const auto target=created[nodeIndex];
      const u32 slot=placed[nodeIndex]++;
      if(slot>scene::MeshRenderer::MaximumSubmeshes) { return rollback("Primitivas demais num nó para os slots de material."); }
      auto value=*document_.find(target);
      auto *render=editMeshRenderer(value);
      if(!render) { return rollback("Não foi possível criar a malha do objeto."); }
      if(slot) render->submeshes.resize(slot);
      *render->editSlotMesh(slot)=drawSlot;
      *render->editSlotAsset(slot)=tree.identities[i];
      *render->editSlotMaterial(slot)=mapScene_.materialForAsset(drawSlot-1);
      history_.applyValues(document_,target,value);
    }
    for(usize n=0;n<tree.nodes.size();++n) if(!placed[n]) ++report.groups;
    // G6-B: o nó da malha com skin ou blend shapes ganha Malha deformável,
    // com os ossos já ligados aos objetos que esta instanciação criou para as
    // juntas; a raiz da instância ganha Animação com os clipes da fonte.
    if(!bindImportedDeformation(tree,created,newRoots,report.skinnedMeshes,report.animations))
      return rollback("Não foi possível criar a deformação ou a animação do modelo.");
    // Convenção `_LOD<n>` do Model Importer da Unity: com os slots já no lugar,
    // o grupo nasce medido e entra no mesmo Desfazer da instanciação. O grupo
    // da importação também conta como pai, porque modelos costumam trazer os
    // níveis como raízes; a raiz da cena nunca recebe o componente.
    {
      std::vector<EditorEntityId> candidates(created.begin(),created.end());
      if(instanceParent!=document_.root()) candidates.push_back(instanceParent);
      report.lodGroups=addImportedLodGroups(document_,history_,mapScene_,candidates,report.lodNotes);
    }
    history_.end();
  }

  state_.status="Recurso instanciado: "+std::to_string(report.objects)+" objetos"+
      (report.lodGroups?" · "+std::to_string(report.lodGroups)+(report.lodGroups==1?" LOD Group":" LOD Groups")+" pelos nomes _LOD":std::string())+
      (report.skinnedMeshes?" · "+std::to_string(report.skinnedMeshes)+" com esqueleto":std::string())+
      (report.animations?" · animação":std::string());
  for(const auto &note:report.lodNotes) reportProblem(EditorConsoleSeverity::Warning,note);
  // The newly imported object must be discoverable immediately. Fit only this
  // instance (all its roots), preserving authored scale and source transforms.
  if(!newRoots.empty()) {
    state_.workspace=EditorWorkspace::Scene;setSelection(newRoots.front());
    float low[3]{},high[3]{};bool first=true;
    std::vector<EditorEntityId> importedIds;
    for(const auto root:newRoots) {std::vector<EditorEntityId> subtree;document_.collectSubtree(root,subtree);importedIds.insert(importedIds.end(),subtree.begin(),subtree.end());}
    for(const auto id:importedIds) {
      float center[3],radius;
      if(!mapScene_.bounds(document_,id,center,radius)) continue;
      for(u32 axis=0;axis<3;++axis) {
        low[axis]=first?center[axis]-radius:std::min(low[axis],center[axis]-radius);
        high[axis]=first?center[axis]+radius:std::max(high[axis],center[axis]+radius);
      }
      first=false;
    }
    if(!first) {
      float center[3],radius2=0;
      for(u32 axis=0;axis<3;++axis) {center[axis]=(low[axis]+high[axis])*.5f;radius2+=(high[axis]-center[axis])*(high[axis]-center[axis]);}
      auto fit=projection_;
      if(layout_.viewport.height>0 && layout_.viewport.width<layout_.viewport.height)
        fit.verticalFieldOfViewRadians=2*std::atan(std::tan(fit.verticalFieldOfViewRadians*.5f)*layout_.viewport.width/layout_.viewport.height);
      frameEditorCamera(camera_,center,std::max(.01f,std::sqrt(radius2)),fit);
    }
  }
  return true;
}

bool EditorSession::commitModelImport(std::span<const u8> bytes,const resources::GltfImport &model,
                                      const std::string &path,const std::string &expectedHash,ModelImportReport &report,
                                      resources::ImportAmbiguityPolicy policy,
                                      std::span<const resources::AssetGuid> excludedNodes,
                                      std::span<const FolderCompanion> companions,std::string_view contentHash,
                                      std::span<const resources::AssetGuid> sourceDependencies) {
  report={};
  if(isPlaying()) {report.diagnostic="Pare a execução antes de publicar.";return false;}
  // O mapa de nós entra na MESMA transação que fonte e registro.
  const auto *known=assets_.findByPath(path);
  const auto mapSource=known?known->guid:resources::assetGuidFromSeed("fonte:"+path);
  EditorImportTransaction transaction(files_.rootPath());
  std::vector<EditorImportTransaction::Companion> moved;
  for(const auto &companion:companions) moved.push_back({companion.staged,companion.relative});
  if(!transaction.begin(path,expectedHash,report.diagnostic,resources::importNodeMapPath(mapSource),moved)) {
    if(report.diagnostic.empty()) report.diagnostic="Não foi possível preparar os backups da importação.";
    return false;
  }
  const auto previousSources=importedSources_;const auto previousAssets=assets_;
  const auto previousDocument=document_;const auto previousMap=mapScene_;const auto previousHistory=history_;
  const bool previousDirty=assetRegistryDirty_;
  bool published=publishModel(model,contentHash.empty()?Sha256::hex(bytes):std::string(contentHash),path,report,policy,excludedNodes);
  const bool libraryPublished=published;
  if(published&&!sourceDependencies.empty()) {
    const auto record=*assets_.find(report.source);
    published=assets_.publishImport(report.source,record.contentHash,record.importerVersion,record.importerParameters,record.derived,
                                   {sourceDependencies.begin(),sourceDependencies.end()});
  }
  const auto *nodeMap=published?importNodeMap(report.source):nullptr;
  if(published && nodeMap && transaction.commit(bytes,assets_.serialize(),nodeMap->serialize())) {
    assetRegistryDirty_=false;files_.rebuildTree();state_.selectedFile=path;
    // Reveal each ancestor so the selected resource is actually visible.
    for(usize i=0;i<files_.tree().size();++i) {
      const auto entry=files_.tree()[i];
      if(entry.directory && !entry.expanded && !entry.relativePath.empty() && path.starts_with(entry.relativePath+"/"))
        files_.toggle(static_cast<unsigned>(i));
    }
    return true;
  }
  if(libraryPublished) {
    report.diagnostic="Não foi possível gravar fonte e registro; importação revertida.";
    importedSources_=previousSources;assets_=previousAssets;
    std::string rollback;
    if(!publishAndAdopt(flattenSources(importedSources_),rollback)) report.diagnostic+=" GPU: "+rollback;
    // A reconciliação entrou no histórico: cena e histórico voltam juntos.
    document_=previousDocument;mapScene_=previousMap;history_=previousHistory;assetRegistryDirty_=previousDirty;
  }
  if(!transaction.rollback()) report.diagnostic+=" Recuperação de disco pendente; backups preservados.";
  return false;
}

void EditorSession::showImportPreview(std::string path,const resources::GltfImport &model,std::string_view contentHash,
                                     const resources::ImportProfile &prepared,
                                     const resources::ImportSourceReport *sourceReport) {
  state_.importEnvironment=false;state_.importTexture=false;
  state_.importAmbiguities=0;state_.importAmbiguityChoice=0;
  state_.importPreparedScale=prepared.scale;state_.importPreparedTextureDimension=prepared.maximumTextureDimension;
  state_.importPreparedNormals=prepared.normals;state_.importPreparedNormalWeighting=prepared.normalWeighting;
  state_.importPreparedSmoothingAngle=prepared.smoothingAngle;
  state_.importPreparedTangents=prepared.tangents;state_.importPreparedCameras=prepared.importCameras;
  state_.importPreparedLights=prepared.importLights;
  state_.importPreparedTextureCompression=prepared.textureCompression;
  state_.importPreparedGenerateLods=prepared.generateLods;state_.importPreparedLodLevels=prepared.maximumLodLevels;
  state_.importPreparedOptimizeOrder=prepared.optimizePolygonOrder;
  state_.importReprepare=false;
  // R3: saídas estruturadas para as abas do painel (I23).
  {
    const auto nodeCount=model.nodes.size();
    std::vector<u32> depth(nodeCount,0),draws(nodeCount,0);
    std::vector<std::array<float,16>> world(nodeCount);
    for(const auto node:model.drawNodes) if(node<nodeCount) ++draws[node];
    state_.importNodes.clear();state_.importNodes.reserve(nodeCount);
    float low[3]{INFINITY,INFINITY,INFINITY},high[3]{-INFINITY,-INFINITY,-INFINITY};
    for(usize n=0;n<nodeCount;++n) {
      const auto &node=model.nodes[n];
      const bool child=node.parent>=0 && static_cast<usize>(node.parent)<n;
      depth[n]=child?depth[static_cast<usize>(node.parent)]+1:0;
      // Mundo = pai · local, colunas primeiro. Os pais vêm antes dos filhos na lista.
      if(child) {
        const auto &a=world[static_cast<usize>(node.parent)];
        for(u32 c=0;c<4;++c) for(u32 r=0;r<4;++r) {
          float sum=0;for(u32 k=0;k<4;++k) sum+=a[k*4+r]*node.localMatrix[c*4+k];
          world[n][c*4+r]=sum;
        }
      } else std::copy(node.localMatrix,node.localMatrix+16,world[n].begin());
      state_.importNodes.push_back({node.name,depth[n],draws[n]});
    }
    // Tamanho aproximado: esfera de cada desenho levada ao mundo do seu nó.
    for(usize d=0;d<model.draws.size() && d<model.drawNodes.size();++d) {
      const auto node=model.drawNodes[d];if(node>=nodeCount) continue;
      const auto &m=world[node];const auto &draw=model.draws[d];
      const float stretch=std::max({std::hypot(m[0],m[1],m[2]),std::hypot(m[4],m[5],m[6]),std::hypot(m[8],m[9],m[10])});
      for(u32 axis=0;axis<3;++axis) {
        const float centre=m[axis]*draw.boundsCenter[0]+m[4+axis]*draw.boundsCenter[1]+m[8+axis]*draw.boundsCenter[2]+m[12+axis];
        low[axis]=std::min(low[axis],centre-draw.boundsRadius*stretch);
        high[axis]=std::max(high[axis],centre+draw.boundsRadius*stretch);
      }
    }
    state_.importHasExtent=low[0]<=high[0] && std::isfinite(low[0]) && std::isfinite(high[0]);
    for(u32 axis=0;axis<3;++axis) state_.importExtent[axis]=state_.importHasExtent?high[axis]-low[axis]:0;
    state_.importTextures.clear();state_.importTextures.reserve(model.textures.size());
    for(usize t=0;t<model.textures.size();++t) {
      EditorScreenState::ImportTextureRow row;
      if(const auto &texture=model.textures[t]) {
        row.width=texture->width;row.height=texture->height;row.levels=texture->levels;row.srgb=texture->srgb;
        row.format=texture->format;row.bytes=texture->expectedBytes();
      }
      for(const auto &material:model.materials)
        for(const auto index:material.textureIndices) if(index==t) ++row.uses;
      state_.importTextures.push_back(row);
    }
    // G6-A: a fonte medida, malha a malha. As posições do buffer não têm a
    // escala do perfil — ela mora na pose das raízes —, então o relatório mede o
    // mundo do arquivo e recebe 1 aqui.
    state_.importMeshes.clear();state_.importMeshSummary.clear();
    state_.importMeshSelected=0;state_.importMeshDetail=false;
    resources::ImportSourceReport measured;
    const bool reported=sourceReport || resources::buildImportSourceReport(model,1.0f,measured);
    const auto &report=sourceReport?*sourceReport:measured;
    if(reported) {
      const auto metres=[](float value) {return decimalText(value,value<10?2:1);};
      const auto thousands=[](u64 value) {
        return value>=10000?decimalText(static_cast<float>(value)/1000.0f,1)+" mil":std::to_string(value);
      };
      state_.importMeshes.reserve(report.meshes.size());
      for(const auto &mesh:report.meshes) {
        EditorScreenState::ImportMeshRow row;
        row.name=mesh.name;row.material=mesh.material+(mesh.normalMap?" · com mapa normal":"");
        row.level=static_cast<u8>(mesh.level);
        row.counts=thousands(mesh.data.vertexCount)+" vértices · "+thousands(mesh.data.triangleCount)+" triângulos";
        // Os canais na ordem em que a prévia de malha da Unity os lista.
        for(const auto &[present,label]:{std::pair{mesh.data.hasUv0,"UV0"},std::pair{mesh.data.hasUv1,"UV1"},
                                         std::pair{mesh.data.hasNormals,"Normais"},std::pair{mesh.data.hasTangents,"Tangentes"},
                                         std::pair{mesh.data.hasColors,"Cores"}})
          if(present) row.channels+=(row.channels.empty()?"":", ")+std::string(label);
        if(row.channels.empty()) row.channels="sem canais além da posição";
        row.size=metres(mesh.data.boundsMaximum[0]-mesh.data.boundsMinimum[0])+" × "+
                 metres(mesh.data.boundsMaximum[1]-mesh.data.boundsMinimum[1])+" × "+
                 metres(mesh.data.boundsMaximum[2]-mesh.data.boundsMinimum[2])+" m";
        if(const float density=mesh.texelDensity();density>0)
          row.density=decimalText(density,0)+" texels/m em "+std::to_string(mesh.textureWidth)+"×"+
                      std::to_string(mesh.textureHeight);
        if(mesh.data.stretchRatio>0)
          row.stretch=mesh.data.stretchRatio<1.5f?"uniforme ("+decimalText(mesh.data.stretchRatio,1)+"×)":
                      decimalText(mesh.data.stretchRatio,1)+"× entre o decil baixo e o alto";
        row.issues=mesh.issues;
        state_.importMeshes.push_back(std::move(row));
      }
      state_.importMeshSummary=std::to_string(report.nodeCount)+" nós · "+std::to_string(report.meshCount)+" malhas · "+
          std::to_string(report.materialCount)+" materiais · profundidade "+std::to_string(report.depth);
      state_.importMeshSummary+=" · "+metres(report.largestDimension())+" m na maior dimensão";
      if(report.densityMedian>0) {
        state_.importMeshSummary+=" · densidade mediana "+decimalText(report.densityMedian,0)+" texels/m";
        // Duas densidades muito diferentes no MESMO arquivo é o que produz uma
        // parede nítida ao lado de uma borrada, sem nada a ajustar na engine.
        if(report.densityLowest>0 && report.densityHighest/report.densityLowest>=4)
          state_.importMeshSummary+=" (de "+decimalText(report.densityLowest,0)+" a "+
              decimalText(report.densityHighest,0)+")";
      }
    }
  }
  state_.importPanel=true;state_.importReady=true;state_.importError=false;state_.importPage=0;state_.importPath=std::move(path);
  state_.importStatus=assets_.findByPath(state_.importPath)?"Atualizar recurso existente":"Registrar novo recurso";
  state_.importSummary=std::to_string(model.nodes.size())+" nós · "+std::to_string(model.draws.size())+" malhas · "+
      std::to_string(model.materials.size())+" materiais";
  // G6-B: skin e clipes importados, e o que ficou de fora com o motivo contado.
  {
    usize skins=0;for(const auto &skin:model.skins) if(!skin.joints.empty()) ++skins;
    if(skins) state_.importSummary+=" · "+std::to_string(skins)+(skins==1?" skin":" skins");
    if(!model.animations.empty())
      state_.importSummary+=" · "+std::to_string(model.animations.size())+(model.animations.size()==1?" clipe":" clipes");
    if(model.skippedSkins || model.unsupportedAnimationChannels)
      state_.importSummary+="\nNão importado: "+std::to_string(model.skippedSkins)+" skin(s) e "+
          std::to_string(model.unsupportedAnimationChannels)+" canal(is) de animação (pesos de morph ou alvo inválido).";
  }
  state_.importSummary+="\nSó recurso: guarda no projeto.\nImportar na cena: guarda, instancia e enquadra o modelo.";
  // G6-A: o que a medição da fonte achou. Vai no Resumo porque é ali que o
  // autor decide publicar; o detalhe de cada malha fica na aba Malhas.
  if(!state_.importMeshes.empty()) {
    u32 errors=0,warnings=0;
    for(const auto &mesh:state_.importMeshes) {if(mesh.level==2) ++errors;else if(mesh.level==1) ++warnings;}
    state_.importSummary+=errors||warnings
        ? "\nMalhas medidas: "+std::to_string(errors)+" com erro e "+std::to_string(warnings)+
          " com atenção — a aba Malhas diz qual e por quê."
        : "\nMalhas medidas: nada a apontar na fonte.";
  }
  // Texturas (M09.1): o que entrou, o que não entrou e por quê. Ausência de
  // textura nunca pode parecer material final correto.
  if(!model.textures.empty())
    state_.importSummary+="\nTexturas: "+std::to_string(model.textures.size())+" aplicadas ("+
      std::to_string((model.textureBytes+(u64{1}<<19))>>20)+" MB com mipmaps).";
  if(model.compressedTextures || model.compressionFailures) {
    const auto format=[&]{
      for(const auto &texture:model.textures) if(texture && texture->format!=renderer::AuthoringTextureAstc4x4 &&
                                                texture->format!=renderer::AuthoringTextureRgba8)
        return texture->format==renderer::AuthoringTextureAstc6x6?std::string("ASTC 6×6"):std::string("ASTC 8×8");
      return std::string("ASTC 4×4");
    }();
    state_.importSummary+="\nCompressão: "+std::to_string(model.compressedTextures)+" textura(s) em "+format+
      " na importação (perfil)"+(model.compressionFailures?"; "+std::to_string(model.compressionFailures)+
      " ficaram em RGBA8 porque o encoder recusou (motivo nas notas).":std::string("."));
  }
  // S3: o que a geração de LOD e a ordem de índices fizeram, medido na importação.
  if(model.lodLevels)
    state_.importSummary+="\nLOD: "+std::to_string(model.lodDraws)+" desenho(s) com "+std::to_string(model.lodLevels)+
      " nível(is) extra; "+std::to_string(model.lodTriangles)+" triângulos nos níveis sobre "+
      std::to_string(model.lodSourceTriangles)+" da fonte"+
      (model.lodSkippedSmall?"; "+std::to_string(model.lodSkippedSmall)+" com menos de 256 triângulos":std::string())+
      (model.lodSkippedDeformed?"; "+std::to_string(model.lodSkippedDeformed)+" com skin ou blend shapes sem LOD":std::string())+".";
  // S4: nenhuma luz "some" em silêncio. Intensidade 0 no arquivo é importada
  // como está (é o que o arquivo diz), e o resumo aponta onde acendê-las.
  if(!model.lights.empty()) {
    u32 zero=0;
    for(const auto &light:model.lights) zero+=!(light.intensity>0);
    state_.importSummary+="\nLuzes do arquivo: "+std::to_string(model.lights.size())+
      (zero?"; "+std::to_string(zero)+" com intensidade 0 no arquivo, importadas assim e sem iluminar. "
            "Acenda em Gráficos › Luz e pós › Explorador de luzes.":std::string("."));
  }
  if(model.skippedLights)
    state_.importSummary+="\n"+std::to_string(model.skippedLights)+" luz(es) do arquivo não entraram "
      "(perfil sem \"Importar luzes\" ou nó incompatível; motivo nas notas).";
  if(model.acmrBefore>0 && model.acmrAfter>0 && model.acmrAfter<model.acmrBefore)
    state_.importSummary+="\nOrdem dos polígonos: "+decimalText(model.acmrBefore,2)+" → "+decimalText(model.acmrAfter,2)+
      " vértices processados por triângulo (cache de 16).";
  if(model.astcTextures)
    state_.importSummary+="\nTexturas KTX2 em ASTC 4x4 na GPU: "+std::to_string(model.astcTextures)+
      " (o aparelho amostra ASTC; sem RGBA intermediário).";
  if(model.bakedTextureTransforms)
    state_.importSummary+="\nTransformação de UV (KHR_texture_transform) aplicada nas UVs em "+
      std::to_string(model.bakedTextureTransforms)+" referência(s) de textura.";
  if(model.mirroredNodes)
    state_.importSummary+="\nReflexão (escala negativa) resolvida em "+std::to_string(model.mirroredNodes)+
      " nó(s): geometria espelhada e pose com escala positiva.";
  if(model.dracoPrimitives || model.meshoptViews || model.ktx2Images)
    state_.importSummary+="\nDescomprimido na importação: "+std::to_string(model.dracoPrimitives)+" primitiva(s) Draco, "+
      std::to_string(model.meshoptViews)+" visão(ões) meshopt, "+std::to_string(model.ktx2Images)+" imagem(ns) KTX2.";
  if(model.reducedTextures)
    state_.importSummary+="\nResolução reduzida em "+std::to_string(model.reducedTextures)+" textura(s), até "+
      std::to_string(model.residentTextureDimension)+" px, para caber no limite do aparelho.";
  if(model.skippedTextures)
    state_.importSummary+="\nTexturas não aplicadas: "+std::to_string(model.skippedTextures)+"; esses slots ficam só com os fatores.";
  for(const auto &note:model.textureNotes) state_.importSummary+="\n• "+note;
  for(const auto &note:model.notes) state_.importSummary+="\n• "+note;
  if(model.unappliedTextureTransforms)
    state_.importSummary+="\nTransformação de UV (KHR_texture_transform) não aplicada em "+
      std::to_string(model.unappliedTextureTransforms)+" slot(s): a textura aparece sem ela.";
  if(model.unappliedOcclusion)
    state_.importSummary+="\nOclusão não aplicada em "+std::to_string(model.unappliedOcclusion)+
        " material(is): só entra no canal R do mapa metal/rugosidade, no mesmo UV e com força 1.";
  if(model.appliedOcclusion)
    state_.importSummary+="\nOclusão aplicada pelo canal R do mapa metal/rugosidade em "+
        std::to_string(model.appliedOcclusion)+" material(is).";
  // G2: o que a importação DERIVOU e o que ela encontrou de errado na fonte.
  // Geração de normal não é defeito — é o glTF permitindo o que este renderer
  // não aceita —, mas precisa aparecer para o autor saber por que a malha
  // sombreia diferente do editor 3D de origem.
  if(model.generatedNormalPrimitives)
    state_.importSummary+="\nNormais calculadas em "+std::to_string(model.generatedNormalPrimitives)+
      " primitiva(s): o arquivo não trazia NORMAL ou o perfil pediu recálculo.";
  if(model.generatedTangentPrimitives)
    state_.importSummary+="\nTangentes calculadas em "+std::to_string(model.generatedTangentPrimitives)+
      " primitiva(s) com mapa normal.";
  // Estes dois são da FONTE, e nenhuma escolha de importação conserta. Trocar a
  // textura por uma maior não melhora um mapeamento que não existe.
  if(model.texturedPrimitivesWithoutUv)
    state_.importSummary+="\nSem UV com textura declarada em "+std::to_string(model.texturedPrimitivesWithoutUv)+
      " primitiva(s): a fonte precisa trazer TEXCOORD, não há coordenada a inventar.";
  if(model.stretchedUvPrimitives)
    state_.importSummary+="\nDensidade de texel muito desigual em "+std::to_string(model.stretchedUvPrimitives)+
      " primitiva(s) (pior razão "+std::to_string(static_cast<u32>(model.worstTexelDensityRatio+.5f))+
      "×): textura esticada vem da UV da fonte.";
  if(!model.cameras.empty())
    state_.importSummary+="\nCâmeras importadas: "+std::to_string(model.cameras.size())+
      "; entram desligadas, o enquadramento fica disponível sem trocar a câmera do Play.";
  if(model.skippedCameras)
    state_.importSummary+="\nCâmeras não importadas: "+std::to_string(model.skippedCameras)+
      "; ligue \"Importar câmeras\" no perfil para trazê-las.";
  if(model.skippedAnimations||model.skippedSkins)
    state_.importSummary+="\nNão suportado neste perfil: "+std::to_string(model.skippedAnimations)+" animações, "+
      std::to_string(model.skippedSkins)+" skins.";
  if(!model.appearanceExtensions.empty()) {
    state_.importSummary+="\nGeometria estática; aparência avançada não reproduzida:";
    for(const auto &extension:model.appearanceExtensions) state_.importSummary+="\n"+extension;
  }
  // A mesma correspondência que a publicação fará, sem publicar — também para
  // fonte NOVA: a identidade da fonte nova é determinística ("fonte:"+caminho),
  // e é com ela que o autor escolhe o que excluir já na primeira importação.
  const auto *record=assets_.findByPath(state_.importPath);
  importPreviewSource_=record?record->guid:resources::assetGuidFromSeed("fonte:"+state_.importPath);
  importPreviewMap_={};importPreviewMatch_={};
  resources::ImportNodeMap previous;
  const bool hasPrevious=record && previousImportMap(importPreviewSource_,previous);
  {
    std::string mapDiagnostic;
    importPreviewMapped_=resources::buildImportNodeMap(model,importPreviewSource_,contentHash,hasPrevious?&previous:nullptr,
        resources::ImportAmbiguityPolicy::Refuse,importPreviewMap_,importPreviewMatch_,mapDiagnostic,state_.importExcludedNodes);
    // O mapa segue a ordem do arquivo, a mesma das linhas da Estrutura. Sem mapa
    // (correspondência ambígua), as linhas ficam sem identidade e sem escolha de
    // exclusão até o autor decidir a ambiguidade.
    if(importPreviewMapped_ && importPreviewMap_.nodes.size()==state_.importNodes.size())
      for(usize n=0;n<state_.importNodes.size();++n) {
        state_.importNodes[n].node=importPreviewMap_.nodes[n].id;
        state_.importNodes[n].excluded=importPreviewMap_.nodes[n].excluded;
      }
  }
  refreshImportImpact();
  if(record) {
    const auto &match=importPreviewMatch_;
    auto &summary=state_.importSummary;
    if(match.sameContent&&!match.excluded&&!match.included) summary+="\nMesmo conteúdo da versão publicada; nada muda nas instâncias.";
    else if(match.sameContent) summary+="\nMesmo conteúdo da versão publicada; muda só o que o perfil exclui.";
    else if(hasPrevious) {
      summary+="\nCorrespondência: "+std::to_string(match.byAuthoredId)+" por id do autor · "+std::to_string(match.byStructure)+
          " por estrutura · "+std::to_string(match.renamed)+" renomeados · "+std::to_string(match.reparented)+" com pai novo";
      if(match.ambiguities.empty())
        summary+="\nNós novos: "+std::to_string(match.added)+" · removidos da fonte: "+std::to_string(match.removed);
    } else summary+="\nSem mapa anterior: identidades derivadas das chaves da fonte.";
    summary+="\nInstâncias: alterações locais preservadas; removidos com dados locais ficam órfãos.";
    if(!match.ambiguities.empty()) {
      state_.importAmbiguities=static_cast<u32>(match.ambiguities.size());
      summary+="\nAmbíguos — escolha abaixo antes de publicar:";
      for(const auto &item:match.ambiguities)
        summary+="\n• "+(item.parent.empty()?std::string("raiz"):item.parent)+" / "+item.name+": "+
            std::to_string(item.previous)+" anteriores, "+std::to_string(item.incoming)+" novos";
    }
  }
}

// O que a publicação faz com a CENA ABERTA, refeito a cada nó marcado ou
// desmarcado na Estrutura. O resumo acima dele fala da FONTE e não muda com a
// exclusão; este fala do que o autor já montou, que é o que muda.
void EditorSession::refreshImportImpact() {
  auto &text=state_.importImpact;
  text.clear();
  u32 excludedRows=0;
  for(const auto &row:state_.importNodes) if(row.excluded) ++excludedRows;
  if(excludedRows)
    text+="Excluídos pelo perfil: "+std::to_string(excludedRows)+" nó(s). Não entram na cena, e a identidade deles "
          "fica guardada: reincluir depois traz de volta o mesmo nó.";
  // Fonte que ainda não está no projeto não tem objeto nenhum na cena.
  if(!assets_.findByPath(state_.importPath)) return;
  const auto impact=importSceneImpact(document_,importPreviewSource_,importPreviewMatch_,
                                      importPreviewMapped_?&importPreviewMap_:nullptr);
  if(!impact.linked && !impact.alreadyOrphan && !impact.unlinked) return;
  const auto line=[&](const std::string &value) {if(!text.empty()) text+="\n";text+=value;};
  std::string scene="Nesta cena: "+std::to_string(impact.instances)+" instância(s), "+std::to_string(impact.linked)+
                    " objeto(s) vinculado(s)";
  if(impact.editedObjects) scene+=", "+std::to_string(impact.editedObjects)+" com edição local";
  if(impact.unlinked) scene+=", "+std::to_string(impact.unlinked)+" desvinculado(s)";
  if(impact.alreadyOrphan) scene+=", "+std::to_string(impact.alreadyOrphan)+" já órfão(s)";
  line(scene+".");
  if(impact.removedObjects)
    line("Saem da cena: "+std::to_string(impact.removedObjects)+" objeto(s) sem edição local"+
         (impact.excludedObjects?" (inclui os de nós excluídos).":"."));
  if(impact.orphanObjects) {
    line("Ficam órfãos (com as edições dentro): "+std::to_string(impact.orphanObjects)+" objeto(s)");
    for(const auto &name:impact.orphanNames) line("• "+name);
    if(impact.orphanObjects>impact.orphanNames.size()) line("• …");
  }
  if(impact.newNodes&&impact.instances)
    line("Entram em cada instância: "+std::to_string(impact.newNodes)+" nó(s) novo(s) da fonte.");
  if(!impact.touchesScene()) line("Nenhum objeto desta cena muda de lugar na reimportação.");
}

// Marca ou desmarca um nó na Estrutura. Só o nó cujo pai NÃO está excluído
// aceita a escolha: o filho de um excluído já não vem, e desmarcá-lo sozinho
// não teria efeito — oferecer o toque seria oferecer um botão que não faz nada.
bool EditorSession::toggleImportNodeExclusion(usize row) {
  if(row>=state_.importNodes.size() || !importPreviewMapped_) return false;
  const auto node=state_.importNodes[row].node;
  if(!node.valid()) return false;
  const auto *record=importPreviewMap_.find(node);
  if(!record) return false;
  if(record->parent.valid()) if(const auto *parent=importPreviewMap_.find(record->parent); parent&&parent->excluded) return false;
  auto &excluded=state_.importExcludedNodes;
  const auto at=std::find(excluded.begin(),excluded.end(),node);
  if(at!=excluded.end()) excluded.erase(at);
  else excluded.push_back(node);
  resources::markExcludedNodes(importPreviewMap_,excluded);
  for(usize n=0;n<state_.importNodes.size() && n<importPreviewMap_.nodes.size();++n)
    state_.importNodes[n].excluded=importPreviewMap_.nodes[n].excluded;
  refreshImportImpact();
  return true;
}

bool EditorSession::importMap(std::span<const renderer::MapDrawRecord> draws, std::span<const renderer::MapMaterialRecord> materials, bool instantiate, std::span<const u8> vertices, std::span<const u32> indices, u64 packageFingerprint) {
  if(isPlaying()) return false;
  cancelPointers();
  if (!mapScene_.import(document_, draws, materials, instantiate,vertices,indices,packageFingerprint)) return false;
  document_.setTags(projectTags_);
  packageFingerprint_=packageFingerprint;
  importedSources_.clear();
  state_.creationAvailable=creationAlwaysAvailable();
  for(u32 i=0;i<mapScene_.assetCount();++i) {
    const auto flags=mapScene_.materialFlagsForAsset(i);
    const auto primitive=renderer::primitiveFromFlags(flags);
    if(scene::validPrimitive(primitive)) for(u32 recipe=0;recipe<editorCreationCatalog.size();++recipe)
      if(editorCreationCatalog[recipe].primitive==primitive||editorCreationCatalog[recipe].childVisual==primitive) state_.creationAvailable[recipe]=1;
    if(flags & renderer::BoxAuthoringResource) {
      enableCreation(state_,EditorWidget::CreateCube);
      enableCreation(state_,EditorWidget::CreateGround);
      enableCreation(state_,EditorWidget::CreateSceneTemplate);
      if(flags & renderer::WaterAuthoringResource) enableCreation(state_,EditorWidget::CreateBuoyantBox);
    }
    if(flags & renderer::WaterRouteResource) enableCreation(state_,EditorWidget::CreateRiverWater);
    if((flags & renderer::WaterAuthoringResource) && (flags & renderer::MapMaterialWater) && !(flags & renderer::WaterRouteResource))
      enableCreation(state_,(flags & renderer::MapMaterialWaterCameraGrid)?
                            EditorWidget::CreateOceanWater:EditorWidget::CreateFiniteWater);
  }
  if(state_.workspace==EditorWorkspace::Assets && !mapScene_.assetCount()) state_.workspace=EditorWorkspace::Scene;
  // Sem água na cena, a seção de água some; o projeto continua na primeira seção.
  if(state_.projectSection==EditorProjectSection::Water && !waterCreationAvailable(state_))
    state_.projectSection=EditorProjectSection::Layers;
  state_.creationCategory=0;state_.creationSelection=0;state_.creationScroll=0;

  sceneEpoch_=nextSceneEpoch();state_.groupPicker=false;state_.editingGroupName=false;state_.groupEntity=0;cameraPreview_.close();state_.colorField=0;state_.impactInstance=0;state_.impactAsset={};state_.impactTrail.clear();state_.impactRepair=false;state_.impactReplacement={};state_.impactRepairMaterial={};
  history_.clear();
  state_.selection = kInvalidEntity;
  state_.hierarchyScroll = 0;
  state_.collapsedEntities.clear();state_.renameEntity=kInvalidEntity;
  frameAll();
  return true;
}

EditorEntityId EditorSession::instantiateAsset(u32 index, EditorEntityId parent, const float worldPosition[3], const EditorAssetInstantiation *options) {
  if(history_.isOpen()) return kInvalidEntity;
  if(!history_.begin("Instanciar malha")) return kInvalidEntity;
  const auto id=instantiateAssetInTransaction(index,parent,worldPosition,options);
  history_.end();
  return id;
}

EditorEntityId EditorSession::instantiateAssetInTransaction(u32 index, EditorEntityId parent, const float worldPosition[3], const EditorAssetInstantiation *options) {
  if(!worldPosition || !mapScene_.asset(index) || !document_.exists(parent)) return kInvalidEntity;
  EditorEntity values;auto *render=editMeshRenderer(values);if(!render) return kInvalidEntity;render->mesh=index+1;render->asset=mapScene_.assetGuid(index);render->material=mapScene_.materialForAsset(index);
  float parentWorld[16],world[16];EditorTransform pose;
  std::copy(worldPosition,worldPosition+3,pose.position);editorTransformMatrix(pose,world);
  if(!editorWorldMatrix(document_,parent,parentWorld) ||
     !editorLocalTransformForWorld(world,parentWorld,values.transform) || !validEditorAppearance(values)) return kInvalidEntity;
  const bool water=(mapScene_.materialFlagsForAsset(index)&renderer::MapMaterialWater)!=0;
  values.rigidBodyEnabled=(mapScene_.materialFlagsForAsset(index)&(renderer::BoxAuthoringResource|renderer::WaterAuthoringResource))==(renderer::BoxAuthoringResource|renderer::WaterAuthoringResource);
  if(!setWaterBodyFlags(values,true,(mapScene_.materialFlagsForAsset(index)&renderer::MapMaterialWaterCameraGrid)!=0)) return kInvalidEntity;
  if(mapScene_.materialFlagsForAsset(index)&renderer::WaterRouteResource) {
    auto *route=editWaterRoute(values);if(!route) return kInvalidEntity;
    route->count=3;
    route->points[0].position[2]=-12;
    route->points[1].position[0]=8;
    route->points[2].position[2]=12;
  }
  const char *role=waterRoute(values).count?"Rio":waterBody(values).infinite?"Oceano":water?"Superfície de água":values.rigidBodyEnabled?"Corpo rígido":"Malha";
  char name[64];std::snprintf(name,sizeof(name),"%s %u",role,document_.entityCount());assignEntityName(values,name);
  if(options) {
    if(!options->name.empty()) assignEntityName(values,options->name);
    std::copy(options->scale,options->scale+3,values.transform.scale);values.rigidBodyEnabled=options->rigidBody;
    if(!isTransformValid(values.transform)) return kInvalidEntity;
  }
  const auto flags=mapScene_.materialFlagsForAsset(index);
  const auto primitive=renderer::primitiveFromFlags(flags);
  if(scene::validPrimitive(primitive) && !(flags&renderer::WaterAuthoringResource) &&
     !runtime::configurePrimitive(values,primitive,{index+1,mapScene_.assetGuid(index),mapScene_.materialForAsset(index)})) return kInvalidEntity;
  const auto id=history_.createEntity(document_,parent,water?EditorEntityKind::Water:EditorEntityKind::Mesh,values.name);
  if(id) {
    if(!history_.applyValues(document_,id,values)) {history_.cancel(document_);return kInvalidEntity;}
    setSelection(id);state_.status="Malha adicionada";
  }
  return id;
}

void EditorSession::setRenderStats(u32 width,u32 height,float gpuMilliseconds,
                                   std::string_view detectedLevel) {
  state_.qualityDetected=std::string(detectedLevel);
  std::string text="Renderizando "+std::to_string(width)+"×"+std::to_string(height);
  if(gpuMilliseconds>0) text+=" · GPU "+decimalText(gpuMilliseconds,1)+" ms";
  state_.qualityStats=std::move(text);
}

u32 EditorSession::boxAssetSlot() const {
  for(u32 i=0;i<mapScene_.assetCount();++i)
    if(mapScene_.materialFlagsForAsset(i)&renderer::BoxAuthoringResource) return i+1;
  return 0;
}

bool EditorSession::createSceneTemplate(u32 index) {
  const auto templates=sceneTemplates();
  if(index>=templates.size() || isPlaying() || history_.isOpen()) return false;
  const auto slot=boxAssetSlot();
  if(!slot) {state_.status="O cubo autoral não está nesta biblioteca";return false;}
  const auto &model=templates[index];
  if(!history_.begin(model.name)) return false;
  const auto group=history_.createEntity(document_,document_.root(),EditorEntityKind::Folder,model.name);
  if(!group) {history_.end();return false;}
  std::vector<EditorEntityId> created(model.nodes.size(),kInvalidEntity);
  bool complete=true;
  for(usize n=0;n<model.nodes.size();++n) {
    const auto &node=model.nodes[n];
    const auto parent=node.parent<0?group:
        (static_cast<usize>(node.parent)<n?created[static_cast<usize>(node.parent)]:kInvalidEntity);
    if(parent==kInvalidEntity) {complete=false;break;}
    EditorEntityId id=kInvalidEntity;
    if(node.mesh) {
      // A pose do modelo é LOCAL ao pai; a instanciação recebe mundo, então o
      // caminho comum é montar a pose depois, com o valor do nó.
      const float origin[3]{0,0,0};
      EditorAssetInstantiation options;options.name=node.name;
      std::copy(node.scale,node.scale+3,options.scale);
      id=instantiateAssetInTransaction(slot-1,parent,origin,&options);
    } else {
      id=history_.createEntity(document_,parent,EditorEntityKind::Folder,node.name);
    }
    if(!id) {complete=false;break;}
    auto values=*document_.find(id);
    std::copy(node.position,node.position+3,values.transform.position);
    std::copy(node.rotationDegrees,node.rotationDegrees+3,values.transform.rotationDegrees);
    std::copy(node.scale,node.scale+3,values.transform.scale);
    if(node.mesh) {
      auto *render=editMeshRenderer(values);
      if(!render) {complete=false;break;}
      // Cor autoral por objeto: é o que separa piso, parede e referência humana
      // sem depender de nenhuma textura importada.
      render->material.enabled=true;
      std::copy(node.color,node.color+3,render->material.baseColor);
      render->material.roughness=.85f;
      render->material.metallic=0;
    }
    if(node.light!=SceneTemplateLight::None) {
      auto *light=static_cast<scene::Light*>(values.components.add(scene::Light::descriptor));
      if(!light) {complete=false;break;}
      light->kind=node.light==SceneTemplateLight::Directional?scene::LightKind::Directional:
                  node.light==SceneTemplateLight::Point?scene::LightKind::Point:scene::LightKind::Spot;
      // Lux para a direcional, lúmen para as locais: as unidades fotométricas
      // que o componente já consome.
      light->unit=node.light==SceneTemplateLight::Directional?scene::LightUnit::LuxCandela:scene::LightUnit::LuxLumen;
      light->intensity=node.lightIntensity;
      if(node.lightRange>0) light->range=node.lightRange;
      std::copy(node.color,node.color+3,light->color);
      if(node.shadows) light->shadowMode=2;
    }
    if(!history_.applyValues(document_,id,values)) {complete=false;break;}
    created[n]=id;
  }
  if(!complete) {
    // Modelo pela metade é pior do que modelo nenhum: o autor não sabe o que
    // faltou e a comparação nasce torta.
    history_.cancel(document_);
    state_.status="Não foi possível montar o modelo de cena";
    return false;
  }
  // Volumes de Ambiente do modelo: a exposição coerente com a luz fotométrica.
  for(const auto &volume:model.volumes) {
    if(!complete) break;
    const auto parent=volume.parent<0?group:
        (static_cast<usize>(volume.parent)<created.size()?created[static_cast<usize>(volume.parent)]:kInvalidEntity);
    const auto id=parent!=kInvalidEntity?history_.createEntity(document_,parent,EditorEntityKind::Folder,volume.name):kInvalidEntity;
    if(!id) {complete=false;break;}
    auto values=*document_.find(id);
    std::copy(volume.position,volume.position+3,values.transform.position);
    auto *environment=static_cast<scene::Environment*>(values.components.add(scene::Environment::descriptor));
    if(!environment) {complete=false;break;}
    environment->shape=volume.box?renderer::EnvironmentVolumeShape::Box:renderer::EnvironmentVolumeShape::Global;
    std::copy(volume.size,volume.size+3,environment->boxSize);
    environment->blendDistance=volume.blendDistance;
    environment->values.priority=volume.priority;
    environment->values.exposureEv=volume.exposureEv;
    environment->values.post=true;
    environment->values.indirectDiffuse=volume.indirectDiffuse;
    environment->values.indirectSpecular=volume.indirectSpecular;
    // A caixa do interior só troca a exposição: céu e neblina continuam os do
    // dia, que é o que se vê pela porta.
    if(volume.box) {environment->overrideSky=false;environment->overrideFog=false;}
    if(!history_.applyValues(document_,id,values)) complete=false;
  }
  if(!complete) {
    history_.cancel(document_);
    state_.status="Não foi possível montar o modelo de cena";
    return false;
  }
  // As vistas do modelo entram como vistas salvas da cena, no MESMO passo.
  auto views=document_.views();
  for(const auto &view:model.views) {
    runtime::SceneView saved;
    saved.name=view.name;
    std::copy(view.target,view.target+3,saved.target);
    saved.distance=view.distance;saved.yaw=view.yaw;saved.pitch=view.pitch;
    saved.verticalFov=projection_.verticalFieldOfViewRadians;
    views.replace(saved);
  }
  history_.setViews(document_,views);
  history_.end();
  setSelection(group);
  state_.creationMenu=false;
  // Abre na primeira vista DO MODELO, que pode não ser a primeira da cena
  // quando o autor já tinha vistas salvas.
  if(!model.views.empty())
    for(u32 v=0;v<document_.views().count();++v)
      if(document_.views().at(v)->name==model.views.front().name) {applySceneView(v);break;}
  state_.status=std::string(model.name)+" montado com "+std::to_string(model.views.size())+" vistas salvas";
  return true;
}

bool EditorSession::dropObjectOnField(u32 field, EditorEntityId dropped) {
  const u32 operation=field&0xff000000u,index=field&0xffu,property=(field>>8)&0xffffu;
  const auto *entity=document_.find(state_.selection);
  if(!entity || !document_.exists(dropped) || index>=entity->components.size()) return false;
  const auto *component=entity->components.at(index);
  EditorActionRequest request;request.version=sceneVersion();request.entity=entity->id;
  request.componentInstance=component->instanceId();
  if(operation==widgetId(EditorWidget::ComponentReferenceBase)) {
    if(property>=component->type().references.size()) return false;
    request.action=EditorAction::ComponentProperty;request.componentType=component->type().id;
    request.componentProperty=component->type().references[property].id;
    request.componentValue=scene::ObjectReference{dropped};
  } else if(operation==widgetId(EditorWidget::ScriptFieldBase)) {
    const auto *script=scene::scriptBehavior(component);
    if(!script) return false;
    const EditorScriptType *type=nullptr;
    for(const auto &candidate:code_.scriptTypes()) if(candidate.id==script->scriptType) type=&candidate;
    if(!type || property>=type->properties.size()) return false;
    const auto &declared=type->properties[property].valueType;
    if(declared!="object" && scene::scriptComponentTypeId(declared).empty()) {
      state_.status="Este campo não recebe objetos";return false;
    }
    request.action=EditorAction::ScriptProperty;request.componentProperty=type->properties[property].id;
    request.scriptPropertyType=declared;request.scriptPropertyValue=editorScriptReferenceValue(document_,declared,dropped);
    if(request.scriptPropertyValue.empty()) {
      const auto *schema=scene::findComponentSchema(scene::scriptComponentTypeId(declared));
      state_.status=std::string("O objeto solto não tem ")+(schema?schema->name:"o componente pedido");
      return false;
    }
  } else return false;
  if(dispatch(request).status==EditorActionStatus::Applied) return true;
  state_.status="O objeto solto não é compatível com este campo";
  return false;
}

bool EditorSession::dropAssetOnField(u32 field, u32 assetIndex) {
  // O botão de malha do componente Malha é o mesmo destino do seletor de malha.
  if(field==widgetId(EditorWidget::MeshChoose)) {
    if(assetIndex>=mapScene_.assetCount() || !document_.exists(state_.selection)) return false;
    EditorActionRequest request;request.version=sceneVersion();request.entity=state_.selection;
    request.action=EditorAction::AssignMesh;request.property=assetIndex+1;
    if(dispatch(request).status==EditorActionStatus::Applied) return true;
    state_.status="A malha solta não é compatível com este objeto";return false;
  }
  if((field&0xff000000u)!=widgetId(EditorWidget::ComponentResourceBase)) return false;
  const u32 index=field&0xffu,bindingIndex=(field>>8)&0xffu,slot=(field>>16)&0xffu;
  const auto *entity=document_.find(state_.selection);
  if(!entity || index>=entity->components.size() || assetIndex>=mapScene_.assetCount()) return false;
  const auto *component=entity->components.at(index);
  if(bindingIndex>=component->type().resourceBindings.size()) return false;
  EditorActionRequest request;request.version=sceneVersion();request.entity=entity->id;
  request.action=EditorAction::ComponentResource;request.componentInstance=component->instanceId();
  request.componentProperty=component->type().resourceBindings[bindingIndex].id;request.componentResourceSlot=slot;
  request.componentResource=mapScene_.assetGuid(assetIndex);
  if(dispatch(request).status==EditorActionStatus::Applied) return true;
  state_.status="O recurso solto não é do tipo deste campo";
  return false;
}

EditorEntityId EditorSession::createRecipe(u32 index, EditorEntityId parent) {
  if(index>=editorCreationCatalog.size() || !editorCreationCatalog[index].composed()) return kInvalidEntity;
  const auto &recipe=editorCreationCatalog[index];
  if(scene::validPrimitive(recipe.primitive)) {
    if(isPlaying() || history_.isOpen()) return kInvalidEntity;
    if(!document_.exists(parent)) parent=document_.root();
    for(u32 i=0;i<mapScene_.assetCount();++i) if(renderer::primitiveFromFlags(mapScene_.materialFlagsForAsset(i))==recipe.primitive) {
      const float position[3]{camera_.target[0],recipe.height,camera_.target[2]};
      EditorAssetInstantiation options;options.name=recipe.name;
      return instantiateAsset(i,parent,position,&options);
    }
    state_.status="A geometria desta primitiva não está na biblioteca";return kInvalidEntity;
  }
  const auto refuse=[&](const std::string &reason) {state_.status=reason;return kInvalidEntity;};
  if(isPlaying()||history_.isOpen()) return refuse("Criação exige Edit Mode sem edição em andamento");
  u32 childAsset=~0u;
  if(scene::validPrimitive(recipe.childVisual)) {
    for(u32 i=0;i<mapScene_.assetCount();++i) if(renderer::primitiveFromFlags(mapScene_.materialFlagsForAsset(i))==recipe.childVisual) {childAsset=i;break;}
    if(childAsset==~0u) return refuse("A geometria do filho visual não está na biblioteca");
  }
  const auto selected=state_.selection!=document_.root() && document_.exists(state_.selection)?state_.selection:kInvalidEntity;
  // A pose "atrás da seleção" é relativa ao alvo: nascer como filho dele faria
  // a câmera herdar o movimento que ela mesma deve acompanhar.
  if(recipe.pose==CreationPose::BehindSelection || !document_.exists(parent)) parent=document_.root();

  EditorEntity values;
  for(const auto &part:recipe.components) {
    auto plan=scene::planComponentAddition(values.components,part.type);
    if(!plan.ready) return refuse(std::string(recipe.name)+": "+(plan.error?plan.error:"composição inválida"));
    values.components=std::move(plan.candidate);
  }
  for(const auto &part:recipe.components) for(const auto &initial:part.values)
    if(scene::setComponentProperty(values.components,initial.component,initial.property,initial.value)!=
       scene::ComponentPropertyStatus::Applied)
      return refuse(std::string(recipe.name)+": valor inicial recusado ("+std::string(initial.property)+")");
  if(!recipe.pathPoints.empty()){
    auto*c=values.components.edit(scene::Path::descriptor);
    if(!c)return refuse("Receita de caminho sem componente Path");
    auto&path=*static_cast<scene::Path*>(c);
    for(const auto&point:recipe.pathPoints){u64 allocated=0;if(!path.insertPoint(0,point,allocated))return refuse("Receita contém pontos inválidos ou excede limite 128");}
    if(!path.valid())return refuse("Curva inicial inválida");
  }
  const bool selectedJointBody=recipe.selectionComponent!="astra.physics.joint" ||
    (document_.find(selected)&&runtime::physicsBody(*document_.find(selected)));
  if(!recipe.selectionComponent.empty() && selected!=kInvalidEntity && selectedJointBody &&
     scene::setComponentProperty(values.components,recipe.selectionComponent,recipe.selectionProperty,
                                 scene::ObjectReference{selected})!=scene::ComponentPropertyStatus::Applied)
    return refuse(std::string(recipe.name)+": o objeto selecionado não é um alvo aceito");

  constexpr float degrees=57.2957795f;
  auto &transform=values.transform;
  transform.position[0]=camera_.target[0];transform.position[1]=recipe.height;transform.position[2]=camera_.target[2];
  switch(recipe.pose) {
    case CreationPose::ViewTarget: break;
    case CreationPose::ViewTargetFacing:
      transform.rotationDegrees[0]=camera_.pitch*degrees;transform.rotationDegrees[1]=camera_.yaw*degrees;break;
    case CreationPose::ViewTargetTilted:
      transform.rotationDegrees[0]=45.f;transform.rotationDegrees[1]=camera_.yaw*degrees;break;
    case CreationPose::EditorCamera:
      editorCameraPosition(camera_,transform.position);
      transform.rotationDegrees[0]=camera_.pitch*degrees;transform.rotationDegrees[1]=camera_.yaw*degrees;break;
    case CreationPose::BehindSelection: {
      transform.rotationDegrees[0]=camera_.pitch*degrees;transform.rotationDegrees[1]=camera_.yaw*degrees;
      float targetWorld[16];
      if(selected==kInvalidEntity || !editorWorldMatrix(document_,selected,targetWorld)) {
        editorCameraPosition(camera_,transform.position);break;
      }
      // O deslocamento é o da própria referência, lido pelo contrato: a câmera
      // nasce onde o componente vai colocá-la no primeiro quadro de Play.
      float offset[3]{};
      if(const auto *source=values.components.find(recipe.selectionComponent))
        for(const auto &triple:source->type().triples) if(triple.id=="offset")
          for(u32 axis=0;axis<3;++axis) for(const auto &number:source->type().numbers)
            if(number.id==triple.channels[axis]) offset[axis]=number.read(*source);
      for(u32 axis=0;axis<3;++axis) transform.position[axis]=targetWorld[12+axis]+offset[axis];
      break;
    }
  }
  // Um corpo não pode herdar pose de corpo móvel ou de personagem: o solver
  // escreveria a pose do filho duas vezes.
  if(runtime::physicsBody(values) || runtime::characterComponent(values))
    for(auto ancestor=document_.find(parent);ancestor;ancestor=document_.find(ancestor->parent)) {
      const auto *body=runtime::physicsBody(*ancestor);
      if(runtime::characterComponent(*ancestor) || (body && body->motion!=scene::BodyMotion::Static))
        return refuse("Corpo ou personagem não pode herdar pose de corpo móvel ou personagem");
    }
  assignEntityName(values,recipe.name);
  float parentWorld[16],objectWorld[16];editorTransformMatrix(values.transform,objectWorld);
  if(recipe.anchor2DAtCreation) {
    auto *joint=static_cast<scene::Joint2D*>(values.components.edit(scene::Joint2D::descriptor));
    if(!joint || !joint->worldAnchor) return refuse("Receita de âncora 2D inválida");
    joint->anchorBX=objectWorld[12];joint->anchorBY=objectWorld[13];
    if(!joint->valid()) return refuse("Âncora fora dos limites suportados");
  }
  if(!editorWorldMatrix(document_,parent,parentWorld) ||
     !editorLocalTransformForWorld(objectWorld,parentWorld,values.transform))
    return refuse("Não foi possível criar objeto neste pai");
  if(!history_.begin(recipe.name)) return refuse("Finalize a edição atual antes de criar");
  const auto id=history_.createEntity(document_,parent,recipe.kind,recipe.name);
  if(!id || !history_.applyValues(document_,id,values)) {
    history_.cancel(document_);return refuse("Não foi possível criar objeto");
  }
  if(childAsset!=~0u) {
    EditorEntity visual;
    if(!runtime::configurePrimitive(visual,recipe.childVisual,{childAsset+1,mapScene_.assetGuid(childAsset),mapScene_.materialForAsset(childAsset)})) {
      history_.cancel(document_);return refuse("Não foi possível compor o filho visual");
    }
    while(visual.components.remove(scene::Collider::descriptor)){}
    while(visual.components.remove(scene::PhysicsBody::descriptor)){}
    const auto *character=runtime::characterComponent(values);
    const auto *motor=values.components.find(scene::DynamicBodyMotor::descriptor);
    const auto *collider=runtime::colliderComponent(values);
    if((!character&&(!motor||!collider||collider->shape!=scene::ColliderShape::Cylinder))||recipe.childVisual!=scene::PrimitiveType::Cylinder) {
      history_.cancel(document_);return refuse("Esta composição visual ainda não é suportada");
    }
    const float halfHeight=character?character->halfHeight+character->radius:collider->halfHeight;
    visual.transform.position[1]=character?halfHeight:0;
    visual.transform.scale[0]=visual.transform.scale[2]=(character?character->radius:collider->radius)*2;
    visual.transform.scale[1]=halfHeight;
    assignEntityName(visual,"Visual cilíndrico");
    const auto child=history_.createEntity(document_,id,EditorEntityKind::Mesh,"Visual cilíndrico");
    if(!child||!history_.applyValues(document_,child,visual)) {
      history_.cancel(document_);return refuse("Não foi possível criar o filho visual");
    }
  }
  // Cinemachine: a primeira câmera virtual põe um Cérebro na câmera principal.
  // Mesma transação: desfazer remove a câmera virtual e o Cérebro juntos.
  std::string brainNote;
  if(recipe.sceneBrain) {
    bool hasBrain=false;std::vector<EditorEntityId> all;document_.collectSubtree(document_.root(),all);
    for(const auto other:all) if(const auto *e=document_.find(other);e&&e->components.find(scene::CameraBrain::descriptor)) hasBrain=true;
    if(!hasBrain) {
      const auto camera=resolveSceneCamera(document_).entity;
      if(camera) {
        auto values=*document_.find(camera);
        auto plan=scene::planComponentAddition(values.components,scene::CameraBrain::descriptor.id);
        if(!plan.ready) brainNote=std::string(" · Cérebro não adicionado: ")+(plan.error?plan.error:"composição inválida");
        else {
          values.components=std::move(plan.candidate);
          if(!history_.applyValues(document_,camera,values)) {history_.cancel(document_);return refuse("Não foi possível pôr o Cérebro na câmera");}
          brainNote=std::string(" · Cérebro em ")+document_.find(camera)->name;
        }
      } else {
        EditorEntity main;
        for(const auto type:{scene::Camera::descriptor.id,scene::CameraBrain::descriptor.id}) {
          auto plan=scene::planComponentAddition(main.components,type);
          if(!plan.ready) {history_.cancel(document_);return refuse("Não foi possível compor a câmera principal");}
          main.components=std::move(plan.candidate);
        }
        editorCameraPosition(camera_,main.transform.position);
        main.transform.rotationDegrees[0]=camera_.pitch*degrees;main.transform.rotationDegrees[1]=camera_.yaw*degrees;
        assignEntityName(main,"Câmera principal");
        const auto created=history_.createEntity(document_,document_.root(),runtime::ObjectKind::Camera,"Câmera principal");
        if(!created||!history_.applyValues(document_,created,main)) {history_.cancel(document_);return refuse("Não foi possível criar a câmera principal");}
        brainNote=" · Câmera principal com Cérebro criada";
      }
    }
  }
  history_.end();setSelection(id);
  if(!recipe.authoringComponent.empty()) {
    if(const auto *component=document_.find(id)->components.find(recipe.authoringComponent)) {
      state_.componentSelection=id;state_.expandedNative=component->instanceId();
      state_.expandedComponent.clear();state_.componentGroup.clear();state_.propertyPage=0;
      state_.inspectorSurface=EditorInspectorSurface::Inspection;
      state_.compactPanel=EditorScreenState::CompactPanel::Inspector;
    }
  }
  if(!recipe.pathPoints.empty()){
    refreshPathEditorState();state_.pathEditorOpen=true;state_.pathPointList=false;
  }
  state_.status=std::string("Objeto criado: ")+recipe.name+brainNote;
  return id;
}

EditorEntityId EditorSession::createWaterSurface(bool cameraRelative, EditorEntityId parent) {
  if(parent==kInvalidEntity) parent=document_.root();
  for(u32 i=0;i<mapScene_.assetCount();++i) {
    const auto flags=mapScene_.materialFlagsForAsset(i);
    if(!(flags & renderer::WaterAuthoringResource) || !(flags & renderer::MapMaterialWater) || (flags & renderer::WaterRouteResource) ||
       bool(flags & renderer::MapMaterialWaterCameraGrid)!=cameraRelative) continue;
    const float position[3]{camera_.target[0],0,camera_.target[2]};
    const auto id=instantiateAsset(i,parent,position);
    if(id) {state_.workspace=EditorWorkspace::Scene;state_.status="Água criada; ondas e óptica em Configurações";
      if(cameraRelative) {std::copy(position,position+3,camera_.target);camera_.distance=40;} else frameSelection();}
    return id;
  }
  state_.status="Gerador de agua indisponivel nesta biblioteca";
  return kInvalidEntity;
}

void EditorSession::frameAll() { frameSubtree(document_.root()); }

void EditorSession::frameSubtree(EditorEntityId root) {
  finishCameraGesture(false);state_.cameraViewEntity=0;state_.cameraPiloting=false;
  buildPickCandidates();
  float low[3]{}, high[3]{};
  bool first = true;
  for (const auto &candidate : candidates_) {
    if (!candidate.selectable || !document_.isDescendantOf(candidate.id,root)) continue;
    for (u32 axis = 0; axis < 3; ++axis) {
      const float a = candidate.center[axis] - candidate.radius;
      const float b = candidate.center[axis] + candidate.radius;
      low[axis] = first ? a : std::min(low[axis], a);
      high[axis] = first ? b : std::max(high[axis], b);
    }
    first = false;
  }
  // Logical components have editor presence without fabricated pick geometry.
  // Frame their origins too; a camera's million-unit far plane must not force
  // an unusable zoom when the user asks to focus its object.
  std::vector<EditorEntityId> ids;document_.collectSubtree(root,ids);
  for(auto id:ids) {
    const auto *entity=document_.find(id);
    if(!entity || id==document_.root() || meshAsset(*entity)) continue;
    if(!cameraComponent(*entity) && !runtime::lightComponent(*entity) && !colliderComponent(*entity)) continue;
    bool visible=true;
    for(auto p=entity;p;p=document_.find(p->parent)) if(!p->visible) {visible=false;break;}
    float world[16];if(!visible || !editorWorldMatrix(document_,id,world)) continue;
    for(u32 axis=0;axis<3;++axis) {
      low[axis]=first?world[12+axis]-.5f:std::min(low[axis],world[12+axis]-.5f);
      high[axis]=first?world[12+axis]+.5f:std::max(high[axis],world[12+axis]+.5f);
    }
    first=false;
  }
  if (first) {
    if(root==document_.root()) camera_=EditorCamera{};
    else {
      float world[16];
      if(editorWorldMatrix(document_,root,world)) {
        for(u32 axis=0;axis<3;++axis) {low[axis]=world[12+axis]-.5f;high[axis]=world[12+axis]+.5f;}
        first=false;
      }
    }
    if(first) return;
  }
  float center[3]{}, radiusSquared = 0;
  for (u32 axis = 0; axis < 3; ++axis) {
    center[axis] = (low[axis] + high[axis]) * 0.5f;
    radiusSquared += (high[axis]-center[axis])*(high[axis]-center[axis]);
  }
  renderer::PerspectiveVisibilitySettings fit=projection_;
  if(layout_.viewport.height>0 && layout_.viewport.width<layout_.viewport.height)
    fit.verticalFieldOfViewRadians=2*std::atan(std::tan(fit.verticalFieldOfViewRadians*0.5f)*
      layout_.viewport.width/layout_.viewport.height);
  frameEditorCamera(camera_, center, std::max(0.01f, std::sqrt(radiusSquared)), fit);
}

void EditorSession::refreshLodStatus() {
  state_.lodStatus.clear();state_.lodViewPercent=-1;
  const auto &graph=isPlaying()&&playScene_.active()?playScene_.document():document_;
  float relative=0;u32 level=0;
  if(!runtime::lodGroupViewLevel(graph,state_.selection,lodView(),relative,level)) return;
  const auto *group=static_cast<const scene::LodGroup *>(graph.find(state_.selection)->components.find(scene::LodGroup::descriptor));
  const std::string height=std::isinf(relative)?std::string("dentro do grupo"):
      std::to_string(static_cast<int>(std::lround(std::min(relative,99.99f)*100)))+"% da tela";
  state_.lodStatus="Na vista: "+(level<group->levelCount?"LOD "+std::to_string(level):std::string("Culled"))+" · "+height;
  state_.lodViewPercent=std::isinf(relative)?100.f:std::min(relative,1.f)*100.f;
}
void EditorSession::refreshSkinningStatus() {
  state_.skinStatus.clear();state_.animationStatus.clear();state_.blendShapeNames.clear();
  const bool playing=isPlaying()&&playScene_.active();
  const auto &graph=playing?playScene_.document():document_;
  const auto *entity=graph.find(state_.selection);
  if(!entity) return;
  if(const auto *skinned=static_cast<const scene::SkinnedMesh *>(entity->components.find(scene::SkinnedMesh::descriptor))) {
    const auto *render=meshRenderer(*entity);
    const u32 mesh=render?render->slotMesh(0):0;
    const auto *deform=mesh?mapScene_.deformation(mesh-1):nullptr;
    if(!deform) state_.skinStatus="A malha deste objeto não tem skin nem blend shapes na fonte: desenho estático";
    else {
      std::string text;
      float model[16];
      EditorMapScene::DeformedPose pose;
      const bool posed=editorWorldMatrix(graph,entity->id,model) && mapScene_.deformedPose(graph,*skinned,mesh-1,model,pose);
      if(deform->skin)
        text=std::to_string(deform->skin->joints.size())+" juntas · "+
             (pose.missingBones?std::to_string(pose.missingBones)+" sem osso (pose de bind)":std::string("todos os ossos ligados"));
      if(deform->morph) {
        if(!text.empty()) text+=" · ";
        text+=std::to_string(deform->morph->targetCount)+" blend shapes";
        state_.blendShapeNames=deform->morph->names;
        // Nomes dos alvos na ordem dos slots, para quem edita o peso saber qual é qual.
        if(!deform->morph->names.empty()) {
          text+=":";
          for(u32 t=0;t<deform->morph->names.size() && t<6;++t)
            text+=std::string(t?", ":" ")+(deform->morph->names[t].empty()?"#"+std::to_string(t):deform->morph->names[t]);
          if(deform->morph->names.size()>6) text+=", …";
        }
      }
      if(!posed) text+=" · pose recusada: osso com escala nula";
      state_.skinStatus=std::move(text);
    }
  }
  if(const auto *animation=static_cast<const scene::Animation *>(entity->components.find(scene::Animation::descriptor))) {
    runtime::AnimationClipView view;
    u32 missing=0;
    for(const auto &entry:animation->clips) if(entry.asset.valid() && !mapScene_.findClip(entry.asset,view)) ++missing;
    std::string text;
    if(animation->clips.empty() && !animation->clip.valid()) text="Sem clipes: escolha na lista Clipes";
    else if(!animation->clip.valid()) text=std::to_string(animation->clips.size())+" clipe(s) · sem clipe padrão";
    else if(!mapScene_.findClip(animation->clip,view)) text="Clipe padrão ausente: a fonte dele não está carregada";
    else {
      char duration[32];std::snprintf(duration,sizeof duration,"%.2f s",view.clip->duration);
      text="Padrão: "+view.name+" · "+duration+" · "+std::to_string(animation->clips.size())+" clipe(s)";
    }
    if(missing) text+=" · "+std::to_string(missing)+" ausente(s)";
    if(playing) {
      // O que o avaliador está tocando agora, com peso e tempo.
      const auto &animator=playScene_.animator();
      std::string states;
      for(const auto &entry:animation->clips) {
        runtime::AnimationStateView state;
        if(animator.state(entity->id,animation->instanceId(),entry.asset,state)!=runtime::AnimationCommandStatus::Ok || !state.enabled)
          continue;
        char line[64];
        std::snprintf(line,sizeof line," %.2f s ×%.2f",state.time,state.weight);
        states+=(states.empty()?std::string():std::string(" · "))+(mapScene_.findClip(entry.asset,view)?view.name:std::string("?"))+line;
      }
      text+=states.empty()?" · parado":" · tocando "+states;
    }
    state_.animationStatus=std::move(text);
  }
}
void EditorSession::update() {
  refreshMotorDecomposition();
  if(inputCapture_ && !validateInputCapture()) cancelInputBindingCapture();
  refreshScriptInspection();
  state_.document=playInspecting()?&playScene_.document():&document_;
  state_.characterRuntime=playInspecting()&&playScene_.active()?&playScene_.physics():nullptr;
  state_.characterWorld=playInspecting()&&playScene_.active()?&playScene_.world():nullptr;
  state_.tweenRuntime=playInspecting()&&playScene_.active()?&playScene_.tweens():nullptr;
  state_.tweenSequenceRuntime=playInspecting()&&playScene_.active()?&playScene_.tweenSequences():nullptr;
  state_.physicsQueryRuntime=playInspecting()&&playScene_.active()?&playScene_.physicsQueries():nullptr;
  state_.virtualCameraRuntime=playInspecting()&&playScene_.active()?&playScene_.virtualCameras():nullptr;
  state_.audioRuntime=playInspecting()&&playScene_.active()?&playScene_.audio():nullptr;
  state_.animatorRuntime=playInspecting()&&playScene_.active()?&playScene_.animatorGraphs():nullptr;
  state_.animatorControllers=&animatorControllers_;
  state_.animatorControllerDiagnostic.clear();state_.animatorResolved=nullptr;
  if(state_.animatorOpen) {
    state_.animatorResolved=openAnimator();
    const auto *owner=state_.document->find(state_.animatorEntity);
    const auto *component=owner?owner->components.findInstance(state_.animatorInstance):nullptr;
    if(component&&&component->type()==&scene::Animator::descriptor&&scene::animator(*component).controller.valid()) {
      if(state_.animatorRuntime) {
        if(const auto *live=state_.animatorRuntime->find(state_.animatorEntity,state_.animatorInstance)) state_.animatorControllerDiagnostic=live->controllerDiagnostic;
      } else if(!animatorResourceDrag_) {
        scene::Animator check;resources::resolveAnimatorController(scene::animator(*component),animatorControllers_,check,state_.animatorControllerDiagnostic);
      }
    }
  }
  state_.timerRuntime=playInspecting()&&playScene_.active()?&playScene_.timers():nullptr;
  state_.uiTime=clockPrimed_?lastWallSeconds_:0;
  refreshColliderAuthoring();
  // Travado num objeto que deixou de existir (apagado, outra cena): volta a seguir a seleção.
  if(state_.inspectorLocked && !state_.document->exists(state_.inspectorLocked)) state_.inspectorLocked=0;
  // Objeto de um focado apagado: a aba fecha (depois que a cena foi conferida).
  if(focusedValidated_ && !isPlaying())
    for(const auto &focused:state_.focusedInspectors) if(focused.kind==EditorScreenState::FocusedAsset::None && !document_.exists(focused.entity)) {validateFocusedInspectors();break;}
  // O campo acabou de abrir: o texto é o valor atual formatado, a base do
  // `+=` e da prévia. Fechar e abrir outro recomeça no teclado numérico.
  if(state_.numericField!=numericFieldSeen_) {
    numericFieldSeen_=state_.numericField;state_.numericExpression=false;
    if((state_.numericField&0xff000000u)!=widgetId(EditorWidget::ComponentSlotNumberBase))numericPathPointId_=0;
    std::string text=state_.numericText;
    if(text.find('.')==std::string::npos) std::replace(text.begin(),text.end(),',','.');
    char *stop=nullptr;const double value=std::strtod(text.c_str(),&stop);
    state_.numericCurrent=stop&&stop!=text.c_str()&&std::isfinite(value)?value:0;
  }
  state_.cameraPreviewEntity=cameraPreview_.camera();
  state_.cameraPreviewReady=cameraPreview_.hasCurrentImage(sceneVersion());
  state_.cameraPreviewFailed=cameraPreview_.failed();
  state_.cameraPreviewDiagnostic=cameraPreview_.diagnostic();
  state_.cameraPreviewWidth=cameraPreview_.width();state_.cameraPreviewHeight=cameraPreview_.height();
  state_.cameraPreviewFrequency=cameraPreview_.frequency();
  // As leituras que o Inspector mostra vêm do objeto que ELE mostra (travado
  // ou a seleção), não necessariamente do que está selecionado na cena.
  inInspectorScope(state_.inspectorLocked,[&] {
    refreshPathEditorState();
    refreshImportLinkView();
    refreshMaterialSlotView();
    refreshLodStatus();
    refreshSkinningStatus();
    state_.constraintStatus.clear();state_.constraintStatusWarning=false;
    state_.audioStatus.clear();state_.audioStatusWarning=false;
    const auto &graph=isPlaying()&&playScene_.active()?playScene_.document():document_;
    const auto inspected=state_.inspectorTarget?state_.inspectorTarget:state_.selection;
    const auto *object=graph.find(inspected);
    if(object)for(usize i=0;i<object->components.size();++i){
      const auto *c=object->components.at(i);
      if(c->type().id=="astra.audio.source"){
        state_.audioStatus="Pedido salvo · estado real em Play";
        if(isPlaying()&&playScene_.active()){
          const auto *diagnostic=playScene_.audio().diagnostic(object->id,c->instanceId());
          state_.audioStatus=diagnostic?diagnostic->message:"Aguardando avaliação de áudio";
          state_.audioStatusWarning=diagnostic&&diagnostic->state>=runtime::SceneAudio::State::MissingClip;
        }
      }
    }
    bool hasConstraint=false,enabled=false;
    if(object) for(usize i=0;i<object->components.size();++i) {
      const auto *value=object->components.at(i);
      if(!value->type().id.starts_with("astra.constraint.")&&!value->type().id.starts_with("astra.spring.")) continue;
      hasConstraint=true;
      for(const auto &p:value->type().booleans) if(p.id=="enabled") enabled|=p.read(*value);
    }
    if(hasConstraint) {
      state_.constraintStatus=isPlaying()?(enabled?"Avaliação em Play · após animação e física":"Constraints desativadas"):
        "Autoria · a fonte será avaliada em Play";
      if(isPlaying()&&playScene_.active()) for(const auto &diagnostic:playScene_.constraints().diagnostics()) {
        if(diagnostic.object!=inspected) continue;
        if(!state_.constraintStatusWarning) state_.constraintStatus.clear();
        else state_.constraintStatus+=" · ";
        state_.constraintStatus+=runtime::SceneConstraints::issueText(diagnostic.issue);
        state_.constraintStatusWarning=true;
      }
    }
    return true;
  });
  // R4: miniaturas nascem uma por atualização enquanto o seletor está aberto.
  if(state_.texturePicker || (state_.textureManager && !state_.textureManagerSources)) generatePendingTextureThumbnail();
  if(state_.textureManager && state_.textureManagerSources) generatePendingSourceThumbnail();
  // S4: contagem para a linha do painel Qualidade e a página do explorador.
  if(state_.qualityPanel || state_.lightExplorer) refreshLightExplorer();
  if(const auto *selected=document_.find(state_.selection)) state_.routePoint=waterRoute(*selected).count?std::min(state_.routePoint,waterRoute(*selected).count-1):0;
  if (font_ == nullptr || icons_ == nullptr) return;
  state_.assetCount=mapScene_.assetCount();
  refreshGuiTree();
  state_.canUndo = history_.canUndo();
  state_.canRedo = history_.canRedo();
  // Conjunto coerente com o ativo, que muitas rotinas ainda escrevem direto:
  // apagados saem; um ativo novo fora do conjunto vira seleção única.
  {
    auto &set=state_.selectionSet;
    // Em Inspect, objetos criados por scripts só existem no mundo de execução.
    const auto &visible=state_.document?*state_.document:document_;
    set.erase(std::remove_if(set.begin(),set.end(),[&](EditorEntityId id){return !visible.exists(id);}),set.end());
    if(!visible.exists(state_.selection)) {state_.selection=set.empty()?kInvalidEntity:set.back();}
    if(state_.selection==kInvalidEntity) set.clear();
    else if(!state_.isSelected(state_.selection)) set={state_.selection};
  }
  if(state_.sceneVisibilityChanged) {
    state_.sceneVisibilityChanged=false;appearanceChanged_=true;saveEditorPreferences();
  }
  refreshMultiEdit();
  refreshMultiAsset();
  refreshFocusedAsset();
  if(state_.environmentInspector.valid()) refreshEnvironmentInspector();
  if(state_.profileInspector.valid()) refreshProfileInspector();
  state_.backgroundTasks=shellTasks_;
  if(state_.codeBuildBusy) state_.backgroundTasks.push_back({EditorBackgroundTask::Kind::CodeBuild,"Compilando scripts","C# do projeto",-1,false});
  if(state_.globalSearch) {
    state_.globalResults=editorGlobalSearch(document_,searchFiles_,state_.globalQuery,state_.globalProvider,
        [&](u32 i){return creationAvailable(state_,i) && !isPlaying();},state_.globalCounts);
  }
  if(state_.undoHistory) {
    if(isPlaying()) state_.undoHistory=false;
    state_.undoEntries.resize(history_.entryCount());
    for(u32 i=0;i<state_.undoEntries.size();++i) state_.undoEntries[i]=history_.describe(i);
    state_.undoApplied=history_.undoDepth();
  }
  u32 pressed = 0;
  state_.pressedWidget = router_.pressedWidget(pressed) ? pressed : 0;

  const UiFontMetrics metrics = font_->metrics(UiFontWeight::Regular);

  // Duas passagens, e é o preço de fazer o certo. A grade e o gizmo precisam da
  // projeção, a projeção precisa do retângulo da cena, e o retângulo da cena só
  // existe depois que os painéis tomaram a largura deles. Usar o retângulo do
  // frame anterior custaria um quadro de atraso justamente quando o usuário
  // está arrastando um divisor — o momento em que ele mais olha para a borda.
  list_.begin(state_.surface, metrics);
  router_.beginFrame();
  state_.colliderHandleOccluders={};
  const bool measureColliderUi=state_.colliderTopology||state_.physicsDiagnosticOpen;
  const auto colliderUiStart=measureColliderUi?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
  layout_ = buildEditorScreen(state_, state_.workspace==EditorWorkspace::Code?editorCodeTheme():editorTheme(), list_, router_);
  // The first pass measures current content. Clamp before the final paint,
  // including after resize or changing to shorter contextual content.
  if(state_.animatorOpen&&state_.animatorDrawer>=2)
    state_.animatorDetailsScroll=std::clamp(state_.animatorDetailsScroll,0.f,std::max(0.f,layout_.animatorDetailsExtent-layout_.animatorDetailsWindow.height));
  // A rolagem persegue o cursor a cada quadro enquanto o codigo esta aberto: a
  // altura util so encolhe quando o teclado termina de subir, um ou dois
  // quadros depois do toque, e a contagem de linhas visiveis daquele instante e
  // a unica que vale.
  if(state_.editingCode && state_.platformTextInput && !state_.platformCodeView) followCodeCaret();
  const u32 maximumScroll=layout_.hierarchyRowCount>layout_.hierarchyVisibleRows
      ? layout_.hierarchyRowCount-layout_.hierarchyVisibleRows : 0;
  state_.hierarchyScroll=std::min(state_.hierarchyScroll,maximumScroll);


  // Identidade, e não a pré-rotação do display: o retângulo da vista já está no
  // espaço lógico em paisagem, e o shader da interface roda tudo uma vez no fim.
  view_ = buildEditorViewport(camera_, layout_.viewport, {}, projection_);
  if(state_.cameraViewEntity && state_.workspace==EditorWorkspace::Scene) {
    const auto pose=resolveSceneCamera(document_,state_.cameraViewEntity,true);
    if(pose.entity && !layout_.viewport.isEmpty()) {
      auto projection=projection_;projection.nearPlane=pose.nearPlane;projection.farPlane=pose.farPlane;
      projection.verticalFieldOfViewRadians=pose.verticalFov*.017453292519943295f;projection.roll=pose.roll;
      projection.projection=pose.projection==scene::CameraProjection::Orthographic?renderer::CameraProjection::Orthographic:renderer::CameraProjection::Perspective;
      projection.orthographicHalfHeight=pose.orthographicHalfHeight;
      view_.frustum=renderer::buildPerspectiveFrustum(pose.position,pose.yaw,pose.pitch,layout_.viewport.width/layout_.viewport.height,projection);
    } else {finishCameraGesture(true);state_.cameraViewEntity=0;state_.cameraPiloting=false;}
  }
  // Inspection diagnostics must use precisely the camera used to render Play,
  // including authored projection, roll and the runtime camera hierarchy.
  if(isPlaying() && state_.playInspect && state_.physicsDiagnosticOpen)
    view_=guiPlayView();
  state_.view = &view_;
  if(isPlaying() && playScene_.active()) {
    state_.debugLines=&playScene_.debugLines();
    state_.debugView=view_;state_.debugView.frustum=gameView().frustum;
  } else state_.debugLines=nullptr;
  // Only an explicitly focused Collider with an authoring tool needs these
  // depth candidates. BVHs remain cached and resolved lazily along handle rays.
  if(!isPlaying() && state_.workspace==EditorWorkspace::Scene && state_.showComponentVisuals &&
     (state_.tool!=EditorGizmoMode::Select||colliderTopology_.active()) && !state_.cameraViewEntity &&
     inspectedCollider(document_,state_.selection,state_.expandedNative)) {
    buildPickCandidates(nullptr,true);state_.colliderHandleOccluders=candidates_;
  }

  list_.begin(state_.surface, metrics);
  router_.beginFrame();
  layout_ = buildEditorScreen(state_, state_.workspace==EditorWorkspace::Code?editorCodeTheme():editorTheme(), list_, router_);
  // A rolagem persegue o cursor a cada quadro enquanto o codigo esta aberto: a
  // altura util so encolhe quando o teclado termina de subir, um ou dois
  // quadros depois do toque, e a contagem de linhas visiveis daquele instante e
  // a unica que vale.
  if(state_.editingCode && state_.platformTextInput && !state_.platformCodeView) followCodeCaret();

  refreshGuiImages();
  if(measureColliderUi)state_.colliderUiMs=ColliderTopology::ms(colliderUiStart);
  if(state_.guiInspector && !state_.workspaceMenu) {
    const auto *owner=document_.find(state_.guiSelection.owner);u32 shared=0;
    for(const auto &row:state_.guiRows)if(!row.target.node&&row.target.document==state_.guiSelection.document)++shared;
    auto area=layout_.inspectorPanel;
    area.height=std::max(0.f,area.height-state_.surface.height*state_.platformImeFraction);
    if(owner && gui_.drawInspector(area,state_.surface,list_,owner->name,shared,guiDeltaSeconds_)) {
      const auto component=state_.guiSelection.component;setSelection(owner->id);state_.expandedNative=component;
    } else state_.guiSelection.node=gui_.selected();
  }
  drawGuiAuthoring(metrics);
  if(state_.workspace==EditorWorkspace::Gui && !state_.workspaceMenu)
  {
    auto area=layout_.viewport;
    area.height=std::max(0.0f,area.height-state_.surface.height*state_.platformImeFraction);
    gui_.draw(area,state_.surface,list_,guiDeltaSeconds_);
  }
  if(isPlaying() && playScene_.active() && !state_.playInspect && !state_.workspaceMenu) {
    auto &sceneGui=playScene_.sceneGui();sceneGui.prepare(playScene_.world(),guiPlayView().frustum,layout_.viewport,state_.surface);
    sceneGui.draw(list_,metrics);
    auto &runtime=playScene_.gui();const auto &canvas=runtime.document().canvas();
    if(!sceneGui.diagnostic().empty())state_.status=sceneGui.diagnostic();
    if(sceneGui.instances().empty()&&sceneGui.diagnostic().empty()) {
    if(canvas.mode==ui::GuiCanvasMode::World) {
      if(guiWorld_.configure(canvas,guiPlayView().frustum,layout_.viewport,state_.surface)) {
        runtime.layout({0,0,canvas.resolution.x,canvas.resolution.y});
        guiWorldDrawing_.begin({0,0,canvas.resolution.x,canvas.resolution.y},metrics);
        runtime.draw(guiWorldDrawing_);
        guiWorldDrawing_.projectRange(0,guiWorld_.projection(),canvas.occlusion,layout_.viewport);
        list_.append(guiWorldDrawing_);
      }
    } else {runtime.layout(layout_.viewport);runtime.draw(list_);}
    }
  }
  instances_.clear();
  buildUiInstances(list_, *font_, *icons_, kMaximumInstances, instances_);
}

void EditorSession::drawGuiAuthoring(const ui::UiFontMetrics &metrics) {
  guiAuthorPrepared_=false;
  if(!state_.guiInspector || state_.workspaceMenu || layout_.viewport.isEmpty())return;
  const auto *owner=document_.find(state_.guiSelection.owner);
  const auto *value=owner?owner->components.findInstance(state_.guiSelection.component):nullptr;
  if(!value || value->type().id!=scene::UiCanvas::descriptor.id)return;
  const auto &canvas=static_cast<const scene::UiCanvas&>(*value);
  if(!canvas.enabled || !canvas.valid())return;
  for(const auto *object=owner;object;object=document_.find(object->parent))if(!object->active||!object->visible)return;
  auto target=state_.guiSelection;target.node=0;
  if(guiAuthorRevision_!=gui_.document().revision() || guiAuthorGraphRevision_!=document_.revision() || guiAuthorTarget_!=target) {
    guiAuthorView_.load(gui_.document(),false);std::string error;
    if(!guiAuthorView_.document().setCanvas(runtime::uiCanvasPresentation(canvas),error)){state_.status=error;return;}
    guiAuthorRevision_=gui_.document().revision();guiAuthorGraphRevision_=document_.revision();guiAuthorTarget_=target;
  }
  guiAuthorView_.setImages(&guiImages_);const auto &c=guiAuthorView_.document().canvas();
  auto outline=[&](ui::UiDrawList &drawing) {
    if(const auto *p=guiAuthorView_.placement(gui_.selected())) {
      const auto r=p->bounds;const auto color=editorTheme().color.accent;
      drawing.addLine({r.x,r.y},{r.right(),r.y},color,2);drawing.addLine({r.right(),r.y},{r.right(),r.bottom()},color,2);
      drawing.addLine({r.right(),r.bottom()},{r.x,r.bottom()},color,2);drawing.addLine({r.x,r.bottom()},{r.x,r.y},color,2);
    }
  };
  if(c.mode==ui::GuiCanvasMode::World) {
    float host[16];if(!runtime::worldMatrix(document_,owner->id,host)||!guiWorld_.configure(c,view_.frustum,layout_.viewport,state_.surface,host))return;
    guiAuthorView_.layout({0,0,c.resolution.x,c.resolution.y});guiWorldDrawing_.begin({0,0,c.resolution.x,c.resolution.y},metrics);
    guiAuthorView_.draw(guiWorldDrawing_);outline(guiWorldDrawing_);
    guiWorldDrawing_.projectRange(0,guiWorld_.projection(),c.occlusion,layout_.viewport);list_.append(guiWorldDrawing_);
  } else {
    guiAuthorView_.layout(layout_.viewport);list_.pushClip(layout_.viewport);guiAuthorView_.draw(list_);outline(list_);list_.popClip();
  }
  guiAuthorPrepared_=true;
}
bool EditorSession::guiAuthorPointer(const ui::UiPointerEvent &event) {
  auto captured=std::find_if(guiAuthorPointers_.begin(),guiAuthorPointers_.end(),[&](const auto &p){return p.id==event.pointerId&&p.device==event.device;});
  if(captured!=guiAuthorPointers_.end()) {
    if(event.phase==UiPointerPhase::Up||event.phase==UiPointerPhase::Cancel)guiAuthorPointers_.erase(captured);
    return true;
  }
  if(event.phase!=UiPointerPhase::Down || !guiAuthorPrepared_ || !state_.guiInspector || state_.workspaceMenu ||
     state_.workspace!=EditorWorkspace::Scene || !layout_.viewport.contains(event.position) ||
     router_.hitTest(event.position).target==UiPointerTarget::Widget)return false;
  auto point=event.position;float distance=0;
  if(guiAuthorView_.document().canvas().mode==ui::GuiCanvasMode::World && !guiWorld_.map(point,point,distance))return false;
  if(guiAuthorView_.document().canvas().mode==ui::GuiCanvasMode::World && guiAuthorView_.document().canvas().occlusion) {
    buildPickCandidates();const auto scene=pickNearest(candidates_,screenPointToRay(view_,event.position));
    if(scene.hit&&scene.distance<distance-.0001f)return false;
  }
  const auto node=guiAuthorView_.hit(point,false);if(!node||guiAuthorPointers_.size()>=32)return false;
  gui_.select(node);state_.guiSelection.node=node;guiAuthorPointers_.push_back({event.pointerId,event.device});return true;
}
bool EditorSession::selectGuiElement(const EditorGuiTarget &target) {
  if(isPlaying() || history_.isOpen() || !target.valid())return false;
  const auto *owner=document_.find(target.owner);const auto *value=owner?owner->components.findInstance(target.component):nullptr;
  if(!value || value->type().id!=scene::UiCanvas::descriptor.id || static_cast<const scene::UiCanvas&>(*value).document!=target.document)return false;
  const auto *record=assets_.find(target.document);
  gui_.history().commit(gui_.document());
  if(!record || record->type!=resources::AssetType::UiDocument || !gui_.openResource(record->path)) {
    state_.status="Documento UI ausente ou documento atual ainda nao salvo";return false;
  }
  if(target.node && !gui_.document().find(target.node))return false;
  setSelection(target.owner);gui_.setPreview(false);gui_.select(target.node);
  state_.guiSelection=target;state_.guiInspector=true;state_.inspectorLocked=0;
  state_.expandedNative=target.component;
  state_.workspace=EditorWorkspace::Scene;state_.workspaceMenu=false;
  state_.inspectorVisible=true;state_.compactPanel=EditorScreenState::CompactPanel::Inspector;
  state_.textureInspector=false;state_.textureManager=false;state_.importPanel=false;
  state_.status="Edicao da fonte UI compartilhada: "+record->path;return true;
}
void EditorSession::refreshGuiTree() {
  if(state_.workspace!=EditorWorkspace::Scene){state_.guiRows={};state_.guiInspector=false;return;}
  if(state_.guiSelection.valid()) {
    const auto &target=state_.guiSelection;const auto *owner=document_.find(target.owner);
    const auto *value=owner?owner->components.findInstance(target.component):nullptr;
    const auto *record=assets_.find(target.document);
    if(state_.selection!=target.owner || !value || value->type().id!=scene::UiCanvas::descriptor.id ||
       static_cast<const scene::UiCanvas&>(*value).document!=target.document || !record || record->path!=gui_.resource()) {
      gui_.cancelPointers();state_.guiSelection={};state_.guiInspector=false;
    } else {
      gui_.select(gui_.selected());state_.guiSelection.node=gui_.selected();state_.guiInspector=true;
    }
  }
  guiTree_.rebuild(document_,assets_,[this](resources::AssetGuid id,ui::GuiDocument &doc,std::string &error){return loadGuiDocument(id,doc,error);},gui_.resource(),gui_.document());
  state_.guiRows=guiTree_.rows();
  if(!guiTree_.diagnostic().empty())state_.status=guiTree_.diagnostic();
}
bool EditorSession::loadGuiDocument(resources::AssetGuid asset,ui::GuiDocument &document,std::string &error) const {
  const auto *record=assets_.find(asset);std::filesystem::path path;
  if(!record || record->type!=resources::AssetType::UiDocument || !EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),record->path,path) || path.extension()!=".aeui") {error="Referência de documento UI ausente ou inválida";return false;}
  // Play instantiates the authored draft, just like scene properties. It never
  // writes the source, and each runtime instance still owns an independent copy.
  if(record->path==gui_.resource()){document=gui_.document();return document.validate(error);}
  std::vector<u8> bytes;
  if(!EditorImportTransaction::read(path,bytes,8u*1024u*1024u)){error="Documento UI ausente ou maior que 8 MiB: "+record->path;return false;}
  std::istringstream input(std::string(bytes.begin(),bytes.end()));return document.read(input,error);
}
bool EditorSession::publishGuiDocument(std::string_view relative,const std::string &text,std::string &error) {
  auto next=assets_;const auto *known=assets_.findByPath(relative);
  if(known&&known->type!=resources::AssetType::UiDocument){error="Caminho pertence a outro tipo de recurso";return false;}
  resources::AssetRecord record;record.guid=known?known->guid:resources::assetGuidFromSeed("ui:"+std::string(relative)+":"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+":"+std::to_string(++importInstanceCounter_));
  record.type=resources::AssetType::UiDocument;record.path=std::string(relative);record.importerVersion=1;
  const std::span<const u8> bytes{reinterpret_cast<const u8*>(text.data()),text.size()};record.contentHash=Sha256::hex(bytes);
  if(!(known?next.publishImport(record.guid,record.contentHash,1,"AEUI",{},{}):next.add(record))){error="Registro recusou documento UI";return false;}
  EditorImportTransaction transaction(files_.rootPath());
  std::filesystem::path source;std::error_code ec;std::string expectedHash;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),record.path,source)) {error="Documento UI fora do projeto";return false;}
  if(std::filesystem::exists(source,ec)) {
    std::vector<u8> current;
    if(!EditorImportTransaction::read(source,current,8u<<20)){error="Documento UI não pôde ser lido antes da publicação";return false;}
    expectedHash=Sha256::hex(current);
    if(known && expectedHash!=known->contentHash){error="Documento UI mudou fora do editor; reabra antes de salvar";return false;}
  }
  if(ec){error="Falha ao verificar documento UI";return false;}
  if(!transaction.begin(record.path,expectedHash,error))return false;
  if(!transaction.commit(bytes,next.serialize())){error=transaction.rollback()?"Publicação UI falhou; arquivo e registro restaurados":"Recuperação UI pendente no journal";return false;}
  assets_=std::move(next);assetRegistryDirty_=false;files_.rebuildTree();playScene_.sceneGui().invalidateResources();return true;
}
void EditorSession::refreshGuiImages() {
  const auto &document=isPlaying()?playScene_.gui().document():gui_.document();
  const double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
  std::vector<const ui::GuiDocument*> documents{&document};u64 revision=document.revision();
  if(isPlaying())for(const auto &i:playScene_.sceneGui().instances()){documents.push_back(&i->runtime.document());revision^=i->id*1099511628211ull+i->runtime.document().revision();}
  if(revision==guiImageDocumentRevision_ && now-guiImageCheckAt_<.3)return;
  guiImageDocumentRevision_=revision;guiImageCheckAt_=now;
  u64 stamp=1469598103934665603ull;
  const std::filesystem::path root(files_.rootPath());
  auto hash=[&](std::string_view text){for(unsigned char c:text){stamp^=c;stamp*=1099511628211ull;}};
  hash(root.generic_string());
  for(const auto *doc:documents)for(const auto &n:doc->nodes()) if(n.kind==ui::GuiKind::Image) {
    hash(n.image);std::filesystem::path path;std::error_code ec;
    if(EditorImportTransaction::safePath(root,n.image,path)) {const auto time=std::filesystem::last_write_time(path,ec);hash(ec?"missing":std::to_string(static_cast<long long>(time.time_since_epoch().count())));}
    else hash("unsafe");
  }
  const auto previous=guiImages_.revision();
  guiImages_.reconcile(documents,stamp,[&](std::string_view relative,std::vector<u8> &rgba,u32 &width,u32 &height,std::string &error) {
    std::filesystem::path path;
    if(!EditorImportTransaction::safePath(root,std::string(relative),path)){error="Imagem fora do projeto";return false;}
    std::error_code ec;const auto size=std::filesystem::file_size(path,ec);
    if(ec || size==0 || size>(8ull<<20)){error="Imagem ausente ou maior que 8 MiB";return false;}
    std::vector<u8> bytes(static_cast<usize>(size));std::ifstream file(path,std::ios::binary);
    if(!file.read(reinterpret_cast<char *>(bytes.data()),static_cast<std::streamsize>(bytes.size()))){error="Falha ao ler imagem";return false;}
    resources::ImageDecodeLimits limits;limits.maximumDimension=1024;limits.maximumPixels=1024ull*1024;limits.maximumEncodedBytes=8ull<<20;
    resources::DecodedImage image;if(!resources::decodeImageRgba8(bytes,limits,image,error))return false;
    width=image.width;height=image.height;rgba=std::move(image.rgba);return true;
  });
  if(guiImages_.revision()!=previous) {gui_.setImages(&guiImages_);playScene_.gui().setImages(&guiImages_);playScene_.sceneGui().setImages(&guiImages_);}
}
EditorViewport EditorSession::guiPlayView() const {
  auto view=view_;view.rect=layout_.viewport;
  // GUI input has already been mapped from the physical display to logical
  // coordinates. Use the same ray as GuiWorldFrame, without a second rotation.
  view.surfaceTransform={};const auto pose=sceneCameraPose();
  if(pose.entity && !view.rect.isEmpty()) {
    auto projection=projection_;projection.nearPlane=pose.nearPlane;projection.farPlane=pose.farPlane;projection.verticalFieldOfViewRadians=pose.verticalFov*.017453292519943295f;projection.roll=pose.roll;
    projection.projection=pose.projection==scene::CameraProjection::Orthographic?renderer::CameraProjection::Orthographic:renderer::CameraProjection::Perspective;projection.orthographicHalfHeight=pose.orthographicHalfHeight;
    view.frustum=renderer::buildPerspectiveFrustum(pose.position,pose.yaw,pose.pitch,view.rect.width/view.rect.height,projection);
  }
  return view;
}
bool EditorSession::guiPlayPointer(const ui::UiPointerEvent &event) {
  auto &sceneGui=playScene_.sceneGui();sceneGui.prepare(playScene_.world(),guiPlayView().frustum,layout_.viewport,state_.surface);
  if(sceneGui.captures(event.pointerId,event.device)||!sceneGui.instances().empty()||!sceneGui.diagnostic().empty())return sceneGui.pointer(playScene_.world(),event,[&](ui::UiPoint point,float distance){
    buildPickCandidates(&playScene_.document(),true);const auto scene=pickNearest(candidates_,screenPointToRay(guiPlayView(),point));return scene.hit&&scene.distance<distance-.0001f;
  });
  auto &runtime=playScene_.gui();const auto &canvas=runtime.document().canvas();
  if(canvas.mode==ui::GuiCanvasMode::Screen)return runtime.pointer(event);
  ui::UiPoint point;float distance=0;
  if(!guiWorld_.configure(canvas,guiPlayView().frustum,layout_.viewport,state_.surface) || !guiWorld_.map(event.position,point,distance)) {
    if(runtime.captures(event.pointerId,event.device)){auto cancel=event;cancel.phase=ui::UiPointerPhase::Cancel;runtime.pointer(cancel);return true;}return false;
  }
  if(event.phase==ui::UiPointerPhase::Down && canvas.occlusion) {
    buildPickCandidates(&playScene_.document(),true);const auto ray=screenPointToRay(guiPlayView(),event.position);const auto scene=pickNearest(candidates_,ray);
    if(scene.hit && scene.distance<distance-.0001f)return false;
  }
  runtime.layout({0,0,canvas.resolution.x,canvas.resolution.y});auto mapped=event;mapped.position=point;
  return runtime.pointer(mapped);
}

void EditorSession::refreshMaterialSlotView() {
  auto &view=state_.materialSlotView;view={};
  state_.projectMaterials.clear();
  for(const auto &material:materials_) state_.projectMaterials.push_back(material.name);
  state_.projectTextureNames.clear();state_.projectTextureDetails.clear();
  for(const auto &texture:textures_) {
    state_.projectTextureNames.push_back(texture.name);
    const auto users=textureUsersOf(texture.guid);
    state_.projectTextureDetails.push_back((texture.width?std::to_string(texture.width)+"×"+std::to_string(texture.height):std::string("ilegível"))+
                                           " · "+std::to_string(users)+(users==1?" uso · ":" usos · ")+texture.path);
  }
  const auto *entity=document_.find(state_.selection);
  const auto *render=entity?meshRenderer(*entity):nullptr;
  refreshTexturePanels();
  // R4: isolar na prévia vale para o objeto e slot em edição, nunca em Play.
  {
    const EditorEntityId isolated=(render && !isPlaying() && state_.materialIsolate)?state_.selection:kInvalidEntity;
    if(mapScene_.setMaterialIsolation(isolated,state_.materialSlot,isolated==kInvalidEntity?std::uint8_t{0}:state_.materialIsolate))
      appearanceChanged_=true;
  }
  state_.projectTextureThumbs.assign(textures_.size(),{});
  for(usize index=0;index<thumbnails_.size() && index<textures_.size();++index)
    if(thumbnails_[index].guid==textures_[index].guid) state_.projectTextureThumbs[index]=thumbnails_[index].content;
  // Material do projeto em Propriedades: a vista vem do recurso, não de um slot.
  if(materialAssetMode()) {
    if(const auto *material=findMaterialAsset(state_.materialInspector)) {refreshMaterialAssetView(*material);return;}
    state_.materialInspector={};state_.materialShared=false;state_.status="O material aberto não existe mais no projeto";
  }
  // A textura em Propriedades usa o mesmo visualizador sem objeto selecionado.
  if(!render) {state_.materialPicker=false;state_.texturePicker=false;state_.textureViewer=state_.textureViewer && state_.textureInspector;return;}
  view.slots=render->slotCount();
  state_.materialSlot=std::min(state_.materialSlot,view.slots-1);
  const u32 slot=state_.materialSlot;
  const auto guid=render->slotMaterialAsset(slot);
  const auto *shared=findMaterialAsset(guid);
  view.shared=shared!=nullptr;view.missing=guid.valid() && !shared;view.overridden=render->slotMaterial(slot).enabled;
  view.name=shared?shared->name:view.missing?std::string("Referência sem arquivo"):sourceMaterialName(render->slotAsset(slot));
  if(view.name.empty()) view.name="Material do pacote";
  // Sem recurso do projeto não há alcance compartilhado a mostrar.
  if(!view.shared) state_.materialShared=false;
  scene::MaterialParameters values=state_.materialShared?shared->values:mapScene_.slotMaterial(*render,slot);
  if(!state_.materialShared && !values.enabled && render->slotMesh(slot)) values=mapScene_.materialForAsset(render->slotMesh(slot)-1);
  scene::MeshRenderer probe;probe.material=values;
  // R4: modo de alfa, corte e faces efetivos no alcance em edição, e de onde vêm.
  {
    const u32 sourceFlags=render->slotMesh(slot)?mapScene_.materialFlagsForAsset(render->slotMesh(slot)-1):0u;
    const auto &local=render->slotSurface(slot);
    const scene::MaterialSurface none{};
    const auto &sharedSurface=shared?shared->surface:none;
    const auto &edited=state_.materialShared?sharedSurface:local;
    auto alpha=scene::MaterialAlphaKeep;float cutoff=.5f;const char *alphaOrigin="da fonte";
    if(edited.alphaMode!=scene::MaterialAlphaKeep) {
      alpha=edited.alphaMode;cutoff=edited.alphaCutoff;alphaOrigin=state_.materialShared?"do material do projeto":"só esta instância";
    } else if(!state_.materialShared && sharedSurface.alphaMode!=scene::MaterialAlphaKeep) {
      alpha=sharedSurface.alphaMode;cutoff=sharedSurface.alphaCutoff;alphaOrigin="do material do projeto";
    }
    if(alpha==scene::MaterialAlphaKeep)
      alpha=(sourceFlags&renderer::MapMaterialBlend)?scene::MaterialAlphaBlend:
            (sourceFlags&renderer::MapMaterialAlphaMask)?scene::MaterialAlphaMask:scene::MaterialAlphaOpaque;
    view.alphaLabel=alpha==scene::MaterialAlphaBlend?"Transparente":alpha==scene::MaterialAlphaMask?"Recorte":"Opaco";
    view.alphaOrigin=alphaOrigin;
    view.cutoffEditable=edited.alphaMode==scene::MaterialAlphaMask;
    view.alphaCutoff=cutoff;
    view.cutoffLabel=view.cutoffEditable?std::string():alpha==scene::MaterialAlphaMask?"corte definido fora deste alcance":"só vale no modo Recorte";
    auto sides=scene::MaterialSidesKeep;const char *sidesOrigin="da fonte";
    if(edited.sides!=scene::MaterialSidesKeep) {sides=edited.sides;sidesOrigin=state_.materialShared?"do material do projeto":"só esta instância";}
    else if(!state_.materialShared && sharedSurface.sides!=scene::MaterialSidesKeep) {sides=sharedSurface.sides;sidesOrigin="do material do projeto";}
    if(sides==scene::MaterialSidesKeep)
      sides=(sourceFlags&renderer::MapMaterialCullBackFaces) && !(sourceFlags&renderer::MapMaterialDoubleSided)?
            scene::MaterialSidesSingle:scene::MaterialSidesDouble;
    view.sidesLabel=sides==scene::MaterialSidesSingle?"Uma face":"Duas faces";
    view.sidesOrigin=state_.materialCulling?sidesOrigin:"sem efeito neste aparelho (sem culling dinâmico)";
  }
  // R4: oclusão, canais, normal, origem do alfa e isolamento, e de onde vêm.
  {
    const u32 sourceFlags=render->slotMesh(slot)?mapScene_.materialFlagsForAsset(render->slotMesh(slot)-1):0u;
    const scene::MaterialChannels none{};
    const auto &sharedChannels=shared?shared->channels:none;
    const auto &edited=state_.materialShared?sharedChannels:render->slotChannels(slot);
    const bool inherits=!state_.materialShared;
    const auto pick=[&](std::uint8_t own,std::uint8_t inherited) {return own?own:(inherits?inherited:std::uint8_t{0});};
    const auto originOf=[&](std::uint8_t own,std::uint8_t inherited)->const char * {
      if(own) return state_.materialShared?"do material do projeto":"só esta instância";
      return inherits && inherited?"do material do projeto":"da fonte";
    };
    const auto source=pick(edited.occlusionSource,sharedChannels.occlusionSource);
    static constexpr const char *sources[]{"","Sem oclusão","Canal do metal/rugosidade","Textura própria"};
    view.occlusionLabel=source?sources[source]:
        ((sourceFlags&renderer::MapMaterialOcclusionInMetallicRoughness)?"Canal do metal/rugosidade":"Sem oclusão");
    view.occlusionOrigin=originOf(edited.occlusionSource,sharedChannels.occlusionSource);
    resources::AssetGuid occlusionTexture=shared?shared->occlusionTexture:resources::AssetGuid{};
    if(!state_.materialShared && render->slotOcclusionTexture(slot).valid()) occlusionTexture=render->slotOcclusionTexture(slot);
    const auto *occlusionProject=occlusionTexture.valid()?findProjectTexture(occlusionTexture):nullptr;
    view.occlusionTextureLabel=occlusionProject?occlusionProject->name:occlusionTexture.valid()?std::string("Textura ausente"):std::string("Nenhuma");
    const float strength=edited.occlusionStrength>=0?edited.occlusionStrength:
        (inherits && sharedChannels.occlusionStrength>=0?sharedChannels.occlusionStrength:1.0f);
    char strengthText[16];std::snprintf(strengthText,sizeof(strengthText),"%.2f",static_cast<double>(strength));
    view.occlusionStrengthLabel=strengthText;
    static constexpr const char *letters[]{"R","G","B","A"};
    const auto channelLabel=[&](const char *prefix,std::uint8_t own,std::uint8_t inherited,unsigned fallback) {
      const auto chosen=pick(own,inherited);return std::string(prefix)+letters[chosen?chosen-1u:fallback];
    };
    view.channelLabels[0]=channelLabel("Rug. ",edited.roughness,sharedChannels.roughness,1);
    view.channelLabels[1]=channelLabel("Metal ",edited.metallic,sharedChannels.metallic,2);
    view.channelLabels[2]=channelLabel("Ocl. ",edited.occlusion,sharedChannels.occlusion,0);
    view.normalFlipLabel=pick(edited.normalFlipY,sharedChannels.normalFlipY)==scene::MaterialToggleOn?"Y invertido (DirectX)":"Y como no arquivo (OpenGL)";
    view.normalFlipOrigin=originOf(edited.normalFlipY,sharedChannels.normalFlipY);
    static constexpr const char *alphaSources[]{"Alfa da cor base","Alfa da cor base","Ignorar alfa (opaco)","Luminância da cor base"};
    view.alphaSourceLabel=alphaSources[pick(edited.alphaSource,sharedChannels.alphaSource)];
    view.alphaSourceOrigin=originOf(edited.alphaSource,sharedChannels.alphaSource);
    static constexpr const char *isolates[]{"Desligado","Oclusão","Rugosidade","Metal","Alfa","Normal"};
    view.isolateLabel=isolates[std::min<unsigned>(state_.materialIsolate,5u)];
  }
  // R4: textura efetiva de cada binding no alcance em edição, e de onde vem.
  for(u32 binding=0;binding<scene::MaterialTextureCount;++binding) {
    const auto &local=render->slotTextures(slot)[binding];
    resources::AssetGuid texture;const char *origin="da fonte";
    if(state_.materialShared) {
      texture=shared->textures[binding];
      if(texture.valid()) origin="do material do projeto";
    } else if(local.valid()) {
      texture=local;origin="só esta instância";
    } else if(shared && shared->textures[binding].valid()) {
      texture=shared->textures[binding];origin="do material do projeto";
    }
    if(texture==scene::MaterialTextureNone) view.textureNames[binding]="Sem textura";
    else if(texture.valid()) {
      const auto *project=findProjectTexture(texture);
      view.textureNames[binding]=project?project->name:std::string("Textura ausente");
    } else view.textureNames[binding]="Textura da fonte";
    view.textureOrigins[binding]=origin;
    // R4: amostragem no alcance em edição; herdar mostra o que vale por baixo.
    const scene::MaterialSampling noSampling{};
    const auto &sharedSampling=shared?shared->sampling[binding]:noSampling;
    const auto &edited=state_.materialShared?sharedSampling:render->slotSampling(slot)[binding];
    auto effective=edited;
    if(!state_.materialShared) {
      if(effective.uvSet==scene::MaterialUvKeep) effective.uvSet=sharedSampling.uvSet;
      if(effective.wrap==scene::MaterialWrapKeep) effective.wrap=sharedSampling.wrap;
      if(effective.filter==scene::MaterialFilterKeep) effective.filter=sharedSampling.filter;
      if(!effective.transformed()) {
        std::copy(std::begin(sharedSampling.offset),std::end(sharedSampling.offset),effective.offset);
        std::copy(std::begin(sharedSampling.scale),std::end(sharedSampling.scale),effective.scale);
        effective.rotation=sharedSampling.rotation;
      }
    }
    if(effective.overrides()) view.textureOrigins[binding]+=" · amostragem própria";
    if(binding==state_.textureBinding) {
      static constexpr const char *uvNames[]{"da fonte","UV 0","UV 1","Mundo (m)"};
      static constexpr const char *wrapNames[]{"da fonte","Repetir","Limitar","Espelhar"};
      static constexpr const char *filterNames[]{"da fonte","Linear","Próximo"};
      const auto label=[](const char *prefix,const char *const *names,std::uint8_t own,std::uint8_t inherited) {
        return std::string(prefix)+(own?names[own]:inherited?std::string("herda ")+names[inherited]:std::string("Herdar"));
      };
      state_.textureUvLabel=label("UV: ",uvNames,edited.uvSet,effective.uvSet);
      state_.textureWrapLabel=label("Rep.: ",wrapNames,edited.wrap,effective.wrap);
      state_.textureFilterLabel=label("Filtro: ",filterNames,edited.filter,effective.filter);
      state_.textureSamplerEditable=texture.valid() && texture!=scene::MaterialTextureNone;
      const auto number=[](const char *format,float value) {
        char text[32];std::snprintf(text,sizeof(text),format,static_cast<double>(value));return std::string(text);
      };
      state_.textureUvLabels[0]=number("Desl. U %.2f",effective.offset[0]);
      state_.textureUvLabels[1]=number("Desl. V %.2f",effective.offset[1]);
      state_.textureUvLabels[2]=number("Esc. U %.2f",effective.scale[0]);
      state_.textureUvLabels[3]=number("Esc. V %.2f",effective.scale[1]);
      state_.textureUvLabels[4]=number("Rot. %.0f°",effective.rotation);
    }
  }
  for(u32 field=0;field<scene::meshRendererNumbers.size() && field<std::size(view.values);++field)
    view.values[field]=scene::meshRendererNumbers[field].read(probe);
}

// Vista do material do projeto sozinho (Unity 6000.0 Material Inspector): o
// que o recurso define; o que ele deixa como está vem "da fonte de cada uso".
void EditorSession::refreshMaterialAssetView(const resources::MaterialAsset &material) {
  auto &view=state_.materialSlotView;view={};
  state_.materialShared=true;state_.materialPicker=false;state_.materialIsolate=0;
  view.slots=1;view.shared=true;view.name=material.name;
  const auto *record=assets_.find(material.guid);
  state_.materialInspectorPath=record?record->path:std::string();
  state_.materialInspectorRevision=material.revision;
  // Usos: slots de objetos da cena que apontam para ele.
  state_.materialInspectorSlots=0;state_.materialInspectorObjects.clear();
  std::vector<EditorEntityId> ids;document_.collectSubtree(document_.root(),ids);
  for(const auto id:ids) {
    const auto *entity=document_.find(id);const auto *render=entity?meshRenderer(*entity):nullptr;
    if(!render) continue;
    u32 slots=0;
    for(u32 slot=0;slot<render->slotCount();++slot) slots+=render->slotMaterialAsset(slot)==material.guid;
    if(slots) {state_.materialInspectorSlots+=slots;state_.materialInspectorObjects.push_back(id);}
  }
  const char *project="do material do projeto";const char *each="da fonte de cada uso";
  // Superfície.
  const auto &surface=material.surface;
  view.alphaLabel=surface.alphaMode==scene::MaterialAlphaBlend?"Transparente":surface.alphaMode==scene::MaterialAlphaMask?"Recorte":
                  surface.alphaMode==scene::MaterialAlphaOpaque?"Opaco":"Como a fonte";
  view.alphaOrigin=surface.alphaMode!=scene::MaterialAlphaKeep?project:each;
  view.cutoffEditable=surface.alphaMode==scene::MaterialAlphaMask;view.alphaCutoff=surface.alphaCutoff;
  view.cutoffLabel=view.cutoffEditable?std::string():"só vale no modo Recorte";
  view.sidesLabel=surface.sides==scene::MaterialSidesSingle?"Uma face":surface.sides==scene::MaterialSidesDouble?"Duas faces":"Como a fonte";
  view.sidesOrigin=!state_.materialCulling?"sem efeito neste aparelho (sem culling dinâmico)":surface.sides!=scene::MaterialSidesKeep?project:each;
  // Canais.
  const auto &channels=material.channels;
  static constexpr const char *sources[]{"Como a fonte","Sem oclusão","Canal do metal/rugosidade","Textura própria"};
  view.occlusionLabel=sources[std::min<unsigned>(channels.occlusionSource,3u)];
  view.occlusionOrigin=channels.occlusionSource?project:each;
  const auto *occlusion=material.occlusionTexture.valid()?findProjectTexture(material.occlusionTexture):nullptr;
  view.occlusionTextureLabel=occlusion?occlusion->name:material.occlusionTexture.valid()?std::string("Textura ausente"):std::string("Nenhuma");
  char strength[16];std::snprintf(strength,sizeof(strength),"%.2f",static_cast<double>(channels.occlusionStrength>=0?channels.occlusionStrength:1.0f));
  view.occlusionStrengthLabel=strength;
  static constexpr const char *letters[]{"R","G","B","A"};
  const auto channelLabel=[&](const char *prefix,std::uint8_t own,unsigned fallback) {return std::string(prefix)+letters[own?own-1u:fallback];};
  view.channelLabels[0]=channelLabel("Rug. ",channels.roughness,1);
  view.channelLabels[1]=channelLabel("Metal ",channels.metallic,2);
  view.channelLabels[2]=channelLabel("Ocl. ",channels.occlusion,0);
  view.normalFlipLabel=channels.normalFlipY==scene::MaterialToggleOn?"Y invertido (DirectX)":
                       channels.normalFlipY?"Y como no arquivo (OpenGL)":"Como a fonte";
  view.normalFlipOrigin=channels.normalFlipY?project:each;
  static constexpr const char *alphaSources[]{"Como a fonte","Alfa da cor base","Ignorar alfa (opaco)","Luminância da cor base"};
  view.alphaSourceLabel=alphaSources[std::min<unsigned>(channels.alphaSource,3u)];
  view.alphaSourceOrigin=channels.alphaSource?project:each;
  view.isolateLabel="Desligado";
  // Texturas e a amostragem do binding aberto no seletor.
  for(u32 binding=0;binding<scene::MaterialTextureCount;++binding) {
    const auto texture=material.textures[binding];
    if(texture==scene::MaterialTextureNone) view.textureNames[binding]="Sem textura";
    else if(texture.valid()) {
      const auto *found=findProjectTexture(texture);
      view.textureNames[binding]=found?found->name:std::string("Textura ausente");
    } else view.textureNames[binding]="Textura da fonte de cada uso";
    view.textureOrigins[binding]=texture.valid()?project:each;
    const auto &sampling=material.sampling[binding];
    if(sampling.overrides()) view.textureOrigins[binding]+=" · amostragem própria";
    if(binding==state_.textureBinding) {
      static constexpr const char *uvNames[]{"da fonte","UV 0","UV 1","Mundo (m)"};
      static constexpr const char *wrapNames[]{"da fonte","Repetir","Limitar","Espelhar"};
      static constexpr const char *filterNames[]{"da fonte","Linear","Próximo"};
      state_.textureUvLabel=std::string("UV: ")+(sampling.uvSet?uvNames[sampling.uvSet]:"Herdar");
      state_.textureWrapLabel=std::string("Rep.: ")+(sampling.wrap?wrapNames[sampling.wrap]:"Herdar");
      state_.textureFilterLabel=std::string("Filtro: ")+(sampling.filter?filterNames[sampling.filter]:"Herdar");
      state_.textureSamplerEditable=texture.valid() && texture!=scene::MaterialTextureNone;
      const auto number=[](const char *format,float value) {
        char text[32];std::snprintf(text,sizeof(text),format,static_cast<double>(value));return std::string(text);
      };
      state_.textureUvLabels[0]=number("Desl. U %.2f",sampling.offset[0]);
      state_.textureUvLabels[1]=number("Desl. V %.2f",sampling.offset[1]);
      state_.textureUvLabels[2]=number("Esc. U %.2f",sampling.scale[0]);
      state_.textureUvLabels[3]=number("Esc. V %.2f",sampling.scale[1]);
      state_.textureUvLabels[4]=number("Rot. %.0f°",sampling.rotation);
    }
  }
  scene::MeshRenderer probe;probe.material=material.values;
  for(u32 field=0;field<scene::meshRendererNumbers.size() && field<std::size(view.values);++field)
    view.values[field]=scene::meshRendererNumbers[field].read(probe);
}

bool EditorSession::openMaterialInspector(const resources::AssetGuid &guid) {
  if(!findMaterialAsset(guid)) return false;
  state_.materialInspector=guid;state_.materialShared=true;state_.materialInspectorUse=0;state_.environmentInspector={};
  state_.profileInspector={};
  state_.materialPicker=false;state_.texturePicker=false;state_.textureViewer=false;
  state_.textureInspector=false;state_.textureManager=false;state_.propertyPage=0;state_.propertyQuery.clear();
  state_.compactPanel=EditorScreenState::CompactPanel::Inspector;
  refreshMaterialSlotView();
  return true;
}

void EditorSession::publishMaterialLibrary() {
  std::vector<std::pair<resources::AssetGuid,EditorMapScene::SharedMaterial>> library;
  library.reserve(materials_.size());
  for(const auto &material:materials_)
    library.push_back({material.guid,{material.values,material.textures,material.surface,material.sampling,material.channels,
                                      material.occlusionTexture}});
  mapScene_.setMaterialLibrary(std::move(library));
  appearanceChanged_=true;
}

namespace {
std::string textureFileStem(std::string_view text) {
  std::string stem;
  for(unsigned char c:text) stem.push_back(c<32 || c==127 || c=='/' || c=='\\' || c==':' || c=='"' ? '_' : static_cast<char>(c));
  while(!stem.empty() && (stem.back()==' ' || stem.back()=='.')) stem.pop_back();
  if(stem.empty() || stem=="." || stem=="..") stem="textura";
  return stem.size()>96?stem.substr(0,96):stem;
}
} // namespace

namespace {
bool readProjectTextureProfile(const std::string &root,const std::string &relative,resources::TextureProfile &out) {
  std::filesystem::path absolute;
  if(root.empty() || !EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),relative,absolute)) return false;
  std::error_code error;
  if(!std::filesystem::exists(absolute,error)) return false;
  std::vector<u8> bytes;
  return EditorImportTransaction::read(absolute,bytes,64u*1024u) &&
         resources::parseTextureProfile(std::string_view(reinterpret_cast<const char *>(bytes.data()),bytes.size()),out);
}
bool writeProjectTextureProfile(const std::string &root,const std::string &relative,const resources::TextureProfile &profile) {
  std::filesystem::path absolute;
  if(root.empty() || !resources::validTextureProfile(profile) ||
     !EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),relative,absolute)) return false;
  std::error_code error;std::filesystem::create_directories(absolute.parent_path(),error);
  return !error && EditorImportTransaction::writeText(absolute,resources::serializeTextureProfile(profile));
}
} // namespace

void EditorSession::openTextureInspector(u32 projectTextureIndex) {
  if(projectTextureIndex>=textures_.size()) return;
  textureInspectorFromManager_=state_.textureManager;
  state_.textureManager=false;state_.textureInspector=true;
  texturePanelSelection_=state_.selection;
  openTextureViewer(projectTextureIndex);
}

void EditorSession::openTextureManager(std::string folder) {
  state_.textureManager=true;state_.textureInspector=false;state_.textureViewer=false;
  state_.textureFolder=std::move(folder);state_.textureManagerPage=0;
  texturePanelSelection_=state_.selection;
}

void EditorSession::refreshTexturePanels() {
  // Escolher outra coisa na cena devolve Propriedades ao objeto.
  if((state_.textureInspector || state_.textureManager) && state_.selection!=texturePanelSelection_) {
    state_.textureInspector=false;state_.textureManager=false;state_.textureViewer=false;
  }
  if(state_.textureInspector && state_.textureViewerIndex<textures_.size()) {
    const auto guid=textures_[state_.textureViewerIndex].guid;
    state_.textureUserLabels.clear();state_.textureUserEntities.clear();
    std::vector<EditorEntityId> ids;document_.collectSubtree(document_.root(),ids);
    for(const auto id:ids) {
      const auto *entity=document_.find(id);
      const auto *render=entity?meshRenderer(*entity):nullptr;
      if(!render) continue;
      bool uses=false;
      for(u32 slot=0;slot<render->slotCount() && !uses;++slot)
        uses=std::find(render->slotTextures(slot).begin(),render->slotTextures(slot).end(),guid)!=render->slotTextures(slot).end() ||
             render->slotOcclusionTexture(slot)==guid || render->slotLightmap(slot).texture==guid;
      if(!uses) continue;
      state_.textureUserLabels.push_back(std::string("Objeto: ")+entity->name);
      state_.textureUserEntities.push_back(id);
    }
    for(const auto &material:materials_)
      if(std::find(material.textures.begin(),material.textures.end(),guid)!=material.textures.end() || material.occlusionTexture==guid) {
        state_.textureUserLabels.push_back("Material do projeto: "+material.name);
        state_.textureUserEntities.push_back(kInvalidEntity);
      }
  }
  if(state_.textureManager) {
    const auto lower=[](std::string text) {for(auto &c:text) if(c>='A' && c<='Z') c=static_cast<char>(c-'A'+'a');return text;};
    const auto query=lower(state_.textureQuery);
    state_.textureManagerRows.clear();
    for(u32 index=0;index<textures_.size();++index) {
      const auto &texture=textures_[index];
      if(!state_.textureFolder.empty() && !texture.path.starts_with(state_.textureFolder+"/")) continue;
      if(!query.empty() && lower(texture.path).find(query)==std::string::npos) continue;
      bool pass=true;
      switch(state_.textureFilter) {
      case 1: pass=textureUsersOf(texture.guid)==0;break;
      case 2: pass=texture.width==0;break;
      case 3: pass=texture.changed;break;
      // Só texturas já analisadas pela miniatura entram em "sem alfa".
      case 4: pass=index<thumbnails_.size() && thumbnails_[index].guid==texture.guid && thumbnails_[index].opaque;break;
      case 5: pass=std::max(texture.width,texture.height)>importLimits_.maximumTextureDimension;break;
      default: break;
      }
      if(pass) state_.textureManagerRows.push_back(index);
    }
    // Bloco F: a lista das fontes, com a mesma busca por nome.
    if(state_.textureManagerSources) {
      collectSourceTextures();
      state_.textureManagerRows.clear();
      state_.sourceTextureNames.assign(sourceTextures_.size(),std::string());
      state_.sourceTextureThumbs.assign(sourceTextures_.size(),ui::UiRect{});
      for(u32 row=0;row<sourceTextures_.size();++row) {
        const auto &entry=sourceTextures_[row];
        state_.sourceTextureNames[row]=entry.name;
        const u32 cell=row%TextureThumbnailCapacity;
        if(sourceThumbCells_[cell]==entry.texture.get()) state_.sourceTextureThumbs[row]=sourceThumbContent_[cell];
        if(query.empty() || lower(entry.image+" "+entry.sourcePath).find(query)!=std::string::npos)
          state_.textureManagerRows.push_back(row);
      }
    }
  }
}

void EditorSession::setTextureStreamingLevels(std::span<const u32> desired,std::span<const u32> loaded) {
  if(std::equal(desired.begin(),desired.end(),textureStreamingDesired_.begin(),textureStreamingDesired_.end()) &&
     std::equal(loaded.begin(),loaded.end(),textureStreamingLoaded_.begin(),textureStreamingLoaded_.end())) return;
  textureStreamingDesired_.assign(desired.begin(),desired.end());
  textureStreamingLoaded_.assign(loaded.begin(),loaded.end());
  // Só as duas linhas de leitura mudam; o resto do visualizador fica como está.
  if(state_.textureViewer && state_.textureViewerIndex<textures_.size()) {
    const auto levels=textureStreamingLevelsOf(textures_[state_.textureViewerIndex].guid);
    if(levels.known) {
      state_.textureProfileLabels[10]="Carregado: mip "+std::to_string(levels.loaded)+" · "+std::to_string(levels.loadedWidth)+" px";
      state_.textureProfileLabels[11]="Tela pede: mip "+std::to_string(levels.desired);
    }
  }
}

EditorSession::TextureStreamingLevels EditorSession::textureStreamingLevelsOf(const resources::AssetGuid &texture) const {
  TextureStreamingLevels levels;
  for(const auto &[binding,residency]:publishedTextures_) {
    if(!(binding.guid==texture) || binding.index>=textureStreamingLoaded_.size() ||
       binding.index>=textureStreamingDesired_.size()) continue;
    levels.known=true;
    levels.loaded=textureStreamingLoaded_[binding.index];
    levels.desired=textureStreamingDesired_[binding.index];
    levels.loadedWidth=std::max(1u,residency.width>>std::min(levels.loaded,31u));
    break;
  }
  return levels;
}

void EditorSession::refreshLightExplorer() {
  u32 total=0,dark=0,filtered=0;
  const u32 first=state_.lightExplorerPage*8u;
  state_.lightExplorerRows.clear();
  std::vector<EditorEntityId> members;
  document_.collectSubtree(document_.root(),members);
  for(const auto id:members) {
    const auto *entity=document_.find(id);
    if(!entity) continue;
    for(usize c=0;c<entity->components.size();++c) {
      const auto *value=entity->components.at(c);
      if(!value || &value->type()!=&scene::Light::descriptor) continue;
      const auto &light=static_cast<const scene::Light &>(*value);
      const bool isDark=!light.enabled || !(light.intensity>0);
      ++total;dark+=isDark;
      if(state_.lightExplorerDarkOnly && !isDark) continue;
      if(filtered++<first || state_.lightExplorerRows.size()>=8) continue;
      static constexpr const char *kinds[]{"Direcional","Pontual","Spot"};
      const char *unit=light.unit==scene::LightUnit::Engine?"":light.kind==scene::LightKind::Directional?" lx":
                       light.unit==scene::LightUnit::LuxCandela?" cd":" lm";
      std::string detail=std::string(kinds[std::min<u32>(static_cast<u32>(light.kind),2u)])+" · "+
                         decimalText(light.intensity,light.intensity<10?2:0)+unit;
      if(light.kind!=scene::LightKind::Directional) detail+=" · "+decimalText(light.range,1)+" m";
      if(!(light.intensity>0)) detail+=" · intensidade 0: não ilumina";
      state_.lightExplorerRows.push_back({id,value->instanceId(),entity->name,detail,light.enabled,isDark});
    }
  }
  state_.lightExplorerTotal=total;state_.lightExplorerDark=dark;state_.lightExplorerFiltered=filtered;
  const u32 pages=std::max(1u,(filtered+7u)/8u);
  if(state_.lightExplorerPage>=pages) {state_.lightExplorerPage=pages-1;refreshLightExplorer();}
}

u32 EditorSession::applyLightExplorerIntensity() {
  if(isPlaying() || history_.isOpen()) return 0;
  std::vector<std::pair<EditorEntityId,u64>> targets;
  std::vector<EditorEntityId> members;
  document_.collectSubtree(document_.root(),members);
  for(const auto id:members)
    if(const auto *entity=document_.find(id))
      for(usize c=0;c<entity->components.size();++c) {
        const auto *value=entity->components.at(c);
        if(!value || &value->type()!=&scene::Light::descriptor) continue;
        const auto &light=static_cast<const scene::Light &>(*value);
        if(!state_.lightExplorerDarkOnly || !light.enabled || !(light.intensity>0)) targets.push_back({id,value->instanceId()});
      }
  if(targets.empty() || !history_.begin("Intensidade das luzes")) return 0;
  u32 changed=0;
  for(const auto &[id,instance]:targets) {
    const auto *entity=document_.find(id);
    if(!entity) continue;
    auto values=*entity;
    // Mesma validação do Inspector: fora da faixa, a luz fica como estava.
    const bool intensity=scene::setComponentProperty(values.components,scene::Light::descriptor.id,"intensity",
                                                     state_.lightExplorerIntensity,instance)==scene::ComponentPropertyStatus::Applied;
    const bool lit=scene::setComponentProperty(values.components,scene::Light::descriptor.id,"enabled",true,instance)==
                   scene::ComponentPropertyStatus::Applied;
    if((intensity||lit) && history_.applyValues(document_,id,values)) ++changed;
  }
  history_.end();
  refreshLightExplorer();
  return changed;
}

resources::TextureProfile EditorSession::textureProfileFor(const resources::AssetGuid &texture) const {
  for(const auto &[guid,profile]:textureProfiles_) if(guid==texture) return profile;
  return {};
}

bool EditorSession::setTextureProfile(const resources::AssetGuid &texture,const resources::TextureProfile &profile,
                                      std::string &diagnostic,bool recordHistory) {
  diagnostic.clear();
  if(isPlaying()) {diagnostic="Pare a execução antes de mudar o perfil da textura.";return false;}
  if(history_.isOpen()) {diagnostic="Finalize a edição antes de mudar o perfil da textura.";return false;}
  if(!resources::validTextureProfile(profile)) {diagnostic="Perfil de textura inválido.";return false;}
  const auto *record=assets_.find(texture);
  if(!record || record->type!=resources::AssetType::Texture) {diagnostic="Textura fora do projeto.";return false;}
  const auto before=textureProfileFor(texture);
  if(resources::sameTextureProfile(before,profile)) return true;
  // Cozinhe antes de persistir a receita: um formato ou orçamento incompatível
  // não pode deixar o registro apontando para um derivado que não será publicado.
  const auto root=EditorImportTransaction::fromUtf8(files_.rootPath());
  std::filesystem::path source;std::vector<u8> sourceBytes;
  if(!EditorImportTransaction::safePath(root,record->path,source)||
     !EditorImportTransaction::read(source,sourceBytes,importLimits_.image.maximumEncodedBytes)||
     Sha256::hex(sourceBytes)!=record->contentHash) {
    diagnostic="Fonte da textura está ausente ou mudou fora do editor; reimporte antes de alterar a receita.";
    return false;
  }
  resources::TextureImportLimits limits;limits.image=importLimits_.image;
  limits.projectMaximumDimension=importLimits_.maximumTextureDimension;
  resources::PreparedTextureImport prepared;
  if(!resources::prepareTextureImport(sourceBytes,profile,true,EditorMapScene::DefaultTextureSampler,
                                      limits,nullptr,prepared)) {
    diagnostic="Receita incompatível com a fonte: "+prepared.diagnostic;return false;
  }
  if(profile.interpretation==resources::TextureInterpretationUse) {
    auto normalProfile=profile;
    normalProfile.interpretation=resources::TextureInterpretationNormal;
    if(!resources::prepareTextureImport(sourceBytes,normalProfile,false,EditorMapScene::DefaultTextureSampler,
                                        limits,nullptr,prepared)) {
      diagnostic="Receita incompatível com uso normal: "+prepared.diagnostic;return false;
    }
  }
  resources::TextureProfile registeredRecipe;
  const bool canonical=record->importerVersion==resources::TextureAssetImporterRevision&&
      resources::parseTextureProfile(record->importerParameters,registeredRecipe);
  const auto previousAssets=assets_;
  const auto previousProfiles=textureProfiles_;
  const auto previousDecoded=decodedTextures_;
  const auto previousViewer=viewerChain_;
  const bool previousRegistryDirty=assetRegistryDirty_;
  EditorImportTransaction transaction(files_.rootPath());
  std::string nextRegistry;
  if(canonical) {
    auto next=assets_;
    if(!next.publishImport(texture,record->contentHash,record->importerVersion,
                           resources::serializeTextureProfile(profile),record->derived,record->dependencies)) {
      diagnostic="Registro recusou a receita da textura.";return false;
    }
    if(!transaction.begin(record->path,record->contentHash,diagnostic)) return false;
    nextRegistry=next.serialize();
    assets_=std::move(next);assetRegistryDirty_=false;
  }
  std::erase_if(textureProfiles_,[&](const auto &entry){return entry.first==texture;});
  textureProfiles_.emplace_back(texture,profile);
  // A decodificação muda (espaço de cor, mips, bordas, teto): nada do cache serve.
  std::erase_if(decodedTextures_,[&](const auto &entry){return entry.guid==texture;});
  if(viewerChain_.guid==texture) viewerChain_={};
  const auto restore=[&]() {
    assets_=previousAssets;textureProfiles_=previousProfiles;
    decodedTextures_=previousDecoded;viewerChain_=previousViewer;
    assetRegistryDirty_=previousRegistryDirty;
    std::string rollback;
    if(!publishAndAdopt(flattenSources(importedSources_),rollback))
      diagnostic+=" A GPU anterior não pôde ser republicada: "+rollback;
  };
  std::string publication;
  bool published=publishAndAdopt(flattenSources(importedSources_),publication);
  if(published) {
    std::vector<UsedTexture> used;collectUsedTextures(used);
    for(const auto &item:used) if(item.guid==texture) {
      const bool present=std::any_of(publishedTextures_.begin(),publishedTextures_.end(),[&](const auto &entry) {
        const auto &binding=entry.first;
        return binding.guid==item.guid&&binding.srgb==item.srgb&&
               binding.sampler==item.sampler&&binding.normal==item.normal;
      });
      if(!present) {published=false;publication="O binding da textura não chegou ao pacote gráfico.";break;}
    }
  }
  if(!published) {
    diagnostic="Perfil não aplicado: "+publication;
    if(canonical&&!transaction.rollback()) diagnostic+=" Recuperação de disco pendente; backups preservados.";
    restore();return false;
  }
  const bool saved=canonical?transaction.commit(sourceBytes,nextRegistry):
      writeProjectTextureProfile(files_.rootPath(),resources::textureProfilePath(texture),profile);
  if(!saved) {
    diagnostic="Perfil não gravado; publicação anterior restaurada.";
    if(canonical&&!transaction.rollback()) diagnostic+=" Recuperação de disco pendente; backups preservados.";
    restore();return false;
  }
  if(state_.textureViewer&&state_.textureViewerIndex<textures_.size()&&
     textures_[state_.textureViewerIndex].guid==texture) {
    state_.textureProfileSaved=state_.textureProfileDraft=profile;
    state_.textureProfileDirty=false;
    refreshTextureViewerImage();
  }
  if(recordHistory) {
    const auto project=files_.rootPath();
    if(!history_.recordResource("Perfil de textura",[this,texture,before,after=profile,project](bool forward) {
      if(files_.rootPath()!=project||!assets_.find(texture)||
         !resources::sameTextureProfile(textureProfileFor(texture),forward?before:after)) {
        state_.status="Perfil de textura mudou; histórico preservado sem sobrescrever.";return false;
      }
      std::string error;
      if(!setTextureProfile(texture,forward?after:before,error,false)) {state_.status=error;return false;}
      state_.status=forward?"Perfil de textura refeito":"Perfil de textura desfeito";
      return true;
    })) diagnostic="Perfil aplicado, mas o histórico não aceitou o passo de desfazer.";
  }
  return true;
}

std::vector<EditorSession::TextureResidency> EditorSession::textureResidencyOf(const resources::AssetGuid &texture) const {
  std::vector<TextureResidency> out;
  for(const auto &[binding,residency]:publishedTextures_) if(binding.guid==texture) out.push_back(residency);
  return out;
}

void EditorSession::loadTextureAssets() {
  textures_.clear();
  textureProfiles_.clear();
  const auto root=files_.rootPath();
  for(const auto &record:assets_.records()) {
    if(record.type!=resources::AssetType::Texture) continue;
    ProjectTexture texture;
    texture.guid=record.guid;texture.path=record.path;
    resources::TextureProfile profile;
    const bool registryRecipe=record.importerVersion==resources::TextureAssetImporterRevision &&
        resources::parseTextureProfile(record.importerParameters,profile);
    if(registryRecipe||readProjectTextureProfile(root,resources::textureProfilePath(record.guid),profile))
      textureProfiles_.emplace_back(record.guid,profile);
    const auto slash=record.path.find_last_of('/');
    texture.name=slash==std::string::npos?record.path:record.path.substr(slash+1);
    std::filesystem::path absolute;std::vector<u8> bytes;
    if(root.empty() || !EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),record.path,absolute) ||
       !EditorImportTransaction::read(absolute,bytes,importLimits_.image.maximumEncodedBytes) ||
       !resources::readImageDimensions(bytes,importLimits_.image,texture.width,texture.height))
      reportProblem(EditorConsoleSeverity::Warning,"Textura do projeto ausente ou ilegível: "+record.path+"; os bindings que a usam ficam com a textura da fonte.");
    // Filtro "alteradas": o arquivo no disco não é mais o conteúdo registrado.
    if(!bytes.empty()) texture.changed=Sha256::hex(bytes)!=record.contentHash;
    textures_.push_back(std::move(texture));
  }
}

renderer::SharedAuthoringTexture EditorSession::decodeProjectTexture(const resources::AssetGuid &guid,bool srgb,u32 sampler,bool normal) {
  const auto *record=assets_.find(guid);
  if(!record || record->type!=resources::AssetType::Texture) return {};
  // R4: perfil da textura: interpretação, tamanho, mips, bordas e anisotropia.
  const auto profile=textureProfileFor(guid);
  const u32 effectiveSampler=sampler|(profile.anisotropy?0u:renderer::AuthoringTextureNoAnisotropy);
  renderer::SharedAuthoringTexture base;
  for(const auto &entry:decodedTextures_)
    if(entry.guid==guid && entry.srgb==srgb && entry.normal==normal && entry.contentHash==record->contentHash) {
      if(entry.sampler==effectiveSampler) return entry.texture;
      base=entry.texture;
    }
  if(!base) {
  const auto root=files_.rootPath();
  std::filesystem::path absolute;std::vector<u8> bytes;
  if(root.empty() || !EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),record->path,absolute) ||
     !EditorImportTransaction::read(absolute,bytes,importLimits_.image.maximumEncodedBytes)) {
    reportProblem(EditorConsoleSeverity::Warning,"Textura do projeto não decodificada: "+record->path+
                  "; o binding usa a textura da fonte.");
    return {};
  }
  resources::TextureImportLimits limits;limits.image=importLimits_.image;
  limits.projectMaximumDimension=importLimits_.maximumTextureDimension;
  auto effectiveProfile=profile;
  if(normal&&profile.interpretation==resources::TextureInterpretationUse)
    effectiveProfile.interpretation=resources::TextureInterpretationNormal;
  const auto sourceHash=Sha256::hex(bytes);
  if(sourceHash!=record->contentHash) {
    reportProblem(EditorConsoleSeverity::Warning,"Textura mudou fora do editor: "+record->path+
                  "; reimporte para atualizar fonte, receita e cache juntos.");
    return {};
  }
  const auto expectedKey=resources::textureAssetCacheKey(sourceHash,effectiveProfile,srgb,sampler,limits);
  resources::PreparedTextureImport prepared;
  std::filesystem::path cachePath;std::vector<u8> cacheBytes;
  if(EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),resources::textureAssetCachePath(guid),cachePath)&&
     EditorImportTransaction::read(cachePath,cacheBytes,limits.maximumDerivedBytes+64u*1024u))
    resources::readTextureAssetCache(cacheBytes,expectedKey,limits,prepared);
  if(!prepared.valid()&&!resources::prepareTextureImport(bytes,effectiveProfile,srgb,sampler,limits,nullptr,prepared)) {
    reportProblem(EditorConsoleSeverity::Warning,"Textura do projeto não preparada: "+record->path+
                  (prepared.diagnostic.empty()?std::string():" ("+prepared.diagnostic+")")+"; o binding usa a textura da fonte.");
    return {};
  }
  auto texture=prepared.texture;
  std::erase_if(decodedTextures_,[&](const auto &entry){return entry.guid==guid && entry.srgb==srgb && entry.normal==normal;});
  decodedTextures_.push_back({guid,srgb,normal,effectiveSampler,record->contentHash,texture});
  base=texture;
  }
  if(base->samplerFlags==effectiveSampler) return base;
  // Outro sampler para a mesma imagem: cópia da cadeia com as flags pedidas. A
  // cadeia fica duplicada na CPU e na GPU enquanto as duas amostragens forem usadas.
  auto variant=std::make_shared<renderer::AuthoringTexture>(*base);
  variant->samplerFlags=effectiveSampler;
  decodedTextures_.push_back({guid,srgb,normal,effectiveSampler,record->contentHash,variant});
  return variant;
}

void EditorSession::collectUsedTextures(std::vector<UsedTexture> &out) const {
  out.clear();
  const auto add=[&](const resources::AssetGuid &guid,u32 binding,const scene::MaterialSampling &sampling) {
    if(!guid.valid() || guid==scene::MaterialTextureNone) return;
    const UsedTexture item{guid,EditorMapScene::bindingIsSrgb(binding),binding==1,EditorMapScene::samplerFlags(sampling)};
    if(std::find(out.begin(),out.end(),item)==out.end()) out.push_back(item);
  };
  for(const auto &material:materials_)
    for(u32 binding=0;binding<scene::MaterialTextureCount;++binding) add(material.textures[binding],binding,material.sampling[binding]);
  // Oclusão própria: dado linear, amostrada como o metal/rugosidade (binding 2).
  for(const auto &material:materials_) add(material.occlusionTexture,2,material.sampling[2]);
  // Texturas da cena que ainda vai ser aberta (reabertura do projeto).
  for(const auto &item:anticipatedTextures_)
    if(std::find(out.begin(),out.end(),item)==out.end()) out.push_back(item);
  for(const auto &item:runtimeTextures_)
    if(std::find(out.begin(),out.end(),item)==out.end()) out.push_back(item);
  std::vector<EditorEntityId> ids;document_.collectSubtree(document_.root(),ids);
  for(const auto id:ids) {
    const auto *entity=document_.find(id);
    const auto *render=entity?meshRenderer(*entity):nullptr;
    if(!render) continue;
    // Combinação efetiva de cada binding: a textura pode vir do material e a
    // amostragem da instância, e é essa textura com esse sampler que sobe.
    for(u32 slot=0;slot<render->slotCount();++slot)
      for(u32 binding=0;binding<scene::MaterialTextureCount;++binding)
        add(mapScene_.slotTexture(*render,slot,binding),binding,mapScene_.slotSampling(*render,slot,binding));
    for(u32 slot=0;slot<render->slotCount();++slot)
      add(mapScene_.slotOcclusionTexture(*render,slot),2,mapScene_.slotSampling(*render,slot,2));
    for(u32 slot=0;slot<render->slotCount();++slot)
      add(render->slotLightmap(slot).texture,2,scene::lightmapSampling());
  }
}

bool EditorSession::ensureTexturesPublished(std::string &diagnostic) {
  std::vector<UsedTexture> used;
  collectUsedTextures(used);
  const auto &published=mapScene_.textureLibrary();
  const bool complete=std::all_of(used.begin(),used.end(),[&](const auto &item) {
    return std::any_of(published.begin(),published.end(),[&](const auto &entry){
      return entry.guid==item.guid && entry.srgb==item.srgb && entry.normal==item.normal && entry.sampler==item.sampler;
    });
  });
  // Publicar de novo custa uma reconstrução da biblioteca (R2 ainda não publica
  // textura incremental); só acontece quando falta alguma textura na GPU.
  return complete || publishAndAdopt(flattenSources(importedSources_),diagnostic);
}

bool EditorSession::extractSourceTextures(const std::string &sourcePath,TextureExtraction &report,std::string &diagnostic) {
  report={};diagnostic.clear();
  if(isPlaying()) {diagnostic="Pare a execução antes de extrair texturas.";return false;}
  const auto root=files_.rootPath();
  std::filesystem::path absolute;std::vector<u8> bytes;
  if(root.empty() || !EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),sourcePath,absolute) ||
     !EditorImportTransaction::read(absolute,bytes)) {
    diagnostic="Não foi possível ler "+sourcePath+".";return false;
  }
  std::vector<resources::GlbEmbeddedImage> images;
  if(!resources::listGlbEmbeddedImages(bytes,images,diagnostic)) return false;
  if(images.empty()) {diagnostic="A fonte não tem imagens embutidas.";return false;}
  const auto slash=sourcePath.find_last_of('/');
  std::string stem=slash==std::string::npos?sourcePath:sourcePath.substr(slash+1);
  if(const auto dot=stem.find_last_of('.');dot!=std::string::npos && dot>0) stem.erase(dot);
  stem=textureFileStem(stem);
  auto nextAssets=assets_;
  struct Pending {std::filesystem::path target;std::vector<u8> bytes;};
  std::vector<Pending> writes;
  for(auto &image:images) {
    const char *extension=image.container==resources::ImageContainer::Png?".png":
                          image.container==resources::ImageContainer::Jpeg?".jpg":nullptr;
    if(!extension) {++report.skipped;continue;}
    const auto hash=Sha256::hex(image.bytes);
    const resources::AssetRecord *existing=nullptr;
    for(const auto &record:nextAssets.records())
      if(record.type==resources::AssetType::Texture && record.contentHash==hash) {existing=&record;break;}
    if(existing) {report.textures.push_back(existing->guid);++report.reused;continue;}
    std::string name=image.name.empty()?"imagem-"+std::to_string(image.index+1):image.name;
    for(const auto *suffix:{".png",".PNG",".jpg",".JPG",".jpeg",".JPEG"})
      if(name.size()>std::strlen(suffix) && name.ends_with(suffix)) {name.resize(name.size()-std::strlen(suffix));break;}
    name=textureFileStem(name);
    std::string path="Texturas/"+stem+"/"+name+extension;
    for(u32 n=2;nextAssets.findByPath(path) || files_.exists(path);++n) path="Texturas/"+stem+"/"+name+" "+std::to_string(n)+extension;
    resources::AssetRecord record;
    record.guid=resources::assetGuidFromSeed("texture:"+path+":"+hash+":"+
        std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+":"+std::to_string(++importInstanceCounter_));
    record.type=resources::AssetType::Texture;record.path=path;record.contentHash=hash;
    // Proveniência, sem transformar a textura em fonte reimportável.
    record.importerParameters="extraida-de "+sourcePath+" imagem "+std::to_string(image.index);
    std::filesystem::path target;
    if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),path,target) || !nextAssets.add(record)) {
      diagnostic="O registro recusou "+path+"; nada foi extraído.";return false;
    }
    writes.push_back({target,std::move(image.bytes)});
    report.textures.push_back(record.guid);++report.created;
  }
  std::vector<std::filesystem::path> written;
  for(const auto &pending:writes) {
    std::error_code error;std::filesystem::create_directories(pending.target.parent_path(),error);
    if(error || !EditorImportTransaction::write(pending.target,pending.bytes)) {
      for(const auto &path:written) std::filesystem::remove(path,error);
      diagnostic="Não foi possível gravar as texturas extraídas; nada foi registrado.";return false;
    }
    written.push_back(pending.target);
  }
  if(report.created) {assets_=std::move(nextAssets);assetRegistryDirty_=true;}
  loadTextureAssets();
  files_.rebuildTree();
  state_.status=std::to_string(report.created)+" textura(s) extraída(s), "+std::to_string(report.reused)+" já no projeto"+
                (report.skipped?", "+std::to_string(report.skipped)+" sem arquivo próprio (KTX2)":std::string());
  return true;
}

bool EditorSession::repairSceneResource(resources::AssetGuid from,resources::AssetGuid to,EditorSceneVersion expectedVersion,std::string &diagnostic) {
  if(isPlaying()||history_.isOpen()||expectedVersion.epoch!=sceneEpoch_||expectedVersion.revision!=document_.revision()) {
    diagnostic="A cena mudou ou está ocupada. Reabra o reparo.";return false;
  }
  auto stagedDocument=document_;auto stagedHistory=history_;
  if(!stagedHistory.begin("Reparar recursos da cena")) return false;
  std::vector<EditorEntityId> ids;document_.collectSubtree(document_.root(),ids);u32 count=0;
  bool textureChanged=false;
  for(auto id:ids) {
    auto candidate=*document_.find(id);bool changed=false;
    for(usize index=0;index<candidate.components.size();++index) {
      const auto *component=candidate.components.at(index);
      const auto uses=localResourceUses(component,from);if(uses.empty()) continue;
      const auto choices=resourceRepairChoices(component,from,&assets_,&mapScene_);
      if(std::none_of(choices.begin(),choices.end(),[&](const auto &entry){return entry.asset==to;})) {
        diagnostic="Um consumidor tem vínculo incompatível; nada foi substituído.";return false;
      }
      if(uses.front().kind==ComponentResourceKind::Texture) {
        if(!findProjectTexture(to)||!decodeProjectTexture(to,true,EditorMapScene::DefaultTextureSampler)) {diagnostic="Textura não carregada; nada foi substituído.";return false;}
        textureChanged=true;
      }
      auto *editable=candidate.components.editInstance(component->instanceId());
      if(!editable) {diagnostic="Componente ausente durante o reparo; nada foi substituído.";return false;}
      count+=replaceLocalResource(*editable,from,to,uses.front().kind,mapScene_);changed=true;
      if(!editable->valid()) {diagnostic="Componente inválido; nada foi substituído.";return false;}
    }
    if(changed&&!stagedHistory.applyValues(stagedDocument,id,candidate)) {diagnostic="Histórico recusou o reparo; cena preservada.";return false;}
  }
  if(!count) {diagnostic="Nenhum uso local encontrado na cena.";return false;}
  stagedHistory.end();document_=std::move(stagedDocument);history_=std::move(stagedHistory);
  diagnostic=std::to_string(count)+" vínculo(s) local(is) reparado(s) na cena";
  if(textureChanged) {std::string publication;if(!ensureTexturesPublished(publication)) diagnostic+="; publicação pendente: "+publication;}
  return true;
}

bool EditorSession::repairSharedTexture(resources::AssetGuid material,resources::AssetGuid from,resources::AssetGuid to,
    u32 expectedMaterialRevision,EditorSceneVersion expectedVersion,std::string &diagnostic) {
  const auto *current=findMaterialAsset(material);
  if(isPlaying()||history_.isOpen()||expectedVersion.epoch!=sceneEpoch_||expectedVersion.revision!=document_.revision()||
      !current||current->revision!=expectedMaterialRevision) {diagnostic="A prévia mudou; reabra o reparo.";return false;}
  if(!from.valid()||from==scene::MaterialTextureNone||from==to||!findProjectTexture(to)||!decodeProjectTexture(to,true,EditorMapScene::DefaultTextureSampler)) {
    diagnostic="Textura substituta indisponível.";return false;
  }
  auto candidate=*current;u32 changed=0;
  for(auto &texture:candidate.textures) if(texture==from) {texture=to;++changed;}
  if(candidate.occlusionTexture==from) {candidate.occlusionTexture=to;++changed;}
  if(!changed) {diagnostic="O material não usa essa textura.";return false;}
  ++candidate.revision;
  if(!commitSharedMaterial(candidate,diagnostic)) return false;
  std::string publication;
  diagnostic=std::to_string(changed)+" binding(s) do material reparado(s)";
  if(!ensureTexturesPublished(publication)) diagnostic+="; publicação pendente: "+publication;
  return true;
}

bool EditorSession::repairComponentResource(EditorEntityId id,u64 instance,resources::AssetGuid from,resources::AssetGuid to,
                                            EditorSceneVersion expectedVersion,std::string &diagnostic) {
  if(isPlaying()||history_.isOpen()||expectedVersion.revision!=document_.revision()||expectedVersion.epoch!=sceneEpoch_) {
    diagnostic="A cena mudou ou está ocupada. Abra novamente o reparo.";return false;
  }
  const auto *object=document_.find(id);
  const auto *component=object?object->components.findInstance(instance):nullptr;
  const auto choices=resourceRepairChoices(component,from,&assets_,&mapScene_);
  if(std::none_of(choices.begin(),choices.end(),[&](const auto &entry){return entry.asset==to;})) {
    diagnostic="Recurso incompatível ou indisponível para este vínculo.";return false;
  }
  const auto uses=localResourceUses(component,from);
  if(uses.front().kind==ComponentResourceKind::Texture&&(!findProjectTexture(to)||!decodeProjectTexture(to,true,EditorMapScene::DefaultTextureSampler))) {
    diagnostic="Carregue a textura do projeto antes de reparar o vínculo.";return false;
  }
  auto candidate=*object;
  auto *editable=candidate.components.editInstance(instance);
  const auto count=editable?replaceLocalResource(*editable,from,to,uses.front().kind,mapScene_):0;
  if(!count||!editable->valid()||!history_.applyValues(document_,id,candidate)) {
    diagnostic="O histórico recusou o reparo.";return false;
  }
  diagnostic=std::to_string(count)+" vínculo(s) local(is) substituído(s)";
  if(uses.front().kind==ComponentResourceKind::Texture) {
    std::string publication;
    if(!ensureTexturesPublished(publication)) diagnostic+="; publicação pendente: "+publication;
  }
  return true;
}

bool EditorSession::setSlotTexture(EditorEntityId id,u32 slot,u32 binding,MaterialScope scope,const resources::AssetGuid &texture,
                                   std::string &diagnostic) {
  diagnostic.clear();
  const auto *entity=document_.find(id);
  const auto *render=entity?meshRenderer(*entity):nullptr;
  if(isPlaying() || history_.isOpen()) {diagnostic="Finalize a edição antes de trocar a textura.";return false;}
  // O binding logo depois dos quatro do pacote é a textura de oclusão própria.
  const bool assetShared=scope==MaterialScope::Shared && materialAssetMode();
  if((!assetShared && (!render || slot>=render->slotCount())) || binding>scene::MaterialOcclusionTextureBinding) {diagnostic="Binding de textura inexistente.";return false;}
  if(texture.valid() && texture!=scene::MaterialTextureNone && !findProjectTexture(texture)) {diagnostic="Textura fora do projeto.";return false;}
  if(scope==MaterialScope::Shared) {
    const auto guid=assetShared?state_.materialInspector:render->slotMaterialAsset(slot);
    auto found=std::find_if(materials_.begin(),materials_.end(),[&](const auto &material){return material.guid==guid;});
    if(!guid.valid() || found==materials_.end()) {
      diagnostic="Este slot usa o material da fonte; crie um material do projeto para trocar a textura em todos os usos.";return false;
    }
    const auto *record=assets_.find(guid);
    if(!record) {diagnostic="Material fora do registro.";return false;}
    auto candidate=*found;
    if(binding==scene::MaterialOcclusionTextureBinding) candidate.occlusionTexture=texture;
    else candidate.textures[binding]=texture;
    ++candidate.revision;
    if(!commitSharedMaterial(candidate,diagnostic,true,binding==scene::MaterialOcclusionTextureBinding?"ch1":"tex"+std::to_string(binding))) return false;
  } else {
    auto values=*entity;
    if(binding==scene::MaterialOcclusionTextureBinding) *editMeshRenderer(values)->editSlotOcclusionTexture(slot)=texture;
    else (*editMeshRenderer(values)->editSlotTextures(slot))[binding]=texture;
    if(!history_.applyValues(document_,id,values)) {diagnostic="O histórico recusou a troca de textura.";return false;}
  }
  std::string publish;
  if(!ensureTexturesPublished(publish)) {diagnostic="Textura trocada, mas a publicação falhou: "+publish;return false;}
  return true;
}

bool EditorSession::setSlotSurface(EditorEntityId id,u32 slot,MaterialScope scope,const scene::MaterialSurface &surface,
                                   std::string &diagnostic,std::string_view field) {
  diagnostic.clear();
  const auto *entity=document_.find(id);
  const auto *render=entity?meshRenderer(*entity):nullptr;
  if(isPlaying() || history_.isOpen()) {diagnostic="Finalize a edição antes de mudar o material.";return false;}
  const bool assetShared=scope==MaterialScope::Shared && materialAssetMode();
  if(!assetShared && (!render || slot>=render->slotCount())) {diagnostic="Slot de material inexistente.";return false;}
  if(!scene::validMaterialSurface(surface)) {diagnostic="Modo de alfa, corte ou faces inválido.";return false;}
  if(scope==MaterialScope::Shared) {
    const auto guid=assetShared?state_.materialInspector:render->slotMaterialAsset(slot);
    auto found=std::find_if(materials_.begin(),materials_.end(),[&](const auto &material){return material.guid==guid;});
    if(!guid.valid() || found==materials_.end()) {
      diagnostic="Este slot usa o material da fonte; crie um material do projeto para editar todos os usos.";return false;
    }
    const auto *record=assets_.find(guid);
    if(!record) {diagnostic="Material fora do registro.";return false;}
    auto candidate=*found;
    candidate.surface=surface;++candidate.revision;
    if(!commitSharedMaterial(candidate,diagnostic,true,field)) return false;
    return true;
  }
  auto values=*entity;
  *editMeshRenderer(values)->editSlotSurface(slot)=surface;
  if(!history_.applyValues(document_,id,values)) {diagnostic="O histórico recusou a troca de material.";return false;}
  return true;
}

bool EditorSession::setSlotChannels(EditorEntityId id,u32 slot,MaterialScope scope,const scene::MaterialChannels &channels,
                                    std::string &diagnostic,std::string_view field) {
  diagnostic.clear();
  const auto *entity=document_.find(id);
  const auto *render=entity?meshRenderer(*entity):nullptr;
  if(isPlaying() || history_.isOpen()) {diagnostic="Finalize a edição antes de mudar o material.";return false;}
  const bool assetShared=scope==MaterialScope::Shared && materialAssetMode();
  if(!assetShared && (!render || slot>=render->slotCount())) {diagnostic="Slot de material inexistente.";return false;}
  if(!scene::validMaterialChannels(channels)) {diagnostic="Canal, oclusão, normal ou origem do alfa inválido.";return false;}
  if(scope==MaterialScope::Shared) {
    const auto guid=assetShared?state_.materialInspector:render->slotMaterialAsset(slot);
    auto found=std::find_if(materials_.begin(),materials_.end(),[&](const auto &material){return material.guid==guid;});
    if(!guid.valid() || found==materials_.end()) {
      diagnostic="Este slot usa o material da fonte; crie um material do projeto para editar todos os usos.";return false;
    }
    const auto *record=assets_.find(guid);
    if(!record) {diagnostic="Material fora do registro.";return false;}
    auto candidate=*found;
    candidate.channels=channels;++candidate.revision;
    if(!commitSharedMaterial(candidate,diagnostic,true,field)) return false;
    return true;
  }
  auto values=*entity;
  *editMeshRenderer(values)->editSlotChannels(slot)=channels;
  if(!history_.applyValues(document_,id,values)) {diagnostic="O histórico recusou a troca de material.";return false;}
  return true;
}

bool EditorSession::setSlotSampling(EditorEntityId id,u32 slot,u32 binding,MaterialScope scope,const scene::MaterialSampling &sampling,
                                    std::string &diagnostic,std::string_view field) {
  diagnostic.clear();
  const auto *entity=document_.find(id);
  const auto *render=entity?meshRenderer(*entity):nullptr;
  if(isPlaying() || history_.isOpen()) {diagnostic="Finalize a edição antes de mudar a amostragem.";return false;}
  const bool assetShared=scope==MaterialScope::Shared && materialAssetMode();
  if((!assetShared && (!render || slot>=render->slotCount())) || binding>=scene::MaterialTextureCount) {diagnostic="Binding de textura inexistente.";return false;}
  if(!scene::validMaterialSampling(sampling)) {diagnostic="Conjunto de UV, repetição ou filtro inválido.";return false;}
  if(scope==MaterialScope::Shared) {
    const auto guid=assetShared?state_.materialInspector:render->slotMaterialAsset(slot);
    auto found=std::find_if(materials_.begin(),materials_.end(),[&](const auto &material){return material.guid==guid;});
    if(!guid.valid() || found==materials_.end()) {
      diagnostic="Este slot usa o material da fonte; crie um material do projeto para editar todos os usos.";return false;
    }
    const auto *record=assets_.find(guid);
    if(!record) {diagnostic="Material fora do registro.";return false;}
    auto candidate=*found;
    candidate.sampling[binding]=sampling;++candidate.revision;
    if(!commitSharedMaterial(candidate,diagnostic,true,field)) return false;
  } else {
    auto values=*entity;
    (*editMeshRenderer(values)->editSlotSampling(slot))[binding]=sampling;
    if(!history_.applyValues(document_,id,values)) {diagnostic="O histórico recusou a troca de amostragem.";return false;}
  }
  // Outro sampler numa textura do projeto pode pedir uma textura publicada nova.
  std::string publish;
  if(!ensureTexturesPublished(publish)) {diagnostic="Amostragem trocada, mas a publicação falhou: "+publish;return false;}
  return true;
}

void EditorSession::loadMaterialAssets() {
  loadTextureAssets();
  materials_.clear();
  const auto root=files_.rootPath();
  if(!root.empty()) for(const auto &record:assets_.records()) {
    if(record.type!=resources::AssetType::Material) continue;
    std::filesystem::path absolute;std::vector<u8> bytes;resources::MaterialAsset material;
    // Arquivo ausente ou ilegível NÃO some em silêncio: o slot que o referencia
    // volta ao material da fonte, e o motivo fica no console.
    if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),record.path,absolute) ||
       !EditorImportTransaction::read(absolute,bytes,64u*1024u) ||
       !resources::MaterialAsset::deserialize(std::string_view(reinterpret_cast<const char *>(bytes.data()),bytes.size()),material) ||
       material.guid!=record.guid) {
      reportProblem(EditorConsoleSeverity::Warning,"Material do projeto ilegível ou ausente: "+record.path+"; slots usam o material da fonte.");
      continue;
    }
    materials_.push_back(std::move(material));
  }
  publishMaterialLibrary();
}

void EditorSession::loadEnvironmentProfiles() {
  environmentProfiles_.clear();
  const auto root=files_.rootPath();
  if(root.empty()) return;
  for(const auto &record:assets_.records()) {
    if(record.type!=resources::AssetType::EnvironmentProfile) continue;
    std::filesystem::path absolute;std::vector<u8> bytes;resources::EnvironmentProfile profile;
    if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),record.path,absolute) ||
       !EditorImportTransaction::read(absolute,bytes,64u*1024u) ||
       !resources::EnvironmentProfile::deserialize(
          std::string_view(reinterpret_cast<const char*>(bytes.data()),bytes.size()),profile) ||
       profile.guid!=record.guid) {
      reportProblem(EditorConsoleSeverity::Warning,
                    "Perfil de ambiente ilegível ou ausente: "+record.path+"; o volume conserva sua cópia local.");
      continue;
    }
    environmentProfiles_.push_back(std::move(profile));
  }
}

bool EditorSession::writeEnvironmentProfile(const resources::EnvironmentProfile &profile,
                                            const std::string &path,std::string &diagnostic) {
  std::filesystem::path absolute;
  if(files_.rootPath().empty() || !EditorImportTransaction::safePath(
      EditorImportTransaction::fromUtf8(files_.rootPath()),path,absolute)) {
    diagnostic="Perfis de ambiente precisam de um projeto aberto.";return false;
  }
  std::error_code error;std::filesystem::create_directories(absolute.parent_path(),error);
  if(error||!EditorImportTransaction::writeText(absolute,profile.serialize())) {
    diagnostic="Não foi possível gravar "+path+".";return false;
  }
  return true;
}

bool EditorSession::commitEnvironmentProfile(const resources::EnvironmentProfile &candidate,
                                             std::string &diagnostic,bool recordHistory) {
  return commitProfileBatch({candidate},diagnostic,recordHistory);
}

void EditorSession::synchronizeEnvironmentProfile(const resources::EnvironmentProfile &profile) {
  std::vector<EditorEntityId> ids;document_.collectSubtree(document_.root(),ids);
  for(const auto id:ids) {
    const auto *entity=document_.find(id);if(!entity) continue;
    auto values=*entity;bool changed=false;
    for(u32 index=0;index<values.components.size();++index) {
      auto *component=values.components.editInstance(values.components.at(index)->instanceId());
      if(!component||&component->type()!=&scene::Environment::descriptor) continue;
      auto &environment=static_cast<scene::Environment&>(*component);
      if(environment.profile!=profile.guid) continue;
      environment.values=resources::applyEnvironmentProfile(environment.values,profile);changed=true;
    }
    // A cópia é o fallback serializado para recurso ausente. Sua sincronização
    // faz parte da transação do recurso; o callback de undo/redo repete-a.
    if(changed) document_.applyEntityValues(id,values);
  }
}

resources::AssetGuid EditorSession::createEnvironmentProfile(EditorEntityId id,u64 instance,
                                                              std::string &diagnostic) {
  diagnostic.clear();const auto *entity=document_.find(id);
  const auto *component=entity?entity->components.findInstance(instance):nullptr;
  const auto *environment=component && &component->type()==&scene::Environment::descriptor?
      static_cast<const scene::Environment*>(component):nullptr;
  if(isPlaying()||history_.isOpen()||!environment) {diagnostic="Volume de ambiente indisponível.";return {};}
  resources::EnvironmentProfile profile;profile.name=entity->name[0]?entity->name:"Ambiente";
  profile.values=environment->values;profile.values.active=true;profile.values.priority=0;
  std::string stem;for(unsigned char c:profile.name)
    stem.push_back(c<32||c==127||c=='/'||c=='\\'||c==':'||c=='"'?'_':static_cast<char>(c));
  if(stem.empty()||stem=="."||stem=="..") stem="Ambiente";
  std::string path="Ambientes/"+stem+".environment";
  for(u32 n=2;assets_.findByPath(path)||files_.exists(path);++n)
    path="Ambientes/"+stem+" "+std::to_string(n)+".environment";
  profile.guid=resources::assetGuidFromSeed("environment:"+path+":"+
      std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+":"+
      std::to_string(++importInstanceCounter_));
  if(!profile.valid()) {diagnostic="Valores de ambiente inválidos.";return {};}
  const auto serialized=profile.serialize();resources::AssetRecord record;
  record.guid=profile.guid;record.type=resources::AssetType::EnvironmentProfile;record.path=path;
  record.contentHash=Sha256::hex(std::span<const u8>(reinterpret_cast<const u8*>(serialized.data()),serialized.size()));
  if(profile.values.environmentMap.valid()) {
    const auto *map=assets_.find(profile.values.environmentMap);
    if(!map||map->type!=resources::AssetType::EnvironmentMap) {diagnostic="O mapa HDRI não pertence ao projeto.";return {};}
    record.dependencies.push_back(profile.values.environmentMap);
  }
  auto nextAssets=assets_;if(!nextAssets.add(record)) {diagnostic="O registro recusou o perfil.";return {};}
  auto values=*entity;auto *editable=static_cast<scene::Environment*>(values.components.editInstance(instance));
  if(!editable) {diagnostic="O volume mudou durante a criação.";return {};}
  editable->profile=profile.guid;
  auto stagedDocument=document_;auto stagedHistory=history_;
  if(!stagedHistory.applyValues(stagedDocument,id,values)) {diagnostic="O histórico recusou o vínculo do perfil.";return {};}
  // Valide toda a mutação autoral antes de criar o arquivo. Assim uma recusa do
  // histórico não deixa um perfil órfão fora do registro do projeto.
  if(!writeEnvironmentProfile(profile,path,diagnostic)) return {};
  document_=std::move(stagedDocument);history_=std::move(stagedHistory);
  assets_=std::move(nextAssets);assetRegistryDirty_=true;environmentProfiles_.push_back(profile);files_.rebuildTree();
  diagnostic="Perfil criado: "+path;return profile.guid;
}

bool EditorSession::updateEnvironmentProfile(EditorEntityId id,u64 instance,std::string &diagnostic) {
  const auto *entity=document_.find(id);const auto *component=entity?entity->components.findInstance(instance):nullptr;
  const auto *environment=component && &component->type()==&scene::Environment::descriptor?
      static_cast<const scene::Environment*>(component):nullptr;
  const auto *current=environment?findEnvironmentProfile(environment->profile):nullptr;
  if(!environment||!current) {diagnostic="Escolha ou crie um perfil antes de atualizá-lo.";return false;}
  auto candidate=*current;candidate.values=environment->values;
  candidate.values.active=true;candidate.values.priority=0;++candidate.revision;
  if(!commitEnvironmentProfile(candidate,diagnostic)) return false;
  diagnostic="Perfil compartilhado atualizado: "+candidate.name;return true;
}

bool EditorSession::commitSharedMaterial(const resources::MaterialAsset &candidate,std::string &diagnostic,
                                        bool recordHistory,std::string_view field) {
  // Só uma ação do Inspector principal amplia o alvo. Replay e janelas focadas
  // continuam editando exatamente os recursos capturados pelo seu comando.
  std::vector<resources::MaterialAsset> candidates{candidate};
  if(recordHistory && multiAssetEditing_ && candidate.guid==state_.materialInspector && multiMaterials_.size()>1) {
    const auto *before=findMaterialAsset(candidate.guid);
    if(!before) {diagnostic="Material ativo indisponível.";return false;}
    for(const auto &guid:multiMaterials_) if(guid!=candidate.guid) {
      const auto *other=findMaterialAsset(guid);
      if(!other) {diagnostic="Um material da seleção está ilegível; nenhuma alteração aplicada.";return false;}
      auto next=*other;
      copyMaterialChange(*before,candidate,next,field);
      // Inclui os alvos sem diferença: todos precisam passar pela verificação
      // de revisão e do arquivo antes de publicar qualquer mudança.
      next.revision=other->revision+1;candidates.push_back(std::move(next));
    }
  }
  return commitMaterialBatch(candidates,diagnostic,recordHistory);
}

bool EditorSession::commitMaterialBatch(const std::vector<resources::MaterialAsset> &candidates,
                                       std::string &diagnostic,bool recordHistory) {
  diagnostic.clear();
  if(isPlaying()||history_.isOpen()) {diagnostic="Finalize a edição antes de alterar o recurso.";return false;}
  if(candidates.empty()) return true;
  auto nextAssets=assets_;
  std::vector<resources::MaterialAsset> before,after;
  std::vector<std::string> paths,texts,hashes;
  for(const auto &candidate:candidates) {
    const auto *current=findMaterialAsset(candidate.guid);const auto *record=assets_.find(candidate.guid);
    if(!candidate.valid()||!current||!record||record->type!=resources::AssetType::Material||
       current->revision==std::numeric_limits<u32>::max()||candidate.revision!=current->revision+1) {
      diagnostic="Material ou revisão indisponível; reabra o recurso.";return false;
    }
    std::filesystem::path absolute;std::vector<u8> previous;
    if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(files_.rootPath()),record->path,absolute)||
       !EditorImportTransaction::read(absolute,previous)) {
      diagnostic="Arquivo do material indisponível: "+record->path+"; nenhuma alteração aplicada.";return false;
    }
    resources::MaterialAsset onDisk;
    if(!resources::MaterialAsset::deserialize(std::string(previous.begin(),previous.end()),onDisk)||onDisk.serialize()!=current->serialize()) {
      diagnostic="O material mudou no disco: "+record->path+". Reabra o recurso antes de editar.";return false;
    }
    auto same=candidate;same.revision=current->revision;
    if(same.serialize()==current->serialize()) continue;
    std::vector<resources::AssetGuid> dependencies;
    for(const auto &guid:record->dependencies) {
      const auto *dependency=assets_.find(guid);
      if(!dependency) {diagnostic="Dependência não registrada no material.";return false;}
      if(dependency->type!=resources::AssetType::Texture) dependencies.push_back(guid);
    }
    const auto append=[&](resources::AssetGuid guid) {
      if(!guid.valid()||guid==scene::MaterialTextureNone) return true;
      const auto *texture=assets_.find(guid);
      // Desfazer pode restaurar um vínculo originalmente quebrado, sem inventar
      // uma aresta válida no registro.
      if(!texture||texture->type!=resources::AssetType::Texture) return !recordHistory;
      if(std::find(dependencies.begin(),dependencies.end(),guid)==dependencies.end()) dependencies.push_back(guid);
      return true;
    };
    for(const auto &texture:candidate.textures) if(!append(texture)) {
      diagnostic="Textura do material ausente ou com tipo incompatível.";return false;
    }
    if(!append(candidate.occlusionTexture)) {diagnostic="Textura de oclusão ausente ou incompatível.";return false;}
    const auto text=candidate.serialize();
    const std::span<const u8> bytes{reinterpret_cast<const u8*>(text.data()),text.size()};
    if(!nextAssets.publishImport(candidate.guid,Sha256::hex(bytes),record->importerVersion,
        record->importerParameters,record->derived,std::move(dependencies))) {
      diagnostic="Registro recusou a atualização do material.";return false;
    }
    before.push_back(*current);after.push_back(candidate);paths.push_back(record->path);
    texts.push_back(text);hashes.push_back(Sha256::hex(previous));
  }
  if(after.empty()) return true;
  std::vector<EditorImportTransaction::TextEdit> edits;
  for(usize i=0;i<after.size();++i) edits.push_back({paths[i],hashes[i],texts[i]});
  if(!EditorImportTransaction::publishTextBatch(files_.rootPath(),edits,nextAssets.serialize(),diagnostic)) return false;
  assets_=std::move(nextAssets);
  for(const auto &value:after) for(auto &material:materials_) if(material.guid==value.guid) {material=value;break;}
  assetRegistryDirty_=true;publishMaterialLibrary();
  if(recordHistory) {
    const auto project=files_.rootPath();
    history_.recordResource(candidates.size()>1?"Material · "+std::to_string(candidates.size())+" materiais":"Material compartilhado",
        [this,before,after,project](bool forward) {
      if(files_.rootPath()!=project) {state_.status="Recurso do histórico indisponível neste projeto.";return false;}
      std::vector<resources::MaterialAsset> restored;
      for(usize i=0;i<before.size();++i) {
        const auto *current=findMaterialAsset(before[i].guid);
        if(!current) {state_.status="Material do histórico indisponível.";return false;}
        auto expected=forward?before[i]:after[i];expected.revision=current->revision;
        if(expected.serialize()!=current->serialize()) {state_.status="O material mudou; histórico preservado sem sobrescrever.";return false;}
        auto value=forward?after[i]:before[i];value.revision=current->revision+1;restored.push_back(std::move(value));
      }
      std::string error;
      if(!commitMaterialBatch(restored,error,false)) {state_.status=error;return false;}
      if(!ensureTexturesPublished(error)) state_.status="Materiais restaurados; publicação pendente: "+error;
      else state_.status=forward?"Materiais refeitos":"Materiais desfeitos";
      return true;
    });
  }
  return true;
}

bool EditorSession::writeMaterialAsset(const resources::MaterialAsset &material,const std::string &path,std::string &diagnostic) {
  const auto root=files_.rootPath();
  std::filesystem::path absolute;
  if(root.empty() || !EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),path,absolute)) {
    diagnostic="Materiais precisam de um projeto aberto.";return false;
  }
  std::error_code error;std::filesystem::create_directories(absolute.parent_path(),error);
  if(error || !EditorImportTransaction::writeText(absolute,material.serialize())) {
    diagnostic="Não foi possível gravar "+path+".";return false;
  }
  return true;
}

std::string EditorSession::sourceMaterialName(const resources::AssetGuid &draw) const {
  for(const auto &block:importedSources_)
    for(usize i=0;i<block.identities.size() && i<block.draws.size();++i) {
      if(block.identities[i]!=draw) continue;
      const auto index=block.draws[i].materialIndex;
      if(index<block.materialNames.size() && !block.materialNames[index].empty()) return block.materialNames[index];
      return "Material "+std::to_string(index+1);
    }
  return {};
}

resources::AssetGuid EditorSession::createMaterialFromSlot(EditorEntityId id,u32 slot,std::string &diagnostic) {
  diagnostic.clear();
  const auto *entity=document_.find(id);
  const auto *render=entity?meshRenderer(*entity):nullptr;
  if(isPlaying() || history_.isOpen()) {diagnostic="Finalize a edição antes de criar material.";return {};}
  if(!render || slot>=render->slotCount()) {diagnostic="Slot de material inexistente.";return {};}
  resources::MaterialAsset material;
  material.name=sourceMaterialName(render->slotAsset(slot));
  if(material.name.empty()) material.name="Material";
  // Os valores que o usuário VÊ agora viram o recurso: substituição local, ou
  // material já compartilhado, ou o da fonte.
  material.values=scene::withoutResolvedTextures(mapScene_.slotMaterial(*render,slot));
  if(!material.values.enabled && render->slotMesh(slot)) material.values=mapScene_.materialForAsset(render->slotMesh(slot)-1);
  material.values.enabled=true;
  // R4: as texturas que o slot mostra agora (trocadas nesta instância ou já do
  // material compartilhado) vão para o recurso novo.
  for(u32 binding=0;binding<scene::MaterialTextureCount;++binding) material.textures[binding]=mapScene_.slotTexture(*render,slot,binding);
  material.surface=mapScene_.slotSurface(*render,slot);
  for(u32 binding=0;binding<scene::MaterialTextureCount;++binding) material.sampling[binding]=mapScene_.slotSampling(*render,slot,binding);
  material.channels=mapScene_.slotChannels(*render,slot);
  material.occlusionTexture=mapScene_.slotOcclusionTexture(*render,slot);
  std::string stem;
  for(unsigned char c:material.name) stem.push_back(c<32 || c==127 || c=='/' || c=='\\' || c==':' || c=='"' ? '_' : static_cast<char>(c));
  if(stem.empty() || stem=="." || stem=="..") stem="Material";
  std::string path="Materiais/"+stem+".material";
  for(u32 n=2;assets_.findByPath(path) || files_.exists(path);++n) path="Materiais/"+stem+" "+std::to_string(n)+".material";
  material.guid=resources::assetGuidFromSeed("material:"+path+":"+
      std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+":"+std::to_string(++importInstanceCounter_));
  if(!material.valid()) {diagnostic="Valores de material inválidos.";return {};}
  resources::AssetRecord record;
  record.guid=material.guid;record.type=resources::AssetType::Material;record.path=path;
  const auto serialized=material.serialize();
  record.contentHash=Sha256::hex(std::span<const u8>(reinterpret_cast<const u8 *>(serialized.data()),serialized.size()));
  auto nextAssets=assets_;
  if(!nextAssets.add(record)) {diagnostic="O registro recusou o material.";return {};}
  if(!writeMaterialAsset(material,path,diagnostic)) return {};
  assets_=std::move(nextAssets);assetRegistryDirty_=true;
  materials_.push_back(material);
  publishMaterialLibrary();
  files_.rebuildTree();
  // O slot passa a usar o recurso; a substituição local sai, porque os valores
  // dela acabaram de virar o próprio recurso.
  auto values=*entity;auto *edit=editMeshRenderer(values);
  *edit->editSlotMaterialAsset(slot)=material.guid;
  edit->editSlotMaterial(slot)->enabled=false;
  *edit->editSlotTextures(slot)={};
  *edit->editSlotSurface(slot)={};
  *edit->editSlotSampling(slot)={};
  *edit->editSlotChannels(slot)={};
  *edit->editSlotOcclusionTexture(slot)={};
  history_.applyValues(document_,id,values);
  state_.status="Material do projeto criado: "+path;
  return material.guid;
}

bool EditorSession::assignSlotMaterial(EditorEntityId id,u32 slot,const resources::AssetGuid &material) {
  const auto *entity=document_.find(id);
  const auto *render=entity?meshRenderer(*entity):nullptr;
  if(isPlaying() || history_.isOpen() || !render || slot>=render->slotCount()) return false;
  if(material.valid() && !findMaterialAsset(material)) return false;
  auto values=*entity;
  *editMeshRenderer(values)->editSlotMaterialAsset(slot)=material;
  return history_.applyValues(document_,id,values);
}

bool EditorSession::setSlotMaterialValue(EditorEntityId id,u32 slot,MaterialScope scope,u32 field,float value,std::string &diagnostic) {
  diagnostic.clear();
  const auto *entity=document_.find(id);
  const auto *render=entity?meshRenderer(*entity):nullptr;
  if(isPlaying() || history_.isOpen()) {diagnostic="Finalize a edição antes de mudar o material.";return false;}
  const bool assetShared=scope==MaterialScope::Shared && materialAssetMode();
  if((!assetShared && (!render || slot>=render->slotCount())) || field>=scene::meshRendererNumbers.size()) {diagnostic="Campo de material inexistente.";return false;}
  const auto &number=scene::meshRendererNumbers[field];
  if(!std::isfinite(value) || value<number.minimum || value>number.maximum) {diagnostic="Valor fora do intervalo do campo.";return false;}
  if(scope==MaterialScope::Shared) {
    const auto guid=assetShared?state_.materialInspector:render->slotMaterialAsset(slot);
    auto found=std::find_if(materials_.begin(),materials_.end(),[&](const auto &material){return material.guid==guid;});
    if(!guid.valid() || found==materials_.end()) {
      diagnostic="Este slot usa o material da fonte; crie um material do projeto para editar todos os usos.";return false;
    }
    const auto *record=assets_.find(guid);
    if(!record) {diagnostic="Material fora do registro.";return false;}
    auto candidate=*found;
    scene::MeshRenderer probe;probe.material=candidate.values;
    *number.write(probe)=value;
    candidate.values=probe.material;candidate.values.enabled=true;++candidate.revision;
    if(!commitSharedMaterial(candidate,diagnostic,true,"num"+std::to_string(field))) return false;
    state_.status="Material compartilhado atualizado em todos os usos";
    return true;
  }
  auto values=*entity;
  auto *edit=editMeshRenderer(values);
  auto *material=edit->editSlotMaterial(slot);
  if(!material->enabled) {
    // Primeira substituição: parte do que está na tela, não de valores padrão.
    *material=scene::withoutResolvedTextures(mapScene_.slotMaterial(*render,slot));
    if(!material->enabled && render->slotMesh(slot)) *material=mapScene_.materialForAsset(render->slotMesh(slot)-1);
  }
  scene::MeshRenderer probe;probe.material=*material;
  *number.write(probe)=value;
  *material=probe.material;material->enabled=true;
  return history_.applyValues(document_,id,values);
}

bool EditorSession::clearSlotMaterialOverride(EditorEntityId id,u32 slot) {
  const auto *entity=document_.find(id);
  const auto *render=entity?meshRenderer(*entity):nullptr;
  if(isPlaying() || history_.isOpen() || !render || slot>=render->slotCount() || !render->slotMaterial(slot).enabled) return false;
  auto values=*entity;
  auto *edit=editMeshRenderer(values);
  auto *material=edit->editSlotMaterial(slot);
  *material=render->slotMesh(slot)?mapScene_.materialForAsset(render->slotMesh(slot)-1):scene::MaterialParameters{};
  material->enabled=false;
  return history_.applyValues(document_,id,values);
}

bool EditorSession::previousImportMap(const resources::AssetGuid &source,resources::ImportNodeMap &out) const {
  for(const auto &block:importedSources_) if(block.guid==source && block.map.revision) {out=block.map;return true;}
  const auto root=files_.rootPath();
  if(root.empty()) return false;
  std::filesystem::path absolute;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),resources::importNodeMapPath(source),absolute)) return false;
  std::error_code error;
  if(!std::filesystem::exists(absolute,error)) return false;
  std::vector<u8> bytes;
  // Mapa ilegível não trava a importação: as identidades voltam a nascer das
  // chaves da fonte, que é o comportamento de antes do mapa.
  return EditorImportTransaction::read(absolute,bytes,64u*1024u*1024u) &&
         resources::ImportNodeMap::deserialize(std::string_view(reinterpret_cast<const char *>(bytes.data()),bytes.size()),out);
}

bool EditorSession::persistImportMap(const resources::AssetGuid &source) {
  const auto *nodeMap=importNodeMap(source);const auto root=files_.rootPath();
  if(!nodeMap || root.empty()) return false;
  std::filesystem::path absolute;
  if(!EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),resources::importNodeMapPath(source),absolute)) return false;
  const auto text=nodeMap->serialize();
  std::vector<u8> current;
  if(EditorImportTransaction::read(absolute,current,64u*1024u*1024u) && std::string(current.begin(),current.end())==text) return true;
  std::error_code error;std::filesystem::create_directories(absolute.parent_path(),error);
  return !error && EditorImportTransaction::writeText(absolute,text);
}

namespace {
bool readProjectImportProfile(const std::string &root,const std::string &relative,resources::ImportProfile &out) {
  std::filesystem::path absolute;
  if(root.empty() || !EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),relative,absolute)) return false;
  std::error_code error;
  if(!std::filesystem::exists(absolute,error)) return false;
  std::vector<u8> bytes;
  return EditorImportTransaction::read(absolute,bytes,64u*1024u) &&
         resources::parseImportProfile(std::string_view(reinterpret_cast<const char *>(bytes.data()),bytes.size()),out);
}
bool writeProjectImportProfile(const std::string &root,const std::string &relative,const resources::ImportProfile &profile) {
  std::filesystem::path absolute;
  if(root.empty() || !resources::validImportProfile(profile) ||
     !EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),relative,absolute)) return false;
  std::error_code error;std::filesystem::create_directories(absolute.parent_path(),error);
  return !error && EditorImportTransaction::writeText(absolute,resources::serializeImportProfile(profile));
}
} // namespace

resources::ImportProfile EditorSession::projectImportProfile() const {
  resources::ImportProfile profile;
  if(!readProjectImportProfile(files_.rootPath(),resources::ImportProfileDefaultPath,profile)) profile={};
  return profile;
}

resources::ImportProfile EditorSession::importProfileFor(const resources::AssetGuid &source) const {
  resources::ImportProfile profile;
  if(source.valid() && readProjectImportProfile(files_.rootPath(),resources::importProfilePath(source),profile)) return profile;
  return projectImportProfile();
}

resources::ImportProfile EditorSession::importProfileForPath(std::string_view path) const {
  if(!path.empty())
    if(const auto *record=assets_.findByPath(std::string(path))) return importProfileFor(record->guid);
  return newSourceImportProfile();
}

resources::ImportProfile EditorSession::newSourceImportProfile() const {
  // Fonte que ainda não existe no projeto: o padrão do projeto, quando o autor
  // salvou um; senão o preset de importação nova, com ASTC 6x6 (o padrão da
  // Unity no Android). Fontes já publicadas nunca passam por aqui.
  resources::ImportProfile profile;
  if(readProjectImportProfile(files_.rootPath(),resources::ImportProfileDefaultPath,profile)) return profile;
  profile={};
  profile.textureCompression=static_cast<u8>(resources::TextureCompression::Astc6x6);
  // S3: como a Godot (LOD na importação por padrão) e o Optimize Mesh da Unity.
  profile.generateLods=true;
  profile.optimizePolygonOrder=true;
  return profile;
}

bool EditorSession::saveImportProfile(const resources::AssetGuid &source,const resources::ImportProfile &profile) {
  return source.valid() && writeProjectImportProfile(files_.rootPath(),resources::importProfilePath(source),profile);
}

bool EditorSession::saveProjectImportProfile(const resources::ImportProfile &profile) {
  return writeProjectImportProfile(files_.rootPath(),resources::ImportProfileDefaultPath,profile);
}

void EditorSession::beginImportPreparation(std::string_view path,bool batch) {
  state_.importBatch=batch;
  state_.importPanel=true;state_.importReady=false;state_.importError=false;state_.importPage=0;state_.importIntoScene=false;
  state_.importSummary.clear();state_.importPath=std::string(path);state_.importAmbiguities=0;state_.importAmbiguityChoice=0;
  state_.importStatus="Preparando recurso…";state_.importTab=EditorScreenState::ImportTab::Summary;
  state_.importNodes.clear();state_.importTextures.clear();state_.importMeshes.clear();
  state_.importMeshSummary.clear();state_.importMeshSelected=0;state_.importMeshDetail=false;
  state_.importHasExtent=false;state_.importReprepare=false;
  const auto profile=importProfileForPath(path);
  state_.importScale=state_.importPreparedScale=profile.scale;
  state_.importTextureDimension=state_.importPreparedTextureDimension=profile.maximumTextureDimension;
  state_.importNormals=state_.importPreparedNormals=profile.normals;
  state_.importNormalWeighting=state_.importPreparedNormalWeighting=profile.normalWeighting;
  state_.importSmoothingAngle=state_.importPreparedSmoothingAngle=profile.smoothingAngle;
  state_.importTangents=state_.importPreparedTangents=profile.tangents;
  state_.importCameras=state_.importPreparedCameras=profile.importCameras;
  state_.importLights=state_.importPreparedLights=profile.importLights;
  state_.importTextureCompression=state_.importPreparedTextureCompression=profile.textureCompression;
  state_.importGenerateLods=state_.importPreparedGenerateLods=profile.generateLods;
  state_.importLodLevels=state_.importPreparedLodLevels=profile.maximumLodLevels;
  state_.importOptimizeOrder=state_.importPreparedOptimizeOrder=profile.optimizePolygonOrder;
  state_.importTextureStreaming=profile.textureStreaming;
  state_.importTextureStreamingPriority=profile.textureStreamingPriority;
  state_.importAstcSupported=importLimits_.astc4x4;
  state_.importExcludedNodes=profile.excludedNodes;state_.importImpact.clear();
  // O importador é Propriedades: ele precisa estar à vista, inclusive no layout
  // compacto e vindo do workspace de código.
  if(state_.workspace==EditorWorkspace::Code) state_.workspace=EditorWorkspace::Scene;
  state_.inspectorVisible=true;state_.compactPanel=EditorScreenState::CompactPanel::Inspector;
}

void EditorSession::removeImportMapFile(const resources::AssetGuid &source) {
  const auto root=files_.rootPath();
  std::filesystem::path absolute;
  if(root.empty() || !EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),resources::importNodeMapPath(source),absolute)) return;
  std::error_code error;std::filesystem::remove(absolute,error);
  // O perfil acompanha o mapa: é da mesma fonte.
  if(EditorImportTransaction::safePath(EditorImportTransaction::fromUtf8(root),resources::importProfilePath(source),absolute))
    std::filesystem::remove(absolute,error);
}

void EditorSession::reportImportReconcile(const ImportReconcileReport &report,const char *context) {
  if(!report.changed() && !report.conflicts && !report.skippedInstances) return;
  std::string text=std::string(context)+": "+std::to_string(report.updated)+" atualizados · "+std::to_string(report.created)+
      " criados · "+std::to_string(report.removed)+" removidos · "+std::to_string(report.orphaned)+" órfãos · "+
      std::to_string(report.conflicts)+" conflitos";
  if(report.adopted) text+=" · "+std::to_string(report.adopted)+" objetos anteriores vinculados";
  if(report.unproven) text+=" · "+std::to_string(report.unproven)+" sem prova de vínculo";
  const bool attention=report.conflicts || report.orphaned || report.skippedInstances;
  setImportStatus(text,attention?EditorConsoleSeverity::Warning:EditorConsoleSeverity::Info);
  for(const auto &line:report.notes) {
    EditorConsoleEntry entry;entry.origin=EditorConsoleOrigin::Importer;entry.severity=EditorConsoleSeverity::Warning;
    entry.message=line;entry.project=files_.rootPath();console_.add(std::move(entry));
  }
}

void EditorSession::refreshImportLinkView() {
  auto &view=state_.importLink;view={};
  const auto *entity=document_.find(state_.selection);
  const auto *link=entity?scene::importLink(entity->components):nullptr;
  if(!link || link->unlinked) {state_.importLinkMenu=false;return;}
  view.linked=true;view.orphan=link->orphan;view.root=link->root;view.overrides=importOverrides(document_,entity->id);
  if(const auto *record=assets_.find(link->source)) {
    view.source=record->path;
    const auto slash=view.source.find_last_of('/');if(slash!=std::string::npos) view.source.erase(0,slash+1);
  } else view.source="Fonte ausente do registro";
  if(const auto *nodeMap=importNodeMap(link->source)) if(const auto *node=nodeMap->find(link->node)) view.node=node->name;
  if(!link->node.valid()) view.node="instância";
}

bool EditorSession::revertImportLink(EditorEntityId id,u32 mask) {
  if(isPlaying() || history_.isOpen()) return false;
  if(!revertImportOverrides(document_,history_,id,mask,[this](const resources::AssetGuid &guid){return mapScene_.assetSlot(guid);}))
    return false;
  mapScene_.hydrateMaterials(document_);
  return true;
}

u32 EditorSession::unlinkImport(EditorEntityId id) {
  return isPlaying()?0:unlinkImportInstance(document_,history_,id);
}

bool EditorSession::resolveImportOrphan(EditorEntityId id,bool keep) {
  const auto *entity=document_.find(id);
  const auto *link=entity?scene::importLink(entity->components):nullptr;
  if(isPlaying() || !link || !link->orphan || history_.isOpen()) return false;
  if(keep) return unlinkImportObject(document_,history_,id);
  // Apagar é decisão explícita do usuário; o histórico ainda desfaz.
  const bool destroyed=history_.destroyEntity(document_,id);
  if(destroyed) state_.selection=kInvalidEntity;
  return destroyed;
}

bool EditorSession::openComponentPresets(EditorEntityId entity,u64 instance) {
  const auto *object=document_.find(entity);const auto *value=object?object->components.findInstance(instance):nullptr;
  if(!object||(instance&&!value)||isPlaying()) return false;
  std::string error;if(!componentPresets_.load(files_.rootPath(),error)) {state_.status=error;return false;}
  state_.presetPanel=true;state_.presetEntity=entity;state_.presetInstance=instance;
  state_.presetSelected=0;state_.presetPage=0;state_.presetDeleteConfirm=false;state_.presetNaming=false;
  state_.presetInputValues.clear();state_.presetInputIndex=0;state_.presetInputProperty=nullptr;
  state_.presetRecipeDetails=false;state_.presetRecipePage=0;
  state_.presetRenaming=false;state_.presetRecipeNaming=false;
  state_.impactInstance=0;refreshComponentPresets();return true;
}

void EditorSession::refreshComponentPresets() {
  state_.presetChoices.clear();state_.presetPreview.clear();state_.presetFields.clear();state_.presetFieldPage=0;
  state_.presetEpoch=sceneEpoch_;state_.presetRevision=document_.revision();state_.presetSelectedIsRecipe=false;
  state_.presetInputNames.clear();state_.presetRecipeReady=false;
  const auto *object=document_.find(state_.presetEntity);const auto *source=object?object->components.findInstance(state_.presetInstance):nullptr;
  if(!object||(state_.presetInstance&&!source)) {state_.presetPanel=false;return;}
  // Com um componente em contexto, a lista mostra o que serve NELE. Sem
  // componente (painel aberto pelo Adicionar), mostra tudo — inclusive as
  // receitas, que é justamente o caso de "montar um objeto de uma vez".
  for(const auto &entry:componentPresets_.entries) {
    if(source&&!entry.contains(source->type().id)) continue;
    state_.presetChoices.push_back({entry.id,entry.recipe()?entry.name+"  ·  "+std::to_string(entry.entries.size())+" componentes":entry.name});
  }
  if(!state_.presetSelected) return;
  const auto *record=componentPresets_.find(state_.presetSelected);
  state_.presetSelectedIsRecipe=record&&record->recipe();
  if(state_.presetSelectedIsRecipe) {
    state_.presetRecipeName=record->name;state_.presetRecipeCount=static_cast<u32>(record->entries.size());
    state_.presetInputNames=record->inputs;state_.presetInputValues.resize(record->inputs.size());
    // Receita: o preview responde o que vai ser ADICIONADO, o que vai ser
    // ATUALIZADO e o que a receita exige e não traz. Sem essa terceira linha, o
    // autor descobre o requisito externo só quando a transação inteira falha.
    std::string recipeError;auto values=componentPresets_.instantiateAll(state_.presetSelected,recipeError,0,{},true);
    if(values.empty()) {state_.presetPreview.push_back(recipeError);state_.presetRecipeStatus=recipeError;return;}
    for(const auto &value:values) {
      const auto *schema=scene::findComponentSchema(value->type().id);
      const bool present=!value->type().allowMultiple && object->components.find(value->type().id)!=nullptr;
      state_.presetPreview.push_back(std::string(present?"Atualizar: ":"Adicionar: ")+(schema?schema->name:std::string(value->type().id).c_str()));
    }
    for(const auto &value:values) {
      const auto *schema=scene::findComponentSchema(value->type().id);
      if(!schema) continue;
      for(const auto &rule:schema->requirements) {
        bool satisfied=object->components.find(rule.typeId)!=nullptr;
        for(const auto &other:values) if(other->type().id==rule.typeId) satisfied=true;
        if(!satisfied) state_.presetPreview.push_back(std::string("Exige e não traz: ")+rule.message);
      }
    }
    EditorEntity candidate;
    state_.presetRecipeReady=prepareComponentRecipe(state_.presetSelected,object->id,state_.presetInputValues,candidate,recipeError);
    state_.presetRecipeStatus=state_.presetRecipeReady?"Alvos preparados; aplicar grava uma transação":recipeError;
    if(std::any_of(record->entries.begin(),record->entries.end(),[](const auto &e){return std::any_of(e.references.begin(),e.references.end(),[](const auto &r){return !r.input;});}))
      state_.presetPreview.push_back("Referências próprias serão remapeadas ao destino");
    return;
  }
  std::string error;auto preset=componentPresets_.instantiate(state_.presetSelected,error);
  if(!preset) {state_.presetPreview.push_back(error);return;}
  if(!source) {
    const auto plan=scene::planComponentAddition(object->components,preset->type().id,false,false);
    if(!plan.ready) state_.presetPreview.push_back(plan.error);
    else for(const auto type:plan.addedTypes) state_.presetPreview.push_back(std::string("Adicionar: ")+scene::findComponentSchema(type)->name);
    state_.presetPreview.push_back("Novas referências de cena começam vazias");return;
  }
  // O diff vem da reflexão do componente, não de um resumo escrito por tipo.
  // Antes havia um texto fixo para MeshRenderer ("substitui geometria e
  // materiais") justamente porque o resumo genérico não enxergava recurso
  // nenhum: ele percorria números, booleanos e enumerações e parava aí. Agora a
  // mesma lista cobre referência e binding de recurso, e cada linha é uma
  // escolha — o autor leva a rugosidade sem levar a malha.
  const auto delta=scene::componentDelta(*source,*preset);
  u32 changes=0;
  for(const auto &row:delta) {
    if(!row.differs) continue;
    ++changes;
    state_.presetFields.push_back({row.address,std::string(row.label),row.current,row.candidate,true,row.applicable,row.applicable});
  }
  state_.presetPreview.push_back(std::to_string(changes)+(changes==1?" campo diferente":" campos diferentes"));
  u32 blocked=0;
  for(const auto &row:state_.presetFields) if(!row.applicable) ++blocked;
  if(blocked) state_.presetPreview.push_back(std::to_string(blocked)+" não aplicáveis neste objeto");
  if(!changes) state_.presetPreview.push_back("Este preset já é o valor atual");
  // A única diferença que sobrou entre marcar tudo e escolher campo a campo é
  // ESTRUTURAL: substituir o componente pode trocar a quantidade de slots;
  // aplicar valores nunca cria slot, porque isso é mudar a geometria do objeto.
  // Amostragem, canais, superfície e fatores por slot já são endereçáveis.
  if(&preset->type()==&scene::MeshRenderer::descriptor && changes) {
    const auto *mesh=static_cast<const scene::MeshRenderer*>(source);
    const auto *incoming=static_cast<const scene::MeshRenderer*>(preset.get());
    if(mesh->slotCount()!=incoming->slotCount())
      state_.presetPreview.push_back("Marcar tudo também ajusta a quantidade de slots; escolher campo a campo, não");
  }
}

bool EditorSession::saveComponentPreset(EditorEntityId entity,u64 instance,std::string name,std::string &error) {
  if(isPlaying()||history_.isOpen()) {error="Conclua a edição antes de salvar preset";return false;}
  const auto *object=document_.find(entity);const auto *value=object?object->components.findInstance(instance):nullptr;
  if(!value) {error="Componente ausente";return false;}
  auto copy=value->clone();
  if(&copy->type()==&scene::MeshRenderer::descriptor) {
    auto &mesh=static_cast<scene::MeshRenderer&>(*copy);
    for(u32 slot=0;slot<mesh.slotCount();++slot) {
      if(!mesh.slotAsset(slot).valid() && mesh.slotMesh(slot)) *mesh.editSlotAsset(slot)=mapScene_.assetGuid(mesh.slotMesh(slot)-1);
      if(!mesh.slotAsset(slot).valid()) {error="Malha sem identidade persistente; importe antes de salvar preset";return false;}
      *mesh.editSlotMesh(slot)=0;
    }
  }
  return componentPresets_.capture(std::move(name),*copy,error);
}

// Um preset é portátil por identidade; o PROJETO é que precisa ter o recurso.
// A verificação percorre os bindings declarados pelo componente — não há aqui
// uma lista de campos do MeshRenderer escrita à mão — e reconcilia o slot
// resolvido no pacote, que é índice de processo e não identidade.
bool EditorSession::resolveComponentResources(scene::ComponentValue &value,const std::vector<scene::FieldAddress> *only,std::string &error) {
  const auto applies=[&](const scene::FieldAddress &address) {
    if(!only) return true;
    for(const auto &field:*only) if(field==address) return true;
    return false;
  };
  auto *mesh=&value.type()==&scene::MeshRenderer::descriptor?static_cast<scene::MeshRenderer*>(&value):nullptr;
  if(mesh) for(u32 slot=0;slot<mesh->slotCount();++slot) {
    if(!applies({"mesh",slot,scene::FieldKind::Resource})) continue;
    const auto resolved=mapScene_.assetSlot(mesh->slotAsset(slot));
    if(!resolved) {error="Malha do recurso não está carregada neste projeto";return false;}
    *mesh->editSlotMesh(slot)=resolved;
  }
  for(const auto &binding:value.type().resourceBindings) {
    if(!binding.read) continue;
    for(u32 slot=0;slot<binding.slotCount(value);++slot) {
      if(!applies({binding.id,slot,scene::FieldKind::Resource})) continue;
      const auto asset=binding.read(value,slot);
      if(!asset.valid()||binding.declaresNone(asset)) continue;
      if(binding.kind==resources::AssetType::AnimationClip) {
        runtime::AnimationClipView view;
        if(!mapScene_.findClip(asset,view)) {error="Clipe do recurso não está carregado";return false;}
        continue;
      }
      if(binding.kind==resources::AssetType::AnimatorController&&!resources::findAnimatorController(animatorControllers_,asset)) {
        error="Controller do recurso ausente ou inválido";return false;
      }
      if(binding.kind==resources::AssetType::AudioClip && !audioClipAvailable(asset,error)) return false;
      if(binding.kind==resources::AssetType::EnvironmentProfile && !findEnvironmentProfile(asset)) {
        error="Perfil de ambiente do recurso não está carregado";return false;
      }
      if(binding.kind==resources::AssetType::Mesh) {
        // The recipe revision addresses the imported GLB source record. Its
        // baseline_mesh bindings address the independently instantiable meshes.
        // Keep both dependencies portable without treating a source GUID as a
        // renderer slot or silently accepting a missing source.
        if(&value.type()==&scene::CollisionRecipe::descriptor&&binding.id=="bake_source") {
          const auto *record=assets_.find(asset);
          if(!record||record->type!=resources::AssetType::Mesh||std::none_of(importedSources_.begin(),importedSources_.end(),[&](const auto &source){return source.guid==asset;})) {
            error="Revisão da receita não está carregada neste projeto";return false;
          }
          continue;
        }
        // O MeshRenderer já resolveu também o índice transitório acima. Outros
        // componentes, como o Colisor, consomem a identidade diretamente.
        if(&value.type()!=&scene::MeshRenderer::descriptor&&!mapScene_.assetSlot(asset)) {
          error="Malha do recurso não está carregada neste projeto";return false;
        }
        continue;
      }
      if(binding.kind==resources::AssetType::Material&&!mapScene_.sharedMaterial(asset)) {error="Material do recurso indisponível";return false;}
      if(binding.kind==resources::AssetType::Texture) {
        const auto *record=assets_.find(asset);
        if(!record||record->type!=resources::AssetType::Texture||!decodeProjectTexture(asset,true,EditorMapScene::DefaultTextureSampler)) {
          error="Textura do recurso indisponível ou inválida";return false;
        }
      }
      if(binding.kind==resources::AssetType::EnvironmentMap) {
        const auto *record=assets_.find(asset);
        if(!record||record->type!=resources::AssetType::EnvironmentMap||!findEnvironmentMap(asset)) {
          error="Mapa HDRI do recurso indisponível ou inválido";return false;
        }
      }
    }
  }
  return true;
}

bool EditorSession::openRecipeInput(u32 index) {
  const auto *record=componentPresets_.find(state_.presetSelected);const auto *object=document_.find(state_.presetEntity);
  if(!record || !object || index>=record->inputs.size() || state_.presetEpoch!=sceneEpoch_ || state_.presetRevision!=document_.revision())return false;
  std::string error;auto values=componentPresets_.instantiateAll(record->id,error,0,{},true);
  if(values.empty()){state_.status=error;return false;}
  auto candidate=*object;std::vector<const scene::ComponentObjectReference*> properties;
  for(usize i=0;i<values.size();++i) {
    const auto &value=*values[i];const auto *existing=candidate.components.find(value.type().id);u64 target=0;
    if(existing && !value.type().allowMultiple)target=existing->instanceId();
    else {auto plan=scene::planComponentAddition(candidate.components,value.type().id);if(!plan.ready){state_.status=plan.error;return false;}target=plan.requestedInstance;candidate.components=std::move(plan.candidate);}
    if(!candidate.components.replaceInstance(target,value))return false;
    for(const auto &binding:record->entries[i].references)if(binding.input==index+1)
      for(const auto &property:value.type().references)if(property.id==binding.property)properties.push_back(&property);
  }
  if(properties.empty()){state_.status="Entrada de receita sem endereço válido";return false;}
  auto checked=document_;if(!checked.applyEntityValues(object->id,candidate))return false;
  state_.presetInputChoices=editorReferenceChoices(checked,object->id,*properties.front(),{});
  std::erase_if(state_.presetInputChoices,[&](auto id){return std::any_of(properties.begin(),properties.end(),[&](const auto *p){return !editorReferenceAccepts(checked,object->id,*p,id,true);});});
  state_.presetInputIndex=index+1;state_.presetInputProperty=properties.front();state_.componentSelection=object->id;
  state_.referenceInstance=std::numeric_limits<u64>::max();state_.referenceScript=false;state_.referencePage=0;state_.referenceQuery.clear();state_.referenceHighlight=0;
  return true;
}

bool EditorSession::saveComponentRecipe(EditorEntityId entity,std::string name,std::string &error) {
  if(isPlaying()||history_.isOpen()) {error="Conclua a edição antes de salvar preset";return false;}
  const auto *object=document_.find(entity);
  if(!object) {error="Objeto ausente";return false;}
  // A malha precisa da identidade persistente antes de virar preset, senão a
  // receita levaria um índice de pacote que não significa nada em outro projeto.
  auto components=object->components;
  for(usize i=0;i<components.size();++i) {
    auto *value=components.editInstance(components.at(i)->instanceId());
    if(&value->type()!=&scene::MeshRenderer::descriptor) continue;
    auto &mesh=static_cast<scene::MeshRenderer&>(*value);
    for(u32 slot=0;slot<mesh.slotCount();++slot) {
      if(!mesh.slotAsset(slot).valid()&&mesh.slotMesh(slot)) *mesh.editSlotAsset(slot)=mapScene_.assetGuid(mesh.slotMesh(slot)-1);
      if(!mesh.slotAsset(slot).valid()) {error="Malha sem identidade persistente; importe antes de salvar receita";return false;}
      *mesh.editSlotMesh(slot)=0;
    }
  }
  return componentPresets_.captureRecipe(std::move(name),components,error,entity);
}

bool EditorSession::applyComponentRecipe(u64 preset,EditorEntityId entity,EditorSceneVersion expected,std::string &error) {
  return applyComponentRecipe(preset,entity,expected,{},error);
}
bool EditorSession::applyComponentRecipe(u64 preset,EditorEntityId entity,EditorSceneVersion expected,std::span<const u64> inputs,std::string &error) {
  if(isPlaying()||history_.isOpen()||expected.epoch!=sceneEpoch_||expected.revision!=document_.revision()) {error="Cena alterada; selecione novamente o preset";return false;}
  EditorEntity candidate;
  if(!prepareComponentRecipe(preset,entity,inputs,candidate,error))return false;
  const auto before=document_.find(entity)->components.size();
  if(!history_.applyValues(document_,entity,candidate)) {error="Não foi possível aplicar a receita";return false;}
  state_.propertyPage=0;appearanceChanged_=true;
  error="Receita aplicada: "+std::to_string(candidate.components.size()-before)+" componentes adicionados; valores e alvos disponíveis em Desfazer";
  std::string publication;if(!ensureTexturesPublished(publication)) error+="; publicação pendente: "+publication;
  return true;
}
bool EditorSession::prepareComponentRecipe(u64 preset,EditorEntityId entity,std::span<const u64> inputs,EditorEntity &candidate,std::string &error) {
  const auto *object=document_.find(entity);
  if(!object) {error="Objeto ausente";return false;}
  auto values=componentPresets_.instantiateAll(preset,error,entity,inputs);
  if(values.empty()) return false;
  const auto *recipe=componentPresets_.find(preset);
  candidate=*object;usize part=0;std::vector<u64> authoredOrder;
  for(auto &value:values) {
    if(!resolveComponentResources(*value,nullptr,error)) return false;
    const auto *existing=candidate.components.find(value->type().id);
    u64 target=0;
    if(existing&&!value->type().allowMultiple) {
      target=existing->instanceId();
      // O que é da CENA fica na cena: um preset nunca carrega ObjectId, então a
      // referência já escolhida neste objeto é preservada em vez de zerada.
      for(const auto &p:value->type().references) if(p.read&&p.write &&
          std::none_of(recipe->entries[part].references.begin(),recipe->entries[part].references.end(),[&](const auto &r){return r.property==p.id;}))p.write(*value,p.read(*existing));
    } else {
      auto plan=scene::planComponentAddition(candidate.components,value->type().id);
      if(!plan.ready) {error=plan.error;return false;}
      target=plan.requestedInstance;candidate.components=std::move(plan.candidate);
    }
    if(!value->valid()) {error="Valores incompatíveis";return false;}
    if(!candidate.components.replaceInstance(target,*value)) {error="Não foi possível preparar a receita";return false;}
    authoredOrder.push_back(target);
    ++part;
  }
  std::vector<usize> slots;std::vector<u64> desired;
  for(usize i=0;i<candidate.components.size();++i) {
    const auto id=candidate.components.at(i)->instanceId();desired.push_back(id);
    if(std::find(authoredOrder.begin(),authoredOrder.end(),id)!=authoredOrder.end())slots.push_back(i);
  }
  if(slots.size()!=authoredOrder.size()){error="Identidade ambígua na receita";return false;}
  for(usize i=0;i<slots.size();++i)desired[slots[i]]=authoredOrder[i];
  for(usize i=0;i<desired.size();++i)if(!candidate.components.moveInstance(desired[i],i))return false;
  auto checked=document_;
  if(!checked.applyEntityValues(entity,candidate)){error="Composição inválida";return false;}
  for(usize i=0;i<candidate.components.size();++i)if(!editorReferencesAccept(checked,entity,*candidate.components.at(i))) {error="Referências incompatíveis com a composição final";return false;}
  return true;
}

bool EditorSession::applyComponentPreset(u64 preset,EditorEntityId entity,u64 instance,EditorSceneVersion expected,bool add,std::string &error) {
  if(isPlaying()||history_.isOpen()||expected.epoch!=sceneEpoch_||expected.revision!=document_.revision()) {error="Cena alterada; selecione novamente o preset";return false;}
  const auto *object=document_.find(entity);const auto *destination=object?object->components.findInstance(instance):nullptr;
  auto replacement=componentPresets_.instantiate(preset,error);
  if(!object||!replacement||(!add&&(!destination||&destination->type()!=&replacement->type()))) {error="Tipo de preset incompatível";return false;}
  // Portable presets do not copy scene-local object IDs. Applying values keeps
  // the destination's references; a new instance starts with null references.
  if(!add) for(const auto &p:replacement->type().references) if(p.read&&p.write) p.write(*replacement,p.read(*destination));
  // Os campos que vão entrar. Ao ADICIONAR não há destino com que comparar: o
  // preset inteiro vira o componente novo. Ao aplicar valores, entra apenas o
  // que difere E o que o autor deixou marcado no diff — por isso a validação de
  // recurso abaixo pergunta pelos endereços escolhidos, e não pelo preset
  // inteiro: levar a rugosidade não pode ser recusado porque a MALHA do preset
  // não existe neste projeto.
  //
  // Há DOIS alcances, e a diferença é honesta em vez de escondida:
  //
  //  - com tudo marcado, o componente é SUBSTITUÍDO. Isso leva também o que
  //    ainda não é propriedade refletida — a estrutura de slots, a amostragem,
  //    os canais e a superfície por slot que R4 introduziu como dado do
  //    componente sem PropertyId. É o comportamento que sempre existiu;
  //  - com algum campo desmarcado, entram só os campos endereçáveis. Enquanto
  //    aquele estado por slot não tiver identidade de propriedade (tarefa do
  //    bloco de materiais), ele não pode ser escolhido nem recusado linha a
  //    linha, e por isso não viaja no caminho seletivo.
  std::vector<scene::FieldAddress> fields;
  bool selective=false;
  if(!add) {
    const auto delta=scene::componentDelta(*destination,*replacement);
    const bool chosen=state_.presetSelected==preset&&!state_.presetFields.empty();
    for(const auto &row:delta) {
      if(!row.differs||!row.applicable) continue;
      bool marked=true;
      if(chosen) {
        marked=false;
        for(const auto &choice:state_.presetFields) if(choice.address==row.address&&choice.selected) marked=true;
      }
      if(marked) fields.push_back(row.address);
      else selective=true;
    }
    if(fields.empty()) {error="Nenhum campo marcado para aplicar";return false;}
  }
  if(!resolveComponentResources(*replacement,add||!selective?nullptr:&fields,error)) return false;
  if(!replacement->valid()||!editorReferencesAccept(document_,entity,*replacement)) {error="Referências ou valores incompatíveis";return false;}
  auto candidate=*object;u64 target=instance;
  if(add||!selective) {
    if(add) {
      auto plan=scene::planComponentAddition(candidate.components,replacement->type().id);
      if(!plan.ready) {error=plan.error;return false;}
      target=plan.requestedInstance;candidate.components=std::move(plan.candidate);
    }
    if(!candidate.components.replaceInstance(target,*replacement)) {error="Não foi possível preparar o preset";return false;}
  } else {
    const auto outcome=scene::applyComponentFields(candidate.components,*replacement,fields,target);
    if(!outcome.ok()) {error=outcome.error;return false;}
    // O slot resolvido é índice de processo: depois de trocar a IDENTIDADE da
    // malha por campo, ele precisa voltar a apontar para o pacote carregado.
    auto *applied=candidate.components.editInstance(target);
    if(applied&&&applied->type()==&scene::MeshRenderer::descriptor) {
      auto &mesh=static_cast<scene::MeshRenderer&>(*applied);
      for(u32 slot=0;slot<mesh.slotCount();++slot)
        if(auto *resolved=mesh.editSlotMesh(slot)) *resolved=mapScene_.assetSlot(mesh.slotAsset(slot));
    }
  }
  if(!history_.applyValues(document_,entity,candidate)) {error="Não foi possível aplicar o preset";return false;}
  state_.expandedNative=target;state_.propertyPage=0;appearanceChanged_=true;
  error=selective?std::to_string(fields.size())+" campos do preset aplicados; disponível em Desfazer"
                 :std::string("Preset aplicado; disponível em Desfazer");
  if(&replacement->type()==&scene::MeshRenderer::descriptor) {std::string publication;if(!ensureTexturesPublished(publication)) error+="; publicação pendente: "+publication;}
  return true;
}

} // namespace ae::editor

#include "editor/editor_session.h"
#include "scene/component_schema.h"
#include "scene/prefab_link.h"
#include "scene/script_behavior.h"
#include <cmath>
#include <limits>

namespace ae::editor {
bool EditorSession::prepareCharacterConversion(EditorEntityId id,EditorEntity &root,EditorEntity &child,std::string &error) const {
  const auto fail=[&](std::string reason){error=std::move(reason);return false;};
  const auto *source=document_.find(id);
  const auto *mesh=source?runtime::meshRenderer(*source):nullptr;
  if(isPlaying()||history_.isOpen()) return fail("Converta em Edit Mode, após finalizar a edição atual.");
  if(!source||id==document_.root()||!mesh||!mesh->slotCount()) return fail("Selecione uma malha para criar sua raiz Character.");
  for(auto object=source;object;object=document_.find(object->parent)) {
    if(scene::prefabLink(object->components)) return fail("Desvincule a instância de prefab antes de alterar sua estrutura.");
    if(runtime::characterComponent(*object)) return fail("Este objeto já pertence a um Character.");
    if(object!=source) if(const auto *body=runtime::physicsBody(*object);body&&body->motion!=scene::BodyMotion::Static)
      return fail("Um Character não pode herdar a pose de um corpo móvel.");
  }
  std::vector<EditorEntityId> subtree;document_.collectSubtree(id,subtree);
  for(auto member:subtree) {
    const auto *object=document_.find(member);
    if(object->components.find("astra.physics2d.body")) return fail("A conversão não mistura autoridades físicas 2D e 3D.");
    if(member!=id&&(runtime::physicsBody(*object)||runtime::colliderComponent(*object)||runtime::characterComponent(*object)))
      return fail(std::string("Separe a autoridade física do filho ")+object->name+" antes de converter.");
  }
  float world[16],identity[16]{};identity[0]=identity[5]=identity[10]=identity[15]=1;
  EditorTransform worldPose;
  if(!editorWorldMatrix(document_,id,world)||!editorLocalTransformForWorld(world,identity,worldPose))
    return fail("Pose singular ou com cisalhamento: não é possível conservar o visual.");
  float low[3]{INFINITY,INFINITY,INFINITY},high[3]{-INFINITY,-INFINITY,-INFINITY};
  const auto visit=[&](auto consume) {
    for(u32 slot=0;slot<mesh->slotCount();++slot) {
      const auto asset=mesh->slotAsset(slot).valid()?mapScene_.assetSlot(mesh->slotAsset(slot)):mesh->slotMesh(slot);
      std::span<const EditorPickMesh::Triangle> triangles;float relative[16];
      if(!asset||!mapScene_.localGeometry(asset,triangles,relative)||triangles.empty()) return false;
      for(const auto &triangle:triangles) for(u32 vertex=0;vertex<3;++vertex) {
        float local[3],point[3];
        for(u32 axis=0;axis<3;++axis) local[axis]=relative[12+axis]+relative[axis]*triangle[vertex*3]+relative[4+axis]*triangle[vertex*3+1]+relative[8+axis]*triangle[vertex*3+2];
        for(u32 axis=0;axis<3;++axis) point[axis]=world[12+axis]+world[axis]*local[0]+world[4+axis]*local[1]+world[8+axis]*local[2];
        for(float value:point) if(!std::isfinite(value)) return false;
        consume(point);
      }
    }
    return true;
  };
  if(!visit([&](const float *p){for(u32 axis=0;axis<3;++axis){low[axis]=std::min(low[axis],p[axis]);high[axis]=std::max(high[axis],p[axis]);}}))
    return fail("Geometria de um dos slots não está disponível para medir a cápsula.");
  const float centerX=(low[0]+high[0])*.5f,centerZ=(low[2]+high[2])*.5f;
  float radiusSquared=0;
  if(!visit([&](const float *p){const float x=p[0]-centerX,z=p[2]-centerZ;radiusSquared=std::max(radiusSquared,x*x+z*z);}))
    return fail("Não foi possível medir o raio visual.");
  root=EditorEntity{};child=*source;
  while(child.components.remove(scene::Collider::descriptor)){}
  while(child.components.remove(scene::PhysicsBody::descriptor)){}
  child.rigidBodyEnabled=false;
  for(usize i=0;i<child.components.size();++i) if(const auto *schema=scene::findComponentSchema(child.components.at(i)->type().id))
    for(const auto &requirement:schema->requirements) if(!child.components.find(requirement.typeId))
      return fail(std::string(schema->name)+" ainda exige "+std::string(requirement.typeId)+"; resolva essa dependência antes de converter.");
  const auto plan=scene::planComponentAddition(root.components,scene::Character::descriptor.id);
  if(!plan.ready) return fail(plan.error?plan.error:"Character indisponível.");
  root.components=plan.candidate;
  auto *character=runtime::editCharacter(root);
  character->radius=std::sqrt(radiusSquared);
  character->halfHeight=std::max(.01f,(high[1]-low[1])*.5f-character->radius);
  character->eyeHeight=std::max(character->radius+.01f,1.65f*(character->halfHeight+character->radius));
  if(!character->valid()) return fail("A geometria excede os limites da cápsula Character; ajuste a escala antes de converter.");
  root.active=source->active;root.layer=source->layer;root.tag=source->tag;root.groups=source->groups;
  assignEntityName(root,std::string(source->name)+" Character");
  EditorTransform rootWorld;rootWorld.position[0]=centerX;rootWorld.position[1]=low[1];rootWorld.position[2]=centerZ;
  rootWorld.rotationDegrees[1]=std::atan2(world[8],world[10])*57.2957795f;
  float rootMatrix[16],parentMatrix[16];editorTransformMatrix(rootWorld,rootMatrix);
  if(!editorWorldMatrix(document_,source->parent,parentMatrix)||!editorLocalTransformForWorld(rootMatrix,parentMatrix,root.transform)||
     !editorLocalTransformForWorld(world,rootMatrix,child.transform)) return fail("A hierarquia não permite conservar a pose sem cisalhamento.");
  // Preflight the resulting graph, including reference scope changes, before
  // opening history. No real IDs/resources/undo cursor are modified here.
  EditorDocument candidate=document_;
  const auto rootId=candidate.createEntity(source->parent,EditorEntityKind::Folder,root.name);
  if(!rootId||!candidate.applyEntityValues(rootId,root)||!candidate.applyEntityValues(id,child)||!candidate.reparent(id,rootId,0))
    return fail("A composição resultante foi recusada pelo documento.");
  std::vector<EditorEntityId> objects;document_.collectSubtree(document_.root(),objects);
  for(auto owner:objects) for(usize i=0;i<document_.find(owner)->components.size();++i) {
    const auto *component=document_.find(owner)->components.at(i);
    // Removed colliders are not surviving sources of references.
    if(owner==id&&!child.components.findInstance(component->instanceId())) continue;
    for(const auto &reference:component->type().references) {
      const auto target=reference.read(*component);
      const bool required=reference.requiredForExecution&&reference.requiredForExecution(*component);
      if(runtime::referenceAccepts(document_,owner,reference,target,required)&&!runtime::referenceAccepts(candidate,owner,reference,target,required))
        return fail(std::string(document_.find(owner)->name)+" / "+reference.name+": o vínculo deixaria de ser compatível.");
    }
    if(const auto *script=scene::scriptBehavior(component)) for(const auto &property:script->properties) {
      const auto type=scene::scriptArrayElementType(property.valueType).empty()?std::string_view(property.valueType):scene::scriptArrayElementType(property.valueType);
      const auto required=scene::scriptComponentTypeId(type);
      if(required!=scene::PhysicsBody::descriptor.id&&required!=scene::Collider::descriptor.id) continue;
      bool removed=false;scene::forEachScriptPropertyObject(property,[&](u64 target){removed=removed||target==id;});
      if(removed) return fail(std::string(document_.find(owner)->name)+" / "+property.id+": script referencia a autoridade que seria removida.");
    }
  }
  error.clear();return true;
}

EditorEntityId EditorSession::convertToCharacter(EditorEntityId visual) {
  EditorEntity root,child;std::string error;
  if(!prepareCharacterConversion(visual,root,child,error)) {state_.status=error;return kInvalidEntity;}
  const auto parent=document_.find(visual)->parent;u32 index=0;document_.childIndexOf(visual,index);
  if(!history_.begin("Converter para Character")) return kInvalidEntity;
  const auto id=history_.createEntity(document_,parent,EditorEntityKind::Folder,root.name);
  if(!id||!history_.applyValues(document_,id,root)||!history_.applyValues(document_,visual,child)||
     !history_.reparent(document_,id,parent,index)||!history_.reparent(document_,visual,id,0)) {
    history_.cancel(document_);state_.status="Conversão cancelada sem alterar a cena";return kInvalidEntity;
  }
  history_.end();state_.characterConversionTarget=0;state_.inspectorMenu=false;state_.entityMenu=false;
  setSelection(id);state_.componentSelection=id;
  state_.expandedNative=runtime::characterComponent(*document_.find(id))->instanceId();
  state_.expandedComponent.clear();state_.componentGroup.clear();state_.propertyPage=0;state_.addingComponent=false;
  state_.inspectorSurface=EditorInspectorSurface::Inspection;state_.compactPanel=EditorScreenState::CompactPanel::Inspector;
  state_.status="Character criado; visual preservado. Undo restaura Body/Collider e hierarquia.";
  return id;
}
}

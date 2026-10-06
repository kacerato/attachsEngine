#include "harness.h"
#include "editor/editor_session.h"
#include "editor/editor_creation_catalog.h"
#include "editor/editor_archive.h"
#include "renderer/primitive_geometry.h"
#include "runtime/primitive_object.h"
#include "scene/script_behavior.h"
#include "scene/constant_force.h"
#include <sstream>
#include <cmath>
using namespace ae;
namespace {
bool primitives(editor::EditorSession &session) {
  std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  return renderer::appendPrimitiveLibrary(renderer::MapVertexStride,vertices,indices,draws,materials)&&session.importMap(draws,materials,false,vertices,indices,0);
}
std::string components(const runtime::SceneObject &object) {std::ostringstream out;object.components.write(out,true);return out.str();}
u32 characterRecipe() {for(u32 i=0;i<editor::editorCreationCatalog.size();++i)if(editor::editorCreationCatalog[i].id=="physics.character_cylinder")return i;return ~0u;}
}
AE_TEST(character_cylinder_recipe_atomic_history_archive_and_real_motor) {
  editor::EditorSession session;AE_EXPECT_TRUE(primitives(session),"real primitive library");
  const auto recipe=characterRecipe();AE_EXPECT_TRUE(recipe!=~0u&&editor::creationAvailable(session.screen(),recipe),"recipe available only with visual resource");
  const auto depth=session.history().undoDepth();const auto actor=session.createRecipe(recipe,session.document().root());
  auto &g=session.document();AE_EXPECT_TRUE(actor&&runtime::characterComponent(*g.find(actor))&&!runtime::physicsBody(*g.find(actor)),"root has one Character authority");
  const auto children=g.childrenOf(actor);AE_EXPECT_TRUE(children.size()==1,"one actual visual child");const auto visual=children[0];
  AE_EXPECT_TRUE(runtime::meshRenderer(*g.find(visual))&&!runtime::physicsBody(*g.find(visual))&&!runtime::colliderComponent(*g.find(visual))&&std::string(g.find(visual)->name)=="Visual cilíndrico","visual carries real mesh/material, its authored name and no second solver");
  AE_EXPECT_TRUE(session.history().undoDepth()==depth+1&&session.history().undo(g)&&!g.find(actor)&&!g.find(visual),"whole composition undone once");
  AE_EXPECT_TRUE(session.history().redo(g)&&g.find(actor)&&g.find(visual),"IDs and complete composition restored");
  const auto floor=g.createEntity(g.root(),runtime::ObjectKind::Mesh,"Floor");auto value=*g.find(floor);runtime::configurePrimitive(value,scene::PrimitiveType::Cube,{1,session.mapScene().assetGuid(0),session.mapScene().materialForAsset(0)});value.transform.position[1]=-.5f;value.transform.scale[0]=value.transform.scale[2]=30;g.applyEntityValues(floor,value);
  editor::EditorDocument loaded;AE_EXPECT_TRUE(editor::deserializeEditorDocument(editor::serializeEditorDocument(g,0),0,loaded),"recipe persists in native scene format");
  editor::EditorPlayScene play;AE_EXPECT_TRUE(play.start(loaded,session.mapScene()),"existing Jolt Character runtime accepts recipe");
  AE_EXPECT_TRUE(play.setCharacterMove(actor,1,0,0),"real motor receives movement");
  for(int i=0;i<60;++i)AE_EXPECT_TRUE(play.advance(1./60),"solver lifecycle advances");
  float world[16];AE_EXPECT_TRUE(editor::editorWorldMatrix(play.document(),visual,world)&&world[12]>6,"child visual follows motor world pose");
  AE_EXPECT_TRUE(play.world().authorityOf(play.world().handle(actor))==runtime::TransformAuthority::Character&&play.world().authorityOf(play.world().handle(visual))==runtime::TransformAuthority::Free,"single authority survives archive and Play");
}
AE_TEST(character_conversion_preserves_visual_children_references_and_undo) {
  editor::EditorSession session;AE_EXPECT_TRUE(primitives(session),"geometry ready");auto &g=session.document();
  const auto parent=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Parent");auto value=*g.find(parent);value.transform.position[0]=5;value.transform.rotationDegrees[1]=25;g.applyEntityValues(parent,value);
  g.createEntity(parent,runtime::ObjectKind::Folder,"Before");
  const float position[]{7,1,3};const auto visual=session.instantiateAsset(3,parent,position);AE_EXPECT_TRUE(visual,"real cylinder creation path");
  value=*g.find(visual);auto *body=static_cast<scene::PhysicsBody*>(value.components.edit(scene::PhysicsBody::descriptor));body->motion=scene::BodyMotion::Dynamic;body->mass=17;body->velocityX=3;
  auto *mesh=runtime::editMeshRenderer(value);mesh->material.baseColor[0]=.23f;value.transform.rotationDegrees[0]=15;value.transform.scale[2]=-1.2f;g.applyEntityValues(visual,value);
  value=*g.find(visual);value.components.add(scene::Collider::descriptor);g.applyEntityValues(visual,value);
  g.createEntity(parent,runtime::ObjectKind::Folder,"After");
  const auto attachment=g.createEntity(visual,runtime::ObjectKind::Folder,"Attachment");value=*g.find(attachment);value.transform.position[1]=3;g.applyEntityValues(attachment,value);
  const auto camera=g.createEntity(g.root(),runtime::ObjectKind::Camera,"Camera");value=*g.find(camera);value.components.add(scene::Camera::descriptor);auto *follow=static_cast<scene::CameraFollow*>(value.components.add(scene::CameraFollow::descriptor));follow->target=visual;g.applyEntityValues(camera,value);
  const auto old=*g.find(visual);const auto before=components(old);float visualWorld[16],childWorld[16];editor::editorWorldMatrix(g,visual,visualWorld);editor::editorWorldMatrix(g,attachment,childWorld);
  const auto originalSiblings=std::vector<u32>(g.childrenOf(parent).begin(),g.childrenOf(parent).end());const auto depth=session.history().undoDepth();
  const auto actor=session.convertToCharacter(visual);AE_EXPECT_TRUE(actor&&g.find(visual)->parent==actor&&runtime::characterComponent(*g.find(actor)),"wrap with actual Character");
  float converted[16],convertedChild[16];editor::editorWorldMatrix(g,visual,converted);editor::editorWorldMatrix(g,attachment,convertedChild);
  for(u32 i=0;i<16;++i)AE_EXPECT_TRUE(std::abs(converted[i]-visualWorld[i])<.0001f&&std::abs(convertedChild[i]-childWorld[i])<.0001f,"world pose of visual and descendants conserved");
  AE_EXPECT_TRUE(runtime::meshRenderer(*g.find(visual))->instanceId()==runtime::meshRenderer(old)->instanceId()&&runtime::meshRenderer(*g.find(visual))->material==runtime::meshRenderer(old)->material,"original resource/material/component identity stays on original object");
  AE_EXPECT_TRUE(static_cast<const scene::CameraFollow*>(g.find(camera)->components.find(scene::CameraFollow::descriptor))->target==visual,"valid external visual reference preserved");
  AE_EXPECT_TRUE(session.history().undoDepth()==depth+1&&session.history().undo(g),"single undo");
  AE_EXPECT_TRUE(!g.find(actor)&&g.find(visual)->parent==parent&&components(*g.find(visual))==before,"exact Body/Collider config and stable component IDs restored");
  AE_EXPECT_TRUE(std::vector<u32>(g.childrenOf(parent).begin(),g.childrenOf(parent).end())==originalSiblings,"sibling order restored");
  AE_EXPECT_TRUE(session.history().redo(g)&&g.find(visual)->parent==actor,"redo wraps same IDs");
  editor::EditorDocument loaded;AE_EXPECT_TRUE(editor::deserializeEditorDocument(editor::serializeEditorDocument(g,0),0,loaded)&&loaded.find(visual)->parent==actor&&runtime::characterComponent(*loaded.find(actor)),"converted hierarchy survives save/reopen");
}
AE_TEST(character_conversion_rejects_dependencies_references_and_invalid_pose_without_mutation) {
  editor::EditorSession session;AE_EXPECT_TRUE(primitives(session),"real geometry");auto &g=session.document();const float pos[]{0,1,0};const auto id=session.instantiateAsset(3,g.root(),pos);
  auto value=*g.find(id);value.components.add(scene::ConstantForce::descriptor);g.applyEntityValues(id,value);
  const auto before=editor::serializeEditorDocument(g,0);const auto depth=session.history().undoDepth();
  AE_EXPECT_TRUE(!session.convertToCharacter(id)&&editor::serializeEditorDocument(g,0)==before&&session.history().undoDepth()==depth,"Body-dependent force cannot silently survive removal");
  value=*g.find(id);value.components.remove(scene::ConstantForce::descriptor);g.applyEntityValues(id,value);
  const auto scriptOwner=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Script");value=*g.find(scriptOwner);auto *script=static_cast<scene::ScriptBehavior*>(value.components.add(scene::ScriptBehavior::descriptor));script->scriptType="fixture.reference";
  const auto body=runtime::physicsBody(*g.find(id));script->properties.push_back({"body","component:astra.physics.body",scene::scriptComponentValue(id,body->instanceId())});g.applyEntityValues(scriptOwner,value);
  const auto withReference=editor::serializeEditorDocument(g,0);AE_EXPECT_TRUE(!session.convertToCharacter(id)&&editor::serializeEditorDocument(g,0)==withReference,"serialized script component reference blocks conversion");
  value=*g.find(scriptOwner);value.components.remove(scene::ScriptBehavior::descriptor);g.applyEntityValues(scriptOwner,value);
  const auto parent=g.createEntity(g.root(),runtime::ObjectKind::Folder,"Scaled parent");value=*g.find(parent);value.transform.scale[0]=2;g.applyEntityValues(parent,value);
  value=*g.find(id);value.transform.rotationDegrees[1]=45;g.applyEntityValues(id,value);g.reparent(id,parent,0);const auto sheared=editor::serializeEditorDocument(g,0);
  AE_EXPECT_TRUE(!session.convertToCharacter(id)&&editor::serializeEditorDocument(g,0)==sheared,"unrepresentable sheared hierarchy leaves scene unchanged");
}

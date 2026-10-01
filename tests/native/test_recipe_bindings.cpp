#include "harness.h"
#include "editor/editor_session.h"
#include "editor/editor_component_presets.h"
#include "editor/editor_archive.h"
#include "editor/editor_import_transaction.h"
#include "editor/editor_reference_picker.h"
#include "runtime/scene_timers.h"
#include "scene/timer.h"
#include "scene/path.h"
#include "scene/camera.h"
#include "scene/camera_follow.h"
#include "scene/collider.h"
#include "scene/physics_body.h"
#include "ui/ui_font.h"
#include "ui/ui_icon_atlas.h"
#include <chrono>
#include <filesystem>
using namespace ae;using namespace ae::editor;using namespace ae::runtime;
namespace {
struct RecipeFixture {
  std::filesystem::path path=std::filesystem::temp_directory_path()/("astra-recipe-inputs-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  EditorSession session;ObjectId source=0,destination=0,original=0,receiver=0;u64 preset=0;std::string error;
  ui::UiFont font;ui::UiIconAtlas icons;std::vector<u8> fontBytes,iconBytes;
  float width=1200,height=560;
  bool initialize() {
    std::filesystem::create_directories(path);if(!session.setProjectDirectory(path.string().c_str()))return false;
    auto &d=session.document();source=d.createEntity(d.root(),ObjectKind::Folder,"Source");destination=d.createEntity(d.root(),ObjectKind::Folder,"Destination");
    original=d.createEntity(d.root(),ObjectKind::Folder,"Original receiver");receiver=d.createEntity(d.root(),ObjectKind::Folder,"Chosen receiver");
    auto v=*d.find(source);auto *self=static_cast<scene::Timer*>(v.components.add(scene::Timer::descriptor));self->intervalSeconds=.1f;self->elapsedAction=1;self->elapsedTarget=source;
    auto *external=static_cast<scene::Timer*>(v.components.add(scene::Timer::descriptor));external->intervalSeconds=.1f;external->elapsedAction=2;external->elapsedTarget=original;
    auto *follow=static_cast<scene::CameraFollow*>(v.components.add(scene::CameraFollow::descriptor));follow->target=original;v.components.add(scene::Camera::descriptor);
    if(!d.applyEntityValues(source,v) || !session.openComponentPresets(source,0) || !session.saveComponentRecipe(source,"Connected",error))return false;
    if(!session.openComponentPresets(destination,0))return false;
    preset=session.screen().presetChoices.front().first;
    return true;
  }
  bool initializeUi(float w=1200,float h=560) {
    width=w;height=h;
    const auto root=std::filesystem::path(AETHER_REPOSITORY_ROOT);
    if(!EditorImportTransaction::read(root/"assets/astra-visual/ui/astra-ui-font.aeuf",fontBytes)||
       !EditorImportTransaction::read(root/"assets/astra-visual/ui/astra-ui-icons.aeui",iconBytes)||!font.load(fontBytes)||!icons.load(iconBytes))return false;
    session.initialize(&font,&icons);session.setSurface({0,0,width,height},{});session.setSelection(destination);
    if(!session.openComponentPresets(destination,0))return false;
    session.update();return true;
  }
  bool tap(u32 widget) {
    session.update();ui::UiInputRouter router;ui::UiDrawList list;list.begin(session.screen().surface,font.metrics(ui::UiFontWeight::Regular));
    buildEditorScreen(session.screen(),ui::defaultTheme(),list,router);
    for(float y=2;y<height;y+=4)for(float x=2;x<width;x+=4) {
      const auto hit=router.route({99,ui::UiPointerPhase::Down,{x,y},0});router.route({99,ui::UiPointerPhase::Up,{x,y},0});
      if(hit.target==ui::UiPointerTarget::Widget && hit.widgetId==widget) {
        session.handlePointer({77,ui::UiPointerPhase::Down,{x,y},0});session.handlePointer({77,ui::UiPointerPhase::Up,{x,y},0});session.update();return true;
      }
    }
    return false;
  }
  ~RecipeFixture(){std::error_code ec;std::filesystem::remove_all(path,ec);}
};
}
AE_TEST(recipe_bindings_roundtrip_aliases_self_and_actual_runtime_actions) {
  RecipeFixture f;AE_EXPECT_TRUE(f.initialize(),f.error.c_str());EditorComponentPresets reopened;
  AE_EXPECT_TRUE(reopened.load(f.path.string(),f.error),f.error.c_str());const auto *record=reopened.find(f.preset);
  AE_EXPECT_TRUE(record && record->inputs.size()==1 && record->entries[0].references[0].input==0 && record->entries[1].references[0].input==record->entries[2].references[0].input,"self remap and shared external argument survive archive");
  const auto before=serializeEditorDocument(f.session.document(),7);const auto depth=f.session.history().undoDepth();
  AE_EXPECT_TRUE(!f.session.applyComponentRecipe(f.preset,f.destination,f.session.sceneVersion(),f.error),"unbound required input rejected");
  AE_EXPECT_EQ(serializeEditorDocument(f.session.document(),7),before,"missing argument atomic");
  const u64 targets[]{f.receiver};AE_EXPECT_TRUE(f.session.applyComponentRecipe(f.preset,f.destination,f.session.sceneVersion(),targets,f.error),f.error.c_str());
  const auto &components=f.session.document().find(f.destination)->components;
  AE_EXPECT_TRUE(components.at(0)->type().id==scene::Timer::descriptor.id && components.at(2)->type().id==scene::CameraFollow::descriptor.id && components.at(3)->type().id==scene::Camera::descriptor.id,"recipe order survives dependency closure");
  AE_EXPECT_EQ(static_cast<const scene::Timer*>(components.at(0))->elapsedTarget,u64(f.destination),"self remapped to destination");
  AE_EXPECT_EQ(static_cast<const scene::Timer*>(components.at(1))->elapsedTarget,u64(f.receiver),"external timer assigned");
  AE_EXPECT_EQ(static_cast<const scene::CameraFollow*>(components.at(2))->target,u64(f.receiver),"shared external target assigned");
  AE_EXPECT_EQ(f.session.history().undoDepth(),depth+1,"single undo record");const auto after=serializeEditorDocument(f.session.document(),7);
  AE_EXPECT_TRUE(f.session.history().undo(f.session.document()) && serializeEditorDocument(f.session.document(),7)==before,"undo complete composition");
  AE_EXPECT_TRUE(f.session.history().redo(f.session.document()) && serializeEditorDocument(f.session.document(),7)==after,"redo complete composition");
  EditorDocument scene;AE_EXPECT_TRUE(deserializeEditorDocument(after,7,scene),"scene reopened");GameWorld world;AE_EXPECT_TRUE(world.load(scene),"real world");SceneTimers timers;
  AE_EXPECT_TRUE(timers.advance(world,.11,[](ObjectId,u64,u32){return true;}) && !world.activeSelf(world.handle(f.receiver)) && !world.activeSelf(world.handle(f.original)),"original and remapped authored timer consumers execute");
}
AE_TEST(recipe_bindings_validate_self_dependency_and_external_scope_before_commit) {
  RecipeFixture f;AE_EXPECT_TRUE(f.initialize(),f.error.c_str());auto &d=f.session.document();auto v=*d.find(f.source);v.components={};
  v.components.add(scene::PhysicsBody::descriptor);auto *collider=static_cast<scene::Collider*>(v.components.add(scene::Collider::descriptor));collider->owner=f.source;
  AE_EXPECT_TRUE(d.applyEntityValues(f.source,v) && f.session.openComponentPresets(f.source,0) && f.session.saveComponentRecipe(f.source,"Own collider",f.error),f.error.c_str());
  AE_EXPECT_TRUE(f.session.openComponentPresets(f.destination,0),"reload");const auto own=f.session.screen().presetChoices.back().first;
  AE_EXPECT_TRUE(f.session.applyComponentRecipe(own,f.destination,f.session.sceneVersion(),f.error),f.error.c_str());
  AE_EXPECT_EQ(static_cast<const scene::Collider*>(d.find(f.destination)->components.find(scene::Collider::descriptor))->owner,u64(f.destination),"self reference accepts required body added by same transaction");
  const auto empty=d.createEntity(d.root(),ObjectKind::Folder,"Empty receiver");
  const u64 absent[]{65536};const auto before=serializeEditorDocument(d,7);
  AE_EXPECT_TRUE(!f.session.applyComponentRecipe(f.preset,empty,f.session.sceneVersion(),absent,f.error),"missing target rejected");
  AE_EXPECT_EQ(serializeEditorDocument(d,7),before,"reference failure atomic");
  auto version=f.session.sceneVersion();d.createEntity(d.root(),ObjectKind::Folder,"Concurrent");
  const u64 valid[]{f.receiver};AE_EXPECT_TRUE(!f.session.applyComponentRecipe(f.preset,empty,version,valid,f.error),"stale preview rejected");
}
AE_TEST(recipe_bindings_real_pointer_workflow_keeps_scene_unmodified_until_apply) {
  RecipeFixture f;AE_EXPECT_TRUE(f.initialize() && f.initializeUi(853,394),"small landscape regression with actual font/atlas");auto &d=f.session.document();const auto before=serializeEditorDocument(d,7);
  AE_EXPECT_TRUE(f.tap(widgetId(EditorWidget::PresetChoiceBase)),"select actual recipe through router");
  AE_EXPECT_TRUE(f.session.screen().presetInputNames.size()==1 && !f.session.screen().presetRecipeReady,"required argument visible, Apply gated");
  AE_EXPECT_TRUE(f.tap(widgetId(EditorWidget::PresetInputBase)),"open actual object picker");
  const auto choices=editorRecipeReferenceChoices(d,f.session.screen().presetInputChoices,{});const auto at=std::find(choices.begin(),choices.end(),f.receiver);
  AE_EXPECT_TRUE(at!=choices.end(),"compatible receiver exists");
  const auto choice=widgetId(EditorWidget::ReferenceChoiceBase)+static_cast<u32>(at-choices.begin());
  const bool selected=f.tap(choice) || (f.tap(widgetId(EditorWidget::ReferenceNext)) && f.tap(choice));
  AE_EXPECT_TRUE(selected,"choose compatible receiver through actual pagination");
  AE_EXPECT_EQ(serializeEditorDocument(d,7),before,"argument choice does not mutate scene");
  AE_EXPECT_TRUE(f.session.screen().presetRecipeReady && f.tap(widgetId(EditorWidget::PresetApply)),"one real Apply publishes prepared recipe");
  AE_EXPECT_TRUE(d.find(f.destination)->components.size()==4 && f.session.history().undo(d) && serializeEditorDocument(d,7)==before,"pointer workflow and Undo close the chain");
}

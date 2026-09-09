#include "editor/editor_archive.h"
#include "editor/editor_map_scene.h"
#include "editor/editor_history.h"
#include "editor/editor_properties.h"
#include "renderer/environment_lighting.h"
#include "harness.h"
#include <cmath>
#include <cstring>
#include <sstream>
using namespace ae;
using namespace ae::editor;

AE_TEST(editor_v4_water_archive_keeps_original_cascade_inheritance) {
  EditorEntity defaults;
  std::ostringstream legacy;
  legacy<<"AETHER_EDITOR 4 42 1\n1 0 0 \"Scene\" 0 0 0 0 0 0 1 1 1 1 1 1 1 0 0 0 0";
  for(u32 i=9;i<58;++i) legacy<<' '<<editorPropertyValue(defaults,i);
  legacy<<" 1 1\n";
  EditorDocument document;
  AE_EXPECT_TRUE(deserializeEditorDocument(legacy.str(),42,document),"v4 remains loadable");
  AE_EXPECT_TRUE(document.find(document.root())->waterSpectrumEnabled,"existing spectrum preserved");
  AE_EXPECT_TRUE(!document.find(document.root())->waterLayoutEnabled,"no implicit layout change during migration");
}

AE_TEST(editor_water_layout_roundtrips_and_undo_restores_inheritance) {
  EditorDocument document;EditorHistory history;
  auto value=*document.find(document.root());
  AE_EXPECT_TRUE(setEditorPropertyValue(value,58,4),"add fourth cascade");
  AE_EXPECT_TRUE(setEditorPropertyValue(value,59,6),"64 square FFT");
  AE_EXPECT_TRUE(setEditorPropertyValue(value,62,123),"seed edit");
  AE_EXPECT_TRUE(history.applyValues(document,document.root(),value),"history edit");
  const auto saved=serializeEditorDocument(document,9);EditorDocument loaded;
  AE_EXPECT_TRUE(deserializeEditorDocument(saved,9,loaded),"v5 load");
  AE_EXPECT_TRUE(loaded.find(loaded.root())->waterLayoutEnabled,"layout enabled survives");
  AE_EXPECT_EQ(loaded.find(loaded.root())->waterLayout[0],4.0f,"count survives");
  AE_EXPECT_EQ(loaded.find(loaded.root())->waterLayout[4],123.0f,"seed survives");
  AE_EXPECT_TRUE(history.undo(document),"undo configuration");
  AE_EXPECT_TRUE(!document.find(document.root())->waterLayoutEnabled,"inheritance restored");
  AE_EXPECT_TRUE(!setEditorPropertyValue(value,58,2.5f),"fractional count rejected");
  value.waterLayout[2]=value.waterLayout[3];
  AE_EXPECT_TRUE(!document.applyEntityValues(document.root(),value),"empty wavelength interval rejected");
}
namespace {
renderer::MapDrawRecord mesh() {
  renderer::MapDrawRecord draw{};
  draw.model[0]=2;draw.model[5]=3;draw.model[10]=4;draw.model[15]=1;
  draw.model[12]=40;draw.model[13]=-2;draw.boundsCenter[0]=41;draw.boundsCenter[1]=3;
  draw.boundsRadius=12;draw.indexCount=36;return draw;
}
}
AE_TEST(editor_map_import_preserves_original_matrix_and_bounds) {
  auto draw=mesh();EditorDocument doc;EditorMapScene scene;
  AE_EXPECT_TRUE(scene.import(doc,{&draw,1}),"import");
  std::vector<EditorMapUpdate> out;AE_EXPECT_TRUE(scene.extract(doc,out),"extract");
  AE_EXPECT_EQ(out.size(),1u,"one draw");
  AE_EXPECT_TRUE(std::memcmp(out[0].pose.draw.model,draw.model,sizeof(draw.model))==0,"opening must not move geometry");
  AE_EXPECT_EQ(out[0].pose.draw.boundsRadius,draw.boundsRadius,"bounds preserved");
}
AE_TEST(editor_map_mutation_undo_visibility_and_delete_reach_render_state) {
  auto draw=mesh();EditorDocument doc;EditorMapScene scene;EditorHistory history;
  AE_EXPECT_TRUE(scene.import(doc,{&draw,1}),"import");
  const auto id=doc.childrenOf(doc.root())[0];auto value=*doc.find(id);
  value.transform.position[0]+=10;value.visible=false;value.castShadow=false;
  AE_EXPECT_TRUE(history.applyValues(doc,id,value),"edit");
  std::vector<EditorMapUpdate> out;AE_EXPECT_TRUE(scene.extract(doc,out),"extract");
  AE_EXPECT_EQ(out[0].pose.draw.model[12],50.0f,"geometry moved");
  AE_EXPECT_TRUE(!out[0].visible && !out[0].castShadow,"visibility and shadows");
  AE_EXPECT_TRUE(history.undo(doc),"undo");AE_EXPECT_TRUE(scene.extract(doc,out),"extract undo");
  AE_EXPECT_EQ(out[0].pose.draw.model[12],40.0f,"original geometry restored");
  AE_EXPECT_TRUE(out[0].visible,"visibility restored");
  AE_EXPECT_TRUE(history.destroyEntity(doc,id),"delete");AE_EXPECT_TRUE(scene.extract(doc,out),"extract deleted");
  AE_EXPECT_TRUE(!out[0].visible,"deleted geometry hidden");
}
AE_TEST(editor_map_parent_transform_and_visibility_are_inherited) {
  auto draw=mesh();EditorDocument doc;EditorMapScene scene;
  AE_EXPECT_TRUE(scene.import(doc,{&draw,1}),"import");
  auto root=*doc.find(doc.root());root.transform.position[0]=9;root.visible=false;
  AE_EXPECT_TRUE(doc.applyEntityValues(root.id,root),"parent change");
  std::vector<EditorMapUpdate> out;AE_EXPECT_TRUE(scene.extract(doc,out),"extract");
  AE_EXPECT_EQ(out[0].pose.draw.model[12],49.0f,"parent movement");
  AE_EXPECT_TRUE(!out[0].visible,"parent hides descendants");
}
AE_TEST(editor_archive_roundtrip_preserves_values_ids_and_rejects_corruption) {
  EditorDocument doc;const auto id=doc.createEntity(doc.root(),EditorEntityKind::Mesh,"Mesh with \"quotes\"");
  auto value=*doc.find(id);value.assetId=1;value.transform.position[0]=0.123456789f;
  value.transform.rotationDegrees[1]=123.456f;value.visible=false;
  AE_EXPECT_TRUE(doc.applyEntityValues(id,value),"edit");
  const auto text=serializeEditorDocument(doc,42);
  EditorDocument loaded;AE_EXPECT_TRUE(deserializeEditorDocument(text,42,loaded),"roundtrip");
  AE_EXPECT_EQ(serializeEditorDocument(loaded,42),text,"exact stable representation");
  AE_EXPECT_TRUE(!deserializeEditorDocument(text,43,loaded),"foreign package");
  AE_EXPECT_TRUE(!deserializeEditorDocument(text.substr(0,text.size()/2),42,loaded),"truncation");
  AE_EXPECT_TRUE(!deserializeEditorDocument(text+"junk",42,loaded),"trailing garbage");
  AE_EXPECT_EQ(serializeEditorDocument(loaded,42),text,"failed loads leave document intact");
}

AE_TEST(editor_map_duplicate_creates_render_instance_and_undo_removes_it) {
  auto draw=mesh();EditorDocument doc;EditorMapScene scene;EditorHistory history;
  AE_EXPECT_TRUE(scene.import(doc,{&draw,1}),"import");
  const auto original=doc.childrenOf(doc.root())[0];const auto copy=history.duplicateEntity(doc,original);
  AE_EXPECT_TRUE(copy!=0 && copy!=original,"new stable identity");
  std::vector<EditorMapUpdate> out;AE_EXPECT_TRUE(scene.extract(doc,out),"extract");
  AE_EXPECT_EQ(out.size(),2u,"new actual draw");
  AE_EXPECT_TRUE(out[0].visible && out[1].visible,"both copies visible");
  AE_EXPECT_EQ(out[1].sourceDrawIndex,0u,"shared geometry resource");
  AE_EXPECT_TRUE(history.undo(doc),"undo duplication");
  AE_EXPECT_TRUE(scene.extract(doc,out),"extract undone");AE_EXPECT_EQ(out.size(),1u,"clone removed");
  AE_EXPECT_TRUE(history.redo(doc),"redo duplication");
  AE_EXPECT_TRUE(scene.extract(doc,out),"extract redone");AE_EXPECT_EQ(out.size(),2u,"clone restored");
}

AE_TEST(editor_real_test_maps_roundtrip_and_extract_every_draw) {
  const char *paths[]={"samples/ocean/Imported/scene.aemap","samples/dirt-road/Imported/scene.aemap"};
  for(const char *relative:paths) {
    const std::string path=std::string(AETHER_REPOSITORY_ROOT)+"/"+relative;
    FILE *file=std::fopen(path.c_str(),"rb");AE_EXPECT_TRUE(file,"test map exists");
    std::fseek(file,0,SEEK_END);const auto length=std::ftell(file);std::rewind(file);
    std::vector<u8> bytes(static_cast<usize>(length));
    const auto read=std::fread(bytes.data(),1,bytes.size(),file);std::fclose(file);
    AE_EXPECT_EQ(read,bytes.size(),"read map");
    renderer::MapPackageView package;
    AE_EXPECT_TRUE(renderer::decodeMapPackage(bytes,package),"valid real package");
    EditorDocument doc;EditorMapScene scene;
    AE_EXPECT_TRUE(scene.import(doc,package.draws,package.materials),"every package draw imports");
    const auto archive=serializeEditorDocument(doc,package.contentFingerprint);
    EditorDocument restored;AE_EXPECT_TRUE(deserializeEditorDocument(archive,package.contentFingerprint,restored),"saved scene reloads");
    std::vector<EditorMapUpdate> extracted;
    AE_EXPECT_TRUE(scene.extract(restored,extracted),"every reloaded draw extracts");
    AE_EXPECT_EQ(extracted.size(),package.draws.size(),"no missing geometry records");
    for(usize i=0;i<extracted.size();++i) {
      AE_EXPECT_EQ(extracted[i].pose.draw.indexCount,package.draws[i].indexCount,"topology unchanged");
      for(u32 j=0;j<16;++j)
        AE_EXPECT_TRUE(std::abs(extracted[i].pose.draw.model[j]-package.draws[i].model[j])<0.0001f,"matrix preserved");
    }
    std::printf("[Editor real map] %s: %zu draws, %zu archive bytes\n",relative,extracted.size(),archive.size());
  }
}

AE_TEST(editor_material_instance_override_is_independent_and_roundtrips) {
  auto draw=mesh();renderer::MapMaterialRecord material{};
  material.baseColorFactor[0]=.8f;material.baseColorFactor[1]=.6f;material.baseColorFactor[2]=.2f;material.baseColorFactor[3]=.4f;
  material.roughness=.7f;material.metallic=.3f;material.normalScale=1;material.specular=.5f;
  material.flags=renderer::MapMaterialAlphaMask;material.textureIndices[0]=7;
  EditorDocument doc;EditorMapScene scene;EditorHistory history;
  AE_EXPECT_TRUE(scene.import(doc,{&draw,1},{&material,1}),"import with source appearance");
  const auto id=doc.childrenOf(doc.root())[0];const auto copy=history.duplicateEntity(doc,id);
  auto value=*doc.find(copy);AE_EXPECT_TRUE(setEditorPropertyValue(value,12,.1f),"roughness edit");
  AE_EXPECT_TRUE(history.applyValues(doc,copy,value),"record override");
  std::vector<EditorMapUpdate> out;AE_EXPECT_TRUE(scene.extract(doc,out),"extract");
  const auto unchanged=renderer::applyMaterialOverride(material,out[0].material);
  const auto edited=renderer::applyMaterialOverride(material,out[1].material);
  AE_EXPECT_EQ(unchanged.roughness,.7f,"shared source remains unchanged");
  AE_EXPECT_EQ(edited.roughness,.1f,"clone changes");
  AE_EXPECT_EQ(edited.textureIndices[0],7u,"texture remains shared");
  AE_EXPECT_EQ(edited.baseColorFactor[3],.4f,"alpha pipeline contract remains intact");
  const auto saved=serializeEditorDocument(doc,17);EditorDocument restored;
  AE_EXPECT_TRUE(deserializeEditorDocument(saved,17,restored),"v2 roundtrip");
  AE_EXPECT_EQ(restored.find(copy)->material.roughness,.1f,"override persisted");
  AE_EXPECT_TRUE(history.undo(doc),"undo appearance");
  AE_EXPECT_TRUE(!doc.find(copy)->material.enabled,"source material restored");
}
AE_TEST(editor_archive_v1_migrates_neutral_appearance_and_v2_rejects_invalid_values) {
  const std::string legacy="AETHER_EDITOR 1 42 1\n1 0 0 \"Scene\" 0 0 0 0 0 0 1 1 1 1 1 1 1 0 0 0\n";
  EditorDocument doc;AE_EXPECT_TRUE(deserializeEditorDocument(legacy,42,doc),"old scene remains readable");
  const auto *root=doc.find(doc.root());
  AE_EXPECT_EQ(root->environment[0],1.0f,"sun default neutral");
  AE_EXPECT_TRUE(!root->material.enabled,"old materials keep package values");
  auto value=*root;value.material.roughness=-1;
  const auto revision=doc.revision();
  AE_EXPECT_TRUE(!doc.applyEntityValues(value.id,value),"reject malformed appearance before mutation");
  AE_EXPECT_EQ(doc.revision(),revision,"rejection is atomic");
}

AE_TEST(editor_environment_adjustment_uses_source_without_accumulation) {
  renderer::EnvironmentLighting source{},output{};source.sunDirectionIntensity[3]=3;
  source.ambientColorStrength[3]=.5f;source.parameters[0]=2;
  const float values[4]{2,4,.5f,90};
  AE_EXPECT_TRUE(renderer::adjustEnvironmentLighting(source,values,output),"apply");
  AE_EXPECT_EQ(output.sunDirectionIntensity[3],6.0f,"sun power");
  AE_EXPECT_EQ(output.ambientColorStrength[3],2.0f,"ambient power");
  AE_EXPECT_EQ(output.parameters[0],1.0f,"exposure");
  AE_EXPECT_TRUE(std::abs(output.parameters[1]-1.5707963f)<.00001f,"degrees to shader radians");
  AE_EXPECT_TRUE(renderer::adjustEnvironmentLighting(source,values,output),"next frame");
  AE_EXPECT_EQ(output.sunDirectionIntensity[3],6.0f,"not multiplied cumulatively");
}

AE_TEST(editor_reparent_preserves_world_matrix_and_undo_restores_hierarchy) {
  EditorDocument doc;EditorHistory history;
  const auto parent=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Parent");
  const auto child=doc.createEntity(doc.root(),EditorEntityKind::Mesh,"Child");
  auto value=*doc.find(parent);value.transform.position[0]=10;value.transform.rotationDegrees[1]=37;
  value.transform.scale[0]=value.transform.scale[1]=value.transform.scale[2]=2;
  doc.applyEntityValues(parent,value);value=*doc.find(child);value.transform.position[2]=8;doc.applyEntityValues(child,value);
  float before[16],after[16];editorWorldMatrix(doc,child,before);
  AE_EXPECT_TRUE(history.reparentKeepingWorld(doc,child,parent),"reparent");
  editorWorldMatrix(doc,child,after);
  for(u32 i=0;i<16;++i) AE_EXPECT_TRUE(std::abs(before[i]-after[i])<.0001f,"world placement retained");
  AE_EXPECT_EQ(history.undoDepth(),1u,"single transaction");
  AE_EXPECT_TRUE(history.undo(doc),"undo");AE_EXPECT_EQ(doc.find(child)->parent,doc.root(),"old hierarchy restored");
  AE_EXPECT_TRUE(history.redo(doc),"redo");AE_EXPECT_EQ(doc.find(child)->parent,parent,"new parent restored");
}
AE_TEST(editor_reparent_rejects_cycle_without_document_changes) {
  EditorDocument doc;EditorHistory history;const auto parent=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Parent");
  const auto child=doc.createEntity(parent,EditorEntityKind::Folder,"Child");const auto revision=doc.revision();
  AE_EXPECT_TRUE(!history.reparentKeepingWorld(doc,parent,child),"cycle refused");
  AE_EXPECT_EQ(doc.revision(),revision,"unchanged");AE_EXPECT_EQ(history.undoDepth(),0u,"no empty history");
}

AE_TEST(editor_reparent_refuses_unrepresentable_shear_without_mutating) {
  EditorDocument doc;EditorHistory history;
  const auto parent=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Scaled parent");
  const auto child=doc.createEntity(doc.root(),EditorEntityKind::Mesh,"Rotated mesh");
  auto value=*doc.find(parent);value.transform.scale[0]=2;doc.applyEntityValues(parent,value);
  value=*doc.find(child);value.transform.rotationDegrees[2]=45;doc.applyEntityValues(child,value);
  const auto before=serializeEditorDocument(doc,1);
  AE_EXPECT_TRUE(!history.reparentKeepingWorld(doc,child,parent),"shear cannot be represented as local TRS");
  AE_EXPECT_EQ(serializeEditorDocument(doc,1),before,"no approximation or partial edit");
}

AE_TEST(editor_archive_v2_migrates_without_enabling_water_override) {
  const std::string legacy="AETHER_EDITOR 2 42 1\n1 0 0 \"Scene\" 0 0 0 0 0 0 1 1 1 1 1 1 1 0 0 0 0 1 1 1 .5 0 1 1 0 0 0 1 1 1 1 0\n";
  EditorDocument doc;
  AE_EXPECT_TRUE(deserializeEditorDocument(legacy,42,doc),"v2 scene accepted");
  AE_EXPECT_TRUE(!doc.find(doc.root())->waterEnabled,"old water behaviour preserved");
}

AE_TEST(editor_spectrum_properties_roundtrip_and_undo_activation) {
  EditorDocument doc;EditorHistory history;
  auto value=*doc.find(doc.root());
  for(u32 i=37;i<58;++i) {
    const auto &p=editorNumericProperties[i];
    AE_EXPECT_TRUE(setEditorPropertyValue(value,i,(p.minimum+p.maximum)*.5f),"editable spectral field");
  }
  AE_EXPECT_TRUE(history.applyValues(doc,doc.root(),value),"single appearance command");
  AE_EXPECT_TRUE(doc.find(doc.root())->waterSpectrumEnabled,"authoring enabled");
  const auto archive=serializeEditorDocument(doc,42);EditorDocument loaded;
  AE_EXPECT_TRUE(deserializeEditorDocument(archive,42,loaded),"v4 roundtrip");
  for(u32 i=37;i<58;++i)
    AE_EXPECT_EQ(editorPropertyValue(*loaded.find(loaded.root()),i),editorPropertyValue(value,i),"parameter preserved");
  AE_EXPECT_TRUE(loaded.find(loaded.root())->waterSpectrumEnabled,"activation preserved");
  AE_EXPECT_TRUE(history.undo(doc),"undo");
  AE_EXPECT_TRUE(!doc.find(doc.root())->waterSpectrumEnabled,"undo restores runtime inheritance");
  AE_EXPECT_TRUE(history.redo(doc),"redo");
  AE_EXPECT_TRUE(doc.find(doc.root())->waterSpectrumEnabled,"redo reapplies authoring");
  AE_EXPECT_TRUE(!deserializeEditorDocument(archive.substr(0,archive.size()-5),42,loaded),"truncated spectral data rejected");
  AE_EXPECT_EQ(editorPropertyValue(*loaded.find(loaded.root()),37),editorPropertyValue(value,37),"failed load is atomic");
}

AE_TEST(editor_archive_v3_preserves_water_without_enabling_spectrum) {
  const std::string legacy="AETHER_EDITOR 3 42 1\n1 0 0 \"Scene\" 0 0 0 0 0 0 1 1 1 1 1 1 1 0 0 0 0 1 1 1 .5 0 1 1 0 0 0 1 1 1 1 0 3 1 1 1.6 .72 1 1.05 .14 .1 1.333 0 0 1400 1\n";
  EditorDocument doc;
  AE_EXPECT_TRUE(deserializeEditorDocument(legacy,42,doc),"v3 accepted");
  AE_EXPECT_TRUE(doc.find(doc.root())->waterEnabled,"optical override retained");
  AE_EXPECT_TRUE(!doc.find(doc.root())->waterSpectrumEnabled,"spectrum still inherited");
}

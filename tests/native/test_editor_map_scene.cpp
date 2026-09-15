#include "renderer/authoring_geometry.h"
#include "editor/editor_screen.h"
#include "editor/editor_route_component.h"
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
  AE_EXPECT_TRUE(waterSettings(*document.find(document.root())).spectrumEnabled,"existing spectrum preserved");
  AE_EXPECT_TRUE(!waterSettings(*document.find(document.root())).layoutEnabled,"no implicit layout change during migration");
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
  AE_EXPECT_TRUE(waterSettings(*loaded.find(loaded.root())).layoutEnabled,"layout enabled survives");
  AE_EXPECT_EQ(waterSettings(*loaded.find(loaded.root())).legacyField(34+0),4.0f,"count survives");
  AE_EXPECT_EQ(waterSettings(*loaded.find(loaded.root())).legacyField(34+4),123.0f,"seed survives");
  AE_EXPECT_TRUE(history.undo(document),"undo configuration");
  AE_EXPECT_TRUE(!waterSettings(*document.find(document.root())).layoutEnabled,"inheritance restored");
  AE_EXPECT_TRUE(!setEditorPropertyValue(value,58,2.5f),"fractional count rejected");
  editWaterSettings(value)->legacyField(34+2)=editWaterSettings(value)->legacyField(34+3);
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
  auto value=*doc.find(id);editMeshRenderer(value)->mesh=1;value.transform.position[0]=0.123456789f;
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
  AE_EXPECT_EQ(meshMaterial(*restored.find(copy)).roughness,.1f,"override persisted");
  AE_EXPECT_TRUE(history.undo(doc),"undo appearance");
  AE_EXPECT_TRUE(!meshMaterial(*doc.find(copy)).enabled,"source material restored");
}
AE_TEST(editor_archive_v1_migrates_neutral_appearance_and_v2_rejects_invalid_values) {
  const std::string legacy="AETHER_EDITOR 1 42 1\n1 0 0 \"Scene\" 0 0 0 0 0 0 1 1 1 1 1 1 1 0 0 0\n";
  EditorDocument doc;AE_EXPECT_TRUE(deserializeEditorDocument(legacy,42,doc),"old scene remains readable");
  const auto *root=doc.find(doc.root());
  AE_EXPECT_EQ(root->environment[0],1.0f,"sun default neutral");
  AE_EXPECT_TRUE(!meshMaterial(*root).enabled,"old materials keep package values");
  auto value=*root;editMeshRenderer(value)->material.roughness=-1;
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
  AE_EXPECT_TRUE(!waterSettings(*doc.find(doc.root())).enabled,"old water behaviour preserved");
}

AE_TEST(editor_spectrum_properties_roundtrip_and_undo_activation) {
  EditorDocument doc;EditorHistory history;
  auto value=*doc.find(doc.root());
  for(u32 i=37;i<58;++i) {
    const auto &p=editorNumericProperties[i];
    AE_EXPECT_TRUE(setEditorPropertyValue(value,i,(p.minimum+p.maximum)*.5f),"editable spectral field");
  }
  AE_EXPECT_TRUE(history.applyValues(doc,doc.root(),value),"single appearance command");
  AE_EXPECT_TRUE(waterSettings(*doc.find(doc.root())).spectrumEnabled,"authoring enabled");
  const auto archive=serializeEditorDocument(doc,42);EditorDocument loaded;
  AE_EXPECT_TRUE(deserializeEditorDocument(archive,42,loaded),"v4 roundtrip");
  for(u32 i=37;i<58;++i)
    AE_EXPECT_EQ(editorPropertyValue(*loaded.find(loaded.root()),i),editorPropertyValue(value,i),"parameter preserved");
  AE_EXPECT_TRUE(waterSettings(*loaded.find(loaded.root())).spectrumEnabled,"activation preserved");
  AE_EXPECT_TRUE(history.undo(doc),"undo");
  AE_EXPECT_TRUE(!waterSettings(*doc.find(doc.root())).spectrumEnabled,"undo restores runtime inheritance");
  AE_EXPECT_TRUE(history.redo(doc),"redo");
  AE_EXPECT_TRUE(waterSettings(*doc.find(doc.root())).spectrumEnabled,"redo reapplies authoring");
  AE_EXPECT_TRUE(!deserializeEditorDocument(archive.substr(0,archive.size()-5),42,loaded),"truncated spectral data rejected");
  AE_EXPECT_EQ(editorPropertyValue(*loaded.find(loaded.root()),37),editorPropertyValue(value,37),"failed load is atomic");
}

AE_TEST(editor_archive_v3_preserves_water_without_enabling_spectrum) {
  const std::string legacy="AETHER_EDITOR 3 42 1\n1 0 0 \"Scene\" 0 0 0 0 0 0 1 1 1 1 1 1 1 0 0 0 0 1 1 1 .5 0 1 1 0 0 0 1 1 1 1 0 3 1 1 1.6 .72 1 1.05 .14 .1 1.333 0 0 1400 1\n";
  EditorDocument doc;
  AE_EXPECT_TRUE(deserializeEditorDocument(legacy,42,doc),"v3 accepted");
  AE_EXPECT_TRUE(waterSettings(*doc.find(doc.root())).enabled,"optical override retained");
  AE_EXPECT_TRUE(!waterSettings(*doc.find(doc.root())).spectrumEnabled,"spectrum still inherited");
}

AE_TEST(editor_archive_v7_migrates_v6_and_keeps_defaults_stable) {
  // Frozen v6 fixture from the independent authoring validation project.
  const std::string legacy=R"ARCHIVE(AETHER_EDITOR 6 0 2
1 0 0 "Cena" 0 0 0 0 0 0 1 1 1 1 1 1 1 0 0 0 0 1 1 1 0.5 0 1 1 0 0 0 1 1 1 1 0 3 1 1 1.60000002 0.720000029 1 1.04999995 0.140000001 0.100000001 1.33299994 0 0 1400 10 100000 20 0.800000012 0.200000003 0.100000001 0 65 100000 1 0.100000001 0.349999994 1 1 1 1 1 1 0.800000012 4 0.5 3 7 2 2048 1 1 8 1 1 3 0 0 1 1 1 1 50 1 0.5 0.5 0.5 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 0 1 0 0
2 1 0 "GrupoIME" -2.5 0 0 0 0 0 1 1 1 1 1 1 1 0 0 0 0 1 1 1 0.5 0 1 1 0 0 0 1 1 1 1 0 3 1 1 1.60000002 0.720000029 1 1.04999995 0.140000001 0.100000001 1.33299994 0 0 1400 10 100000 20 0.800000012 0.200000003 0.100000001 0 65 100000 1 0.100000001 0.349999994 1 1 1 1 1 1 0.800000012 4 0.5 3 7 2 2048 1 1 8 1 1 3 0 0 1 1 1 1 50 1 0.5 0.5 0.5 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 6 3 1 1 0 0 0 0 0 1 0 0
)ARCHIVE";
  EditorDocument document;
  AE_EXPECT_TRUE(deserializeEditorDocument(legacy,0,document),"read v6");
  const EditorEntity defaults;
  for(u32 i=9;i<editorNumericProperties.size();++i)
    AE_EXPECT_EQ(editorPropertyValue(*document.find(document.root()),i),editorPropertyValue(defaults,i),"v7 omitted defaults remain compatible with v6");
  const auto sparse=serializeEditorDocument(document,0);
  AE_EXPECT_TRUE(sparse.starts_with("AETHER_EDITOR 12 "),"write current version");
  AE_EXPECT_TRUE(sparse.size()<legacy.size()/2,"default arrays are not repeated");
  EditorDocument restored;
  AE_EXPECT_TRUE(deserializeEditorDocument(sparse,0,restored),"read v7");
  AE_EXPECT_EQ(restored.find(2)->transform.position[0],-2.5f,"authored transform preserved");
}

AE_TEST(editor_archive_v7_preserves_disabled_nondefault_properties) {
  EditorDocument doc;auto value=*doc.find(doc.root());
  AE_EXPECT_TRUE(setEditorPropertyValue(value,24,4.5f),"configure water");
  editWaterSettings(value)->enabled=false;editWaterSettings(value)->spectrumEnabled=false;editWaterSettings(value)->layoutEnabled=false;
  AE_EXPECT_TRUE(doc.applyEntityValues(doc.root(),value),"store disabled configuration");
  EditorDocument restored;
  AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(doc,0),0,restored),"roundtrip");
  AE_EXPECT_EQ(editorPropertyValue(*restored.find(restored.root()),24),4.5f,"disabled value retained");
  AE_EXPECT_TRUE(!waterSettings(*restored.find(restored.root())).enabled,"disabled remains disabled");
}

AE_TEST(editor_archive_v7_rejects_duplicate_unknown_and_truncated_properties_transactionally) {
  const std::string prefix="AETHER_EDITOR 7 0 1\n1 0 0 \"Cena\" 0 0 0 0 0 0 1 1 1 1 1 1 1 0 0 0 0 ";
  const std::string suffix=" 0 0 0 0 1 0 0\n";
  EditorDocument doc;const auto before=serializeEditorDocument(doc,0);
  for(const auto &fields:{std::string("2 24 4 24 5"),std::string("1 999999 1"),std::string("1 24"),std::string("1 24 nan"),std::string("1 24 1000")}) {
    AE_EXPECT_TRUE(!deserializeEditorDocument(prefix+fields+suffix,0,doc),"malformed data rejected");
    AE_EXPECT_TRUE(serializeEditorDocument(doc,0)==before,"failure preserves document");
  }
}

AE_TEST(editor_property_accessors_address_named_members_and_isolate_snapshots) {
  EditorEntity source;
  editMeshRenderer(source)->material.baseColor[2]=.25f;
  editMeshRenderer(source)->material.emission[1]=.75f;
  editWaterRoute(source)->points[15].tension=.6f;
  AE_EXPECT_EQ(editorPropertyValue(source,11),.25f,"blue channel");
  AE_EXPECT_EQ(editorPropertyValue(source,17),.75f,"green emission");
  AE_EXPECT_EQ(editorPropertyValue(source,RoutePropertyBase+15*8+7),.6f,"last point tension");
  auto edited=source;
  AE_EXPECT_TRUE(setEditorPropertyValue(edited,9,.8f),"red channel");
  AE_EXPECT_EQ(meshMaterial(edited).baseColor[0],.8f,"typed material destination");
  AE_EXPECT_EQ(meshMaterial(source).baseColor[0],1.0f,"snapshot is unchanged");
  for(u32 point=0;point<renderer::MaximumWaterRoutePoints;++point) {
    const float values[]{float(point+1),-2,3,4,5,-6,7,.8f};
    for(u32 field=0;field<8;++field)
      AE_EXPECT_TRUE(setEditorPropertyValue(edited,RoutePropertyBase+point*8+field,values[field]),"route property");
    const auto &p=waterRoute(edited).points[point];
    AE_EXPECT_EQ(p.position[0],values[0],"point X");
    AE_EXPECT_EQ(p.position[1],-2.0f,"point Y");
    AE_EXPECT_EQ(p.position[2],3.0f,"point Z");
    AE_EXPECT_EQ(p.width,4.0f,"width");
    AE_EXPECT_EQ(p.depth,5.0f,"depth");
    AE_EXPECT_EQ(p.speed,-6.0f,"flow");
    AE_EXPECT_EQ(p.foam,7.0f,"foam");
    AE_EXPECT_EQ(p.tension,.8f,"tension");
    AE_EXPECT_EQ(waterRoute(source).points[point].position[0],0.0f,"source point unchanged");
  }
  AE_EXPECT_TRUE(!setEditorPropertyValue(edited,RoutePropertyBase+15*8+7,2),"reject invalid tension");
  AE_EXPECT_EQ(waterRoute(edited).points[15].tension,.8f,"rejection preserves value");
  AE_EXPECT_TRUE(!setEditorPropertyValue(edited,u32(editorNumericProperties.size()),0),"reject unknown ID");
}

AE_TEST(editor_optional_route_is_absent_for_dry_scene_and_survives_history) {
  EditorDocument doc;EditorHistory history;
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Water,"River");
  auto value=*doc.find(id);
  AE_EXPECT_TRUE(!hasWaterRoute(value),"no points allocated on creation");
  for(u32 i=RoutePropertyBase;i<editorNumericProperties.size();++i) {
    const auto initial=editorPropertyValue(value,i);
    AE_EXPECT_TRUE(setEditorPropertyValue(value,i,initial),"legacy default read");
  }
  AE_EXPECT_TRUE(!hasWaterRoute(value),"default property writes do not allocate");
  EditorDocument dry;
  AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(doc,0),0,dry),"dry roundtrip");
  AE_EXPECT_TRUE(!hasWaterRoute(*dry.find(id)),"dry load does not allocate");
  auto &route=*editWaterRoute(value);route.count=2;route.points[1].position[2]=12;
  AE_EXPECT_TRUE(history.applyValues(doc,id,value),"route attached in history");
  auto snapshot=value;
  route.points[1].position[2]=24;
  AE_EXPECT_EQ(waterRoute(snapshot).points[1].position[2],12.0f,"copy after mutable reference is independent");
  AE_EXPECT_EQ(waterRoute(*doc.find(id)).points[1].position[2],12.0f,"document owns its payload");
  AE_EXPECT_TRUE(history.undo(doc),"undo attachment");
  AE_EXPECT_TRUE(!hasWaterRoute(*doc.find(id)),"undo restores absence");
  AE_EXPECT_TRUE(history.redo(doc),"redo attachment");
  AE_EXPECT_EQ(waterRoute(*doc.find(id)).points[1].position[2],12.0f,"history is independent");
  EditorDocument loaded;
  AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(doc,0),0,loaded),"wet roundtrip");
  AE_EXPECT_EQ(waterRoute(*loaded.find(id)).count,2u,"route restored");
  value=*doc.find(id);value.components.remove(EditorRouteComponent::descriptor);
  AE_EXPECT_TRUE(history.applyValues(doc,id,value),"remove optional payload");
  AE_EXPECT_TRUE(!hasWaterRoute(*doc.find(id)),"payload released");
  AE_EXPECT_TRUE(history.undo(doc),"undo removal");
  AE_EXPECT_EQ(waterRoute(*doc.find(id)).count,2u,"restore owned payload");
  const auto duplicate=history.duplicateEntity(doc,id);
  AE_EXPECT_TRUE(duplicate!=kInvalidEntity,"duplicate optional route");
  value=*doc.find(duplicate);editWaterRoute(value)->points[1].position[2]=30;
  AE_EXPECT_TRUE(history.applyValues(doc,duplicate,value),"edit duplicate");
  AE_EXPECT_EQ(waterRoute(*doc.find(id)).points[1].position[2],12.0f,"original isolated from duplicate");
  AE_EXPECT_TRUE(history.destroyEntity(doc,duplicate),"delete route");
  AE_EXPECT_TRUE(history.undo(doc),"undo deletion");
  AE_EXPECT_EQ(waterRoute(*doc.find(duplicate)).points[1].position[2],30.0f,"deleted payload restored");
  AE_EXPECT_TRUE(sizeof(EditorComponents)<sizeof(renderer::WaterRoute),"empty storage is smaller");
}

namespace {
template<int Index> class ArchiveTestComponent final : public EditorComponentValue {
public:
  int value=0;
  static const EditorComponentType descriptor;
  const EditorComponentType &type() const override { return descriptor; }
  std::unique_ptr<EditorComponentValue> clone() const override { return std::make_unique<ArchiveTestComponent>(*this); }
  bool valid() const override { return value>=0 && value<=100; }
  void write(std::ostream &out) const override { out << value; }
  bool read(std::istream &in,u32 version) override { return version==1 && bool(in>>value); }
};
template<int Index> const EditorComponentType ArchiveTestComponent<Index>::descriptor{
  Index==1?"test.first":"test.second",1,[]() -> std::unique_ptr<EditorComponentValue> { return std::make_unique<ArchiveTestComponent<Index>>(); }
};
}
AE_TEST(editor_registered_components_use_generic_history_and_archive) {
  using First=ArchiveTestComponent<1>;using Second=ArchiveTestComponent<2>;
  const EditorComponentType *const registry[]{&First::descriptor,&Second::descriptor};
  EditorDocument doc;EditorHistory history;auto value=*doc.find(doc.root());
  auto *first=static_cast<First *>(value.components.edit(First::descriptor));
  auto *second=static_cast<Second *>(value.components.edit(Second::descriptor));
  AE_EXPECT_TRUE(first && second,"registered payloads");
  first->value=11;second->value=22;
  AE_EXPECT_TRUE(history.applyValues(doc,doc.root(),value),"generic command");
  first->value=33;
  const auto archive=serializeEditorDocument(doc,0);EditorDocument loaded;
  AE_EXPECT_TRUE(deserializeEditorDocument(archive,0,loaded,registry),"two non-water types roundtrip");
  AE_EXPECT_EQ(static_cast<const First *>(loaded.find(loaded.root())->components.find(First::descriptor))->value,11,"snapshot preserved");
  AE_EXPECT_EQ(static_cast<const Second *>(loaded.find(loaded.root())->components.find(Second::descriptor))->value,22,"second payload preserved");
  const auto before=serializeEditorDocument(loaded,0);
  AE_EXPECT_TRUE(deserializeEditorDocument(archive,0,loaded),"missing extension preserved for recovery");
  AE_EXPECT_TRUE(loaded.find(loaded.root())->components.hasUnresolved(),"missing state is explicit");
  AE_EXPECT_EQ(serializeEditorDocument(loaded,0),before,"unknown payload survives without interpretation");
  AE_EXPECT_TRUE(deserializeEditorDocument(archive,0,loaded,registry),"registration recovers actual types");
  AE_EXPECT_TRUE(history.undo(doc),"generic undo");
  AE_EXPECT_EQ(doc.find(doc.root())->components.size(),usize{0},"undo restores empty collection");
  AE_EXPECT_TRUE(history.redo(doc),"generic redo");
  AE_EXPECT_EQ(serializeEditorDocument(doc,0),archive,"redo restores both types");
  const EditorComponentType *const duplicateRegistry[]{&First::descriptor,&First::descriptor,&Second::descriptor};
  AE_EXPECT_TRUE(!deserializeEditorDocument(archive,0,loaded,duplicateRegistry),"ambiguous registry rejected");
}
AE_TEST(editor_component_records_reject_invalid_duplicate_and_future_payloads) {
  using First=ArchiveTestComponent<1>;
  const EditorComponentType *const registry[]{&First::descriptor};
  EditorComponents values;static_cast<First *>(values.edit(First::descriptor))->value=7;
  for(const char *text:{
      "1 \"test.first\" 2 \"5\"", "1 \"test.first\" 1 \"-1\"",
      "1 \"test.first\" 1 \"5 extra\"", "1 \"unknown\" 1 \"5\"",
      "2 \"test.first\" 1 \"5\" \"test.first\" 1 \"6\"", "65",
      "1 \"test.first\" 1 \""}) {
    std::istringstream input(text);
    AE_EXPECT_TRUE(!values.read(input,registry),"malformed component rejected");
    AE_EXPECT_EQ(static_cast<const First *>(values.find(First::descriptor))->value,7,"transactional rejection");
  }
}
AE_TEST(editor_v7_route_migrates_to_registered_component) {
  const std::string legacy="AETHER_EDITOR 7 0 1\n1 0 0 \"Cena\" 0 0 0 0 0 0 1 1 1 1 1 1 1 0 0 0 0 1 89 12 0 0 0 2 1 0 0\n";
  EditorDocument doc;
  AE_EXPECT_TRUE(deserializeEditorDocument(legacy,0,doc),"legacy route migration");
  AE_EXPECT_TRUE(hasWaterRoute(*doc.find(doc.root())),"registered component created");
  const auto archive=serializeEditorDocument(doc,0);
  AE_EXPECT_TRUE(archive.find("astra.water.route")!=std::string::npos,"stable type ID written");
  EditorDocument restored;
  AE_EXPECT_TRUE(deserializeEditorDocument(archive,0,restored),"component archive reload");
  AE_EXPECT_EQ(waterRoute(*restored.find(restored.root())).points[1].position[2],12.0f,"authored route kept");
  AE_EXPECT_TRUE(!deserializeEditorDocument(legacy,0,restored,{}),"disabled package cannot be bypassed by legacy migration");
}

AE_TEST(editor_water_body_component_metadata_history_and_archive) {
  EditorDocument doc;EditorHistory history;const auto id=doc.createEntity(doc.root(),EditorEntityKind::Water,"Body");
  auto value=*doc.find(id);
  for(u32 i=0;i<waterBodyNumbers.size();++i) {
    const auto index=WaterBodyPropertyBase+i;
    AE_EXPECT_TRUE(setEditorPropertyValue(value,index,editorPropertyValue(value,index)),"default metadata edit");
  }
  AE_EXPECT_EQ(value.components.size(),usize{0},"dry/default reads do not allocate");
  const float authored[]{8,-2,3,.5f,2,0,3};
  for(u32 i=0;i<7;++i) AE_EXPECT_TRUE(setEditorPropertyValue(value,WaterBodyPropertyBase+i,authored[i]),"edit registered metadata");
  AE_EXPECT_TRUE(setWaterBodyFlags(value,false,true),"configure disabled infinite domain");
  AE_EXPECT_TRUE(history.applyValues(doc,id,value),"body command");
  const auto &body=waterBody(*doc.find(id));
  AE_EXPECT_EQ(body.depth,8.0f,"named depth");
  AE_EXPECT_EQ(body.currentX,-2.0f,"named current");
  AE_EXPECT_TRUE(!body.physicsEnabled && body.infinite,"flags");
  auto archive=serializeEditorDocument(doc,0);EditorDocument loaded;
  AE_EXPECT_TRUE(archive.find("astra.water.body")!=std::string::npos,"registered payload");
  AE_EXPECT_TRUE(deserializeEditorDocument(archive,0,loaded),"v8 reload");
  for(u32 i=0;i<7;++i) AE_EXPECT_EQ(editorPropertyValue(*loaded.find(id),WaterBodyPropertyBase+i),authored[i],"metadata preserved");
  AE_EXPECT_TRUE(!waterBody(*loaded.find(id)).physicsEnabled && waterBody(*loaded.find(id)).infinite,"flags roundtrip");
  AE_EXPECT_TRUE(history.undo(doc),"undo");
  AE_EXPECT_EQ(doc.find(id)->components.size(),usize{0},"undo restores absence");
  AE_EXPECT_TRUE(history.redo(doc),"redo");
  AE_EXPECT_EQ(serializeEditorDocument(doc,0),archive,"redo restores payload");
  auto invalid=*doc.find(id);editWaterBody(invalid)->depth=-1;
  const auto revision=doc.revision();
  AE_EXPECT_TRUE(!history.applyValues(doc,id,invalid),"reject invalid direct field");
  AE_EXPECT_EQ(doc.revision(),revision,"no partial command");
}
AE_TEST(editor_old_v8_water_body_migrates_and_duplicate_representation_is_rejected) {
  const std::string prefix="AETHER_EDITOR 8 0 1\n1 0 0 \"Cena\" 0 0 0 0 0 0 1 1 1 1 1 1 1 0 0 0 0 ";
  const std::string old=prefix+"1 67 8 0 0 0 0 0 1 0\n";
  EditorDocument doc;
  AE_EXPECT_TRUE(deserializeEditorDocument(old,0,doc),"old v8 scalar body");
  AE_EXPECT_EQ(waterBody(*doc.find(doc.root())).depth,8.0f,"depth migrated");
  AE_EXPECT_TRUE(!waterBody(*doc.find(doc.root())).physicsEnabled && waterBody(*doc.find(doc.root())).infinite,"flags migrated");
  const auto before=serializeEditorDocument(doc,0);
  const auto conflicting=prefix+"1 67 8 0 0 0 1 0 0 1 \"astra.water.body\" 1 \"1 0 9 0 0 1 1 1 1\"\n";
  AE_EXPECT_TRUE(!deserializeEditorDocument(conflicting,0,doc),"ambiguous legacy and registered body rejected");
  AE_EXPECT_EQ(serializeEditorDocument(doc,0),before,"no data silently overwritten");
  const EditorComponentType *const onlyRoute[]{&EditorRouteComponent::descriptor};
  AE_EXPECT_TRUE(!deserializeEditorDocument(old,0,doc,onlyRoute),"migration respects enabled types");
}

AE_TEST(editor_water_body_toggle_notifies_runtime_and_is_undoable) {
  EditorDocument doc;EditorHistory history;
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Water,"Body");
  EditorScreenState state;state.selection=id;
  ui::UiPointerRouting tap;tap.target=ui::UiPointerTarget::Widget;tap.tapped=true;
  tap.widgetId=widgetId(EditorWidget::ToggleWaterPhysics);
  const auto outcome=applyEditorPointer(state,{},tap,doc,history);
  AE_EXPECT_TRUE(outcome.documentChanged,"consumer must publish new physics state");
  AE_EXPECT_TRUE(!waterBody(*doc.find(id)).physicsEnabled,"toggle applies to component");
  AE_EXPECT_TRUE(history.undo(doc),"undo toggle");
  AE_EXPECT_TRUE(waterBody(*doc.find(id)).physicsEnabled,"default restored");
  AE_EXPECT_EQ(doc.find(id)->components.size(),usize{0},"undo removes newly allocated component");
}

AE_TEST(editor_pick_resources_share_bvh_and_reject_bad_indices_transactionally) {
  std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  AE_EXPECT_TRUE(renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride,vertices,indices,draws,materials),"resource");
  EditorMapScene scene;EditorDocument doc;
  AE_EXPECT_TRUE(scene.import(doc,draws,materials,false,vertices,indices),"import CPU geometry");
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Mesh,"Thin floor");
  auto value=*doc.find(id);editMeshRenderer(value)->mesh=1;value.transform.scale[0]=20;value.transform.scale[1]=.2f;value.transform.scale[2]=20;
  AE_EXPECT_TRUE(doc.applyEntityValues(id,value),"scale floor");
  EditorPickCandidate first;first.id=id;first.radius=30;
  AE_EXPECT_TRUE(scene.pickGeometry(doc,id,first),"pick geometry");
  EditorPickCandidate second;
  AE_EXPECT_TRUE(scene.pickGeometry(doc,id,second),"second candidate");
  AE_EXPECT_TRUE(first.mesh==second.mesh,"shared immutable BVH");
  EditorRay ray;ray.valid=true;ray.origin[1]=2;ray.direction[0]=1;
  AE_EXPECT_TRUE(!pickNearest({&first,1},ray).hit,"ray above thin floor does not hit its huge sphere");
  const auto before=serializeEditorDocument(doc,0);indices[0]=999999;
  AE_EXPECT_TRUE(!scene.import(doc,draws,materials,false,vertices,indices),"invalid topology rejected");
  AE_EXPECT_EQ(serializeEditorDocument(doc,0),before,"document retained");
  AE_EXPECT_TRUE(scene.pickGeometry(doc,id,second) && first.mesh==second.mesh,"old resource retained");
}
#include "editor/editor_play_scene.h"
AE_TEST(editor_play_scene_isolates_mutations_and_restarts_from_authoring) {
  std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  AE_EXPECT_TRUE(renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride,vertices,indices,draws,materials),"resource");
  EditorMapScene resources;EditorDocument doc;
  AE_EXPECT_TRUE(resources.import(doc,draws,materials,false,vertices,indices),"resources");
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Mesh,"Runtime object");
  auto value=*doc.find(id);editMeshRenderer(value)->mesh=1;
  AE_EXPECT_TRUE(doc.applyEntityValues(id,value),"bind resource");
  const auto original=serializeEditorDocument(doc,0);
  EditorPlayScene play;std::vector<renderer::MapDrawState> poses;
  for(int cycle=0;cycle<32;++cycle) {
    AE_EXPECT_TRUE(play.start(doc,resources),"start isolated instance");
    AE_EXPECT_TRUE(!play.start(doc,resources),"no replacement of live instance");
    auto moved=play.document().find(id)->transform;moved.position[1]=7;
    AE_EXPECT_TRUE(play.executionGraph()->setTransform(id,moved),"runtime mutation");
    AE_EXPECT_TRUE(play.extract(resources,poses),"runtime render projection");
    AE_EXPECT_EQ(poses.size(),usize{1},"one object");
    AE_EXPECT_TRUE(std::abs(poses[0].pose.draw.model[13]-7)<.001f,"render consumes runtime transform");
    AE_EXPECT_EQ(serializeEditorDocument(doc,0),original,"authoring never modified");
    play.stop();
    AE_EXPECT_TRUE(!play.active() && !play.executionGraph(),"runtime destroyed");
    AE_EXPECT_TRUE(!play.extract(resources,poses),"stopped instance cannot publish");
  }
}
AE_TEST(scene_physics_component_roundtrip_falls_on_authored_floor_and_stop_restores) {
  std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  AE_EXPECT_TRUE(renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride,vertices,indices,draws,materials),"resource");
  EditorMapScene resources;EditorDocument doc;
  AE_EXPECT_TRUE(resources.import(doc,draws,materials,false,vertices,indices),"import");
  const auto floor=doc.createEntity(doc.root(),EditorEntityKind::Mesh,"Floor");
  auto value=*doc.find(floor);editMeshRenderer(value)->mesh=1;value.transform.position[1]=-.1f;
  value.transform.scale[0]=20;value.transform.scale[1]=.2f;value.transform.scale[2]=20;
  AE_EXPECT_TRUE(editPhysicsBody(value)!=nullptr && editCollider(value)!=nullptr,"explicit static box");
  AE_EXPECT_TRUE(doc.applyEntityValues(floor,value),"floor");
  const auto cube=doc.createEntity(doc.root(),EditorEntityKind::Mesh,"Cube");
  value=*doc.find(cube);editMeshRenderer(value)->mesh=1;value.transform.position[1]=4;
  editPhysicsBody(value)->motion=scene::BodyMotion::Dynamic;editCollider(value);
  AE_EXPECT_TRUE(doc.applyEntityValues(cube,value),"dynamic box");
  const auto saved=serializeEditorDocument(doc,0);EditorDocument loaded;
  AE_EXPECT_TRUE(deserializeEditorDocument(saved,0,loaded),"generic body archive");
  AE_EXPECT_TRUE(physicsBody(*loaded.find(cube))->motion==scene::BodyMotion::Dynamic,"motion restored");
  EditorPlayScene play;
  for(int cycle=0;cycle<100;++cycle) {
    AE_EXPECT_TRUE(play.start(loaded,resources),"start one shared world");
    for(int frame=0;frame<180;++frame) AE_EXPECT_TRUE(play.advance(1.0/60.0),"fixed simulation");
    const float height=play.document().find(cube)->transform.position[1];
    AE_EXPECT_TRUE(height>.45f && height<.55f,"body rests on authored static collider");
    AE_EXPECT_EQ(serializeEditorDocument(loaded,0),saved,"play cannot change authoring");
    play.stop();
  }
}
AE_TEST(editor_v7_rejects_properties_introduced_by_later_component_versions) {
  EditorDocument doc;
  doc.createEntity(doc.root(),EditorEntityKind::Folder,"Preserve");
  const auto before=serializeEditorDocument(doc,0);
  const std::string prefix="AETHER_EDITOR 7 0 1\n1 0 0 \"Cena\" 0 0 0 0 0 0 1 1 1 1 1 1 1 0 0 0 0 1 ";
  for(u32 id:{207u,213u,219u}) {
    const auto legacy=prefix+std::to_string(id)+" 1 0 0 0 0 1 0 0\n";
    AE_EXPECT_TRUE(!deserializeEditorDocument(legacy,0,doc),"v7 cannot introduce newer component properties");
    AE_EXPECT_EQ(serializeEditorDocument(doc,0),before,"invalid migration preserves destination");
  }
}
AE_TEST(editor_water_storage_is_optional_and_legacy_wire_format_remains_unique) {
  EditorDocument doc;
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Mesh,"Dry");
  AE_EXPECT_TRUE(!doc.find(id)->components.find(LegacyWaterSettings::descriptor),"dry entity owns no water payload");
  AE_EXPECT_EQ(waterSettings(*doc.find(id)).density,1400.0f,"default read available for legacy adapters");
  AE_EXPECT_EQ(doc.find(id)->components.size(),usize{0},"default read does not allocate");
  const auto dry=serializeEditorDocument(doc,0);EditorDocument loaded;
  AE_EXPECT_TRUE(deserializeEditorDocument(dry,0,loaded),"dry roundtrip");
  AE_EXPECT_EQ(loaded.find(id)->components.size(),usize{0},"dry load does not allocate legacy water");
  auto value=*doc.find(id);AE_EXPECT_TRUE(setEditorPropertyValue(value,24,4.0f),"legacy wave edit");
  AE_EXPECT_TRUE(doc.applyEntityValues(id,value),"authored optional settings");
  AE_EXPECT_EQ(waterSettings(*doc.find(id)).waveHeight,4.0f,"named field receives legacy property");
  const auto wet=serializeEditorDocument(doc,0);
  AE_EXPECT_TRUE(wet.find("astra.legacy.water-settings")==std::string::npos,"v8 emits legacy scalars once, not duplicate records");
  AE_EXPECT_TRUE(deserializeEditorDocument(wet,0,loaded),"historical encoding migrates into optional storage");
  AE_EXPECT_TRUE(loaded.find(id)->components.find(LegacyWaterSettings::descriptor)!=nullptr,"nondefault settings owned");
  AE_EXPECT_EQ(serializeEditorDocument(loaded,0),wet,"wire format preserved exactly");
  auto copy=*loaded.find(id);editWaterSettings(copy)->waveHeight=8;
  AE_EXPECT_EQ(waterSettings(*loaded.find(id)).waveHeight,4.0f,"copy never aliases water state");
}
AE_TEST(scene_play_repeated_failed_start_releases_partial_world_and_recovers) {
  EditorDocument doc;EditorMapScene resources;
  const auto floor=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Floor");
  auto value=*doc.find(floor);value.transform.position[1]=-.5f;
  editPhysicsBody(value);auto *body=editCollider(value);body->halfX=20;body->halfZ=20;
  AE_EXPECT_TRUE(doc.applyEntityValues(floor,value),"floor");
  const auto actor=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Actor");
  value=*doc.find(actor);editCharacter(value)->speed=2;
  AE_EXPECT_TRUE(doc.applyEntityValues(actor,value),"character");
  EditorDocument invalid=doc;
  const auto unsupported=invalid.createEntity(invalid.root(),EditorEntityKind::Folder,"Scaled character");
  value=*invalid.find(unsupported);editCharacter(value);value.transform.scale[0]=2;
  AE_EXPECT_TRUE(invalid.applyEntityValues(unsupported,value),"valid authoring but unsupported physical scale");
  const auto original=serializeEditorDocument(doc,0);
  EditorPlayScene play;
  for(int cycle=0;cycle<100;++cycle) {
    // Failure occurs after a world, floor and first character have been created.
    AE_EXPECT_TRUE(!play.start(invalid,resources),"partial initialization rejected");
    AE_EXPECT_TRUE(!play.active() && !play.executionGraph(),"failed runtime inaccessible");
    AE_EXPECT_TRUE(!play.advance(1.0/60.0),"failed world cannot advance");
    AE_EXPECT_TRUE(play.start(doc,resources),"retry after partial initialization");
    AE_EXPECT_EQ(play.document().find(actor)->transform.position[0],0.0f,"fresh authored pose");
    for(int tick=0;tick<30;++tick) AE_EXPECT_TRUE(play.advance(1.0/60.0),"settle");
    AE_EXPECT_TRUE(play.jumpCharacter(actor),"fresh ground state");
    AE_EXPECT_TRUE(play.setCharacterMove(actor,1,0,0),"move");
    for(int tick=0;tick<15;++tick) AE_EXPECT_TRUE(play.advance(1.0/60.0),"move and jump");
    AE_EXPECT_TRUE(play.document().find(actor)->transform.position[0]>.3f,"new motor executes input");
    AE_EXPECT_TRUE(play.document().find(actor)->transform.position[1]>.4f,"new motor executes jump");
    play.pause(true);
    play.stop();play.stop();
    AE_EXPECT_TRUE(!play.step() && !play.jumpCharacter(actor),"stopped actions rejected");
    AE_EXPECT_EQ(serializeEditorDocument(doc,0),original,"authoring preserved across failures and execution");
  }
}
AE_TEST(scene_physics_inspector_component_is_undoable) {
  EditorDocument doc;EditorHistory history;
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Mesh,"Box");
  EditorScreenState state;state.selection=id;
  ui::UiPointerRouting tap;tap.target=ui::UiPointerTarget::Widget;tap.tapped=true;
  tap.widgetId=widgetId(EditorWidget::ToggleSceneBody);
  AE_EXPECT_TRUE(applyEditorPointer(state,{},tap,doc,history).documentChanged,"add from inspector");
  AE_EXPECT_TRUE(physicsBody(*doc.find(id))!=nullptr,"component exists");
  tap.widgetId=widgetId(EditorWidget::ToggleDynamicBody);
  AE_EXPECT_TRUE(applyEditorPointer(state,{},tap,doc,history).documentChanged,"motion toggle");
  AE_EXPECT_TRUE(physicsBody(*doc.find(id))->motion==scene::BodyMotion::Dynamic,"dynamic");
  AE_EXPECT_TRUE(history.undo(doc),"undo mode");
  AE_EXPECT_TRUE(physicsBody(*doc.find(id))->motion==scene::BodyMotion::Static,"static restored");
  AE_EXPECT_TRUE(history.undo(doc),"undo add");
  AE_EXPECT_TRUE(!physicsBody(*doc.find(id)),"component removed");
}
AE_TEST(scene_play_pause_freezes_physics_and_step_advances_exactly_once) {
  EditorDocument doc;EditorMapScene resources;
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Body");
  auto value=*doc.find(id);value.transform.position[1]=20;editPhysicsBody(value)->motion=scene::BodyMotion::Dynamic;editCollider(value);
  AE_EXPECT_TRUE(doc.applyEntityValues(id,value),"author body without rendering dependency");
  EditorPlayScene play,reference;
  AE_EXPECT_TRUE(play.start(doc,resources) && reference.start(doc,resources),"independent worlds");
  AE_EXPECT_TRUE(!play.step(),"step requires pause");
  play.pause(true);
  for(int i=0;i<100;++i) AE_EXPECT_TRUE(play.advance(.1),"paused wall frames");
  AE_EXPECT_TRUE(play.document().find(id)->transform.position[1]==20,"paused body does not fall");
  AE_EXPECT_TRUE(play.step() && reference.advance(1.0/60.0),"one step");
  AE_EXPECT_TRUE(play.document().find(id)->transform.position[1]<20,"step changes physical state");
  AE_EXPECT_EQ(play.document().find(id)->transform.position[1],reference.document().find(id)->transform.position[1],"exactly one fixed tick");
  const float pausedY=play.document().find(id)->transform.position[1];
  AE_EXPECT_TRUE(play.advance(30),"wall pause ignored");
  AE_EXPECT_EQ(play.document().find(id)->transform.position[1],pausedY,"step remains paused");
  play.pause(false);
  AE_EXPECT_TRUE(play.advance(1.0/60.0) && reference.advance(1.0/60.0),"resume without catchup");
  AE_EXPECT_EQ(play.document().find(id)->transform.position[1],reference.document().find(id)->transform.position[1],"resume preserves physical time");
}
AE_TEST(authored_character_roundtrips_moves_in_scene_world_and_preserves_authoring) {
  EditorDocument doc;EditorMapScene resources;
  const auto floor=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Floor collider");
  auto value=*doc.find(floor);value.transform.position[1]=-.5f;
  editPhysicsBody(value);auto *body=editCollider(value);body->halfX=20;body->halfY=.5f;body->halfZ=20;
  AE_EXPECT_TRUE(doc.applyEntityValues(floor,value),"scene floor");
  const auto actor=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Character");
  value=*doc.find(actor);value.transform.position[1]=3;editCharacter(value)->speed=2;
  AE_EXPECT_TRUE(doc.applyEntityValues(actor,value),"author character");
  const auto archive=serializeEditorDocument(doc,0);EditorDocument loaded;
  AE_EXPECT_TRUE(deserializeEditorDocument(archive,0,loaded),"load character component");
  AE_EXPECT_EQ(characterComponent(*loaded.find(actor))->speed,2.0f,"configuration restored");
  EditorPlayScene play;
  AE_EXPECT_TRUE(play.start(loaded,resources),"instantiate character in scene world");
  for(int i=0;i<120;++i) AE_EXPECT_TRUE(play.advance(1.0/60.0),"settle on floor");
  AE_EXPECT_TRUE(std::abs(play.document().find(actor)->transform.position[1])<.1f,"feet meet authored floor");
  AE_EXPECT_TRUE(play.setCharacterMove(actor,1,0,0),"move action");
  for(int i=0;i<60;++i) AE_EXPECT_TRUE(play.advance(1.0/60.0),"move fixed ticks");
  const auto x=play.document().find(actor)->transform.position[0];
  AE_EXPECT_TRUE(x>1.8f && x<2.2f,"authored speed drives motion");
  AE_EXPECT_EQ(serializeEditorDocument(loaded,0),archive,"authoring remains intact");
  play.stop();
  AE_EXPECT_TRUE(play.start(loaded,resources),"recreate after stop");
  AE_EXPECT_EQ(play.document().find(actor)->transform.position[0],0.0f,"initial position restored");
}
AE_TEST(authored_character_jump_is_grounded_configurable_and_v1_compatible) {
  EditorCharacter old;std::istringstream payload("0.45 0.55 1.65 8 45");
  AE_EXPECT_TRUE(old.read(payload,1)&&old.valid(),"v1 character remains readable");
  AE_EXPECT_EQ(old.jumpSpeed,5.0f,"migration default");
  EditorDocument doc;EditorMapScene resources;
  const auto floor=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Floor");
  auto value=*doc.find(floor);value.transform.position[1]=-.5f;
  editPhysicsBody(value);auto *body=editCollider(value);body->halfX=20;body->halfY=.5f;body->halfZ=20;
  AE_EXPECT_TRUE(doc.applyEntityValues(floor,value),"floor");
  const auto actor=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Character");
  value=*doc.find(actor);editCharacter(value)->jumpSpeed=4;
  AE_EXPECT_TRUE(doc.applyEntityValues(actor,value),"jump config");
  EditorDocument restored;AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(doc,0),0,restored),"v2 archive");
  AE_EXPECT_EQ(characterComponent(*restored.find(actor))->jumpSpeed,4.0f,"jump speed persists");
  EditorPlayScene play;AE_EXPECT_TRUE(play.start(restored,resources),"play");
  for(int i=0;i<30;++i) AE_EXPECT_TRUE(play.advance(1.0/60.0),"settle");
  AE_EXPECT_TRUE(play.jumpCharacter(actor),"grounded jump accepted");
  AE_EXPECT_TRUE(!play.jumpCharacter(actor),"no duplicate impulse before tick");
  float apex=0;
  for(int i=0;i<90;++i) {
    AE_EXPECT_TRUE(play.advance(1.0/60.0),"jump physics");
    const float y=play.document().find(actor)->transform.position[1];apex=std::max(apex,y);
    if(i==10) AE_EXPECT_TRUE(!play.jumpCharacter(actor),"no air jump");
  }
  AE_EXPECT_TRUE(apex>.7f && apex<1.0f,"height follows authored launch speed and gravity");
  AE_EXPECT_TRUE(std::abs(play.document().find(actor)->transform.position[1])<.1f,"lands on scene collider");
  play.pause(true);AE_EXPECT_TRUE(!play.jumpCharacter(actor),"paused input rejected");
}

#include "editor/editor_scene_camera.h"
AE_TEST(game_camera_follows_runtime_character_hierarchy_without_changing_authoring) {
  EditorDocument doc;EditorMapScene resources;
  const auto floor=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Floor");
  auto value=*doc.find(floor);value.transform.position[1]=-.5f;
  editPhysicsBody(value);auto *body=editCollider(value);body->halfX=20;body->halfY=.5f;body->halfZ=20;
  AE_EXPECT_TRUE(doc.applyEntityValues(floor,value),"floor");
  const auto actor=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Actor");
  value=*doc.find(actor);editCharacter(value)->speed=2;
  AE_EXPECT_TRUE(doc.applyEntityValues(actor,value),"actor");
  const auto camera=doc.createEntity(actor,EditorEntityKind::Camera,"Camera");
  value=*doc.find(camera);editCamera(value);AE_EXPECT_TRUE(doc.applyEntityValues(camera,value),"explicit camera capability");
  auto pose=doc.find(camera)->transform;pose.position[1]=1.65f;pose.rotationDegrees[1]=90;
  AE_EXPECT_TRUE(doc.setTransform(camera,pose),"camera child authored");
  const auto original=serializeEditorDocument(doc,0);
  EditorPlayScene play;AE_EXPECT_TRUE(play.start(doc,resources),"play");
  const auto initial=resolveSceneCamera(play.document());
  AE_EXPECT_EQ(initial.entity,camera,"resolve active child camera");
  AE_EXPECT_TRUE(std::abs(initial.yaw-1.5707963f)<.001f,"authored camera heading");
  AE_EXPECT_TRUE(play.setCharacterMove(actor,0,1,initial.yaw),"forward relative to game camera");
  for(int i=0;i<60;++i) AE_EXPECT_TRUE(play.advance(1.0/60.0),"scene ticks");
  const auto moved=resolveSceneCamera(play.document());
  AE_EXPECT_TRUE(moved.position[0]>1.8f && moved.position[0]<2.2f,"camera follows simulated parent");
  AE_EXPECT_TRUE(std::abs(moved.position[2])<.01f,"camera-relative forward projects on world X");
  AE_EXPECT_TRUE(std::abs(moved.position[1]-1.65f)<.1f,"camera preserves local height");
  AE_EXPECT_EQ(serializeEditorDocument(doc,0),original,"authored camera and actor unchanged");
}
AE_TEST(camera_look_component_is_optional_serialized_and_clamped_in_runtime) {
  EditorDocument doc;EditorMapScene resources;
  const auto rig=doc.createEntity(doc.root(),EditorEntityKind::Folder,"Rig");
  const auto camera=doc.createEntity(rig,EditorEntityKind::Camera,"Camera");
  AE_EXPECT_TRUE(!applyCameraLook(doc,camera,.1f,.1f),"camera alone has no controller");
  auto value=*doc.find(camera);editCamera(value);auto *look=editCameraLook(value);
  look->yawSensitivity=180;look->pitchSensitivity=90;look->pitchLimit=60;
  AE_EXPECT_TRUE(doc.applyEntityValues(camera,value),"author controller");
  const auto saved=serializeEditorDocument(doc,0);EditorDocument loaded;
  AE_EXPECT_TRUE(deserializeEditorDocument(saved,0,loaded),"controller roundtrip");
  EditorPlayScene play;AE_EXPECT_TRUE(play.start(loaded,resources),"isolated play");
  AE_EXPECT_TRUE(applyCameraLook(*play.executionGraph(),camera,.5f,2),"normalized look");
  const auto &transform=play.document().find(camera)->transform;
  AE_EXPECT_EQ(transform.rotationDegrees[1],90.0f,"sensitivity applies");
  AE_EXPECT_EQ(transform.rotationDegrees[0],60.0f,"vertical limit clamps");
  AE_EXPECT_EQ(serializeEditorDocument(loaded,0),saved,"runtime look does not edit authoring");
  AE_EXPECT_TRUE(applyCameraLook(*play.executionGraph(),camera,4,-4),"multiple turns stay bounded");
  AE_EXPECT_EQ(play.document().find(camera)->transform.rotationDegrees[1],90.0f,"yaw wraps without losing heading");
  AE_EXPECT_EQ(play.document().find(camera)->transform.rotationDegrees[0],-60.0f,"lower limit");
}

namespace {
renderer::MapDrawRecord identityDraw(u32 material) {
  renderer::MapDrawRecord draw{};
  draw.model[0]=draw.model[5]=draw.model[10]=draw.model[15]=1;
  draw.boundsRadius=1;draw.indexCount=36;draw.materialIndex=material;
  return draw;
}
renderer::MapMaterialRecord flatMaterial(float red) {
  renderer::MapMaterialRecord material{};
  material.baseColorFactor[0]=red;material.baseColorFactor[1]=material.baseColorFactor[2]=material.baseColorFactor[3]=1;
  return material;
}
} // namespace

AE_TEST(mesh_reference_survives_the_package_being_reordered) {
  // O defeito que a identidade existe para impedir: o pacote é recarregado com
  // os desenhos em outra ordem e a cena passa a apontar para outra malha. Com
  // índice puro isso é silencioso — o objeto simplesmente vira outra coisa.
  const renderer::MapDrawRecord draws[]{identityDraw(0),identityDraw(1)};
  const renderer::MapMaterialRecord materials[]{flatMaterial(1),flatMaterial(0)};
  EditorDocument document;EditorMapScene scene;
  AE_EXPECT_TRUE(scene.import(document,draws,materials,true,{},{},7),"pacote importado");
  const auto ids=document.childrenOf(document.root());
  AE_EXPECT_EQ(ids.size(),2u,"dois objetos");
  const auto segundo=ids[1];
  const auto *render=meshRenderer(*document.find(segundo));
  AE_EXPECT_TRUE(render && render->mesh==2,"slot inicial");
  AE_EXPECT_TRUE(render->asset.valid(),"identidade atribuída na importação");
  const auto identidade=render->asset;
  AE_EXPECT_TRUE(scene.assetGuid(1)==identidade,"a identidade é a do desenho 1");

  const auto salvo=serializeEditorDocument(document,7);

  // O MESMO pacote, com os dois desenhos trocados de lugar.
  const renderer::MapDrawRecord trocados[]{identityDraw(1),identityDraw(0)};
  EditorDocument outro;EditorMapScene reordenado;
  AE_EXPECT_TRUE(reordenado.import(outro,trocados,materials,true,{},{},7),"pacote reordenado");

  EditorDocument carregado;
  AE_EXPECT_TRUE(deserializeEditorDocument(salvo,7,carregado),"cena relida");
  reordenado.reconcileAssets(carregado);
  const auto *reconciliado=meshRenderer(*carregado.find(segundo));
  AE_EXPECT_TRUE(reconciliado!=nullptr,"malha preservada");
  AE_EXPECT_TRUE(reconciliado->asset==identidade,"identidade preservada");
  // O slot mudou porque o pacote mudou. A identidade é que não mudou, e é ela
  // que diz qual desenho é o certo agora.
  AE_EXPECT_EQ(reconciliado->mesh,2u,"o slot foi recalculado pela identidade");
  AE_EXPECT_TRUE(reordenado.assetGuid(reconciliado->mesh-1)==identidade,"o slot aponta para a mesma malha");
}

AE_TEST(mesh_reference_without_identity_is_adopted_and_missing_identity_is_not_faked) {
  const renderer::MapDrawRecord draws[]{identityDraw(0)};
  const renderer::MapMaterialRecord materials[]{flatMaterial(1)};
  EditorDocument document;EditorMapScene scene;
  AE_EXPECT_TRUE(scene.import(document,draws,materials,true,{},{},3),"pacote importado");
  const auto id=document.childrenOf(document.root())[0];

  // Cena anterior ao registro: slot válido, identidade ausente.
  auto legado=*document.find(id);
  editMeshRenderer(legado)->asset={};
  AE_EXPECT_TRUE(document.applyEntityValues(id,legado),"cena sem identidade");
  AE_EXPECT_TRUE(!meshRenderer(*document.find(id))->asset.valid(),"sem identidade antes");
  scene.reconcileAssets(document);
  AE_EXPECT_TRUE(meshRenderer(*document.find(id))->asset==scene.assetGuid(0),
                 "a identidade derivada do slot é adotada");

  // Identidade que este pacote não tem: o slot vai a zero. Apontar para a malha
  // que por acaso ocupa o índice antigo seria corromper a cena em silêncio.
  auto ausente=*document.find(id);
  editMeshRenderer(ausente)->asset=resources::assetGuidFromSeed("pacote:999:0");
  AE_EXPECT_TRUE(document.applyEntityValues(id,ausente),"identidade de outro pacote");
  scene.reconcileAssets(document);
  AE_EXPECT_EQ(meshRenderer(*document.find(id))->mesh,0u,"referência ausente vira slot zero");
}

AE_TEST(mesh_component_v1_archive_still_loads_and_gains_identity_on_save) {
  // Um projeto salvo antes desta mudança precisa abrir. O componente é v4 (slots
  // e texturas por slot) e a carga de v1 não pode exigir nenhum campo posterior.
  const renderer::MapDrawRecord draws[]{identityDraw(0)};
  const renderer::MapMaterialRecord materials[]{flatMaterial(1)};
  EditorDocument document;EditorMapScene scene;
  AE_EXPECT_TRUE(scene.import(document,draws,materials,true,{},{},11),"pacote importado");
  const auto id=document.childrenOf(document.root())[0];

  auto texto=serializeEditorDocument(document,11);
  // Rebaixa o componente para a versão 1 e corta tudo o que veio depois dela —
  // identidade (v2), material compartilhado e contagem de slots (v3), texturas
  // do slot (v4) —, exatamente como um arquivo gravado antes dessas mudanças.
  const std::string alvo="\"astra.render.mesh\" 8 ";
  const auto posicao=texto.find(alvo);
  AE_EXPECT_TRUE(posicao!=std::string::npos,"componente encontrado no arquivo");
  texto.replace(posicao,alvo.size(),"\"astra.render.mesh\" 1 ");
  const auto guid=meshRenderer(*document.find(id))->asset.text();
  const std::string cauda=" - 0 - - - - 0 0 0.5 000 000 000 000 0 0 1 1 0 0 0 1 1 0 0 0 1 1 0 0 0 1 1 0 0 0 0 0 -1 0 0 - ";
  const auto comGuid=texto.find(guid+cauda);
  AE_EXPECT_TRUE(comGuid!=std::string::npos,"identidade e caudas v3/v4 presentes no arquivo");
  texto.erase(comGuid,guid.size()+cauda.size());

  EditorDocument antigo;
  AE_EXPECT_TRUE(deserializeEditorDocument(texto,11,antigo),"arquivo v1 do componente carrega");
  const auto *render=meshRenderer(*antigo.find(id));
  AE_EXPECT_TRUE(render && render->mesh==1,"slot preservado");
  AE_EXPECT_TRUE(!render->asset.valid(),"sem identidade, como no arquivo antigo");
  scene.reconcileAssets(antigo);
  AE_EXPECT_TRUE(meshRenderer(*antigo.find(id))->asset==scene.assetGuid(0),
                 "a reconciliação dá identidade ao projeto antigo");
}

AE_TEST(what_is_drawn_and_what_is_picked_share_the_same_pivot) {
  // O defeito: `extract` — o que aparece na tela — usava o pivô do nó, e
  // `pickGeometry` — o que o toque acerta — continuava usando o centro dos
  // limites. Em geometria vinda de um GLB com hierarquia os dois não coincidem
  // (a porta gira na dobradiça, não no meio dela), então a malha de seleção
  // ficava deslocada da malha desenhada: tocar o objeto não selecionava nada e
  // tocar ao lado selecionava.
  std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  AE_EXPECT_TRUE(renderer::appendBoxAuthoringGeometry(renderer::MapVertexStride,vertices,indices,draws,materials),"geometria");
  AE_EXPECT_EQ(draws.size(),usize{1},"um desenho no pacote");

  // Pivô deliberadamente longe do centro dos limites.
  const float pivots[3]{1.5f,0.0f,-.75f};
  EditorMapScene scene;EditorDocument doc;
  AE_EXPECT_TRUE(scene.adoptPackage(doc,draws,materials,vertices,indices,{},31,pivots),"pacote com pivô de nó");
  const auto id=doc.createEntity(doc.root(),EditorEntityKind::Mesh,"Porta");
  auto value=*doc.find(id);editMeshRenderer(value)->mesh=1;
  value.transform.position[0]=4;value.transform.position[2]=2;value.transform.rotationDegrees[1]=34;
  AE_EXPECT_TRUE(doc.applyEntityValues(id,value),"posicionar e girar");

  std::vector<EditorMapUpdate> updates;
  AE_EXPECT_TRUE(scene.extract(doc,updates),"extrair a cena");
  const EditorMapUpdate *drawn=nullptr;
  for(const auto &update:updates) if(update.objectId==id) drawn=&update;
  AE_EXPECT_TRUE(drawn!=nullptr,"o objeto foi desenhado");

  EditorPickCandidate candidate{};
  AE_EXPECT_TRUE(scene.pickGeometry(doc,id,candidate),"malha de seleção");
  bool identical=true;
  for(u32 term=0;term<16;++term)
    identical&=std::abs(candidate.model[term]-drawn->pose.draw.model[term])<1e-4f;
  AE_EXPECT_TRUE(identical,"a malha de seleção usa a mesma matriz da malha desenhada");
}

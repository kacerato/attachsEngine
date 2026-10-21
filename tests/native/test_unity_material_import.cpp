#include "editor/editor_screen.h"
#include "harness.h"
#include "resources/unity_material_import.h"
#include "renderer/material_override.h"
#include "scene/mesh_renderer.h"
#include <sstream>
#include "editor/editor_session.h"
#include "editor/editor_import_transaction.h"
#include "runtime/primitive_object.h"
#include "renderer/primitive_geometry.h"
#include "ui_software_raster.h"
#include <chrono>
#include <filesystem>
#include <cstdio>
#include <cstdlib>

using namespace ae;
using namespace ae::editor;
namespace {
std::string materialSource(const std::string &properties="",const std::string &keywords="") {
  return "%YAML 1.1\n--- !u!21 &2100000\nMaterial:\n  m_Name: Example\n"
    "  m_Shader: {fileID: 46, guid: 0000000000000000f000000000000000, type: 0}\n"
    "  m_ShaderKeywords: "+keywords+"\n  m_SavedProperties:\n"+properties;
}
struct Project {
  std::filesystem::path path=std::filesystem::temp_directory_path()/
    ("astra-unity-material-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Project() {std::filesystem::create_directories(path);}
  ~Project() {std::error_code error;std::filesystem::remove_all(path,error);}
  void write(const char *name,const std::string &text) {EditorImportTransaction::writeText(path/name,text);}
};
bool captureUi(const std::string &path,u32 width,u32 height,ui::UiDrawList &list,ui::UiFont &font,ui::UiIconAtlas &icons) {
  std::vector<ui::UiInstance> instances;const auto result=ui::buildUiInstances(list,font,icons,16384,instances);
  if(result.dropped||result.missingGlyphRuns) return false;
  test::UiSoftwareTarget target;target.resize(width,height,.12f,.13f,.14f);test::rasterizeUi(instances,font,icons,target);
  std::FILE *output=std::fopen(path.c_str(),"wb");if(!output) return false;
  std::fprintf(output,"P6\n%u %u\n255\n",width,height);
  for(usize i=0;i<target.pixels.size();i+=4) for(u32 c=0;c<3;++c)
    std::fputc(static_cast<u8>(std::clamp(target.pixels[i+c],0.0f,1.0f)*255+.5f),output);
  return std::fclose(output)==0;
}
}
AE_TEST(unity_material_converts_effective_factors_surface_and_uv_without_mutating_source) {
  resources::UnityMaterialImport result;
  const auto source=materialSource("    m_TexEnvs:\n    - _MainTex:\n"
    "        m_Texture: {fileID: 2800000, guid: 11111111111111111111111111111111, type: 3}\n"
    "        m_Scale: {x: 2, y: 3}\n        m_Offset: {x: 0.25, y: 0.5}\n"
    "    m_Floats:\n    - _Metallic: 0.6\n    - _Glossiness: 0.8\n    - _Mode: 1\n"
    "    m_Colors:\n    - _Color: {r: 0.3, g: 0.4, b: 0.5, a: 1}\n","_ALPHATEST_ON");
  AE_EXPECT_TRUE(resources::readUnityMaterial(source,result)&&result.supported(),"supported Standard");
  AE_EXPECT_TRUE(std::abs(result.material.values.roughness-.2f)<.0001f,"smoothness inverted");
  // [Gamma] _Metallic: 0,6 sRGB no Inspector chega linear ao shader.
  AE_EXPECT_TRUE(std::abs(result.material.values.metallic-.31855f)<.0005f,"metallic consumed in linear space");
  AE_EXPECT_EQ(result.audit(),resources::UnityMaterialAudit::Complete,"plain Standard is a complete conversion");
  AE_EXPECT_EQ(result.material.surface.alphaMode,scene::MaterialAlphaMask,"cutout consumed");
  AE_EXPECT_EQ(result.textures.size(),1u,"texture dependency");
  AE_EXPECT_EQ(result.textures[0].sampling.scale[1],3.f,"UV tiling preserved");
  AE_EXPECT_EQ(result.textures[0].sampling.offset[0],.25f,"UV offset preserved");
  resources::MaterialAsset reopened;
  AE_EXPECT_TRUE(resources::MaterialAsset::deserialize(result.material.serialize(),reopened),"native persistence");
}
AE_TEST(unity_material_rejects_shader_and_incoherent_variants_instead_of_silent_degradation) {
  resources::UnityMaterialImport result;
  auto source=materialSource("    m_TexEnvs:\n    - _MetallicGlossMap:\n"
    "        m_Texture: {fileID: 2800000, guid: 11111111111111111111111111111111, type: 3}\n");
  // Sem _METALLICGLOSSMAP o Standard não amostra o mapa: ele não vira dependência.
  AE_EXPECT_TRUE(resources::readUnityMaterial(source,result)&&result.supported()&&result.textures.empty(),"inactive metallic map ignored like Unity");
  AE_EXPECT_TRUE(resources::readUnityMaterial(materialSource("","_METALLICGLOSSMAP"),result)&&!result.supported(),"metallic map keyword without texture refused");
  AE_EXPECT_TRUE(resources::readUnityMaterial(materialSource("    m_Floats:\n    - _SmoothnessTextureChannel: 1\n"),result)&&!result.supported(),"smoothness channel bookkeeping without the shader variant refused");
  // PK-9: _PARALLAXMAP tem consumidor (Height Map); sem textura a altura é 0,5 e o deslocamento, zero.
  AE_EXPECT_TRUE(resources::readUnityMaterial(materialSource("","_PARALLAXMAP"),result)&&result.audit()==resources::UnityMaterialAudit::Complete,"parallax converts");
  source.replace(source.find("fileID: 46"),10,"fileID: 99");
  AE_EXPECT_TRUE(resources::readUnityMaterial(source,result)&&!result.supported(),"unknown shader rejected");
  AE_EXPECT_TRUE(!resources::readUnityMaterial(materialSource("    m_Floats:\n    - _Metallic: nan\n"),result),"nonfinite rejected");
  AE_EXPECT_TRUE(!resources::readUnityMaterial(materialSource("    m_Floats:\n    - _Metallic: 0\n    - _Metallic: 1\n"),result),"duplicate property rejected");
  AE_EXPECT_TRUE(resources::readUnityMaterial(materialSource("    m_Floats:\n    - _Mode: 1\n"),result)&&!result.supported(),"Inspector mode alone cannot enable cutout");
  AE_EXPECT_TRUE(resources::readUnityMaterial(materialSource("    m_Floats:\n    - _Mode: 0\n","_ALPHATEST_ON"),result)&&!result.supported(),"conflicting mode and active shader refused");
  AE_EXPECT_TRUE(resources::readUnityMaterial(materialSource("","_ALPHATEST_ON _ALPHABLEND_ON"),result)&&!result.supported(),"incoherent alpha variants refused");
  AE_EXPECT_TRUE(resources::readUnityMaterial(materialSource("    m_Floats:\n    - _Mode: 2\n    - _SrcBlend: 5\n    - _DstBlend: 10\n    - _ZWrite: 0\n","_ALPHABLEND_ON"),result)&&result.supported()&&result.material.surface.alphaMode==scene::MaterialAlphaBlend,"effective Fade blend accepted");
  AE_EXPECT_TRUE(resources::readUnityMaterial(materialSource("    m_Colors:\n    - _EmissionColor: {r: 4, g: 1, b: 0, a: 1}\n","_EMISSION_FAKE"),result)&&result.supported()&&result.material.values.emission[0]==0,"keyword substring cannot enable emission");
  AE_EXPECT_TRUE(resources::readUnityMaterial(materialSource("","_SPECULARHIGHLIGHTS_OFF"),result)&&result.supported()&&
    result.audit()==resources::UnityMaterialAudit::Complete&&!result.material.shading.specularHighlights&&
    result.material.shading.environmentReflections&&result.material.values.specular==1,"highlights alone has its own consumer (PK-9)");
}
// G3a: Standard.shader 2022.3, UnityStandardInput.cginc MetallicGloss/Alpha.
AE_TEST(unity_material_metallic_gloss_map_becomes_inverted_smoothness_channel) {
  resources::UnityMaterialImport result;
  const auto source=materialSource("    m_TexEnvs:\n    - _MainTex:\n"
    "        m_Texture: {fileID: 2800000, guid: 22222222222222222222222222222222, type: 3}\n"
    "        m_Scale: {x: 4, y: 4}\n        m_Offset: {x: 0, y: 0}\n"
    "    - _MetallicGlossMap:\n        m_Texture: {fileID: 2800000, guid: 11111111111111111111111111111111, type: 3}\n"
    "        m_Scale: {x: 1, y: 1}\n        m_Offset: {x: 0, y: 0}\n"
    "    m_Floats:\n    - _Metallic: 0.25\n    - _Glossiness: 0.9\n    - _GlossMapScale: 0.6\n","_METALLICGLOSSMAP");
  AE_EXPECT_TRUE(resources::readUnityMaterial(source,result)&&result.supported(),"metallic/smoothness map converts");
  // Unity 5 legado: entrada "<noninit>" sem consumidor no shader não recusa.
  auto legacy=source;legacy.insert(legacy.find("    - _MainTex:"),"    - <noninit>:\n        m_Texture: {fileID: 2800000, guid: 33333333333333333333333333333333, type: 3}\n");
  resources::UnityMaterialImport legacyResult;
  AE_EXPECT_TRUE(resources::readUnityMaterial(legacy,legacyResult)&&legacyResult.supported()&&legacyResult.textures.size()==2,"<noninit> ignored");
  AE_EXPECT_EQ(result.audit(),resources::UnityMaterialAudit::Complete,"no approximation needed");
  const auto &asset=result.material;
  AE_EXPECT_EQ(asset.channels.metallic,scene::MaterialChannelR,"metallic from R");
  AE_EXPECT_EQ(asset.channels.roughness,scene::MaterialChannelA,"smoothness from A");
  AE_EXPECT_EQ(asset.channels.roughnessSource,scene::MaterialRoughnessSmoothnessMap,"channel read as smoothness");
  AE_EXPECT_EQ(asset.values.metallic,1.f,"map value is metallic unscaled; _Metallic ignored");
  AE_EXPECT_TRUE(std::abs(asset.values.roughness-.4f)<.0001f,"factor stores 1 - _GlossMapScale; _Glossiness ignored");
  bool mapped=false;for(const auto &texture:result.textures)if(texture.property=="_MetallicGlossMap")
    mapped=texture.binding==2&&texture.sampling.scale[0]==4;
  AE_EXPECT_TRUE(mapped,"map bound to metal/roughness slot with _MainTex_ST");
  // Extensão por desenho: linha 10.y diz ao shader que o canal é suavidade.
  float entry[renderer::MaterialExtensionFloats]{};auto values=asset.values;values.channels=asset.channels;
  AE_EXPECT_TRUE(renderer::needsMaterialExtension(values),"smoothness requires the material extension");
  renderer::materialExtensionEntry(values,0,renderer::MaterialExtensionNoTexture,entry);
  AE_EXPECT_EQ(entry[9*4+0],3.f,"roughness channel index A");AE_EXPECT_EQ(entry[9*4+1],0.f,"metallic channel index R");
  AE_EXPECT_EQ(entry[10*4+1],1.f,"row 10.y: channel as smoothness");
  // Persistência atual e leitura de um v6 antigo sem a origem (v7) nem o shader (v8, 32 números).
  resources::MaterialAsset reopened;
  AE_EXPECT_TRUE(resources::MaterialAsset::deserialize(asset.serialize(),reopened)&&reopened.channels==asset.channels,"current round trip");
  auto old=asset.serialize();old.replace(old.find("ASTRA_MATERIAL 8"),16,"ASTRA_MATERIAL 6");
  while(!old.empty()&&(old.back()=='\n'||old.back()=='\r'))old.pop_back();
  for(int token=0;token<33;++token) old.erase(old.rfind(' '));
  old+="\n";
  AE_EXPECT_TRUE(resources::MaterialAsset::deserialize(old,reopened)&&reopened.channels.roughnessSource==scene::MaterialRoughnessKeep,"v6 material keeps inherited roughness");
}
AE_TEST(unity_material_albedo_alpha_smoothness_and_forward_options) {
  resources::UnityMaterialImport result;
  AE_EXPECT_TRUE(resources::readUnityMaterial(materialSource("    m_Floats:\n    - _SmoothnessTextureChannel: 1\n    - _GlossMapScale: 0.8\n    - _Metallic: 1\n",
    "_SMOOTHNESS_TEXTURE_ALBEDO_CHANNEL_A"),result)&&result.supported(),"albedo alpha smoothness converts");
  AE_EXPECT_EQ(result.material.channels.roughnessSource,scene::MaterialRoughnessSmoothnessBaseAlpha,"base alpha read as smoothness");
  AE_EXPECT_EQ(result.material.channels.alphaSource,scene::MaterialAlphaSourceOpaque,"albedo alpha is no longer opacity");
  AE_EXPECT_TRUE(std::abs(result.material.values.roughness-.2f)<.0001f&&result.material.values.metallic==1,"scale and scalar metallic");
  float entry[renderer::MaterialExtensionFloats]{};auto values=result.material.values;values.channels=result.material.channels;
  renderer::materialExtensionEntry(values,0,renderer::MaterialExtensionNoTexture,entry);
  AE_EXPECT_EQ(entry[10*4+1],2.f,"row 10.y: base alpha as smoothness");
  AE_EXPECT_TRUE(resources::readUnityMaterial(materialSource("    m_Floats:\n    - _SpecularHighlights: 0\n    - _GlossyReflections: 0\n",
    "_SPECULARHIGHLIGHTS_OFF _GLOSSYREFLECTIONS_OFF"),result)&&result.audit()==resources::UnityMaterialAudit::Complete&&
    !result.material.shading.specularHighlights&&!result.material.shading.environmentReflections,"both forward options off map to Advanced Options");
  AE_EXPECT_TRUE(resources::readUnityMaterial(materialSource("    m_Floats:\n    - _Metallic: 1\n","_SPECULARHIGHLIGHTS_OFF _GLOSSYREFLECTIONS_OFF"),result)&&
    result.audit()==resources::UnityMaterialAudit::Complete,"metal with both off is exact now");
  AE_EXPECT_TRUE(resources::readUnityMaterial(materialSource("    m_Floats:\n    - _GlossyReflections: 0\n"),result)&&
    result.audit()==resources::UnityMaterialAudit::Complete&&!result.material.shading.environmentReflections&&result.material.values.specular==1,
    "reflections alone exact");
  AE_EXPECT_TRUE(resources::readUnityMaterial(materialSource("    m_Colors:\n    - _Color: {r: 1, g: 1, b: 1, a: 0.5}\n"),result)&&result.supported(),
    "opaque mode ignores colour alpha like Unity");
  AE_EXPECT_TRUE(resources::readUnityMaterial(materialSource("    m_Floats:\n    - _Mode: 1\n    m_Colors:\n    - _Color: {r: 1, g: 1, b: 1, a: 0.5}\n","_ALPHATEST_ON"),result)&&
    !result.supported(),"cutout with colour alpha still has no consumer");
  // Componente de malha v10: a origem por slot volta do texto.
  scene::MeshRenderer mesh;mesh.channels.roughnessSource=scene::MaterialRoughnessSmoothnessMap;
  std::ostringstream out;mesh.write(out);scene::MeshRenderer back;std::istringstream in(out.str());
  AE_EXPECT_TRUE(back.read(in,10)&&back.channels.roughnessSource==scene::MaterialRoughnessSmoothnessMap,"mesh v10 round trip");
}
AE_TEST(unity_material_editor_conversion_undo_redo_reopen_and_local_conflict_are_real) {
  Project project;project.write("Example.mat",materialSource("    m_Floats:\n    - _Metallic: 0.2\n"));
  EditorSession session;session.initialize(nullptr,nullptr);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.path.string().c_str()),"project");
  std::string diagnostic;const auto guid=session.importUnityMaterial("Example.mat",diagnostic);
  AE_EXPECT_TRUE(guid.valid(),diagnostic.c_str());
  AE_EXPECT_TRUE(std::abs(session.findMaterialAsset(guid)->values.metallic-.0331f)<.0005f,"shared runtime resource ([Gamma] _Metallic 0,2)");
  AE_EXPECT_TRUE(session.history().undo(session.document()),"undo conversion");
  AE_EXPECT_TRUE(!session.findMaterialAsset(guid)&&!std::filesystem::exists(project.path/"Example.mat.material"),"native resource removed");
  AE_EXPECT_TRUE(std::filesystem::exists(project.path/"Example.mat"),"source preserved");
  AE_EXPECT_TRUE(session.history().redo(session.document()),"redo conversion");
  EditorSession reopened;reopened.initialize(nullptr,nullptr);
  AE_EXPECT_TRUE(reopened.setProjectDirectory(project.path.string().c_str())&&reopened.loadAssets(session.serializeAssets()),"shell cold registry load");
  AE_EXPECT_TRUE(reopened.findMaterialAsset(guid),"native material reopened");
  const auto oldRecipe=reopened.assets().find(guid)->importerParameters;
  const float initialMetallic=reopened.findMaterialAsset(guid)->values.metallic;
  project.write("Example.mat",materialSource("    m_Floats:\n    - _Metallic: 0.8\n"));
  AE_EXPECT_TRUE(reopened.importUnityMaterial("Example.mat",diagnostic)==guid,diagnostic.c_str());
  const float updatedMetallic=reopened.findMaterialAsset(guid)->values.metallic;
  AE_EXPECT_TRUE(std::abs(reopened.findMaterialAsset(guid)->values.metallic-.6038f)<.0005f,"same identity source update ([Gamma] 0,8)");
  const auto newRecipe=reopened.assets().find(guid)->importerParameters;
  AE_EXPECT_TRUE(oldRecipe!=newRecipe&&reopened.history().undo(reopened.document()),"undo reimport");
  AE_EXPECT_TRUE(reopened.findMaterialAsset(guid)->values.metallic==initialMetallic&&reopened.assets().find(guid)->importerParameters==oldRecipe,"undo values and recipe atomically");
  AE_EXPECT_TRUE(reopened.history().redo(reopened.document())&&reopened.findMaterialAsset(guid)->values.metallic==updatedMetallic&&reopened.assets().find(guid)->importerParameters==newRecipe,"redo values and recipe");
  AE_EXPECT_TRUE(reopened.history().undo(reopened.document())&&reopened.importUnityMaterial("Example.mat",diagnostic)==guid,"reimport after undo without false conflict");
  const auto stableMaterial=reopened.findMaterialAsset(guid)->serialize(),stableRegistry=reopened.serializeAssets();
  project.write("Example.mat.material",stableMaterial+"corrupt external edit");
  AE_EXPECT_TRUE(!reopened.importUnityMaterial("Example.mat",diagnostic).valid()&&reopened.serializeAssets()==stableRegistry&&reopened.findMaterialAsset(guid)->serialize()==stableMaterial,"disk conflict preserves runtime and registry");
  project.write("Example.mat.material",stableMaterial);
  resources::AssetRegistry external;AE_EXPECT_TRUE(resources::AssetRegistry::deserialize(stableRegistry,external),"external fixture registry");
  const auto externalRecord=*external.find(guid);
  AE_EXPECT_TRUE(external.publishImport(guid,externalRecord.contentHash,externalRecord.importerVersion,"external changed recipe",externalRecord.derived,externalRecord.dependencies),"changed external metadata");
  project.write(".astra/assets.astra",external.serialize());
  AE_EXPECT_TRUE(!reopened.importUnityMaterial("Example.mat",diagnostic).valid()&&reopened.serializeAssets()==stableRegistry,"external metadata refused without mutation");
  AE_EXPECT_TRUE(!reopened.history().undo(reopened.document()),"external metadata blocks history overwrite");
  project.write(".astra/assets.astra",stableRegistry);
  AE_EXPECT_TRUE(reopened.setSlotMaterialValue(0,0,EditorSession::MaterialScope::Shared,4,.3f,diagnostic),diagnostic.c_str());
  AE_EXPECT_TRUE(!reopened.importUnityMaterial("Example.mat",diagnostic).valid()&&diagnostic.find("Conflito")!=std::string::npos,"actionable local edit conflict");
}
AE_TEST(unity_material_missing_guid_dependency_leaves_no_partial_registration) {
  Project project;project.write("Missing.mat",materialSource("    m_TexEnvs:\n    - _MainTex:\n"
    "        m_Texture: {fileID: 2800000, guid: 11111111111111111111111111111111, type: 3}\n"));
  EditorSession session;session.initialize(nullptr,nullptr);
  AE_EXPECT_TRUE(session.setProjectDirectory(project.path.string().c_str()),"project");std::string diagnostic;
  AE_EXPECT_TRUE(!session.importUnityMaterial("Missing.mat",diagnostic).valid(),"missing dependency refused");
  AE_EXPECT_TRUE(diagnostic.find("11111111111111111111111111111111")!=std::string::npos,"missing GUID reported");
  AE_EXPECT_TRUE(!std::filesystem::exists(project.path/"Missing.mat.material")&&session.materialAssets().empty(),"nothing partially published");
}
AE_TEST(unity_material_texture_dependency_resolves_and_touch_applies_real_shared_resource) {
  Project project;
  const std::vector<u8> png{137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,6,0,0,0,31,21,196,137,0,0,0,13,73,68,65,84,120,156,99,104,112,80,248,15,0,4,4,1,224,242,235,128,249,0,0,0,0,73,69,78,68,174,66,96,130};
  AE_EXPECT_TRUE(EditorImportTransaction::write(project.path/"Color.png",png),"image source");
  project.write("Color.png.meta","fileFormatVersion: 2\nguid: 11111111111111111111111111111111\n");
  const auto paintSource=materialSource("    m_TexEnvs:\n    - _MainTex:\n"
    "        m_Texture: {fileID: 2800000, guid: 11111111111111111111111111111111, type: 3}\n"
    "    m_Floats:\n    - _Metallic: 0.7\n");project.write("Paint.mat",paintSource);
  // These vectors own the spans loaded by font/atlas through all rasterizations.
  std::vector<u8> fontBytes,iconBytes;ui::UiFont font;ui::UiIconAtlas icons;
  AE_EXPECT_TRUE(EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui/astra-ui-font.aeuf",fontBytes)&&font.load(fontBytes),"actual font");
  AE_EXPECT_TRUE(EditorImportTransaction::read(std::filesystem::path(AETHER_REPOSITORY_ROOT)/"assets/astra-visual/ui/astra-ui-icons.aeui",iconBytes)&&icons.load(iconBytes),"actual icon atlas");
  EditorSession session;session.initialize(&font,&icons);session.setSurface({0,0,1200,540},{});
  AE_EXPECT_TRUE(session.setProjectDirectory(project.path.string().c_str()),"project");
  std::vector<u8> vertices;std::vector<u32> indices;std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  AE_EXPECT_TRUE(renderer::appendPrimitiveLibrary(renderer::MapVertexStride,vertices,indices,draws,materials)&&session.importMap(draws,materials,false,vertices,indices,0),"publish actual primitive geometry");
  const auto parent=session.document().createEntity(session.document().root(),EditorEntityKind::Folder,"Conjunto");
  const auto id=session.document().createEntity(parent,EditorEntityKind::Mesh,"Material target");auto object=*session.document().find(id);
  AE_EXPECT_TRUE(runtime::configurePrimitive(object,scene::PrimitiveType::Cube,{1,session.mapScene().assetGuid(0),session.mapScene().materialForAsset(0)})&&session.document().applyEntityValues(id,object),"engine primitive authoring");
  std::string diagnostic;
  AE_EXPECT_TRUE(session.setSlotMaterialValue(id,0,EditorSession::MaterialScope::Instance,4,.4f,diagnostic),"Inspector creates local factor override");
  AE_EXPECT_TRUE(session.clearSlotMaterialOverride(id,0),"Inspector inheritance clears local factors");
  AE_EXPECT_TRUE(meshRenderer(*session.document().find(id)),"actual mesh component");session.setSelection(id);
  const auto guid=session.importUnityMaterial("Paint.mat",diagnostic);AE_EXPECT_TRUE(guid.valid(),diagnostic.c_str());
  const auto texture=session.findMaterialAsset(guid)->textures[0];
  AE_EXPECT_TRUE(texture.valid()&&texture!=scene::MaterialTextureNone,"resolved GUID dependency");session.update();
  ui::UiDrawList list;ui::UiInputRouter router;
  list.begin(session.screen().surface,font.metrics(ui::UiFontWeight::Regular));router.beginFrame();buildEditorScreen(session.screen(),ui::defaultTheme(),list,router);
  ui::UiRect button{};
  for(const auto &command:list.commands()) if(command.kind==ui::UiPrimitive::Text&&list.textOf(command).starts_with("Aplicar no slot")) button=command.bounds;
  AE_EXPECT_TRUE(button.width>0&&button.height>=42,"touch action in short landscape");
  const ui::UiPoint point{button.x+button.width*.5f,button.y+button.height*.5f};
  AE_EXPECT_EQ(router.route({91,ui::UiPointerPhase::Down,point,1}).widgetId,widgetId(EditorWidget::MaterialInspectorApply),"pixel hit region matches real action");
  if(const char *capture=std::getenv("ASTRA_MATERIAL_CAPTURE")) AE_EXPECT_TRUE(captureUi(capture,1200,540,list,font,icons),"native UI capture without dropped commands or missing glyphs");
  session.handlePointer({92,ui::UiPointerPhase::Down,point,2});session.handlePointer({92,ui::UiPointerPhase::Up,point,2.05});session.update();
  const auto *render=meshRenderer(*session.document().find(id));
  AE_EXPECT_TRUE(render&&render->slotMaterialAsset(0)==guid,"touch binds native resource");
  AE_EXPECT_TRUE(std::abs(session.screen().resources->slotMaterial(*render,0).metallic-.4480f)<.0005f,"effective renderer factor ([Gamma] _Metallic 0,7)");
  auto &preview=session.cameraPreview();const auto version=session.sceneVersion();
  AE_EXPECT_TRUE(preview.pinMaterial(session.document(),version,session.mapScene(),parent,guid),"group previews actual child mesh");
  renderer::RenderViewSnapshot request;const auto revision=session.findMaterialAsset(guid)->revision;
  AE_EXPECT_TRUE(preview.request(session.document(),version,10,true,false,request,&session.mapScene(),revision),"bounded preview lease");
  AE_EXPECT_TRUE(request.isolatedObjects&&request.isolatedObjects->size()==1&&request.isolatedObjects->front()==id,"unrelated geometry excluded");
  std::vector<EditorMapUpdate> extracted;AE_EXPECT_TRUE(session.mapScene().extract(session.document(),extracted),"published world bounds");
  for(const auto &draw:extracted) if(draw.objectId==id) AE_EXPECT_TRUE(renderer::isSphereVisible(request.frustum,draw.pose.draw.boundsCenter,draw.pose.draw.boundsRadius),"real world bounds framed");
  AE_EXPECT_TRUE(!preview.complete(request,version,true,{},revision+1),"stale resource completion rejected");
  AE_EXPECT_TRUE(preview.request(session.document(),version,11,true,false,request,&session.mapScene(),revision),"new lease after stale completion");
  AE_EXPECT_TRUE(preview.complete(request,version,true,{},revision)&&preview.hasCurrentImage(version,revision),"matching backend completion publishes");
  AE_EXPECT_TRUE(!preview.hasCurrentImage(version,revision+1),"resource edits invalidate image");
  AE_EXPECT_TRUE(!preview.pinMaterial(session.document(),version,session.mapScene(),0,guid)&&!preview.active()&&!preview.hasCurrentImage(version,revision),"failed target clears old pinned image");preview.close();
  AE_EXPECT_TRUE(!preview.hasCurrentImage(version,revision)&&!preview.camera(),"closing releases preview state");
  MaterialObjectPreviewView missing;const auto empty=session.document().createEntity(session.document().root(),EditorEntityKind::Folder,"Sem material");
  AE_EXPECT_TRUE(!buildMaterialObjectPreview(session.document(),session.mapScene(),empty,guid,640,360,missing)&&!missing.diagnostic.empty(),"no-use diagnostic without fabricated mesh");
  AE_EXPECT_TRUE(session.history().undo(session.document())&&!meshRenderer(*session.document().find(id))->slotMaterialAsset(0).valid(),"undo actual material binding");
  AE_EXPECT_TRUE(!preview.pinStandalone(session.sceneVersion(),session.mapScene(),guid)&&!preview.active()&&!preview.diagnostic().empty(),"unpublished texture refuses image without fabricated texture");
  project.write("Unused.mat",materialSource("    m_Floats:\n    - _Metallic: 0.7\n"));
  const auto unused=session.importUnityMaterial("Unused.mat",diagnostic);AE_EXPECT_TRUE(unused.valid(),diagnostic.c_str());
  const auto *unusedMaterial=session.findMaterialAsset(unused);AE_EXPECT_TRUE(unusedMaterial,"unused resource exists");
  const auto unusedRevision=unusedMaterial?unusedMaterial->revision:0;
  AE_EXPECT_TRUE(preview.pinStandalone(session.sceneVersion(),session.mapScene(),unused),"unused material has independent geometry");
  AE_EXPECT_TRUE(preview.request(session.document(),session.sceneVersion(),12,true,false,request,&session.mapScene(),unusedRevision)&&request.materialGeometry&&!request.isolatedObjects,"standalone packet owns genuine geometry");
  AE_EXPECT_TRUE(request.materialGeometry->indices.size()>0&&std::abs(request.previewMaterial.metallic-.4480f)<.0005f,"actual geometry and shared factors consumed");
  const auto oldPosition=request.frustum.cameraPosition[0];
  AE_EXPECT_TRUE(preview.orbit(.3f,.2f,.8f)&&preview.changeShape(),"independent orbit zoom and shape");
  AE_EXPECT_TRUE(!preview.complete(request,session.sceneVersion(),true,{},unusedRevision),"controls invalidate previous submission lease");
  AE_EXPECT_TRUE(preview.request(session.document(),session.sceneVersion(),13,true,false,request,&session.mapScene(),unusedRevision)&&request.materialGeometry->shape==renderer::MaterialPreviewShape::Cube&&request.frustum.cameraPosition[0]!=oldPosition,"controls alter rendered camera and mesh");
  session.setSurface({0,0,655,300},{});session.update();list.begin(session.screen().surface,font.metrics(ui::UiFontWeight::Regular));router.beginFrame();
  buildEditorScreen(session.screen(),ui::defaultTheme(),list,router);ui::UiRect shapeAction{};
  for(const auto &command:list.commands()) if(command.kind==ui::UiPrimitive::Text&&list.textOf(command)=="Caixa") shapeAction=command.bounds;
  AE_EXPECT_TRUE(shapeAction.height>=40,"compact focal shape touch target remains accessible");
  if(!shapeAction.isEmpty()) {const ui::UiPoint shapePoint{shapeAction.x+shapeAction.width*.5f,shapeAction.y+shapeAction.height*.5f};
    session.handlePointer({93,ui::UiPointerPhase::Down,shapePoint,3});session.handlePointer({93,ui::UiPointerPhase::Up,shapePoint,3.05});
    AE_EXPECT_TRUE(preview.shape()==renderer::MaterialPreviewShape::Plane,"real UI shape action reaches geometry owner");}
  if(const char *capture=std::getenv("ASTRA_MATERIAL_CAPTURE")) AE_EXPECT_TRUE(captureUi(std::string(capture)+".standalone.655x300.ppm",655,300,list,font,icons),"native focal workflow capture; GPU image still pending");
  preview.close();
  project.write("Paint.mat",materialSource("    m_Floats:\n    - _Metallic: 0.7\n"));
  AE_EXPECT_TRUE(session.importUnityMaterial("Paint.mat",diagnostic)==guid&&session.assets().find(guid)->dependencies.empty(),"reimport removes texture edge");
  AE_EXPECT_TRUE(session.history().undo(session.document())&&session.findMaterialAsset(guid)->textures[0]==texture&&session.assets().find(guid)->dependencies.size()==1,"undo restores texture and dependency edge");
  AE_EXPECT_TRUE(session.history().redo(session.document())&&session.assets().find(guid)->dependencies.empty(),"redo dependency change");
  AE_EXPECT_TRUE(session.history().undo(session.document()),"restore textured source baseline");project.write("Paint.mat",paintSource);
  project.write("Color.png.meta","fileFormatVersion: 2\nguid: 22222222222222222222222222222222\n");
  AE_EXPECT_TRUE(!session.importUnityMaterial("Paint.mat",diagnostic).valid()&&session.findMaterialAsset(guid),"changed dependency identity retains last valid material");
  if(const char *capture=std::getenv("ASTRA_MATERIAL_CAPTURE")) {
    session.setSurface({0,0,655,300},{});session.update();list.begin(session.screen().surface,font.metrics(ui::UiFontWeight::Regular));router.beginFrame();
    buildEditorScreen(session.screen(),ui::defaultTheme(),list,router);
    AE_EXPECT_TRUE(captureUi(std::string(capture)+".655x300.ppm",655,300,list,font,icons),"native compact capture with owned font and atlas spans");
  }
}

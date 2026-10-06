#pragma once
#include "editor/editor_session.h"
#include "editor/editor_import_transaction.h"
#include "renderer/primitive_geometry.h"
#include "scene/import_link.h"
#include <bit>
#include <chrono>
#include <thread>

namespace ae::test {
// Real geometry packet consumed by EditorMapScene, without GPU allocation.
struct ConvexCpuLibrary {
  std::vector<u8> vertices;std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;std::vector<renderer::MapMaterialRecord> materials;
  bool refuse=false;
  bool publish(std::span<const u8> v,std::span<const u32> i,std::span<const renderer::MapDrawRecord> d,
               std::span<const renderer::MapMaterialRecord> m,editor::EditorSession::PublishedGeometry &out) {
    if(refuse)return false;
    vertices.clear();indices.clear();draws.clear();materials.clear();
    if(!renderer::appendPrimitiveLibrary(renderer::MapVertexStride,vertices,indices,draws,materials))return false;
    const auto vertexBase=static_cast<u32>(vertices.size()/renderer::MapVertexStride),indexBase=static_cast<u32>(indices.size()),materialBase=static_cast<u32>(materials.size());
    vertices.insert(vertices.end(),v.begin(),v.end());indices.insert(indices.end(),i.begin(),i.end());materials.insert(materials.end(),m.begin(),m.end());
    for(auto draw:d){draw.firstIndex+=indexBase;draw.vertexOffset+=vertexBase;draw.materialIndex+=materialBase;draw.lodGroupId=static_cast<u32>(draws.size());draws.push_back(draw);}
    out={draws,materials,vertices,indices};return true;
  }
  bool connect(editor::EditorSession &s) {
    editor::EditorSession::PublishedGeometry out;if(!publish({},{},{},{},out)||!s.importMap(draws,materials,false,vertices,indices,0))return false;
    s.setGeometryPublisher([this](auto v,auto i,auto d,auto m,std::span<const renderer::SharedAuthoringTexture>,auto &out){return publish(v,i,d,m,out);});return true;
  }
};
inline std::vector<u8> convexFixtureU() {
  const float polygon[][2]{{-2,-2},{2,-2},{2,2},{1,2},{1,-1},{-1,-1},{-1,2},{-2,2}};
  std::vector<u8> binary;const auto word=[&](u32 v){for(u32 k=0;k<4;++k)binary.push_back(static_cast<u8>(v>>(k*8)));};
  for(float y:{-.5f,.5f})for(const auto &p:polygon){word(std::bit_cast<u32>(p[0]));word(std::bit_cast<u32>(y));word(std::bit_cast<u32>(p[1]));}
  std::vector<u32> indices;const u32 cap[][3]{{0,1,4},{0,4,5},{1,2,3},{1,3,4},{0,5,6},{0,6,7}};
  for(const auto &t:cap){indices.insert(indices.end(),t,t+3);indices.insert(indices.end(),{t[0]+8,t[2]+8,t[1]+8});}
  for(u32 a=0;a<8;++a){const auto b=(a+1)%8;indices.insert(indices.end(),{a,b+8,b,a,a+8,b+8});}
  for(auto index:indices)word(index);
  std::string json="{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"byteLength\":"+std::to_string(binary.size())+"}],\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":192},{\"buffer\":0,\"byteOffset\":192,\"byteLength\":"+std::to_string(indices.size()*4)+"}],\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":16,\"type\":\"VEC3\",\"min\":[-2,-0.5,-2],\"max\":[2,0.5,2]},{\"bufferView\":1,\"componentType\":5125,\"count\":"+std::to_string(indices.size())+",\"type\":\"SCALAR\"}],\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}],\"nodes\":[{\"name\":\"Concave U\",\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}],\"scene\":0}";
  while(json.size()%4)json+=' ';
  auto payload=std::move(binary);binary.clear();word(0x46546c67);word(2);word(static_cast<u32>(28+json.size()+payload.size()));word(static_cast<u32>(json.size()));word(0x4e4f534a);binary.insert(binary.end(),json.begin(),json.end());word(static_cast<u32>(payload.size()));word(0x004e4942);binary.insert(binary.end(),payload.begin(),payload.end());return binary;
}
inline editor::EditorEntityId convexFixtureObject(editor::EditorSession &s,std::string &error) {
  const auto bytes=convexFixtureU();resources::GltfImport model;if(!resources::importGlb(bytes,{},{},model)){error=model.diagnostic;return 0;}
  editor::EditorSession::ModelImportReport report;if(!s.commitModelImport(bytes,model,"Sources/concave-u.glb","",report)){error=report.diagnostic;return 0;}
  if(!s.instantiateModel(report.source,report,false)){error=report.diagnostic;return 0;}
  const auto id=s.selection();auto entity=*s.document().find(id);
  while(entity.components.remove(scene::Collider::descriptor)){}while(entity.components.remove(scene::PhysicsBody::descriptor)){}
  if(!s.document().applyEntityValues(id,entity)){error="Fixture object rejected";return 0;}
  return id;
}
inline bool waitConvexBake(editor::EditorSession &s) {
  // Authoring allows a shared budget up to 120 seconds. Do not fail a valid
  // multi-source job halfway through its configured budget on a debug host.
  const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(125);
  while(s.motorDecompositionProgress().status==resources::ConvexBakeStatus::Running&&std::chrono::steady_clock::now()<end){s.update();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
  s.update();return s.motorDecompositionProgress().status==resources::ConvexBakeStatus::Ready&&s.screen().motorBakeReady;
}
inline editor::EditorEntityId convexFixtureHierarchy(editor::EditorSession &s,std::string &error) {
  const auto mesh=convexFixtureObject(s,error);if(!mesh)return 0;
  auto &g=s.document();const auto root=g.createEntity(g.root(),runtime::ObjectKind::Folder,"CompoundPlayer");
  const auto branch=g.createEntity(root,runtime::ObjectKind::Folder,"ScaledBranch");auto parent=*g.find(branch);
  parent.transform.position[0]=-4;parent.transform.scale[0]=1.3f;parent.transform.scale[2]=.8f;
  if(!g.applyEntityValues(branch,parent)||!g.reparent(mesh,branch,0)){error="Hierarchy rejected";return 0;}
  auto first=*g.find(mesh);editor::assignEntityName(first,"RotatedPart");first.transform.rotationDegrees[1]=23;
  if(!g.applyEntityValues(mesh,first)){error="Rotated source rejected";return 0;}
  const auto instance=g.createEntity(root,runtime::ObjectKind::Mesh,"SecondInstance");auto second=*g.find(instance);
  second.components=first.components;second.components.remove(scene::ImportLink::descriptor);
  second.transform.position[0]=4;second.transform.scale[0]=second.transform.scale[2]=.8f;second.transform.rotationDegrees[1]=180;
  if(!g.applyEntityValues(instance,second)){error="Second source instance rejected";return 0;}
  s.setSelection(root);return root;
}
inline bool tapConvexWidget(editor::EditorSession &s,const ui::UiFont &font,editor::EditorWidget widget) {
  s.update();ui::UiInputRouter router;ui::UiDrawList list;list.begin(s.screen().surface,font.metrics(ui::UiFontWeight::Regular));
  editor::buildEditorScreen(s.screen(),ui::defaultTheme(),list,router);
  for(float y=2;y<s.screen().surface.height;y+=4)for(float x=2;x<s.screen().surface.width;x+=4) {
    const auto hit=router.hitTest({x,y});if(hit.target!=ui::UiPointerTarget::Widget||hit.widgetId!=editor::widgetId(widget))continue;
    s.handlePointer({88,ui::UiPointerPhase::Down,{x,y},0});s.handlePointer({88,ui::UiPointerPhase::Up,{x,y},.02});s.update();return true;
  }
  std::fprintf(stderr,"Convex workflow widget unreachable: %08x\n",editor::widgetId(widget));return false;
}
class ConvexMapGeometry final : public runtime::CollisionGeometrySource {
public:
  explicit ConvexMapGeometry(const editor::EditorMapScene &s):map(s){}
  bool meshTriangles(u32 slot,std::vector<float> &out) const override {
    std::span<const editor::EditorPickMesh::Triangle> triangles;float m[16];if(!map.localGeometry(slot,triangles,m))return false;
    for(const auto &t:triangles)for(u32 v=0;v<3;++v)for(u32 k=0;k<3;++k)out.push_back(m[12+k]+m[k]*t[v*3]+m[4+k]*t[v*3+1]+m[8+k]*t[v*3+2]);
    return true;
  }
  bool meshTriangles(const resources::AssetGuid &guid,std::vector<float> &out) const override{return meshTriangles(map.assetSlot(guid),out);}
private:const editor::EditorMapScene &map;
};
}

#include "harness.h"
#include "renderer/primitive_geometry.h"
#include "editor/editor_session.h"
#include "editor/editor_creation_catalog.h"
#include "editor/editor_archive.h"
#include "runtime/scene_components.h"
#include <cmath>
#include <cstring>

using namespace ae;
using namespace ae::editor;
namespace {
struct Library {
  std::vector<u8> vertices;
  std::vector<u32> indices;
  std::vector<renderer::MapDrawRecord> draws;
  std::vector<renderer::MapMaterialRecord> materials;
  bool build() {return renderer::appendPrimitiveLibrary(renderer::MapVertexStride,vertices,indices,draws,materials);}
};
}

AE_TEST(primitives_geometry_has_outward_faces_uv_frames_and_exact_dimensions) {
  Library lib;AE_EXPECT_TRUE(lib.build(),"build all six resources");
  AE_EXPECT_EQ(lib.draws.size(),usize{6},"six draws");
  const float extents[6][3]{{.5f,.5f,.5f},{.5f,.5f,.5f},{.5f,1,.5f},{.5f,1,.5f},{5,0,5},{.5f,.5f,0}};
  for(u32 kind=0;kind<6;++kind) {
    const auto &draw=lib.draws[kind];
    AE_EXPECT_EQ(renderer::primitiveFromFlags(lib.materials[draw.materialIndex].flags),static_cast<scene::PrimitiveType>(kind),"resource role");
    if(kind) AE_EXPECT_TRUE(lib.materials[draw.materialIndex].flags&renderer::MapMaterialCullBackFaces,"new geometry has defined front face");
    const auto first=static_cast<u32>(draw.vertexOffset);
    const auto end=kind+1<6?static_cast<u32>(lib.draws[kind+1].vertexOffset):static_cast<u32>(lib.vertices.size()/renderer::MapVertexStride);
    float maximum[3]{};
    for(u32 v=first;v<end;++v) {
      const auto *bytes=lib.vertices.data()+v*renderer::MapVertexStride;
      float p[3],uv[2];i16 n[4],t[4];
      std::memcpy(p,bytes,12);std::memcpy(n,bytes+12,8);std::memcpy(t,bytes+20,8);std::memcpy(uv,bytes+28,8);
      float norm=0,dot=0,radius=0;
      for(u32 k=0;k<3;++k) {maximum[k]=std::max(maximum[k],std::abs(p[k]));norm+=std::pow(n[k]/32767.f,2);dot+=n[k]/32767.f*(t[k]/32767.f);radius+=p[k]*p[k];}
      AE_EXPECT_TRUE(std::abs(norm-1)<.0001f && std::abs(dot)<.0001f,"unit normal and orthogonal tangent");
      AE_EXPECT_TRUE(std::abs(t[3])==32767,"defined tangent handedness");
      AE_EXPECT_TRUE(uv[0]>=0 && uv[0]<=1 && uv[1]>=0 && uv[1]<=1,"UV domain");
      AE_EXPECT_TRUE(std::sqrt(radius)<=draw.boundsRadius+.00001f,"bounds contain geometry");
    }
    for(u32 k=0;k<3;++k) AE_EXPECT_TRUE(std::abs(maximum[k]-extents[kind][k])<.00001f,"documented dimensions");
    for(u32 i=0;i<draw.indexCount;i+=3) {
      float p[3][3],normal[3]{};
      for(u32 v=0;v<3;++v) {
        const auto index=first+lib.indices[draw.firstIndex+i+v];AE_EXPECT_TRUE(index>=first && index<end,"indices stay inside draw");
        const auto *bytes=lib.vertices.data()+index*renderer::MapVertexStride;std::memcpy(p[v],bytes,12);
        i16 n[4];std::memcpy(n,bytes+12,8);for(u32 k=0;k<3;++k) normal[k]+=n[k]/32767.f;
      }
      float a[3],b[3];for(u32 k=0;k<3;++k) {a[k]=p[1][k]-p[0][k];b[k]=p[2][k]-p[0][k];}
      const float cross[3]{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
      const float dot=cross[0]*normal[0]+cross[1]*normal[1]+cross[2]*normal[2];
      AE_EXPECT_TRUE(dot>1e-7f,"nondegenerate triangles point outward");
    }
  }
  const auto before=lib.vertices.size();AE_EXPECT_TRUE(!renderer::appendPrimitiveLibrary(1,lib.vertices,lib.indices,lib.draws,lib.materials),"reject invalid vertex format");
  AE_EXPECT_EQ(lib.vertices.size(),before,"failure preserves library");
}

AE_TEST(primitives_editor_history_archive_and_resource_reordering_preserve_shapes) {
  Library lib;AE_EXPECT_TRUE(lib.build(),"library");EditorSession session;
  AE_EXPECT_TRUE(session.importMap(lib.draws,lib.materials,false,lib.vertices,lib.indices,41),"load resources without objects");
  std::array<EditorEntityId,6> ids{};
  const float position[3]{};
  ids[0]=session.instantiateAsset(0,session.document().root(),position);
  const char *recipes[]{"geometry.sphere","geometry.capsule","geometry.cylinder","geometry.plane","geometry.quad"};
  for(u32 k=1;k<6;++k) {
    u32 recipe=0;AE_EXPECT_TRUE(findCreationRecipe(recipes[k-1],&recipe),"recipe exists");
    AE_EXPECT_TRUE(creationAvailable(session.screen(),recipe),"backed by resource");
    ids[k]=session.createRecipe(recipe,session.document().root());
  }
  for(u32 k=0;k<6;++k) {
    const auto *object=session.document().find(ids[k]);AE_EXPECT_TRUE(object,"complete creation");
    AE_EXPECT_TRUE(object->components.find(scene::MeshRenderer::descriptor) && object->components.find(scene::PhysicsBody::descriptor),"render and physics consumers");
    const auto *collider=static_cast<const scene::Collider*>(object->components.find(scene::Collider::descriptor));
    AE_EXPECT_TRUE(collider && collider->valid(),"collider attached");
    AE_EXPECT_TRUE(!collider->convex,"primitive cylinder no longer requires mesh cooking");
    if(k==3) AE_EXPECT_TRUE(collider->shape==scene::ColliderShape::Cylinder&&collider->halfHeight==1.f,"cylinder collision matches visual height");
    AE_EXPECT_TRUE(runtime::meshRenderer(*object)->asset.valid(),"persistent resource identity before save");
  }
  AE_EXPECT_TRUE(session.history().undo(session.document()),"undo last primitive");
  AE_EXPECT_TRUE(!session.document().exists(ids[5]),"whole object removed");
  AE_EXPECT_TRUE(session.history().redo(session.document()),"redo preserves identity");
  EditorDocument restored;AE_EXPECT_TRUE(deserializeEditorDocument(serializeEditorDocument(session.document(),41),41,restored),"archive round trip");
  std::reverse(lib.draws.begin()+1,lib.draws.end());EditorMapScene resources;
  AE_EXPECT_TRUE(resources.adoptPackage(restored,lib.draws,lib.materials,lib.vertices,lib.indices,{},41),"resource order changed");
  for(u32 k=0;k<6;++k) {
    const auto &mesh=*runtime::meshRenderer(*restored.find(ids[k]));
    AE_EXPECT_EQ(renderer::primitiveFromFlags(resources.materialFlagsForAsset(mesh.mesh-1)),static_cast<scene::PrimitiveType>(k),"GUID finds correct geometry after reorder");
  }
  EditorPlayScene play;AE_EXPECT_TRUE(play.start(restored,resources),"all collider recipes cook in real Jolt backend");
  std::vector<renderer::MapDrawState> draws;AE_EXPECT_TRUE(play.extract(resources,draws),"render extraction");
  AE_EXPECT_EQ(draws.size(),usize{6},"all primitive renderers consumed");play.stop();
}

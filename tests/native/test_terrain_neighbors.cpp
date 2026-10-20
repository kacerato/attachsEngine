#include "harness.h"
#include "resources/terrain_neighbors.h"
#include <algorithm>
#include <limits>
using namespace ae::resources;
namespace {
TerrainNeighborTile tile(ae::u64 id,double x,double z,float h) {
  TerrainNeighborTile t; t.group=77;t.member=id;t.x=x;t.z=z;
  t.field.width=t.field.depth=3;t.field.height=1;t.field.step=1;
  t.field.heights.assign(9,h);t.layers={10,20};
  for(int i=0;i<9;++i) {t.weights.push_back(1);t.weights.push_back(0);}
  return t;
}
}
AE_TEST(terrain_neighbors_corner_identity_and_determinism) {
  std::vector<TerrainNeighborTile> a{tile(4,0,0,0),tile(3,2,0,2),tile(2,0,2,4),tile(1,2,2,6)};
  a[1].layers={20,10}; // Physical layer 20, despite same numeric weight slot.
  a[2].y=2;
  auto b=a;std::reverse(b.begin(),b.end());std::string e;
  AE_EXPECT_TRUE(reconcileTerrainNeighbors(a,e),e.c_str());
  AE_EXPECT_TRUE(reconcileTerrainNeighbors(b,e),e.c_str());
  AE_EXPECT_EQ(a[0].field.heights[8],3.5f,"four-way corner averages world heights once");
  AE_EXPECT_EQ(a[2].field.heights[2],1.5f,"shared world Y converted back to local height");
  AE_EXPECT_EQ(a[0].weights[8*2],.75f,"layer 10 follows identity");
  AE_EXPECT_EQ(a[0].weights[8*2+1],.25f,"layer 20 follows identity");
  AE_EXPECT_EQ(a[0].field.heights[4],0.f,"interior untouched");
  for(size_t i=0;i<a.size();++i) {
    auto& other=b[b.size()-1-i];
    AE_EXPECT_EQ(a[i].field.heights,other.field.heights,"tile order independent heights");
    AE_EXPECT_EQ(a[i].weights,other.weights,"tile order independent weights");
  }
  auto old=a;
  AE_EXPECT_TRUE(reconcileTerrainNeighbors(a,e),e.c_str());
  AE_EXPECT_EQ(a[0].weights,old[0].weights,"reconciliation idempotent");
}
AE_TEST(terrain_neighbors_validation_is_atomic_and_group_scoped) {
  std::vector<TerrainNeighborTile> a{tile(1,0,0,0),tile(2,2.00005,0,2)};
  std::vector<TerrainNeighborLink> links;std::string e;
  AE_EXPECT_TRUE(terrainNeighborLinks(a,links,e),e.c_str());
  AE_EXPECT_EQ(links.size(),size_t(1),"tolerant full edge recognized");
  a[1].field.depth=5;a[1].field.step=.5f;a[1].field.width=3;
  a[1].field.heights.assign(15,2);a[1].weights.assign(30,0);
  for(size_t i=0;i<15;++i)a[1].weights[i*2]=1;
  auto old=a;
  AE_EXPECT_TRUE(!reconcileTerrainNeighbors(a,e),"resolution mismatch rejected");
  AE_EXPECT_TRUE(e.find("resolution")!=std::string::npos,"explicit resolution error");
  AE_EXPECT_EQ(a[0].field.heights,old[0].field.heights,"failed operation atomic");
  a[1].group=88;
  AE_EXPECT_TRUE(terrainNeighborLinks(a,links,e),e.c_str());
  AE_EXPECT_TRUE(links.empty(),"different groups isolated");
  a[1].member=1;
  AE_EXPECT_TRUE(!terrainNeighborLinks(a,links,e),"duplicate stable member rejected");
}
AE_TEST(terrain_neighbors_union_partial_overlap_and_invalid_weights) {
  std::vector<TerrainNeighborTile> a{tile(1,0,0,0),tile(2,2,0,2)};
  a[1].layers={30,20};std::string e;
  AE_EXPECT_TRUE(reconcileTerrainNeighbors(a,e),e.c_str());
  AE_EXPECT_EQ(a[0].layers,(std::vector<TerrainIdentity>{10,20,30}),"sorted component layer union");
  AE_EXPECT_EQ(a[0].weights[2*3],.5f,"original identity preserved");
  AE_EXPECT_EQ(a[0].weights[2*3+2],.5f,"new neighbor identity propagated");
  a[1].z=.5;
  AE_EXPECT_TRUE(!reconcileTerrainNeighbors(a,e),"partial edge rejected");
  a[1].z=0;a[0].weights[0]=std::numeric_limits<float>::quiet_NaN();
  AE_EXPECT_TRUE(!reconcileTerrainNeighbors(a,e),"NaN weight rejected");
}
AE_TEST(terrain_neighbors_descriptor_roundtrip_bind_and_atomic_parse) {
  TerrainNeighborMember m;m.group=77;m.member=1;m.x=-23.1234567890123;
  m.y=12;m.z=2;m.sourceAsset="terrain://stable/asset \\\"a\"";m.layers={20,10};
  TerrainNeighborDescriptor d{{m}},loaded;std::string text,e;
  AE_EXPECT_TRUE(serializeTerrainNeighbors(d,text,e),e.c_str());
  AE_EXPECT_TRUE(text.find("heights")==std::string::npos,"descriptor excludes grids");
  AE_EXPECT_TRUE(deserializeTerrainNeighbors(text,loaded,e),e.c_str());
  AE_EXPECT_EQ(loaded.members[0].x,m.x,"double coordinate exact roundtrip");
  AE_EXPECT_EQ(loaded.members[0].sourceAsset,m.sourceAsset,"escaped source identity roundtrip");
  AE_EXPECT_EQ(loaded.members[0].layers,m.layers,"material order preserved");
  auto f=tile(99,0,0,3);TerrainNeighborTile bound;
  AE_EXPECT_TRUE(bindTerrainNeighbor(loaded.members[0],f.field,f.weights,bound,e),e.c_str());
  AE_EXPECT_EQ(bound.member,m.member,"binding uses persisted member ID");
  AE_EXPECT_EQ(bound.x,m.x,"binding uses persisted coordinates");
  AE_EXPECT_EQ(bound.layers,m.layers,"binding uses persisted material order");
  for(auto bad:std::vector<std::string>{text+"extra",text.substr(0,text.size()-4),
      "AETERRAINNEIGHBORS1 4097", "AETERRAINNEIGHBORS2 0",
      "AETERRAINNEIGHBORS1 1 1 1 nan 0 0 \"asset\" 1 10",
      "AETERRAINNEIGHBORS1 1 1 -1 0 0 0 \"asset\" 1 10",
      "AETERRAINNEIGHBORS1 1 1 1 0 0 0 \"asset\" 2 10 10",
      "AETERRAINNEIGHBORS1 2 1 1 0 0 0 \"a\" 1 10 1 1 2 0 0 \"b\" 1 10"}) {
    AE_EXPECT_TRUE(!deserializeTerrainNeighbors(bad,loaded,e),"malformed descriptor rejected");
    AE_EXPECT_EQ(loaded.members[0].sourceAsset,m.sourceAsset,"failed parse preserves output");
  }
  auto prior=bound;
  AE_EXPECT_TRUE(!bindTerrainNeighbor(m,f.field,std::span<const float>{},bound,e),"invalid loaded weights rejected");
  AE_EXPECT_EQ(bound.member,prior.member,"failed binding preserves output");
}

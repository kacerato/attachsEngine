#include "harness.h"
#include "renderer/sphere_mesh.h"
#include "renderer/texture_payload.h"
#include <array>
#include <limits>
using namespace ae;
using namespace ae::renderer;
using namespace ae::rhi;
AE_TEST(Sphere_mesh_bounds_tangents_and_seam) {
  MeshData mesh;
  AE_EXPECT_TRUE(makeUvSphere(32,16,mesh),"sphere");
  AE_EXPECT_EQ(mesh.vertices.size(),33u*17u,"vertex count");
  AE_EXPECT_EQ(mesh.indices.size(),32u*15u*6u,"no degenerate pole triangles");
  for(const auto &v:mesh.vertices) {
    float length=0,dot=0;
    for(int i=0;i<3;++i){length+=v.normal[i]*v.normal[i];dot+=v.normal[i]*v.tangent[i];}
    AE_EXPECT_TRUE(std::abs(length-1)<1e-5f,"unit normal");
    AE_EXPECT_TRUE(std::abs(dot)<1e-5f,"orthogonal tangent");
  }
  for(u32 y=0;y<=16;++y) {
    const auto &a=mesh.vertices[y*33],&b=mesh.vertices[y*33+32];
    for(int i=0;i<3;++i)AE_EXPECT_EQ(a.position[i],b.position[i],"seam positions match");
    AE_EXPECT_EQ(a.uv[0],0,"seam starts at zero");AE_EXPECT_EQ(b.uv[0],1,"seam ends at one");
  }
}
AE_TEST(Sphere_triangles_nonzero_outward_and_in_range) {
  MeshData mesh;AE_EXPECT_TRUE(makeUvSphere(32,16,mesh),"sphere");
  for(usize i=0;i<mesh.indices.size();i+=3) {
    for(usize j=0;j<3;++j)AE_EXPECT_TRUE(mesh.indices[i+j]<mesh.vertices.size(),"index in range");
    const auto *a=mesh.vertices[mesh.indices[i]].position,*b=mesh.vertices[mesh.indices[i+1]].position,
        *c=mesh.vertices[mesh.indices[i+2]].position;
    float u[3]={b[0]-a[0],b[1]-a[1],b[2]-a[2]},v[3]={c[0]-a[0],c[1]-a[1],c[2]-a[2]};
    float n[3]={u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]};
    AE_EXPECT_TRUE(n[0]*a[0]+n[1]*a[1]+n[2]*a[2]>0,"outward triangle");
  }
}
AE_TEST(Sphere_invalid_dimensions_preserve_destination) {
  MeshData mesh;mesh.indices.push_back(42);
  AE_EXPECT_TRUE(!makeUvSphere(2,16,mesh),"too few");
  AE_EXPECT_TRUE(!makeUvSphere(32,1,mesh),"rings");
  AE_EXPECT_TRUE(!makeUvSphere(4096,4096,mesh),"bounded memory");
  AE_EXPECT_EQ(mesh.indices[0],42,"no partial writes");
}
AE_TEST(Mip_chain_accounts_compressed_tail_and_rejects_overflow) {
  AE_EXPECT_EQ(sampledMipByteSize(VK_FORMAT_ASTC_6x6_UNORM_BLOCK,1,1),16,"tail block");
  AE_EXPECT_EQ(sampledMipByteSize(VK_FORMAT_ASTC_6x6_UNORM_BLOCK,7,7),64,"round blocks up");
  AE_EXPECT_EQ(sampledMipByteSize(VK_FORMAT_R16G16B16A16_SFLOAT,8,8),512,"HDR format");
  AE_EXPECT_EQ(sampledMipByteSize(VK_FORMAT_R16G16B16A16_SFLOAT,UINT32_MAX,UINT32_MAX),0,"overflow");
  ImageDesc d{8,8,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,VK_IMAGE_ASPECT_COLOR_BIT};
  d.mipLevels=4;AE_EXPECT_EQ(sampledChainByteSize(d),340,"8+4+2+1 chain");
  AE_EXPECT_TRUE(!isRgba8UploadValid(d,256),"legacy one mip contract");
  d.mipLevels=5;AE_EXPECT_TRUE(!isImageDescValid(d),"too many mips");
  d.mipLevels=0;AE_EXPECT_TRUE(!isImageDescValid(d),"zero mips");
}
AE_TEST(Texture_policy_reduces_resident_chain_not_source_identity) {
  ImageDesc d{8192,8192,VK_FORMAT_ASTC_6x6_SRGB_BLOCK,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,VK_IMAGE_ASPECT_COLOR_BIT};
  d.mipLevels=14;
  AE_EXPECT_EQ(chooseResidentMip(d,8192,64ull*1024*1024),0,"8K fits");
  AE_EXPECT_EQ(chooseResidentMip(d,4096,64ull*1024*1024),1,"capability limit");
  AE_EXPECT_EQ(chooseResidentMip(d,8192,16ull*1024*1024),1,"budget limit");
  AE_EXPECT_EQ(chooseResidentMip(d,8192,1),14,"cannot fit even tail");
}
AE_TEST(Texture_payload_rejects_truncation_version_and_unknown_encoding) {
  std::array<u8,32> bytes{};
  auto set=[&](usize offset,u32 v){for(int i=0;i<4;++i)bytes[offset+i]=static_cast<u8>(v>>(8*i));};
  set(0,0x58544541);set(4,1);set(8,4);set(12,4);set(16,3);set(20,1);set(24,64);
  TexturePayload payload;
  AE_EXPECT_TRUE(decodeTextureHeader(bytes,96,payload),"valid payload");
  AE_EXPECT_TRUE(!decodeTextureHeader(bytes,95,payload),"truncated");
  set(4,2);AE_EXPECT_TRUE(!decodeTextureHeader(bytes,96,payload),"future version");
  set(4,1);set(16,99);AE_EXPECT_TRUE(!decodeTextureHeader(bytes,96,payload),"unknown format");
  set(16,3);set(8,65536);AE_EXPECT_TRUE(!decodeTextureHeader(bytes,96,payload),"dimension limit");
}

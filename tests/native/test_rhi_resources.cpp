#include "harness.h"
#include "rhi/memory_allocator.h"
#include "rhi/resource.h"
#include "rhi/surface_transform.h"

#include <cmath>
#include <limits>

using namespace ae::rhi;
using namespace ae::test;

AE_TEST(RhiImageDesc_aceita_textura_e_render_target_validos) {
  ImageDesc texture{};
  texture.width = 64;
  texture.height = 64;
  texture.format = VK_FORMAT_R8G8B8A8_SRGB;
  texture.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  texture.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  texture.memoryClass = MemoryClass::Texture;
  AE_EXPECT_TRUE(isImageDescValid(texture), "textura 2D completa deveria ser valida");

  ImageDesc depth = texture;
  depth.format = VK_FORMAT_D32_SFLOAT;
  depth.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  depth.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  depth.memoryClass = MemoryClass::RenderTarget;
  AE_EXPECT_TRUE(isImageDescValid(depth), "depth attachment completo deveria ser valido");
}

AE_TEST(RhiImageDesc_rejeita_dimensao_formato_aspecto_ou_classe_invalida) {
  ImageDesc desc{};
  desc.width = 32;
  desc.height = 32;
  desc.format = VK_FORMAT_R8G8B8A8_UNORM;
  desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
  desc.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  desc.memoryClass = MemoryClass::Texture;
  desc.width = 0;
  AE_EXPECT_TRUE(!isImageDescValid(desc), "largura zero deve ser recusada");
  desc.width = 32;
  desc.height = 0;
  AE_EXPECT_TRUE(!isImageDescValid(desc), "altura zero deve ser recusada");
  desc.height = 32;
  desc.format = VK_FORMAT_UNDEFINED;
  AE_EXPECT_TRUE(!isImageDescValid(desc), "formato indefinido deve ser recusado");
  desc.format = VK_FORMAT_R8G8B8A8_UNORM;
  desc.aspectMask = 0;
  AE_EXPECT_TRUE(!isImageDescValid(desc), "aspecto ausente deve ser recusado");
  desc.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  desc.memoryClass = MemoryClass::Buffer;
  AE_EXPECT_TRUE(!isImageDescValid(desc), "imagem nao pode consumir a cota de buffer");
  desc.memoryClass = static_cast<MemoryClass>(99);
  AE_EXPECT_TRUE(!isImageDescValid(desc), "classe de memoria desconhecida deve ser recusada");
}

AE_TEST(RhiUpload_rejeita_tamanho_formato_uso_e_overflow_antes_da_GPU) {
  ImageDesc desc{64, 64, VK_FORMAT_R8G8B8A8_SRGB,
                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                 VK_IMAGE_ASPECT_COLOR_BIT, MemoryClass::Texture};
  AE_EXPECT_TRUE(isRgba8UploadValid(desc, 64 * 64 * 4), "RGBA8 compacto deve ser aceito");
  AE_EXPECT_TRUE(!isRgba8UploadValid(desc, 64 * 64 * 4 - 1), "nao ler alem do staging");
  AE_EXPECT_TRUE(!isRgba8UploadValid(desc, 64 * 64 * 4 + 1), "padding exige contrato proprio");
  desc.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
  AE_EXPECT_TRUE(!isRgba8UploadValid(desc, 64 * 64 * 4), "transfer dst obrigatorio");
  desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  AE_EXPECT_TRUE(!isRgba8UploadValid(desc, 64 * 64 * 4), "sampled obrigatorio");
  desc.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
  desc.format = VK_FORMAT_D32_SFLOAT;
  AE_EXPECT_TRUE(!isRgba8UploadValid(desc, 64 * 64 * 4), "depth nao e RGBA8");
  desc.format = VK_FORMAT_R8G8B8A8_UNORM;
  desc.width = desc.height = std::numeric_limits<ae::u32>::max();
  AE_EXPECT_TRUE(!isRgba8UploadValid(desc, 4), "dimensoes nao podem causar overflow");
}

AE_TEST(RhiSurfaceTransform_preserva_proporcao_em_todas_rotacoes_e_espelhos) {
  constexpr VkSurfaceTransformFlagBitsKHR flags[] = {
      VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR, VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR,
      VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR, VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR,
      VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR,
      VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR,
      VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR,
      VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR};
  for (auto flag : flags) {
    SurfaceTransform transform{};
    AE_EXPECT_TRUE(describeSurfaceTransform(flag, transform), "transform concreta suportada");
    const VkExtent2D display{2772, 1280};
    const VkExtent2D image = transformSurfaceExtent(display, transform);
    const VkExtent2D roundTrip = transformSurfaceExtent(image, transform);
    AE_EXPECT_TRUE(roundTrip.width == display.width && roundTrip.height == display.height,
                   "display e imagem natural devem fazer round trip");
    const float xPixels = std::hypot(transform.xx * image.width, transform.yx * image.height) /
                          (static_cast<float>(display.width) / display.height);
    const float yPixels = std::hypot(transform.xy * image.width, transform.yy * image.height);
    AE_EXPECT_TRUE(std::abs(xPixels - yPixels) < 0.01f, "cubo nao pode ser achatado pela surface");
  }
  SurfaceTransform transform{};
  describeSurfaceTransform(VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR, transform);
  AE_EXPECT_TRUE(transform.xy == -1 && transform.yx == 1, "90 graus tem sentido definido");
  AE_EXPECT_TRUE(!describeSurfaceTransform(VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR, transform),
                 "inherit nao pode fingir uma orientacao concreta");
}

AE_TEST(RhiSamplerDesc_valida_lod_e_anisotropia_explicitamente) {
  SamplerDesc desc{};
  AE_EXPECT_TRUE(isSamplerDescValid(desc), "sampler default deveria ser valido");
  desc.maxLod = -1.0f;
  AE_EXPECT_TRUE(!isSamplerDescValid(desc), "maxLod negativo deve ser recusado");
  desc.maxLod = 0.0f;
  desc.enableAnisotropy = true;
  desc.maxAnisotropy = 1.0f;
  AE_EXPECT_TRUE(!isSamplerDescValid(desc), "anisotropia habilitada precisa de fator maior que um");
}

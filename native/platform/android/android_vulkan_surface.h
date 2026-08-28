#pragma once

#include "rhi/device.h"

struct ANativeWindow;

namespace ae::platform::android {

// Vertical slice do item 0.1.3 (instance, device, fila de apresentação e
// surface Android) estendido pelo item 5.2 do plano de lacunas ("shell
// gráfico mínimo"): agora também possui a swapchain de apresentação —
// unidas aqui porque swapchain depende diretamente de device+surface, e
// separar em outro tipo só empurraria o mesmo acoplamento para o chamador.
// A renderização de fato (pipeline/comandos) fica em TriangleRenderer
// (android_triangle_renderer.h), que consome só ISwapchain/imageView() —
// mantém a possibilidade futura de trocar o conteúdo desenhado sem tocar
// aqui.
class AndroidVulkanSurface final {
public:
  AndroidVulkanSurface() = default;
  ~AndroidVulkanSurface();

  AndroidVulkanSurface(const AndroidVulkanSurface &) = delete;
  AndroidVulkanSurface &operator=(const AndroidVulkanSurface &) = delete;

  bool initialize(ANativeWindow *window, bool allowBindless = true);
  void shutdown();
  bool isReady() const { return surface_ != VK_NULL_HANDLE && swapchain_.isReady(); }

  // Recria a swapchain para o tamanho atual da janela — chamar quando
  // OutOfDateMustRecreate/SurfaceLost forem reportados por acquire/present,
  // ou quando APP_CMD_CONFIG_CHANGED indicar novo tamanho.
  bool recreateSwapchain(ANativeWindow *window);

  rhi::VulkanDevice &device() { return device_; }
  rhi::VulkanSwapchain &swapchain() { return swapchain_; }

private:
  rhi::VulkanDevice device_;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  rhi::VulkanSwapchain swapchain_;
};

} // namespace ae::platform::android

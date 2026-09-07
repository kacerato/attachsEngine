// O atlas de ícones da interface.
//
// O arquivo `.aeui` é produzido por `tools/pack-icon-atlas.py` na mesma execução
// que gera `ui_icon_id.h`. Os dois são um par: o índice do enum É a posição no
// binário. É por isso que a validação abaixo compara `kUiIconCount` com o que o
// arquivo traz e recusa a divergência — um binário de outra geração desenharia
// o ícone errado sem nenhum sintoma.
//
// Sem Vulkan, sem Android, sem I/O: recebe bytes já lidos e testável no host.
#pragma once

#include "core/base.h"
#include "ui/ui_geometry.h"
#include "ui/ui_icon_id.h"

#include <span>
#include <vector>

namespace ae::ui {

class UiIconAtlas final {
public:
  // `bytes` continua pertencendo ao chamador e precisa sobreviver ao uso de
  // `pixels()`, pela mesma razão que em UiFont: são alguns MiB que existem para
  // ir à GPU uma vez.
  bool load(std::span<const u8> bytes);
  void unload() noexcept;
  bool isReady() const noexcept { return ready_; }

  u32 width() const noexcept { return width_; }
  u32 height() const noexcept { return height_; }
  // RGBA8, alfa direto (não pré-multiplicado), uma linha após a outra.
  std::span<const u8> pixels() const noexcept { return pixels_; }

  // Retângulo em texels. Vazio para UiIcon::None e para um índice fora da
  // tabela — desenhar o slot zero por engano é o erro que o enum existe para
  // impedir, e devolver vazio o mantém impedido também em tempo de execução.
  UiRect rectOf(UiIcon icon) const noexcept;

private:
  bool ready_ = false;
  u32 width_ = 0;
  u32 height_ = 0;
  std::vector<UiRect> rects_;
  std::span<const u8> pixels_;
};

} // namespace ae::ui

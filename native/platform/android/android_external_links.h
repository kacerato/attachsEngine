#pragma once
#include <string>

namespace ae::platform::android {
// Enfileira um link https para o shell Java abrir no navegador. Um pedido novo
// substitui o anterior ainda não aberto; outros esquemas são descartados.
void requestExternalLink(std::string link);
} // namespace ae::platform::android

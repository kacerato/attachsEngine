#pragma once
#include "core/base.h"
#include <string>
#include <vector>

namespace ae::platform::android {

// A ponte entre o editor e o seletor de arquivos do Android.
//
// Mesma forma da ponte de texto: o nativo levanta um pedido, o Java pergunta
// por ele em intervalos e devolve o resultado. O `content://` do seletor NUNCA
// vira caminho POSIX — quem lê os bytes é o `ContentResolver` do lado Java, e
// o que atravessa a fronteira é o conteúdo, com o nome que o provedor declara.
//
// Sem isso, a alternativa seria pedir permissão de armazenamento e adivinhar um
// caminho a partir do URI; em Android moderno esse caminho frequentemente não
// existe, e quando existe não é legível.
struct ModelPickerResult {
  std::vector<u8> bytes;
  std::string displayName;
  bool accepted = false;
  // Preenchido quando o lado Java não conseguiu ler: arquivo grande demais,
  // permissão revogada, provedor que sumiu. Nunca "erro ao importar".
  std::string diagnostic;
};

// Pede um arquivo ao usuário. Só um pedido por vez: um segundo pedido enquanto
// o seletor está aberto é ignorado, em vez de empilhar diálogos.
void requestModelPick();
void cancelModelPick();
bool modelPickPending();
// Devolve e CONSOME o resultado, quando há um.
bool takeModelPickResult(ModelPickerResult &out);

} // namespace ae::platform::android

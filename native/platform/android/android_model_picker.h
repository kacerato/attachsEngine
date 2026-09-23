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
  // Seleção múltipla (Entrega 4): os arquivos escolhidos junto com o principal,
  // candidatos a dependência de um glTF. Casados por nome, nunca por caminho.
  struct Companion {
    std::string name;
    std::vector<u8> bytes;
  };
  std::vector<Companion> companions;
};

// A new request invalidates the old token. Textures and HDRI use a single
// document; only a model can include its external dependency files.
void requestModelPick(bool allowCompanions = true);
void cancelModelPick();
bool modelPickPending();
// Devolve e CONSOME o resultado, quando há um.
bool takeModelPickResult(ModelPickerResult &out);

} // namespace ae::platform::android

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
  // Pasta de modelo (S0): o principal está em `bytes`, no caminho `mainRelative`
  // dentro da pasta listada; `folderFiles` é a listagem inteira (caminho e
  // tamanho, -1 quando o provedor não diz). Nenhum outro conteúdo atravessou.
  struct FolderFile {
    std::string relative;
    i64 bytes = -1;
  };
  bool folder = false;
  std::string folderName, mainRelative;
  std::vector<FolderFile> folderFiles;
};

// A new request invalidates the old token. Textures and HDRI use a single
// document; only a model can include its external dependency files.
void requestModelPick(bool allowCompanions = true);
void cancelModelPick();
bool modelPickPending();
// Devolve e CONSOME o resultado, quando há um.
bool takeModelPickResult(ModelPickerResult &out);
// Seletor de PASTA (S0). O resultado chega por `takeModelPickResult` com `folder`.
void requestFolderPick();

// Cópia dos arquivos de uma pasta listada para o preparo do projeto. `sources`
// são caminhos da listagem; `targets`, caminhos relativos a `destination` (já
// normalizados). O Java calcula o SHA-256 enquanto copia.
struct FolderCopyRequest {
  std::vector<std::string> sources, targets;
  std::string destination;
  u64 totalBytes = 0;
};
struct FolderCopyState {
  u64 doneBytes = 0, totalBytes = 0;
  u32 files = 0;
  bool finished = false, cancelled = false;
  std::vector<std::string> sha256; // um por arquivo, na ordem do pedido
  std::string diagnostic;
};
u64 requestFolderCopy(FolderCopyRequest request);
// Falso quando o token não é mais o da cópia atual.
bool folderCopyState(u64 token, FolderCopyState &out);
void cancelFolderCopy(u64 token);

} // namespace ae::platform::android

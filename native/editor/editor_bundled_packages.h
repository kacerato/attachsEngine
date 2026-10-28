#pragma once
#include "core/base.h"
#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

// Pacotes incluídos no APK: cada arquivo do pacote é um asset do APK com nome
// neutro (bundled-packages/p0/f00000.bin), listado num índice com o caminho
// original, o tamanho e o SHA-256. Na primeira importação o shell extrai o
// pacote para a pasta de pacotes do app; daí em diante a importação é a mesma
// da "Pasta de pacote": cópia para o preparo do projeto e prepareLibraryPackage.
namespace ae::editor {
inline constexpr const char *BundledPackageIndexAsset="bundled-packages/index.tsv";
inline constexpr const char *BundledPackageMarker=".astra-incluido";

struct BundledPackageFile {std::string relative,asset,sha256;u64 bytes=0;};
struct BundledPackage {
  std::string name,directory;
  std::vector<BundledPackageFile> files;
  u64 bytes=0;
  // Hash das linhas do pacote no índice: marca a extração na pasta de pacotes.
  std::string indexHash;
};

// Formato (UTF-8, campos separados por tabulação):
//   ASTRA_BUNDLED_PACKAGES_1
//   package <nome> <diretório de assets>
//   file <asset> <bytes> <sha256> <caminho relativo original>
bool parseBundledPackageIndex(std::string_view text,std::vector<BundledPackage> &packages,std::string &diagnostic);

// A pasta extraída está completa para este índice (marca igual e cada arquivo
// com o tamanho esperado)? Evita extrair de novo a cada importação.
bool bundledPackageExtracted(const std::filesystem::path &directory,const BundledPackage &package);
bool writeBundledPackageMarker(const std::filesystem::path &directory,const BundledPackage &package);

// Copia a pasta extraída para o preparo do projeto conferindo o SHA-256 de cada
// arquivo contra o índice. `done` acumula bytes copiados para a barra de estado.
bool copyBundledPackageToStaging(const std::filesystem::path &source,const std::filesystem::path &staging,
    const BundledPackage &package,const std::function<bool()> &cancelled,std::atomic<u64> &done,std::string &diagnostic);
}

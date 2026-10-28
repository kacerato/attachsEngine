#pragma once
#include "editor/editor_bundled_packages.h"

struct AAssetManager;

namespace ae::platform::android {
// Lê bundled-packages/index.tsv do APK. Sem o índice (APK sem pacotes), a lista
// volta vazia e sem erro.
std::vector<editor::BundledPackage> readBundledPackages(AAssetManager *assets,std::string &diagnostic);

// Extrai o pacote dos assets do APK para `directory`, conferindo o SHA-256 de
// cada arquivo, e grava a marca no fim. Se a pasta já está completa para este
// índice, não copia nada. `done` acumula bytes para a barra de estado.
bool extractBundledPackage(AAssetManager *assets,const editor::BundledPackage &package,const std::filesystem::path &directory,
    const std::function<bool()> &cancelled,std::atomic<u64> &done,std::string &diagnostic);
}

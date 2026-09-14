#pragma once
#include "core/base.h"
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ae::resources {
// Dependências externas de um glTF (M08/M09 Entrega 4, trilha 3).
//
// Um `.gltf` aponta para `.bin` e imagens por URI; um GLB também pode apontar.
// No Android o arquivo chega pelo seletor (SAF) como CONTEÚDO, com o nome que o
// provedor declara — não há pasta, e `content://` nunca vira caminho POSIX. Por
// isso as dependências são os arquivos que o usuário escolheu JUNTO com o
// principal, casados pelo último segmento da URI:
//
// - `data:` em base64 é decodificado;
// - URI com esquema (http, https, file, content…) ou caminho absoluto é recusada:
//   nada de rede implícita nem de busca pelo armazenamento;
// - segmento `..` é recusado; nome que casa com dois arquivos é ambíguo e recusado;
// - dependência que falta é listada no diagnóstico, todas de uma vez.
//
// O resultado é um GLB autocontido: buffers e imagens copiados para um único
// bloco binário, com as bufferViews (inclusive as de meshopt) reendereçadas. É
// esse GLB que o projeto guarda, então reabrir funciona offline e sem a
// permissão temporária do seletor. O manifesto registra de onde veio cada byte.
struct GltfPackageFile {
  std::string name;
  std::span<const u8> bytes;
};

struct GltfDependency {
  std::string uri;    // como está no arquivo (limitada a 512 bytes)
  std::string file;   // nome do arquivo escolhido que casou, ou vazio para data:
  std::string sha256;
  u64 bytes = 0;
  bool dataUri = false;
};

struct GltfPackage {
  std::vector<u8> glb;
  std::vector<GltfDependency> dependencies;
  u32 unusedFiles = 0; // escolhidos junto, mas não referenciados
};

// Verdadeiro para `.gltf` de texto e para GLB com buffer ou imagem por URI.
bool gltfNeedsPackage(std::span<const u8> main);

bool packGltf(std::span<const u8> main, std::span<const GltfPackageFile> companions, u64 maximumBytes,
              GltfPackage &out, std::string &diagnostic);

// ASTRA_GLTF_DEPS 1: principal e dependências com tamanho e SHA-256.
std::string serializeGltfManifest(std::string_view mainName, std::span<const u8> main, const GltfPackage &package);
} // namespace ae::resources

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

// --- Fonte em pasta (S0) -----------------------------------------------------
//
// Uma cena grande (o Sponza da Intel: `.bin` de 140 MB e 72 PNG 4K, 1,9 GB) não
// cabe no empacotamento acima, que junta tudo num GLB em memória. Importada
// como PASTA, a fonte fica no projeto com os arquivos originais nos mesmos
// caminhos relativos; só os buffers entram em memória, e cada imagem é lida do
// disco pelo importador na hora de decodificar (`GltfImportProgress::externalFile`).
// As dependências casam pelo CAMINHO relativo ao principal, não pelo nome:
// numa pasta, `a/cor.png` e `b/cor.png` são arquivos diferentes.

// Caminho relativo normalizado de uma URI do glTF: decodificação percentual,
// segmentos `.` removidos, separador '/'. Recusa esquema (http:, file:,
// content:…), caminho absoluto, barra invertida e `..` — a fonte nunca sai da
// própria pasta. `data:` não é caminho: devolve falso com `refusal` vazio.
bool gltfRelativeUri(std::string_view uri, std::string &relative, std::string &refusal);

struct GltfFolderDependency {
  std::string uri;      // como está no arquivo (até 512 bytes)
  std::string relative; // normalizado, relativo à pasta do principal
  bool image = false;   // falso: buffer
};
// Buffers e imagens por URI do principal, sem repetição, na ordem do arquivo.
// `data:` e o bloco binário de um GLB não são dependência. Falha fechada em
// JSON inválido ou URI recusada, com o motivo.
bool listGltfFolderDependencies(std::span<const u8> main, std::vector<GltfFolderDependency> &out,
                                std::string &diagnostic);

struct GltfFolder {
  // Lê um arquivo da pasta pelo caminho relativo normalizado; falso quando não existe.
  bool (*read)(void *context, const std::string &relative, std::vector<u8> &bytes) = nullptr;
  void *context = nullptr;
};
// GLB com os BUFFERS da pasta embutidos e as imagens ainda por URI: é o que o
// importador lê, com `externalFile` apontando para a mesma pasta. As
// dependências registradas são só as dos buffers (as imagens são conferidas
// pela cópia, que já calcula tamanho e SHA-256 de cada arquivo).
bool packGltfFolder(std::span<const u8> main, const GltfFolder &folder, u64 maximumBytes, GltfPackage &out,
                    std::string &diagnostic);
} // namespace ae::resources

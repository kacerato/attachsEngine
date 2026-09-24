#pragma once
#include "resources/gltf_import.h"
#include "resources/gltf_package.h"
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace ae::resources {
// Fonte glTF em PASTA no disco (S0): o principal `.gltf` com os arquivos que ele
// referencia nos mesmos caminhos relativos, como vieram do autor.
//
// É a forma que uma cena grande entra no projeto (Sponza da Intel: 140 MB de
// buffer e 1,9 GB de PNG 4K). Nada é juntado num GLB em memória: os buffers são
// lidos para o GLB que o importador consome, e cada imagem é lida do disco na
// hora de decodificar, uma de cada vez. Primeira importação (da pasta de
// preparo) e reabertura (da pasta publicada) passam pelas mesmas funções.

// Maior arquivo lido da pasta de uma vez: o `.bin` do Sponza tem 140 MB.
inline constexpr u64 GltfFolderMaximumFileBytes = 512ull << 20;

// Lê `relative` (normalizado por `gltfRelativeUri`) dentro de `directory`.
// Recusa link simbólico em qualquer segmento e arquivo acima de `maximum`.
// `prefix` diferente de zero lê só os primeiros bytes (arquivo maior é aceito).
bool readGltfFolderFile(const std::filesystem::path &directory, const std::string &relative, std::vector<u8> &bytes,
                        u64 maximum = GltfFolderMaximumFileBytes, u64 prefix = 0);

// Verdadeiro quando o principal só pode ser lido como fonte em pasta: `.gltf`
// de texto, ou GLB, que aponta arquivos por URI. No projeto, a rota de seleção
// múltipla guarda o GLB já empacotado; o que ainda tem URI é pasta.
inline bool gltfIsFolderSource(std::span<const u8> main) { return gltfNeedsPackage(main); }

// Um arquivo da pasta como a cópia o viu: caminho relativo ao principal,
// tamanho e SHA-256 (calculado durante a cópia, sem reler 2 GB).
struct GltfFolderFileRecord {
  std::string relative;
  u64 bytes = 0;
  std::string sha256;
};
// ASTRA_GLTF_DEPS 2: principal e cada arquivo da pasta que ele referencia.
std::string serializeGltfFolderManifest(std::string_view mainName, std::span<const u8> main,
                                        std::span<const GltfFolderFileRecord> files);
// Conteúdo de uma fonte em pasta: principal + manifesto. Mudar uma textura
// muda o manifesto (SHA do arquivo), e portanto este hash — sem reler as imagens.
std::string gltfFolderContentHash(std::span<const u8> main, std::string_view manifest);

// Importa o principal `main`, cuja pasta é `directory`: buffers embutidos no
// GLB de trabalho (`packed`, também devolvido para quem calcula conteúdo) e
// imagens lidas do disco. `progress` do chamador segue valendo (cancelamento,
// relato); o leitor de imagens é acrescentado aqui.
bool importGltfFolder(std::span<const u8> main, const std::filesystem::path &directory, u64 maximumPackedBytes,
                      const GltfImportLimits &limits, const GltfImportProgress &progress, GltfImport &out,
                      std::vector<u8> &packed);
} // namespace ae::resources

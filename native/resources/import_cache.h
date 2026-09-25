#pragma once
#include "core/base.h"
#include "resources/gltf_import.h"
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ae::resources {
// Cache de derivados da importação (R2).
//
// Reabrir um projeto não precisa repetir o importador para uma fonte que não
// mudou: o `GltfImport` preparado (geometria, materiais, árvore, texturas já
// decodificadas e com mips, contadores da prévia) é gravado num arquivo
// regenerável e lido de volta. É a mesma separação que o DDC da Unreal faz entre
// fonte e derivado, feita localmente dentro do projeto.
//
// A chave é o SHA-256 de tudo o que muda a saída para os mesmos bytes: o hash do
// conteúdo da fonte (um `.gltf` chega empacotado, então as dependências já estão
// nele), o schema deste arquivo, a revisão do importador, as versões dos codecs e
// TODOS os limites de importação, inclusive o formato alvo (ASTC ou RGBA8).
// Qualquer diferença produz outra chave; um arquivo com a chave errada, truncado
// ou corrompido é recusado inteiro e o importador roda de novo.
//
// O cache nunca é fonte de verdade: apagar `.astra/cache/` custa só tempo.
inline constexpr u32 ImportCacheSchema = 7; // 7: blend shapes e canal de pesos
// Subir quando a SAÍDA de `importGlb` mudar para o mesmo arquivo e os mesmos
// limites (correção de importador, nova derivação de dados).
// 5: preserva minificação, magnificação e uso/interpolação de mip do sampler
// glTF separadamente. Derivados da revisão 4 tinham só os dois bits legados e
// não permitem reconstruir essa informação sem reler a fonte.
inline constexpr u32 ImportCacheImporterRevision = 7;

std::string importCacheKey(std::string_view sourceContentHash, const GltfImportLimits &limits);
// Caminho relativo à raiz do projeto.
std::string importCacheRelativePath(std::string_view key);
bool writeImportCache(const GltfImport &model, std::string_view key, std::vector<u8> &out);
// Bloco C (S2): leitura parcial. As texturas guardam na memória só os níveis
// com o maior lado até `keepDimension`; os de cima ficam no arquivo `path`
// (o mesmo que forneceu `bytes`) e são lidos sob demanda pelo streaming.
struct ImportCachePartialTextures {
  std::string path;
  u32 keepDimension = 256;
};
// Falha fechada: em qualquer inconsistência `out` volta vazio.
bool readImportCache(std::span<const u8> bytes, std::string_view key, GltfImport &out,
                     const ImportCachePartialTextures *partial = nullptr);
} // namespace ae::resources

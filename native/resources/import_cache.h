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
inline constexpr u32 ImportCacheSchema = 5; // 5: luzes KHR_lights_punctual
// Subir quando a SAÍDA de `importGlb` mudar para o mesmo arquivo e os mesmos
// limites (correção de importador, nova derivação de dados).
inline constexpr u32 ImportCacheImporterRevision = 4; // 4: materialização de KHR_lights_punctual

std::string importCacheKey(std::string_view sourceContentHash, const GltfImportLimits &limits);
// Caminho relativo à raiz do projeto.
std::string importCacheRelativePath(std::string_view key);
bool writeImportCache(const GltfImport &model, std::string_view key, std::vector<u8> &out);
// Falha fechada: em qualquer inconsistência `out` volta vazio.
bool readImportCache(std::span<const u8> bytes, std::string_view key, GltfImport &out);
} // namespace ae::resources

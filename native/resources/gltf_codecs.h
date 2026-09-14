#pragma once
#include "core/base.h"
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ae::resources {
// Adaptadores dos codecs de importação (M08/M09 Entrega 4). As bibliotecas
// ficam atrás desta interface — nenhum tipo delas vaza para o importador — e
// com versão/licença fixadas em native/third_party/*/VERSION.txt:
//
// | Extensão | Biblioteca | O que resolve |
// |---|---|---|
// | EXT_meshopt_compression | meshoptimizer v1.2 | bytes de uma bufferView |
// | KHR_draco_mesh_compression | Draco 1.5.7 | atributos e índices de uma primitiva |
// | KHR_texture_basisu (KTX2) | Basis Universal v2_50 | uma imagem, transcodificada para RGBA8 |
//
// São trilhas separadas: decodificar geometria não diz nada sobre textura. Toda
// função falha fechada, com a saída vazia e um diagnóstico concreto, e confere
// o orçamento de expansão ANTES de alocar o resultado.

// Uma bufferView de EXT_meshopt_compression. `mode` é ATTRIBUTES, TRIANGLES ou
// INDICES; `filter` é NONE (ou vazio), OCTAHEDRAL, QUATERNION, EXPONENTIAL ou COLOR.
// A saída tem exatamente `count * byteStride` bytes.
bool decodeMeshoptView(std::span<const u8> compressed, u32 count, u32 byteStride, std::string_view mode,
                       std::string_view filter, u64 maximumBytes, std::vector<u8> &out, std::string &diagnostic);

// Um atributo pedido ao bloco Draco: o id único vem do mapa `attributes` da
// extensão e o formato de saída vem do acessor glTF correspondente.
struct DracoAttributeRequest {
  u32 uniqueId = 0;
  u32 componentType = 0; // códigos glTF (5120..5126)
  u32 components = 0;
};

struct DracoPrimitive {
  u32 vertexCount = 0;
  std::vector<u32> indices;                // lista de triângulos
  std::vector<std::vector<u8>> attributes; // um por pedido, empacotado no componentType pedido
};

bool decodeDracoPrimitive(std::span<const u8> compressed, std::span<const DracoAttributeRequest> requests,
                          u64 maximumBytes, DracoPrimitive &out, std::string &diagnostic);

// KTX2: dimensões lidas só do cabeçalho, e transcodificação do nível 0 para
// RGBA8. Os mips são refeitos pelo mesmo caminho das imagens PNG/JPEG, para o
// limite de resolução residente e o espaço de cor valerem igual.
bool isKtx2(std::span<const u8> bytes);
bool readKtx2Dimensions(std::span<const u8> bytes, u32 &width, u32 &height);
bool transcodeKtx2Rgba8(std::span<const u8> bytes, u32 &width, u32 &height, std::vector<u8> &rgba,
                        std::string &diagnostic);
} // namespace ae::resources

// Escrito à mão pela Astra no lugar do arquivo que o CMake do Draco gera
// (cmake/draco_options.cmake, draco_generate_features_h). Mesmo conjunto que a
// configuração padrão do Draco 1.5.7 fora do Emscripten, sem transcoder nem plugins:
// só o decodificador é vendorizado (ver ../VERSION.txt).

#ifndef DRACO_FEATURES_H_
#define DRACO_FEATURES_H_

#define DRACO_POINT_CLOUD_COMPRESSION_SUPPORTED
#define DRACO_MESH_COMPRESSION_SUPPORTED
#define DRACO_NORMAL_ENCODING_SUPPORTED
#define DRACO_STANDARD_EDGEBREAKER_SUPPORTED
#define DRACO_PREDICTIVE_EDGEBREAKER_SUPPORTED
#define DRACO_BACKWARDS_COMPATIBILITY_SUPPORTED
#define DRACO_ATTRIBUTE_INDICES_DEDUPLICATION_SUPPORTED
#define DRACO_ATTRIBUTE_VALUES_DEDUPLICATION_SUPPORTED

#endif  // DRACO_FEATURES_H_

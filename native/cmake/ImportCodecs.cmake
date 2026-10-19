# --- Codecs de importação (M08/M09 Entrega 4) --------------------------------
# Draco 1.5.7 (KHR_draco_mesh_compression), meshoptimizer v1.2 (EXT_meshopt_compression)
# e Basis Universal v2_50 (KHR_texture_basisu/KTX2). Só decodificadores, vendorizados com
# origem, commit e licença em third_party/*/VERSION.txt. Entram antes das flags estritas pelo
# mesmo motivo do Jolt, mas continuam sem RTTI e sem exceções como o resto do binário. O
# adaptador que conversa com eles (resources/gltf_codecs.cpp) é código nosso, com -Werror.
set(AETHER_DRACO_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/draco/src/draco)
add_library(aether_codecs STATIC
  third_party/meshoptimizer/vertexcodec.cpp
  third_party/meshoptimizer/indexcodec.cpp
  third_party/meshoptimizer/vertexfilter.cpp
  third_party/meshoptimizer/simplifier.cpp
  third_party/meshoptimizer/vcacheoptimizer.cpp
  third_party/meshoptimizer/indexanalyzer.cpp
  third_party/basis_universal/transcoder/basisu_transcoder.cpp
  third_party/basis_universal/zstd/zstddeclib.c
  ${AETHER_DRACO_SOURCE}/attributes/attribute_octahedron_transform.cc
  ${AETHER_DRACO_SOURCE}/attributes/attribute_quantization_transform.cc
  ${AETHER_DRACO_SOURCE}/attributes/attribute_transform.cc
  ${AETHER_DRACO_SOURCE}/attributes/geometry_attribute.cc
  ${AETHER_DRACO_SOURCE}/attributes/point_attribute.cc
  ${AETHER_DRACO_SOURCE}/compression/attributes/attributes_decoder.cc
  ${AETHER_DRACO_SOURCE}/compression/attributes/kd_tree_attributes_decoder.cc
  ${AETHER_DRACO_SOURCE}/compression/attributes/sequential_attribute_decoder.cc
  ${AETHER_DRACO_SOURCE}/compression/attributes/sequential_attribute_decoders_controller.cc
  ${AETHER_DRACO_SOURCE}/compression/attributes/sequential_integer_attribute_decoder.cc
  ${AETHER_DRACO_SOURCE}/compression/attributes/sequential_normal_attribute_decoder.cc
  ${AETHER_DRACO_SOURCE}/compression/attributes/sequential_quantization_attribute_decoder.cc
  ${AETHER_DRACO_SOURCE}/compression/bit_coders/adaptive_rans_bit_decoder.cc
  ${AETHER_DRACO_SOURCE}/compression/bit_coders/direct_bit_decoder.cc
  ${AETHER_DRACO_SOURCE}/compression/bit_coders/rans_bit_decoder.cc
  ${AETHER_DRACO_SOURCE}/compression/bit_coders/symbol_bit_decoder.cc
  ${AETHER_DRACO_SOURCE}/compression/decode.cc
  ${AETHER_DRACO_SOURCE}/compression/mesh/mesh_decoder.cc
  ${AETHER_DRACO_SOURCE}/compression/mesh/mesh_edgebreaker_decoder.cc
  ${AETHER_DRACO_SOURCE}/compression/mesh/mesh_edgebreaker_decoder_impl.cc
  ${AETHER_DRACO_SOURCE}/compression/mesh/mesh_sequential_decoder.cc
  ${AETHER_DRACO_SOURCE}/compression/point_cloud/point_cloud_decoder.cc
  ${AETHER_DRACO_SOURCE}/compression/point_cloud/point_cloud_kd_tree_decoder.cc
  ${AETHER_DRACO_SOURCE}/compression/point_cloud/point_cloud_sequential_decoder.cc
  ${AETHER_DRACO_SOURCE}/compression/point_cloud/algorithms/dynamic_integer_points_kd_tree_decoder.cc
  ${AETHER_DRACO_SOURCE}/compression/point_cloud/algorithms/float_points_tree_decoder.cc
  ${AETHER_DRACO_SOURCE}/compression/entropy/symbol_decoding.cc
  ${AETHER_DRACO_SOURCE}/core/bit_utils.cc
  ${AETHER_DRACO_SOURCE}/core/bounding_box.cc
  ${AETHER_DRACO_SOURCE}/core/data_buffer.cc
  ${AETHER_DRACO_SOURCE}/core/decoder_buffer.cc
  ${AETHER_DRACO_SOURCE}/core/divide.cc
  ${AETHER_DRACO_SOURCE}/core/draco_types.cc
  ${AETHER_DRACO_SOURCE}/core/hash_utils.cc
  ${AETHER_DRACO_SOURCE}/core/options.cc
  ${AETHER_DRACO_SOURCE}/core/quantization_utils.cc
  ${AETHER_DRACO_SOURCE}/mesh/corner_table.cc
  ${AETHER_DRACO_SOURCE}/mesh/mesh.cc
  ${AETHER_DRACO_SOURCE}/mesh/mesh_attribute_corner_table.cc
  ${AETHER_DRACO_SOURCE}/mesh/mesh_misc_functions.cc
  ${AETHER_DRACO_SOURCE}/point_cloud/point_cloud.cc
  ${AETHER_DRACO_SOURCE}/metadata/geometry_metadata.cc
  ${AETHER_DRACO_SOURCE}/metadata/metadata.cc
  ${AETHER_DRACO_SOURCE}/metadata/metadata_decoder.cc
)
target_include_directories(aether_codecs SYSTEM PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/draco/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/meshoptimizer
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/basis_universal/transcoder
)
# Saídas do transcodificador que o importador não pede (ele decodifica para RGBA8) ficam
# desligadas; PUBLIC porque o cabeçalho do transcodificador lê as mesmas definições.
target_compile_definitions(aether_codecs PUBLIC
  BASISD_SUPPORT_KTX2=1 BASISD_SUPPORT_KTX2_ZSTD=1
  BASISD_SUPPORT_DXT1=0 BASISD_SUPPORT_DXT5A=0 BASISD_SUPPORT_BC7_MODE5=0
  BASISD_SUPPORT_PVRTC1=0 BASISD_SUPPORT_PVRTC2=0 BASISD_SUPPORT_ATC=0 BASISD_SUPPORT_FXT1=0
)
target_compile_options(aether_codecs PRIVATE -w
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
)

# Arm ASTC Encoder 5.7.0 (S1 do plano Sponza): codificação ASTC LDR na importação.
# Vendorizado com origem e licença em third_party/astc-encoder/VERSION.txt; como os
# codecs, compila sem as flags estritas e sem RTTI/exceções. SIMD pelas macros do
# compilador (SSE no host, NEON no arm64).
add_library(aether_astcenc STATIC
  third_party/astc-encoder/astcenc_averages_and_directions.cpp
  third_party/astc-encoder/astcenc_block_sizes.cpp
  third_party/astc-encoder/astcenc_color_quantize.cpp
  third_party/astc-encoder/astcenc_color_unquantize.cpp
  third_party/astc-encoder/astcenc_compress_symbolic.cpp
  third_party/astc-encoder/astcenc_compute_variance.cpp
  third_party/astc-encoder/astcenc_decompress_symbolic.cpp
  third_party/astc-encoder/astcenc_diagnostic_trace.cpp
  third_party/astc-encoder/astcenc_entry.cpp
  third_party/astc-encoder/astcenc_find_best_partitioning.cpp
  third_party/astc-encoder/astcenc_ideal_endpoints_and_weights.cpp
  third_party/astc-encoder/astcenc_image.cpp
  third_party/astc-encoder/astcenc_integer_sequence.cpp
  third_party/astc-encoder/astcenc_mathlib.cpp
  third_party/astc-encoder/astcenc_mathlib_softfloat.cpp
  third_party/astc-encoder/astcenc_partition_tables.cpp
  third_party/astc-encoder/astcenc_percentile_tables.cpp
  third_party/astc-encoder/astcenc_pick_best_endpoint_format.cpp
  third_party/astc-encoder/astcenc_quantization.cpp
  third_party/astc-encoder/astcenc_symbolic_physical.cpp
  third_party/astc-encoder/astcenc_weight_align.cpp
  third_party/astc-encoder/astcenc_weight_quant_xfer_tables.cpp
)
target_include_directories(aether_astcenc SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/astc-encoder)
# Otimizado em qualquer tipo de build: em -O0 o encoder é dezenas de vezes mais
# lento (72 texturas do Sponza levaram 65 min no host Debug). Não há depuração
# útil dentro dele; o nosso adaptador continua com as flags do build.
target_compile_options(aether_astcenc PRIVATE -w -fno-rtti -fno-exceptions -O2)


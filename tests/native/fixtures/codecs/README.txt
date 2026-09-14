Fixtures dos testes de codecs (tests/native/test_gltf_codecs.cpp), gerados pelas ferramentas
oficiais das MESMAS versões vendorizadas em native/third_party. Os bytes estão embutidos no teste;
estes arquivos existem para regenerar e conferir.

quad.drc          draco_encoder (Draco 1.5.7) -i quad.obj -o quad.drc -cl 7
                  SHA-256 d3c1b37d2c69da21bb2c8b21857e7aa4294fc1375d2e8ca69263d8c4c02369df
                  ids únicos dos atributos (ordem do leitor OBJ): posição 0, UV 1, normal 2
grad8.png         8x8 RGBA gerado por script (R = 32*x, G = 32*y, B = 128, A = 255)
grad8-etc1s.ktx2  basisu (Basis Universal v2_50) -ktx2 grad8.png
                  SHA-256 36289bb2fd93efa7db5ce1e4f51106346367bdfaa6b605dcedcceeb13384a215
grad8-uastc.ktx2  basisu (Basis Universal v2_50) -ktx2 -uastc grad8.png  (UASTC com Zstandard)
                  SHA-256 b152bb235eea4e2ac80995165617efa28718951d5cf7895914788e0df6b18a29

meshopt não tem fixture binária: o teste codifica com meshopt_encodeVertexBuffer/IndexBuffer da v1.2.

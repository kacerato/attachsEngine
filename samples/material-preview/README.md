# Material preview — asset de referência

O aplicativo abre uma esfera de material. Este diretório contém os oito assets
cozidos necessários ao sample, não uma biblioteca de assets do Core.

## Origem e licenças

Albedo, normal OpenGL e ARM (occlusion/roughness/metallic) são derivados de
[Metal Plate 02, Poly Haven](https://polyhaven.com/a/metal_plate_02), disponibilizado
sob [CC0](https://polyhaven.com/license). As fontes são JPG **8192×8192**, sem
upscale. `manifest.json` registra URLs, MD5 e SHA-256 das fontes, encoder e
SHA-256 de cada arquivo importado. O Gradle verifica os oito hashes antes de
empacotar; asset ausente/corrompido falha o build.

`studio.aetex` é um ambiente HDR analítico original, com três emissores
retangulares, pré-filtrado GGX. `brdf.aetex` é uma integral split-sum calculada
offline. Não há fotografia de estúdio nem imagem gerada por IA nesses arquivos.

Encoder externo: [ARM astcenc 5.7.0](https://github.com/ARM-software/astc-encoder/releases/tag/5.7.0),
[Apache-2.0](https://github.com/ARM-software/astc-encoder/blob/5.7.0/LICENSE.txt).
Usado somente no import offline; não é distribuído no APK nem adicionado como
dependência nativa. ZIP Windows x64 usado:
`astcenc-5.7.0-windows-x64.zip`, SHA-256
`cbde7c78adcc90d0d90ecf10f32d03f3340fbe1720cb6d6c892147d625e74d2d`.

## Regerar

O build normal usa os `.aetex` presentes no checkout: não baixa assets nem
executa o encoder. Para reimportar, use Python com Pillow/numpy e astcenc 5.7.0
(validado com Pillow 12.3.0 e numpy 2.3.5). Na raiz:

```powershell
python tools/cook-material-preview.py --astcenc caminho/astcenc-avx2.exe
python tests/tools/test_material_assets.py
```

O primeiro import baixa três JPGs para `build/material-preview/source`; os
seguintes reaproveitam as fontes após conferir hash. A compressão ASTC 6×6
`medium` utiliza quatro threads e pode demorar vários minutos. O import é uma
operação de workstation e pode usar vários GiB de RAM; não roda no Android.
O algoritmo é determinístico, mas hashes de uma reimportação com outro encoder,
SIMD ou versões de bibliotecas não são garantidos iguais: revisar e atualizar
manifesto junto dos outputs. `--environment-only` recompõe somente IBL/LUT.

Albedo: mipmaps em luz linear e armazenamento sRGB. Normal: média dos vetores,
normalização por mip, armazenamento linear. ARM: média linear. Há 14 níveis
8K→1 para ASTC e 11 níveis 1K→1 para fallback RGBA8. O fallback é uma redução
real das mesmas fontes, não outro material.

Assets importados somam **136.708.564 bytes (~130,4 MiB)**; são armazenados sem
compressão ZIP adicional no APK para leitura/seek pelo AssetManager. É um custo
deliberado deste sample de alta resolução, não o orçamento de um jogo vazio.
As fontes e ferramentas ficam no cache ignorado; nunca substituem os dados
versionados necessários ao build offline.

Contrato de runtime, memória, validações e limites: `docs/MATERIAL-PREVIEW.md`.

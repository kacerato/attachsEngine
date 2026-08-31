# Dirt Road Through Forest — mapa de teste

Vertical slice de importação/renderização de uma cena glTF real no Android. O
ZIP original não é necessário no build nem no runtime: `tools/cook-gltf-map.py`
gera `Imported/scene.aemap` (geometria, materiais, draws, bounds e câmera) e as
texturas AETX com mipmaps. O caminho ASTC 6×6 mantém até 4096×4096; o fallback
RGBA8 é limitado a 512 px por eixo para conter APK e memória.

O pacote atual é AEMAP v2. Posição e UV permanecem em float32; normal e tangente
usam SNORM16 e cor usa UNORM8, reduzindo o stride de 72 para 48 bytes sem reduzir
geometria ou resolução. O runtime mantém leitura do AEMAP v1 para projetos antigos.
Detalhes e gates de precisão estão em `docs/adr/ADR-015-AEMAP-V2-VERTICES-COMPACTOS.md`.

## Controles mobile

- toque/arrasto na metade esquerda: joystick flutuante de movimento;
- toque/arrasto na metade direita: olhar para os lados e para cima/baixo;
- os dois controles funcionam simultaneamente por pointer ID;
- o HUD mostra FPS no topo e o joystick permanece no canto inferior esquerdo.

O modo de benchmark usa um `CharacterMotor` com cápsula, gravidade, fixed step e
colisão estática Jolt; a câmera apenas consome a posição dos olhos. A colisão é
construída globalmente a partir do AEMAP em espaço mundial. Material BLEND não
desliga física (estradas e decals podem ser superfícies reais); cartões alpha-mask
ficam fora por padrão, com flags de importação `NoCollision`/`ForceCollision`.
Posição e orientação sobrevivem à recriação da surface e ao ciclo
background/foreground.

## Reprodução do import

Requer Python com Pillow/numpy e `astcenc` 5.7.0. Exemplo:

```powershell
python tools/cook-gltf-map.py `
  "C:\Users\jamaa\Downloads\update_dirt_road_through_forest.zip" `
  --astcenc "C:\caminho\astcenc-avx2.exe" --quality=-medium --jobs 8
```

O Gradle confere SHA-256 de cada saída contra `manifest.json`. Ele nunca busca
ou altera o ZIP externo.

## Céu e ambiente global

O céu ativo não usa mais a fotografia de árvores. O source autoral do projeto é
`Source/day-clouds-panorama-v1.png`; ele é uma panorâmica equiretangular 2:1 e
deve permanecer fora do caminho de runtime. Para reproduzir os recursos:

```powershell
python tools/cook-sky-panorama.py
```

A ferramenta gera `Imported/environment.aetex` em 1024×512 RGBA8 sRGB, com
mipmaps filtrados em espaço linear e borda horizontal tratada, e
`Imported/environment.aeenv` v2 com sol, ambiente, exposição e parâmetros de
iluminação globais. O runtime aceita AEEN v1 apenas como migração. O shader usa a
direção da câmera para amostrar a esfera, por isso o céu gira com o mundo sem
esticamento de imagem de tela. A imagem source foi gerada com a ferramenta de
imagem da OpenAI e curada especificamente para o sample da Aether.

## Licença e atribuição

`[UPDATE] Dirt Road Through Forest`, de **99.Miles**, licenciado em
**CC BY 4.0**. A licença fornecida com o download está preservada em
`LICENSE.txt`; origem e hash do ZIP estão registrados em `manifest.json`.

Este checkout usa o mapa somente como conteúdo de teste durante a produção da
engine. A licença original permite outros usos desde que a atribuição seja
preservada; isso não altera a licença do código da Aether.

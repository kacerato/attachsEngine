# Dirt Road Through Forest — mapa de teste

Vertical slice de importação/renderização de uma cena glTF real no Android. O
ZIP original não é necessário no build nem no runtime: `tools/cook-gltf-map.py`
gera `Imported/scene.aemap` (geometria, materiais, draws, bounds e câmera) e as
texturas AETX com mipmaps. O caminho ASTC 6×6 mantém até 4096×4096; o fallback
RGBA8 é limitado a 512 px por eixo para conter APK e memória.

## Controles mobile

- um dedo: olhar para os lados e para cima/baixo;
- dois dedos arrastando: mover a câmera lateralmente e subir/descer;
- pinça com dois dedos: avançar/recuar.

Mudanças de quantidade/ID dos dedos apenas recalibram o gesto, evitando saltos.
A posição sobrevive à recriação da surface e ao ciclo background/foreground.

## Reprodução do import

Requer Python com Pillow/numpy e `astcenc` 5.7.0. Exemplo:

```powershell
python tools/cook-gltf-map.py `
  "C:\Users\jamaa\Downloads\update_dirt_road_through_forest.zip" `
  --astcenc "C:\caminho\astcenc-avx2.exe" --quality=-medium --jobs 8
```

O Gradle confere SHA-256 de cada saída contra `manifest.json`. Ele nunca busca
ou altera o ZIP externo.

## Licença e atribuição

`[UPDATE] Dirt Road Through Forest`, de **99.Miles**, licenciado em
**CC BY 4.0**. A licença fornecida com o download está preservada em
`LICENSE.txt`; origem e hash do ZIP estão registrados em `manifest.json`.

Este checkout usa o mapa somente como conteúdo de teste durante a produção da
engine. A licença original permite outros usos desde que a atribuição seja
preservada; isso não altera a licença do código da Aether.

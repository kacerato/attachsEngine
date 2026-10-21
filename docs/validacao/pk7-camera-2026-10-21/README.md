# PK-7 Câmera — validação no POCO F7 e referência Unity (20–21/10/2026)

Referência: Unity 6000.0 URP Camera component
(https://docs.unity3d.com/6000.0/Documentation/Manual/urp/camera-component-reference.html).

## Frames de vídeo da Unity (`referencia-unity/`)

| Arquivo | Vídeo | Instante | O que mostra |
|---|---|---|---|
| `dvAcLqUuwEg_t176s.jpg` | https://youtu.be/dvAcLqUuwEg?t=176 | 2:56 | Clear Flags Solid Color: cor no lugar do céu |
| `iYaOkbC58W4_t168s.jpg` | https://youtu.be/iYaOkbC58W4?t=168 | 2:48 | Culling Mask "Mixed…" |
| `iYaOkbC58W4_t175s.jpg` | https://youtu.be/iYaOkbC58W4?t=175 | 2:55 | Culling Mask Nothing |
| `8iaRrewHQuw_t51s.jpg`, `_t66s.jpg` | https://youtu.be/8iaRrewHQuw?t=51 | 0:51 / 1:06 | Unity 6 URP: Viewport Rect W 0,5 / X 0,5 e a ordem do Inspector |
| `loddo4XcYng_t99s.jpg`, `_t110s.jpg` | https://youtu.be/loddo4XcYng?t=99 | 1:39 / 1:50 | Physical Camera: Super 35 vs sensor 70 mm a 18 mm, Gate Fit Horizontal |

## Capturas do aparelho (`poco/`) e pranchas (`pranchas/`)

Vila Astra em Play, cenas variantes geradas a partir da original (md5 2e2e996841f1f0eb5af1572c68dec3fc),
devolvida com o app parado ao fim de cada rodada.

- `01-fundo`, `apk400-21-fundo`: Fundo Cor sólida. `02-mascara`, `apk400-22-mascara`: casas na camada
  "Casas" e a câmera só com "Padrão" — casas e as sombras delas somem.
- `04-fisica85` / `05-fisica14`: 36×24 mm, Gate Fit Vertical, 85 mm (16° vertical) e 14 mm (81°).
- `20-dividida`, `21-dividida-olhar`: câmera do jogador com tag MainCamera em X 0 L 0,5 e câmera alta
  em X 0,5 L 0,5 prioridade 1; arrastar à esquerda gira só a câmera do jogador.
- `14-insp`…`17-ambiente`, `15-mascara`, `16-nada`, `apk400-35-insp`: Inspector (Lente, Renderização,
  Ambiente, Saída), seletor de camadas e Viewport em pares.

APKs: Astra Dev 131 (fundo, máscara, física, Inspector), 400 (base mesclada) e 500 (MainCamera embutida,
tela dividida). `prancha.py` regenera as pranchas.

## Julgamento

- Fundo e Culling Mask: mesmo comportamento da Unity. A cor sólida passa pela exposição e pelo tom, como
  num alvo HDR da URP.
- Câmera física: a relação focal/sensor/campo segue a Unity; nenhuma metade estica a imagem.
- Tela dividida: cada metade com a proporção do próprio retângulo. A câmera de cima é desenhada pela vista
  independente: sem sombras e sem TAA (declarado em `render.camera.viewport`).
- Não testado no aparelho: HDR desligado, Antisserrilhado por câmera, Gate Fit Fill/Overscan/None e
  Uninitialized (cobertos por testes de host).

# PK-4 + PK-5 — Volume Profile de verdade e o pós completo

Bloco da auditoria `docs/planos/auditoria-full-unity-2026-10-19.md` (V-01, V-02, V-04, V-05, V-06; P-03 e o resto
do P-05). Branch `claude/dreamy-lamarr-981ed5`. APK `0.3.0-dev.261023999-pos-0949` (canal Astra Dev), POCO F7,
23/10/2026. A Vila Astra foi copiada antes (md5 `2e2e996841f1f0eb5af1572c68dec3fc`) e devolvida com o mesmo md5.

## Como funciona na Unity 6 e o que a Astra faz

| Item | Unity 6 (URP 17) | Astra |
|---|---|---|
| V-01 Perfil compartilhado | `VolumeProfile` é um asset; vários Volumes apontam para ele e editar em um muda todos. New cria vazio, Clone copia. | O perfil vale no runtime para todos os Ambientes que o usam. A edição no Inspector vai para o perfil numa única etapa de desfazer (`EditorSession::reconcileEnvironmentProfiles`). Novo e Clonar na linha do perfil. Cópias divergentes de cenas antigas viram "Clone de …" ao abrir. |
| V-02 Override por parâmetro | Cada `VolumeParameter` tem `overrideState`; ALL/NONE no cabeçalho; Add Override adiciona o componente com tudo desmarcado; caixa ativa por override. | `VolumeOverrides` (256 bits por parâmetro, mais adicionados e ativos), tabela `renderer/volume_parameters.cpp`. Cartão por override: chevron, caixa ativa, contagem, ALL/NONE, ⋮ (Redefinir valores / Remover override) e Adicionar override por categoria. |
| V-04 Volume Mask | A câmera filtra Volumes pela Layer do GameObject. | A Máscara de volume da câmera (v5) usa as camadas de objeto. As "Ambiente 0..7" antigas viram camadas de objeto "Ambiente N" (`runtime::migrateLegacyVolumeLayers`). |
| V-05 Discretos com t > 0 | `VolumeParameter.Interp` troca enum, bool e textura assim que o peso passa de zero. | Igual, por parâmetro. O panorama HDRI leva junto a orientação e a exposição dele. |
| V-06 Inspector do perfil | Lista de overrides com seções recolhíveis. | O perfil aberto em Propriedades é uma lista rolável de cartões recolhíveis, sem a paginação "< 1/5 >". |
| Depth of Field | Gaussian (Start/End/Max Radius/High Quality) e Bokeh (Focus Distance, Focal Length, Aperture, lâminas). | Os dois modos, em meia resolução e com render pass próprio. Bokeh pelo kernel do PPv2/URP, com 8, 22 ou 43 leituras (Low, Medium, High). |
| Motion Blur | Camera Only, Quality Low/Medium/High, Intensity, Clamp. | Igual (4, 6 ou 8 leituras, ruído intercalado), pelo movimento da câmera entre quadros. Desligado na vista do editor, como na Scene view. |
| Chromatic Aberration | 3 leituras deslocadas pelo quadrado da distância ao centro. | Igual. |
| Lens Distortion | Intensity, X/Y Multiplier, Center, Scale. | Igual (DistortUV da URP), aplicado antes de todos os outros efeitos. |
| Vignette | Color, Center, Intensity, Smoothness, Rounded. | Completa. |
| Color Curves | Master/R/G/B, Hue vs Hue, Hue vs Sat, Sat vs Sat, Lum vs Sat. | As 8 curvas, editadas no editor de curvas e embutidas na LUT 32³. |
| Bloom Dirt | Textura e intensidade de sujeira da lente; High Quality Filtering. | Textura do projeto reamostrada para 512², somada onde o bloom passa; filtragem bicúbica opcional. |

Referências (Unity 6 / URP 17): [Volumes](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/Volumes.html),
[Volume Profile](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/Volume-Profile.html),
[Depth of Field](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/depth-of-field-urp.html),
[Motion Blur](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/Post-Processing-Motion-Blur.html),
[Chromatic Aberration](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/post-processing-chromatic-aberration.html),
[Lens Distortion](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/Post-Processing-Lens-Distortion.html),
[Vignette](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/post-processing-vignette.html),
[Color Curves](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/Post-Processing-Color-Curves.html),
[Bloom](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/post-processing-bloom.html).

## Frames de referência (vídeos)

Em `referencia-unity/`, com o vídeo e o tempo no nome:
- Sasquatch B Studios, "EVERY Post-Processing effect in Unity", https://www.youtube.com/watch?v=xxM6AUHqTMA
  (00:38 criar volume, 02:33 bloom dirt, 03:19/03:25 curvas, 03:35 New/Clone, 05:57/06:07 aberração e ALL/NONE,
  06:15 DoF Bokeh, 06:48/07:00 distorção, 07:10 motion blur, 07:58/08:06 vinheta).
- Game Dev Guide, "Dynamic Depth of Field in Unity", https://www.youtube.com/watch?v=7od2j4s85ww (04:30, 07:00, 08:30).

## Pranchas Unity × POCO

- `prancha-profundidade-de-campo.png`: Gaussian, Bokeh Medium e High.
- `prancha-lente.png`: aberração cromática, distorção em barril e vinheta.
- `prancha-movimento-e-curvas.png`: desfoque de movimento com a câmera girando, curvas de cor e todos os efeitos juntos.

Julgamento: proporção e intensidade batem nos efeitos de lente, vinheta, curvas e movimento. O DoF Gaussian e o Bokeh
desfocam no lugar certo (Gaussian só o fundo; Bokeh frente e fundo). O Bokeh do vídeo (sala, foco 2,9 m) desfoca muito
mais que a praça da Vila com foco a 5 m, pela distância e não por diferença de fórmula. As lâminas não aparecem como
polígono porque a cena não tem pontos de luz pequenos no fundo.

## Custo no POCO (gpu_post_ms, mediana das janelas de 600 quadros)

Play na Vila, câmera parada, resolução dinâmica e qualidade travadas, `aether.profile_frames`, sem esperar esfriar
(pedido do dono). O ruído entre rodadas é de ±1 ms (a vinheta mediu menos que a neutra).

| Variante | Pós (ms) | Δ sobre a neutra | Quadro GPU (ms) | fps |
|---|---|---|---|---|
| Neutra | 14,29 | — | 36,8 | 25,7 |
| DoF Gaussian (Medium) | 15,60–17,17 | +1,3 a +2,9 | 40,6–41,2 | 22,4 |
| DoF Bokeh Low | 15,59–17,90 | +1,3 a +3,6 | 39,2–41,5 | 22,4 |
| DoF Bokeh Medium | 16,66–17,76 | +2,4 a +3,5 | 40,7–40,9 | 22,1 |
| DoF Bokeh High | 17,31–17,82 | +3,0 a +3,5 | 38,0–42,2 | 21,9 |
| Motion Blur Low / High (câmera parada) | 13,88 / 13,13 | ruído | 37,2 / 36,7 | 24,6 |
| Chromatic Aberration 1 | 13,57 | ruído | 37,7 | 24,1 |
| Lens Distortion −0,5 | 13,61 | ruído | 37,7 | 24,2 |
| Vignette | 12,88 | ruído | 36,6 | 24,8 |
| Color Curves | 12,98 | ruído (LUT) | 36,8 | 24,9 |
| Tudo junto | 17,92–18,16 | +3,6 a +3,9 | 35,4–42,8 | 22,6 |

O efeito caro é a profundidade de campo (+2 a +3,5 ms). Ela já tem os modos baratos pedidos (Low/Medium/High, meia
resolução, Gaussian sem bokeh). O motion blur só custa leituras quando a câmera anda; com a câmera parada ele sai antes
do laço. A neutra nesta rodada fica dentro da faixa histórica da Vila (8 a 18 ms de pós), mas o quadro inteiro desta
sessão (~37 ms) está acima das medições antigas, e esta entrega não compara contra um APK anterior.

## Inspector no aparelho (PK-4)

`poco/13` a `poco/19`: o Ambiente selecionado com o cartão Volume (modo, Sem perfil + Novo, peso), cartões por override
com contagem e ALL/NONE, o DoF com o modo Bokeh, o menu ⋮ (Redefinir valores / Remover override), Adicionar override com
a lista por categoria, o override readicionado com as 5 caixas desmarcadas e as linhas apagadas (como o Add Override da
Unity) e ALL marcando as 5.

## Separação

- Conceito: as pranchas usam frames de vídeo da Unity como hipótese de design; nenhum controle foi copiado sem
  consumidor.
- Implementado: tudo da tabela acima (a janela Iluminação edita o Ambiente da cena sem a interface de override, como a Lighting da Unity), com modelo, serialização (Ambiente v19, perfil v14, câmera v5), shaders, renderer,
  Inspector, API C# (`Components.g.cs`) e migração.
- Testado no host: `aether_environment_authoring_tests` 75/75 e `aether_tests` 1663/1663 (exe de 23/10 13:09), alvo Android arm64 compilado com clang.
- Testado no aparelho: os 12 casos de efeito e o fluxo de override do Inspector acima. O APK pos-0949 é anterior a dois ajustes feitos depois da suíte: a janela Iluminação sem a interface de override e os links de referência do DoF e da Exposure. Os dois só foram verificados no host.
- Não testado no aparelho: Bloom Dirt (a Vila não tem textura de sujeira registrada), curvas editadas pelo editor de
  curvas no toque, Novo/Clonar perfil no toque e o compartilhamento entre dois Volumes no aparelho (cobertos só por
  `volume_profile_is_shared_and_edits_from_any_volume_reach_all` no host). O DoF não roda junto com o upscaler temporal,
  e a prévia da câmera não mostra DoF.

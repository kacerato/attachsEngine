# G3a item 2 · auditoria de materiais importados (19/10/2026)

Plano: `docs/planos/DESEMPENHO-GRAFICOS-VIVOS-2026-10-17.md`, bloco G3a, item 2.
Aparelho: POCO F7, Astra Dev `dev.aether.editor.u07` vc 108 (`0.3.0-dev.108-g3amat`), instalado
com `install -r` (projetos preservados). Branch `claude/g3a-material-audit-a6b1f4` sobre 7e54f141.

## Referências

- Unity 2022.3, Standard Shader, Smoothness
  (<https://docs.unity3d.com/2022.3/Documentation/Manual/StandardShaderMaterialParameterSmoothness.html>):
  origem da suavidade no alfa do mapa metálico ou no alfa do albedo; Specular Highlights e
  Reflections em Forward Rendering Options.
- Fonte dos shaders embutidos da Unity (`DefaultResourcesExtra/Standard.shader` e
  `CGIncludes/UnityStandardInput.cginc`, funções `MetallicGloss` e `Alpha`): `_METALLICGLOSSMAP`
  lê metálico em R e suavidade em A (ou no alfa do albedo) vezes `_GlossMapScale`, `_Metallic`
  é `[Gamma]`, e com a suavidade no albedo o alfa passa a ser só `_Color.a`.
- KHR_materials_specular (glTF): fator especular dielétrico, o `materialFactors.w` da Astra.

## O que foi implementado

| Parte | Onde |
|---|---|
| `MaterialChannels::roughnessSource` (Herdar, Rugosidade no canal, Suavidade no canal, Suavidade no alfa da cor base). Na suavidade, rugosidade = 1 − (1 − fator) × canal | `native/scene/material_parameters.h` |
| Persistência: componente de malha v10 e ASTRA_MATERIAL v7; v9 e v6 abrem como Herdar | `native/scene/mesh_renderer.h`, `native/resources/material_asset.cpp` |
| Herança instância → material do projeto → fonte; extensão por desenho linha 10.y | `native/editor/editor_map_scene.h`, `native/renderer/material_override.h` |
| Shader: alfa cru da cor base guardado antes da origem do alfa; suavidade invertida depois da leitura do canal; oclusão especular intacta | `native/rhi/shaders/dirt_road_shading.glsl` (+ 4 SPIR-V) |
| Conversor Standard: mapa metal/suavidade no binding 2 (R/A), suavidade no alfa do albedo, `_Metallic` sRGB → linear, especular 0 com os dois toggles desligados, aproximação declarada com um só, alfa da cor ignorado no opaco (como a Unity), entradas `<noninit>` da Unity 5 ignoradas | `native/resources/unity_material_import.cpp` |
| Auditoria por material (completo, aproximado, sem conversão); aproximações ficam nas perdas conhecidas do item | `unity_material_import.h`, `native/editor/editor_library.cpp` |
| Biblioteca: cartão "Materiais Unity" com três blocos que filtram a grade; selo "≈" no cartão do material aproximado; linha "Aproximação" com ícone próprio no detalhe | `native/editor/editor_screen.cpp`, `editor_session_actions.cpp` |
| Inspector do material, cartão Canais: linha "Origem da rugosidade" (toque alterna, com histórico e edição em lote) | `editor_screen.cpp`, `editor_session*.cpp` |
| Ícones novos no atlas real: `assets/material-audit`, `-complete`, `-approximate`, `-unconverted` | `tools/generate-component-icons.py` → `pack-icon-atlas.py` |
| C#: `MaterialRoughnessSource`, `MaterialSlot.RoughnessSource`; fachada gerada `Get/SetChannelsRoughnessSource` | `managed/Astra.Scripting/` |

Conceito × implementação: não houve imagem conceitual gerada. O cartão foi desenhado direto na
UI executável e julgado em captura no host e no aparelho; os quatro ícones são vetoriais, do
mesmo gerador e no atlas.

## Testado no host

`aether_unity_material_tests` 28/28 (cenários novos: mapa metal/suavidade, suavidade no albedo e
opções de forward, `<noninit>`, migração v6/v9, linha 10.y, cartão de auditoria com toque que
filtra e limpa). Suíte gerenciada 523 aprovados, 0 falhas. APK arm64 (clang `-Werror`) compilado.
Captura nativa do cartão: `host-cartao-auditoria.png`.

## Testado no POCO

1. Recorte real do pacote **MapMagic 2 v2.1.11** (`Assets/MapMagic/Demo`: `Stones.mat` com
   `_METALLICGLOSSMAP`, `MegaSplatMaterial2.mat`, `Sedge.mat`, texturas e `.meta` originais)
   importado como pasta: o relatório aparece logo após o catálogo, com **2 completos, 0
   aproximados, 1 sem conversão** (`Sedge.mat`, alfa premultiplicado). `poco-biblioteca-relatorio.png`.
2. As TIFF do pacote são LZW (e `Stones_s.tif` é cinza + alfa com preditor horizontal); o
   decodificador TIFF da Astra só aceita sem compressão e recusou a conversão do material. Para
   validar o material, as quatro TIFF viraram PNG sem perda (decodificador LZW próprio,
   idêntico pixel a pixel ao PIL na `Stones.tif`), com os mesmos `.meta`/GUIDs.
3. `Stones.mat` convertido por toque: cor, normal e `Stones_s.png` no mapa metal/rugosidade;
   canais Rug. A / Metal R; origem "Suavidade no canal (1 − canal)"; rugosidade 0 (= 1 −
   `_GlossMapScale` 1), metálico 1. `poco-material-canais.png`.
4. Aplicado numa esfera e comparado alternando a origem no próprio aparelho (região da esfera,
   luminância média): suavidade no canal **104,4**; alfa da cor base (sempre 1 nesta textura)
   **113,0**; rugosidade no canal sem inversão **113,0**, com pontos de especular saturado. Só a
   leitura correta tira o brilho uniforme. Da esquerda para a direita em
   `poco-esfera-ab-origem-rugosidade.png`; a tela inteira com a origem restaurada está em
   `poco-esfera-stones.png`. Logcat sem `E Aether.Vulkan`.

## Lacunas

- TIFF LZW/preditor/cinza + alfa não decodifica na Astra: pacotes Unity com TIFF continuam
  recusando a conversão do material até essa extensão do decodificador.
- Pacotes catalogados antes desta versão guardam a classificação antiga no índice da
  biblioteca; reimportar o pacote atualiza o relatório.
- `_DETAIL_MULX2`, `_PARALLAXMAP`, alfa premultiplicado e alfa da cor fora do modo opaco
  continuam sem conversão. A variante Specular setup (`_SPECGLOSSMAP` com outro shader) não é
  convertida; a keyword sozinha no Standard metálico é inerte e é ignorada.
- Nenhum pacote real com uma única opção de forward desligada foi importado no aparelho; o
  caminho "aproximado" foi testado só no host.
- A prévia isolada de material (`material_preview` → `material_shading.glsl`) lê rugosidade e
  metálico fixos em G/B e ignora a extensão de canais inteira (já antes desta entrega); a
  miniatura 3D de um material Unity com suavidade não representa o resultado da cena.

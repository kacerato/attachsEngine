# PK-9 — Material como na Unity 6/URP (T-01..T-05) — validação de 20/10/2026

Pacote da auditoria full Unity (`docs/planos/auditoria-full-unity-2026-10-19.md`, seção "Materiais"), branch
`claude/beautiful-kalam-695ced`, base `9fbbbe57`.

Referências (Unity 6000.0, URP 17):
[Lit shader](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/lit-shader.html),
[Complex Lit](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/shader-complex-lit.html),
[Simple Lit](https://docs.unity3d.com/6000.2/Documentation/Manual/urp/simple-lit-shader.html),
[Blending modes](https://docs.unity3d.com/6000.1/Documentation/Manual/urp/blending-modes.html),
[Particles Lit](https://docs.unity3d.com/6000.1/Documentation/Manual/urp/particles-lit-shader.html).

## Como cada item funciona na prática na Unity (e o que a Astra faz)

| Item | Unity 6/URP, no Inspector do Material | Astra |
|---|---|---|
| T-01 | O campo **Shader** no topo troca o Inspector inteiro: Lit, Complex Lit (Lit + Clear Coat), Simple Lit (Blinn-Phong), Unlit e Particles Lit/Unlit. Cada um mostra só as propriedades que lê. | `MaterialAsset` v8 guarda o shader; o Inspector do material do projeto (cartão "Shader", lista com os 6) mostra só os campos que o shader lê (tabela única `editor/editor_material_fields.h`). |
| T-02 | **Smoothness** 0..1 com **Source** Metallic Alpha / Albedo Alpha (no Specular: Specular Alpha). | O arquivo continua em rugosidade (sem migração de dado); Inspector e API C# mostram Suavidade = 1 − rugosidade e a Origem (alfa do mapa, alfa da cor base, ou rugosidade glTF para modelos glTF). |
| T-03 | Height Map (paralaxe, G, 0,005..0,08), Detail Inputs (Mask no A, Base Map ×2 com visibilidade, Normal Map com força, Tiling/Offset), Clear Coat (Mask, Smoothness, mapa R/G) e **Workflow Mode** Metallic/Specular. | Altura por um passo de paralaxe (URP `ParallaxOffset1Step`), detalhe ×2 no espaço da Unity (cinza sRGB 0,5 neutro), normal de detalhe reorientado, verniz dielétrico F0 0,04 sobre a camada de baixo (sol + ambiente), workflow especular com o RGB do mapa como cor especular. |
| T-04 | Surface Type Opaque/Transparent; Blending Mode Alpha/Premultiply/Additive/Multiply; Preserve Specular; Render Face Front/Back/Both; Alpha Clipping + Threshold; Sorting Priority (fila 3000 + prioridade). | Pipelines de mistura próprias (ONE/1−SA, ONE/ONE, DST_COLOR/ZERO); Render Face Back descarta a frente; recorte também no transparente; prioridade ordena os transparentes antes da distância. |
| T-05 | O `.mat` do Standard/URP traz `_PARALLAXMAP`, `_DETAIL_MULX2`, `_ALPHAPREMULTIPLY_ON`. | O importador aceita os três e lê também URP Lit/Simple Lit/Unlit/Particles e o Standard (Specular setup). |

(resultado, medições e capturas: em preenchimento)

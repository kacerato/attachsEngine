# PK-3 (Luz) e PK-8 (Mesh Renderer) — validação de 20/10/2026

Pacotes da auditoria full Unity (`docs/planos/auditoria-full-unity-2026-10-19.md`, branch
`claude/nice-herschel-a55329`): L-01..L-06, A-07 e M-01. Referências estudadas (Unity 6000.0):
[URP Light component](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/light-component.html),
[Light](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Light.html),
[Lighting window](https://docs.unity3d.com/6000.0/Documentation/Manual/lighting-window.html),
[Mesh Renderer](https://docs.unity3d.com/6000.0/Documentation/Manual/class-MeshRenderer.html),
[Rendering Layers](https://docs.unity3d.com/6000.0/Documentation/Manual/features/rendering-layers.html).

## O que foi implementado (com consumidor real)

| Item | Unity | Astra |
|---|---|---|
| L-01 | Shadow Type/Strength/Bias/Normal Bias no Light, inclusive o sol | Luz v4: tipo (sem/dura/suave), força, desvio e desvio na normal da Luz principal chegam às cascatas; política do aparelho = teto (cascatas, distância, resolução) |
| L-02 | Luz principal + adicionais (8 por objeto) | 2ª direcional em diante = luz adicional sem sombra; 16 por quadro (perfil C: 8, B: 12), 8 por objeto por influência |
| L-03 | Mode, Culling Mask, Rendering Layers | Culling Mask (camadas do objeto) e Rendering Layers (8) como máscaras no Inspector/API, decididas por desenho na CPU. Mode gravado (`lightmapMode`), **não exposto** até existir bake (PK-6). Cookie e Área: **pendentes** (capacidades planejadas) |
| L-04 | Spot Angle / Inner Spot Angle completos | Ângulos totais, padrão 30°/21,8°; cenas antigas dobradas na leitura |
| L-05 | — | Tabela única `scene/light_defaults.h`: direcional relativa 3 (= Sol com dia e noite), pontual/spot 1000 lm, sol físico 100 000 lux só no modelo com exposição |
| L-06 | Cor sRGB no seletor | Cor da Luz em sRGB (campo e seletor); runtime lineariza; cenas antigas convertidas sem mudança de aparência |
| A-07 | Sun Source na janela Lighting | Componente "Iluminação da cena" (`astra.render.lighting_settings`) com Fonte do sol; vazia/inválida = direcional mais brilhante (Hora do dia primeiro) |
| M-01 | Cast/Receive Shadows, Probes, Rendering Layer Mask | Mesh Renderer v11: Projetar sombras (Desligado/Ligado/Dupla face/Só sombra), Receber sombras, Camadas de renderização. Sondas gravadas com o padrão da Unity, **não expostas** até o PK-6. Flags antigas do objeto adotadas na leitura; prefab Unity aceita os 4 ShadowCastingMode |

## Testes de host

- `aether_lighting_family_tests`: 65/65 (novo `test_light_renderer_unity.cpp`: migração v3→v4, padrões únicos,
  Fonte do sol, cor linear, máscaras e limite por objeto, política, Mesh Renderer v11 e adoção das flags antigas).
- `aether_tests`: 1590/1594 executado da raiz + os 4 `path_input_*` passam executados de `build/editor-host`
  (o `__FILE__` passou a ser relativo com o `base_dir` novo do ccache; não é regressão destes pacotes).
- `aether_recipe_family_tests`: 21/21. SPIR-V regenerado com `-All`; API C# regenerada.

## Aparelho (POCO F7, Astra Dev 126 e 135; Vila Astra em Play, câmera fixa)

Variantes da cena preparadas no host e enviadas com o app parado; a Vila foi devolvida ao fim com o mesmo md5
(`2e2e996841f1f0eb5af1572c68dec3fc`).

| Captura | O que mostra |
|---|---|
| 01 | Cena v3 aberta no APK novo: igual ao que era (sol migrado para sombra suave) |
| 02 | Sol com sombra **dura**: bordas nítidas |
| 03 | Sol com **força 0,4**: sombra bem mais clara (o chão iluminado já está no ombro do tonemapping, então o efeito é discreto) |
| 04 | Sol **sem sombra** |
| 05 | **Duas direcionais**: a segunda (azulada, vinda do lado oposto) ilumina o lado escuro das casas e não projeta |
| 06 | **Casas sem projetar sombra** (flags antigas do objeto → Mesh Renderer v11): a sombra das casas some, poste e bancos continuam projetando |
| 07–10 | Mesh Renderer, aba Iluminação; seletor dos 4 modos; "Só sombra" ao vivo (a Casa A some da câmera); seletor de camadas de renderização com Nada/Tudo |
| 11 | Inspector da Luz direcional com a seção Sombra |
| 12–13 | Astra Dev 135: seletor da Máscara de culling só com camadas nomeadas e grupos da Luz na ordem da Unity |

## Não testado no aparelho

Fonte do sol escolhida pelo Inspector, Culling Mask/Rendering Layers recusando um objeto, limite de 8 luzes por
objeto com mais de 8 lâmpadas próximas e a 16ª vaga (o host cobre as decisões; o fragmento só no aparelho).

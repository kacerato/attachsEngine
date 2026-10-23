# V-3: não receber sombra, não projetar e sem sol por objeto — revalidação no POCO (22/10/2026)

Pendência: `docs/planos/PENDENCIAS-VISUAIS-2026-10-21.md`, seção V-3. As flags por desenho
`DRAW_NO_SHADOWS` (1u<<17) e `DRAW_NO_SUN` (1u<<18) do PK-3/PK-8 saíam com 16 bits na Adreno
e viravam 0. A correção (constantes em highp) é o commit 5b1b938c, já na base.

## Resultado

**As três opções valem no aparelho. Nada a corrigir.**

| Opção (Unity) | Variante da Vila | POCO |
|---|---|---|
| Receive Shadows desligado | calçamento ("Vila") com Receber sombras desligado | Some toda sombra do chão (casa, poste, bancos); o chão continua com sol |
| Cast Shadows Off | Casa C com Projetar sombras desligado | Some a sombra grande da Casa C; a do poste continua |
| Luz excluída por Culling Mask | Casas A e E na camada 9; Máscara de culling do sol sem a camada 9 | As duas casas ficam só com o ambiente; as outras seguem com sol |

Prancha: `prancha-v3-unity-poco.jpg` (montada de `prancha.json` com `tools/build-comparison-board.py`).
Capturas do POCO em `poco-pk9-1423/` (APK 0.3.0-dev.261022999-pk9-1423, que já tem 5b1b938c) e
`poco-base/` (APK da base desta entrega).

## Como as variantes foram feitas

`tools/v3_shadow_flags_scene_variant.py` edita uma cópia de `scenes/editor.aescene`:

- a Vila no aparelho ainda está gravada com o Mesh Renderer v9, então Receber/Projetar sombras são
  as flags do objeto, que o Mesh Renderer v11 adota na leitura (`adoptLegacyObjectShadows`);
- o sol ("Sol de fim de tarde") está gravado como Luz v3, sem Máscara de culling; o script o
  regrava como v4 com os valores que `Light::read` deriva da v3 e muda só a máscara;
- só objetos com Mesh Renderer mudam: os colisores físicos repetem o nome das casas, e a
  camada também decide colisões.

Cada variante foi enviada com o app parado; a Vila abriu em Play (câmera do player) e a cena
original voltou ao fim com o md5 conferido (`2e2e996841f1f0eb5af1572c68dec3fc`).
Diagnóstico de quem projeta a sombra grande: Projetar desligado em B, D e F não a tira; em C, sim.

## Referências Unity

| Arquivo | Vídeo | O que mostra |
|---|---|---|
| `unity5-QjO2546RnUo-04m08s-recebe-sombra.jpg` | *Unity 5 Audio and Visual Effects: Implementing Shadows* (Packt), [4:08](https://www.youtube.com/watch?v=QjO2546RnUo&t=248s) | Receive ligado: a caixa alta recebe a sombra do cubo |
| `unity62-meCiuRR_0lM-01m58s-cubo-cast-on.jpg` | *Mesh Filter vs Mesh Renderer* (Unity 6.2), [1:58](https://www.youtube.com/watch?v=meCiuRR_0lM&t=118s) | Cast Shadows On no cubo |
| `unity62-meCiuRR_0lM-01m40s-cast-off.jpg`, `...-01m44s-cast-two-sided.jpg` | idem, [1:40](https://www.youtube.com/watch?v=meCiuRR_0lM&t=100s) e [1:44](https://www.youtube.com/watch?v=meCiuRR_0lM&t=104s) | menu Cast Shadows (Off/On/Two Sided/Shadows Only) num plano |
| `unity2019-1cjKPU_3A4A-01m53s-paredes-na-mascara.jpg`, `...-01m57s-paredes-fora-da-mascara.jpg` | *Using Unity Layers to Control Lighting on Certain Objects* (Unity 2019.4), [1:53](https://www.youtube.com/watch?v=1cjKPU_3A4A&t=113s) e [1:57](https://www.youtube.com/watch?v=1cjKPU_3A4A&t=117s) | paredes tiradas do Culling Mask da luz ficam pretas |

Lacuna: nenhum dos vídeos encontrados mostra o estado **desligado** de Receive Shadows nem de
Cast Shadows num objeto com sombra visível. O comportamento esperado vem do manual
[Mesh Renderer (Unity 6000.0)](https://docs.unity3d.com/6000.0/Documentation/Manual/class-MeshRenderer.html)
e de [Rendering Layers (URP, Unity 6000.0)](https://docs.unity3d.com/6000.0/Documentation/Manual/features/rendering-layers.html).

## Relação com a V-1

Com o calçamento sem receber sombra, a cor do chão fora e dentro da antiga sombra fica igual:
a sombra lavada da V-1 não vem destas flags (elas não estavam ativas no aparelho antes do PK-9).

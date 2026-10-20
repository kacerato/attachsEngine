# G4b — Nuvens no modelo do Cloud Layer da Unity (19–20/10/2026)

Aparelho: POCO F7 (Android, Vulkan). APKs `0.3.0-dev.116-nuvens` (primeira versão) e
`0.3.0-dev.117-nuvens` (versão final), canal `dev.aether.editor.u07`. Cena: Vila Astra
(céu físico). As variantes de teste (sol com Hora do dia + nuvens) foram trocadas só
durante as capturas; a cena original foi devolvida (md5 `2e2e9968…dec3fc`).

## Como funciona (igual à Unity)

| Unity (HDRP 17 / Unity 6) | Astra |
|---|---|
| Volume › Add Override › Sky › **Cloud Layer** | Ambiente › seção **Nuvens** (o Ambiente é o Volume da Astra) |
| Visual Environment › Cloud Type = Cloud Layer | Interruptor **Nuvens** |
| Override próprio, independente do céu | **Sobrescrever nuvens** na seção Volume, separado de Sobrescrever céu |
| Opacity, Altitude, Tint, Exposure Compensation | Opacidade, Altitude, Tom, Compensação de exposição |
| Opacity R/B do DefaultCloudMap (cúmulos/cirros) | Cobertura e Cirros sobre um mapa procedural repetível (256², mips) |
| Wind › Orientation (graus) e Speed (km/h) | Direção do vento e Velocidade do vento |
| Raymarching › Density, Ambient Probe Dimmer | Densidade (3 passos rumo ao sol) e Luz do céu |
| Cast Shadows, Shadow Multiplier | Projetar sombras, Força da sombra — a sombra é o MESMO mapa projetado pela direção do sol a partir da Altitude |
| A sonda de ambiente do HDRP vê as nuvens | O ambiente do céu perde o azul na proporção do céu coberto |
| GameObject › Volume com só o override | **+ › Luzes › Volume de nuvens**: Ambiente global prioridade 1 que só sobrescreve as nuvens |

O sol é a luz direcional (com Hora do dia, G4): girar o transform muda a luz, a cor e a
direção da sombra das nuvens. Funciona nos céus procedural, físico e HDRI.

Referências: Unity HDRP 17.0, [Create simple clouds (Cloud Layer)](https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.0/manual/create-simple-clouds-cloud-layer.html)
e [Cloud Layer Volume Override reference](https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.0/manual/cloud-layer-volume-override-reference.html).
UniStorm foi só estudo de fluxo (cobertura como eixo de clima); nenhum código nem asset dele entrou.

## Capturas

| Arquivo | O que mostra |
|---|---|
| `00-primeira-versao-bordas-esfareladas.png` | APK 116: bordas esfareladas (erosão forte) e miolo escuro/bege — corrigido no 117 |
| `01-meio-dia-12h.png` | APK 117, Play, 12 h, cobertura 0,4: cúmulos com topo claro, sombra do sol na praça |
| `02-poente-16h54.png` | APK 117, Play, 16,9 h: base das nuvens acesa de laranja pelo sol rasante |
| `03-nublado-12h.png` | APK 117, Play, cobertura 0,95: céu fechado cinza, sombras do sol quase somem |
| `04-inspector-nuvens.png` | Editor: Atmosfera › Ambiente › seção Nuvens (interruptor, sombras, 14 campos) |

## Custo de GPU (POCO F7, nível S nativo 2772×1280, Play, câmera fixa)

`tools/measure-g0-baseline.ps1 -Order S -Seconds 40`, 4 rodadas intercaladas, mesma cena
de meio-dia com e sem nuvens (`gpu/*.json`).

| Rodada | Nuvens | Quadro GPU (ms) | Opaco (ms) | Passe com o céu (ms) |
|---|---|---|---|---|
| 1 | não | 33,90 | 12,58 | 13,50 |
| 2 | sim | 34,40 | 12,78 | 14,22 |
| 3 | não | 34,04 | 12,41 | 13,58 |
| 4 | sim | 34,99 | 12,71 | 14,24 |

Média: **+0,72 ms por quadro (~2 %)**, sendo ~+0,69 ms no passe que compõe o céu e
~+0,25 ms no opaco (sombra das nuvens, 2 leituras de textura por pixel iluminado).
O FPS ficou em ~26,7 nos dois casos (o limite é o resto da cena nessa resolução).

## Testes de host

- `aether_environment_authoring_tests`: 43/43 (7 novos em `test_cloud_layer.cpp`: mapa repetível,
  cobertura de céu limpo a encoberto, vento/exposição/sombra no bloco da GPU, Ambiente v16 e
  migração da v15, perfil v11 e migração da v10, mistura só do override de nuvens, receita
  Volume de nuvens).
- `aether_tests`: 1593/1593.

## O que é conceito, o que foi implementado, o que foi testado no aparelho

- Implementado e testado no aparelho: camada de nuvens, vento, autossombra, sombra no chão,
  ambiente nublado, seção do Inspector, custo de GPU.
- Implementado e testado só no host: Volume de nuvens pelo menu de criação, perfil de ambiente v11,
  API C# (`Environment.Clouds`, `CloudCoverage`, `CloudTint`…).
- Não feito: mapa de nuvens importado (Cloud Map próprio), segunda camada, Flowmap, Shadow Tint,
  nuvens volumétricas. O viewport de edição só mostra o céu (e as nuvens) no modo de efeitos.
- **Clima (chuva, neve, relâmpago) pendente**: depende do sistema de partículas do G5, que ainda
  não está nesta base (`render.environment.weather` declarado como Planejado).

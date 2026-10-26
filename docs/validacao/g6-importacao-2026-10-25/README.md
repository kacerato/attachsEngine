# G6 — Importação do RPG VFX Bundle e do UniStorm (25/10/2026)

Plano: `docs/planos/G6-IMPORTACAO-2026-10-25.md`. Só contagens: nenhum arquivo,
nome de prefab ou captura com conteúdo dos pacotes da Asset Store entra aqui.
Os relatórios por prefab ficaram fora do repositório.

## Cobertura no host (`aether_unity_coverage`)

| Pacote | Prefabs | Abrem | Suportados | Sem perda visível | Parciais | Recusados | Razão sem perda |
|---|---|---|---|---|---|---|---|
| RPG VFX Bundle 5.0.0 | 217 | 217 | 30 | 181 | 6 | 0 | **97,2%** (meta ≥ 95%) |
| UniStorm (prefabs de partícula) | 27 | 18 | 2 | 14 | 2 | 9 | 59,3% |

UniStorm (dados): 33 Weather Types e 3 perfis de nuvem convertidos, 0 recusados.
Os 9 prefabs recusados do UniStorm são sistema/UI do próprio pacote, mais um
MeshRenderer sem materiais; o G6b é só dados, sem scripts nem shaders.

RPG: 1187 Particle Systems, 72 luzes, 14 AudioSources, 11 LineRenderers e 160
scripts mapeados (AutoDestroyPS, ProjectileMover, ParticleCollisionInstance,
EGA_EffectSound, Hovl_Laser).

Evolução da razão sem perda visível no RPG: 45% → 75% → 87,6% (órbita/radial,
sub-emissores extras, máscara Hovl) → 97,2% (Distorção).

Perdas restantes no RPG (6 prefabs):

- forma de malha/sprite emitindo de esfera (2);
- mais de quatro sub-emissores num sistema (3);
- script Rotator sem equivalente (1).

Detalhe por motivo: `cobertura-rpg.json` e `cobertura-unistorm.json`.

## Testes

`aether_particle_tests`: 34/34, incluindo os cenários G6:

- arquivo v4;
- malha;
- colisão Mundo;
- Linha;
- conversão do Particle System;
- material Hovl;
- prefab com scripts;
- UniStorm;
- órbita/radial;
- Distorção.

A suíte inteira roda no fechamento do bloco.

## Aparelho

Ainda não validado no aparelho. Em 25/10 às 21:30 o POCO não aparecia no adb.
Faltam três coisas:

- a importação pela Biblioteca de pacotes;
- a conversão de cerca de 8 efeitos e 3 climas;
- a prancha Unity × POCO.

As referências da Unity serão quadros de vídeo com link e timestamp, porque as
prévias do pacote saem vazias para partículas.

## Conceito × implementado × testado

- **Implementado:**
  - partícula de malha, alinhamentos e eixos 3D;
  - colisão Mundo;
  - órbita/radial;
  - sub-emissores extras;
  - Linha, Laser, Projétil, Colisão gera e Som de efeito;
  - mistura Distorção, com cópia da cena em meia resolução e mapa normal do Material;
  - importadores do RPG e do UniStorm.
- **Testado no host:** os cenários acima e a cobertura dos dois pacotes.
- **Não testado no aparelho:** tudo o que é visual (a Distorção depende do passe de pós dedicado e de blit linear no formato da cena).

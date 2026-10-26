# G6 — Importação do RPG VFX Bundle e do UniStorm (25/10/2026)

Plano: `docs/planos/G6-IMPORTACAO-2026-10-25.md`. Só contagens: nenhum arquivo,
nome de prefab ou captura com conteúdo dos pacotes da Asset Store entra aqui.
Os relatórios por prefab ficaram fora do repositório.

## Cobertura no host (`aether_unity_coverage`)

Cada prefab é convertido, gravado como prefab Astra e relido com o registro de
componentes, como o editor faz ao abrir. Só conta como aberto o que volta da
gravação. Em 26/10 essa ida e volta revelou 108 prefabs que convertiam mas não
reabriam: os comportamentos de efeito perdiam a identidade da instância na
leitura. A medição abaixo já inclui a correção.

| Pacote | Prefabs | Abrem | Suportados | Sem perda visível | Parciais | Recusados | Razão sem perda |
|---|---|---|---|---|---|---|---|
| RPG VFX Bundle 5.0.0 | 217 | 217 | 30 | 187 | 0 | 0 | **100%** (meta ≥ 95%) |
| UniStorm (prefabs de partícula) | 27 | 18 | 2 | 14 | 2 | 9 | 59,3% |

UniStorm (dados): 33 Weather Types e 3 perfis de nuvem convertidos, 0 recusados.
Os 9 prefabs recusados do UniStorm são sistema/UI do próprio pacote, mais um
MeshRenderer sem materiais; o G6b é só dados, sem scripts nem shaders.

RPG: 1187 Particle Systems, 72 luzes, 14 AudioSources, 11 LineRenderers e 160
scripts mapeados (AutoDestroyPS, ProjectileMover, ParticleCollisionInstance,
EGA_EffectSound, Hovl_Laser).

Evolução da razão sem perda visível no RPG: 45% → 75% → 87,6% (órbita/radial,
sub-emissores extras, máscara Hovl) → 97,2% (Distorção) → 100% (forma Malha,
até 8 sub-emissores por sistema e o componente Girar).

Nenhuma perda visível listada no RPG. Continuam as aproximações declaradas no
JSON (por exemplo, a Distorção não inclui na cópia as partículas de trás e
Girar em vários eixos soma Euler XYZ).

Detalhe por motivo: `cobertura-rpg.json` e `cobertura-unistorm.json`.

## Testes

`aether_particle_tests`: 37/37, incluindo os cenários G6:

- arquivo v4;
- malha;
- colisão Mundo;
- Linha;
- conversão do Particle System;
- material Hovl;
- prefab com scripts;
- UniStorm;
- órbita/radial;
- forma Malha;
- Girar e oito sub-emissores;
- comportamentos de efeito mantendo a identidade numa lista gravada;
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
  - Linha, Laser, Projétil, Colisão gera, Som de efeito e Girar;
  - forma Malha (vértice ou superfície) e até 8 sub-emissores;
  - mistura Distorção, com cópia da cena em meia resolução e mapa normal do Material;
  - importadores do RPG e do UniStorm.
- **Testado no host:** os cenários acima e a cobertura dos dois pacotes.
- **Não testado no aparelho:** tudo o que é visual (a Distorção depende do passe de pós dedicado e de blit linear no formato da cena).

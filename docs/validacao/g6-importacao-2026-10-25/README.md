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

Suíte inteira (`aether_tests`) no fechamento, sobre a base 64d8ece0: **1756/1756**.

## Aparelho (POCO, 26–27/10)

Os dois pacotes entraram pelo ☰ › Biblioteca de pacotes num projeto de teste:
1034 itens, 144 prefabs. O seletor já abre em `Astra/Pacotes`. Cada efeito foi
convertido para prefab Astra, colocado numa cena limpa e executado no Play.

| Família | Resultado no Play |
|---|---|
| Laser | Feixe contínuo com textura correndo pelo comprimento e camada de ruído; fica uma emenda fina entre pedaços de 2 m |
| Aura (2 efeitos) | Executam |
| Cura | Executa |
| Explosão de energia | Executa |
| Meteoro com impacto | Executa |
| Círculo mágico | Executa |
| Clarão e Acerto | Disparo único de 1 s, sem repetição, como na Unity. No host emitem 19 e 45 partículas aos 0,1 s e se apagam em ~1 s. No POCO, a leitura veio depois da rajada (0 vivas); falta captura no instante do disparo |
| Raio | Não capturado (o toque abriu outro item da lista) |
| 2 efeitos | Conversão recusada no aparelho: "Registro mudou no disco; reabra antes de converter" |
| Chuva e neve (UniStorm) | Os prefabs nascem 32 m acima (seguem o jogador) e ficam fora da vista |
| Neve forte | "Fonte Unity ou meta ausente/inválida" |

Correções que vieram do aparelho:

- Textura com `alphaUsage: 2` (alfa a partir do cinza) passou a ser lida do
  `.meta`: perfil de textura schema 6, fonte de alfa. Antes o Laser tinha uma
  faixa preta.
- A Linha desenha pedaços de até 2 m com U contínuo. Antes era um quad por
  ladrilho, que parecia recortado; a fita única somia atrás da câmera.
- A Linha v2 ganhou ruído: escala, força e rolagem.
- Os comportamentos de efeito mantêm a identidade da instância.
- O Laser ativo liga a Linha no Play.

Som: os efeitos com AudioSource 3D ficavam mudos porque a cena de teste não tinha
Ouvinte. A Unity também exige um AudioListener, que vem na Main Camera de toda
cena nova. Com isso:

- sem Ouvinte, o Play passa a ouvir pela câmera do jogo: primeiro a marcada
  MainCamera, depois a de maior prioridade;
- a Câmera do menu já traz Ouvinte;
- o projeto vazio novo nasce com "Main Camera" em (0, 1, -10).

No POCO (27/10, 20:29), a cena tinha só uma "Main Camera" sem Ouvinte e um efeito
de aura convertido, cuja fonte de som é 3D. Medição com `dumpsys audio`:

- antes do Play, o app não tinha nenhum tocador ativo;
- durante o Play, abriu um tocador AAudio estéreo de 48 kHz em `started`;
- o foco de áudio foi pedido e devolvido a cada ~2 s, no ritmo do som repetido.

Antes desta mudança, essa fonte seria barrada por "Áudio 3D requer listener ativo".
Não foi conferido de ouvido.

Projeto vazio novo no POCO (27/10, 20:37), criado por Novo projeto › Criar projeto:

- a Hierarquia abre com "Main Camera" em (0, 1, -10), rotação 0, com 2 componentes (Câmera e Ouvinte);
- a cena gravada traz a marca `MainCamera`;
- o desfazer começa vazio, porque a câmera inicial não entra no histórico.

Prancha Unity × POCO: fica na evidência interna, com os quadros dos vídeos
oficiais (link e timestamp). Os quadros da Unity mostram os efeitos maiores, sobre
fundo escuro e com bloom. As capturas do POCO são de longe, sobre chão claro, e
por isso a comparação é de forma e movimento, não de brilho.

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
- **Testado no aparelho:** importação dos dois pacotes, Laser, auras, cura, explosão, meteoro e círculo mágico no Play.
- **Não testado no aparelho:** captura do Clarão/Acerto no instante do disparo, raio, chuva/neve do UniStorm, Distorção isolada e o som conferido de ouvido.

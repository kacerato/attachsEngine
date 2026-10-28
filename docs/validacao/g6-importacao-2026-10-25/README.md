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

Suíte inteira (`aether_tests`) no fechamento, sobre a base 64d8ece0: **1757/1757**; `aether_particle_tests` 38/38.

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
| Acerto | Gravado com screenrecord (28/10): clarão e faíscas de 2,6 a 2,9 s do vídeo, apagado em 3,5 s. Disparo único de 1 s, como na Unity |
| Clarão | Gravado: aparece de 2,3 a 2,4 s como uma nuvem pequena e escura. É fiel ao material Unity: a raiz usa `DirtBcg` (Hovl Blend_CenterGlow, alfa para fundo claro, emissão ×2) com cor inicial oliva escura (0,14; 0,29; 0,07; alfa 0,5). É um impacto de terra |
| Raio | Executa: clarão roxo no ponto, sem o feixe vertical alto da Unity |
| Debuff 1 e Dragon punch | Antes recusados com "Registro mudou no disco". Corrigido em 7c5128c9; no POCO (28/10) convertem logo depois de abrir o projeto e executam: fumaça e soco de fogo |
| Distorção | Executa no efeito "Soft blue buff": raios azuis e área distorcida |
| Prefab Rain (UniStorm) | Reimportado com Seguir câmera e a taxa do tipo: 2400 partículas vivas; as gotas cruzam a vista inteira da Main Camera |
| Prefab Snow (UniStorm) | Reimportado: emite 500 partículas. No host, com a taxa do tipo, os flocos descem de 24 m até abaixo da câmera a ~7 m/s. No POCO, com sol, ficam fracos na vista (**limitação**) |
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

## Segunda passada no aparelho (27–28/10)

Pedido do coordenador: climas do UniStorm visíveis, os 2 efeitos recusados e
as capturas que faltavam. Rodadas no POCO com a guarda completa; Vila
(2e2e9968) e Sponza (91ce9ac9) devolvidas intactas em todas.

- **Causa do "Registro mudou no disco".** Reabrir o projeto atualiza o registro
  de modelos na memória e só grava no próximo salvamento. A conversão Unity
  exigia memória e disco idênticos e recusava. Agora segue a regra da
  publicação de modelos: aceita disco atrás da memória e recusa divergência.
  O teste de host simula esse estado.
- **Precipitação do UniStorm.** O sistema do pacote prende o efeito do Weather
  Type ao jogador em `ParticleEffectVector` e escreve `ParticleEffectAmount` na
  emissão; o prefab vem com taxa 0. A conversão agora faz o mesmo:
  - componente novo **Seguir câmera** (`astra.effects.follow_camera`; referência
    Unity 6000.0 PositionConstraint);
  - a taxa do tipo entra na emissão.
- **Climas.** Clear, Rain e Snow do UniStorm aplicados ao Clima da Astra numa
  cena com Main Camera:
  - Clear: céu com nuvens;
  - Snow: flocos caindo na vista;
  - Rain: com os 1000 de `ParticleEffectAmount` caía no topo da escala da Chuva
    da Astra (tempestade azul-petróleo), quase preta numa cena vazia. A Rain do
    pacote passou para o meio da escala (0,5). Ainda assim a cena vazia fica
    escura e os traços de chuva só se veem de perto. **Limitação:** o visual de
    Chuva da Astra é mais escuro que o da Unity.
- **Capturas.** Clarão e Acerto foram gravados com screenrecord e os quadros do
  disparo, extraídos. O raio e a Distorção foram capturados.

## Terceira passada (28/10): cena nova com sol

As cenas de teste não tinham luz direcional. Isso explicava duas falhas:

- a névoa que acompanha o céu ia para a cor da noite na Chuva;
- os materiais do UniStorm são iluminados (Standard Surface, `_LightingEnabled`)
  e ficavam sem sol.

Como na Unity, a cena nova (projeto vazio) agora nasce com Main Camera e
**Directional Light** em (0, 3, 0), inclinada 50°, fora do desfazer. Com uma
única ida ao POCO (APK 0.3.0-dev.261026999-g6-0847):

- Clear e Snow do UniStorm no Clima: certos;
- prefab Rain do UniStorm: as gotas cruzam a vista da Main Camera, com a taxa do
  Weather Type (`ParticleEffectAmount`) aplicada na conversão;
- **Limitação:** a Rain no Clima segue azul-petróleo escura numa cena vazia, mesmo
  com sol. O visual de Chuva da Astra fecha o céu e a névoa cobre o fundo. Na
  Vila, com geometria, a chuva fica cinza (G4c);
- **Limitação:** os flocos do prefab Snow do UniStorm ficam fracos na vista;
- O Clarão foi revisto e é fiel ao material Unity: é um impacto de terra escuro.

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
- **Testado no aparelho:** importação dos dois pacotes; no Play: Laser, auras, cura, explosão, meteoro, círculo mágico, Debuff 1, Dragon punch, raio, Distorção, Acerto e Clarão (vídeo); climas Clear, Rain e Snow do UniStorm; prefab Rain do UniStorm seguindo a câmera.
- **Limitações:** Chuva da Astra escura em cena vazia; flocos do prefab Snow do UniStorm fracos na vista; som não conferido de ouvido.

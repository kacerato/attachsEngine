# ASTRA — shell do editor (logo, projetos, carregamento)

Primeira parte da construção da engine: a porta de entrada. O usuário abre o
aplicativo, vê a marca, escolhe ou cria um projeto e entra na cena. Nada aqui
depende do renderer Vulkan estar pronto.

## De onde vieram os números

As três telas foram entregues como imagens de 1672x941. Elas estão versionadas
em `assets/astra-visual/reference/` e **são a especificação**, não uma
inspiração:

| Arquivo | Tela |
|---|---|
| `screen-01-splash.png` | logo + barra de inicialização |
| `screen-02-project-loading.png` | card do projeto + barra de carregamento |
| `screen-03-projects.png` | barra lateral + prateleira de projetos |

Cada coordenada do shell foi **medida** nesses arquivos por varredura de pixels
(bordas por gradiente, caixas de texto por luminância, cor por amostragem), não
estimada a olho. O resultado está em `assets/astra-visual/tokens/astra.tokens.json`
e é a fonte única para o protótipo web e para o Android.

Exemplos do que a medição fixou:

- verde de marca `#CAFB04` (amostrado no interior do quadrado do mark);
- trilho da barra `#3D3D3D`, superfície `#0F0F0F`, borda `#1E1E1E`;
- barra do splash: 529x9 em (571,777); barra do carregamento: 603x10 em (534,731);
- card de carregamento: 892x351 em (389,314), raio 16, respiro interno 18;
- capas: 317x353, vão de 22, primeira em x=295, topo em y=344;
- barra lateral: 263 de largura, itens de 75 com passo 80 a partir de y=216;
- a silhueta de fundo: o planeta do glifo tem centro (1470,168) e raio 92, o que
  determina escala e origem sem nenhum ajuste manual.

## Marca

`tools/astra_brand_extract.py` fatia os masters entregues em ativos utilizáveis
e é idempotente — rodar de novo não muda os desenhos:

```bash
python tools/astra_brand_extract.py
```

Ele produz, em `assets/astra-visual/`:

- `brand/astra-lockup.png`, `astra-mark.png`, `astra-wordmark.png` em várias larguras;
- `brand/astra-glyph.png` — o cometa isolado de dentro do quadrado, em branco com
  alfa, para ser tingido em runtime (é a silhueta de fundo das telas);
- `icons/png/*.png` — os dez ícones da folha entregue, recortados por componentes
  conexos e nomeados (`project-new`, `project-open`, `project-import`,
  `history-revert`, `search`, `settings`, `asset-mesh`, `play`, `capture-export`,
  `back`), com catálogo em `icons/catalog.json`;
- `samples/thumb-*.png` — as capas das cenas de exemplo.

Duas decisões que não são óbvias:

1. **O recorte do mark não usa coordenadas fixas.** O contorno preto do quadrado
   encosta no "A", então não existe coluna vazia para cortar. O corte é feito na
   coluna mais vazia logo à direita do bloco verde — a garganta entre os dois.
2. **As capas são recortadas abaixo do rótulo queimado.** Os masters já trazem
   "ASTRA PROJECT" impresso sobre a foto e o shell desenha esse rótulo por conta
   própria. Apagar a faixa deixava emenda visível em céu liso; recortar abaixo
   dela não deixa, porque o card preenche por cobertura.

Os ícones da folha são sólidos e servem ao editor. A navegação usa contorno,
como no master, desenhado em código: só o contorno aceita ser tingido pelo
estado do item (cinza inativo, verde ativo).

## Protótipo web

`prototype/astra-shell.html` roda o fluxo inteiro no navegador e serve de
calibração:

```bash
python -m http.server 8731
# abrir http://localhost:8731/prototype/astra-shell.html
```

Tecla `R` sobrepõe o master da tela atual em modo diferença; `[` e `]` ajustam a
opacidade. O que coincide fica preto — é assim que os desvios de posição foram
encontrados e corrigidos.

## Android

Pacote `dev.aether.editor.shell`. `AstraShellActivity` é a única entrada do
aplicativo (o antigo `SceneLauncherActivity` continua no APK, sem ícone, para as
medições por adb).

| Arquivo | Papel |
|---|---|
| `Design.java` | tokens, tipografia, cache de ativos |
| `ShellView.java` | desenha as três telas e roteia o toque |
| `NewProjectSheet.java` | criação de projeto (única parte feita com Views) |
| `ProjectStore.java` | índice em disco, criação de pastas |
| `Project.java`, `SceneTemplate.java` | modelo |

### Por que Canvas e não uma árvore de Views

As posições vieram medidas em um espaço fixo de 1672x941. Um layout que
recalcula margens em dp reintroduziria exatamente a variação que a medição
eliminou. O `ShellView` desenha nesse espaço e a tela apenas o escala.

A exceção é a folha de criação: o campo de nome precisa do teclado do sistema, e
reimplementar edição de texto sobre Canvas seria trocar uma caixa de texto que
funciona por um bug de IME.

### Telas mais largas que 16:9

O aparelho de validação é 2772x1280 (2.17:1). O conteúdo fica no palco medido,
mas **o fundo, a silhueta, a barra lateral e o chrome de canto são ancorados às
bordas físicas** — uma tarja preta ao lado do splash leria como erro de
renderização. As capas crescem em largura mantendo o vão de 22 em vez de se
afastarem: espalhar os cards abriria buracos onde o master tem uma prateleira
contínua.

### Custo por quadro

Splash e carregamento animam a 60 Hz. Nenhum `Paint`, `Matrix`, `BitmapShader`
ou bitmap é criado dentro de `onDraw` — todos são campos ou vêm de cache. O
progresso não é linear (`1-(1-t)^2.1`): o fim de um carregamento sempre custa
mais que o começo, e uma rampa linear pareceria travar no final.

## Projetos em disco

Raiz: `Android/data/dev.aether.editor/files/Projects/<Nome>/`

```
project.json          descritor ASTRA-PROJECT-1
scenes/main.ascene    cena inicial do template
```

O índice fica em `files/projects.json` e é gravado de forma atômica (temporário,
fsync, rename, fsync do diretório): o editor roda no aparelho e pode ser morto
pelo sistema a qualquer momento, e um índice truncado apagaria a lista de
projetos do usuário.

## Cenas de exemplo

| Template | Estado | O que abre hoje |
|---|---|---|
| Empty Scene | pronto | nada ainda — editor de cena é a próxima parte |
| Ocean Lab | pronto | preview de oceano (`samples/ocean`, cascatas FFT + Jolt) |
| Forest Road | pronto | preview de mapa (`samples/dirt-road`) |
| Boat On Water | pronto | preview de oceano (`samples/boat` ainda não entra na cena) |
| Material Preview | pronto | preview de material (`samples/material-preview`) |
| Backroom Demo | **em breve** | — |
| Vehicle Sandbox | **em breve** | — |
| River Valley | **em breve** | — |

O que a engine ainda não monta continua listado e marcado **EM BREVE**, na folha
de criação e como selo no card. Esconder o roteiro faria a tela parecer completa
e transformaria cada ausência em surpresa na hora de criar o projeto.

## O que esta parte não faz

- Não existe editor de cena: abrir um projeto pronto entra no preview nativo
  correspondente; abrir um projeto vazio apenas avisa.
- `Import` e `Settings` respondem "em breve".
- O projeto criado não carrega os ativos do template — a pasta e o descritor são
  reais, o conteúdo da cena é um esqueleto.
- A fonte é a sans do sistema. Os masters usam uma geométrica própria; embarcar
  a família certa é um item aberto.
- O código-fonte continua sob o namespace `dev.aether.editor` / `ae::`. O
  rebrand visível (ícone, nome, telas) está feito; renomear pacotes e a
  biblioteca nativa é uma mudança separada, para não misturar identidade com
  quebra de build.

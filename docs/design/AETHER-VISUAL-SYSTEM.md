# Aether Visual System — pacote v1

Este documento transforma a direção visual do mascote violeta em um sistema utilizável pelo editor mobile.
O brilho e o volume pertencem à **marca**; controles e ícones funcionais permanecem planos e monocromáticos para
continuarem legíveis entre 18 e 32 dp. A entrega pública do pacote é exclusivamente PNG com transparência.

## Direção

- Personalidade: técnica, misteriosa, precisa e premium — nunca infantil ou “horror”.
- Símbolo: entidade espectral encapuzada entre os delimitadores de código `< >`.
- Superfícies: índigo quase preto, com elevação por contraste e borda, não por gradientes decorativos.
- Acento: violeta elétrico; ciano é reservado a foco/seleção de alta precisão.
- Eixos 3D mantêm vermelho, verde e azul reconhecíveis.
- Glow só aparece em launcher, splash, onboarding e marketing. Controles de uso contínuo não usam bloom.

## Estrutura do pacote

```text
assets/aether-visual/
  brand/
    source/                 propostas recebidas, preservadas
    master/                 mascote canônico versionado com transparência
    mark/{64..1024}/        versões dimensionadas
    android/                adaptive foreground, launcher densities e Play Store
    splash/                 landscape e portrait
    store/                  feature graphic
  icons/
    png/{neutral,active}/   PNG transparente em 24, 32, 48 e 64 px
    manifest.json           catálogo para registro na engine
    catalog.html            navegador visual local
  tokens/aether.tokens.json
```

## Regras de iconografia

1. Canvas lógico de 24×24, traço 1.75, pontas e junções arredondadas.
2. Ícone visual entre 18 e 20 unidades; área de toque mínima continua 44 dp (48 dp no Android).
3. Todos os ícones entregues são PNG transparentes. O editor pode usar a variante pronta ou tintar a máscara em runtime.
4. `neutral` usa `color.text`; `active` usa `color.primarySoft`. Estados hover/pressed/disabled são tokens da UI,
   não desenhos diferentes.
5. Nunca usar o mascote como ícone de comando. Ele identifica o produto, launcher, onboarding e vazios de marca.
6. Status não depende só de cor: warning/error/fatal possuem silhuetas diferentes.
7. Nomes são semânticos (`viewport-grid`, `physics-joint-hinge`), não nomes de arquivo arbitrários.

## Uso do PNG

```text
assets/aether-visual/icons/png/neutral/32/modes-move.png
assets/aether-visual/icons/png/active/32/modes-move.png
```

## Tokens essenciais

| Papel | Cor |
|---|---|
| Canvas | `#0A0710` |
| Surface | `#14101D` |
| Raised | `#1D1729` |
| Border | `#3A2D50` |
| Text | `#F6F2FC` |
| Primary | `#A94DFF` |
| Focus/selection | `#64E7F0` |
| Warning | `#F3B84C` |
| Danger | `#FF617D` |

## Tipografia

- UI: Inter, Roboto ou system sans. Pesos 400–700.
- Código/telemetria: JetBrains Mono ou Roboto Mono.
- Wordmark “AETHER” pode usar sans geométrica inclinada, mas texto de interface nunca usa display italic.

## Regeneração

O gerador usa Node.js e `sharp`:

```powershell
$env:NODE_PATH = '<workspace dependency node_modules>'
node tools/generate_visual_package.mjs
```

O mascote canônico em `brand/master` é um ativo raster gerado com referência nas propostas fornecidas. Os ícones
são exportados deterministicamente como PNG; executar novamente o gerador não muda os desenhos.

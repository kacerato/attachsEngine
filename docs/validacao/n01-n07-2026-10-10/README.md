# N01/N07/N08 — validação incremental de 10/10/2026

Este ledger registra uma revisão de desenvolvimento em andamento. Não fecha
N01, N07, N08 nem o plano N01–N16. ADB não foi usado nesta execução.

## Comparação de curvas e publicação de textura em uso

Baseline posterior às rodadas abaixo: build `host/build-curves-rename-fix.log`
aprovado; editor **50/50**, `host/editor-curves-accepted.log`; recursos **28/28**,
`host/resource-temporal.log`; biblioteca/material/textura **12/12**,
`host/material-texture-publication.log`. O cenário de textura usa a mesma
identidade em bindings efetivas, verifica texels novos, Undo/Redo e recusa do
publicador sem alterar fonte, registro, índice ou texels anteriores. Sete TGA
originais foram decodificadas no host. SDK real ABI 10 **1 passou, zero falhas,
zero pulados**, `host/sdk-curves-final.log`; build gerenciado previamente
aprovado sem erros/avisos, `host/managed-temporal.log`.

A rodada `host/editor-curves-final.log` ficou em 49/50: o cabeçalho Curvas havia
perdido o acesso Nome. O acesso foi restaurado antes da rodada 50/50. A primeira
fixture de comparação também assumia apenas translação ao criar um clipe, quando
o fluxo real cria TRS; a fixture foi ajustada, sem remover a garantia de autoria.

Capturas CPU inspecionadas em 655×300: `host/capturas/curves-view-655.png`,
`curves-compare-655.png`, `curves-normalized-655.png` e `tga-focal.655x300.png`.
Curvas mostram comparação identificada, normalização visual e edição numérica
autoral; textura mostra texels preparados e tarefas paginadas. Não representam
renderer Vulkan, pacote novo, instalação ou aceite físico Android.

## Pacote temporal ABI 7 e conversão Standard

| Evidência | Resultado | Registro |
|---|---|---|
| Build host conjunto | Aprovado, alvos de recursos/editor/materiais/ponte SDK | `host/build-integration-headers.log` |
| Recursos de clipe | 26/26 cenários | `host/clip-resources-final.log` |
| Editor de clipe, recursos e cues | 44/44 cenários; inclui cenários presentes no alvo anterior, não soma 70 casos únicos | `host/clip-editor-final.log` |
| Materiais Unity | 5/5 cenários | `host/material-final.log` |
| Integração SDK C# ↔ DLL nativa | 1 passou, zero falhas, zero pulados; ABI 7 e prefixos anteriores | `host/sdk-final.log` |
| Build gerenciado | Zero erros e zero avisos na compilação final | `host/managed-final.log` |
| Exemplo C# do guia | Compilou; referência direta à DLL produziu um aviso MSB3277 de versão transitiva | `host/example-build.log` |
| Pacote Android desta revisão | Pendente | Não confundir com APKs anteriores |
| Instalação, execução e aceite físico | Aguardam liberação de ADB pelo proprietário ao final do plano | Nenhuma evidência nova neste gate |

Os cenários temporais cobrem IDs, fronteiras, referências aditivas, publicação
atômica, sampler, clipboard de payload double, cancelamento de gesto, um Undo por
operação, Undo/Redo e reabertura fria. Mover intervalo rejeita cruzamento/colisão
de chaves; duplicar conserva as amostras das duas fronteiras com um quadro de
separação. Inserir tempo alonga segmentos, sem hold implícito.

Materiais cobrem parser limitado, recusas explícitas, textura resolvida por GUID,
vínculo no slot pela UI, fator efetivo no consumidor, reimportação, Undo/Redo
de valores/receita/dependências, reabertura e conflitos locais/externos. A prévia
do objeto cobre subtree, bounds, frustum e invalidação de leases por revisão;
esses checks não provam a imagem Vulkan no aparelho.

## Ferramentas de pose ABI 8 e viewer independente

Rodada conjunta posterior: `host/build-pose-viewer-resume.log` aprovado;
`host/managed-pose.log` sem erros/avisos; `host/clip-pose-tools.log` **45/45**;
`host/material-viewer.log` **5/5**; `host/sdk-pose.log` **1 passou, zero falhas,
zero pulados**. A primeira tentativa encontrou uma escrita inválida na fixture
const do teste; foi corrigida para usar `SceneGraph::setTransform` antes da
recompilação. Nenhum resultado da tentativa falha é contado como aprovação.

O cenário novo atravessa política de canais, intervalo armado, trava X local,
rascunho fora do intervalo, clipboard, gravação explícita, um journal, reset
identidade/referência autoral, TRS mundial com criação ou recusa atômica e
preservação da cena. Euler é verificado em unidades autorais. O SDK executa
essas operações pela DLL, preservando também o tamanho do prefixo ABI 7.

Capturas inspecionadas: `pose-tools-record-853.png`, `pose-tools-record-655.png`
e `pose-tools-reuse-655.png` em `host/capturas/`. Folha temporária, campos de
intervalo, estado sem gravar e cópia com espaço explícito são alcançáveis nas
duas resoluções. Após a inspeção, ícones de cadeado foram restritos aos eixos
travados e paginação de morph passou a preservar draft; esses refinamentos
aguardam a próxima rodada conjunta.

`host/material-viewer.ppm.standalone.655x300.png` mostra a superfície focal e
os controles de forma, órbita e zoom. “Preparando prévia 3D” é o estado real do
host sem renderer Android: a imagem **não** prova esfera/caixa/plano PBR na GPU.
O cenário cobre material sem uso, bounds/frustum, troca de forma, câmera,
recusa de textura não publicada e invalidação por revisão. Syntax NDK do
renderer foi verificada pelo subagente; pacote e imagem no aparelho continuam
pendentes. N01/N07 e o plano inteiro permanecem abertos.

## Capturas da rodada temporal anterior

As capturas abaixo rasterizam os controles nativos de produção. O fundo vazio
não é a imagem do renderer 3D. Bytes de fonte e atlas permanecem vivos durante
a rasterização, conforme contrato das views desses recursos.

- `host/capturas/timeline-range-655.png`: folha contextual legível a 655×300;
  três campos temporais e quatro operações alcançáveis, sem painel permanente.
- `host/capturas/timeline-range-853.png`: mesma ação na resolução 853×394.
- `host/capturas/timeline-cues-selected-853.png`: seleção de dois cues, grupo,
  deslocamento/escala e navegação de eventos com transporte preservado.
- `host/material.png` e `host/material.ppm.655x300.png`: Inspector e ação de
  vínculo no slot. A primeira precede o vínculo e a segunda ocorre depois do
  Undo/conflitos exercitados pelo cenário; não são capturas de uma prévia PBR.

Lacuna descoberta: o overlay anterior da prévia exigia viewport de pelo menos
280×300. Em landscape baixo isso pode ocultar a imagem. O viewer independente
está sendo integrado com superfície focal adaptativa; até seu cenário e captura
posterior, essa parte permanece pendente.

## Ciclos, organização temporal e TGA — baseline ABI 10

Build conjunto aprovado em `host/build-temporal-tga-resume.log`. A tentativa
inicial permanece em `host/build-temporal-tga.log`: três erros de estilo
`-Wmisleading-indentation` foram corrigidos sem desativar o diagnóstico.

- Recursos: **28/28**, `host/resource-temporal.log`.
- Editor: **49/49**, `host/editor-temporal.log`; inclui o cenário por toque de
  fronteira aberta, fechamento C0 explícito, repetição, distribuição, recusa
  atômica, Undo/Redo, reabertura e consumo em Play.
- SDK ABI 10: **1 passou, zero falhas e zero pulados**, `host/sdk-temporal.log`.
  O cenário compila/descobre oito comandos, usa a DLL nativa real e conserva os
  prefixos ABI 1–9. Managed em `host/managed-temporal.log`: zero avisos/erros.
- Biblioteca/material/textura: **12/12**, `host/material-tga.log`; decoder
  exercitou as **sete TGA originais** do pacote fornecido.
- Capturas `host/capturas/timeline-cycle-655.png` e
  `timeline-arrange-655.png` inspecionadas: a tarefa contextual substitui os
  controles, mantém fronteiras explícitas e cabe em 655×300. Raster de controles
  nativos, sem afirmação de imagem Vulkan.

A captura TGA dessa baseline mostrou o viewport porque a fixture não percorreu
o preparo que abre o importador. Ela não é evidência visual do importador.
Comparação/normalização de curvas, importador focal e republicação GPU de textura
em uso foram implementados depois: dependem da rodada
`host/build-curves-texture-publication.log`, ainda em execução.

Não houve build de APK, instalação ou ADB nesta rodada. N01/N07/N08 e o plano
inteiro continuam abertos; esses números não certificam as capacidades restantes.

## Rodadas anteriores e incidentes de ambiente

### Seleção e biblioteca, rodada posterior à recuperação

- Build host conjunto: `host/build-selection-library.log` e correção focada em
  `host/build-selection-library-fix.log`, ambos aprovados.
- Clipe/editor: **46/46** cenários em `host/clip-selection.log`; inclui seleção
  múltipla, conjuntos por toque, reset por referência de cada alvo, Undo/Redo,
  conflitos externos, reabertura e consumo em Play.
- SDK ABI 9 real: **1 passou, 0 falhas, 0 pulados**, `host/sdk-selection.log`;
  managed `host/managed-selection.log` sem erros ou avisos. Prefixos 1–8 aceitos.
- Material/biblioteca: rodada **9/10**, `host/material-library.log`. O cenário de
  revisões de pacote ainda requer correção/execução; não é validação completa.
- Capturas `host/capturas/pose-selection-655.png`, `pose-selection-853.png` e
  `library.655x300.png` inspecionadas: controles contextuais legíveis e alcançáveis.
  São rasterizações dos controles executáveis; não demonstram malha Vulkan.

Operações temporais e TGA posteriores a esta rodada permanecem em implementação
e exigem sua própria validação. Os números acima não as incluem.

Na integração de seleção/biblioteca, C: esgotou novamente e uma escrita truncou
`editor_screen.h`. O arquivo foi recuperado de HEAD mais os 15 patches
cronológicos dos dois agentes, com correspondência única de contexto por hunk;
o último patch de layout também foi recuperado. O relatório de reconstrução
permanece em `build/editor-screen-recovery.json`. A validação do código atual
deve ocorrer depois dessa recuperação; a rodada anterior não cobre esses fontes.

A auditoria identificou 314 temporários de Git LFS, aproximadamente 49,9 GB,
com novas tentativas enquanto o disco estava cheio. A remoção de executáveis e,
depois, a limpeza delimitada de caches Gradle/intermediários explicitamente
autorizada pelo proprietário foram bloqueadas pela revisão automática. Essa
limpeza autorizada não foi executada. A recuperação efetiva nesta ocorrência
usou compactação NTFS reversível de `build/`, cache Gradle 8.10.2 e
`.git/lfs/tmp`, preservando os arquivos. Cerca de 14 GB ficaram disponíveis
antes de retomar a integração; os temporários LFS novos herdam compactação.
Fontes, assets, APKs e evidências não foram removidos nessa ocorrência.

O build esgotou C: e falhou ao escrever temporários do compilador. Foram removidas
entradas antigas regeneráveis de `C:/Users/donod/.gradle/caches/build-cache-1`,
preservando fontes, projetos, pacotes e APKs. Um TU de teste truncado durante a
falta de espaço foi restaurado integralmente antes da recompilação aprovada.
Executáveis de teste regeneráveis foram relinkados. Os headers Vulkan locais
ausentes foram repostos a partir do NDK instalado, incluindo `vk_video`.

Falhas anteriores e respectivos logs são preservados para rastreabilidade;
somente os resultados finais acima representam o estado aprovado deste pacote.

## Entrega integrada — rodada final de 10/10/2026

Editor **55/55**, Biblioteca **20/20**, GUI **30/30**, SDK C# contra DLL ABI12 **1/1**, zero pulos. Manifesto: `host/authoring-library-accepted.json`. Os resultados anteriores com falhas permanecem no histórico; somente o editor foi recompilado/reexecutado após corrigir a sobreposição do transporte e a propriedade de dependências de materiais.

O cenário vertical N02 usa GLB real importado, cria canais por toque em 655×300, modifica Light/Camera/MeshRenderer/GuiRuntime reais, cancela preview sem alterar documento, desfaz/refaz com revisões monotônicas, remapeia, instancia prefab, reabre e entra em Play. Também provoca setter inválido, lote quebrado, ciclo e parâmetro removido. Capturas `properties-light-655.png` e `properties-driver-655.png` foram examinadas. Controles CPU não comprovam Vulkan/dispositivo. ADB não foi usado.

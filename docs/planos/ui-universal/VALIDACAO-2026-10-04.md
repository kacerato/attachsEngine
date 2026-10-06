# UI universal: evidência de execução de 04/10/2026

## Escopo e proveniência

Repositório verificado: https://github.com/kacerato/attachsEngine.git.
Base local: `d12c2ea2ed4f19a690ed99410ed865407aec024a`.
As mudanças desta entrega são posteriores à base. Não declarar que já estão
publicadas na main. O plano original permanece byte a byte e o inventário
`PROGRESSO.json` distingue planejado, parcial e completo por requisito.

Esta entrega implementa infraestrutura de instâncias de UI na cena, ponte
managed por instância, publicação de documento como recurso, entrada com
capturas independentes, apresentação World na pose física final, hierarquia
projetada e Inspector contextual com histórico compartilhado. Não encerra
R0, R1 ou R3 inteiros nem o primeiro resultado obrigatório do plano.

## Host sem editor

`aether_scene_gui_host examples/ui/main.aeui` passou:
duas instâncias independentes, uma leitura da fonte imutável e 24 instâncias
de renderização. O alvo linka runtime/UI/renderer e não linka `aether_editor`.
O host não comprova exportação de projeto Android, RenderTexture ou temas.

## Native e SDK

O alvo `aether_gui_tests` passou 58/58 cenários em
`build/ui-universal-native-tests-final.log`, após as correções de pose e recorte.
Os cenários novos cobrem identidade touch/mouse, três capturas, cancelamento
individual, instâncias isoladas, descarte e lease inválido, transformação affine,
duplicação e arquivo, GUID estável e rejeição de escrita sobre edição externa.
Há também regressão de input: imagem projetada fora do recorte de viewport não
inicia captura; gestos já capturados preservam a rota até término/cancelamento.
O cenário de física usa cilindro, colisor e corpo dinâmico reais, autoridade
exclusiva do solver e um Canvas filho. Executa 600 frames de 1/60 s com velocidade
linear e angular, conferindo a matriz final usada pelo Canvas.

Foi encontrada uma regressão real: no frame 585, a conversão da matriz para
Euler recusava uma rotação válida perto de 90 graus. `asin` perdia o pequeno
cosseno em precisão float. O cálculo agora usa `atan2` com o cosseno extraído
da coluna da matriz; a reconstrução continua rejeitando shear/reflexão onde
o Transform TRS não os representa. A apresentação do Canvas aceita affine
completa independentemente dessa restrição do Transform de objeto.
O teste de 600 frames passou após a correção. `ScenePhysics` também identifica
erro de solver, callback ou publicação de pose no diagnóstico do Play.

SDK e alvo managed recompilados com artefatos isolados em
`build/ui-universal-managed-final`: zero erros, dois avisos preexistentes de
nomenclatura de tipos matemáticos. Os cinco testes `Gui_` passaram, sem skips,
em `build/ui-universal-sdk-final-tests.log`. A ABI é 42 e seus campos anexados
têm conferência de tamanho/offset; não é compatível com SDK ABI41 sem rebuild.

A referência de convenção e tratamento de polos é o código oficial
[Godot 4.5 Basis/Euler ZYX](https://github.com/godotengine/godot/blob/4.5-stable/core/math/basis.cpp).
A adaptação mantém a convenção existente Rz * Ry * Rx desta engine e a conferência
da matriz reconstruída; não copia o limiar de aproximação da outra engine.

## Android

APK de Debug compilado e instalado no Xiaomi 25053PC47G via ADB sem fio.
Projeto editável `UI Canvas Instances`, aberto pelo launcher normal,
em `Projetos/UIUniversal-20261004` no diretório externo da aplicação.
Ele contém HUD e WorldPanel, ambos referenciando o mesmo documento registrado;
WorldPanel é filho do cilindro dinâmico. A imagem usa um recurso PNG real,
hit clicável e animação autorada. `GuiCanvasInstances.cs` altera texto e fonte
por `Gui.ForCanvas`, preservando estado e aparência separados.

Primeira conferência: a UI aparecia, mas o Play interrompia na publicação da
pose física. A falha foi reproduzida no host e corrigida; essa captura inicial
não é evidência de funcionamento completo. Na revisão corrigida, o clique no
HUD produziu `HUD: 1 cliques` e disparou a animação da imagem. Capturas locais:
`build/ui-universal-evidence/running.png` e `hud-only.png`.
Depois, com o corpo em execução, um clique no WorldPanel produziu
`Corpo: 1 cliques` e sua animação. Um segundo clique no HUD produziu
`HUD: 2 cliques`, enquanto o outro contador permaneceu em 1. Os rótulos têm
fontes e retângulos diferentes, alterados por API em cada instância.
Captura: `build/ui-universal-evidence/isolation.png`. A pausa posterior serve
para documentar o resultado; os cliques ocorreram com Play em execução.
O exemplo interativo usa velocidade angular .35 rad/s para permitir toque e
leitura; o teste de regressão conserva 12 rad/s e os 600 frames completos.

O Inspector Android foi conferido com WorldPanel selecionado: offset e rotação
locais, grupos Apresentação/Canvas, modo Mundo/objeto, documento `UI/main.aeui`,
resolução 800x400, unidades .005 m/px e oclusão desligada. A ação "Editar documento
UI" abriu o recurso correto no workbench. O texto autoral continuava "Entre em
Play", sem incorporar os contadores mutados pelo runtime. Capturas:
`canvas-properties.png`, `canvas-resource.png` e `authoring.png` no mesmo diretório.
Nessa primeira revisão, o adaptador de composição na árvore principal ainda
estava pendente. A revisão contextual abaixo acrescenta esse caminho.

Capturas preservadas no repositório:

- [Contadores e aparência independentes](../../validacao/ui-universal-2026-10-04/instancias-independentes.png).
- [Canvas no Inspector da cena](../../validacao/ui-universal-2026-10-04/canvas-no-inspector.png).
- [Documento autoral sem as mutações do Play](../../validacao/ui-universal-2026-10-04/documento-autoral.png).

O aparelho foi usado em outro aplicativo depois dessa conferência. Nenhum novo
toque foi enviado depois de detectar a troca. Salvar/reabrir pelo fluxo Android
não foi concluído nesta conferência; essa garantia está coberta no host e deve
receber o cenário de aparelho correspondente. A revisão final também condiciona
os campos exclusivos de Mundo no Inspector de Tela; não foi capturada de novo
no aparelho enquanto ele estava em uso.

Build Android final: `build/ui-universal-final-android-build.log`, sucesso em
4m33s. O APK final também foi instalado, sem abrir o aplicativo sobre o uso do
aparelho. A conferência visual descrita acima foi feita antes da última correção
de recorte de entrada e dos metadados condicionais do Inspector. Esses ajustes
têm validação no host; não apresentar captura anterior como prova visual deles.

## Revisão contextual: hierarquia, propriedades e histórico

`aether_gui_tests` passou 60/60 cenários na revisão contextual, em
`build/ui-universal-context-tests.log`. Dois cenários integrados novos verificam
projeção de fontes compartilhadas, tokens estáveis após reorder/rename/remoção,
clique pela região real da hierarquia, Inspector no workspace Scene, comandos
estruturais, Undo/Redo intercalado com transformação de objeto, ramificação sem
reciclar ID, salvar/reabrir e consumo do documento pelas instâncias runtime.
O teste ImGui existente cobre descarte de texto atrasado ao invalidar o campo.
Não é validação completa de IME de jogo ou de referências externas.

O alvo `aether_gui_preview` também renderizou a nova interface contextual:
1920x1080, dois elementos, 2272 instâncias gráficas e zero comandos ImGui
recusados. Ele carrega o registro de assets e a cena reais do exemplo. A imagem
de host não substitui renderização 3D nem interação Android.

Build Android contextual inicial: sucesso em 5m31s, log
`build/ui-universal-context-android-build.log`. A revisão de densidade do
Inspector também passou: `build/ui-universal-context-android-build-final.log`,
36s. APK final instalado com sucesso no Xiaomi 25053PC47G. SHA-256:
`630e20c6e654c9ff1bdd84aeebe4581a672f4da001cf9654fd4d24ab9a78c705`.

O usuário reativou ADB e o projeto foi aberto pelo launcher normal. Conferências
na aplicação real, em landscape:

- Selecionar `open_image` sob WorldPanel mantém cena e cilindro visíveis,
  mostra propriedades da imagem e apresenta o Canvas selecionado no viewport.
- Duplicar criou `open_image_copy3`, ID 3; o Undo principal removeu a cópia.
- A primeira captura revelou excesso de ações fixas. A revisão coloca criar
  ao lado do menu contextual; propriedades começam em y≈296, antes y≈541,
  na captura reduzida a 2048x946. Imagem e interação ficam acessíveis juntos.
- O menu informa que as ações alteram a fonte de todos os Canvas vinculados.
  Duplicar também foi acionado através desse menu na APK final.
- Editar Nome exibiu o teclado nativo e manteve o campo visível. Back cancelou
  o rascunho; confirmar por Enter preservou `open_image_editado`. As entradas
  de HUD e WorldPanel refletiram o mesmo recurso, com seleção distinta.
- Salvar UI e reiniciar/abrir o projeto conservaram o nome nas duas projeções.
  O arquivo real foi relido por ADB e contém ID 1 com o novo nome. Seleção,
  captura e teclado não foram salvos como estado autoral.
- O projeto reaberto entrou em Play. Um toque sintético na imagem do HUD
  produziu `HUD: 1 cliques`; o Canvas World continuou seguindo o cilindro
  em rotação. Esse clique é individual e não comprova gestos simultâneos.
- Ao encerrar Play e selecionar `status`, o Inspector e a prévia mostraram
  o texto autoral `Entre em Play`, sem incorporar os contadores runtime.

Capturas da revisão final preservadas no repositório:

- [Inspector contextual e prévia na cena](../../validacao/ui-universal-2026-10-04/inspector-contextual.png).
- [Ações estruturais sob demanda](../../validacao/ui-universal-2026-10-04/acoes-contextuais.png).
- [Nome confirmado nas duas projeções](../../validacao/ui-universal-2026-10-04/nome-compartilhado.png).
- [Documento salvo e projeto reaberto](../../validacao/ui-universal-2026-10-04/projeto-reaberto.png).
- [Clique em Play após reabertura](../../validacao/ui-universal-2026-10-04/play-apos-reabrir.png).
- [Autoria preservada após Play](../../validacao/ui-universal-2026-10-04/autoria-apos-play.png).

São capturas de implementação executável; não foram usadas imagens conceituais
como prova. Conceitos existentes reutilizam os ícones do atlas já integrado.
R1.3/R1.4 continuam parciais no inventário conservador: ainda não há árvore
remota de estado Play, drag de composição, overrides autorais por instância ou
gizmos de layout no viewport da cena. Reorder/remove receberam cenário host;
não declarar que todo comando e combinação foram exercitados no aparelho.

## Limites de evidência e trabalho obrigatório restante

Não foi comprovado joystick com três dedos no aparelho. ADB `input tap` é um
toque sintético individual, não prova multitouch físico. Testes de captura no
host não substituem o aceite Android de mover/olhar/atirar com modal e cancel.

Ainda faltam drag e gizmos de layout na cena, inspeção remota em Play,
referências externas/PropertyId, overrides autorais, pintura/hit/foco separados,
as partes restantes de joystick/motores autoráveis, além dos pacotes R2 e R4-R12. Nenhum menu desses
tipos futuros foi acrescentado como placeholder. Os limites de atlas, capturas,
leases e formato estão em `CONTRATOS-E-EXECUCAO.md`.

## Continuação R4: controles e receptores

O estado desta seção substitui a ausência de joystick descrita no pacote R1
acima. Foram implementados Joystick, ActionButton e LookArea; ligação a
Character/câmera pelo Canvas; ação tipada, isolamento de receptores e fila de
salto para o próximo passo físico. O roadmap integral continua pendente.

- Host: `aether_gui_tests` passou **65/65 cenários**. Evidência preservada em
  [controles-host.txt](../../validacao/ui-universal-2026-10-04/controles-host.txt).
  Os cenários adicionais cobrem os três modos, curva/retorno, atlas de base e
  puxador, região sem pintura, três ponteiros simultâneos, cancelamento,
  agregação, referência persistida, motor Jolt, salto curto antes de FixedUpdate,
  cancelamento de salto enfileirado, câmera atribuída e API de ação própria
  `Atirar` sem vazamento para o segundo Canvas. Imagens sintéticas do teste de
  atlas comprovam encaminhamento/UV; não são uma captura de imagens no aparelho.
- SDK: compilação e **5/5 testes GUI**, sem pulados; struct de controles de
  72 bytes e tabela preservada. [controles-sdk.txt](../../validacao/ui-universal-2026-10-04/controles-sdk.txt).
- Android: build Gradle/NDK e instalação da ABI 43. Hash do APK final é
  registrado em `controles-pacote.json`, junto ao nome da cena instalada.
- Execução física: a cena **UI Authored Controls v2** compila um script real
  que lê `Gui.ForCanvas(HUD).Input`, publica a posição após física/LateUpdate
  e conta os pulsos de salto. Após um gesto de joystick, a captura registrou
  `X 5.97, Y 0.00, Z -1.14`; o vetor voltou a zero ao soltar. Um toque no botão
  registrou `Y 1.12` e `saltos 1`, com o cilindro e sua sombra separados.
  Arrastar LookArea mudou a vista mantendo a posição do jogador.

Capturas preservadas:

- [Inspector no host](../../validacao/ui-universal-2026-10-04/joystick-inspector-host.png).
- [Inspector e hierarquia Android](../../validacao/ui-universal-2026-10-04/controles-inspector-final.png).
- [Posição após movimento](../../validacao/ui-universal-2026-10-04/controles-movimento.png).
- [Salto e leitura pelo script](../../validacao/ui-universal-2026-10-04/controles-salto.png).
- [Câmera após olhar](../../validacao/ui-universal-2026-10-04/controles-olhar.png).
- [Movimento na revisão final instalada](../../validacao/ui-universal-2026-10-04/controles-final-play.png).
- [Retorno à autoria após o movimento](../../validacao/ui-universal-2026-10-04/controles-final-autoria.png).

A revisão posterior de aparência torna fundo/cor do puxador configuráveis e
alinha o clique opcional com o Inspector. A captura final também levou ao
encurtamento dos rótulos numéricos que estavam cortados na lateral Android;
o Inspector foi capturado novamente após build e instalação. O hash do
`base.apk` instalado confere com o APK local. Na revisão final, o joystick
levou o jogador a `X 3.05` e o vetor voltou a zero ao soltar. O
[arquivo de pacote](../../validacao/ui-universal-2026-10-04/controles-pacote.json)
distingue essa revisão das capturas anteriores de movimento. Nenhuma captura
individual de ADB comprova três dedos físicos; esse aceite e o ensaio Android
de todos os modos/imagens permanecem pendentes. Na entrega inicial, recipe e
conversão ainda estavam ausentes; R4.5 abaixo substitui essa situação.
Naquela revisão, DynamicBodyMotor ainda estava ausente; R4.6 abaixo acrescenta
essa cadeia. Pareamento de jogadores, buffer geral para scripts e inspeção
remota continuam pendentes, conforme `R4-CONTROLES-E-RECEPTORES.md`.

## Continuação R4.5: autoria Character

Receita **Personagem cilíndrico** e conversão transacional de MeshRenderer para
raiz Character com o visual original subordinado. Não adiciona tipos de
componentes; o catálogo de criação passa a 78 receitas.

- Host: **68/68** cenários. Os três novos cenários protegem criação com Jolt,
  persistência, IDs e referências, pose rotacionada/refletida e filhos, ordem
  dos irmãos, material, restauração exata dos componentes pelo Undo e recusa
  por dependência/referência/cisalhamento sem mutação.
  [character-host.txt](../../validacao/ui-universal-2026-10-04/character-host.txt).
- SDK: exemplo `GuiAuthoredControls.cs` compilado com o SDK real: **0 avisos,
  0 erros**. Usa o receptor atribuído ao Canvas, sem nome fixo de jogador.
  [character-exemplo-sdk.txt](../../validacao/ui-universal-2026-10-04/character-exemplo-sdk.txt).
  Não foi repetida a suíte SDK de 5 cenários da entrega anterior; ABI permanece 43.
- Android: `assembleDebug`, **BUILD SUCCESSFUL in 57s**, instalação `Success`.
  SHA-256 local e instalado idênticos, registrados em
  [character-pacote.json](../../validacao/ui-universal-2026-10-04/character-pacote.json).
- Aparelho: **UI Character Conversion v3**. Conversão pela UI, Undo/Redo,
  receptor escolhido pelo picker, Save/reabertura e Play. Após joystick:
  **X=3,05**, vetor zero ao soltar. Após botão: **Y=0,82, saltos=1**.
  Criação pela receita e Undo da composição também foram exercitados.

Capturas da revisão instalada:

- [Menu e autoridade anterior](../../validacao/ui-universal-2026-10-04/character-menu.png).
- [Prévia da conversão](../../validacao/ui-universal-2026-10-04/character-previa.png).
- [Raiz convertida](../../validacao/ui-universal-2026-10-04/character-convertido.png).
- [Undo restaura Body/Collider](../../validacao/ui-universal-2026-10-04/character-undo.png).
- [Receptor atribuído](../../validacao/ui-universal-2026-10-04/character-receptor-atribuido.png).
- [Cena reaberta](../../validacao/ui-universal-2026-10-04/character-reaberto.png).
- [Movimento após reabertura](../../validacao/ui-universal-2026-10-04/character-play-movimento.png).
- [Salto após reabertura](../../validacao/ui-universal-2026-10-04/character-play-salto.png).
- [Receita antes de criar](../../validacao/ui-universal-2026-10-04/character-receita.png).
- [Composição e filho visual sem física](../../validacao/ui-universal-2026-10-04/character-receita-visual.png).
- [Autoria final após Undo da receita](../../validacao/ui-universal-2026-10-04/character-autoria-final.png).

Undo conserva a configuração física anterior apenas durante a sessão. Prefab
exige desvinculação; modelos animados não receberam aceite. Cápsula é aproximação
editável, não a forma original dos colliders. Código de script que busca Body
precisa de adaptação explícita. Nenhum desses limites é mascarado por fallback.
Gestos ADB individuais não comprovam multitouch físico nem todos os casos de R4.

## Continuação R4.6: jogador dinâmico com visual separado

`DynamicBodyMotor` conectado ao Body/Jolt por forças limitadas e impulso de
salto, com sondagem de apoio, filtro de camadas e velocidade de plataforma no
ponto de apoio. A receita **Jogador dinâmico** cria Body, Collider cilíndrico,
motor e filho visual sem segunda autoridade. Catálogo: 79 receitas; família
Física 3D: 11 tipos. Não equivale a concluir R4 ou o roadmap integral.

- Host: **71/71** cenários; três cenários novos para composição/Undo/arquivo,
  movimento, frenagem, impulso externo, reconstrução sem perder momento,
  comandos tipados por instância, salto com dois ponteiros e cancelamento,
  plataforma móvel, camada de apoio excluída e autoridade incompatível.
  O cenário de API scoped existente agora percorre Character e DynamicBodyMotor.
  [motor-host.txt](../../validacao/ui-universal-2026-10-04/motor-host.txt).
- SDK: exemplo real com ação IMPULSO e leitura `DynamicMotor().ReadBodyState()`:
  **0 avisos, 0 erros**. [motor-exemplo-sdk.txt](../../validacao/ui-universal-2026-10-04/motor-exemplo-sdk.txt).
  A suíte SDK anterior de cinco cenários não foi repetida. Tabela ABI permanece 43.
- Android: APK compilado e instalado; SHA local/instalado e revisões das capturas
  registrados em [motor-pacote.json](../../validacao/ui-universal-2026-10-04/motor-pacote.json).
- Aparelho: **UI Dynamic Motor v1**. Editor expõe Locomoção/Chão e ícone do atlas;
  picker aceita PlayerDynamic. Velocidade foi alterada de 6 para **8 m/s**,
  Save/reabertura preservaram o valor. Play: **X 0,00 → 3,50** por joystick;
  vetor e velocidade retornaram a zero. Salto: **Y 1,70, saltos 1**. A ação
  IMPULSO executou o comando C# no Body: **X 3,50 → 3,77, impulsos 1**.
  A captura posterior mostra **vx 0,00**, após frenagem; não mede o pico do impulso.

O primeiro aceite Android encontrou leitura scoped de ações ainda restrita a
Character no bridge C#. A validação foi substituída pelo contrato compartilhado
do picker/runtime e o cenário de ABI foi ampliado. As capturas de falha são
diagnóstico da revisão anterior, nunca evidência da revisão corrigida.

Capturas do fluxo:

- [Locomoção após reabrir](../../validacao/ui-universal-2026-10-04/motor-reaberto.png).
- [Propriedades do apoio](../../validacao/ui-universal-2026-10-04/motor-chao.png).
- [Picker do receptor](../../validacao/ui-universal-2026-10-04/motor-receptor-picker.png).
- [Movimento na revisão corrigida](../../validacao/ui-universal-2026-10-04/motor-play-final-movimento.png).
- [Salto](../../validacao/ui-universal-2026-10-04/motor-play-final-salto.png).
- [Impulso pelo script](../../validacao/ui-universal-2026-10-04/motor-play-final-impulso.png).
- [Frenagem posterior](../../validacao/ui-universal-2026-10-04/motor-play-final-frenagem.png).
- [Receita final sem rótulos cortados](../../validacao/ui-universal-2026-10-04/motor-receita-final.png).

A revisão visual mostrou corte no nome/descrição da receita; o nome passou a
**Jogador dinâmico**, com descrição curta e componentes/filho visíveis na prévia.
Não foram acrescentados painéis permanentes. Criação e Undo da composição foram
exercitados no aparelho; os IDs e o Redo são protegidos no cenário host.

Rampas/bordas em todos os formatos, degraus, respawn/reconexão, pareamento de
hardware, Inspector remoto e multitouch de dedos simultâneos no Android continuam
pendentes. Y é o eixo vertical mundial e as dimensões da sonda são autoradas.
O roadmap original permanece idêntico ao arquivo fornecido, verificado por SHA-256.

## Objetos existentes e motor v2 — ampliação universal

Repositório novamente verificado: `https://github.com/kacerato/attachsEngine.git`,
branch local `codex/gameplay-runtime`. Esta revisão ainda não recebeu commit/push.
Plano ampliado: `ROADMAP-OBJETOS-COLISAO-CONTROLE-UNIVERSAL-2026-10-04.md`.

Ações do objeto agora oferece **Configurar locomoção…** sobre a seleção existente.
Preservar mantém o composto; Ajustar mede todos os slots visuais; Convexo usa o
cooking real do Play. Prévia valida a cena física candidata sem executar scripts,
uma transação conserva IDs/visual/filhos e permite Undo/Redo. Colisores dos filhos
podem pertencer ao Body receptor sem existir Collider na raiz. Dados v1 mantêm
suas sondas manuais; v2 salva o apoio automático e o SDK expõe `AutomaticSupport`.

A revisão final reproduziu dois defeitos com apenas cinco sondas: casco inclinado
em dois eixos e composto com partes diagonais não detectavam apoio. O teste foi
de 3/5 para **5/5** após acrescentar o ponto inferior real de cada parte convexa
via função de suporte do Jolt 5.6.0. Consulta usa buffer fixo, sem vetor de hits
alocado por frame. **73/73** cenários da suíte específica de UI passaram depois
dessa correção; exemplo C# compilou com zero avisos e erros.

Android final: **BUILD SUCCESSFUL in 1m 3s**, instalação `Success`, APK de
**268386700 bytes**, SHA-256
`bd0d47ec2166408813c001c38277a757810143f91b3dcc479891b1526a61fc87`.
O hash de `base.apk` instalado corresponde exatamente ao APK local.

No aparelho Xiaomi 25053PC47G, projeto **UI Object Motor v2**: caixa visual
existente com escala 2,7 / 0,4 / 1,1 e yaw 35°, política Convexo aplicada pela
interface, salva e reaberta conservando IDs e recurso visual. Captura da prévia
mostra as escolhas, consequências e Apply; o Inspector mostra suporte automático
e esconde medidas manuais inativas. O viewport continua disponível. O ícone do
motor já integrado ao atlas representa o mesmo conceito nesta ação; não há imagem
conceitual sendo apresentada como implementação.

Na revisão final instalada, joystick levou X de 0,00 a **2,56**, soltura deixou
vetor/vx zero; salto levou Y de 0,20 a **1,94** com contador 1. Impulso externo por
script levou X de 2,56 a **2,83**, contador 1; vx capturado depois da frenagem,
não no pico. O exemplo usa `mass * 5` para a mesma mudança de velocidade em objetos
de massas diferentes. Inclinação em dois eixos, composto diagonal e migração v1
têm prova **host**, não foram declarados aceitos no dispositivo.

Evidência estruturada e hashes das capturas finais:
`docs/validacao/ui-universal-2026-10-04/objetos-motor-v2.json`.
`object-motor-regression-before-fix.log` registra a falha real anterior;
`object-motor-tests.log` e `object-motor-gui-regression.log` registram a correção.
As capturas `object-motor-final-*` pertencem ao APK final; capturas preliminares
pertencem ao APK anterior e estão identificadas no JSON.

Ainda pendentes: decomposição convexa automática/recurso gerado persistente,
autoria de partes e hierarquias com prefab overrides, apoio por contatos/shapecast
em terreno irregular, modos adicionais de locomoção, arbitragem de controle,
integração de animação/root motion e medição de orçamento mobile em cenas grandes.
O apoio por pontos não equivale a um manifold contínuo. O roadmap UI original
continua com 164 âncoras: 140 planejadas, 20 parciais e 4 completas; a ampliação
não infla essa contagem nem declara conclusão universal.

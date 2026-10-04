# UI autorável + Dear ImGui

O documento de jogo e as ferramentas por código usam o renderer e a entrada da
attachsEngine. A UI autorável possui dez tipos: Panel, Text, Button, Toggle,
Slider, Progress, Image, HBox, VBox e Grid. Não é uma reprodução completa de Unity uGUI.

## Usar no editor

1. Abra um projeto e escolha **Cena → Interface (UI + ImGui)**.
2. Use **+ Criar**. Um Panel ou container selecionado recebe o novo elemento como filho.
3. Selecione na árvore ou no canvas. Arraste no canvas; configure nome, texto,
   estado, layout e aparência em Propriedades. Um arraste é uma transação de Undo.
4. **Interagir** executa uma cópia isolada: botão emite Click, toggle/slider emitem
   ValueChanged. Saia do modo para continuar editando.
5. Salve em um caminho `.aeui` do projeto. `UI/main.aeui` é o recurso padrão.
   Salvar/abrir registra o documento ativo em `.astra/gui-resource`; reabrir o
   projeto carrega esse recurso, inclusive quando você escolhe outro nome.
6. Para o exemplo, copie `main.aeui` para `UI/main.aeui` e `GuiMenu.cs` para
   `Scripts/GuiMenu.cs`. Adicione `example.gui.menu` a um objeto, aplique o código
   do projeto e entre em Play. Iniciar muda para Pronto; o toggle controla se o
   slider recebe entrada; o slider atualiza seu próprio texto.

Para containers, imagem e canvas 3D, use `layouts.aeui` no lugar de `main.aeui`
e `GuiExpansion.cs` no lugar de `GuiMenu.cs` (não compile ambos). Copie também
`images/banner.png` para `images/banner.png` do projeto. Adicione uma Camera
na cena, em `(0, 0, -6)`, sem rotação. Em Play, Recompor muda as colunas do Grid;
Mundo 3D posiciona a mesma interface num plano inclinado; Tela restaura o overlay.

HBox/VBox distribuem filhos usando mínimo, preferido e peso de expansão. Grid
deriva suas linhas a partir do número de colunas. O pai controla os retângulos;
**Fora do fluxo** permite anchors e arraste manual. Subir/Descer muda a ordem real.
Image aceita um caminho relativo ao projeto e oferece Stretch, Contain e Cover,
tinta e diagnóstico de recurso. **Canvas** abre destino, resolução e transformação
mundial na mesma área de propriedades. A composição continua em 2D; Play mostra
o plano na cena e usa a câmera ativa para desenho e entrada por raio.

Anchors são normalizadas em relação ao pai. Offsets são **esquerda, topo, direita,
base**, em pixels relativos às respectivas anchors; não são largura e altura.
AEUI 3 preserva IDs, hierarquia, conteúdo, estilo, domínio, sizing, imagem, canvas
e comportamento; o leitor continua aceitando AEUI 1/2. Cancelar um
arraste restaura o estado anterior. Play não grava alterações no documento.

## API C#

`Behavior.Gui` usa o mundo da sessão atual. `Find`/`TryFind` buscam por nome;
`Create` cria nós reais; `Poll` entrega eventos tipados. `GuiElement` permite
ler/editar Text, Name, Value, Visible, Enabled, Layout, Style, Sizing, Image e
ImageStyle, além de Remove e MoveEarlier/MoveLater. `Gui.Canvas` configura o
destino e a transformação. A ponte nativa atual usa ABI 40.
Snapshot contém tipo, domínio e estado. Cores usam `0xAARRGGBB`.

```csharp
var button = Gui.Create(GuiKind.Button, name: "inventory");
button.Text = "Inventario";
button.Layout = new(new(0, 0), new(0, 0), new(24, 24, 224, 72));
while (Gui.Poll(out var message))
    if (message.Element.Id == button.Id && message.Kind == GuiEventKind.Click)
        button.Text = "Aberto";
```

IDs removidos, handles de outro mundo e chamadas fora do Play são recusados.
Filhos herdam clipping, visibilidade e habilitação. Desabilitar um ancestral
cancela a captura de seus controles. Eventos têm fila limitada e diagnóstico
de descarte; não há crescimento ilimitado.

## Ferramentas C++ com Dear ImGui

`GuiWorkbench::addTool` registra uma ferramenta desenhada por um callback no
contexto oficial do ImGui. Veja `imgui_tool.h`. O menu Ferramentas oferece também
diagnóstico real de documento, canvas, atlas e comandos recusados. Registre
ferramentas durante a configuração, antes da chamada de desenho.

O backend consome `ImDrawData`: triângulos indexados, cores por vértice, UV,
clipping e atlas de fontes. Usa Dear ImGui **1.91.9b-docking**, commit
`4806a1924ff6181180bf5e4b8b79ab4394118875`, licença MIT vendorizada.
A fonte usa o mesmo Inter OFL do editor; `tools/bake-gui-font.py` gera o TTF
estático. Não existe um segundo renderer externo ou widget falso.

## Fundo opcional e biblioteca de imagens

Em Aparência, **Desenhar fundo** controla o alpha real salvo no documento.
Desmarcar preserva interação/captura; Pressed não reintroduz um fundo opaco.
O Inspector oferece alpha e hexadecimal no formato `RRGGBBAA`; API/arquivo
continuam usando `0xAARRGGBB`. Reativar o fundo estabelece alpha 255.

Escolher imagem oferece busca e lista com até 4096 candidatos; o popup acompanha
a área disponível com o teclado Android. Esse limite do catálogo não altera
o limite de 64 imagens simultâneas do atlas. A varredura visita até 8192 entradas.

O pacote POLYGON fornecido pelo usuário foi convertido para a biblioteca local
`local-assets/SyntyIconLibrary-20261003`: 520 PNGs transparentes, 520 GLBs,
16 paletas derivadas e 22 páginas editáveis. Os originais permanecem em
`local-assets/synty-polygon-icons-v1.01`. Recursos do pacote ficam fora do Git público.
Veja o [roadmap de expansão](../../docs/planos/UI-ROADMAP-COMPLETO-2026-10-03.md)
e a [validação atual](../../docs/validacao/UI-ROADMAP-SYNTY-2026-10-03.md).

## Limites explícitos

Nesta entrega: um documento/canvas ativo por projeto/Play, destino Tela ou Mundo
3D, anchors/offsets e containers automáticos, até 1024 nós, histórico de 64 transações
e fila de 256 eventos. Imagens PNG/JPEG/KTX2 usam o decoder real: até 64 fontes,
1024² por imagem e 8 MiB por arquivo, em atlas 2048². Caminhos externos e symlinks
são recusados. Texto multilinha, entrada de texto como controle de jogo, animação,
navegação por gamepad, vários canvases e vínculo do plano a um objeto continuam
fora desta entrega. O backend ImGui aceita o atlas interno e
marcadores nativos; texturas de usuário e callbacks externos recusados aparecem
no diagnóstico. A tag docking identifica a fonte oficial; não anuncia docking
ou multi-viewport habilitados nesta integração.

O canvas mundial usa a profundidade da cena. A entrada consulta a geometria
atual; picking por pixel de transparência e deformações não é oferecido.
Veja o [plano e limites da extensão](../../docs/planos/GUI-EXTENSAO-LAYOUT-IMAGEM-MUNDO-2026-10-02.md)
e a [validação da extensão](../../docs/validacao/GUI-EXTENSAO-2026-10-02.md).

Referências de modelo e workflow: [Unity uGUI 2.0](https://docs.unity3d.com/Packages/com.unity.ugui@2.0/manual/UIBasicLayout.html),
[Godot Control 4.5](https://docs.godotengine.org/en/4.5/classes/class_control.html),
[Dear ImGui oficial](https://github.com/ocornut/imgui/tree/4806a1924ff6181180bf5e4b8b79ab4394118875).
# Imagem interativa e animada

Use `behavior.aeui` como `UI/main.aeui`, copie `images/banner.png` para o projeto
e adicione `GuiImageActions.cs` a um objeto da cena. O exemplo contém três
Images: uma abre/fecha um painel, outra anima sua própria pose e a terceira
emite clique para uma reação C#. Os três fundos são transparentes.
O Panel `canvas_backdrop` dá contraste ao exemplo sobre uma cena vazia; seu
fundo é autorado e opcional. Pode ser removido ou ter alpha zero no Inspector.

No Inspector de qualquer Image, **Interação → Clicável** habilita entrada.
Escolha ação e alvo; `Este elemento` usa o próprio ID. `Evento para script`
emite `GuiEventKind.Click` para o comportamento que consome `Gui.Poll()`.
Não existe script de gameplay executando no modo Interagir; ele testa as ações
nativas e mostra eventos. Entre em Play para executar o C#.

**Animação → Animar elemento** habilita a transição de deslocamento XY, escala
uniforme e opacidade. Início/Fim são relativos à pose de autoria; filhos herdam
a transformação. Configure duração, atraso, curva, repetição e ida/volta.
Pulsar/Aparecer preenchem propriedades; não são efeitos escondidos. Preview e
Play usam cópias. Stop restaura a pose, enquanto conclusão mantém o resultado.
Uma mudança em parâmetros de animação durante Play reinicia a configuração:
autoplay toca, caso contrário fica na autoria até `PlayAnimation()`.

```csharp
var image = Gui.Find("animate_image");
image.Animation = GuiAnimation.Default with {
    Enabled = true, Duration = .5f,
    To = new GuiPose(new System.Numerics.Vector2(40, 0), 1.2f, .5f)
};
image.OnClick(GuiClickAction.PlayAnimation);
// Ou image.OnClick(GuiClickAction.ToggleVisible, Gui.Find("details_panel"));
image.PlayAnimation();
image.StopAnimation();
```

O arquivo agora salva AEUI 4 e lê versões 1/2/3. Projetos antigos não recebem
interação/animação automaticamente. ABI nativa e SDK C# são v41 e devem ser
distribuídos juntos; bibliotecas geradas por SDK antigo precisam ser recompiladas.
Remover um alvo mantém a referência identificável e gera diagnóstico ao tentar
executar; não escolhe outro alvo silenciosamente. Consulte `Gui.Diagnostic`.

`states-actions.aeui` + `GuiStatesActions.cs` demonstram ações em sequência e
estados visuais. Gere o projeto com `aether_gui_preview write-project NOVA_PASTA states`.
Selecione a imagem: **Sequência de ações** permite adicionar, escolher evento,
editar alvo/valor, subir/descer e remover. **Estados visuais** edita um estado
por vez. Interagir testa a configuração em uma cópia; Play executa também C#.

```csharp
var image = Gui.Find("open_image");
image.OnClick(GuiClickAction.ToggleVisible, Gui.Find("details_panel"));
image.AddAction(GuiEventKind.Click, GuiClickAction.PlayAnimation, Gui.Find("details_panel"));
image.Transitions = GuiTransitions.Default with {
    Enabled = true,
    Pressed = new GuiStateStyle(new GuiPose(System.Numerics.Vector2.Zero, .8f, .5f), 0xFF70CFFF)
};
// image.Actions devolve uma cópia ordenada das ações adicionais.
// ReplaceAction(index, binding), MoveAction(index, destination), RemoveAction(index).
```

Até 16 ações adicionais por elemento, filtradas por Click/ValueChanged; a ação
principal de `OnClick` executa primeiro. `AddAction` não ativa Clicável por si:
configure `OnClick`/`Interaction` para imagem/texto/painel. `Notify` deixa o
evento disponível para `Gui.Poll`, sem produzir notificações duplicadas. Ações
SetValue emitem ValueChanged quando há alteração; escrita de `Value` pela API
continua silenciosa. Ciclos são limitados a 256 eventos por despacho e reportados
em `Gui.Diagnostic`; alvo inválido é reportado e a sequência continua.

Normal/Pressed/Disabled possuem pose e tinta; a duração pode ser zero. Tinta
multiplica as cores locais, sem tornar fundo transparente opaco. Pose/alpha
afetam filhos; desabilitar o pai bloqueia entrada dos filhos imediatamente.
Mudar de estado parte do valor interpolado corrente. A área capturada fica
estável até soltar/cancelar; opacidade zero bloqueia novos hits, mas preserva
um gesto já capturado. Hover/foco/seleção não estão implementados.

Limites: um tween de autoria composto com uma transição de estado, hit retangular,
uma captura de ponteiro, quatro curvas fixas e três estados. Timeline, keyframes,
sprite swap, temas completos, máscaras de clique e callbacks de término continuam
planejados. Estados e ações não completam todas as famílias do roadmap.

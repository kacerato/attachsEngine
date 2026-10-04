# attachsEngine — Image com interação e animação

Data: 2026-10-03. Checkout `C:\Users\donod\Downloads\atchengine`, origin
`https://github.com/kacerato/attachsEngine.git`, branch `codex/gameplay-runtime`.
Alterações locais desta continuação; este relatório não afirma publicação no Git.

## Capacidade entregue e limite

Image mantém seu tipo, recurso, tinta, ajuste, fundo e layout. Interação opcional
emite Click e executa ação sobre ID persistente: notificar script, alternar
visibilidade/habilitação, definir valor, iniciar/parar animação. Text, Panel e
containers também podem optar por clique; Button/Toggle/Slider mantêm entrada
própria. Nenhum fundo é acrescentado para tornar a imagem clicável.

Animação autorada interpola duas poses: deslocamento XY, escala uniforme e alpha.
Duração, atraso inicial, quatro curvas, autoplay, loop e ida/volta têm efeito.
Transformação, opacidade e clipping se propagam aos filhos; hit acompanha o
resultado e opacity zero não captura. O layout medido é reutilizado nos frames
de animação; não altera offsets, revisão do documento ou atlas de recursos.
Stop restaura a pose de autoria; conclusão sem loop conserva o resultado.
Configuração alterada reinicia o estado e respeita autoplay. Play segue o relógio
escalado do mundo e sua pausa; Interagir usa o delta real do editor.

Ownership: a cópia de GuiRuntime possui clocks e estados por ID. Reload descarta
estados/eventos anteriores. Remoção libera estado de animação e filtra eventos
de fontes removidas. Duplicação remapeia alvos internos à subárvore. Alvo externo
continua referenciado; alvo removido/incompatível gera diagnóstico, disponível
no preview e em `Gui.Diagnostic`. Overflow de transformação é diagnosticado e
suprime geometria inválida.

Editor: Image → Interação → Clicável → ação/alvo; Animação → duas poses,
tempo/curva/reprodução. Presets preenchendo os mesmos dados. Preview isolado e
Undo/Redo usam o documento real. AEUI 3 grava novos campos e lê AEUI 1/2 com
defaults sem ações/animação nas imagens antigas. SDK/native ABI 40 acrescenta
`guiBehavior`, struct de 76 bytes e comandos Play/Stop; os dois devem ser
distribuídos juntos. C# expõe propriedades tipadas e `OnClick` com verificação
de identidade do alvo.

**Não é conclusão das famílias E/J/L ou do roadmap.** Uma ação nativa por
clique, uma transição de pose por elemento, hit retangular e uma captura de
ponteiro são os limites atuais. Ainda faltam múltiplas ações/estados, hit mask,
foco/acessibilidade, multipointer, timeline/keyframes, rotação/escala por eixo,
sprite animation, callbacks de término, pause/resume da transição e políticas
de reduced motion. Joystick/player e temas/skins completos continuam planejados.

## Referência e adaptação

[Godot 4.5 TextureButton](https://docs.godotengine.org/en/4.5/classes/class_texturebutton.html)
e [source 4.5-stable](https://github.com/godotengine/godot/blob/4.5-stable/scene/gui/texture_button.cpp)
mostram textura e resposta a input, incluindo mask e ajuste da posição durante
hit testing. Aqui imagem e interação são dados compostos no mesmo node; mask
continua pendente. [Godot 4.5 Tween](https://docs.godotengine.org/en/4.5/classes/class_tween.html)
e [source](https://github.com/godotengine/godot/blob/4.5-stable/scene/animation/tween.cpp)
orientam interpolation, ownership e controles de reprodução sem escrever a
autoria. [Unity uGUI 2.0 Selectable transitions](https://docs.unity3d.com/Packages/com.unity.ugui@2.0/manual/script-SelectableTransition.html)
orienta associar interação a resultado visual. Não se declara paridade com esses
sistemas nem reprodução de suas ferramentas/timelines.

## Evidência no host

Diretório de evidência ignorado: `build/ui-image-behavior-20261003/`.

- Build dos alvos `aether_gui_tests` e `aether_gui_preview`.
- `native-tests-final.log`: **48/48 cenários passaram**. Carga de 1024 nodes com
  1024 animações: **251,815 µs/frame (0,252 ms)** para update/layout de CPU no
  host, build Debug, média de 120 frames após oito de aquecimento. Não inclui
  desenho/GPU, upload nem custo total do frame; não é uma medição no Android.
- Cenários nativos cobrem ação de Image, eventos, alpha, herança, captura,
  mudança de resolução, ida/volta com atraso, relógio independente de frames,
  Stop, remoção, alvo ausente, referências duplicadas, roundtrip e migração.
  ABI é exercitada no EditorPlayScene real, com alterações e comandos de Image.
- Quatro testes C# direcionados passaram: routing, layout/style/recursos,
  structs/offsets ABI, composição e rejeição de alvo estrangeiro.
  `managed-example.log`: GuiImageActions compilou com zero avisos/erros.
- `author-final.png` é captura do renderer/software raster real de EditorSession,
  não imagem conceitual. 13 nodes e recursos reais do projeto privado.
- Helper criou também `public-example-project` com cena, script, AEUI e imagem;
  `public-example-resources.log`: imagem decodificada pelo atlas nativo. Não
  depende da biblioteca POLYGON para funcionar.

## Evidência no Android

Xiaomi `25053PC47G`, transporte ADB 2, screenshot físico 2772×1280. APK debug
compilado e instalado. Projeto `UIImagemComportamento-20261003` em
`/sdcard/Android/data/dev.aether.editor/files/Projetos/`.
APK final SHA-256: `67efbc16f1c557de36e5b6d57d27c46a59a4c3bee78d0294c0d8a9c5a14190c4`.
`lastUpdateTime` no aparelho: 2026-10-03 17:53:06. `manifest.json` registra hashes
das capturas, arquivos salvos e logs, além dos resultados e seus limites.

1. Abrir Interface e selecionar Image: Inspector mostra recurso e comportamento
   no mesmo elemento (`device-author-fold.png`).
2. Interagir: clicar Add abre painel e emite evento (`device-preview-click.png`).
3. Clicar Play: deslocamento, escala e alpha são visíveis em frame posterior
   (`device-preview-motion-mid.png`). Preview não executa C#.
4. Desativar Clicável, Undo, Redo e salvar. Arquivo puxado do aparelho contém
   clickable=0, ação ToggleVisible e alvo=9 (`device-saved-disabled.aeui`).
   Encerrar app e reabrir mantém a opção desativada (`device-reopened-disabled.png`).
5. Reativar e salvar. Selecionar animação, alterar Repetir e salvar: arquivo
   contém loop=1 (`device-saved-loop.aeui`). Pre-visualizar pelo próprio Inspector
   produz movimento (`device-editor-animation-preview.png`). Sair, Undo e salvar
   restaura loop=0 e conserva clickable=1 (`device-saved-final.aeui`).
6. Play: eventos de duas Images chegam ao C#; texto mostra `2 cliques |
   notify_image | Image`, e o script altera duração e toca animação. Captura
   inicial revelou contraste ruim sobre cena vazia; o exemplo recebeu Panel
   backdrop autorado/opcional. Images continuam com background=0.
   Revisão final instalada: `device-play-events-final.png`; Inspector final
   `device-author-final.png` apresenta Ao clicar/Alvo sem nomenclatura de API.

O arquivo final salvo no aparelho foi copiado para o projeto local, preservando
os dados efetivamente editados. O exemplo público `examples/ui/behavior.aeui`
usa `images/banner.png` já existente. O projeto privado usa três renders RGBA
POLYGON locais, mantendo sua exclusão do Git público. Recursos/licença originais
permanecem na biblioteca entregue anteriormente.

## Design e reversão

NÃO IREI SER SIMPLISTA NO DESIGN.

Capturas mostram canvas dominante, grupos contextuais recolhíveis, campos de
pose legíveis e Interagir ocupando a superfície útil. Não acrescentamos painéis
permanentes. O fundo do exemplo é um elemento autorado removível. A captura
gerada é executável; não houve proposta de imagem apresentada como implementação.
Reutiliza ícone de Image existente: não cria um novo tipo de componente ou
ícone falso. Histórico preserva reversão dos parâmetros. Screenshots representam
landscape; portrait e IME nesta nova seção não foram ensaiados nesta continuação.

Para reproduzir: gere projeto com `aether_gui_preview write-project <novo-dir>
behavior`, abra
o projeto e use Interface/Interagir/Play. O helper usa serializer de cena e
ScriptBehavior reais. É necessário SDK e native da mesma revisão.

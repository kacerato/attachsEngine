# P15a — autoria de curvas e percurso

NÃO IREI SER SIMPLISTA NO DESIGN.

O Inspector genérico não deve expandir nove números por ponto em uma curva de 128 pontos. A superfície Pontos substitui o conteúdo contextual do Inspector, mantém suas abas e o viewport e seleciona um ponto por identidade persistente. Uma lista paginada permite salto direto; navegação anterior/próximo, inserir, apagar e mover na ordem operam sobre a coleção real. Posição e Tangentes alternam a superfície em vez de empilhar grupos.

## Referências observadas

- [Godot 4.5 Path3D](https://docs.godotengine.org/en/4.5/classes/class_path3d.html): objeto da cena com curva própria. Astra adapta a curva inline persistente, sem afirmar recurso GUID externo.
- [Godot 4.5 Curve3D](https://docs.godotengine.org/en/4.5/classes/class_curve3d.html): pontos locais, entrada/saída relativas ao ponto e cache de amostras. A UI explicita esses espaços; gizmo transforma os dados locais usando a matriz do objeto.
- [Godot 4.5 PathFollow3D](https://docs.godotengine.org/en/4.5/classes/class_pathfollow3d.html): progresso, repetição, orientação e deslocamentos. Astra usa referência explícita ao Path, distância mundial e consumidor ScenePaths; não exige parentesco hierárquico igual à Godot.
- [PR oficial Godot #104058](https://github.com/godotengine/godot/pull/104058) e [proposta #11570](https://github.com/godotengine/godot-proposals/issues/11570): problema concreto de tangentes inicialmente orientadas para dentro. Astra distingue Entrada/Saída, marca o ponto ativo e mostra suas duas alças; não adiciona modos automáticos sem consumidor.
- [Vídeo Crigz — Path Based Mesh Generation in Godot 4](https://www.youtube.com/watch?v=Gfpnxg-jne4&t=60s), descrição Godot 4.0 Beta3. Vídeo efetivamente observado em 1:00, com pontos/alças no viewport superior ortográfico; transcrição observada em 0:21–1:11: seleção revela toolbar, adicionar/inserir/apagar, fechar curva e arrastar tangentes. Mesh extrusion apresentada depois não faz parte deste pacote.

## Fluxo e estados

Componentes → Path → Editar pontos → escolher identidade → alterar XYZ local → Tangentes → alterar Entrada/Saída → voltar. O cabeçalho apresenta posição na coleção e ID persistente. A lista não usa o número visual como identidade de edição. O teclado guarda ID e versão do documento para impedir que reorder/redirecionamento edite outro ponto.

Coleção vazia permanece autorável. Inserir desativa em 128; apagar e navegar desativam sem seleção; mover desativa nos extremos. Em Play, autoria de Path fica indisponível e nenhum hit numérico/estrutural é publicado. PathFollow apresenta dados autorais e diagnóstico real separado, com reiniciar/parar apenas no Play.

PathFollow distribui os campos reais em Percurso (referência, modo e distância/velocidade/duração), Orientação (orientar e offsets) e Execução (ativo, início, repetição e sentido). O modo condiciona Velocidade/Duração; não deixa ambos simultaneamente sugerindo dois consumidores. Os diagnósticos distinguem alvo ausente, curva inválida, ciclo, autoridade, escrita concorrente e orientação degenerada.

O refinamento de densidade usa primitives reais: Distância inicial + Velocidade/Duração em par com unidades; offsets XYZ numa linha; Auto + Repetir em par. Cada controle mantém seu setter, campo persistido, mixed state e Reset individual. Os eixos do Frame são X lateral, Y vertical e Z tangente, em unidades mundiais; não são offsets XYZ globais. O teclado conserva os títulos completos dos campos originais. Reiniciar/parar fica em Execução no Play, com o status verdadeiro separado.

Quatro ícones vetoriais próprios — path/curve, component/path-follow, path/point e path/tangent — integram o pipeline gerador/SVG/PNG/atlas: 221 entradas após esta rodada. O provider usa cache limitado a uma curva selecionada, invalidado por dados reais, até 512 segmentos, até 384 segmentos de pontos e 11 do ponto/alças ativos. Sem bake repetido a cada quadro para uma curva inalterada.

Skill aplicada: [avoid-ai-design](C:/Users/donod/.agents/skills/avoid-ai-design/SKILL.md), dentro do sistema Astra existente. Seu scanner HTML/CSS não valida o renderer C++.

## Limites e validação

Curva Bézier inline, até 128 pontos, sem tilt, extrusão ou recurso externo. Gizmos são visualização; alças não prometem drag direto no viewport. Edição numérica/CRUD segue sessão, histórico e persistência reais.

A identidade também vale para prefabs: mudança de IDs, ordem ou allocator torna a coleção um override estrutural com Apply seletivo recusado explicitamente; Revert completo continua disponível. SlotNumber só pode Apply quando as estruturas correspondem. Assim o slot visual não aplica um valor ao ponto errado após reorder.

Primeira inspeção executável 853×394: Posição e Tangentes cabem, incluindo Entrada/Saída XYZ e nota. PathFollow em Play deixava os campos sem espaço: corrigido após observação, não presumido pelo contador. Nota compacta com busca contextual substitui uma linha de busca inteira; ações reais de reiniciar/parar estão em Execução, e Percurso prioriza referência ao Path. Enabled usa o checkbox real do cabeçalho, sem linha duplicada. A seleção do ponto agora indica abertura de lista com ícone/chevron. A revisão posterior foi recapturada e julgada.

O provider também detalha a curva-alvo ao selecionar PathFollow, além do link ao Path. A câmera da fixture enquadra os dados reais para não deixar os gizmos fora do viewport. O contador de recortes do renderer sozinho não demonstrou usabilidade: a primeira tela sem campos também reportava zero recortes; o julgamento visual detectou a falha.

Fixtures reais: path-summary, path-points, path-tangents, path-list, path-empty, path-limit, path-follow-edit, path-follow-play, path-follow-play-execution, path-follow-play-orientation e path-follow-missing. As variantes Play usam ScenePaths no GameWorld, sem status simulado. Não foi executado ADB.

## Evidência final executável

22 capturas finais: 16 em 853×394, quatro em 1200×700 e duas em 394×853. As duas imagens `*-before-wave4.png` são comparação anterior e não entram nessa contagem. Zero glifos ausentes e zero instâncias descartadas em todas. Paisagem: zero recortes; retrato: seis recortes por tela na composição global existente.

| Estado | Julgamento e evidência |
|---|---|
| Ponto/tangentes | Todos os campos XYZ e notas cabem em telefone; ponto ativo/alças distinguem-se no viewport. [Tangentes telefone](../validacao/evidencias/inspector-components-20260930/path-tangents-853-wave4.png), [tela maior](../validacao/evidencias/inspector-components-20260930/path-tangents-1200-wave4.png). |
| Coleção | ID ativo, seleção direta e paginação são legíveis; 128 pontos não expandem o Inspector. [Lista](../validacao/evidencias/inspector-components-20260930/path-list-853-wave4.png), [128 pontos](../validacao/evidencias/inspector-components-20260930/path-limit-853-wave4.png), [vazio](../validacao/evidencias/inspector-components-20260930/path-empty-853-wave4.png). A captura de128 usa a lista; não é prova visual do botão Inserir desativado no formulário. |
| Follow Percurso | Edit tem duas páginas; Play tem três: alvo, modo, par de avanço. [Alvo](../validacao/evidencias/inspector-components-20260930/path-follow-play-853-wave4.png), [modo](../validacao/evidencias/inspector-components-20260930/path-follow-play-page2-853-wave4.png), [par](../validacao/evidencias/inspector-components-20260930/path-follow-play-page3-853-wave4.png). |
| Follow Orientação | Duas páginas em Play: Orientar +Z e offsets XYZ juntos, com unidade `u`. [Frame](../validacao/evidencias/inspector-components-20260930/path-follow-play-orientation-page2-853-wave4.png). |
| Follow Execução | Duas páginas em Play: comandos reais e Auto/Repetir, depois Sentido inverso. [Comandos](../validacao/evidencias/inspector-components-20260930/path-follow-play-execution-853-wave4.png), [sentido](../validacao/evidencias/inspector-components-20260930/path-follow-play-execution-page2-853-wave4.png). |
| Alvo ausente | Diagnóstico e campo obrigatório coexistem; nenhuma curva-alvo inventada. [Ausência](../validacao/evidencias/inspector-components-20260930/path-follow-missing-853-wave4.png). |
| Retrato | Campos permanecem legíveis, mas toolbar da cena sobrepõe o cabeçalho e viewport tem180px. Compatibilidade completa em retrato não foi demonstrada. [Path](../validacao/evidencias/inspector-components-20260930/path-points-394portrait-wave4.png), [Follow](../validacao/evidencias/inspector-components-20260930/path-follow-edit-394portrait-wave4.png). |

O [conceito gerado](../validacao/evidencias/inspector-components-20260930/concept-path-follow-wave4.png) e seu [prompt exato](../validacao/evidencias/inspector-components-20260930/concept-path-follow-wave4.prompt.txt), pela skill imagegen, são hipótese separada da evidência. A imagem ilustra curva junto de um diagnóstico de alvo ausente; essa associação não foi levada ao produto. O provider real exige referência válida. O conceito orientou a hierarquia entre estado e campos, sem prometer extrusão, viewport Play da fixture ou controles extras.

Preview final SHA256: `51579E7E0D7F6373300EC93409B39077A1C85BAE840CF56C2CF3B8886082F845`. Root relatou builds host e APK final aprovados; não houve validação física/ADB. Testes de comportamento são coordenados no relatório de auditoria central, separadamente das capturas deste documento.

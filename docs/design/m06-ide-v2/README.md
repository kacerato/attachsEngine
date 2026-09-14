# Proposta visual M06 + prioridade M11 — 12/09/2026

Estado: **revisão aplicada aprovada pelo usuário: “ok isso foi bem aplicado”. A primeira implementação havia sido rejeitada por falta de fidelidade; a revisão de composição, transparência e renderização foi implementada e instalada. Capturas em [comparativo.html](comparativo.html); decisões em [FIDELIDADE.md](FIDELIDADE.md). A aprovação visual não fecha os gates funcionais do plano.**

A continuação [funcional de M06](../../planos/M06-PACOTE-FUNCIONAL.md) preserva
essa direção. Busca inline e popups de símbolos usam a mesma família de
superfícies, cores e recortes; não acrescentam SVGs ou uma nova proposta visual.
Gerada com a ferramenta integrada image_gen, usando logo real e referências de layout enviadas pelo usuário.
A aprovação exclui a faixa de horário/sinal/bateria do mockup. A IDE mantém o modo imersivo Android. ADB autorizado; evidências de implementação registradas na validação desta entrega.

## Pedido atual

Priorizar a construção visual da IDE, com identidade derivada da logo, ícones raster sólidos e console funcional mais elaborado. Antecipar IDE em retrato e teclado em retrato, conservando a estrutura da cena. O usuário sugeriu avaliar uma imagem antes de implementar o desenho. Esta proposta não altera a preferência de implementação em blocos completos.

## Artefatos

- [IDE: código, arquivos e console](proposta-ide.png)
- [Família de 12 ícones](proposta-icones.png)
- [Prompts usados](prompts.md)

As imagens são estudos raster, não screenshots da Astra. Código, localização, horários, contadores e mensagens desenhados não são evidência de runtime. A geração ainda apresenta pequenas inconsistências entre números de linha do trecho superior e detalhe, e entre o ponto e vírgula desenhado e a mensagem de erro. A implementação deve derivar ambos do mesmo diagnóstico real. Algumas silhuetas pequenas na prancha da interface diferem da família de ícones: os assets individuais precisam ser produzidos a partir da família escolhida e avaliados em tamanho real.

## Construção visual proposta

Superfícies carvão em três níveis; branco quente para texto; lima sólido para ação principal e seleção; coral/âmbar por severidade. A estrela de quatro pontas, os recortes e a órbita definem proporções e detalhes de ícones, sem repetir a logo em todo botão. Gutter, tab, breadcrumb e divisores possuem alinhamento comum. Cabeçalho compacto, densidade adequada à autoria e ação primária diferenciada das ferramentas secundárias.

A prancha não impõe cores ao teclado do usuário: ele pertence ao IME Android. A proposta de teclado retrato exige orientação real da janela, não desenhar/girar um teclado falso.

## Retrato: contrato de implementação

O plano mestre já cobre isso em §6.4–6.5 e M11. A solicitação atual prioriza este trabalho; os requisitos de integridade continuam obrigatórios.

1. Política de orientação por workspace: retrato no código, restaurar orientação anterior ao voltar à cena. Considerar escolha seguir dispositivo/manter paisagem.
2. Preservar EditorSession/Document/History, seleção de objeto, câmera editorial, buffers, revisões, caret/seleção/scroll e build em curso; não mudar aspect autorado da câmera de jogo.
3. Coordenar Activity, funções nativas que forçam paisagem, recriação/resize Vulkan e painel Android anexado. Trocar apenas screenOrientation do manifest não resolve a cadeia.
4. Recalcular tabs, drawer, console, limites do campo e IME com a geometria real. Retrato não terá árvore lateral permanente.
5. Validar teclado aberto, rotação ida/volta, menu, seletor externo e pausa/retomada no aparelho autorizado. Não declarar recuperação após morte do processo sem prova.

## Console proposto

- Abas Problemas / Registros: diagnósticos ativos separados do histórico de eventos.
- Busca, severidade e origem; contagem explica o filtro selecionado.
- Lista virtualizada com ícone, mensagem, localização e contexto de build/Play quando disponível.
- Detalhe expandido com mensagem integral, snippet/pilha disponível e ações abrir fonte/objeto, copiar e menu de exportação.
- Agrupar repetições, seguir saída, manter posição ao investigar um evento.
- Limpar registros preserva erros ativos.
- Arrastar para ampliar/recolher; modo de foco para console. Com teclado, a IDE preserva área de edição; erros não roubam foco a cada tecla.
- Dados ausentes aparecem como indisponíveis. Copiar/exportar não inclui arquivos privados do projeto automaticamente.

Nem todas essas capacidades existem hoje. Filtros básicos/limites/salto e logs de compilação já existem; busca/origem/detalhe/cópia/exportação e ancoragem por EventId exigem implementação e integração. Não acrescentar botões sem consumidor funcional.

## Referências verificadas

- [Plano completo](../../planos/PLANO-MESTRE-ASTRA-EDITOR-RUNTIME-ASSETS.md), §6.4–6.5, §7, M06 e M11.
- [Unity Console](https://docs.unity3d.com/6000.0/Documentation/Manual/Console.html): toolbar, busca, lista, detalhe e navegação; limpar conserva erros de compilador.
- [Imagem do console Unity](https://docs.unity3d.com/6000.0/Documentation/uploads/Main/Console.png).
- [Godot Debugger panel](https://docs.godotengine.org/en/stable/tutorials/scripting/debug/debugger_panel.html): referência complementar de organização de diagnóstico.
- Duas imagens de IDE mobile enviadas pelo usuário: organização de editor e drawer; sem presumir TSX, Git, linguagens ou APIs na Astra.


## Aplicação aprovada

- Paleta dedicada ao workspace de código: carvão, branco quente e lima; cena conserva seus tokens.
- Cabeçalho de marca e ações, breadcrumb com retorno à cena, tabs com indicação de alteração, drawer com árvore e ações de criação.
- 12 células raster geradas e catalogadas em `ide/*`; atlas e enum são produzidos juntos. Os acessórios Android consomem os mesmos rasters de cópia/desfazer. A geração de produção não entregou alpha real; a versão adotada usa fundo carvão sólido em vez de checkerboard. Não é SVG.
- Orientação real da Activity por workspace, escala de toque ampliada em retrato, retorno à orientação anterior. Nenhuma alteração na projeção/grade da cena.
- Problemas ativos e registros separados, filtros de texto/severidade/origem, seleção por EventId, detalhe paginado integral, abrir origem, copiar pelo clipboard Android, exportar recorte em `Logs/console-<timestamp>.txt`, seguir saída e ampliar/reduzir.
- Eventos exibem tempo relativo à sessão. Origem, arquivo/linha/coluna e objeto só aparecem quando existem. Repetições consecutivas continuam agrupadas na ingestão; não há botão fingindo desagrupar eventos já agregados.
- O detalhe mostra a mensagem/pilha recebida. Snippet do arquivo ao lado do erro, geração/Play estruturados para todos os produtores, busca global de arquivos e redimensionamento por arraste continuam pendentes; não são dados simulados.
- Correção do roteador: ações repetidas em mais de uma área (ex.: abrir arquivos no cabeçalho e no estado vazio) aceitam o toque na área efetivamente atingida; um bloco acima impede acionamento de controles cobertos.

Referência Android consultada: [configuração sem recriar Activity](https://developer.android.com/topic/architecture/views/resources/runtime-changes-views). O manifest já declara orientation/screenSize/smallestScreenSize/screenLayout. Isso não é garantia de recuperação após morte do processo nem de comportamento idêntico em tablets/foldables.

Evidências e limites da entrega: [validação IDE v2](../../validacao/2026-09-12-ide-v2.md).

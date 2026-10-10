# N01 — intervalos, grupos de cues e ferramentas de pose

NÃO IREI SER SIMPLISTA NO DESIGN.

## Problema e direção

A timeline já edita chaves, curvas e poses. Eventos e marcadores precisam de
seleção de grupo, clipboard e transformações temporais coerentes com as chaves.
Colocar mais campos permanentes ao redor do viewport agravaria a altura útil
em celulares landscape. A extensão usa a superfície existente de edição:
`Editar → Intervalo`, com começo, fim, destino e operações agrupadas. Fechar
essa superfície devolve a região à cena e à timeline.

A proposta estrutural concentra tarefas temporárias em uma folha contextual
sobre a timeline, em vez de manter um Inspector temporal permanentemente aberto.
O modo de seleção já existente agora trabalha também nas duas linhas de cues;
selecionar por caixa ou usar Selecionar todos permite mover, escalar, copiar,
recortar, colar e excluir o grupo. Um gesto publica uma única alteração no
journal. Cancelar descarta o rascunho inteiro.

## Referências concretas

- Unity 6, 6000.0: [seleção e manipulação de chaves](https://docs.unity3d.com/6000.0/Documentation/Manual/animeditor-AdvancedKeySelectionAndManipulation.html).
  Princípio: manipular a seleção preservando relações temporais. Adaptação:
  controles explícitos de seleção/adição e transformação numérica para toque.
- Godot 4.5: [Animation](https://docs.godotengine.org/en/4.5/classes/class_animation.html)
  e [tipos de trilha](https://docs.godotengine.org/en/4.5/tutorials/animation/animation_track_types.html).
  Princípio: separar chaves de dados e chamadas/eventos; prévia do editor não
  dispara código de gameplay. A Astra mantém o preview isolado existente.
- Godot 4.5: [editor de trilhas, código oficial](https://github.com/godotengine/godot/blob/4.5/editor/animation/animation_track_editor.cpp).
  Princípio: alteração de seleção e publicação temporal pertencem ao histórico,
  não à geometria dos widgets. A Astra usa seus IDs persistentes e journal.

## Proposta visual e implementação

`intervalos-conceito.png` é uma hipótese gerada a partir da captura real anterior
`docs/validacao/animation-cues-android-2026-10-10/selected.png`. Não é captura da
implementação. A região de materiais/cena desenhada na imagem não é nova feature.
Texto completo da geração em `prompt.txt`.

Os controles implementados chamam `animation_clip_timeline`, usado igualmente
pela API C# ABI 7. Os ícones Intervalo e Grupo de cues foram desenhados com as
primitivas angulares do acervo e integrados ao atlas real, por
`tools/generate-animation-timeline-icons.py` e `tools/pack-icon-atlas.py`.
Capturas host de intervalos e seleção de cues estão no relatório de validação.

## Gravação e reutilização de pose

NÃO IREI SER SIMPLISTA NO DESIGN.

Ações raras de pose compartilham uma folha contextual de duas tarefas:
Gravação e Reutilizar. A primeira reúne Auto-key, política de criação, intervalo
armado e travas locais; a segunda torna explícito o espaço copiado e oferece
reset para identidade ou referência da cena. Fechar a folha devolve espaço à
prévia. O ícone `animation/pose-tools` usa o mesmo atlas de produção.

`poses-conceito.png` é hipótese visual gerada a partir de
`host/capturas/clip-pose-pending-small.png`; o braço ilustrado não prova render.
O prompt está em `poses-prompt.txt`. Capturas posteriores de controles reais
recebem nomes `pose-tools-record-*` e `pose-tools-reuse-*` no relatório.

Referência: [Unity 6, gravação no Animation Window](https://docs.unity3d.com/6000.0/Documentation/Manual/animeditor-AnimatingAGameObject.html).
Princípio extraído: diferenciar gravação de navegação e de alteração temporária.
Na Astra, o sampler/journal existente recebe o rascunho; a folha não possui
outro modelo de animação. O SDK ABI 8 anexa três funções sem mudar AECLIP 4,
mantendo os tamanhos dos prefixos ABI 1–7.

- Intervalo armado inclui as fronteiras. Auto-key fora dele mantém rascunho e
  explica a recusa; Gravar continua sendo uma publicação explícita.
- Travas usam XYZ local, inclusive para quaternion/progressivo. Euler mantém
  seus valores e voltas; morph usa bits por índice e paginação a cada três.
- Referência da cena lê o transform/pesos autorais, não afirma ser bind pose.
  Reset respeita travas e a mesma política de Auto-key.
- Cópia local guarda um canal e sua representação. Cópia mundial guarda TRS
  avaliado na Base. A colagem resolve o pai do destino, recusa matriz singular
  ou shear e cria os três canais apenas quando a política permite.
- Clipboard pertence à sessão e não guarda ponteiros de objeto. Falhas deixam
  o recurso e o journal intactos; trocar projeto limpa a cópia.

## Semântica e estados

- Mover trecho preserva a duração e desloca chaves, cues e referências juntas.
  O intervalo é `[início, fim)`, incluindo o último ponto apenas no fim do clipe.
  Cruzar/colidir com chaves não selecionadas ou sair dos limites recusa tudo.
- Inserir tempo desloca todas as chaves a partir do destino, cues e referências
  aditivas. Segmentos que cruzam a fronteira ficam mais longos; não é hold/bake.
- Remover usa `[início, fim)`. O fim sobrevive no início. Referência aditiva
  dentro do corte ou canal que ficaria vazio recusa a operação atomicamente.
- Duplicar copia curvas recortadas com amostras das fronteiras, todas as camadas
  e cues no intervalo; insere duração mais um quadro de exibição para preservar
  as duas poses na emenda. Referência aditiva continua pertencendo à camada.
- O clipboard de cues preserva metadados e double do payload, aloca IDs novos e
  mantém intervalos relativos. Colagem fora da duração recusa tudo.
- Campos inválidos, revisão/contexto alterados e capacidade excedida geram
  diagnóstico. O recurso, a seleção e o allocator não mudam na falha.
- Estados de seleção, arraste, adição, grupo vazio e cancelamento usam o router
  de produção. Seleção e clipboard são estado do editor, não do runtime.

## Limite do aceite

Verificar as superfícies a 853×394 e 655×300, sem confundir raster host de
controles com malha Vulkan no dispositivo. Captura e gestos físicos aguardam
liberação de ADB ao final do plano. Seleção múltipla, conjuntos, foco e isolamento
passaram pelo cenário host/SDK e pelas capturas reais 655×300 nesta execução;
o contrato está em `N01-selecao-poses.md`. Este pacote não fecha N01: organização
temporal ampliada, curvas, regiões e aceite físico permanecem rastreados.

## Reversão

A mudança está nos backends de timeline, extensões ABI anexadas e superfícies
contextuais do clipe. Não altera o formato AECLIP 4 nem reescreve clips existentes.
Undo restaura o recurso anterior; os ícones são entradas novas do catálogo.

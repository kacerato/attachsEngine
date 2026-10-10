# N02 — proposta de autoria de propriedades e drivers

NÃO IREI SER SIMPLISTA NO DESIGN.

Estado: proposta original acompanhada da implementação executável AECLIP 7.
A imagem `N02-propriedades-drivers-conceito.png` permanece hipótese visual; não
é captura da engine nem prova de suporte. Catálogo, busca, criação, gravação,
remapeamento, curvas e DAG agora usam o backend real. O contrato e o nível de
aceite vigente estão em `N02-contrato-propriedades-drivers.md`. A investigação
em `N02-dependencias-verificadas.md` registra o estado anterior à implementação.

## Superfície de trabalho

A timeline existente permanece como lugar de edição. Selecionar uma propriedade
elegível oferece criar sua trilha, com objeto, componente e unidade vindos dos
descritores reais. O usuário não digita um endereço interno. Busca e destinos paginados usam
componente, nome e propriedade do catálogo efetivo. A edição oferece campos de
44 px e o teclado existente; a curva mostra amostragem efetiva e chaves por ID. Curvas e valores
discretos precisam de representações diferentes; referências não viram números.

O viewport permanece dominante durante seleção e preview. Editar curvas ou
drivers expande temporariamente a superfície contextual. Voltar recupera seleção,
tempo e enquadramento. Propriedades avançadas aparecem sob demanda.

## Drivers no toque

A proposta estrutural é uma cadeia vertical navegável: entrada → transformação
→ saída. Cada etapa abre sua edição com espaço suficiente, mantendo o resumo da
cadeia visível. Combinações ramificadas continuam sendo um DAG; a lista não altera
essa semântica. Um panorama de conexões é complementar à edição por etapa.

Entrada mostra o parâmetro real e seu valor. Remap mostra intervalos de entrada e
saída. Clamp mostra limites. Saída mostra alvo, propriedade, resultado e autoridade.
Ciclos, vínculos ausentes e escritores concorrentes aparecem junto da etapa
responsável. Aplicar valida a cadeia inteira e mantém o rascunho com o motivo específico
quando a publicação é recusada. Cancelar descarta o rascunho.

## Contratos de cada controle

| Controle proposto | Dependência necessária |
|---|---|
| Adicionar propriedade | Catálogo elegível, binding tipado/versionado e criação transacional |
| Chave / valor / curva | Amostragem, journal, transformações temporais e persistência |
| Preview / parar | Cópia isolada, posse e restauração determinística |
| Entrada de driver | Parâmetro existente de Animator ou propriedade permitida |
| Remap / clamp / curva | DAG compilado, orçamento, diagnóstico e avaliação sem alocar |
| Saída e autoridade | Consumidor real, validação do lote e resolução de conflitos |

Os valores e rótulos da imagem são ilustrativos. Unidades virão dos descritores;
não se presume que intensidade da Light use lux. A imagem também não comprova
alvos de 44 px: o layout executável deverá garantir esse tamanho e passar por
captura e interação na resolução mobile.

## Referências e reversão

A [Godot 4.5, introdução à animação](https://docs.godotengine.org/en/4.5/tutorials/animation/introduction.html)
cria trilhas a partir de propriedades. A
[Unity 6000.0 EditorCurveBinding](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/EditorCurveBinding.html)
separa caminho, tipo e propriedade. Extraímos descoberta contextual e identidade
estável; a apresentação vertical é uma adaptação ao toque da Astra.

A expansão contextual reutiliza a timeline e seus dados. A apresentação de
drivers pode ser substituída sem mudar o recurso ou seu grafo. Estados vazio,
selecionado, edição, preview, teclado, erro e vínculo removido precisam de fluxo
explícito antes da exposição pública.

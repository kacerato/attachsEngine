# Aba Jogo do viewport e recarga durante execução

Adendo solicitado em 17/09/2026. Estado: **arquitetura proposta, não implementada**. Não altera a prioridade atual de P01/P02. Vincula M06 (compilação/publicação), M03 (recuperação), P04 (abas/layout), P07 (recursos) e P12 (vistas/câmeras). Não cria um pacote extra no denominador P01–P18.

## O que já estava previsto

O plano mestre, ADR-06, exige compilação automática e publicação atômica de uma geração consistente, preservando a última válida quando houver erro. Também separa documento autoral, mundo Play, revisões de texto/documento, geração de build e epochs gráficos. Isso é a base para recarga, mas não especificava a aba de execução contínua no viewport. Não presumir que o runtime atual já suporta substituir assemblies ou migrar qualquer estado.

## Três comportamentos distintos

| Superfície | Responsabilidade | Atualização |
|---|---|---|
| Cena | Autoria, seleção, gizmos, câmera editorial | Documento e ferramentas editoriais |
| Prévia de câmera | Conferir enquadramento de uma câmera fixada | Atualmente sob demanda, sem simulação própria; não é hot reload |
| Jogo, futura aba do viewport | Mostrar o mundo Play e receber input de gameplay | Renderização contínua, controlada pelo relógio do runtime |

Hot reload é substituir código ou recursos numa execução existente. Renderizar em tempo real é outro contrato. A aba Jogo deve oferecer ambos gradualmente e informar quando uma mudança exigir reinício. O controle 5/15/30 Hz da mini prévia não deve limitar a aba Jogo.

## Donos e fronteiras

1. **AuthoringDocument** continua dono dos dados salvos. Editar durante Play não mistura automaticamente dados autorais e estado transitório.
2. **PlaySession** possui um único mundo, relógio e identidade por execução. Cena e Jogo podem visualizar esse mundo; abrir outra aba não instancia outra física nem duplica callbacks.
3. **RenderView** possui câmera, target, retângulo, projeção, constantes, visibilidade e históricos próprios. Reutiliza geometria/material residentes. Históricos de sombra/oclusão/temporal só são compartilhados quando houver compatibilidade comprovada.
4. **Viewport tabs** selecionam o consumidor de input. Cena recebe ferramentas; Jogo recebe gameplay ao capturar o foco. Saída explícita da captura e indicador visível; teclado do IDE nunca vaza para o jogo.
5. **Reload coordinator**, futuro serviço, recebe candidatas imutáveis com `PlaySessionId`, `DocumentRevision`, `BuildGeneration`, revisões de recurso e epoch. Resultado antigo não publica sobre uma sessão nova.

## Política de atualização

| Mudança | Caminho proposto | Preservação e falha |
|---|---|---|
| Propriedade permitida em Play | Fila de comandos tipados, aplicada entre passos | Preserva mundo; recusa e diagnostica invariantes/dependências inválidas |
| Material/textura | Preparar CPU/GPU, validar dependências, trocar referências após fence | Última versão válida permanece até a candidata ficar pronta; liberar antiga somente sem leitores |
| Malha/colisor | Reimportação incremental com IDs e invalidação de derivados | Reconstruir apenas dependentes; definir política para corpos em contato antes de ativar |
| Código com esquema compatível | Compilar snapshot, preparar provedor, validar e publicar em ponto seguro | Não prometer migração de pilha/corrotina; provedor deve declarar capacidade real |
| Código com esquema incompatível | Relatório de incompatibilidade e ação explícita de reiniciar | Nunca reiniciar silenciosamente nem apagar componentes autorais |
| Mudança estrutural não suportada | Marcar reinício necessário | Mundo atual continua com versão aplicada, rascunho permanece salvo |

Compilação fora da thread de render/simulação; publicação curta no ponto seguro. Uma candidata completa vence por geração, nunca por ordem de chegada. Coalescer mudanças rápidas; cancelamento invalida publicação sem destruir recursos ainda em uso. Falha de compilação não modifica mundo ativo, catálogo aplicado ou parâmetros anexados.

## Estado durante recarga

Antes de implementar migração, inventariar os recursos reais do provedor C# e seu descarregamento. Definir campos serializáveis preserváveis, referências por IDs estáveis, eventos inscritos, callbacks, timers, corrotinas, ponteiros nativos e ownership de física. Sem mapeamento válido, exigir reinício. Se recriar mundo for necessário, apresentar como reinício com restauração parcial, não como hot reload transparente.

Alterações temporárias de Play não vão para o arquivo de cena. Uma futura ação Aplicar ao documento deve mostrar diff por propriedade e conflitos com edições autorais feitas desde o início da sessão; aplicar como comando desfazível. Stop descarta estado transitório e restaura a vista editorial.

## Interface e orçamento

Abas **Cena | Jogo** na área do viewport, com identidade Astra e integração ao layout persistente. Controles compactos Play/Pause/Passo/Stop; estado legível `Executando`, `Pausado`, `Compilando`, `Aplicando`, `Erro` ou `Reinício necessário`. Detalhes e navegação arquivo/linha no console, sem preencher a barra com logs.

Mudar de aba não deve reiniciar o jogo. Política de simulação em aba oculta deve ser configurável e explícita; app em background segue ciclo de vida Android e orçamento, não mantém GPU ativa indiscriminadamente. Aspect/resolução/orientação da aba Jogo têm contrato próprio, sem alterar o componente de câmera por consequência de resize.

## Ordem de entrega e aceitação

1. Auditar Play, build provider e publicação de assets existentes; registrar capacidades e restrições reais.
2. Integrar aba Jogo com o mundo Play existente e RenderView própria. Aceitar somente após animação/tempo/input contínuos, troca de abas sem duplicar mundo e Stop restaurando autoria.
3. Pausa, passo, captura de input e política de aba oculta/background; recuperar surface sem perder documento.
4. Atualização de propriedades e assets em ponto seguro. Confirmar visualmente a troca, continuidade do mundo, rollback e ausência de descarte prematuro de GPU.
5. Recarga de código conforme capacidade comprovada do provedor. Conferir geração inválida/antiga, mudanças compatíveis/incompatíveis, referências e eventos sem duplicação.
6. Aplicar mudanças de Play por diff transacional, apenas depois de definir conflitos.
7. Medir latência de publicação, custo CPU/GPU e memória; percursos prolongados de recarga, rotação/background, erro e recuperação.

Documentar separadamente: aba disponível, tempo real funcionando, propriedades atualizando, assets recarregando e código recarregando. Nenhuma dessas evidências certifica automaticamente as demais. Não aumentar o percentual por este documento de arquitetura.

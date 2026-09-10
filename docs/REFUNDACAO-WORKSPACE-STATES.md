# Estados do workspace — M1

Releitura do plano §§7–8 em 2026-09-09. Este inventário distingue o estado
implementado do alvo; não atribui à Astra componentes existentes em outras engines.

## Composição de referência

```text
Projeto/cena | ferramentas de edição | execução
Hierarquia   | Viewport              | Inspector da seleção
Arquivos     |                      |
```

Os retângulos de painéis são subtraídos antes de resolver o viewport. Arrastar
divisores altera larguras; fullscreen recolhe ambos e preserva as larguras.
Em espaço estreito, o código atual comprime proporcionalmente os painéis. O alvo
ainda pendente é alternar painéis com alvos de toque utilizáveis, mantendo os
mesmos comandos. Não considerar essa compressão como composição mobile concluída.

| Estado | Hierarquia/arquivos | Viewport | Inspector atual e lacuna |
|---|---|---|---|
| Projeto vazio | Raiz e arquivos reais | Grade; zero geometria oculta | Nada selecionado |
| Grupo selecionado | Seleção estável | Gizmo do transform | Transformação, visibilidade herdada e active; sem material/física/sombra própria |
| Malha com recurso | Referência existente | Extração pelo assetId | Transformação, Material, propriedades consumidas; componentes tipados ainda pendentes |
| Câmera sem recurso | Objeto explícito | Transformação | Sem Material ou corpo rígido; projeção e parâmetros tipados ainda não são uma entrega completa |
| Arquivos/importação | Navegação atual | Área restante | Importação externa completa pertence a M5, não presumir pronta |
| Execução | Painéis recolhidos | Área de execução | Integração física atual não comprova M7 nem jogo arbitrário |

## Contratos de controles auditados nesta fatia

| Controle | Contexto | Dados/comando | Consumidor/prova |
|---|---|---|---|
| Material | Seleção com assetId | Abre página; edição usa histórico existente | EditorMapScene extrai material; testes de edição existentes |
| Visível | Seleção | ToggleVisible, booleano versionado | inheritedVisible aplica hierarquia na extração |
| Projetar sombra | Seleção com recurso | ToggleCastShadow | EditorMapScene publica castShadow |
| Corpo rígido/flutuação | Seleção com recurso | ToggleRigidBody | EditorWaterPlay exige active, rigidBodyEnabled e assetId; não é física genérica concluída |
| Receber sombra / Estático | Ausentes | Dados antigos preservados | Sem consumidor encontrado; não expor como efeito funcional |

Ao sair de uma seleção com material para outra sem recurso, a página efetiva
volta a Transformação. Não alteramos o arquivo ou adicionamos material fictício.
Os flags legados receiveShadow/isStatic continuam serializados para preservar
compatibilidade; removê-los do formato é outra decisão, com migração própria.

Gate restante: barra principal por finalidade, propriedades condicionais de
componentes, alternância em telas estreitas e matriz de interação/resize. Este
arquivo não substitui os testes e capturas associados no relatório principal.

## Atualização: prova compacta e feedback

A implementação de 2026-09-09 introduziu alternância abaixo de 672 dp, com um
painel lateral por vez e Arquivos separado. O modo amplo continua lado a lado.
O usuário questionou a faixa de quatro nomes e o tamanho da captura de teste
(escala temporária 1.3, padrão 0.6). A escala normal foi restaurada. A composição
compacta permanece em revisão de UX, não aprovada como layout definitivo.
Ver evidências e limites no relatório principal.

## Revisão após feedback: faixa removida

A faixa de quatro opções foi substituída por Painéis, que abre um menu apenas
sob demanda. Escolher uma opção fecha o menu; Fechar preserva a opção anterior.
O corpo recupera 44 dp e mantém as larguras do modo amplo. Provas e APK em
REFUNDACAO-ASTRA.md, seção acesso sob demanda aos painéis. Esta revisão substitui
a apresentação da prova compacta anterior; não altera a escala padrão.

## Barra por finalidade

Em 2026-09-09, barra passou a Cena/Salvar, contexto textual, Undo/Redo e um único
Play/Stop. Ambiente e água ficaram no menu Cena com nomes explícitos. OpenProject
sem ação e atalhos duplicados foram removidos. Água na raiz e navegador legado
continuam pendências arquiteturais, não funcionalidades reconstruídas por esta
mudança. Evidências da rodada no relatório principal.


## Atualização de implementação — 10/09/2026, componentes e referências visuais

O inspetor normal agora tem uma lista única de Transformação e componentes recolhíveis. A divisão antiga em abas globais Transformação/Material/Propriedades descrita acima é histórica. Geometria/Material passam a ser páginas internas do componente Malha. Add busca tipos e filtra famílias; a geometria é escolhida da biblioteca efetiva. Expandir concentra a área de edição no bloco escolhido e fechar retorna à lista, preservando paginação.

O workspace Código continua central, e arquivos/hierarquia mantêm seus caminhos existentes. A página especializada de água é uma exceção anterior ainda aberta no plano de extração do módulo. Não foram adicionadas abas sem consumidor para NoCode, Store, animação ou Material Graph.

Ver [decisão e rastreio das dez imagens](adr/ADR-REFUNDACAO-CAMERA-MESH-INSPECTOR.md). Este layout está em fonte e ainda não foi compilado ou visto no aparelho. O usuário autorizará ADB depois.


## Atualização de implementação — instâncias e referências físicas

Continuação 10/09/2026: componentes repetíveis são abertos por instanceId. Add respeita cardinalidade e mantém anexos recolhidos. Menu próprio: copiar, colar, restaurar e remover a instância; ajuste geométrico atua no colisor aberto. Seletor contextual por nome/ID, pai e paginação serve owner do colisor, corpo da junta e ObjectReference C#.

Colisor aberto mostra seu volume/rotação no viewport, com owner inválido em vermelho; Junta mostra suas âncoras. A visualização é overlay recortado, sem profundidade nem gizmo de shape. Ícone Junta tem PNG próprio integrado. [Contrato e pendências](adr/ADR-REFUNDACAO-COMPOSICAO-FISICA.md). Nenhuma tela deste bloco foi executada no aparelho; o usuário autorizará ADB depois.

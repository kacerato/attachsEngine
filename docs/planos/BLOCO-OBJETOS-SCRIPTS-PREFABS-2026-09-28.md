# Bloco ampliado — Objetos, scripts e prefabs

Data: 28/09/2026. Branch: `codex/gameplay-runtime`. **Em execução**, por pedido
do usuário para incorporar os próximos blocos em um só. Etapas 1–4 entregues;
as etapas 5–12 continuam pendentes, com seus aceites abaixo.

## Escopo e precedência

Este passa a ser o bloco de execução atual: **O1 + O2 + O4 + P completo** do
[inventário versionado](INVENTARIO-UNITY-INSPECTOR-OBJETOS-2026-09-26.md).
Substitui a sequência curta “O1a → O1b → O1c/P mínimo → O2 → O4”. O recorte de P
deixa de ser apenas a base de instanciação: inclui overrides, edição de recurso,
prefabs aninhados, variantes e identificação das instâncias.

Os códigos e números antigos continuam como rastreabilidade. Um bloco maior
não implica um único patch ou commit: cada etapa entrega um caminho funcional,
com validação própria, sem misturar sistemas independentes numa alteração.

O3, já entregue, é dependência reutilizada. S (Sprite/2D), M (Tilemap), T
(Terreno) e C (Canvas/UI) continuam depois deste bloco; não foram incorporados.
O atlas de expansão continua sendo pesquisa, sem transformar nomes pesquisados
em capacidades disponíveis.

## Base já entregue

- I4: multisseleção e edição conjunta, incluindo materiais, ambientes e UV.
- Ativação de objetos e ciclo de vida sob hierarquia inativa: `002986b8`.
- Tags de projeto, Inspector, persistência e consultas no runtime: `6ba3e8cf`.
- O3: visibilidade/selecionabilidade da hierarquia, registrada no inventário.

As entregas de ativação e tags permanecem válidas. Seus relatórios são históricos;
a continuação vigente é a deste documento. Após as etapas 2 e 3, o inventário geral está em
**115 existentes, 6 parciais, 91 ausentes e 7 adaptações**, total 219.

## Etapas internas, em ordem de execução

### 1. Concluir habilitação por componente — O1a, item 121

**Entregue em 28/09/2026:** [implementação, referências, validação e limites](O1A-HABILITACAO-2026-09-28.md).

Rastrear cada estado de habilitação do schema e da API até o consumidor real.
Definir quais tipos suportam desativação, que estado preservam e como retomam.
Conectar Inspector, scripts, serialização e lifecycle, reutilizando a reflexão
e os consumidores existentes. Não expor um `Enabled` genérico sem semântica.

**Aceite:** desabilitar/reabilitar componentes pelo editor e por script,
confirmar o efeito real, repetir sob pai inativo e conferir salvar/reabrir e
Stop sem duplicar registros, callbacks ou recursos.

### 2. Completar scripts dinâmicos e destruição — O1b, itens 125, 139 e 140

**Entregue em 28/09/2026 junto com a etapa 3:** [implementação, contratos e validação](O1B-SCRIPTS-MENSAGENS-2026-09-28.md).

Expor acesso a comportamento pelo tipo em GameObject; adicionar/remover
instâncias de script durante Play, preservando identidade, campos e isolamento
de falhas. Integrar `Awake`, Enable, Start, Disable e destruição à ordem de
despacho existente. Acrescentar destruição com atraso em ponto seguro, com
contrato explícito de relógio, repetição do pedido e encerramento da sessão.
Os itens 119/120, já existentes para componentes, são dependências a preservar.

**Aceite:** criar um comportamento dentro de um callback, encontrá-lo pelo tipo,
remover apenas esse script e destruir outro objeto com atraso. Referências
vencidas devem falhar de forma identificável; recomeçar Play não herda filas.

### 3. Completar descoberta e mensagens — O1, itens 132–135

**Entregue em 28/09/2026 junto com a etapa 2:** [implementação, contratos e validação](O1B-SCRIPTS-MENSAGENS-2026-09-28.md).

Adicionar busca por nome no mundo e as capacidades de mensagem ao objeto,
descendentes e ancestrais. Antes da implementação, registrar regras para nomes
duplicados, receptores ausentes, assinatura/payload, objetos inativos, ordem de
despacho e remoção de receptores durante uma chamada. Aproveitar identidade e
registro dos scripts da etapa 2. A busca global não deve alterar silenciosamente
o contrato da busca existente em subárvore.

**Aceite:** uma hierarquia com receptores em pai, filho e outro ramo recebe
somente as mensagens esperadas; um receptor que falha ou se remove não corrompe
os demais. Busca e diagnóstico de ausência seguem o contrato documentado.

### 4. Instanciar hierarquias com referências corretas — O1c, item 138

**Entregue em 28/09/2026:** [contrato, referências e aceite](O1C-INSTANCIACAO-2026-09-28.md).

Implementar clonagem de objetos, filhos, componentes e campos serializados de
scripts. Usar novas identidades e remapear referências internas; definir a
política para referências externas e recursos compartilhados. Preparar o grafo
antes dos callbacks e desfazer a criação parcial se houver falha.

**Aceite:** clonar um conjunto cujo script referencia um filho e um componente;
as referências da cópia apontam para a cópia. Modificar/destruir uma instância
não altera a outra nem invalida os recursos compartilhados.

### 5. Incorporar as primitivas reais — O2, itens 141 e 143–148

Entregar esfera, cilindro, cápsula, quad e plano com geometria, normais, UVs,
bounds e colisores adequados, preservando o cubo existente. Conectar as receitas
ao menu de criação, à API de criação, ao histórico e à persistência. Formas de
colisão aproximadas precisam ser declaradas, nunca apresentadas como exatas.

**Aceite:** criar pelo menu e por script, renderizar, aplicar material, colidir
quando aplicável, editar transformações, desfazer/refazer e salvar/reabrir.
Essas peças passam a compor cenários verificáveis das etapas de prefab.

### 6. Criar o recurso prefab e suas instâncias — P, itens 149, 150 e 156

Estabelecer identidade persistente do recurso, formato versionado, dependências
e vínculo entre fonte e instância. Criar o recurso a partir de uma hierarquia,
salvá-lo, carregá-lo e instanciá-lo no editor e no runtime, usando a etapa 4.
Definir diagnósticos para fonte ausente, arquivo inválido e referência quebrada.
Receitas/presets existentes continuam distintos de prefabs vinculados.

**Aceite:** salvar um conjunto como recurso, reabrir o projeto e criar várias
instâncias por ambos os caminhos, preservando dados e referências.

### 7. Overrides, atualização da fonte e Unpack — P, itens 84, 85, 110, 111, 154, 155 e 157

Representar diferenças de propriedades, componentes e objetos por identidade
estável. Implementar Apply/Revert com validação de dependências e atualização
das instâncias; registrar operações no histórico e tratar falha de gravação.
Unpack remove o vínculo preservando o resultado efetivo. Multisseleção deve
explicitar quais operações são compatíveis, sem aplicar alterações ambíguas.

**Aceite:** alterar apenas uma instância, modificar a fonte, conservar os
overrides, aplicar/reverter uma diferença, desfazer/refazer, salvar/reabrir e
desvincular uma instância sem perder seus objetos nem referências.

### 8. Editar o prefab em isolamento e contexto — P, itens 151 e 158–170

**Decisão do usuário em 28/09:** abrir a fonte em rota contextual com breadcrumb;
voltar à cena preservando seleção e enquadramento. Fonte e instância terão
contextos explícitos. A escolha está aprovada; sua implementação permanece pendente.

Integrar abertura da fonte, navegação de ida/volta, indicação do contexto e
inspeção dos overrides. Implementar as preferências de modo, cena de fundo,
visualização do contexto, restrições de seleção/transformação e encaixe
definidas no inventário. Salvar explícito, Auto Save e confirmação ao sair
devem operar sobre a fonte correta e conservar recuperação em caso de erro.

**Aceite:** abrir pela instância, editar a fonte, alternar o contexto e retornar
à cena preservando seleção e alterações. Repetir com Auto Save ligado/desligado
e falha de gravação, sem sobrescrever silenciosamente a cena ou outro recurso.

### 9. Prefabs aninhados e variantes — P, itens 152 e 153

Compor recursos sem achatar seus vínculos e derivar variantes com diferenças
persistentes. Resolver precedência entre fonte, variante e instância; detectar
ciclos e diagnosticar dependências ausentes. Apply/Revert e Unpack precisam
respeitar o nível de origem, sem modificar a fonte errada.

**Aceite:** um prefab contém outro; uma variante muda parte dos dados; duas
instâncias mantêm alterações próprias. Atualizar a fonte propaga somente o que
não foi sobrescrito. Reabrir conserva os níveis e um ciclo é recusado.

### 10. Identificação visual de objetos e recursos — O4 e P

Cobrir O4 (70–74, 76, 77) e os indicadores de P (75, 99–101): atribuição,
remoção e exibição de ícones, rótulos, textura personalizada e ícone de script;
identificar prefab, variante e origem de modelo apenas quando o vínculo real
existir. Integrar controle de visibilidade aos gizmos e persistir o estado no
lugar correto, distinguindo metadados de autoria e preferências do editor.

**Aceite:** identificar e selecionar objetos no aparelho, alternar os ícones,
reabrir o projeto e conferir coerência entre viewport, hierarquia e Inspector.
Indicadores necessários às etapas 6–9 entram junto delas; esta etapa completa
O4 e verifica o conjunto, sem adiar feedback essencial de prefab.

### 11. Resolver Static com consumidores reais — O1, item 115

Auditar separadamente GI, oclusão, batching, navegação e reflexão do recorte do
inventário. Para cada capacidade, localizar consumidor, dependências e política
de invalidação quando o objeto muda; implementar a cadeia necessária antes de
expor a opção. Não recolocar uma flag genérica como substituto desses sistemas.

**Aceite:** cada opção exposta demonstra efeito no consumidor e comportamento
correto após alteração do objeto e reabertura. Esta etapa é um risco de escopo:
se um backend necessário ainda não existir, registrar a dependência e manter
essa capacidade e o fechamento integral do bloco pendentes. Não contar suporte
parcial como entrega de todo o item 115.

### 12. Aceite integrado e fechamento do bloco

Construir um cenário com primitivas, componentes habilitáveis, scripts
dinâmicos, tags e mensagens; salvá-lo como prefab, criar variante e instâncias,
editar overrides e instanciar/destruir durante Play. Salvar, encerrar/reabrir,
repetir Play/Stop e conferir referências, histórico e recursos.

Verificar erros de fonte ausente, script removido e arquivo inválido sem
fallback silencioso. Medir criação/destruição repetida no dispositivo,
alocações e recuperação de recursos em escala declarada, sem inventar limites
de desempenho antes da medição. Atualizar a contagem por item e os links de
evidência. Fechamento só ocorre com os itens cobertos ou adaptações explicitamente
justificadas; dependências pendentes continuam visíveis.

## Dependências e execução

O caminho principal é habilitação → scripts dinâmicos → clonagem → recurso
prefab → overrides → edição da fonte → composição/variantes → aceite integrado.
Mensagens dependem do registro de scripts; primitivas alimentam os cenários;
ícones acompanham a capacidade que identificam. Static tem dependências próprias
e não deve provocar a inserção de opções sem consumidor nas etapas anteriores.

Cada etapa começa pela leitura do caminho real dos dados e pesquisa oficial
versionada correspondente. As referências de partida são as páginas Unity
6000.0 registradas nas seções 9 e 13–16 do inventário e o atlas Unity/Godot do
plano de expansão. A entrega de cada capacidade deve registrar o link oficial
efetivamente consultado, as decisões e as diferenças da Astra. Este agrupamento
não substitui a pesquisa técnica nem fixa antecipadamente assinaturas de APIs.

Para intervenções de editor: **NÃO IREI SER SIMPLISTA NO DESIGN.** Capturar o
fluxo existente, avaliar espaço/toque/estados, propor mudança estrutural quando
necessária e validar a UI executável no aparelho. Escolhas que mudem workflow
ou compatibilidade serão apresentadas antes da implementação correspondente.

Validar por build, smoke test, regressões direcionadas e cenário real; ampliar
testes somente pelo risco encontrado. Commits continuam coesos e locais,
preservando a orientação de não fazer push.

# API, dados e documentação

## Modelo público proposto

**Avanço em 01/10:** Path e Animation declaram identidade, ordem e fronteira de alocação pelo contrato comum `ComponentType.collections`. Presets e Apply seletivo de prefab recusam endereços posicionais incompatíveis; reversão em lote valida o candidato completo e registra um Undo. Isso não implementa merge por ElementId nem operações estruturais genéricas. [Implementação e evidências](../CONTRATOS-COLECOES-2026-10-01.md). A [matriz gerada](../../componentes/MATRIZ-PROPRIEDADES.md) reúne contagem do registro, fachadas, criação, dependências, referência oficial, consumidor e propriedades; não substitui os aceites de runtime e aparelho.

Preservar os objetos e componentes Astra. A API de alto nível deve oferecer descoberta, leitura, escrita e operações com tipos C# coerentes, enquanto a representação nativa conserva IDs persistentes e armazenamento apropriado. A organização inspirada em nodes serve à autoria e às composições, sem acrescentar uma SceneTree Godot concorrente ao SceneGraph Astra.

| Conceito | Contrato observado | Ampliação proposta | Aceite |
|---|---|---|---|
| Objeto | `Astra.GameObject` com world/generation | APIs de duplicação/instanciação, consultas e reparent com política explícita | Referências antigas recusadas; nomes não alteram identidade |
| Componente | `Component` + TypeId/InstanceId | Fachadas tipadas que usam o mesmo descriptor | Inspector e script enxergam os mesmos valores/erros |
| Propriedade | IDs, números/bools/enums/refs, slots e triplas | Tipos compostos, coleções, curvas e strings | Mesmo valor atravessa arquivo, editor e runtime |
| Recurso | `AssetGuid`/bindings | Handles de execução, carregamento assíncrono, dependências, overrides | Guid continua conhecido quando asset está ausente |
| Operação | `WorldStatus` e comandos | Operações agrupadas com resultado por transação | Rejeição não deixa mutação parcial |
| Capability | `EngineCapability` | Consultável na ajuda e API, com motivo do alvo | Inexistente, não compilada e indisponível no aparelho distinguíveis |

Não são assinaturas prontas. Nenhum exemplo neste plano deve levar o usuário a chamar um método proposto como se já compilasse.

## Contrato por tipo de propriedade

| Tipo | Persistência e validação | Editor | API e runtime |
|---|---|---|---|
| Bool/enum/int/float | Default explícito; finitos; enum estável; inteiro sem perda de precisão | Toggle, combo e campo com unidade/faixa | Tipagem exata; erro de tipo distinto de range |
| String | UTF-8 no arquivo/ABI, tamanho permitido e normalização definida | Campo/IME, multiline quando pertinente | Buffer caller-owned ou cópia com lifetime especificado |
| Vector2/3/4 | Validação do conjunto, não 3 escritas parciais inválidas | Campos X/Y/Z e arraste agrupado | Valor estruturado; conversão de espaço explícita |
| Quaternion/rotation | Normalização/política para zero; convenção e handedness | Euler como apresentação, quaternion como dado quando apropriado | Ordem/unidade documentadas, sem perder roll |
| Color/HDR | Espaço linear/encoding e alpha; HDR não limitado a 1 sem motivo | Color picker, exposure quando pertinente | Conversão no limite correto, não em cada consumidor |
| Bitmask | Bits/nomes estáveis; largura definida | Seletor de layers/categorias | Máscaras físicas e visuais não se confundem |
| Object/ComponentRef | Identidade durável; nulo/ausente; tipo e scope | Picker + breadcrumb + localizar + reparar | Resolver no mundo atual, sem ponteiro durável |
| AssetRef | Guid+tipo; estado herdado/vazio/ausente | Picker + thumbnail + fazer único + dependentes | Async resolve com cancelamento; erro explicável |
| Struct | Schema de campos, default e migração próprios | Grupo aninhado, reset agrupado | Transação com validação cruzada |
| List/map | Chave/ElementId quando referenciada; limites; duplicatas/política | Adicionar/remover/reordenar, filtro, virtualização | Operações estruturais em safe point |
| Curve/gradient | Keys/stops, tangentes e extrapolação/cor explícitas | Editor especializado sobre recurso comum | Amostragem previsível; alteração invalida consumidor |
| Event connection | Assinatura/target/método/argumentos serializados | Conectar, localizar alvo, reparar incompatível | Desconexão por lifetime, thread e reentrância |

**Lista de clipes de `astra.animation`:** cada entrada tem `ElementId` monotônico próprio e `AssetGuid` do clipe. A versão 3 do componente persiste ordem, IDs e próximo ID; versões 1–2 continuam legíveis. O Inspector adiciona, remove e move entradas com Undo; o binding `clips` publica o ID por slot. Em Play, `AnimationPlayer.ClipEntries` lê esses IDs, `SetClip(elementId, guid)` troca o recurso por identidade e `AddClip`, `RemoveClip` e `MoveClip` alteram a lista da sessão pela ABI v11. `AddClip` exige clipe carregado, só devolve o ID depois do commit e recusa o limite de 32 entradas; remover a última referência de um clipe também retira seu estado no próximo passo do avaliador, salvo quando ele é o clipe padrão. O documento autoral fica intacto ao parar. Dois elementos podem referir o mesmo recurso legado, mas compartilham um único estado de reprodução identificado pelo `AssetGuid`. A Unity adiciona pelo nome e pode substituir uma entrada homônima; a Astra acrescenta por recurso e devolve um ID distinto. Operações estruturais genéricas para outras coleções e overrides de prefab endereçados por ElementId continuam pendentes. Referências funcionais: [Unity Animation.AddClip](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Animation.AddClip.html) e [Godot AnimationLibrary](https://docs.godotengine.org/en/stable/classes/class_animationlibrary.html).

Defaults novos são escolhidos durante a especificação do componente, e os atuais permanecem no descriptor. Exemplo localizado: `Character` inicia radius=.45 m, halfHeight=.55 m, eyeHeight=1.65 m, speed=8 m/s, slope=45° e jumpSpeed=5 m/s. `halfHeight` refere-se à metade do cilindro, não à altura total da cápsula. Esse nível de precisão é necessário em toda ficha; não se copia a unidade de Unity/Godot pelo nome do campo.

## Dependências que o schema precisa representar

| Relação | Exemplo | Política de adição/remoção |
|---|---|---|
| Requisito no mesmo objeto | Olhar exige Câmera, já localizado | Fechar dependências antes da mutação; uma transação |
| Incompatibilidade | Character versus body/collider no estado atual | Mostrar motivo; troca explícita com migração, não remoção silenciosa |
| Relação hierárquica | Shape sob corpo; widget sob Canvas/layout | Validar scope e pose; reparent revalida composição |
| Referência a componente | Joint para corpo em outro objeto | Validade por instância, não só nome do objeto |
| Recurso tipado | Clip, skin, material, navmesh | Pode preservar ausência autoral; runtime recusa operação dependente |
| Serviço de mundo | AudioSource precisa de áudio; agente de navegação precisa de nav world | Configuração de projeto/mundo, não componente escondido adicionado a cada objeto |
| Capability/backend | GPU particles, shadow, XR | Verificar backend e aparelho; preservar intenção no documento |
| Sugestão/receita | Mesh sugerida para visualizar um collider | Nunca virar dependência obrigatória |
| Conflito de escritor | Animator versus Rigidbody dinâmico | Seleção de autoridade e fase explícitas |
| Requisito condicional | Occlusion áudio usa física somente se habilitada | Revalidar ao mudar a condição, com erro e reparação |

Não inferir dependência obrigatória pela herança ou pelo tipo de um campo. `Texture2D` em uma propriedade pode ser opcional; `Node3D` como base só explica herança. A relação `RequireComponent` Unity também não expressa todo requisito operacional do recurso.

## Transações, arquivo e Undo

Trilha autoral: comando com IDs e versão esperada → resolver composição/referências → construir candidato → validar valores e relações → commit único → histórico → dirty/invalidação → consumidores. Operações de slider/drag consolidam gesto em um Undo, com cancelamento; não publicam todos os intermediários inválidos. Operação composta falha integralmente.

Trilha de gameplay: API → validação/identidade/thread → fila de comando quando necessária → safe point → consumidor → status. Não inserir cada frame no Undo autoral. Mudar o Play não modifica automaticamente a cena salva. Futuro “Aplicar alterações de Play” exige diff selecionável e remapeamento de objetos/recursos, com exclusão de estados efêmeros.

Versão de cena e versão de componente têm funções diferentes. Migração por componente preserva ID, propriedade desconhecida e referência não resolvida. Não apagar um comportamento porque compilação falhou. Para coleções, overrides e tracks apontam a ElementId quando a ordem é editável. Validação de arquivo não pode executar callbacks de jogo.

## Ordem de execução e autoridade

Já existem `Start`, `Update(float)`, `FixedUpdate(float)` e `Stop`, além de callbacks de contatos/triggers no contrato inspecionado. `BehaviorWorld` controla início e invocação. Não prometer `Awake`, `OnEnable`, `_ready` ou `LateUpdate` equivalentes sem implementação. Unity e Godot têm ordens próprias; a adaptação deve documentar exatamente o que Astra executa.

Ordem-alvo a validar antes de alterar o runtime:

1. Processar eventos de plataforma e foco/input; publicar comandos estruturais no safe point definido.
2. Iniciar instâncias elegíveis e executar passos fixos com input estável; scripts/motores produzem intenção/forças; física publica pose e contatos na ordem documentada.
3. Executar lógica variável e avaliação de animação conforme domínio de tempo; root motion encaminha intenção ao motor quando ele é o proprietário.
4. Resolver poses finais/hierarquia, bounds e notificações de dados alterados; preparar áudio, UI e render.
5. Consumidores usam snapshots/referências válidas; descarte é deferido enquanto GPU/jobs ainda utilizarem dados.

Essa sequência é proposta de contrato, não descrição certificada da ordem atual. A implementação deve comparar o loop real antes de consolidá-la. Corpo dinâmico escreve pose via física; cinemático recebe comandos; animação escreve pose visual/ossos ou produz root motion; editor escreve documento fora de Play. Override de script exige operação declarada, como teleport, sem guerra silenciosa de setters.

## ABI e limites de erro

`ScriptSceneAccess` declara version=12 e size no checkout atual. A v11 acrescentou três ponteiros para editar clipes em Play; a v12 acrescentou `setParentWithPolicy` ao fim. O prefixo v10 permanece na mesma ordem. Extensões devem preservar layout/alinhamento do prefixo, append quando compatível, checar tamanho antes de acessar campo e negociar função/capability. Mudança incompatível exige nova versão e adaptação dos dois lados. Enum/tamanhos/layout C# e C++ são conferidos em build/teste dirigido.

Strings/arrays atravessam a fronteira por ponteiro e capacidade com ownership definido, ou handles opacos; nunca usar ponteiro temporário como referência durável. Consultas reportam contagem necessária/truncamento, como a superfície atual de física já prevê. API diferencia objeto ausente, mundo errado, componente removido, tipo incorreto, range inválido, recurso ausente, capability indisponível, rejeição de fase, cancelamento e erro de backend pelo mecanismo de status vigente, ampliado quando necessário.

Nenhum wrapper transforma erro em default silencioso. Operação assíncrona distingue aceita/pendente/aplicada/falhou; aceitação na fila não é confirmação de efeito. Exceção em script é isolada e diagnosticada sem matar todos os outros scripts; descarte de callbacks e resources é garantido.

## Documentação que acompanha cada pacote

| Produto documental | Fonte | Leitor/uso |
|---|---|---|
| Referência de componentes/propriedades | Schema + descriptor + versão da capability | Inspector, busca, ferramenta e autor |
| API C# | Declarações públicas e doc comments alinhados com native | IDE, assinaturas, métodos, erros e exemplos |
| Guias de uso | Texto autoral revisado | “Criar sensor”, “Montar câmera”, “Instanciar prefab”, “Publicar jogo” |
| Matriz de capacidades por alvo | Registro real + detecção de backend | Motivo de recurso indisponível, qualidade desejada/resolvida |
| Guia de diferenças | Decisões Astra × referência fixada | Portar conhecimento de Unity/Godot sem presumir compatibilidade binária |
| Migrações/changelog | Versões reais e fixtures | Abrir projetos antigos e atualizar scripts |

Ficha de referência: propósito; categoria; versão; recurso/componente/serviço; requisitos e conflitos; propriedades com padrões/unidades; operações e erros; uso em edição/Play; ownership; mini-exemplo Astra compilável; backend; limitações; links relacionados e fonte. A documentação externa só serve como pesquisa, não como prova de disponibilidade.

Manter português para orientação e rótulos; nomes de API e IDs são estáveis. Links da ajuda devem abrir por TypeId/PropertyId, resistentes a renomeação de label. Busca por termos Unity/Godot pode apontar para o equivalente Astra com a classificação equivalente, adaptação explícita ou pendente. “Não aplicável” requer motivo, como tipo interno de editor de outra engine sem comportamento de jogo.

## Estratégia para não recriar documentação divergente

Evoluir `component_reflection.h` e a geração existente de `MATRIZ-PROPRIEDADES.md`. Não extrair código C++ com regex para determinar schema de execução. A exportação documental usa introspecção da engine; C# oferece sua superfície pública complementar. Uma verificação compara IDs e contratos compartilhados, sem testar apenas presença de palavras no fonte. Conteúdo manual descreve workflow/decisão que o descriptor não expressa.

O atlas desta entrega é **pesquisa externa**, não gerador de componentes nem importador automático de APIs estrangeiras. O catálogo Unity retém avisos de parser; Godot retém metadados de XML e defaults quando presentes. A autoria Astra só incorpora uma propriedade quando existe contrato, consumidor e critério de aceite.

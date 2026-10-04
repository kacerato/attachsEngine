# Estratégia para completar as capacidades da Astra

> Proposta anterior rejeitada pelo usuário por confundir o contexto com Astra e manter entregas pequenas e lentas. Não usar como plano de execução deste pedido. A proposta revisada para o repositório enviado está em [Produção em lote no attachsEngine](PLANO-PRODUCAO-EM-LOTE-ATTACHSENGINE.md).

Data da análise: 2 de outubro de 2026. Escopo: checkout local de `attachsEngine`, cujo `origin` aponta para `https://github.com/kacerato/attachsEngine.git`. A consulta pública ao GitHub retornou 404; as constatações sobre a Astra vêm dos arquivos locais. Esta entrega é uma proposta técnica baseada em inspeção e pesquisa, sem implementação de novos backends, execução de testes ou certificação de capacidades.

## Recomendação

Para completar a Astra preservando o investimento atual, recomendo **evolução incremental com bibliotecas especializadas, expansão do contrato de autoria existente e fechamento por famílias**. O trabalho repetitivo deve ser derivado dos descritores; algoritmos difíceis devem vir de bibliotecas; integração, ownership, migração e comportamento precisam continuar explícitos.

Se a prioridade absoluta for possuir rapidamente a amplitude de uma engine madura, aceitar **Godot como base real do produto** tende a reduzir mais trabalho que combinar bibliotecas independentes. Isso muda runtime, formatos e manutenção. Não é uma simples troca de backend nem uma decisão aplicada por este documento.

Não recomendo executar simultaneamente as substituições de renderer, ECS, linguagem e UI do plano Astra Forge como pré-requisito para completar componentes. O plano tem pesquisa útil, mas essa combinação abre uma migração de plataforma. Extraia dele bibliotecas e contratos quando uma lacuna concreta justificar a integração.

## O que já existe

| Evidência local | Constatação | Consequência |
|---|---|---|
| `native/scene/components.h` | Tipos persistentes, versões, propriedades, referências, coleções e migração | Preservar identidade e compatibilidade; ampliar o modelo existente |
| `native/scene/component_schema.h` | Registro por famílias, requisitos, conflitos, consumidor e mutabilidade em Play | Já existe uma fonte comum para criação e uso dos componentes |
| `native/scene/component_properties.h` | Escrita validada de propriedades e operações compostas | Editor e scripts devem continuar passando pela mesma semântica |
| `native/scene/component_api_csharp.h` | Geração de fachadas C# sobre a ABI genérica | Não escrever uma ponte nativa independente para cada tipo |
| `native/scene/component_reflection.h` | Matriz de propriedades e auditoria de contratos | Expandir a auditoria existente, sem criar outro registro concorrente |
| `native/runtime/game_world.cpp` | Consumo dos schemas nas operações do mundo | Preservar safe points, geração de handles e invalidação |
| `native/runtime/scene_audio.cpp` | Consumidor efetivo de miniaudio | Áudio permite demonstrar a estratégia sem primeiro integrar uma biblioteca nova |
| `native/CMakeLists.txt` | Jolt, miniaudio, SQLite e codecs já integrados ao build | Reutilizar o trabalho existente antes de substituir dependências |
| `tools/report-family-progress.py` | Relatório do catálogo versionado, com verificações de evidências e estados | Evoluir esse fluxo de saldo, não substituí-lo por contagem de classes |

O saldo registrado em `ampliacao-2026-09-23/FAMILIAS-ESTADO.json` contém **91 capacidades: 87 obrigatórias e 4 condicionais; nas obrigatórias, 10 concluídas no registro, 5 parciais e 72 a auditar**. As 77 sem encerramento não equivalem a 77 capacidades ausentes. Há implementação ainda não reconciliada, e os registros concluídos têm limites de evidência, incluindo revisões sem execução física Android.

A `PROPERTY-MATRIX.md` registra 45 schemas, 44 tipos no Add e 44 fachadas geradas. São números publicados no checkout, não uma nova medição executável desta análise.

O plano Forge propõe versões e tecnologias diferentes das atuais. Exemplo concreto: o checkout fixa miniaudio 0.11.23, enquanto a proposta Forge cita 0.11.25. Não confundir versão planejada com versão integrada.

## O problema que ainda precisa ser resolvido

`auditComponentContracts()` verifica identidades, referências, presença de consumidor e disponibilidade declarada de capacidades. Isso é útil, mas **uma string com o nome do consumidor não prova que ele usa cada propriedade corretamente**. O relatório de famílias também verifica arquivos e estados declarados; não executa o comportamento descrito.

Além disso, `ComponentType` descreve propriedades e recursos, mas não contém uma tabela equivalente de métodos e eventos. Operações comportamentais e APIs próprias precisam entrar no rastreamento de cobertura, sem serem transformadas artificialmente em propriedades.

O ganho está em completar essas ligações e reutilizá-las nas próximas famílias. Criar mais uma camada universal de reflexão desde o zero repetiria uma parte importante do investimento atual.

## Três caminhos possíveis

| Caminho | O que se reaproveita | Trabalho que permanece | Avaliação para este pedido |
|---|---|---|---|
| Astra atual com bibliotecas | Renderer, cena, editor, C#, assets, física, áudio e contratos existentes | Integração vertical e lacunas das famílias | Recomendação para preservar a engine atual |
| Produto sobre Godot | Sistemas que já compartilham runtime, recursos, cenas e ferramentas | Migração de projetos, adaptação de UX, extensões e manutenção do fork | Maior atalho de amplitude se aceitar mudar a base |
| Reconstrução Astra Forge | Algumas bibliotecas e partes portáveis da Astra | Novo mundo, renderer, bindings, UI, formatos e migração | Investimento de plataforma, não atalho automático para fechar o catálogo |

A documentação da Godot 4.5 lista limitações do editor Android, incluindo ausência de C#/Mono e Gradle e UX ainda não otimizada para celulares. Isso precisa ser reavaliado na versão que se escolher; não se deve transportar essas limitações para outra versão sem conferir. [Editor Android da Godot 4.5](https://docs.godotengine.org/en/4.5/tutorials/editor/using_the_android_editor.html).

GDExtension permite estender a Godot por bibliotecas nativas. Sua direção é código externo dentro da Godot; não fornece automaticamente os sistemas internos da Godot como bibliotecas independentes para a Astra. [GDExtension na Godot 4.5](https://docs.godotengine.org/en/4.5/tutorials/scripting/gdextension/what_is_gdextension.html).

## Como fechar o contrato comum

NÃO VOU IMPLEMENTAR DEPENDÊNCIAS DE FORMA CENOGRÁFICA.

Manter `ComponentType` e `ComponentSchema` como base. Ampliar apenas os aspectos exigidos por uma família real, começando por áudio. A divisão proposta é:

| Parte | Fonte de verdade | Derivação automática | Trabalho específico inevitável |
|---|---|---|---|
| Dados e propriedades | Descritores nativos existentes | API tipada, documentação, controles padrão, auditoria de IDs | Domínio válido, unidade, defaults e migração |
| Métodos | Contrato tipado ligado a funções reais | Assinaturas de API, ajuda e descoberta | Execução, erros, política de thread e efeitos |
| Eventos | Payload tipado e produtor identificado | Bindings e catálogo de eventos | Emissão, ordem, desconexão e lifetime |
| Referências e recursos | Bindings e identidades persistentes | Pickers, inventário de dependências e remapeamento comum | Importação, cache, recarga e destruição do recurso |
| Editor | Comandos e descritores compartilhados | Campos usuais e integração com Undo | Gizmos, timeline, gráficos e editores especializados |
| Evidência | Cenários executados e seus resultados | Relatório de cobertura e saldo | Asserção que observa o efeito real |

Um descritor de método precisa conhecer argumentos, resultado, erros, mutabilidade, fase e função efetiva. Um evento precisa conhecer payload, origem, entrega e comportamento após remoção, troca de cena e saída do Play. Referências a APIs próprias continuam válidas: uniformizar rastreabilidade não obriga reescrever toda API em um despachante genérico.

Exemplo de contrato proposto para uma propriedade:

```text
Identidade: TypeId + PropertyId estáveis
Semântica: tipo, unidade, default e domínio
Autoria: editável, animável, serializável, override de prefab
Aplicação: imediata ou no safe point; invalidação necessária
Destino: função de sincronização e estado consumido pelo backend
Falha: valor inválido, recurso ausente, handle obsoleto
Aceite: cenário que altera a propriedade e observa seu efeito
```

Não colocar nomes de cenários de teste nos dados persistidos do usuário. A associação de cobertura pode viver ao lado dos testes, referenciando os IDs de produção. A matriz é gerada a partir dessa associação; uma entrada manual não pode declarar que um cenário passou.

Para serialização, ampliar helpers onde os padrões já são comuns. Migrações entre versões e blobs de recursos continuam explícitos. Gerar um serializer novo para todos os tipos de uma vez adicionaria risco desnecessário aos arquivos existentes.

No caminho quente, resolver IDs durante criação ou mudança e usar acessos tipados em execução. Não percorrer strings e metadados para cada propriedade de cada objeto em todo frame. Nem a biblioteca nem a reflexão eliminam esse cuidado.

## O que extrair do código de outras engines

| Referência | Fato observado | Adaptação proposta |
|---|---|---|
| [Godot 4.5 ClassDB](https://github.com/godotengine/godot/blob/4.5/core/object/class_db.h) | Registro de propriedades, métodos, sinais e construção de tipos | Expandir os descritores Astra para cobrir comportamento e eventos, preservando o modelo atual |
| [Godot 4.5 RigidBody3D](https://github.com/godotengine/godot/blob/4.5/scene/3d/physics/rigid_body_3d.cpp) | `set_mass` valida o valor e chama `PhysicsServer3D`; o registro liga setter e getter | Rastrear a cadeia até o consumidor e testar efeito, não apenas a leitura da propriedade |
| [ezEngine Reflection System](https://ezengine.net/pages/docs/runtime/reflection-system.html) | Reflexão utilizada por ferramentas e serialização; registro explícito por macros | Descrições comuns podem reduzir duplicação sem depender de descoberta mágica do comportamento |
| [Godot 4.5 Audio buses](https://docs.godotengine.org/en/4.5/tutorials/audio/audio_buses.html) | Workflow de roteamento e efeitos de áudio | Especificar a família de mixer por operações utilizáveis, não apenas pelo nome AudioBus |
| [Recast 1.6.0 Sample SoloMesh](https://github.com/recastnavigation/recastnavigation/blob/v1.6.0/RecastDemo/Source/Sample_SoloMesh.cpp) | Exemplo real do pipeline de construção de navmesh | Portar a sequência necessária de bake e seus diagnósticos, ligada ao recurso Astra |

Fontes de engines inteiras servem melhor como referência de contratos e integração; algoritmos isolados e bibliotecas servem melhor para incorporação direta. Copiar uma classe da Godot que depende de Object, Variant, Resource, SceneTree e servidores pode importar um subsistema inteiro por acidente.

O próprio README de [UnityCsReference](https://github.com/Unity-Technologies/UnityCsReference) identifica o código como referência e restringe modificação/redistribuição. Usá-lo para estudar comportamento; não tratá-lo como biblioteca permissiva para incorporar. Para código incorporado, registrar origem, commit, licença e alterações locais. As referências funcionais do catálogo permanecem Unity 6000.0 e Godot 4.5; branches mutáveis de pesquisa não redefinem esse recorte.

## Bibliotecas com ganho concreto

As candidatas abaixo foram pesquisadas nos repositórios oficiais; não foram compiladas ou integradas nesta análise. Fixar um commit e a configuração exata antes de incorporar cada uma. Portabilidade declarada pelo projeto não comprova compatibilidade com o NDK, renderer ou lifecycle da Astra.

| Família | Escolha | Trabalho fornecido | Parte que a Astra ainda precisa fazer |
|---|---|---|---|
| Física 3D | Manter [Jolt](https://github.com/jrouwe/JoltPhysics), já integrado | Simulação, colisões e estruturas físicas | Authoring, sincronização, referências, edição em Play e teardown |
| Física 2D | Auditar implementação atual antes de escolher Box2D | Uma troca pode fornecer um solver 2D dedicado | Migração, formas, queries, contatos e editor; hoje o CMake identifica Box2D como benchmark, não backend do runtime |
| Áudio | Expandir [miniaudio 0.11.23](https://github.com/mackron/miniaudio/tree/0.11.23), já integrado | Device, reprodução, processamento e infraestrutura de áudio | Streaming de recursos Astra, prioridade de vozes, grafo persistente, snapshots e lifecycle Android |
| Animação esquelética | Avaliar [ozz-animation](https://github.com/guillaumeblanc/ozz-animation) | Loading, sampling e blending de animação | Importação preservando IDs, Animator graph, eventos, root motion, retarget quando previsto e ferramentas |
| Navegação | Integrar [Recast e Detour](https://github.com/recastnavigation/recastnavigation) | Bake, queries, caminhos, crowd e tile cache | Recurso versionado, jobs, cancelamento, agentes ligados ao motor, preview e invalidação |
| UI de jogo | Avaliar [RmlUi](https://github.com/mikke89/RmlUi) em uma superfície limitada | Layout, controles, estilo, eventos e dados de desenho | Renderer, toque, IME, acessibilidade, serialização e ligação aos comandos Astra |
| Efeitos | Avaliar [Effekseer](https://github.com/effekseer/Effekseer) | Ferramenta de autoria e runtime de efeitos; inclui caminho Vulkan | Passes, recursos GPU, profundidade, lifetime e controles realmente disponíveis no celular |
| UV para lightmap | [xatlas](https://github.com/jpcy/xatlas) | Unwrap e empacotamento de UV | Bake de iluminação, padding, cache, invalidação e importação da irradiância |
| Assets e geometria | Manter codecs e meshoptimizer existentes | Trabalho já incorporado ao pipeline | Consolidar perfis, GUIDs, reimportação e referências |

Ozz não entrega sozinho um Animator completo. RmlUi deixa input e renderização para a aplicação. xatlas não é um lightmapper. Effekseer com autoria externa pode acelerar efeitos, mas **não encerra a autoria móvel prevista no catálogo**. Cada limite permanece explícito no saldo da família.

Trocar o ECS por flecs ou o renderer por The Forge não fecha propriedades, prefabs, eventos ou recursos automaticamente. Só fazer essa mudança quando houver um problema demonstrado que compense migração e revalidação. Também não substituir C# por Luau como requisito para automatizar bindings: a Astra já gera fachadas C#.

## Como distribuir todo o catálogo

Estes lotes organizam as 91 identidades existentes sem criar uma nova contagem de conclusão. Um lote grande é entregue por segmentos verticais; só encerra quando todos os requisitos correspondentes tiverem evidência. Os IDs completos continuam no saldo original.

| Lote | IDs | Capacidade e dependências principais |
|---|---|---|
| Base e composição | F001 a F011, F076 a F078 | Identidade, cenas, prefabs, métodos/eventos, tween, curvas e caminhos |
| Recursos e representação | F012 a F021, F083 | Mesh, skin, instâncias, LOD, materiais, textures, targets e reimportação |
| Câmera e iluminação | F022 a F034 | Câmera/rig, decals, linhas, luz, sombra, ambiente, probes, bake, fog e água |
| Física | F035 a F045 | Corpos, formas, materiais, sensores, joints, personagem, queries, ragdoll e 2D |
| Entrada e UI | F046 a F057 | Actions, layout, controles, texto, IME, temas, acessibilidade e localização |
| Áudio | F058 a F061 | Clipe/stream, fonte, listener e mixer |
| Animação | F062 a F066 | Clips, player, grafo, rig e timeline |
| Navegação e lógica | F067 a F070 | Região, agente, obstáculos/links e composição de percepção |
| Mundo 2D e partículas | F071 a F075 | Sprites, tiles, câmera, luz 2D e emissão de partículas |
| Mundo e ferramentas | F079 a F082, F084 a F087 | Terrain, scatter, streaming, SaveGame, extensões, código, profiler e exportação |
| Módulos condicionais | F088 a F091 | Rede, XR, vídeo e extensões físicas |

A ordem prática é: contratos que bloqueiam trabalho → recursos/física/áudio já existentes → UI/animation/navigation → recursos gráficos e mundo mais caros. Há interdependências: ragdoll exige animação; timeline exige áudio/animação/câmera; navegação exige geometria e política de movimento; tiles exigem 2D e recursos; streaming exige ownership consistente. Não executar a tabela cegamente de cima para baixo.

O teto de escopo é o catálogo versionado. Atualizações de Unity/Godot alimentam pesquisa futura, sem aumentar continuamente o alvo de conclusão. Um subconjunto pode ser publicado com seu contrato, mas não encerra uma linha mais ampla silenciosamente.

## Primeiro pacote recomendado

Escolher **áudio F058 a F061** como piloto: já há biblioteca, consumidor, recursos, fachadas, testes e cenário anterior. O saldo identifica lacunas concretas, permitindo gastar esforço em integração em vez de descoberta de uma stack inteira.

1. Reconciliar a cadeia atual em `scene/audio.h`, `resources/audio_clip.*`, `runtime/scene_audio.cpp`, API e testes existentes. Registrar o que está comprovado e o que apenas está descrito.
2. Completar streaming e loop points com ownership do decoder, I/O fora do callback de áudio, cancelamento e política explícita para underrun. Streaming só conta se não exigir decodificar o clipe inteiro em memória.
3. Completar prioridade/preempção e mistura espacial contínua com resultado observável. Preservar identidade da voz e impedir resultados de operações antigas após troca de mundo.
4. Completar sends, efeitos/parametrização e snapshots do mixer sobre um grafo validado. Definir a lista de efeitos do pacote antes de expor controles; não prometer todos os efeitos de outra engine.
5. Ligar serialização, Undo, referências, API, editor e diagnóstico à mesma semântica. Migrar cenas anteriores de forma transacional.
6. Executar o cenário de aceite e registrar separadamente host, APK e dispositivo quando houver autorização de execução.

Cenário proposto: importar áudio → configurar fonte 3D, listener e buses → ajustar loop e roteamento → salvar e reabrir → reproduzir → mover a fonte → alterar mistura espacial e snapshot → exceder o orçamento de vozes → interromper/remover → sair e retornar do background → verificar ausência de vozes órfãs, travamentos e referências inválidas.

Uma cena principal pode conter várias asserções significativas. O teste automatizado de áudio pode observar sinais e estado real do backend; a escuta e o lifecycle no Android continuam como evidências distintas. Não criar centenas de testes de getters para fabricar cobertura.

## Evidência que realmente fecha uma cadeia

VOU TESTAR O QUE PROTEGE COMPORTAMENTO REAL.

Proposta de evolução do fluxo existente, ainda não implementada:

```text
Descritores e APIs reais
    → inventário de propriedades, métodos, eventos e recursos
    → associação aos cenários da família
    → execução selecionada por dependências alteradas
    → resultados com revisão, configuração e artefatos
    → relatório de lacunas
    → atualização revisada do saldo de famílias
```

O inventário deve detectar membro sem criação, API, persistência quando aplicável, consumidor ou cenário associado. Isso comprova estrutura. O cenário deve provar comportamento: a alteração chega ao backend e produz o efeito esperado; esse segundo passo não pode ser inferido por busca textual.

Exigir identidade do cenário, revisão da fonte e alterações locais relevantes, configuração de build, versão das bibliotecas, artefato executado e resultados. Em checkout sujo, só o commit é insuficiente; registrar hash do conteúdo efetivamente usado. Evidências históricas permanecem disponíveis, mas não se promovem automaticamente a evidências da revisão atual.

Quando um requisito não se aplica, registrar motivo concreto. Eventos transitórios não precisam ser serializados como ocorrências; conexões autoradas podem precisar. Estado de playback pode ser runtime; configuração de loop deve persistir. Um único checkbox de persistência não distingue essas coisas.

Não multiplicar cegamente todas as combinações de propriedades. Proteger invariantes, operações de remoção, remapeamento, migração, mudança durante Play e casos de referência ausente. Comparar os resultados observados com valores ou relações independentes do código que escreve o estado.

## Editor e fluxo de trabalho

NÃO IREI SER SIMPLISTA NO DESIGN.

A geração automática deve cuidar dos campos comuns e sua semântica. Mixer, curvas, caminhos, Animator e timeline precisam de superfícies próprias porque o usuário edita relações e tempo, além de valores escalares.

Proposta estrutural para avaliar na implementação: a seleção mantém o Inspector habitual e oferece entrada contextual para a ferramenta da família na mesma área expansível. O mixer pode mostrar roteamento e níveis quando aberto; a timeline usa espaço temporal; sair restaura seleção e contexto. Preservar o contrato landscape do editor atual. Evitar reimplementar toda a UI para inaugurar uma família.

Ações e feedback devem acompanhar estados reais: recurso ausente, alteração pendente, operação em andamento, erro recuperável e Play. O fluxo precisa permitir criar, configurar, observar, desfazer e salvar sem obrigar o usuário a entender os backends.

Esta é uma hipótese de interação, sem captura ou implementação nesta entrega. Na execução, comparar o fluxo atual em ambiente executável; gerar proposta visual se a captura mostrar superficialidade; integrar ícones ao pipeline real e conferir a captura posterior conforme AGENTS.md. A troca global de paradigma exige decisão de produto própria.

## Como reduzir o prazo sem reduzir a capacidade

Manter uma família em implementação e outra em pesquisa delimitada. Concluir uma parte funcional antes de abrir mais quatro integrações. Novas bibliotecas passam primeiro pelo menor caminho real: asset → runtime → alteração → persistência → destruição no alvo Android. Um exemplo isolado da biblioteca não substitui esse caminho.

Reaproveitar testes, fixtures e ferramentas existentes. Gerar API e documentação durante o mesmo fluxo de alteração dos contratos. Separar gerações de código das execuções de cenário se o executável atual de testes tornar o uso da ferramenta desnecessariamente pesado; não reconstruir o sistema de build sem medir esse custo.

Planejar limites de investigação, não promessas de entrega: por exemplo, até dois dias úteis para mapear o piloto e uma semana para demonstrar seu primeiro segmento completo. Se não fechar, registrar o bloqueio estrutural antes de iniciar outra família. Esses limites são uma proposta de cadência, não estimativas comprovadas da implementação.

Depois de dois ou três segmentos aceitos, estimar o restante por classe de trabalho: reconciliação, extensão sobre backend existente, integração de biblioteca e sistema novo. Não dividir 77 pelo número de semanas: uma família a auditar pode já existir e GI/streaming pode exigir muito mais que um componente simples.

A medida útil é capacidade utilizável e preservada após salvar/reabrir, com custo de manutenção e desempenho conhecidos. Não há base nesta inspeção para prometer uma data de paridade total. O mecanismo para evitar anos de trabalho repetido é preservar os sistemas conectados, incorporar algoritmos maduros e automatizar os contratos repetidos.

## Decisão operacional proposta

Aplicar primeiro a estratégia incremental à Astra atual, usando áudio para completar o fluxo de contratos e evidências. Aproveitar as pesquisas de Forge sem iniciar sua reconstrução completa. Preservar formatos, C# e renderer enquanto não houver uma justificativa concreta para substituí-los.

Se o objetivo mudar para máxima amplitude pronta com menor desenvolvimento de runtime próprio, decidir explicitamente pela base Godot e avaliar migração com um projeto representativo. Evitar uma solução intermediária que mantenha simultaneamente dois mundos, dois sistemas de recursos e dois runtimes de gameplay para sustentar a mesma cena.

Entrega desta análise: pesquisa, diagnóstico e estratégia de execução. Nenhuma capacidade do saldo foi promovida, nenhuma biblioteca foi integrada, nenhum teste foi executado e nenhuma alteração preexistente do código foi modificada.

# Execução da auditoria Unity × Astra — 30/09/2026

**Estado mais recente:** a seção “Segundo pacote” ao final atualiza as contagens, as dependências entregues e o APK. As seções anteriores registram etapas históricas e não substituem esse estado.

O objetivo continua ativo: aplicar a auditoria com abas de Componentes e Inspeção, cobertura de propriedades, caminhos de uso comparáveis aos da Unity e dependências funcionais. Esta entrega inicial não fecha o bloco de prefabs nem declara paridade com a Unity. O modelo principal implementou UI, API nativa e C#; um único subagente Sol com raciocínio baixo corrigiu o ciclo de Step e acrescentou a regressão da ponte ABI.

## Implementação desta rodada

| Cadeia | Mudança efetiva | Evidência e limite |
|---|---|---|
| Hierarchy → Inspector → Components → Inspection | Duas abas; lista rolável dos componentes reais; abertura por identidade de instância; alvo travado preservado; retorno ao conjunto; Add Component abre o catálogo existente | Testes de roteamento e sessão host passaram. Inspeção detalhada ainda usa o editor de propriedades existente, com paginação; não é a pilha completa da Unity |
| `enabled` → histórico → modelo → consumidor | Checkbox no cabeçalho e na lista, somente onde existe propriedade real; valor misto tem traço; ativação mantém a seleção externa ao Inspector travado | Regressão da sessão passou com Undo. Não é prova de todos os consumidores no Android |
| Preferência editorial → arquivo → reabertura | A aba escolhida fica em `.astra/editor-preferences.astra`; projeto sem preferência usa Inspeção | Regressão de Inspectors focados e reabertura passou. Nenhum estado de UI é usado como dado de runtime |
| Vector3 C# → ABI → GameWorld → componente | Uma operação atômica substitui três escritas escalares. Reutiliza `setComponentTriple`: valida todos os canais e o componente candidato antes de publicar | API gerada, GameWorld e callback real do ScriptBridge passaram nos testes dirigidos; não há fallback de três escritas |
| Step pausado → Update → física → LateUpdate → câmera | Step e avanço contínuo compartilham o mesmo ciclo. Comandos são drenados antes da câmera seguir a pose final | Regressão host com corpo Jolt real passou; pausa e autoria permanecem preservadas |
| Ícones → catálogo → atlas → UI | Dois SVGs novos (`editor/components`, `editor/inspection`), rasterização pelo pipeline existente e enum/atlas regenerados juntos | Prévia executável sem glifos/instâncias ausentes; atlas tem 199 entradas. Zero tipos novos de componentes foram declarados nesta rodada |

O protocolo SceneAccess passou de 19 para **20**, com `setTriple` no fim da estrutura nativa e gerenciada. Os dois lados exigem versão/tamanho compatíveis. É necessário distribuir o pacote nativo e C# juntos; o build Android desta rodada fez essa publicação conjunta.

## Comparação visual e julgamento

NÃO IREI SER SIMPLISTA NO DESIGN.

![Componentes — captura do executável host](C:/Users/donod/Downloads/atchengine/docs/validacao/evidencias/inspector-components-20260930/components-landscape.png)

![Inspeção — captura do executável host](C:/Users/donod/Downloads/atchengine/docs/validacao/evidencias/inspector-components-20260930/inspection-landscape.png)

A lista separa composição e navegação da edição detalhada. Componentes conserva Add Component no rodapé; em Inspeção com pouca altura, a ação passa ao cabeçalho para manter os campos acessíveis. Componentes indisponíveis são identificados e preservados, sem seta ou ação de navegação inerte.

**Correção de premissa:** o editor de cena da Astra funciona em horizontal. O Manifest usa `sensorLandscape`, e `EditorCodeInput.tick()` solicita portrait somente no workspace de código, restaurando a orientação da cena ao sair. A análise de portrait do Inspector surgiu de uma prévia host artificial, sem conferir esse contrato Android. O usuário corrigiu a premissa; a rota portrait experimental e seu teste foram retirados. Capturas portrait anteriores permanecem apenas como histórico de pesquisa, sem valor de aceite para o editor de cena. A IDE vertical segue o fluxo existente e não foi alterada.

![Componentes — landscape de telefone](C:/Users/donod/Downloads/atchengine/docs/validacao/evidencias/inspector-components-20260930/components-phone-landscape.png)

![Inspeção — landscape de telefone](C:/Users/donod/Downloads/atchengine/docs/validacao/evidencias/inspector-components-20260930/inspection-phone-landscape.png)

![Hipótese visual gerada — não é implementação](C:/Users/donod/Downloads/atchengine/docs/validacao/evidencias/inspector-components-20260930/conceito-gerado.png)

A geração serviu para confrontar agrupamento, navegação e densidade com a captura anterior. O checkbox de Transform desenhado no conceito não foi transplantado: Transform não possui habilitação independente com consumidor real. Valores e diagnósticos do conceito também não substituem os defaults e contratos do schema. Os ícones integrados são os SVGs do pipeline, não recortes desta imagem.

## Referências e observações

- [Unity 6000.0 — Using Components](https://docs.unity3d.com/6000.0/Documentation/Manual/UsingComponents.html): composição do objeto, criação via Add Component e habilitação dos componentes. Princípio aplicado: conjunto identificável, ação de criação acessível e propriedade conectada ao comportamento.
- [Unity 6000.0 — execução](https://docs.unity3d.com/6000.0/Documentation/Manual/execution-order.html): LateUpdate depois de Update, usado para acompanhar a pose final. Adaptação: o Step pausado executa o mesmo caminho vertical do avanço contínuo.
- [Unity 6000.0 — overrides de prefab](https://docs.unity3d.com/6000.0/Documentation/Manual/PrefabInstanceOverrides.html) e [ApplyPropertyOverride](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/PrefabUtility.ApplyPropertyOverride.html): override local prevalece sobre a fonte, Apply opera sobre propriedades e a pose da raiz tem tratamento especial.
- [Unity 6000.0 — Static Objects](https://docs.unity3d.com/6000.0/Documentation/Manual/StaticObjects.html) e [StaticEditorFlags](https://docs.unity3d.com/kr/6000.0/ScriptReference/StaticEditorFlags.html): flags pertencem a consumidores de pré-cálculo, não a uma promessa genérica de objeto estático. GI, oclusão, batching e reflexão continuam exigindo cadeias próprias na Astra.
- [Creating And Editing Prefabs in Unity — Gregory Osborne](https://www.youtube.com/watch?v=cbjRdGzgjVc), publicado em 06/06/2026: revisão de transcrição e quadros pausados em aproximadamente **8:31.94**, **8:51.98** e **9:12.02**, com avanço de um quadro nas duas últimas observações. Adição mostra composição relacionada; remoção mantém um registro visual do componente; o fluxo subsequente permite Revert/Apply. A versão exata do editor no título do vídeo não foi confirmada. É evidência de workflow, não contrato Unity 6000.0, e não foi analisado cada quadro dos 19:55 completos.

## Próximo corte vertical: prefab sem perda de autoria

O código atual desserializa `PrefabLink.base`, mas usa essa base apenas para conferir identidade e estrutura. A lista compara a instância diretamente com a fonte atual. Uma edição externa ainda não propagada pode, portanto, aparecer como override local. A correção e o Apply dependem de uma comparação em três vias por endereço:

| Base B, fonte S, instância I | Resultado |
|---|---|
| I = B | Campo herdado: receber a fonte nova |
| I ≠ B, S = B | Override local: preservar |
| I = S | Convergência: atualizar a base |
| I ≠ B, S ≠ B, I ≠ S | Conflito: preservar local na propagação; Apply exige escolha explícita |

O primeiro pacote precisa criar a fonte nova a partir de **S**, transferir apenas os endereços escolhidos de **I**, propagar campos herdados usando a base própria de cada instância e atualizar as bases em IDs da fonte. Referências precisam de mapas bijetivos por `(asset, instanceRoot)`; recursos transitórios precisam da mesma normalização em B/S/I. Coleções sem reflexão usam escopo atômico de componente, declarado na UI.

A publicação deve reunir fonte, registro, documento preparado e Undo/Redo, usando `EditorImportTransaction` e `EditorHistory::recordResource`, com hash dos **bytes reais** do arquivo e recusa de concorrência externa. O primeiro corte deve recusar mudanças estruturais, nested e variants até que seus próprios modelos e reconciliação existam. Propagação na cena aberta não prova atualização de cenas fechadas.

## Trabalho ainda necessário no objetivo completo

1. Expandir a inspeção em landscape, respeitando a orientação real do editor de cena, com capturas e estados de criação, seleção, erro e teclado. IDE vertical é uma superfície separada.
2. Comparação B/S/I, Apply seletivo e propagação; depois edição da fonte com breadcrumb/retorno já escolhida pelo usuário, composição e variantes. Cada pacote precisa persistência, Undo/Redo e referências válidas.
3. Cobertura por família de propriedades, componentes e APIs do atlas. Registrar implementado/parcial/pesquisado, incluindo checkboxes de referência solicitados pelo usuário, com estado e motivo quando a dependência não estiver disponível.
4. Consumidores reais de Static, GI/lightmap, oclusão, batching, navegação e reflexão. `render.gi.lightmap` continua Planned; esta rodada não criou bake, UV2, atlas, probes nem uma flag que simule esses sistemas.
5. Cenários integrados de save/reopen, Play/Stop, recursos e escala. Android visual e físico depende da liberação do aparelho pelo usuário após a implementação. Não pedir ADB antes disso.

## Validação executada

Builds `aether_tests` e `aether_ui_preview` passaram. Testes dirigidos passaram: atribuição vetorial do GameWorld, ponte SceneAccess v20, consistência da API C# gerada, fachada C# gerada, Step/LateUpdate/câmera, navegação da nova superfície, sessão com scroll/checkbox/Undo/catálogo, cadeado do Inspector e preferência/reabertura dos Inspectors focados. A falha inicial do cenário de rolagem foi corrigida: agora o cenário possui conteúdo comprovadamente maior que a janela, antes de exigir scroll. Na edição múltipla, abas e rodapé deixavam altura insuficiente para os campos: o Add Component passou para o cabeçalho em inspeção baixa, e o aviso do conjunto permanece na aba Componentes. O cenário de transformação usa Componentes → Transform → Inspeção, sem presumir que Transform e Luz compartilhem a mesma página da lista antiga. A validação final em landscape passou após a retirada do experimento portrait: edição múltipla, scroll/checkbox/Undo/catálogo, preferência e reabertura dos Inspectors focados e dois cenários de inspeção em Play. As capturas host de 853×394 e 1200×700 foram inspecionadas, sem instâncias descartadas nem fontes ausentes. A inspeção curta mostra Alvo e Deslocamento na mesma página, com uma única ação Add no cabeçalho.

`android/gradlew.bat :app:assembleDebug --console=plain` passou novamente com a UI final horizontal. APK: `android/app/build/outputs/apk/debug/app-debug.apk`, **242757067 bytes**, SHA-256 **E14DB89FDDB8FDAE5C7394DC14F5EB43A5DF6B2D9991F7930F165412FAF022C4**. Avisos existentes da rodada anterior: CS8981 dos nomes `math`/`quaternion` e discrepância de versão de XML do SDK. Não instalado, não aberto no aparelho, sem validação visual ou física no Android nesta rodada. Nenhum commit ou push foi feito.

O bloco completo permanece em andamento. Build, teste host e imagem conceitual não encerram suas dependências pendentes.

## Ampliação posterior — API, inspeção e lightmap externo

Este registro atualiza o estado anterior de lightmap e substitui o hash anterior do APK. O pacote foi dividido entre implementação C# (Sol 6.1, esforço baixo), design executável (Sol 6.1, esforço médio) e integração nativa/renderização. Não houve ADB, instalação, commit ou push. O checkout já continha alterações anteriores; este relatório não atribui todo o diff à ampliação.

### Capacidade entregue e contagem

Permanecem **15 schemas de componentes; zero tipos novos** neste pacote. As ampliações são capacidades dos sistemas existentes, não novas entradas fictícias no catálogo. MeshRenderer recebeu sete propriedades por slot: recurso de lightmap, habilitação, escala U/V, deslocamento U/V e intensidade. As sete caixas Static são referências desabilitadas com motivo, e não sete capacidades implementadas.

- API C#: Transform local/mundo, conversões afins pela cadeia de ancestrais, Translate/Rotate/RotateAround/LookAt, consultas tipadas de componentes e hierarquia, Mathf e RandomStream PCG32 com estado versionado. Acrescenta Awaitable.NextFrame/Seconds/SecondsRealtime/FixedUpdate e StartAsync com cancelamento ligado ao Behavior; IEnumerator permanece opção adicional sobre o mesmo scheduler. Matrizes de conversão preservam shear; a pose nativa continua limitada a TRS. Contratos, limitações e fontes: [API-RUNTIME-EXPANSAO-2026-09-30.md](API-RUNTIME-EXPANSAO-2026-09-30.md).
- Editor: Componentes → Objeto/Transform/instância → Inspeção; navegação anterior/próximo componente, alvo travado, catálogo por família e Static de referência. MeshRenderer → Lightmap usa recurso real, checkbox e propriedades refletidas por slot, com Undo/Redo e edição múltipla. Habilitação/textura juntas e pares U/V reduzem a rota a duas páginas em 853×394; em 1200×700 os campos ficam juntos. Ícone `lighting/lightmap` integrado ao atlas de 201 entradas. Fontes e capturas: [DESIGN-COMPONENTES-INSPECAO-2026-09-30.md](DESIGN-COMPONENTES-INSPECAO-2026-09-30.md).
- Renderização: lightmap externo de **irradiância indireta RGB linear**, aplicado ao difuso como E/π. A contribuição substitui o ambiente difuso; luzes diretas e especular de ambiente permanecem. Não é textura de albedo, iluminação pintada ou lightmap de iluminação total.

### Cadeia do lightmap

`MeshRenderer/slot → LightmapBinding → serialização v9 → textura por GUID → publicação linear/clamp → resolução UV1 → extensão de material → descriptor bindless → shader`.

O formato v9 salva recurso, estado e parâmetros por slot; leitura de v1–v8 mantém o lightmap desligado. A validação recusa valores não finitos e regiões fora do atlas. A importação/consolidação de submeshes preserva a vinculação. A sessão antecipa a textura ao reabrir, publica o recurso durante atribuição e resolve a edição em Play. O cache de geometria verifica uma vez UV1 finito, dentro de 0–1 e com área utilizável; não percorre os vértices a cada quadro. A extensão de material passa de 12 para 14 vec4 e os shaders embarcados foram regenerados.

A tabela de extensões conserva o limite existente de 1024 desenhos por publicação, compartilhado com UV/canais/oclusão. A ampliação acrescenta 32 KiB ao buffer dessa tabela; excedentes continuam sem extensão e geram diagnóstico do renderer. Não foi medida a memória/residência adicional das texturas ou a GPU Android. Cenas grandes precisam respeitar esse limite até uma ampliação própria do publicador.

`render.gi.lightmap` passa de **Planned** para **DeviceLimited**, exclusivamente para esse contrato externo. Backend sem bindless mantém ambiente e emite diagnóstico explícito. O editor informa o requisito. Não há bake nativo, unwrap, geração/empacotamento de atlas, probes, atualização automática ao mover geometria/luzes nem verificação de sobreposição/padding UV. O autor deve fornecer dados compatíveis e refazer o preparo externo quando a cena mudar. A verificação do perfil de cor do arquivo e a qualidade luminosa final continuam responsabilidade da preparação e da validação visual.

Referências concretas: [Unity 6000.0 — Renderer.lightmapScaleOffset](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Renderer-lightmapScaleOffset.html), [Godot 4.5 — Using LightmapGI](https://docs.godotengine.org/en/4.5/tutorials/3d/global_illumination/using_lightmap_gi.html) e [Godot 4.5-stable — implementação LightmapGI](https://github.com/godotengine/godot/blob/4.5-stable/scene/3d/lightmap_gi.cpp). Princípios extraídos: UV dedicado, associação persistente ao recurso e transformação de atlas por renderizador. A Astra implementa agora consumo de resultado externo; não reivindica o baker dessas engines.

Para usar: importe a malha com `TEXCOORD_1` dedicado e a textura indireta como recurso de projeto. Selecione o objeto em Componentes, abra Malha → Lightmap e escolha o slot. Atribua a textura, configure escala/deslocamento da região e intensidade, e ligue Receber lightmap indireto. Um atlas completo usa escala (1,1) e deslocamento (0,0); regiões menores devem ser reduzidas antes de deslocadas para preservar o limite 0–1 durante cada edição. Salve a cena. A textura é vinculada por GUID, e desligar o checkbox preserva sua configuração para reativação posterior. UV inválido aparece no Inspector e impede a contribuição, sem alterar a malha original.

### Evidência e limites

- Host C++: build de `aether_tests` e `aether_ui_preview` passou. Cenário integrado importa GLB com UV1, atribui textura pela sessão, configura atlas, verifica material e payload GPU, migração v8/v9, recusa de valores inválidos, Undo/Redo, save/reopen e republicação. Também percorre Componentes → MeshRenderer → Lightmap, clica no checkbox real, aplica reset e Undo, edita V sem alterar U e limpa/reseleciona a textura no picker real. **1/1 passou** na versão final.
- Regressões nativas dirigidas passaram: API C# gerada, contratos dos componentes, superfície Componentes, seleção múltipla e superfície do editor. Filtros adicionais: **18/18 `r4_`** (materiais/texturas) e **15/15 `inspector`**. Um teste antigo de ações de objeto presumiu que Criar filho vazio estava na primeira página; foi corrigido para navegar pela paginação real antes do toque. Não foi executada a suíte nativa inteira.
- C#: build isolado final sem avisos/erros e filtro `Astra` passaram, **47 testes, zero falhas, zero pulados**. Quatro testes novos da API e três cenários integrados de Awaitable/Coroutine usam `ISceneAccess` controlado pelo teste; o compilador de projeto e BehaviorWorld são reais. Cobrem thread, espera por delta/relógio real, passo físico seguinte mesmo quando agendado dentro de FixedUpdate, cancelamento por token/hierarquia/Stop, erro isolado, descarte de enumeradores, reentrância e handles de outra sessão. Não são prova de execução Android. Os dois avisos CS8981 de nomes existentes ocorreram em build anterior, não no build incremental final.
- Todos os shaders embarcados foram regenerados com sucesso. Isso verifica compilação dos shaders, não qualidade visual ou desempenho na GPU do aparelho.
- Android: `:app:assembleDebug` passou novamente com o layout refinado e Awaitable. A publicação C# usa intermediários em `app/build/managed-artifacts`, evitando colisão com builds host e DLL mapeada, sem encerrar processos externos. APK final: `android/app/build/outputs/apk/debug/app-debug.apk`, **242798906 bytes**, SHA-256 **D29B8F00C256D0E980AD1BCAC2C42D83421AAF1A6BACD9B94D1F0DBA72D15B42**.

O scheduler é da sessão, reutiliza o dispatch nativo de Update/FixedUpdate e cancela seus recursos no lifecycle. As primitivas Awaitable retomam no thread do runtime; Tasks arbitrárias, I/O externo e async void não recebem essa garantia. Use StartAsync e propague o token para observar erros e cancelamento. Não há serialização de execução suspensa nem Time global/escala de tempo nova.

O APK não foi instalado ou executado no aparelho. Capturas host comprovam a UI executável; extração de material e payload não comprovam lightmap visual Android. O conjunto completo dos planos permanece parcial: bake e dependências de GI, consumidores Static e as demais famílias do atlas ainda exigem implementação própria. Não há paridade Unity/Godot declarada.

## Segundo pacote — constraints, forças, prefab, Save e Time

Este pacote foi implementado por duas frentes de código Sol 6.1 em esforço baixo, uma frente de design Sol 6.1 em esforço médio e a integração/código nativo do agente principal. A sessão permite quatro agentes simultâneos incluindo o principal, portanto não foram usados quatro subagentes. Nenhuma frente usou ADB.

### Capacidade e contagem real

O registro passou de **15 para 20 schemas**: **19** podem ser anexados por Add Component; Comportamento usa a rota de código com script escolhido. A fachada C# gerada contém 19 tipos; Behavior conserva a própria API. O catálogo Criar contém **37 entradas**, cinco novas receitas, e o atlas visual integrado contém **206 ícones**, cinco novos. A [matriz de propriedades](../componentes/MATRIZ-PROPRIEDADES.md) foi regenerada por introspecção dos descritores finais, incluindo consumidor, domínio, default e persistência. Essas contagens não representam paridade com o inventário de pesquisa.

| Entrega | Caminho real e limite |
|---|---|
| Position, Rotation, Scale e Aim Constraint | Modelo tipado → reflexão/Undo → cena/prefab → GameWorld → SceneConstraints → pose de mundo. Fonte única, peso e canais; remapeamento de referência, diagnóstico, gizmo e receita reais. Sem múltiplas fontes, ParentConstraint, Lock ou up-object. Rotation/Aim usam a convenção Euler da Astra. [Contrato e evidência](CONSTRAINTS-2026-09-30.md). |
| ConstantForce | Requer corpo dinâmico; vetores mundo/local de força e torque lidos após FixedUpdate em cada subpasso do Jolt. Propriedades ao vivo, lifecycle, save/reopen, gizmo e receita de propulsão. [Contrato](FORCAS-LIGHTMAP-UV-2026-09-30.md). |
| Apply seletivo de prefab | B/S/I → preflight de todas as instâncias da fonte na cena aberta → remapeamento → publicação transacional de fonte/registro → propagação seletiva → Undo/Redo. Preserva overrides e conflitos ao salvar/reabrir. Mudanças estruturais, nested, variantes e cenas fechadas continuam fora do consumidor entregue. [Contrato](PREFAB-APPLY-PROPAGACAO-2026-09-30.md). |
| Save | Behavior.Save → SaveStore do projeto em `.astra/save` → snapshot tipado, arquivos, lease de escritor e substituição atômica com backup. Quotas, paths e versões recusam operações inválidas. Não é storage global de jogo exportado ou sandbox C#. [Contrato](API-SAVE-2026-09-30.md). |
| Time | Relógio por BehaviorWorld alimentado pelos deltas reais do host; Update/FixedUpdate/LateUpdate, scheduler, thread e término de sessão. Sem TimeScale ou UnscaledTime, pois a escala integrada de física/timers/mundo ainda não existe. [Contrato](API-TIME-2026-09-30.md). |
| Lightmap externo | A auditoria UV1 agora detecta área sobreposta, faces degeneradas e coordenadas inválidas; orçamento esgotado é estado explícito e impede contribuição. Cache na publicação da geometria, sem testes de triângulos por quadro. Não certifica padding, charts entre submeshes ou bake. [Contrato](FORCAS-LIGHTMAP-UV-2026-09-30.md). |

Os cinco tipos novos têm referências oficiais versionadas nos próprios schemas: [Unity 6000.0 — ConstantForce](https://docs.unity3d.com/6000.0/Documentation/Manual/class-ConstantForce.html) e os quatro manuais de constraints, além do código oficial `UnityCsReference/6000.0`. Os documentos de cada pacote registram a comparação com defaults, propriedades, arquitetura, editor e limites. O estudo do fluxo visual reutiliza um trecho de vídeo realmente observado, com essa fronteira explicitada na [documentação de design](DESIGN-PREFAB-CONSTRAINTS-2026-09-30.md).

### Editor e refinamento executável

NÃO IREI SER SIMPLISTA NO DESIGN.

A comparação de prefab ocupa temporariamente a superfície ampla: endereço e origem, valores B/I/S, escopo da publicação e ação explícita Substituir fonte em conflito. Constraints organizam Influência e Ajustes; Fonte/Peso ficam próximos, eixos XYZ compartilham espaço conservando setters e reset individuais. ConstantForce organiza Mundo/Local com Força e Torque em cada destino, unidades N/N·m e ativação real no cabeçalho. A proposta preserva o viewport e a identidade Astra; não acrescenta docks permanentes para cada família.

Quinze capturas executáveis foram inspecionadas em 853×394 e 1200×700, com zero fontes ausentes e instâncias descartadas. As primeiras capturas revelaram abas de força truncadas e paginação desnecessária; o layout final corrigiu ambas. O agente principal também inspecionou comparação em conflito, ajustes de constraint e os dois espaços de ConstantForce. Capturas estão em `docs/validacao/evidencias/inspector-components-20260930/*-wave2.png`. São cenas/estados de autoria no host, não prova visual de execução em Play ou Android. Nenhuma imagem conceitual é tratada como implementação.

### Validação consolidada e correções

- Build nativo final de `aether_tests` e `aether_ui_preview`: passou, com warnings tratados como erros.
- Filtros nativos dirigidos: **41/41 componentes**, **15/15 Inspector**, **14/14 prefab**, **5/5 constraints**, **3/3 ConstantForce** e **2/2 lightmap**. Há sobreposição entre filtros; não se soma isso como testes únicos. Não foi executada a suíte nativa completa.
- Constraints: a baseline foi corrigida para conservar repouso/output em espaço local e recompor a base de mundo pelo pai a cada avanço. O cenário com pai em movimento verifica peso parcial estável, além de fontes, cadeia, autoridade, ciclo, serialização, clone, seis eixos de mira, receitas, história e gizmos.
- A regressão de migração MeshRenderer v1 continha versão/cauda v8 fixas; a fixture passou a construir o payload legado explicitamente a partir dos campos v1 e substituir o registro atual completo. A leitura de arquivo antigo e aquisição de identidade continuam verificadas com v9.
- C#: build isolado passou, zero erros e dois avisos CS8981 preexistentes (`math`/`quaternion`). Filtros **Astra 49/49** e **SaveStore 3/3**, total **52 testes distintos**. Save usa filesystem real; Time usa ProjectCompiler/BehaviorWorld reais; as interfaces de cena controladas dos testes da API não são prova de runtime Android.
- Android final `:app:assembleDebug`: **BUILD SUCCESSFUL em 26 s**, 40 tarefas, seis executadas e 34 atualizadas. APK: `android/app/build/outputs/apk/debug/app-debug.apk`, **262425035 bytes**, SHA-256 **41ED123548DCE98B60FCCD2EFC89E335A58F7FAFF62D528A49BBFC235D4D41B0**. Esse APK inclui o último ajuste de baseline e o layout final Mundo/Local.
- `git diff --check` passou para fontes, testes, matriz e documentos rastreados; avisos de conversão LF/CRLF não são erros. O checkout conserva alterações anteriores e artefatos de build. Nenhum commit ou push foi feito.

### Estado do objetivo completo

Os pacotes acima têm implementação e evidência host, com Android compilado. Não há evidência de instalação, toque, resultado visual de lightmap ou desempenho físico/GPU no aparelho. A autorização de ADB continua pendente e não foi solicitada antes da conclusão desta implementação.

O conjunto integral dos planos **não está concluído**. Continuam necessários bake/unwrap/atlas e probes de GI, consumidores dos sete Static, famílias restantes do atlas, prefab estrutural/nested/variantes, fontes múltiplas de constraints, escala integrada de tempo, exportação e qualificação combinada. Os checkboxes Static seguem visíveis como referência desabilitada com motivo, sem transformar ausência de consumidor em flag executável. O universo de pesquisa inclui 575 Component Unity e 271 Node/414 Resource Godot; não existe correspondência de um para um com os 20 schemas Astra, portanto não se publica uma contagem fictícia de “restantes”.

## Terceiro pacote aplicado — áudio, física 2D e transformação

Continuação por famílias com duas frentes de código Sol 6.1 low, uma de design Sol 6.1 medium e código/integração do principal. Quatro agentes simultâneos incluindo o principal, conforme o limite disponível. Nenhum ADB, commit ou push nesta rodada. O checkout anterior foi preservado.

### Contagem e capacidades verificadas

Registro **20 → 30 schemas**, **29** anexáveis por Add Component, **29 fachadas C#** geradas; Behavior conserva sua rota de código. Catálogo Criar **37 → 51 entradas**, atlas **206 → 217 ícones**. Joint2D possui quatro modos consumidos, mas conta como um componente, não quatro classes fictícias. AudioClip conta como recurso, não como componente. A [matriz de propriedades](../componentes/MATRIZ-PROPRIEDADES.md) foi regenerada dos descritores finais.

| Tipos novos | Cadeia e fronteira real |
|---|---|
| AudioSource, AudioListener, AudioBus | WAV validado → importação transacional/registro/GUID → componente/reflexão/histórico/archive → SceneAudio → miniaudio 0.11.23 → dispositivo. Gain, mute, solo, pitch, loop, cursor, listener prioritário, atenuação, cones e Doppler são consumidos. Foco Android real controla a saída, com demanda apenas de vozes em reprodução (Paused/Stopped/EOF liberam a demanda); SAF copia sem sobrescrever. DSP offline validado; reprodução física e latência Android não medidas. Sem streaming, MP3/FLAC, reverb ou efeitos de mixer. [Contrato](AUDIO-2026-09-30.md), [Android](ANDROID-AUDIO-FOCUS-WAV-2026-09-30.md). |
| Body2D, Collider2D, Joint2D, ConstantForce2D | Dados tipados/reflexão/Undo → ScenePhysics2D → Box2D 3.1.1 → pose XY e eventos. Três formas reais, sensores, massa/inércia, camadas, queries, motores/limites/molas em Weld/Revolute/Prismatic/Distance e força contínua por passo. ABI **21**, World.Physics2D e fachadas geradas. Mundo independente de 3D; plano XY, Z conservado e tilt recusado. Backend gameplay **experimental**; gates B/C da ADR-013 e performance mobile ainda pendentes. Sem mundo 2D visual completo, character2D, tilemap ou polygon authoring. [Contrato](PHYSICS2D-2026-09-30.md), [API](PHYSICS2D-API-ABI-2026-09-30.md). |
| ParentConstraint, LookAtConstraint, TransformTween | Canais/offsets/peso → runtime → pose real, sem herdar escala no Parent. LookAt orienta +Z com up e roll. Tween interpola TRS local, delay, easing, loop/pingpong; retarget ativo sem salto, conserva cancelamento, respeita física e writers habilitados. Reiniciar/Cancelar são ações reais na Inspeção em Play; respeitam o alvo travado. Persistência, remap, gizmos e receitas usam os consumidores existentes. Fonte única; Euler autorado; sem arbitragem por canal ou API C# dedicada de sequências. [Contrato](CONSTRAINTS-TWEEN-2026-09-30.md). |

Todos os tipos novos registram referência oficial versionada Unity **6000.0** ou Godot **4.5** no schema. Os documentos acima incluem API, código-fonte, ownership/lifecycle, editor e limites; o [documento de design](DESIGN-AUDIO-PHYSICS2D-TWEEN-2026-09-30.md) registra o workflow comunitário observado e as adaptações para toque. Não se afirma equivalência integral com essas engines.

### Editor e validação consolidada

NÃO IREI SER SIMPLISTA NO DESIGN.

Áudio organiza Som/Tocar/Espaço/Cone com condicionais e seletor WAV real; Física 2D compacta pares XY, e juntas priorizam modo e corpo conectado antes de parâmetros secundários. Tween organiza Tempo/Destino/Repetição e diferencia o pedido persistido do status observado; duração/curva e atraso/autoplay compartilham linhas mantendo setters/reset originais. As ações Play e importação em Edit têm rotas e estados efetivos. Diagnósticos seguem o objeto inspecionado, incluindo travamento do Inspector. Onze SVGs novos foram rasterizados no atlas real; nenhuma imagem conceitual conta como implementação.

As primeiras capturas mostraram Tween sem propriedades no telefone e ordem ruim de Joint2D. Ambos foram refinados; as capturas finais estão em `docs/validacao/evidencias/inspector-components-20260930/*-wave3.png`. O documento de design registra 28 capturas finais: 22 landscape 853×394, quatro 1200×700 e dois stress cases portrait 394×853. Portrait conserva propriedades e ações, mas apresentou sobreposição da toolbar e viewport estreito; não há promessa de adaptação portrait integral. São evidências executáveis de autoria/Inspector no host, não prova visual ou sonora no Android.

- Build final nativo de `aether_tests` e `aether_ui_preview`: passou com warnings tratados como erros. O teste grande de sessão ultrapassou o limite COFF; recebeu `-Wa,-mbig-obj` somente no MinGW, sem alterar o runtime Android.
- Filtros dirigidos: **physics2d 8/8**, **constraints 7/7**, **tween_delay 1/1**, **audio 4/4**, **component_ 42/42**, **inspector 15/15**, **play_ 39/39**. Alguns filtros compartilham cenários, portanto as contagens não são somadas como testes distintos. A suíte nativa integral não foi executada.
- Física 2D: solver e queries reais, stale handles, quatro modos de junta, força local/zero-sleep e receita → Undo/Redo → archive/reopen → edição Play → destruição/Stop/restart. A receita de pivô captura a âncora mundial na posição criada, evitando deslocar o corpo para a origem.
- Áudio: RIFF/chunks/quadros PCM limitados, DSP real offline, ganho em cadeia, mute/pitch, pausa de saída e de voz, EOF sem restart, remoção, GUID, filesystem/journal e reabertura pelo protocolo real do shell. A validação encontrou saída residual após remover a última voz; o contrato de saída inicializada sem vozes agora produz silêncio completo e mantém erros de formato/Stop. A fixture de reabertura foi corrigida para ler `.astra/assets.astra` e usar `loadAssets`, como o shell.
- C# isolado em `build/expansion-wave3-20260930`: **zero erros**, dois avisos CS8981 preexistentes. **Astra 49/49**, **Save 5/5**; filtros com sobreposição. Physics2D e as 29 fachadas foram compiladas. O cenário ABI nativo usa consumidor Box2D real e um serviço CLR de captura; isso não constitui prova de execução de jogo C# Physics2D no aparelho.
- Android final `:app:assembleDebug`: **BUILD SUCCESSFUL em 11 s**, 40 tarefas, seis executadas. APK **264165687 bytes**, SHA-256 **A2706C37D08C4F05E29DFBB577954E463C5A3D8C577B6552033EB3999ABF88A8** em `android/app/build/outputs/apk/debug/app-debug.apk`.
- `git diff --check` passou; avisos LF/CRLF não são erros. Fontes e artefatos anteriores continuam no checkout, sem publicação Git.

### O que ainda falta no objetivo integral

Há capacidade executável adicional, com contagem auditável, mas **todos os componentes do atlas ainda não estão implementados**. A referência não é uma lista de classes para expor sem consumidor. Restam famílias grandes: UI de jogo, mundo visual 2D/tiles/character, navegação, partículas/trails, curvas/splines, animação avançada e os demais pacotes do roadmap. Também permanecem os limites registrados anteriormente de GI/bake/probes, Static, prefab estrutural/variantes, tempo global e exportação. A qualificação física do pacote novo (áudio/foco, toque, performance e thermal) depende do aparelho; ADB continua sem uso. Nenhuma dessas lacunas foi mascarada por componente vazio, checkbox ativo sem efeito ou declaração de paridade.

## Quarto pacote aplicado — P15a, Curve3D, Path e PathFollow

Continuação por duas frentes de código Sol 6.1 low, uma frente de design Sol 6.1 medium e código/integração do principal. As quatro vagas incluem o principal. O pacote implementa autoria e execução de caminhos; não transforma todo P15 em concluído. Nenhum ADB, instalação, commit ou push. Alterações anteriores do checkout permanecem preservadas.

### Contagem e cadeia executável

Registro **30 → 32 schemas**, **31** anexáveis em Add Component, **31 fachadas C#** geradas, Behavior na rota de script existente. Criar **51 → 54 receitas**, atlas **217 → 221 ícones**. Curve3D inline é um valor tipado de Path, não uma terceira classe anexável ou um recurso externo por GUID. As contagens foram conferidas na matriz gerada, nas fachadas, no catálogo de criação e no atlas executável.

`Editor/IDs de pontos → Path/payload v1 → histórico e archive/prefab → GameWorld → bake mundial → ScenePaths → pose → diagnóstico e Inspector`. O fluxo de API passa pela ABI **22**, com dois callbacks acrescentados ao fim da estrutura nativa e gerenciada. Mutação de Play conserva o documento autoral intacto.

| Capacidade | Implementação e limite |
|---|---|
| Curve3D e Path | Bézier cúbica, até 128 pontos, handles locais relativos, IDs e allocator persistentes; inserir/editar/remover/reordenar com operações atômicas, Undo/Redo e save/reopen. Cache adaptativo de comprimento/amostragem separado dos dados autorais. Limites de profundidade/amostras são explícitos; tolerância não é garantia de erro global. [Dados](P15A-CAMINHOS-CURVE3D-2026-09-30.md). |
| PathFollow | Referência tipada ao caminho, velocidade mundial ou duração, distância inicial, loop, sentido inverso, orientação +Z e offsets em frame de percurso. Transformação da curva para mundo antes do bake; escala do caminho altera comprimento sem reinterpretar velocidade. Inativo congela; Stop preserva progresso; Restart reinicia. Lifecycle, dependências, ciclos e escritores concorrentes são tratados. [Runtime](PATHS-2026-09-30.md). |
| API C# | CurvePath por identidade: Count/At/ById/Insert/Set/Remove/Move/Closed/Sample. PathFollower: Restart/Stop/Progress/IsPlaying. Handles validam mundo, geração e instância; callbacks indisponíveis retornam erro, sem execução simulada. A fachada PathComponent evita ambiguidade com System.IO.Path. [API e ABI](PATHS-API-ABI-2026-09-30.md). |
| Prefab | Apply seletivo numérico exige correspondência de IDs, ordem e allocator entre base, instância e fonte em todas as instâncias carregadas. Reorder estrutural recusa publicação para não aplicar o índice a outro ponto. Revert integral continua disponível. Não foi criado um formato fictício de diff estrutural por ID. |

Referências concretas: [Godot 4.5 Curve3D](https://docs.godotengine.org/en/4.5/classes/class_curve3d.html), [Path3D](https://docs.godotengine.org/en/4.5/classes/class_path3d.html), [PathFollow3D](https://docs.godotengine.org/en/4.5/classes/class_pathfollow3d.html) e [source oficial 4.5-stable](https://github.com/godotengine/godot/blob/4.5-stable/scene/3d/path_3d.cpp). Princípios extraídos: dados autorais separados do cache, handles relativos e progresso em distância. Astra adapta com referência explícita entre objetos, IDs estáveis, velocidade no mundo e ferramentas contextuais para toque. O [documento de design](DESIGN-PATH-PATHFOLLOW-2026-09-30.md) registra também o trecho de vídeo comunitário efetivamente observado, sem reivindicar a extrusão demonstrada nele.

### Workflow e revisão visual

NÃO IREI SER SIMPLISTA NO DESIGN.

Criar → Curva de percurso abre a superfície Pontos; seleção por ID, lista paginada, anterior/próximo, inserir/apagar/reordenar e alternância Posição/Tangentes compartilham a área de Inspeção. XYZ usa o teclado real com objeto/instância/revisão/pointID, rejeitando commits antigos após reorder ou remoção. O viewport mostra curva, pontos e alças selecionadas; gizmos deste recorte são visualização, sem drag direto das alças.

Criar → Seguir percurso ou Câmera de percurso compõe os componentes reais. Inspeção → Percurso prioriza referência e modo, seguido de distância/velocidade ou duração; Orientação reúne offsets em XYZ, e Execução reúne autoplay/repetição/sentido e comandos reais de Play. O diagnóstico acompanha o objeto inspecionado, inclusive com Inspector travado. Autoria estrutural de Path fica bloqueada em Play; edição explícita do mundo por API permanece disponível.

A captura inicial de PathFollow no telefone deixou todos os campos fora do espaço útil, embora o contador de recortes fosse zero. Foi corrigida a composição da superfície e depois reduzida a navegação com pares de controles e frame XYZ. A [hipótese visual gerada](../validacao/evidencias/inspector-components-20260930/concept-path-follow-wave4.png) e seu [prompt exato](../validacao/evidencias/inspector-components-20260930/concept-path-follow-wave4.prompt.txt) permanecem separados das capturas executáveis. A imagem inclui curva ilustrativa junto de diagnóstico de caminho ausente; essa associação não foi implementada. O provider real exige referência válida. Nenhum controle do conceito foi integrado sem modelo/ação/consumidor real.

### Fronteira do pacote

Não há recurso Curve3D externo/compartilhado por GUID, tilt/roll por ponto, parallel transport, extrusão, scatter, navegação de obstáculos ou edição das alças por drag no viewport. Loop em curva aberta teleporta ao início; tangente vertical com orientação ativa diagnostica singularidade e conserva pose/progresso. Não há arbitração por canal entre animação, física, constraints e follower; conflitos são explícitos. O cache tem limites por curva, mas o custo combinado de muitas curvas ainda não foi qualificado no aparelho.

P15a amplia capacidades reais; **todos os componentes do atlas e todos os planos ainda não estão concluídos**. Continuam necessários os demais pacotes e os limites anteriores de GI/bake/probes, consumidores Static, prefab estrutural/variantes, escala de tempo e exportação. Não há declaração de paridade Unity/Godot. Resultado visual/físico, toque, áudio, performance e thermal Android continuam sem evidência nesta rodada.

### Validação executada

- UI executável: **22 capturas finais**, 16 em 853×394, quatro em 1200×700 e dois stress cases 394×853. Zero glifos ausentes e instâncias descartadas; paisagem com zero recortes. O agente principal inspecionou tangentes, as três composições de Follow em Play, a superfície ampla e retrato. Retrato apresenta seis recortes em cada caso, sobreposição da toolbar global e viewport de 180 px; não se declara adaptação portrait completa. [Manifesto com hashes das capturas e preview](../validacao/evidencias/inspector-components-20260930/path-wave4-manifest.json). São evidências da UI nativa executável no host; a fixture de Play consulta ScenePaths real, mas não comprova o frame Vulkan do jogo Android.
- Host C++: build final de `aether_tests` e `aether_ui_preview` passou. Doze cenários novos dirigidos passaram: matemática de bake, identidade/history/archive, três cenários de criação/input de pontos, quatro de ScenePaths, ABI/handles, prefab e um de input PathFollow. O cenário de sessão no telefone aciona números dos pares, modo condicional, Frame XYZ, checkbox e Undo/Redo; entra em Play pelo caminho público, aciona Stop/Restart e verifica status do consumidor real sem alterar a revisão autoral.
- Filtros executados: `curve3d` **1/1**, `paths_` **5/5** (quatro novos de runtime e um legado de asset paths), `path_` **6/6** antes do último cenário de input, `prefab_` **15/15**, `inspector` **16/16**, `component_` **42/42**, `play_` **39/39**, `physics2d` **8/8**, `tween_delay` **1/1**. O último cenário `session_path_follow_phone...` passou **1/1** após o layout composto final. Filtros se sobrepõem e não são somados. A suíte nativa integral não foi executada.
- O primeiro resultado do novo teste de PathFollow falhou porque o helper da fixture apenas avançava páginas ao procurar um campo anterior. A fixture foi corrigida para voltar via `PropertyPrevious`, mantendo o input real e todos os asserts. A revalidação passou; não se removeu uma garantia para ocultar a falha.
- C# isolado em `build/expansion-wave4-20260930`: build final **zero erros e zero avisos**; `Astra` **49/49** passou. As 31 fachadas e a API de caminhos foram compiladas. Dois CS8981 preexistentes ocorreram no build inicial. O cenário ABI nativo captura callbacks usando um serviço CLR de teste e consome GameWorld/ScenePaths reais; não prova um jogo C# PathFollow executado no aparelho.
- Android final com pares de controles e unidades: `:app:assembleDebug` **BUILD SUCCESSFUL em 22 s**, 40 tarefas, seis executadas e 34 atualizadas. [APK](../../android/app/build/outputs/apk/debug/app-debug.apk) **264335954 bytes**, SHA-256 **F21EF58E17DDDEAE3644E8036D36FB31CF64B1657F203CD5E15467D443AFBF96**. Não instalado nem executado no aparelho. A correção posterior alterou somente o teste host, não o produto empacotado.
- `git diff --check` passou nos fontes/documentos rastreados; novos textos autorais passaram UTF-8/whitespace. O header upstream miniaudio contém espaços finais preservados, fora desta validação de estilo autoral. Nenhum arquivo anterior foi limpo/resetado para reduzir o diff.

## Continuação com ADB autorizado — qualificação P15a

Após o pedido “continue adb on”, o APK atualizado foi instalado preservando os projetos. Xiaomi 25053PC47G/API36, paisagem 2772×1280. SHA-256 local e do APK instalado: **02A4C8C0F5E0C8491080FBAEA9B5912F68427F12FC6E4306A9E78C4EA8B8FF06**, 264336812 bytes. Build Android passou em 1m14s; build host passou; filtros paths_ 5/5, path_ 7/7 e mesh_collider_visual_uses 1/1 passaram.

Corrigidos cache de falhas de bake, diagnóstico de autoridade de descendentes físicos, identidade completa do ponto destacado e filtros de visibilidade/seleção dos gizmos. Cenários existentes foram ampliados para recuperação da curva, Body2D descendente, dois caminhos com IDs iguais e hits antigos sobre objetos ocultos/bloqueados. Nenhum componente novo foi contado por essas correções.

O projeto **P15a-ADB-20260930** foi criado pelo shell. Criar Path/Follow, editar X/tangente pelo teclado, Undo/Redo, salvar/reabrir e executar um Behavior C# pela ABI22 passaram no aparelho. Logs registram avanço real de pose/progresso, Stop mantendo ambos e Restart voltando a avançar. Um cubo filho foi renderizado em movimento; seus componentes físicos precisaram ser retirados pela UI para respeitar o ownership da física. O WAIT dessa composição inválida foi preservado. Restart/Stop do Inspector também foram acionados por toque, com captura da parada mantendo 0.882187. A cena permaneceu idêntica após reabrir e após sair de Play.

NÃO IREI SER SIMPLISTA NO DESIGN.

Revisão das capturas executáveis confirmou pontos/tangentes e comandos Play legíveis no aparelho; não houve necessidade de outro conceito visual. [Relatório, capturas, vídeo, cenas e hashes](../validacao/evidencias/p15a-android-20260930/README.md). O título genérico “Valor” é uma dívida pequena de contexto. A evidência nova qualifica esse cenário P15a; não transforma as qualificações host anteriores em provas Android nem encerra portrait, áudio/foco, física 2D, performance/thermal, GI/bake/probes ou os demais componentes do atlas.

## Continuação — AudioVoice ABI23 e aceite editável de física 2D/áudio

Duas frentes de código Sol 6.1 low, design Sol 6.1 medium e implementação/integração do principal, respeitando quatro vagas totais. Registro permanece com 32 schemas, 31 tipos nativos anexáveis, 31 fachadas C#, 54 receitas e 221 ícones; nenhum tipo novo foi contado por API ou fixture.

AudioVoice conecta pedidos refletidos e observação real de SceneAudio/miniaudio ao C#. Snapshot retorna estado, cursor e gate de saída, valida mundo/geração/instância e invalida no Stop. ABI23 acrescenta callback ao fim do contrato e rejeita binários antigos. Corrigida validação gerenciada que poderia lançar LastStatus antigo depois de IsAlive; o callback agora produz o status da operação. Referências oficiais versionadas, lifecycle e limites em [AudioVoice](AUDIO-API-ABI-2026-09-30.md). Seek, áudio ouvido e foco físico não são anunciados por um getter de estado.

O projeto Families-ADB-20260930 foi gerado por EditorSession com Body2D/Collider2D, parede estática, AudioSource/AudioListener, WAV importado e dois Behaviors de aceite. Probes consultam solver/voz reais e nunca criam dependências ou emitem PASS para configuração ausente. O exportador recusa diretório não vazio; os seis arquivos permaneceram idênticos na tentativa recusada. O exportador final leu e reserializou a cena publicada em uma importação independente; cena/registro correspondem ao pacote entregue após normalizar o GUID único de importação e o caminho do descritor.

NÃO IREI SER SIMPLISTA NO DESIGN.

Corrigido estado vazio do seletor WAV: uma busca sem correspondência deixa de afirmar ausência de arquivos importados. Duas capturas executáveis de host, 853×394 e 1200×700, mostram busca/voltar/+WAV/limpar/paginação íntegros e mensagem correta. A captura maior conserva três recortes de desenho, com marcador do gizmo na borda do viewport; nenhum campo do Inspector foi cortado. Não houve novo conceito ou ícone: esta correção usa a superfície contextual e acervo reais existentes.

Build host e SDK/probes C# passaram, C# com zero avisos/erros. Testes dirigidos: audio_ 5/5, physics2d 8/8, Path ABI 1/1 e dois contratos Play 1/1 cada. A primeira fixture de remoção assumia aplicação imediata, falhou e foi corrigida para o ponto seguro real; log inicial preservado. APK final passou em 2m23s, 264336992 bytes, SHA-256 066FAD8868A544E8E8D68C66FD57E9720EB542FBEFF8CECE10A53D9881E4E657. A DLL do SDK empacotada coincide com o binário Release atualizado. [Relatório, logs, capturas e manifesto](../validacao/evidencias/runtime-families-20260930/README.md).

ADB autorizado, mas desconectado: anúncio anterior 192.168.30.69:39229 recusou conexão (10061); a consulta final não encontrou dispositivo nem serviço mDNS. Endereço atual da depuração sem fio solicitado. Não houve instalação deste APK, execução CLR destes probes no aparelho, captura Android, importação SAF, prova de audibilidade ou foco/thermal nesta continuação. P15a anterior conserva sua evidência separada. Todas as famílias do atlas, lightmap/GI/probes e demais pacotes continuam com suas fronteiras declaradas; nenhuma paridade integral foi afirmada. Sem commit ou push.

## Continuação — relógio global e consumidores de tempo, ABI24

SimulationClock por GameWorld captura a escala antes dos callbacks: alterações em Update passam a valer no próximo quadro inteiro. Scale 0 mantém Update/LateUpdate e interrompe a simulação física; Step editorial usa escala efetiva 1 sem alterar o valor solicitado. Deltas escalados alimentam os consumidores reais, incluindo destruição adiada. TimeAccess C# recebe o snapshot nativo e permite escrita de Scale com identidade e thread verificadas; host sem serviço de clock recusa essa escrita. O renderer Android do GameWorld usa o tempo da simulação extraída. EditorWaterPlay é uma rota legada separada e mantém contrato anterior, explicitamente sem esse controle.

Timer e TransformTween v2 acrescentam ignore_time_scale com migração v1 para false. A propriedade participa de arquivo, reflexão, Inspector, fachada gerada e consumidor. Troca de relógio conserva progresso; pausa editorial interrompe ambos os modos; cancelamento e autoridade de pose continuam válidos. Scale não entra no arquivo nem no Undo, e novo Play começa em 1. Corrigido reaproveitamento indevido do mundo quando Stop/start chegam no mesmo lote de entrada.

NÃO IREI SER SIMPLISTA NO DESIGN. Tempo substitui Passo durante execução e devolve o mesmo espaço ao pausar. Capturas executáveis em celular vertical, landscape e tablet verificam os controles e os grupos de Timer/Tween; nenhuma imagem conceitual foi contada como implementação. Não há componente nem ícone novo: catálogo permanece com 32 schemas, 31 fachadas, 54 receitas e os ícones existentes.

Host passou em 13/13 cenários dirigidos; C# em 52/52 Astra, zero falhas/pulados; build gerenciado final zero avisos/erros. APK compilado, 264340670 bytes, SHA-256 22567EDD29C4B476045A0190659368E4C1F70B0DBB8745F95F143F1DD9F4364A. A DLL empacotada coincide com o SDK do build Android. Projeto editável Time-20260930 foi publicado por EditorSession, reaberto com roundtrip exato e seu probe compilou. [Relatório, capturas, logs e hashes](../validacao/evidencias/time-abi24-20260930/README.md), [contrato e referências](API-TIME-2026-09-30.md).

ADB continuou sem dispositivo. Não houve execução do probe pelo CLR Android nem verificação visual Vulkan sob escala. O pacote avança P06 e a integração de P03/P04; não conclui esses pacotes, P20 ou o atlas inteiro. As famílias restantes mantêm suas lacunas documentadas, sem APIs decorativas adicionadas para simular encerramento. Alterações anteriores preservadas; sem commit ou push.

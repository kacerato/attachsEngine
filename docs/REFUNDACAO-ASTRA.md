# Refundação Astra — plano executável e registro de gates

Continuidade em 12/09/2026: [plano mestre integral](planos/PLANO-MESTRE-ASTRA-EDITOR-RUNTIME-ASSETS.md),
[entregas A–G](PROXIMO-PACOTE-GAMEPLAY.md) e [estado M06](planos/ESTADO-M06-IDE.md).
A etapa atual amplia a IDE existente, sem reabrir o viewport informado como resolvido.
M06 permanece aberto e a aceitação no aparelho depende de nova autorização de ADB.
Os parágrafos datados abaixo preservam os estados e autorizações de suas próprias rodadas.

Atualização de escopo em 10/09/2026: a referência corrente é
`C:/Users/donod/Downloads/PROMPT_REFUNDACAO_ASTRA_V2.md`, com o complemento
`ASTRA_COMPONENTES_CODIGO_EDITOR.md`. As tabelas datadas abaixo são histórico;
o registro de implementação da V2 está ao final. Checks rápidos nesta fase,
consolidação extensa depois, conforme orientação direta do usuário.

Data: 2026-09-09. Solicitação: implementar o documento externo
`C:/Users/donod/Downloads/PROMPT_REFUNDACAO_ASTRA.md`.
Este registro não constitui aprovação dos marcos. Referências descrevem outras
engines; só código executado e testes da Astra comprovam capacidades da Astra.

## Estado consolidado em 09/09 — não equivale a aprovação dos gates

| Marco | Estado nesta execução | O que ainda impede encerramento |
|---|---|---|
| M0 | Checkout/APK identificados; builds e testes registrados por fatia | Fechar prova reproduzível consolidada, incluindo clean build |
| M1 | Projeto seco abre/reabre; IME e layout mínimo implementados | Consolidar toda a aceitação de foco/layout/lifecycle e independência |
| M2 | Documento/histórico e componentes registrados em evolução | Extrair demais dados legados, completar contratos e validar gate inteiro |
| M3 | Extração existente reaproveitada por adaptadores | Recursos e autoria arbitrários independentes de biblioteca de demo |
| M4 | Navegação/gizmos existentes com regressões | Gate completo de projeção, picking e manipulação em escalas/hierarquias |
| M5 | Ferramentas mínimas existentes | Inspector extensível e importação externa/asset browser completos |
| M6 | Ambiente/material parciais existentes | Criar iluminação e aparência do zero pelo fluxo público |
| M7 | Runtime seco isolado, mundo compartilhado, Pause/Step; 100 ciclos host | Triggers/layers/consultas e debug de autoria; medição de vazamentos e gate completo |
| M8 | Cápsula, movimento/salto e câmera hierárquica com olhar opcional | Input de projeto, rampas/degraus, rig com obstáculos, comportamentos e animação |
| M9 | Não executado | Duas composições criadas inteiramente pela UI |
| M10 | Não aprovado | Exportação e execução independente do editor |
| M11 | Não executado | Remover legado, regressão e aceitação final sem pendências |

As fatias de dados atuais são pré-requisitos da separação exigida em §12,
usando testes antecipados permitidos por §14. Não autorizam iniciar efeitos ou
tratar M3–M11 como concluídos. Referências de outras engines continuam sendo
referências de projeto, nunca prova de capacidade existente na Astra. As tabelas
abaixo registram diagnóstico histórico; as continuações registram as mudanças.

## M0: evidência inicial

Checkout `main`, commit `eb6ea87d5973c29a9169e489ea0a106e7762899d`.
Há alterações preexistentes em artefatos `.cxx`; não foram descartadas.
Nenhuma captura acompanhava o documento. Não foi inventada análise visual.
ADB identifica modelo 25053PC47G, produto onyx_global, em dois transports do
mesmo aparelho; selecionar explicitamente um transport em cada comando.

| Problema observado | Causa no código / símbolo | Impacto | Substituição e gate |
|---|---|---|---|
| Demos na criação | `SceneTemplate.all`, `NewProjectSheet.buildTemplates` | Autoria começa por exemplos | Somente Empty; conservar resolução de IDs antigos. Primeira alteração desta execução |
| Empty ainda usa biblioteca de mapa | `AstraShellActivity.openInEditor`, `android_main.cpp`: `importMap(...,!editorEmpty)` | Ausência de objetos não comprova independência do pacote | Recursos genéricos em M3; não remover arquivos do pacote antes disso |
| Água no objeto genérico | `EditorEntity`: `WaterRoute`, `water[34]`, `waterLayout[9]`, `waterBody[7]` | Núcleo de autoria depende do efeito | Componentes opcionais e migração M2; desligamento do pacote validado antes de M11 |
| Propriedade depende de posição | `editor_properties.h`: tabela de offsets e contrato de ordem | Alteração de UI pode afetar identidade | IDs explícitos estáveis e leitor legado separado em M2 |
| Assets vinculados a pacote | `EditorMapScene::asset`, `assetId` e fingerprint | Importação independente incompleta | AssetId persistente, catálogo e cache de recursos em M3/M5 |
| Picking aproximado | `editor_view.cpp`: `pickNearest` intersecta esferas | Erros em malhas vazadas/longas | BVH de triângulos com transform inverso em M4; bounds apenas broadphase |
| Física particular do personagem | `CharacterMotor::initialize` chama `AetherPhysics_CreateWorld`; shutdown destrói mundo | Sem interação no mesmo mundo com corpos de cena | Mundo pertencente ao runtime e handle emprestado pelo motor em M7 |
| Play associado ao workspace | `EditorSession::isPlaying`, `advanceClock`, `editor_water_play.h`, consumidores Android | Relógio isolado não prova runtime genérico | Instância independente e máquina Edit/Running/Paused em M7 |
| Godot empacotado em paralelo | `app/build.gradle.kts`, `GodotEditorActivity`, manifest | Segundo editor no APK | Não usar como solução da refundação; auditar/remover integração sem consumidor antes de M11 |

Classificação inicial: documento/histórico/persistência e navegação possuem
implementação e testes existentes, ainda sem aprovação desta refundação;
componentes anexáveis e recursos independentes são parciais; escolha de mundos
por demonstração é hardcoded; autoria arbitrária completa não está comprovada.
Não se atribui suporte de runtime a um header ou a um catálogo de ferramentas.

### Cinco caminhos rastreados

1. Criar: toque → `EditorSession::handlePointer` →
   `EditorHistory::createEntity`/`instantiateAsset` → `EditorDocument` →
   `EditorMapScene::extract` → publicação Android no renderer. A geometria
   disponível continua vinculada à biblioteca importada.
2. Propriedade: roteador de UI → propriedade numérica/`dispatch` → histórico
   → revisão do documento → extração; água possui consumidor Android específico.
3. Salvar/reabrir: sessão → `editor_archive.cpp` → arquivo com fingerprint →
   desserialização validada → substituição do documento; não equivale a exportar jogo.
4. Selecionar: coordenadas lógicas → `screenPointToRay` → `pickNearest` →
   seleção da sessão → hierarquia/Inspector/gizmo. Precisão por triângulos pendente.
5. Play: pedido de UI → sessão/workspace → relógio → consumidores Android e
   água/física. Falta comprovar ciclo genérico com personagem e corpos compartilhados.

## Referências consultadas

- [Godot: interface](https://docs.godotengine.org/en/stable/getting_started/introduction/first_look_at_the_editor.html):
  separação entre árvore de cena, arquivos, propriedades e execução.
- [Godot: recursos e alternativas](https://docs.godotengine.org/en/stable/tutorials/best_practices/node_alternatives.html):
  dados reutilizáveis não precisam ser objetos da árvore.
- [Unity: Scene View](https://docs.unity3d.com/Manual/UsingTheSceneView.html):
  edição e navegação da cena como ferramentas de trabalho.
- [Unity Learn: greybox](https://learn.unity.com/tutorial/create-your-greybox-prototype-3?uv=6):
  referência de sequência de prototipagem, não prova de funções Astra.
- [Unity: world building](https://docs.unity3d.com/6000.6/Documentation/Manual/CreatingEnvironments.html).
- [Jolt](https://github.com/jrouwe/JoltPhysics) e [cgltf](https://github.com/jkuhlmann/cgltf):
  bibliotecas candidatas não substituem integração de runtime/importação.
- Os dois vídeos fornecidos, [greyboxing](https://www.youtube.com/watch?v=YsBniZ5ya7k)
  e [modelagem](https://www.youtube.com/watch?v=4Am9E36-7HM), retornaram erro de
  acesso. Não foram assistidos e nenhum timestamp é apresentado como verificado.

| Ação do criador | Ferramenta / serviço | Dados persistidos | Consumidor | Prova exigida |
|---|---|---|---|---|
| Criar vazio | Shell / ProjectStore | Descritor e cena sem objetos | Loader | Reabrir sem entidades ocultas |
| Construir chão/parede/rampa | Criar geometria / comando | Mesh, transform, material | Extração/render | Aparecer e colidir após reabrir |
| Organizar módulos | Árvore / histórico | Parent e ordem | Transform runtime | Duplicar/reparent/Undo com escala não uniforme |
| Iluminar | Inspector / registro | Componentes e recursos | Luzes do renderer | Igualdade Edit/Game sem luz oculta |
| Adicionar personagem | Inspector / registro | Collider, motor e ações | Mundo físico | Salto, rampa, degraus, plataforma |
| Configurar porta | Comportamento do projeto | Grafo/propriedades/referências | Executor de comportamento | Trigger funciona sem recompilar núcleo |
| Importar GLB | Arquivos / importador | AssetId, fonte, settings | Cache CPU/GPU | Renomear e reabrir offline |
| Executar/exportar | Play e Build / runtime | Cena inicial e assets | Runtime sem editor | Dois projetos executam fora do workspace |

## Contratos decididos para implementação

Árvore de objetos com componentes; recursos fora da árvore. C++ é dono da
autoria e instâncias nativas; managed consome API explícita, sem segunda cópia
autoral. Não substituir Vulkan por Godot/Three.js/Filament.

IDs persistentes de objeto/asset de 128 bits; TypeId/PropertyId explícitos e
versionados, independentes do índice visual. Metadados tipados compartilhados
por Inspector, validação, comandos e arquivo. Tipos desconhecidos preservados
como dados opacos com diagnóstico, nunca descartados silenciosamente.

Comandos executam na thread da sessão; transações guardam valores antes/depois
por identidade. Cancel reverte a transação; gesto contínuo produz um Undo.
Workers de importação só publicam resultados imutáveis na thread proprietária.
Falha mantém documento/cache anterior e reporta origem e asset.

Runtime pertence à instância Play: mapa ID autoral → handle com geração,
mundo físico único, acumulador de passo fixo, extração para renderer. Stop
destrói a instância, não reconstrói o documento a partir de estado de jogo.
Renderer recebe pacotes e handles, sem headers de editor. Recursos GPU têm
retirada após fences; importação não escreve diretamente em buffers ativos.

Transform local TRS e matriz global centralizada. Preservar global rejeita
shear não representável, sem mutação parcial; preservar local é outra política.
Ativo herda por parent; visibilidade de editor e bloqueio de seleção são
metadados separados da visibilidade de runtime. Duplicação remapeia IDs internos.

Escolha inicial: reutilizar Jolt e matemática após testes; auditar importador
existente antes de adicionar cgltf. Nenhuma dependência nova nesta etapa.
IME Android deve passar por prova de foco, texto, composição, cancelar e clipboard
antes de decidir manter o toolkit nativo. Godot empacotado não comprova essa ponte.

## Backlog com gates obrigatórios

Todos os itens abaixo permanecem abertos até anexar prova. Arquivos novos são
propostas, não símbolos presumidos existentes. Cada marco depende do anterior.

| Marco | Subetapas / arquivos | Critério e risco | Rollback |
|---|---|---|---|
| M0 | Auditoria de consumidores, build e APK; este documento e logs | Hash do APK executado; testar cinco fluxos; risco de binário antigo | Sem alterar dados |
| M1 | `SceneTemplate`, `NewProjectSheet`, `ProjectStore`; `editor_screen`, input Android | Empty sem mapa/água, IME, resize e resume; atual mudança só remove demos da criação | Reverter UI; manter descritores |
| M2 | Novo `native/scene` com object/component/property; adaptar document/history/archive | IDs, transações, roundtrip, migrações, shear e referências | Backup de formato e leitor legado |
| M3 | `native/resources`, `editor_map_scene`, renderer Android; nova extração runtime | Malhas/câmeras arbitrárias sem pacote de demo | Manter leitor antigo apenas para migração |
| M4 | camera/view/grid/gizmo/session; BVH por recurso | Precisão de picking, local/global, ortográfica, multisseleção, toque cancelado | Testes de projeção e input anteriores |
| M5 | screen/filesystem/registro Inspector, importador e ponte SAF | GLB externo pela UI, materiais compartilhados, instâncias/overrides, Undo | Catálogo publicado atomicamente |
| M6 | Materiais/luzes/ambiente e consumidores renderer | Direcional/ponto/spot e sombra configurável; Edit/Game coerentes | Recursos anteriores preservados |
| M7 | Mundo runtime, `character_motor`, bridge Jolt, estados Play | 100 Play/Stop, Pause/Step físico; autoria intacta | Destruir instância isolada |
| M8 | Input actions, rigs, comportamento C#/Flow auditado, animação | Porta/trigger/plataforma autorados; skin/clip reais | Diagnóstico de comportamento inválido |
| M9 | Dois projetos criados manualmente na UI | Interior e pátio, sem geração interna ou JSON manual | Projetos de teste separados |
| M10 | Loader runtime e build/export Android | APK de projeto sem workspace/editor; offline | Preservar última distribuição válida |
| M11 | CMake/Gradle/manifest, remoção de legado, documentação | Sem UI/demos obrigatórias, pacote de água desligado; regressão e perfil | Release anterior recuperável |

## Wireframes funcionais propostos

Estados partilham uma geometria de viewport: barras/painéis mudam o retângulo
que alimenta projeção, toque, picking e gizmo. Alvos mínimos de toque 48dp;
larguras limitadas e drawers em telas menores. Valores são orçamento de UX,
não medição de implementação.

```text
Vazia:  [Projeto Cena] [Selecionar Mover Girar Escalar] [Play]
        [Árvore +]    [viewport + grade de editor]    [Sem seleção]
        [Arquivos]   [diagnóstico recolhido]
Malha:  [mesma barra e mesmo retângulo]
        [seleção]    [malha + gizmo]                 [Transform / Mesh / Material]
Câmera: [mesma barra e mesmo retângulo]
        [câmera]     [frustum selecionado]           [Transform / Camera]
Import: [Projeto Cena]                               [Cancelar importação]
        [pastas]     [viewport preservado]          [asset / progresso / erros]
Play:   [Projeto] [Game View / Scene View]            [Stop Pause Step]
        [árvore runtime só leitura] [jogo]           [diagnóstico runtime]
```

Comandos propostos: Novo/Abrir/Salvar atuam em projeto/cena; Criar/Excluir/
Duplicar/Reparent/Renomear em comandos transacionais; Enquadrar só em câmera
editor; Play/Stop/Pause/Step só na instância runtime. Inspector habilita campos
compatíveis e exibe valores mistos em multisseleção. Importar publica catálogo;
Excluir arquivo exige confirmação e exibe dependências. Botão não conectado
não aparece. Água, marketplace e animação não ganham abas de reserva.

## Roteiro de aceitação

Novo projeto → criar chão, paredes, passagem, escada, rampa e plataformas →
agrupar e duplicar módulos → material argila → luz/ambiente → cápsula/motor →
câmera separada → colliders → trigger/porta/plataforma por comportamento →
Play, caminhar/saltar/subir/acionar → Stop → modificar → salvar → fechar app →
reabrir → repetir execução. Repetir com outra composição inteiramente diferente.
Nenhuma etapa pode depender de JSON manual ou montagem interna da cena.

Casos negativos: Cancel em gesto e importação; parent cíclico/shear; asset
ausente; Undo de exclusão de subárvore; interrupção de escrita; lifecycle e
surface; 100 ciclos de Play; carga parametrizada de objetos/assets. Registrar
APK SHA-256, modelo/GPU, resolução, qualidade, duração, CPU/GPU p50/p95/p99,
memória e latência. Não substituir percentis por FPS médio.

Escopo posterior: multiplayer, GI avançada, terreno e água avançada. Não
substituem os gates acima. Primeira fatia implementada: bloquear criação de
demos em catálogo e serviço, mantendo compatibilidade de projetos antigos.

## Evidência da primeira fatia — execução de 09/09

- `SceneTemplate.all` oferece somente Empty; `byId` mantém resolução legada.
- `ProjectStore.create` recusa demos antes de criar diretórios.
- `NewProjectSheet` remove o seletor, usa Empty e habilita criação após nome válido.
- Testes Java: 6/6, zero falhas (`app/build/test-results/testDebugUnitTest`).
- Build host e 724/724 testes nativos: `build/refundacao-host-build.log` e
  `build/refundacao-host-tests.log`. Nenhum C++ foi alterado nesta fatia.
- Debug e Release: BUILD SUCCESSFUL, `build/refundacao-android.log`.
- APK Debug compilado e instalado com SHA-256 idêntico:
  `f5118d964b91ef08407cc991e7f10ce3118bba03e58de374c26ab62139b30127`.
- Android 16, modelo 25053PC47G, captura 2772x1280. Instalação preservou dados.
- Pela UI: Novo projeto → nome `RefundacaoValidacao0909` → Criar projeto.
  Hierarquia vazia e Inspector sem seleção; arquivo salvo AETHER_EDITOR 6
  contém uma única entidade raiz. `build/refundacao-created.png`.
- Home → retorno à Activity preserva viewport vazio:
  `build/refundacao-resumed.png`. Isso testa background/foreground, não morte
  do processo nem todos os casos de reconstrução de surface.
- Dumps de UI comprovam ausência de seletor e botão habilitado após entrada:
  `build/refundacao-new-project.xml`, `build/refundacao-new-project-filled.xml`.

**Estado da primeira fatia, anterior à continuação abaixo:** Empty usa fingerprint
9651248282590934415 e carrega biblioteca do mapa; raiz serializa campos de água.
Não foi aprovada criação de cena jogável, importação externa, componentes,
Play genérico, distribuição, performance ou remoção final do legado. Os testes
acima não substituem essas provas. O projeto de validação foi mantido no aparelho.

## Continuação M1 — documento sem biblioteca obrigatória

Plano externo relido antes de implementar e critérios M1/M2 relidos durante a
revisão. Decisão detalhada em
[ADR-REFUNDACAO-EMPTY-WORKSPACE](adr/ADR-REFUNDACAO-EMPTY-WORKSPACE.md).

### Mudanças integradas

- Novos descritores declaram `resourceSource: independent`. `ProjectSceneSource`
  decide a origem sem usar nome do projeto; arquivos antigos mantêm o pacote.
  Cena privada legada não é esquecida quando ainda falta arquivo público.
- `AstraShellActivity` transmite a escolha e diagnostica cabeçalho inválido;
  `AetherActivity` não aceita silenciosamente troca de projeto/fonte numa instância viva.
- `android_main.cpp` inicializa/carrega/salva o documento mesmo com zero recursos.
  Nesse caminho não inicializa CoreCLR, não carrega mapa e não executa WaterPlay.
- `InstancedRenderer` apresenta clear e overlay sem draws de geometria;
  o asset manager fornece os atlas do editor. Mantém capacidade mínima de buffer
  um para reutilizar a configuração Vulkan existente, sem objeto correspondente.
- Persistência independente usa fingerprint zero. Água ainda ocupa campos do
  formato v6: sua extração continua pendente em M2, sem declarar pacote removido.

### Validação executada

Build host e **725/725 testes**; **12/12 testes Java**; Debug/Release passaram.
Logs: `build/refundacao-m1-host-build.log`, `build/refundacao-m1-host-tests.log`,
`build/refundacao-m1-final-android.log`. `git diff --check` sem erros.

APK compilado/instalado com SHA-256 coincidente:
`d87add860ce60300291390e0f0e1a58aa2010d87cccd70d4e9f064a0df18e68b`.
Android 16, 25053PC47G/onyx_global, capturas landscape 2772x1280.

Pela UI: criado `M1SemPacote0909k`, adicionado **Objeto vazio**, salvo. O arquivo
tem cabeçalho `AETHER_EDITOR 6 0 2`. Home e retorno reconstruíram a surface e
preservaram o objeto. Encerrar processo e abrir novamente pelo cartão do shell
restaurou a mesma hierarquia. O log registra `independent resources=0 fingerprint=0
managed=off` nos processos 5992 e 707, e duas entidades na reabertura.

Evidências: `build/refundacao-m1-empty.png`, `refundacao-m1-resume.png`,
`refundacao-m1-cold-reopen.png`, `refundacao-m1-device.log`.
O arquivo antes/depois do encerramento tem o mesmo hash
`fa35c5e8e8834cf782e5324df6fd828de7887a00aa30473a8530f0e379e2ee14`
nas cópias locais de evidência.

Compatibilidade: projeto de teste anterior `RefundacaoValidacao0909` reaberto
pelo shell; manteve fingerprint 9651248282590934415 e hash de arquivo idêntico
antes/depois. Captura `build/refundacao-m1-legacy.png`; log
`build/refundacao-m1-legacy-device.log`. Projetos pessoais não foram editados.

Observação de desempenho, **não benchmark aprovado**: logs registram redução
dinâmica de escala até 0.5, amostras GPU próximas de 9.2 ms e VSYNC perto de
60 Hz. Não foram calculados percentis nem aceitos orçamentos. Primeiros frames
após ativação registraram aproximadamente 323 ms e 351 ms no teste. A mudança
não deve ser apresentada como otimização de frame time.

### Gate atual e próxima subetapa

**Aprovado nesta fatia:** abrir novo projeto, autorar hierarquia mínima, salvar,
retomar/reabrir sem carregar biblioteca de mapa; preservar leitura legada.
**M1 completo ainda aberto:** a UI antiga tem categorias/abas não aplicáveis,
e foco/IME nativo, cancelamento, resize e composição mínima ainda precisam da
prova de toolkit pedida pelo plano. Não iniciar M2 como se esse gate estivesse fechado.

Próxima fatia concreta: `editor_screen.*`, `editor_session.*`, `ui_input.*` e
ponte Android de texto. Inventariar comandos disponíveis com biblioteca vazia;
retirar controles sem consumidor e testar foco/confirmar/cancelar sem alterar
a câmera ou a revisão do documento. Validar layout com painéis recolhidos e
surface reconstruída. Depois conectar o registro tipado de M2 ao Inspector,
histórico e arquivo; não criar um segundo documento autoral desconectado.

## Continuação M1 — entrada Android e disponibilidade real (2026-09-09)

Releitura do plano externo: §§6–8, 13–14. O requisito segue sendo autoria
arbitrária validada; esta fatia não conclui a refundação nem autoriza pular M1.
Decisão/contratos: [ADR de texto Android](adr/ADR-REFUNDACAO-ANDROID-TEXT.md).

Implementado:
- Hierarquia > Renomear e campos numéricos abrem EditText Android. Buscas usam
  a mesma ponte. Aplicar passa pela validação e história da EditorSession;
  cancelar não altera autoria. Número negativo/decimal e UTF-8 têm testes.
- Solicitações carregam versão/entidade/campo; resposta obsoleta é rejeitada.
  A sessão captura os ponteiros enquanto o campo está aberto.
- Catálogo de criação usa recursos efetivamente importados. Projeto independente
  oferece objeto vazio e câmera; geometria/água/física dependem dos seus recursos.
  A máscara é transitória, reconstruída ao importar; não é registro de componentes.

Validação: 729/729 testes nativos; 12/12 testes Java; assembleDebug e
assembleRelease concluídos. Logs: build/refundacao-ime-host-build.log,
refundacao-ime-host-tests.log e refundacao-ime-android.log. Sem warning/error
nesses logs finais. Não foi executado clean build nesta fatia.

Dispositivo: Android 16, 25053PC47G/onyx_global, 2772x1280, transporte ADB 1.
APK final instalado, SHA-256 local e instalado iguais:
414835cb32b93e3c67368dae7754ce3c886ab29d8c1ca1c0673c268fc0275946.
Projeto de teste M1SemPacote0909k; nenhum projeto pessoal foi editado.

Fluxo exercitado pela interface: abrir projeto, selecionar grupo, renomear para
GrupoIME, editar X=-2.5, salvar, reinstalar/reabrir e conferir valor no campo.
Na versão final, digitar 99 e Cancelar preservou arquivo byte a byte. Aplicar
-3.5 salvou o valor; Undo e Salvar restauraram -2.5. Home/retorno preservou a UI.
Arquivo mantém AETHER_EDITOR 6 0 2, sem fingerprint de pacote. Capturas em build:
refundacao-ime-final-number.png, refundacao-ime-creation.png,
refundacao-ime-resumed.png; arquivos final-applied e final-undo.aescene provam
-3.5 e -2.5. O recorte refundacao-ime-device.log não contém fatal Java/native;
não substitui soak nem benchmark. Tentativa anterior de Undo com menu modal
aberto não executou edição; foi repetida após fechar pelo botão Cancelar.

Gate: IME mínimo, persistência e cancelamento desta fatia comprovados.
M1 permanece aberto: composição adaptativa, abas inaplicáveis (Material ainda
visível em grupo), estados/wireframes e testes de resize/recolhimento. Clipboard
manual, outros IMEs e edição durante perda de foco precisam ampliar a matriz.
Não foram validados desempenho, M2 tipado ou cena jogável arbitrária.
Próxima subetapa: inventariar/remover controles sem consumidor em editor_screen,
validar layout por seleção e painéis recolhidos/redimensionados no Android antes
de avançar para registro tipado de componentes. Recursos de outras engines
continuam sendo referências, não capacidades atribuídas à Astra.

## Continuação M1 — Inspector aplicável e prova de painéis (2026-09-09)

Plano relido: §§7–8. Inventário e estados documentados em
[REFUNDACAO-WORKSPACE-STATES.md](REFUNDACAO-WORKSPACE-STATES.md).

Alteração em editor_screen.cpp: Material fica ausente sem assetId; uma página
Material anterior resolve para Transformação nessa seleção. Grupos sem recurso
não expõem corpo rígido nem sombra própria. Visibilidade herdada permanece.
Receber sombra e Estático saíram da UI: busca em native identificou apenas
armazenamento/serialização/comandos, sem consumidor. Campos legados continuam no
formato, sem migração destrutiva. Isto não implementa componentes tipados de M2.

Testes: 730/730 nativos, incluindo troca malha/grupo com Material aberta e
presença/ausência de controles conforme recurso. Gradle testDebugUnitTest,
assembleDebug e assembleRelease concluídos; 12 testes Java, sem falhas.
Logs build/refundacao-controls-host-build.log, refundacao-controls-host-tests.log,
refundacao-controls-android.log. Build incremental, não clean build.

APK instalado e hash conferido local/aparelho:
908c8120a1d392398af7ae33462c393b91baddd5539a5fa37c3dde089c0d0584.
Android 16, 25053PC47G, 2772x1280; projeto de teste M1SemPacote0909k.
Capturas build/refundacao-controls-group.png e refundacao-controls-resize.png
comprovam abas aplicáveis e Visível como única opção de grupo em Propriedades.
Arraste real aumentou a hierarquia e reduziu o viewport; fullscreen recolheu os
painéis e retorno recuperou a composição. Capturas fullscreen/restored.png
sob o mesmo prefixo. Não foram alterados dados de projetos pessoais.

M1 ainda aberto: telas estreitas atualmente comprimem painéis proporcionalmente,
sem alternância mobile suficiente. Barra principal ainda contém organização
legada; parâmetros de câmera e Inspector extensível não estão concluídos.
A validação desta rodada não é prova de M7, jogo arbitrário, soak ou performance.
Próxima subetapa concreta: resolver painéis alternáveis quando não couberem os
mínimos, manter comando para retorno, testar resize e seleção preservada; depois
reorganizar a barra conforme comandos reais do inventário. Não avançar M2 por
ter apenas retirado controles inadequados.

## Continuação M1 — alternância compacta (2026-09-09, revisão de UX aberta)

Plano §7 relido. editor_screen.* resolve modo compacto abaixo de 672 dp de
largura útil (2 painéis de 240 + viewport mínimo 180 + divisores). Nesse modo,
CompactPanel seleciona Viewport, Hierarquia, Inspector ou Arquivos. A faixa de
44 dp reserva espaço; só um painel lateral aparece. As larguras do modo amplo
continuam no estado e não são sobrescritas. Essa navegação não altera a cena nem
a história. Ferramentas ficam horizontais; navegação sai da frente delas.
Arquivos tem acesso separado para não desaparecer por falta de altura.

Teste host: 731/731. O teste novo verifica comandos por toque, seleção/revisão/
histórico preservados, retorno às larguras amplas, Arquivos em corpo curto e
quatro ferramentas tocáveis. Java 12/12; Debug e Release compilados. Logs em
build/refundacao-compact-host-build.log, refundacao-compact-host-tests.log e
refundacao-compact-android.log. Build incremental.

APK instalado e SHA local/aparelho:
4fa9da0421ef29658c966793d317f36f070c93ef9308dca3869ff0ec4293963a.
Android 16/25053PC47G, 2772x1280, densidade física 520. Sem override global de
resolução/densidade. Usada opção existente aether.editor_scale=1.3 para validar
aproximadamente 656x303 unidades lógicas. Escala normal é 0.6. Não confundir as
capturas ampliadas com mudança da escala padrão do produto.

Pela interface, Hierarquia -> selecionar GrupoIME -> Inspector (X=-2.5) ->
Arquivos (diretório real) -> Viewport preservou seleção. Capturas
build/refundacao-compact-final-{hierarchy,inspector,files,viewport}.png.
A primeira prova em escala 1.5 revelou sobreposição das ferramentas e sumiço de
Arquivos; corrigidos antes do APK final. Alturas extremas do Inspector ainda
não foram aprovadas. Retorno à escala normal pela abertura usual confirmado em
build/refundacao-compact-wide-restored.png; nenhum dado autoral foi editado.

Feedback do usuário: os quatro nomes e a captura ampliada não deixaram claro o
propósito e pareceram grandes demais. Portanto a faixa de quatro opções NÃO é
UX aprovada nem layout definitivo. Próxima decisão: acesso compacto aos painéis
com menor ocupação permanente e identificação clara do painel ativo. Preservar
os contratos/testes de alternância; revalidar visualmente antes de fechar M1.
M1 continua aberto; não avançar M2 por esta prova funcional.

## Continuação M1 — acesso sob demanda aos painéis (2026-09-09)

Plano §7 e feedback relidos. Removida a faixa permanente de quatro nomes.
Agora o botão Painéis na área de ferramentas abre um menu 2x2 com indicação da
opção ativa; escolher fecha o menu, Fechar cancela sem mudar painel. A alternância
continua em CompactPanel e o viewport recupera 44 dp de altura. Sem mudanças no
formato autoral, seleção ou histórico. Nenhuma escala padrão foi alterada.

731/731 testes nativos e 12/12 Java; Debug e Release compilados. O teste compacto
agora abre o menu por toque a cada escolha e verifica ausência da faixa fixa,
retorno ao viewport, arquivos, ferramentas e larguras amplas preservadas.
Logs build/refundacao-panels-host-build.log, refundacao-panels-host-tests.log,
refundacao-panels-android.log. Build incremental, sem novo clean build.

APK instalado/hash local e dispositivo:
40469c536be8f3554c7e2f79120642824e5ff7c934d7dc1aece0f7c041574785.
Android 16/25053PC47G/2772x1280. Escala 1.3 usada somente na prova compacta;
restaurada abertura normal após o teste. Capturas build/refundacao-panels-menu.png
(menu aberto) e refundacao-panels-inspector.png (fechado, grupo selecionado,
X=-2.5, três linhas de transformação visíveis). Hierarquia -> selecionar grupo
-> Painéis -> Inspector foi exercitado no aparelho. Fechar também exercitado.
Sem edição dos dados autorais durante a prova.

Gate M1 permanece aberto para organização da barra principal por finalidade e
matriz de alturas/larguras extremas. Próxima fatia deve atacar a barra e os
comandos legados conforme §7, sem reabrir indefinidamente este ajuste de painéis.
Não declarar M2 nem autoria jogável arbitrária concluídos.

## Continuação M1 — barra por finalidade (2026-09-09)

Plano §7 relido. buildTopBar agora separa Cena/Salvar à esquerda de Undo/Redo e
Play/Stop à direita; centro informa o contexto. Removidos OpenProject (sem handler
na UI nativa), segundo menu redundante, Play como aba e Salvar duplicado sobre
viewport. Ambiente/água deixam de ser abas principais; menu Cena mantém os
consumidores existentes com nomes explícitos: Ambiente da cena e Água da cena.
O antigo rótulo Configurações representava apenas água, não Project Settings.

Comandos: Cena -> ProjectMenu abre/fecha menu sem autoria; Salvar -> SaveDocument
usa persistência existente; Undo/Redo -> história; PlayFromTopBar alterna estado
de execução. O contexto central é texto de estado, não botão. Recursos importados
continua sendo navegador legado, não pipeline externo de importação concluído.
Água da cena ainda é legado acoplado à raiz; mover o acesso não satisfaz o
requisito de pacote opcional nem remove essa pendência arquitetural.

731/731 testes nativos e 12/12 Java. Teste de barra verifica ausência de abas
Lighting/Play e botão OpenProject, com ambiente acessível pelo menu; teste de
sessão usa o mesmo PlayFromTopBar para iniciar/parar e restaurar painéis/gizmos.
Debug e Release compilados, diff --check passou. Logs em
build/refundacao-toolbar-host-build.log, refundacao-toolbar-host-tests.log,
refundacao-toolbar-android.log. Build incremental, sem clean build.

APK instalado/hash local e dispositivo:
2e48e1d64c0580c9fbd67554f3151d0620618a479137cad402bad72161a4f3a5.
Android 16/25053PC47G, 2772x1280, escala padrão 0.6. Projeto de teste
M1SemPacote0909k. Capturas build/refundacao-toolbar-edit.png,
refundacao-toolbar-play.png e refundacao-toolbar-menu.png. Exercitados início e
parada pelo mesmo botão, restauração do editor e abertura/fechamento de Cena.
Sem alteração autoral. Execução vazia não é prova de gameplay/física de M7.

Próximo gate concreto: disponibilidade dos acessos legados no projeto sem
recursos e separação de propriedades da raiz antes de considerar o workspace
independente concluído. Consolidar a matriz restante de M1; não reabrir a
apresentação de painéis como tarefa permanente nem declarar M2 aprovado.

## Continuação M1 — acessos condicionados à fonte real (2026-09-09)

Plano §§7 e 12 relidos. Menu Cena deixa de oferecer Recursos importados quando
assetCount é zero e Água da cena quando não há recurso de criação de água.
waterCreationAvailable reutiliza a disponibilidade de superfície/oceano/rota
já derivada dos materiais importados; não infere capacidade pelo nome da cena.
Os handlers também verificam disponibilidade. importMap volta para Scene se a
substituição da biblioteca invalidar o contexto Assets/Settings atual.

Teste novo cobre fonte vazia, importação real da biblioteca de água, abertura de
seu Inspector e remoção da biblioteca com saída do contexto obsoleto. Testes de
persistência/espectro de água agora importam a biblioteca explicitamente antes
de exercitar a UI; não presumem que uma sessão sem recursos possua água.
732/732 testes nativos, 12/12 Java, Debug e Release aprovados. Build incremental.
Logs build/refundacao-capabilities-host-build.log,
refundacao-capabilities-host-tests.log e refundacao-capabilities-android.log.

APK instalado e SHA local/dispositivo:
7e3f6d0d9f5444b6cb240ed2247453bbf4c03860dac110f62559d6344da1dde2.
Android 16/25053PC47G, 2772x1280, escala normal. Projeto de teste
M1SemPacote0909k: menu observado com Voltar à edição, Ambiente da cena e Fechar,
sem Água/Recursos. Captura build/refundacao-capabilities-menu.png. Regressão da
biblioteca de água foi validada no host nesta rodada; não reexecutada no aparelho.
Nenhum arquivo de projeto foi alterado na prova de UI.

Limite: filtro de disponibilidade não extrai água do EditorEntity/arquivo.
A migração para componentes genéricos e pacote opcional continua necessária.
O navegador legado com recursos ainda contém ações específicas; não é o novo
Asset Browser de M5. Consolidar a matriz M1 antes do próximo gate e preparar
contrato de migração autoral sem renomear arrays de água como solução genérica.

## Continuação — migração de persistência v7 (2026-09-09)

Plano §12 relido. Preparação de persistência para a migração autoral, sem
aprovação de M2: v7 escreve pares propertyId/value somente quando diferem dos
padrões atuais; v1–v6 continuam legíveis. Configurações desativadas não são
omitidas se possuem valores personalizados. Os flags existentes são restaurados
após aplicar valores. Arquivo inválido não substitui o documento em memória.
Contrato, alternativas, padrões congelados e rollback:
[ADR-REFUNDACAO-ARCHIVE-V7.md](adr/ADR-REFUNDACAO-ARCHIVE-V7.md).

735/735 testes nativos; 14/14 Java; Debug e Release aprovados. Cobertura nova:
fixture v6 literal -> v7 -> reabrir, defaults congelados, configuração de água
desativada, IDs duplicados/desconhecidos, truncamento, não finitos e limites.
Leitor do shell aceita v7 e rejeita v8; validação integral permanece nativa.
Logs build/refundacao-archive-host-build.log, refundacao-archive-host-tests.log e
refundacao-archive-android.log. Build incremental.

APK instalado, SHA local/aparelho iguais:
8de05c99975d447026add4733169f558bb8f744d55ec20caae2b4da8a672a775.
Android 16/25053PC47G, 2772x1280, escala normal. Na UI do projeto de teste
M1SemPacote0909k: abrir v6, Salvar, force-stop, abrir pelo shell, selecionar grupo.
Confirmados GrupoIME e X=-2.5 após reabertura v7. Arquivo passou de 1235 para
153 bytes; cópia anterior preservada em build/refundacao-archive-device-before.aescene,
v7 em refundacao-archive-device-v7.aescene. Captura refundacao-archive-reopened.png.
Nenhum projeto pessoal foi migrado nesta prova.

Limite explícito: essa entrega reduz repetição no arquivo, não o tamanho de
EditorEntity nem seu acoplamento a água. IDs são o contrato estável legado,
não um registro de componentes já implementado. A extração estrutural e o gate
completo M1/M2 permanecem abertos. APK v6 não abre v7; rollback requer cópia v6.
Próximo passo estrutural deve definir armazenamento opcional e propriedade dos
dados de extensão junto de registro/validação, sem trocar arrays específicos
por outros arrays e declarar o plano atendido.

## Continuação — propriedades sem dependência do layout de memória (09/09/2026)

Relido §12 do plano. Inspector, arquivo e validação usavam offsetof e casts
para interpretar floats dentro de EditorEntity, inclusive supondo os oito
campos de WaterRoutePoint contíguos. Essa dependência impede alterar ownership
ou mover propriedades para armazenamento opcional com segurança.

Decisão: cada descritor fornece acesso tipado de leitura/escrita e um índice
local. Os 207 IDs, limites, ativação de overrides e defaults permanecem iguais.
Rotas acessam membros nomeados; não há aritmética entre membros distintos.
Inspector, comandos e arquivo continuam usando editorPropertyValue e
setEditorPropertyValue. Não há alocação nesses acessos; há despacho indireto
por descritor. Nenhuma dependência externa foi adicionada.

Alternativas: manter offsets perpetuaria a restrição de layout; extrair todos
os dados simultaneamente dificultaria separar regressões de ownership e de
migração. Esta alteração remove apenas o primeiro acoplamento. Os callbacks
não substituem a validação pública: mutações continuam sobre cópias, seguidas
por comandos do documento. Histórico e ownership atuais não mudaram.

Validação: 736/736 testes nativos; testes Java e builds Android Debug/Release
passaram (Gradle, 22s). Teste novo verifica canais de material, todos os campos
dos 16 pontos, isolamento da cópia e rejeição de valores/IDs inválidos. Fixtures
v2/v3/v6, persistência v7 e regressões de histórico continuam passando.
Logs: build/refundacao-accessors-host-build.log, -host-tests.log e
refundacao-accessors-android.log. git diff --check dos fontes alterados passou.

Limites: água ainda ocupa EditorEntity; não existe ainda registro genérico de
componentes nem build sem seus headers. Portanto §12 e M2 não estão concluídos.
Próxima dependência: ownership opcional com snapshots independentes, registro
e migração integrados. Rollback desta fatia não exige converter cenas: formato
v7 e IDs não mudaram.

Prova Android desta fatia: APK instalado com SHA256 local/aparelho idêntico
70d5ca26a7e89ca7c09f25ef6986ee170c9f2bbea237b39bc5d7c66b49ba1b15.
Projeto M1SemPacote0909k reaberto, GrupoIME selecionado, Inspector confirmou
X=-2.500; captura build/refundacao-accessors-device.png. São 14/14 testes Java.
Essa verificação cobre abertura/leitura no aparelho; edição e histórico foram
cobertos pela suíte nativa, não repetidos manualmente nesta fatia.

## Continuação — ownership opcional do traçado (09/09/2026)

Relido §12. O traçado reservava os 16 pontos em toda EditorEntity e em cada
snapshot do histórico, mesmo nos grupos secos. Introduzido EditorOptionalValue<T>,
um armazenamento genérico com unique_ptr, leitura de default sem alocação,
escrita explícita e cópia profunda. Primeiro consumidor real: o traçado legado.
Não é um registro de componentes e não substitui a extração exigida no plano.

O contrato preserva semântica de valor mesmo quando uma referência mutável é
obtida antes de uma cópia. Copy-on-write compartilhado foi descartado porque
uma referência já entregue poderia alterar snapshots posteriores. Atribuição
prepara a cópia antes da troca; movimento transfere ownership; reset libera o
payload. Não existe acesso concorrente novo: edição permanece no dono da sessão.
Copiar uma entidade com traçado agora aloca; copiar entidade sem traçado não.
O custo para rios deve ser medido antes de ampliar esse armazenamento.

Integração: Inspector/propriedades, edição de pontos, extração para renderer e
mundo de água acessam read/edit; documento e histórico copiam o valor opcional.
Arquivos v1-v7 mantidos. Valores default de pontos presentes no formato legado
não alocam o traçado. Valores não default, inclusive de rota desativada, continuam
preservados. A contagem zero também não cria payload em cenas secas.

Validação: 737/737 testes nativos, builds Debug/Release e tarefa Java passaram
(Gradle 23s). Teste novo cobre ausência, leitura/escrita default, persistência
seca e com rota, cópia após referência mutável, undo/redo de inserção/remoção,
duplicação independente e restauração após exclusão. Regressões de edição dos
pontos e projeção existentes passaram. Logs build/refundacao-optional-*.log.

Limites: EditorEntity ainda inclui header de água e os demais campos específicos.
Este é um passo de ownership, não a extensão opcional completa de §12. Nenhum
novo painel, pacote de água ou API de outra engine foi presumido. Próxima etapa:
registro de componentes e migração dos acessos legados para fora do documento,
com validação e persistência por contrato. M1/M2 e build sem água seguem abertos.
Rollback desta fatia não muda o formato de cena v7.

Android: 14/14 testes Java, APK instalado com hash local/aparelho idêntico
 e80e1efbb019429df9f8d1abf66083663bf0ca32a2dc4fd4b93283e8b2625cc5.
Reaberto M1SemPacote0909k e selecionado GrupoIME; X=-2.500 preservado.
Captura build/refundacao-optional-device.png. Rios/undo foram validados no host;
essa prova Android cobre abertura da cena seca, não edição de rio no aparelho.

## Continuação — componentes registrados e arquivo v8 (09/09/2026)

Relido §12. EditorEntity agora possui coleção genérica de componentes; o traçado
foi retirado do membro específico e registrado como astra.water.route v1. O
header do documento deixa de incluir water_route.h. Inspector e extração usam
um adaptador explícito. Removido EditorOptionalValue sem consumidores restantes.

Histórico clona os payloads por contrato. Arquivo v8 usa ID/versão/payload por
componente, com leitura v1–v7 e migração de rota. Registro pode ser fornecido ao
leitor/salvamento, sem singleton mutável. Tipo desconhecido é rejeitado; salvar
valida a recarga antes de substituir arquivo. Não há descarte silencioso.

740/740 testes nativos. Dois componentes de teste não relacionados a água
passaram por documento, comandos e arquivo. Migração, cópias independentes,
undo/redo e rejeição transacional de registros inválidos passaram. Detalhes,
tradeoffs, compatibilidade e limites em adr/ADR-REFUNDACAO-COMPONENTS.md.
§12 não está concluído: outros dados e painéis de água ainda são legados, e o
build sem água não está disponível. Registro genérico de Inspector e lifecycle
de runtime ainda são etapas separadas. APK anterior não abre v8.

Validação Android desta entrega: 15/15 testes Java; Debug/Release passaram em
33s, sem warning/error encontrado no log dessa compilação. APK instalado e
hash local/aparelho conferido:
9ee2ef02657d33ab293541b5f9695a3fc31b78f16726d8a4fac876ecf9af73ac.
Projeto M1SemPacote0909k aberto v7, salvo pelo botão da UI em v8, encerrado e
reaberto; GrupoIME e X=-2.500 preservados. Backup v7:
build/refundacao-components-before-v7.aescene. Resultado:
build/refundacao-components-device-v8.aescene (153 bytes). Captura:
build/refundacao-components-reopened.png. Nenhum projeto pessoal foi migrado.
A prova de componentes de rio e casos inválidos é nativa/host; no aparelho foi
validado o fluxo de projeto seco. Não foi executado clean build nesta fatia.

## Continuação — volume de água como componente com metadados (09/09/2026)

Conferidos §12/§14 e os marcos completos do plano. Estado consolidado M0–M11
adicionado no início deste registro; nenhum gate inteiro foi promovido por
contagem de testes. Checkout continua main em eb6ea87d5973c29a9169e489ea0a106e7762899d,
com alterações de trabalho preservadas.

Implementação: removidos waterBody[7], waterPhysicsEnabled e waterInfinite de
EditorEntity. astra.water.body v1 fornece campos nomeados depth, currentX/Z,
wave/foam/ripple/opticalGain e flags. Não é um array específico deslocado para
outra aba. EditorComponentType ganhou metadados numéricos; os mesmos nomes,
limites e acessores alimentam validação, codec e a ponte com o Inspector legado.
History/documento usam a coleção genérica; extrações de renderer/mundo leem o
componente. O botão de física usa comando e informa documentChanged ao consumidor.

Compatibilidade: mantido envelope v8. O writer deixa os slots antigos de volume
em default e escreve o componente; o reader ainda migra scalars/flags antigos.
Coleções vindas dos dois trechos são combinadas sem sobrescrever tipos iguais:
dupla representação conflitante é rejeitada. Cenas v1–v8 continuam legíveis;
registro sem o tipo impede também sua entrada pela migração. Defaults de entidade
seca não alocam componente. Leitores antigos de v8 rejeitam o novo tipo se não o
registram; rollback usa o backup anterior, nunca altera apenas o cabeçalho.

Validação: 743/743 testes nativos, 15/15 Java, Debug/Release em 27s. Novos testes
cobrem metadados/valores nomeados, flags, default sem alocação, persistência,
undo/redo, recusa de valores inválidos, migração de v8 anterior, dupla representação
e publicação da mudança da UI. Regressões de consulta de água existentes passaram.
Logs build/refundacao-water-body-host-build.log, -host-tests.log e -android.log.
Build incremental, sem alegação de clean build ou de benchmark de escala.

Prova no aparelho: APK SHA256 local/instalado
32c0f5ef52b983c50e0c71f9ff15436d812b176d4b52916786d4030708992fb9.
No projeto de teste RefundacaoValidacao0909: Hierarquia + → Água → Superfície de
água → Criar; Física → desligar volume, undo/redo; Profundidade → teclado Android
→ 8 → Aplicar; Salvar; encerrar app/reabrir; selecionar superfície/Física.
Confirmados toggle desligado e profundidade 8 após reabertura. O objeto foi
criado pela UI e renderizado. Backup v6 original em
build/refundacao-water-body-legacy-before.aescene; resultado v8 de 319 bytes em
build/refundacao-water-body-device-saved.aescene; captura final em
build/refundacao-water-body-reopened.png. Somente o projeto de teste foi editado.

Limites: material/simulação espectral ainda usam water[34], waterLayout[9] e flags
legados. Ausência do componente continua resolvendo defaults para águas antigas;
não é ainda um pacote removível nem um Inspector inteiramente registrado.
Flutuabilidade/corpos rígidos continuam outra capacidade legada. Não foi provada
simulação de corpos no aparelho nesta fatia. Próxima dependência: extrair os
parâmetros restantes e seus metadados, mantendo migração e contratos de runtime.

## Correção de direção — autoria seca independente (09/09/2026)

O usuário apontou repetição indevida em água. Relido o plano: §12 determina
reintegração depois da base, não evolução contínua do efeito. Suspensa a extração
incremental de novos dados de água como prioridade. A meta permanece o plano
inteiro; este avanço não redefine a conclusão para componentes ou testes verdes.

Causa comprovada do bloqueio: InstancedRenderer::queueMapScene retornava apenas
true para listas vazias em emptyScene; drawFrame forçava zero instâncias; o cubo
usado pelo editor só existia em appendWaterAuthoringGeometry. Um projeto sem mapa
não tinha geometria autorável. Portanto continuar apenas em água não aproximava
a prova de criação seca exigida pelo plano.

Mudança: authoring_geometry.h fornece o recurso de cubo unitário, neutro, sem
headers/dados de água. Sua geometria é compartilhada por cubos, chão e blocos
transformados. A biblioteca legada de água chama esse gerador e preserva seus
flags/cor anteriores, sem duplicar o algoritmo. Recursos não são instanciados
como objetos: o documento continua apenas com o que foi criado pelo usuário.
ID 1 no catálogo independente é o cubo v1 e deve permanecer estável.

O backend Vulkan existente agora aceita essa biblioteca gerada em memória pelo
ponto initializePrimitives. Ele não lê AEMAP, texturas ou ambiente de demos;
usa ambiente neutro de pré-visualização (não objeto/luz escondidos no documento).
Reaproveita upload, PBR, buffers, publicação de poses e instancing já existentes.
As classes DirtRoadResources/flag interno ainda têm nomes legados: isso é dívida
de separação do backend, não uma alegação de que o build inteiro já removeu demos.
Fingerprint independente continua zero. Nada é salvo por nome de cena.

WaterSubpass não existe nessa biblioteca; espectro e textura de detalhe não são
criados, e o buffer de ripples passou a ser alocado apenas com subpass de água.
O catálogo seco oferece cubo/chão e omite água/flutuação; acesso residual no
navegador de recursos foi condicionado às capacidades reais. Não habilitar corpo
rígido por padrão ao instanciar o recurso seco diretamente desse navegador.

Validação nativa: 744/744, incluindo geração de 24 vértices/36 índices sem flag
de água, ausência de objeto implícito, criação pela API pública e persistência
com fingerprint zero. Logs refundacao-dry-host*. Android inicial Debug/Release
passou, e a entrega final inclui nova verificação após corrigir o acesso residual.

Prova no aparelho do caminho seco: M1SemPacote0909k abriu com GrupoIME já salvo;
+ → Geometria mostrou Cubo/Chão, sem categoria Água. Criado Cubo pela UI e visto
no renderer; criado Chão pela UI. Arquivo build/refundacao-dry-authored.aescene
contém os dois objetos e o grupo, fingerprint=0, sem componentes de água.
Capturas refundacao-dry-menu.png e -cube.png. Log -device.log confirma
primitive_library=1 packages=0 water=0 authored_objects=0 na biblioteca e
independent resources=1 fingerprint=0 managed=off na sessão.

Pendências não ocultadas: runtime jogável independente, assets importados, picking
exato, grade com oclusão, luzes autoradas, remoção física dos headers/caminhos
legados e exportação permanecem necessários. A biblioteca não fecha M3 inteiro.
O próximo trabalho deve manter o foco na criação seca e na execução genérica,
não continuar polindo a extensão de água.

Entrega final desta direção: 744/744 nativos, 15/15 Java; Debug/Release final
passaram em 26s. APK final SHA256 local/aparelho idêntico:
2adf2196196989e7c680edd2d7fb713031d5bc449deac234613f4839a728934f.
Após instalar, encerrar/reabrir o projeto independente, Cubo e Chão reapareceram
renderizados, com seus transforms e material. Captura refundacao-dry-reopened.png
e log refundacao-dry-reopened.log. Não encontrados FATAL EXCEPTION, Fatal signal,
Validation Error ou cena inválida na janela consultada desse processo.
A grade ainda atravessa superfícies; é uma pendência visível de M4, não omitida.
Progresso deste turno: alterou o fluxo real de autoria seca e comprovou resultado
no dispositivo. O objetivo completo segue ativo, sem fechamento de M0–M11.

## Picking por triângulos na autoria seca (09/09/2026)

Progresso anterior confirmado: geometria independente criada/salva/reaberta no
aparelho. Próxima causa removida, conforme §6: seleção usava apenas esferas, de
modo que a esfera enorme de um chão podia ganhar distância zero e interceptar
o toque no cubo. A esfera agora é broadphase quando existe geometria de CPU.

Decisão: BVH de triângulos na CPU, compartilhado por recurso, com transform de
instância aplicado ao raio. Evita latência/sincronização de readback de um passe
de IDs; oferece precisão geométrica para vazios e sobreposições. Faces são
selecionáveis pelos dois lados. Transparência por alpha de textura/deformações
GPU não está coberta por esse contrato; passe de IDs continua alternativa futura
para esses casos, não uma capacidade presumida.

EditorPickMesh valida posições e constrói BVH balanceado por mediana; consulta
usa pilha fixa e não aloca. Raio local não é normalizado, preservando distância
em unidades de mundo sob escala não uniforme, reflexão e shear herdado. Transform
singular é rejeitado. Limite de 262144 triângulos por biblioteca importada evita
crescimento ilimitado; a importação rejeita topologia inválida e preserva a cena
e recursos anteriores. Esse limite não constitui um benchmark de performance.

Os dados de vértices/índices gerados chegam do recurso Android até importMap e
EditorMapScene; os BVHs são construídos uma vez, e candidatos compartilham
ponteiros imutáveis. Duplicar objetos não duplica a malha de picking. O transform
usa a mesma composição da extração, incluindo o deslocamento do bounds do recurso.
IDs autorais permanecem independentes da posição do draw.

747/747 testes nativos: região vazia de triângulo, distância de superfície,
ordem entre instâncias sobrepostas, shear/reflexão, vários níveis do BVH, chão
fino/largo e índices inválidos com preservação transacional. Build Android
Debug/Release passou em 29s. Logs build/refundacao-picking-*.

Limites explícitos: recursos legados que ainda não fornecem triângulos retêm
fallback aproximado; o caminho independente seco fornece geometria completa.
Importadores deverão fornecer a mesma geometria ao contrato. M4 não foi fechado:
alpha/deformações, demais controles de viewport, grade e testes completos de
interação/performance seguem pendentes. Esta entrega não retoma trabalho em água.

Prova no aparelho Android 16 (onyx_global): APK SHA256
6de3340659c9135c11a284972850427605c1d95522850e0177c6d12ee77999e1.
Projeto M1SemPacote0909k reaberto; com ferramenta de seleção, toque na face do
cubo selecionou Cubo, apesar do chão de escala (20, 0.2, 20). Toque seguinte
fora do cubo, sobre a superfície, selecionou Chão. Hierarquia e Inspector
confirmaram ambos os resultados. Capturas: build/refundacao-picking-before.png,
build/refundacao-picking-cube.png e build/refundacao-picking-ground.png.
Na janela de log coletada em build/refundacao-picking-device.log não foram
encontrados FATAL EXCEPTION, Fatal signal, VUID ou Validation Error; isso não
substitui soak nem prova que validation layers estavam habilitadas.

## Instância de Play independente (09/09/2026)

Conforme §11, o caminho seco agora extrai o renderer de uma cópia isolada do
estado autoral, pertencente à sessão. EditorPlayScene é um adaptador do editor:
não constitui ainda o mundo físico/runtime final. A biblioteca de geometria
permanece compartilhada e imutável; os dados mutáveis têm cópia profunda.
Start valida referências antes de ativar a instância; iniciar novamente enquanto
ativa é recusado. Stop destrói os dados de execução. Import/load durante Play
são recusados para impedir substituição dos recursos sob uma instância ativa.
Falha de execução retorna à edição com mensagem genérica, sem exigir água.

Integração Android: projetos independentes publicam poses extraídas dessa cópia
em Play; ao sair, a revisão publicada é invalidada e a autoria é republicada.
O caminho legado de água ainda existe; sua remoção completa permanece no §12.
Teste de 32 ciclos altera transform de execução, confirma pose renderizada,
confirma arquivo autoral byte a byte intacto e confirma destruição em Stop.
Isso prova isolamento e extração, não física, lifecycle de scripts ou Pause/Step.
M7 permanece aberto: falta ligar corpos/colliders e personagens ao mesmo mundo,
input por ações, controles Pause/Step e validação física real na cena seca.

Validação desta alteração: 748/748 testes nativos; 15 testes Java sem falhas;
Debug/Release BUILD SUCCESSFUL em 25s. APK instalado SHA256
278c8ac1a254846334abdee070fa77732a9df2bbe7855ca5121bb09eb9175f7b.
No aparelho, projeto M1SemPacote0909k entrou em Play mostrando cubo/chão e
retornou à edição com hierarquia e geometria preservadas. Capturas
build/refundacao-play-device.png e build/refundacao-play-stopped.png.
A captura estática prova entrada/saída e publicação, não movimento físico.

## Física compartilhada na cena independente (09/09/2026)

§11/M7: a ponte Jolt existente foi auditada antes de integrar: CreateWorldV2,
CreateBody, SetMassV2, StepV2 e TryGetBodyPoseV2 existem em jolt_bridge.h/cpp.
O novo adaptador EditorScenePhysics instancia um único mundo para os corpos
estáticos/dinâmicos autorados. Não usa WaterSimulation nem CharacterMotor.
Componente astra.physics.body v1 possui colisor de caixa, modo dinâmico,
massa, meias extensões locais, atrito e restituição. É opcional, registrado
no codec v8, clonado no histórico/Play e editado por metadata numérica.
Inspector seco oferece colisor e modo dinâmico; flutuação não aparece nele.
Adicionar/remover/mudar modo passa por histórico. Dimensões do colisor são
multiplicadas pela escala mundial; transform dinâmico volta ao espaço do pai
antes da extração dos desenhos, preservando filhos visuais.

O adaptador é dono do mundo e handles, no thread da sessão. Stop destrói todos.
Passo fixo 1/60 s; retomadas limitam recuperação a 0.25 s. Configuração inicial
interna: gravidade (0,-9.81,0), 1024 corpos, 4096 pares/contatos/broadphase.
Esses limites ainda precisam alimentar Project Settings; não são presets de
qualidade nem um orçamento medido. Overflow retornado pelo Jolt falha Play.
Transform não decomponível e corpo sob ancestral físico dinâmico são recusados:
essa composição exige contrato de compound/joint, ainda não implementado.

750/750 testes nativos passaram. Teste com chão estático 20 x 0.2 x 20 e cubo
a 4 m prova queda/contato em três ciclos de 180 passos, autoria intacta e
roundtrip do componente; outro cobre adição/modo/undo pelo handler do Inspector.
Regressão de v6 mantém quantidade histórica de campos, independentemente das
novas propriedades. Android Debug/Release e testes Java passaram (31 s).

Pendentes: colliders além de caixa, triggers/layers/queries no contrato autoral,
configurações de mundo no projeto, debug draw, Pause/Step e personagem no mesmo
mundo. M7 e a remoção completa da água no build permanecem abertos.

Prova Android 16: APK SHA256
56189256a2e92f37c86ea6dfad0916ebb3b2d554bd559c16157fbfa53feefa66.
No projeto de teste M1SemPacote0909k, pela interface, foram adicionados colisor
estático ao Chão e corpo dinâmico ao Cubo, com Y=4 pelo campo numérico.
Salvar gerou arquivo v8 de 537 bytes com dois registros astra.physics.body,
inspecionado em build/refundacao-physics-authored.aescene. Play fez o cubo cair
e repousar sobre o chão. A gravação build/refundacao-physics-fall.mp4 foi
inspecionada por quatro frames em build/refundacao-physics-motion.png,
mostrando a progressão da queda. Stop restaurou Y=4 no Inspector/cena
(build/refundacao-physics-stop.png). Após encerrar/reabrir o aplicativo,
Inspector conservou colisor, modo dinâmico e parâmetros
(build/refundacao-physics-reopened.png). Não se trata de prova de personagem,
soak ou performance; tais requisitos continuam pendentes.

## Pause e Step no runtime seco (09/09/2026)

§11 relido: parar apenas a imagem não satisfaz Pause. EditorPlayScene agora
recusa Step fora da pausa, congela avanço físico enquanto pausado e oferece
um único passo de 1/60 s, permanecendo pausado. A sessão descarta tempo de
parede durante a pausa; Retomar não recupera esse intervalo. A toolbar oferece
Pausar/Retomar e Passo no Play independente; Passo só recebe toque em pausa.
Esses controles não são anunciados na rota legada de água, que não os consome.
Stop e novo Play limpam solicitações de passo e pausa.

751/751 testes: dois mundos Jolt comparados, 100 frames pausados, um Step,
30 segundos de parede ignorados e retomada com igualdade de posição física.
Isso complementa os testes de queda/colisão, isolamento e persistência.
M7 continua aberto por input/queries/personagem, limites configuráveis e soak.

Prova no aparelho: APK d33b1934e98f5a784e99249ee94afcc7fb3d01f8c939c1817ab9e14dcedb72de;
Debug/Release e testes Java passaram em 27 s. Toque em Pausar durante a queda
congelou o cubo no ar. Duas capturas separadas tiveram SHA256 idêntico
8d82ed7b84bfe8eb0604f1c924c279e3075bafa13035085c251d4233ba2e092f
(refundacao-pause-device.png e refundacao-pause-held.png em build).
Toque em Passo produziu pequeno deslocamento vertical, conservando Pausado;
a captura seguinte confirmou o estado parado (refundacao-pause-step*.png).
A igualdade visual complementa, não substitui, a comparação física nos testes.

## CharacterMotor aceita mundo compartilhado (09/09/2026)

Auditoria de §11: o motor existente sempre criava mundo privado, carregava mesh
estático e filtrava consultas por Static. A ponte CharacterVirtual já aceitava
mundo e máscara, portanto foi estendido o motor existente, sem substituir Jolt.
initializeInWorld toma um mundo emprestado, cria apenas a cápsula e consulta
Static + Dynamic (inclui cinemáticos). O chamador continua dono do mundo e do
passo dos corpos; deve atualizar motores uma vez por tick e destruí-los antes
do mundo. Shutdown destrói apenas sua cápsula nessa modalidade. O initialize
legado preserva mundo próprio e seu comportamento anterior.

752/752 testes passaram: dois motores no mesmo mundo colidem com o chão
estático e uma parede cinemática; destruir o primeiro preserva segundo motor,
parede e chão. O teste antigo do mundo privado continua passando. Não se afirma
colisão personagem-personagem: o teste cobre compartilhamento dos colliders.
Ainda falta componente autoral de personagem, Inspector, input, salto e ligação
com EditorScenePhysics/Play. Esta alteração remove a dependência de mundo
privado, mas não fecha a entrega de personagem nem M7/M8.
Build Android Debug/Release e testes Java passaram em 25 s
(build/refundacao-character-world-android.log). Sem nova prova visual no
aparelho nesta etapa: o consumidor autoral ainda precisa ser conectado.

## Personagem autoral no Play compartilhado (09/09/2026)

astra.physics.character v1 adiciona cápsula, raio, meia altura do cilindro,
altura dos olhos, velocidade e inclinação máxima. Metadata alimenta Inspector
e codec; componente é opcional e clonado com a autoria. Na cena seca, o
Inspector permite adicionar/remover personagem quando não há corpo rígido no
mesmo objeto. A cápsula usa posição autoral como pés e exige escala mundial
unitária; dimensionamento ocorre nas propriedades físicas. Combinação de corpo
rígido e personagem na mesma entidade é recusada pelo runtime.

EditorScenePhysics instancia os motores após os corpos, atualiza-os antes do
passo Jolt compartilhado e publica posições locais antes da extração. Destrói
motores antes do mundo. Limite inicial de 32 personagens; ainda não exposto em
Project Settings. O personagem selecionado ao entrar em Play recebe Move pelo
gesto na metade esquerda, reutilizando FirstPersonTouchControls e a mesma
conversão de viewport. Eixos atuais são mundiais X/Z; não há câmera de primeira
pessoa implícita nem jogador oculto. Outros personagens também simulam gravidade.
Pause cancela o gesto, e Step usa o mesmo passo compartilhado.

753/753 testes: componente salvo/reaberto, pés apoiados no chão autorado,
velocidade configurada de 2 m/s produz cerca de 2 m em 60 ticks, autoria intacta
e reinício restaura posição. Pendentes: salto, câmera vinculada, input map
configurável/múltiplos jogadores, visualização da cápsula/joystick, interações
entre personagens, rampas e plataforma validadas na autoria. M8 permanece aberto.

Prova Android: Debug/Release e testes Java passaram em 28 s; APK SHA256
2f12c80f2b337dfa088691aaebedf2275785b9113dc2a3f82924992f2b9190b4.
No projeto M1SemPacote0909k, pela interface, foi removido o corpo do cubo,
o cubo foi reparentado ao grupo com preservação mundial, depois ajustado
localmente para (0,1,-0.139767915). A cápsula foi adicionada ao grupo, mantendo
o chão estático. Grupo selecionado em Play recebeu gesto 400,900 -> 700,900
em 800 ms; a malha filha se deslocou na cena (capturas
build/refundacao-character-play-before.png e refundacao-character-play-moved.png).
Arquivo autoral v8 de 572 bytes inspecionado em
build/refundacao-character-authored.aescene conserva posição do grupo -2.5,0,0,
um componente character no grupo, body no chão e malha filha sem corpo.
A malha é apenas representação de teste, não um personagem visual final.
Encerrar e reabrir no Android preservou o componente, os valores do Inspector
e a hierarquia com malha filha (build/refundacao-character-reopened.png).

## Salto do personagem (09/09/2026)

Personagem v2 acrescenta velocidade de salto em m/s (0 desliga a capacidade).
Leitura v1 preservada com padrão 5 m/s; propriedades anteriores mantêm IDs.
A ação é aceita somente OnGround, enfileirada até o próximo tick e consumida
uma vez. Solicitação duplicada antes do tick e salto no ar são recusados.
Pausar não aceita salto; a ação não escreve na autoria. No Play, botão Saltar
aparece apenas para o personagem controlado com velocidade positiva. Cancel
Android agora também limpa o gesto Move da sessão.

754/754 testes: leitura v1, roundtrip v2, velocidade configurada de 4 m/s,
ápex entre 0.7 e 1 m sob gravidade 9.81, recusa de duplo impulso e salto no ar,
pouso no colisor e recusa durante pausa. M8 ainda requer câmera/input map,
rampas/plataformas/trigger/comportamento e animação autorados e validados.

Android: Debug/Release e testes Java passaram em 33 s. APK instalado SHA256
19b125a79b0903b262f945e4462acf5716219ee27d5f3cfa034215b267a99da4.
O projeto com personagem v1 abriu e executou. Toque no botão Saltar produziu
salto e retorno ao chão. Gravação build/refundacao-jump.mp4 inspecionada por
frames em build/refundacao-jump-motion.png; captura antes em
build/refundacao-jump-before.png. Stop retornou à autoria. Não constitui prova
de salto em rampa/plataforma nem de todos os layouts multitouch.

## Câmera de jogo acompanha a execução (09/09/2026)

A inspeção encontrou resolveSceneCamera já funcional para hierarquia, porém o
Android o chamava sobre a autoria durante Play. sceneCameraPose agora consulta
a cópia de execução quando ativa, mantendo fallback autoral fora do Play.
Uma câmera filha acompanha o personagem simulado pelo mesmo transform central.
Move usa yaw da câmera ativa (+Z frontal) em vez de eixos mundiais fixos quando
há câmera autorada. Nenhuma câmera é criada automaticamente.

755/755 testes: câmera filha a 1.65 m com yaw 90 graus, personagem movido por
MoveForward e câmera deslocada cerca de 2 m em X, preservando altura e autoria.
Câmera ativa ainda é o menor ID habilitado; prioridade, projeção configurável,
roll e controle de olhar não estão cobertos por esta integração. M8 continua
aberto. A mudança corrige o consumidor real Android, não só uma API de teste.

Android Debug/Release e testes Java passaram em 26 s; APK SHA256
2919ec0816a9186bf56e08493548af96ba4831180f9d8e442e7d857d70a41c39.
Câmera criada pelo catálogo e reparentada ao grupo do personagem na UI. Antes
de vincular, câmera fixa manteve o chão imóvel enquanto personagem andava;
depois de vincular, mesmo gesto manteve personagem enquadrado e deslocou o
chão na imagem. Evidências válidas: build/refundacao-camera-parented.png,
refundacao-camera-follow-before.png e refundacao-camera-follow-moved.png.
Arquivo salvo inspecionado em build/refundacao-camera-authored.aescene;
Stop retornou à autoria. As primeiras capturas camera-play-* são o controle
com câmera ainda na raiz, não evidência de acompanhamento.

## Controle de olhar opcional (09/09/2026)

astra.camera.look v1 separa a câmera do controle de olhar. É adicionado/removido
pelo Inspector de câmera, com histórico, e salva sensibilidade horizontal e
vertical (graus por tela) e limite vertical. A ação usa deltas normalizados do
viewport; não multiplica por delta de frame. Rotação local respeita o rig pai,
yaw é reduzido por volta completa e pitch limitado. Não cria controlador em
câmeras antigas. Gesto direito funciona mesmo sem personagem selecionado quando
a câmera ativa tem controlador; Move continua pertencendo ao personagem.
Pause limpa gestos e nenhum dado de autoria é alterado pelo olhar em Play.

756/756 testes: câmera sem controlador recusa ação, componente salva/reabre,
sensibilidade e limites superior/inferior, yaw após múltiplas voltas e autoria
intacta. Ainda falta configuração completa de input, prioridade/projeção das
câmeras, colisão do rig e validação ampla de layouts/gestos simultâneos.

## Inspector por componentes e regressão de lifecycle (09/09/2026)

Correção de direção solicitada pelo usuário: adicionar/remover capacidades não
é editar um booleano. No fluxo seco, o Inspector agora oferece Adicionar
componente e cabeçalhos recolhíveis. Novos componentes começam fechados; tocar
abre os campos e tocar novamente recolhe. Remover usa comando explícito com
Undo/Redo. Booleanos reais (por exemplo Dinâmico) continuam sendo propriedades.

Referências relidas nesta revisão:
- Unity 6, Use components: https://docs.unity3d.com/6000.0/Documentation/Manual/UsingComponents.html
  confirma catálogo Add Component, composição e propriedades no Inspector.
- Godot, Inspector Dock: https://docs.godotengine.org/en/stable/tutorials/editor/inspector_dock.html
  confirma seções recolhíveis; seu modelo de nós não é tratado como o modelo
  de componentes da Unity. Começar fechado é uma decisão solicitada para Astra,
  não uma afirmação de padrão universal nas referências.

editor_component_catalog.h concentra nome, tipo registrado, grupo de propriedades
já existente, aplicabilidade e acesso booleano. O painel percorre esse catálogo;
a serialização registra os mesmos tipos a partir dele. Corpo físico de caixa,
personagem cápsula e controle de olhar usam o mesmo caminho de adição/remoção.
Componentes repetidos são recusados. Corpo e personagem são incompatíveis na
implementação atual; olhar requer câmera porque o consumidor real exige câmera.
Nenhuma capacidade de outra engine foi adicionada ficticiamente ao catálogo.

Estado aberto/fechado pertence somente à sessão de edição, com TypeId estável;
não suja a cena. Adição/remoção e propriedades usam EditorHistory e o arquivo v8
existente, sem migração de formato. Um componente fica aberto por vez nesta
primeira adaptação de altura mobile. Campos ainda usam o adaptador numérico
legado: reflexão de todos os tipos, busca de componentes, multiedição, reset e
migração de Transform/material/câmera para componentes continuam pendentes.
O fluxo legado de água ainda usa seus painéis anteriores e não passou no gate §12.

758/758 testes host passaram. Regressão de UI percorre catálogo, adição fechada,
abertura sem modificar autoria, booleano configurável, combinações indisponíveis,
remoção e Undo/Redo. Testes físicos agora executam 100 ciclos de queda/colisão;
outra prova executa 100 inicializações parcialmente inválidas (após criar mundo,
chão e primeiro personagem), recuperação, movimento/salto e Stop repetido.
Isso prova recuperação funcional, não ausência medida de vazamento nativo/GPU.
Logs: build/refundacao-components-host-build.log, refundacao-components-host-tests.log,
refundacao-components-android.log. Debug/Release e testes Java passaram.

Validação do olhar anterior: build/refundacao-look-turned.png mostra gesto de
olhar em Play; refundacao-look-stop-final.png confirma retorno à edição.
Arquivo salvo refundacao-look-stop.aescene mantém a rotação autorada da câmera
(25.7831001, 34.3774681, 0) e astra.camera.look v1 com valores 300/195/83.

Prova Android desta UI: onyx_global, Android 16, 2772x1280, projeto seco
M1SemPacote0909k. APK SHA-256
2b17ccbdaa21cd4081361f8400948c393104435a62e95fd09ddc2c8c53c2c61f,
Debug/Release + Java concluídos em 26 s. Inspeção visual confirma catálogo
(refundacao-components-catalog.png), componente recolhido
(refundacao-components-closed.png) e campos abertos
(refundacao-components-final.png). Remoção e readição foram acionadas pela UI;
refundacao-components-authored.aescene contém novamente astra.camera.look v1
300/195/83 após salvar. O teste host comprova especificamente o estado fechado
imediatamente após adicionar; as capturas intermediárias added-closed/expanded
não devem ser interpretadas pelo nome: mostram estados inversos aos nomes.

## Valores universais e identidade dos componentes (09/09/2026)

Copiar, Colar e Restaurar usam o contrato EditorComponentValue e
EditorComponents::replace, sem lógica por tipo no painel. O clipboard guarda
um clone imutável apenas durante a sessão. Colar exige o mesmo descritor e
um componente já anexado; valida o clone antes de trocar valores e mantém
ordem e componentes adjacentes. Restaurar usa a fábrica registrada. Alterações
passam por EditorHistory; copiar e abrir/recolher não alteram autoria.
759/759 testes: cópia não suja cena, reset restaura número e booleano, clipboard
preserva os valores anteriores ao reset, colagem não compartilha estado mutável,
Undo/Redo restaura o destino. Isso substitui a pendência dessas três ações para
os componentes atualmente registrados; a reflexão geral continua incompleta.

Adendo visual autorizado: nomes curtos Corpo físico, Personagem e Olhar;
descrições separam nome e responsabilidade. Add recebeu texto curto e ícone
raster próprio. Cada componente recebeu ícone dimensional PNG com alpha,
verde-lima/branco/grafite inspirados na marca inspecionada. Fontes estão em
assets/astra-visual/icons/named/component; prompts finais e ferramenta usada
estão em REFUNDACAO-COMPONENT-ICONS.md. O atlas foi regenerado pelo empacotador
existente: 152 entradas, 1024x1570, 6281 KiB, células 96 px; não se carregam as
quatro imagens grandes separadamente no runtime. IDs de tipos e formato de
cena permanecem iguais; somente os identificadores gráficos gerados mudaram
junto do atlas e dos consumidores compilados.

Validação final do adendo: Android 16/onyx_global 2772x1280; APK instalado
6ad9dc5fa587259c82e7ce1f59a809370ce9a9eb0cc6ac1cadd9162d62de828e.
Debug/Release e Java: BUILD SUCCESSFUL em 1m18s; host: 759/759.
Capturas inspecionadas refundacao-icons-card.png e refundacao-icons-fields.png
confirmam Add com imagem, cartão recolhido/aberto, título/descrição separados e
campos/ações legíveis neste layout. Logs refundacao-component-icons-* em build.
Não equivale à validação de todos os tamanhos de painel nem ao fechamento M5.

## Contratos de cena sem dependência do editor (09/09/2026)

A base de valores/coleções e os dados de corpo físico, personagem e olhar foram
extraídos para native/scene. Os headers existentes do editor são adaptadores
com aliases; a definição e validação continuam únicas. IDs, versões, campos
serializados, ícones e layout foram preservados. A lista de includes da nova
camada foi inspecionada: core/base.h e STL, sem editor/renderer/UI/água.

760/760 testes host passaram, incluindo uma unidade de compilação que usa os
três contratos sem importar o editor e prova carregamento, isolamento e falha
transacional. Debug/Release + testes Java passaram em 37 s. Logs em
build/refundacao-scene-contract-*. Não houve nova validação no aparelho nesta
fatia de separação de dados; a última inspeção visual pertence ao APK anterior.
Decisão e limites em adr/ADR-REFUNDACAO-SCENE-COMPONENTS.md. Runtime completo e
retirada dos campos de água da entidade permanecem pendentes.

## Fronteira histórica necessária à retirada da água (09/09/2026)

Auditoria dos campos globais confirmou acessos em editor_document.h/.cpp,
editor_properties.h, editor_archive.cpp e android_main.cpp. As três flags e os
43 floats ainda estão na entidade: esta auditoria não declara a extração feita.
Referências completas da revisão em build/water-field-references.txt.

Foi corrigido um defeito concreto na migração: o leitor v7 usava o tamanho da
tabela ATUAL do Inspector, aceitando IDs de corpo/personagem/olhar acrescentados
posteriormente. Agora v6/v7 usam o limite histórico 207 e v8 usa 79 para a seção
escalar. O limite de contagem também respeita a versão. Não há mudança no
escritor nem nos payloads válidos. A regressão recusa IDs 207, 213 e 219 em v7 e
verifica que a cena de destino continua intacta; migração de rotas antigas
continua coberta pela suíte existente. Host: 761/761.

A próxima migração deve retirar armazenamento e consumidores diretos em
conjunto, com campos tipados e leitor histórico dedicado. Não basta encapsular
os arrays num componente ou renomear o diretório; o gate §12 continua exigindo
build sem pacote de água e execução/distribuição do projeto seco.

## Armazenamento de água retirado da entidade (09/09/2026)

Removidos os arrays de 34+9 floats e as três flags de EditorEntity, junto de
suas cópias manuais em EditorDocument. Compatibilidade antiga passa por dados
opcionais de campos nomeados; entidades secas não carregam esse payload.
O arquivo v8 mantém uma única representação histórica, com roundtrip exato.
762/762 testes e Debug/Release + Java passaram (24 s). ADR completo:
adr/ADR-REFUNDACAO-WATER-STORAGE.md. Não fecha a remoção do pacote do build:
ainda existem codec/ponte/painéis legados e enum Water. A mudança removeu o
armazenamento obrigatório real, sem alegar que toda a seção 12 está concluída.

Prova no aparelho: APK b81047504fc3d01229636e5f5bc80f3a64ef0e64c06219771b427075d94c52eb,
Android 16/onyx_global, 2772x1280. Projeto seco M1SemPacote0909k abriu e
renderizou (refundacao-water-storage-device.png). Fechar/reabrir/salvar preservou
o arquivo atual byte a byte: SHA-256
2d47454dfcbc60b9c65e2cf373bcc8a5891a44795b6ffb4e33df3d1c4f5c35ad
em refundacao-water-storage-device.aescene e refundacao-water-storage-reopened.aescene.
O snapshot antigo refundacao-components-authored.aescene tinha outra posição
para o grupo e outro material do cubo; não foi usado como baseline deste ciclo,
nem restaurado sobre as alterações atuais. A origem dessas diferenças anteriores
não foi comprovada nesta execução. Compatibilidade histórica permanece comprovada
pelos testes de codec, separadamente da prova no dispositivo.

## Pacotes de demonstração fora do APK padrão (09/09/2026)

prepareEngineAssets não copia nem exige manifests de ocean, dirt_road ou
material_preview por padrão. A tarefa Sync remove sobras desses pacotes no
próprio diretório gerado; fontes e projetos pessoais não são apagados.
O empacotamento verifica a ausência dos três diretórios no modo padrão.
AstraShellActivity recusa explicitamente projetos dependentes de pacote legado
nesse build, preservando arquivos em vez de carregar uma cena substituta.

Para regressão/migração, o desenvolvedor pode usar o argumento PowerShell
'-Pastra.includeLegacyDemos=true'. A opção aceita somente true/false e gera
BuildConfig.INCLUDE_LEGACY_DEMOS para manter UI e empacotamento coerentes.
prepareEngineAssets com a opção true passou em 4 s, incluindo a validação dos
hashes de todos os pacotes; em seguida o build padrão foi restaurado e verificado.
Isso é uma opção de build para legado, não um novo preset de projeto no produto.

Debug/Release + testes Java passaram. Inspeção ZIP dos APKs finais confirmou
zero entradas assets/ocean/, assets/dirt_road/ e assets/material_preview/ em
ambas as variantes. APK Debug instalado:
0b523c7dc703e7d9c514c40afc6fca49342ad82344966a768e5c26d55594f06d.
Android 16/onyx_global 2772x1280: projeto seco existente abriu, renderizou e
entrou em Play com personagem/câmera; capturas inspecionadas
build/refundacao-no-demo-assets.png e refundacao-no-demo-play.png.
Logs: refundacao-no-demo-assets-android.log, refundacao-no-demo-assets-final.log,
refundacao-legacy-assets-opt-in.log. A última suíte nativa continua 762/762;
essa alteração de empacotamento não modificou C++.

Limite: bibliotecas/código nativo de água e os runtimes legados ainda estão no
build. Ausência dos pacotes de assets não prova a compilação sem o módulo de
água nem a exportação de projetos, que continuam abertas no plano.

## Editor Godot embarcado retirado do produto (09/09/2026)

Auditoria confirmou ausência de consumidores de GodotEditorProject/Activity
fora dos próprios dois arquivos aposentados. O shell abre AetherActivity.
Foram removidos do build o AAR godot-editor-ui, a tarefa prepareGodotUi, o tema
.tres empacotado, a atividade no manifesto e dependências Java exclusivas sem
consumidores atuais (fragment/documentfile/kotlin-stdlib diretos). Os dois fontes
foram preservados em integrations/godot/retired/android, fora do source set.
Fontes vendorizadas, licenças, ferramentas e projetos pessoais não foram apagados.

Debug/Release e testes Java passaram em 12 s. Inspeção dos dois APKs confirmou
zero entradas com godot e ausência de org/godotengine nos arquivos DEX.
Release: 23.548.186 bytes; APK anterior sem pacotes de demos: 101.989.909 bytes.
Essa comparação mede estes dois builds, não orçamento universal do aplicativo.
Debug inicialmente manteve espaço excedente no contêiner incremental; não foi
tratado como artefato de distribuição. C++ não mudou nesta fatia; última suíte
nativa: 762/762. Log: build/refundacao-native-only-editor.log.

Este passo remove o segundo editor embarcado exigido pelo plano. Não declara
concluídas a retirada do código nativo de água, a exportação ou os demais gates.

Conferência adicional corrigiu a hipótese inicial sobre o tamanho Debug: suas
216 entradas ZIP somavam 36.246.988 bytes comprimidos, enquanto o contêiner tinha
527.249.072 bytes. Após preservar o APK antigo em build/refundacao-debug-before-repack.apk
e gerar novamente o artefato com assembleDebug, o resultado ficou em 36.297.680
bytes. Nomes, CRC e tamanhos descomprimidos de todas as entradas são idênticos.
Logo o excesso não era causado pelo conjunto de bibliotecas presente, mas por
espaço do contêiner incremental. Nenhuma otimização do renderer foi inferida.
O APK anterior instalado abriu o projeto seco normalmente no Android (captura
refundacao-native-only-editor.png); o APK reempacotado contém os mesmos arquivos.

APK Debug final instalado: af1821ed1b00fe45d901dde7ff640a8559424cca64e1f248acecdb9fd9043f09.


## Build Android após limpeza (09/09/2026)

Executado `gradlew.bat clean :app:testDebugUnitTest :app:assembleDebug :app:assembleRelease`:
BUILD SUCCESSFUL em 1m49s, 97 tarefas (52 executadas, 43 de cache, 2 atualizadas).
É uma limpeza dos outputs Gradle, com reutilização declarada de cache; não é prova
isolada de compilação sem caches. Log: build/refundacao-clean-android.log.
Os relatórios XML confirmam 15 testes Java, zero falhas/erros/ignorados.

Inspeção ZIP e DEX dos dois APKs: nenhum pacote de assets ocean/dirt_road/material_preview,
nenhuma entrada Godot e nenhuma referência org/godotengine nos DEX.
Debug: 36.297.672 bytes, SHA256 0ffcb878ac552e149e05d7ab23b180cdfde4d339d414b8874677f9b2bc1e5eb7.
Release: 23.548.190 bytes, SHA256 75d1d50b932f1ae2fa09e90facfd44c3c92645089954a2702feda77c375f1700.
O Debug foi instalado com sucesso no dispositivo ADB transporte 1.
A interação visual foi comprovada nas etapas anteriores; esta execução acrescenta
build após limpeza, inspeção de conteúdo e instalação, sem alegar novo ensaio visual.

`git diff --check` passou no escopo de fontes, testes, documentação e configuração
alterados. A verificação global encontra whitespace nos outputs CMake gerados e
rastreados em android/app/.cxx; esses artefatos não foram reformatados como fonte.
Não houve mudança C++ nesta etapa; última suíte nativa permanece 762/762.
Os gates ainda abertos do plano continuam abertos, incluindo ausência do código
nativo de água no build, autoria completa de assets/luzes e exportação de projetos.

## Plataforma Android sem laboratório no build padrão (10/09/2026)

O painel Java antigo não apenas ocupava o APK: AetherActivity.onCreate construía
seus controles mesmo com editor_ui ativo, chamava apply() e publicava defaults
do laboratório no nativo. A Activity padrão agora contém apenas lifecycle,
registro JNI e ponte de texto/IME. Os dois fontes do laboratório foram movidos
para src/legacy/java, selecionado exclusivamente por astra.includeLegacyDemos=true.
Um manifesto de overlay adiciona o seletor técnico somente nesse build. O manifesto
principal mantém o editor não exportado: o shell valida e abre o projeto.

No caminho nativo independente, applyRuntimeControls não consulta/publica o
estado do laboratório nem monta configurações espectrais. A política gráfica
continua resolvida por renderingSettings, capacidades e pressão térmica;
waterAuthoring/spectralWater e os diagnósticos de água não são ativados ali.
Isso remove a interferência do laboratório, mas ainda não remove seus fontes
nativos do link. A exigência integral da seção 12 permanece aberta.

Build padrão Debug/Release e 15 testes Java passaram. A compilação Java e o
manifesto do modo legado também passaram; não foi executado o laboratório no
aparelho nesta etapa. DEX dos APKs padrão não contêm SceneLauncherActivity,
nativeApplyControls, water_controls_v3 ou org/godotengine; ZIP não contém os
três pacotes de demos. Logs: refundacao-editor-platform-final.log,
refundacao-editor-platform-jni.log, refundacao-editor-platform-legacy.log.
APK Debug instalado: 37414374e6d6ee4e9240e8fc240ac95b3293943ed8ad1ce47498490750b200e0.
Release: 7be97f555a940f36765a3ab9f45f928d710b2e6d177c48c32dc686647022eb3c.
Android 16/onyx: projeto M1SemPacote0909k reaberto no editor e em Play, com
personagem/câmera e botão Saltar. Capturas inspecionadas:
refundacao-platform-editor.png e refundacao-platform-play.png.
Log real confirma independent resources=1 fingerprint=0 managed=off,
5 entidades e política térmica aplicada. Não é prova de exportação nem de
compilação sem o módulo nativo de água. Fontes C++ alterados nesta etapa são
exclusivos do Android e foram compilados para ambas as variantes.

## Revisão 2 — direção e implementação corrente (10/09/2026)

A referência de requisitos passa a ser PROMPT_REFUNDACAO_ASTRA_V2.md, acompanhada
por ASTRA_COMPONENTES_CODIGO_EDITOR.md, ambos em Downloads. Comparação com o plano
anterior confirmou acréscimos de composição/schema/scripting, retirada da barra
Copiar/Colar/Restaurar e marcos S0–S8. O catálogo de referências foi consultado em
https://github.com/kacerato/reposplugins/blob/main/docs/unity-godot/README.md e seu
índice docs/README.md. Conteúdo de plugins e listas de outras engines não foram
importados nem considerados capacidades da Astra. Referências primárias conferidas:
Unity PhysicsModule 6000.0 e Godot EditorInspectorPlugin (documentação stable).

A instrução direta do usuário de 10/09 prioriza blocos de implementação e checks
curtos agora; baterias extensas ficam para a consolidação. Não reabre gates já
comprovados nem permite declarar pronta uma capacidade ainda sem execução.

Implementado neste bloco:
- Propriedades numéricas de Corpo físico/Personagem/Olhar têm IDs textuais explícitos,
  independentes dos nomes visíveis e membros C++. Dinâmico é metadado booleano
  do mesmo tipo; o catálogo do Inspector não possui mais callbacks físicos próprios.
- scene/component_properties.h fornece edição tipada por TypeId + PropertyId,
  com checagem de tipo, limites e invariantes antes de substituir valores. O comando
  EditorAction::ComponentProperty expõe a mesma edição com versão de cena e histórico.
  Os widgets numéricos legados encaminham as três famílias para esse serviço.
- O Inspector removeu a barra Copiar/Colar/Restaurar. O cabeçalho oferece menu
  contextual de remoção, os campos ganham espaço e Add permanece único no rodapé.
  Funções de clipboard antigas permanecem disponíveis no contrato interno, sem
  controles permanentes. Ícones próprios e componentes inicialmente recolhidos mantidos.
- MissingComponent preserva TypeId, versão e payload de tipo não registrado no
  arquivo. Clones/histórico mantêm posse independente. O Inspector sinaliza a ausência;
  Play recusa a cena incompleta. Reabrir com registro disponível recupera o tipo.
  Dados inválidos de tipos conhecidos e registros ambíguos continuam recusados.

Verificação curta: build host passou; smoke temporário build/refundacao-v2-smoke.cpp
compilou/executou verificando edição numérica/booleana, erro de tipo, invariante
cruzada e recuperação de payload quando o registro reaparece. APK Debug compilou
em 8 s e foi instalado: SHA256
8aaff137053b445fc38cb9fc698f25495d1a9f44d1e1000103852951ecc03e96.
Log Android: build/refundacao-v2-authoring-android.log. A expectativa antiga de
rejeitar tipos ausentes no teste de arquivo foi atualizada para o contrato da V2;
a suíte completa não foi reexecutada neste bloco por orientação do usuário.

Limites explícitos: os IDs novos são usados pela API de edição; os payloads conhecidos
continuam no formato versionado existente, cuja migração completa para PropertyBag
está pendente. Ainda há adaptador numérico por índice na UI e propriedades de água
no legado. Faltam cardinalidade múltipla, referências tipadas e registro dinâmico
de componentes de projeto. MissingComponent não implementa execução de script.
S0–S8 foram incorporados como requisitos, não como entregas concluídas. Separação
corpo/colisor, modos cinemáticos, CodeWorkspace e execução C# local ainda requerem
implementação. A remoção completa do módulo nativo de água continua pendente.

Conferência visual curta da V2 no Android: cabeçalho recolhível, menu de remoção,
campos numéricos/booleano e Add no rodapé foram abertos e inspecionados em
build/refundacao-v2-fields.png. O glifo de reticências não existia no atlas de
texto; foi substituído pelo ícone EditorAuthorMore já disponível e conferido
no aparelho. APK Debug final desta etapa:
80a47aefb394a2891222bf4dd81dbf1d2ecb765faec721d1fdc451473e066db1.
Build final: refundacao-v2-inspector-icon.log (4 s). Nenhuma bateria extensa
foi executada depois da orientação de priorizar implementação.

## V2 — corpo, colisor e movimento separados (10/09/2026)

Bloco integrado dos contratos M2/M8 e §§17.3/18, após reler a V2 e o complemento:
- `scene::PhysicsBody` v2 contém movimento Estático/Cinemático/Dinâmico,
  massa, atrito, restituição e velocidade inicial em coordenadas globais.
  Não contém dimensões da forma nem o antigo booleano `dynamic`.
- `scene::Collider` v1 é anexável independentemente de malha e define Caixa,
  Esfera ou Cápsula Y. Dimensões são locais; escala mundial é aplicada ao criar
  a forma física. O corpo e o colisor devem coexistir no mesmo objeto para Play.
- A migração versionada do registro desdobra Body v1 em Body v2 + Collider v1,
  preservando movimento, massa, dimensões e material físico. Leitura é transacional;
  versões não suportadas, duplicações e dados inválidos são recusados. Escrever a
  cena mantém AETHER_EDITOR 8, com versões próprias dos componentes.
- Metadados `ComponentEnum` e edição tipada `u32` fazem parte do mesmo contrato
  TypeId/PropertyId. Inspector e comandos validam opções antes de alterar dados;
  seleção de forma/movimento participa do histórico. Não foram criados componentes
  sem consumidor para preencher o catálogo das engines de referência.
- Inspector mantém Add no rodapé e novos componentes recolhidos. Campos abrem
  imediatamente abaixo do cabeçalho correto; dimensões irrelevantes ficam ocultas
  sem apagar seus valores. Massa aparece para Dinâmico; velocidade para corpos
  móveis. Página única não desenha navegação. Colisor 3D recebeu ícone raster próprio.
- `EditorScenePhysics` cria as três formas pela ABI Jolt V2 no mundo compartilhado,
  usa os três modos de corpo, aplica massa apenas ao dinâmico e velocidade inicial
  aos móveis. Poses dinâmicas/cinemáticas voltam à cópia de execução, sem escrever
  no documento autoral. Falhas de composição/escala identificam objeto e motivo;
  o status agora possui sua string para evitar ponteiros pendentes.

Evidência de capacidade: enums, criação V2 e SetLinearVelocity em
native/physics/jolt_bridge.h/.cpp, consumidores reais do Jolt vendorizado.
Referências de organização, não prova de suporte Astra: catálogo
https://github.com/kacerato/reposplugins e Unity PhysicsModule / Godot
CollisionShape3D. O plano completo continua ativo.

Verificação curta: build host passou, Debug Android passou em 15 s. Smoke separado
build/refundacao-collider-smoke.cpp executou migração, enum tipado, recusa de valores,
queda/contato das três formas, deslocamento cinemático sem gravidade e preservação
integral da autoria. Também verifica erro explícito quando falta colisor.
Logs: build/refundacao-collider-host.log, refundacao-collider-android.log e
refundacao-collider-smoke.log. A suíte completa não foi executada neste bloco.
APK instalado: f456f51c39a49be800c100df9bb30f9b0abb17f8ae6ac5d4accfb235fc315f2b.

Limites desta implementação: um corpo e um colisor no mesmo objeto; sem compound,
pose local do colisor, ShapeAsset compartilhável, filtros, sensores/eventos ou joints
no catálogo. Esfera/cápsula exigem escala global uniforme; caixa aceita escala não
uniforme, mas shear é recusado. Corpo filho de corpo móvel requer contrato futuro.
Movimento cinemático nesta fatia é velocidade inicial global constante; API de alvos
movidos por scripts ainda não está conectada à autoria. Personagem conserva sua
cápsula própria. Esses limites não encerram §§17–19, scripting S0–S8, exportação ou
remoção completa do módulo nativo de água.

Conferência Android desta fatia: captura refundacao-collider-closed.png mostra
Body v1 migrado em dois cabeçalhos recolhidos. Toque em Colisor abre os campos;
alternar para Esfera mostra somente Raio (refundacao-collider-sphere.png).
Play nessa esfera, no chão de escala 20/0,2/20, foi recusado com diagnóstico
"Chão: esfera e cápsula requerem escala global uniforme" (invalid-scale.png).
Undo restaurou Caixa e Play voltou a executar (refundacao-collider-play.png).
O teste foi encerrado com Stop. Não foi feita bateria prolongada no aparelho.

Arquivo atual foi copiado antes da instalação: 793 bytes, SHA256
108d4e2feb742730810dcb2380f75502dea89466f457e5116315c1a1368c954d.
Essa versão já tinha material modificado e um Personagem no Cubo; ambos foram
preservados. Comparação textual após salvar confirmou que a única diferença é
a migração explícita do componente do chão (novo arquivo 838 bytes). Não foi
restaurado o snapshot antigo de 720 bytes. Evidências before/after em build.


## 10/09/2026 — Bloco amplo de componentes, IDE e scripting (sem execução autorizada)

Releitura da V2 e do complemento de componentes/código. A orientação direta do usuário passa a ser implementar em blocos amplos, sem alternar uma pequena mudança com testes, e **testar somente com autorização explícita**. Pedido salvo na nota de memória `20260910-115830-astra-implementacao-ampla-testes-autorizados.md`. Após essa orientação não foram executados builds, testes, APKs, smoke checks ou ações no dispositivo. Leitura de código, consulta de documentação e geração dos documentos continuam sendo trabalho de implementação/pesquisa.

O quadro principal está em [componentes/README.md](componentes/README.md), com [117 entradas Unity](componentes/unity.md), [153 componentes ItsMagic](componentes/itsmagic.md) e [JSON com evidências](componentes/catalogo.json). Há mais [253 declarações em sete pacotes Unity](componentes/pacotes-unity.md), ainda sem curadoria funcional completa. Não somar essas listas para alegar componentes implementados. UI Toolkit, classes-base, serviços e recursos não viram botões Add indiscriminadamente. Lacunas de documentação ItsMagic são registradas, especialmente componentes que só têm construtores/atributos genéricos.

Código escrito neste bloco:

- `native/scene/components.h`: identidade persistente por instância, cardinalidade repetível, busca/remoção/substituição por instanceId, cópia profunda e preservação de IDs. API de propriedades aceita instanceId e recusa seleção ambígua por tipo.
- `native/editor/editor_archive.cpp`: cena v9 grava instanceId e próximo ID; leitura das versões anteriores continua, com migração de corpo/colisor. Tipos desconhecidos continuam preservados.
- `native/scene/script_behavior.h`: anexos C# repetíveis, enabled e overrides tipados por PropertyId. Editor oferece anexar, recolher/expandir, remover, abrir origem e editar campos do schema compilado. A duplicação da subárvore remapeia referências internas a objetos.
- `editor_code_workspace.*`, filesystem, screen/session e `EditorTextInput.java`: múltiplos arquivos, criação de classe C#, edição Android, Undo/Redo de texto, busca, salvar, detecção de conflito externo e aplicação de código. Histórico de texto é separado do histórico de cena.
- `managed/Astra.Scripting`: compilador Roslyn 4.14.0/C#12, análise semântica de Behavior/atributos, DLL/PDB/schema, publicação atômica por geração e carregamento da compilação aplicada. Integração de dependências no publish Android, sem executar restore/build nesta rodada.
- `scene/script_runtime.h`, `editor_script_bridge.*`, `BehaviorWorld` e `NativeBehaviorRuntime`: ABI de acesso à cena, instâncias do assembly por sessão Play, Start/Update/FixedUpdate/Stop, diagnóstico por instância e callbacks de transform local, velocidade global e alvo cinemático. Acesso à cena limitado à thread dona; Stop desvincula instâncias. O adaptador ainda usa EditorDocument como cópia de execução, portanto M9 não está encerrado.
- Física sincroniza poses depois de cada subpasso para os próximos callbacks FixedUpdate. API de movimento cinemático utiliza o passo fixo de 1/60 s.
- `editor_collider_fit.h`, Collider v2 e ponte Jolt: ajuste geométrico por vértices, sugestão de caixa/esfera/cápsula envolvente, centro local persistente e editável. Nova função de criação com centro local mantém os layouts ABI V1/V2. A forma sugerida não é reconhecimento semântico do objeto nem collider triangular.
- O inspetor pagina componentes e campos em vez de deixar anexos adicionais fora da área disponível; componentes novos continuam fechados e o acesso permanece ícone + Add.

Estado: **implementação escrita, não compilada/não executada**. Não reutilizar resultado de build ou captura anterior como evidência desses caminhos. As limitações específicas, fronteiras de threads/vida útil, falta de hot reload, debugger, seletores de referências, isolamento de scripts, módulos físicos/gráficos adicionais e exportação estão detalhadas no README do quadro. A refundação permanece em andamento; registrar funções no roadmap não equivale a implementá-las.

A anexação nativa também passa pela fronteira `EditorAction::AddComponent`. Para colisor com geometria disponível, a sugestão envolvente é preparada na mesma operação de Undo da anexação; o botão Ajustar à malha permite recalcular depois. Sem geometria compatível, a forma padrão continua explicitamente editável.


## 10/09/2026 — Câmera e malha por composição; referências visuais do inspetor

O novo pedido do usuário forneceu dez imagens ItsMagic e autorizou continuar as pendências, principalmente componentes. A validação no ADB será autorizada depois. V2 §§18–19 e o complemento de código foram relidos durante a implementação. Não houve build, teste ou uso do dispositivo.

Implementação escrita: `scene::Camera` e `scene::MeshRenderer` anexáveis; remoção dos campos `assetId/material` da entidade; migração de cenas 1–9 para v10; extração, picking, enquadramento e collider fit por capacidade. Câmera selecionada por habilitação/prioridade, com FOV e planos ligados ao renderer e fontes GLSL de vértices/céu/pós-processamento, além de sombras, frustum e LOD. Históricos são invalidados quando a projeção muda.

O inspetor normal unifica Transformação e componentes em cabeçalhos de 44 dp. Add recebe busca e categorias do catálogo, scripts continuam vindos do schema aplicado. Componentes novos ficam recolhidos, na ordem de anexação. Abrir um cabeçalho dá espaço aos campos e fechar retorna à lista. Malha recebe Geometria/Material, seletor da biblioteca real e restauração dos parâmetros de origem. Os números nativos vêm diretamente dos descritores; edição carrega instanceId/PropertyId e passa pelo setter tipado.

Contrato completo, ligação às imagens, migração, fontes consultadas e limites em [ADR — Câmera, malha e inspetor](adr/ADR-REFUNDACAO-CAMERA-MESH-INSPECTOR.md). Quadro Unity/ItsMagic atualizado sem tratar as 270 referências como componentes prontos. Ainda não há AssetGuid geral, MaterialAsset compartilhado, ortográfica, miniaturas renderizadas, compound colliders ou player independente.

Estado: **fontes editados; não compilado/não executado**. Shaders derivados/SPIR-V também aguardam regeneração autorizada. Ajustes mecânicos dos fixtures à nova API não comprovam comportamento. Capturas e APKs anteriores não validam esta versão.


## 10/09/2026 — adição ampla de composição física, juntas e referências

Pedido direto: “prepare uma mega adição pq depois vou autorizar adb”. Releitura da V2 §§18.1–18.4 e do complemento de componentes. Continua a restrição de testes somente autorizados. Não houve build da engine, testes, shader compilation, APK, captura ou ADB; produção de um PNG/atlas e geração do quadro documental não são provas de execução.

[Registro técnico deste bloco](adr/ADR-REFUNDACAO-COMPOSICAO-FISICA.md): Colisor e Corpo v3; várias formas num corpo; owner explícito e pose local; sensor/damping/gravidade/repouso/giro; Junta v1 com ponto, dobradiça, deslizante e distância; referências nativas tipadas e seletor também para C#; operações de instância no inspetor; remapeamento ao duplicar e revalidação de reparent; overlays do colisor e âncoras; forças, impulsos, torques, velocidade e callbacks de sensores pela ABI de script v2. Cadastro/arquivo/consumidor/UI foram conectados em fonte.

Novo ícone de Junta: geração raster image_gen na paleta Astra, empacotada pelo pipeline existente. O atlas e enum agora possuem 154 entradas incluindo marca, sem afirmar número de componentes. O catálogo nativo Add tem sete tipos e scripts do schema; o quadro externo continua com 270 referências curadas, além das declarações de pacotes ainda não curadas funcionalmente.

Persistência mantém arquivo v10 e evolui versões individuais. O bloco anterior que listava compound/owner/joints como inteiramente ausentes fica superado em fonte; ShapeAsset, filtros, subcollider events, CharacterMotor nos sensores, joints avançados, ruptura, mutação em Play, recursos/IDE/player completos continuam abertos. Exemplos e limites em [Física por código](componentes/fisica-codigo.md).

Estado: **implementado em fonte, sem compilação/execução**. Mocks antigos da ABI e expectativas de widgets por tipo precisam de atualização na rodada autorizada. Shaders/SPIR-V pendentes do bloco anterior continuam pendentes. APKs e resultados históricos não validam este conjunto.

## 10/09/2026 — validação autorizada dos blocos amplos

Após “eu autorizo”, foram compilados e exercitados os blocos de instâncias, IDE/C#, câmera/malha e composição física. [Relatório consolidado](validacao/2026-09-10-componentes-codigo.md): 767/767 testes nativos, 513/513 managed (zero pulados), 16/16 Java; shaders regenerados; Debug e Release produzidos. Esses resultados substituem o estado “sem compilação/execução” dos três registros anteriores para o escopo efetivamente coberto.

Falhas corrigidas: erros de compilação nativa; fonte/mapeamento de pacotes Roslyn; referências nativas indevidamente usadas pelo compilador C#; roteamento Java de cenas v10; dependência OpenSSL ausente no APK; transição GC no loop nativo e associação da nova thread ao reabrir NativeActivity. O pacote oficial linux-bionic usa Mono/SGen sob a ABI e o nome libcoreclr.so; a identificação antiga foi corrigida na documentação. OpenSSL 3.5.8 vem da fonte oficial com SHA256 conferido, nomes privados, script de reprodução e conferência de hashes no Gradle. Scene.Log também chega ao log Android por um callback de plataforma, sem acoplar o editor a Android.

ADB em projeto exclusivo, preservando os anteriores: criar objeto, Add, componentes recolhidos, dois colisores por instância, teclado/offset/wireframe, Undo/Redo, salvar/reabrir, arquivos/IDE, Aplicar com DLL/PDB/schema, referência de script, compound com forma filha, junta motorizada, callbacks Enter/Exit, velocidade, pausa, passo e Stop. Capturas da pausa foram idênticas; um passo mudou a simulação. O arquivo de cena antes/depois de Play/Stop ficou byte a byte igual. Reabertura e Aplicar no mesmo processo passaram após corrigir a associação Mono.

O relatório mantém os hashes dos APKs, as capturas relevantes, o vídeo e os limites. Não houve encerramento dos marcos M/S nem implementação integral do catálogo Unity/ItsMagic. ShapeAsset/GUID, formas complexas, eventos sólidos/personagem, juntas avançadas, mutação em Play, API geral, IDE completo e player/exportação continuam pendentes conforme os contratos.

## 10/09/2026 — mundo de execução, física consultável e ações de entrada

Entregas A, D e E de [Próximo pacote — criação de gameplay](PROXIMO-PACOTE-GAMEPLAY.md),
na branch `codex/gameplay-runtime`. Decisões e alternativas descartadas em
[ADR](adr/ADR-RUNTIME-GAMEPLAY.md); contrato efetivo em
[runtime de gameplay](runtime-gameplay.md); números e lacunas em
[validação](validacao/2026-09-10-runtime-gameplay.md).

O Play deixou de rodar sobre uma cópia de `EditorDocument`. `runtime::SceneGraph`
guarda os objetos sem conhecer editor, UI, histórico ou seleção; `EditorDocument`
passou a ser esse grafo com as invariantes autorais por cima. `runtime::GameWorld`
possui a sessão de execução, com identidade `{mundo, id, geração}`, fila de
comandos em ponto seguro e autoridade de pose. `ScenePhysics` e `ScriptBridge`
são os antigos adaptadores do editor, agora consumindo o mundo — e por isso o
mesmo par pode ser ligado a um consumidor sem editor. **Isso é a fundação de um
player futuro, não uma entrega de exportação.**

`scene/component_schema.h` virou a única lista de nome, categoria, exigências,
incompatibilidades e mutabilidade em Play; o catálogo do inspetor deriva dele, e
um teste recusa que voltem a ser duas listas.

Física: consultas com ponto, **normal de superfície real** (segunda consulta ao
corpo acertado) e instância do colisor que respondeu; contatos **sólidos** com
Enter/Stay/Exit aos dois objetos do par; camadas de gameplay nomeadas cuja matriz
recíproca vale **no solver** — um par proibido não gera contato, o que um teste
demonstra derrubando a mesma bola sobre o mesmo piso duas vezes.

Entrada: `InputActionMap` como recurso da cena, com ações, bindings, zona morta,
sensibilidade, inversão e contexto; os papéis (mover, olhar, saltar) apontam para
ações escolhidas pelo usuário, então o núcleo não exige nome nenhum. Perder o
foco para a interface zera as ações e solta os botões na hora.

Sete modelos de comportamento editáveis acompanham o editor e são oferecidos ao
criar um script — **nunca semeados em projeto nenhum**. Eles conversam por
interface (`IInteragivel`, `IColetavel`) resolvida por capacidade. O header
embutido é gerado dos mesmos arquivos que o teste gerenciado compila com o
compilador do projeto do usuário.

Arquivo de cena em **v12** (v11 camadas, v12 ações); arquivos anteriores abrem
com os padrões. ABI de scripts em **v5**, preservando as posições de v2.

Estado: **792/792 testes nativos, 520/520 gerenciados, 16/16 Java; APK Debug e
Release compilados.** Nenhuma sessão ADB nesta rodada: as duas composições de
aceitação não foram montadas pela interface, e nada aqui foi exercitado no
aparelho. Entregas B (recursos/importação), C (materiais/luzes), F (IDE/inspetor)
e G (rodada integrada) continuam abertas, assim como sensor por colisor,
`CharacterVirtual` na broadphase, ShapeAsset e exportação.

## 11/09/2026 — rodada de aparelho do pacote de gameplay

ADB autorizado. Xiaomi 25053PC47G, serial `53e1eb7`, Android 16. Projeto
exclusivo `RuntimeGameplay0910` criado pela interface, projetos anteriores
preservados. [Relatório completo](validacao/2026-09-10-runtime-gameplay.md).

Exercitado pela interface no aparelho: criar projeto e objetos, arquivo de cena
**v12** com as seções `LAYERS` e `INPUT` gravadas e relidas ("Cena restaurada"),
seletor dos sete modelos de comportamento, criação do script a partir de modelo
com a classe e o `ComponentId` renomeados, Aplicar compilando no aparelho e
publicando o schema, schema governando o inspetor ("Incompatível com corpo
físico", "Já adicionado"), Play com o mundo de execução, **contato sólido com
normal chegando ao comportamento C#**, escrita de propriedade de componente por
código, identidade de mundo distinta por Play, pausa determinística, passo
avançando um passo, Stop devolvendo a autoria byte a byte e reabertura no mesmo
processo (PID 27995) com novo Play completo.

Dois defeitos apareceram só no aparelho e foram corrigidos com teste:

1. O shell Java aceitava no máximo a versão 10 do arquivo de cena e o escritor
   nativo passou a emitir a 12 — nenhum projeto salvo pela build nova abria.
   `ArquivoDeCenaTests` agora compara as duas versões e falha no host.
2. O caminho `EditorPlayScene` → `ScriptBridge` → ABI não tinha teste;
   `test_editor_play_scripts.cpp` cobre a entrega de contatos e recusa uma ABI
   incompleta em vez de perder eventos em silêncio.

Uma divergência fica registrada e **não** corrigida: a escrita de `base_color`
por script é aceita pelo mundo, mas o override de material por instância não
chega ao desenho das primitivas do projeto independente no renderer Android.
Isso pertence à entrega C, que este pacote não implementou.

Estado: **794/794 nativos, 521/521 gerenciados, 16/16 Java**; Debug e Release
compilados. A prova de aceitação com duas composições montadas pela interface
continua pendente, assim como as entregas B, C, F e G.

## 12/09/2026 — M06 funcional após aprovação do IDE

A revisão visual aplicada foi aprovada pelo usuário. A Entrega F prosseguiu com
serviço semântico Roslyn, navegação à definição, busca/substituição inline,
busca nas fontes do projeto, preferências de recuo e agrupamento do histórico
por pausa. Console recebeu contexto de build/Play/projeto, snippet imutável e
altura arrastável. Play bloqueia código alterado ou em erro; alterações de schema
preservam instâncias e valores, com diagnósticos explícitos.

O fluxo focal com duas instâncias do mesmo Behavior, valores Alpha/Beta,
execução e reabertura foi observado no Android pela interface. Isso não equivale
à aceitação das duas composições completas da entrega G citadas acima.
Arquitetura/limites em [pacote funcional](planos/M06-PACOTE-FUNCIONAL.md) e
evidência em [validação](validacao/2026-09-12-m06-funcional.md). M06 integral,
matriz de lifecycle e demais entregas continuam com pendências rastreadas.

# Próximo pacote Astra — criação de gameplay

Especificação de execução preparada em 10/09/2026 para repasse a outro agente.
Este documento define trabalho futuro; não declara suas capacidades implementadas.

## 1. Objetivo e ponto de partida

Entregar o caminho completo para criar um projeto vazio, importar um modelo,
compor objetos com componentes, escrever comportamentos C#, configurar controles,
executar interações e salvar/reabrir tudo pela interface da Astra. A criação de
cada jogo não pode exigir mudança no código-fonte da engine ou recompilação do APK.

Repositório: https://github.com/kacerato/attachsEngine

Base publicada: `a34d0325d6c4302942c14442a9d900a199d4cdd0`, branch `main`.
Workspace original: `C:/Users/donod/Downloads/atchengine`.

Antes de editar, conferir HEAD, status, worktrees e eventuais AGENTS.md aplicáveis.
Se outro trabalho estiver ocorrendo no mesmo checkout, usar um worktree próprio,
com branch `codex/…`, a partir da base publicada ou de uma base posterior conferida.
Não integrar indiscriminadamente o worktree separado de renderização/água.

O usuário quer blocos amplos de implementação, documentação contínua e uma rodada
integrada de validação após a construção. Não fazer uma alteração mínima e parar
para uma bateria repetitiva. Preservar as autorizações de execução repassadas na
tarefa; houve autorização explícita de testes/ADB na rodada de 10/09. Este repasse,
por si só, não inicia testes ou operação do aparelho.

### Leituras e precedência

1. Instruções diretas mais recentes do usuário.
2. `C:/Users/donod/Downloads/PROMPT_REFUNDACAO_ASTRA_V2.md`: sobretudo §§4–11,
   §§17–22 e marcos M2–M10/S1–S7.
3. `C:/Users/donod/Downloads/ASTRA_COMPONENTES_CODIGO_EDITOR.md`.
4. `C:/Users/donod/Downloads/PROMPT_REFUNDACAO_ASTRA.md`, como histórico do plano.
5. [Histórico de implementação](REFUNDACAO-ASTRA.md),
   [componentes e dependências](componentes/README.md),
   [composição física](adr/ADR-REFUNDACAO-COMPOSICAO-FISICA.md),
   [câmera/malha/inspetor](adr/ADR-REFUNDACAO-CAMERA-MESH-INSPECTOR.md).
6. [Validação mais recente](validacao/2026-09-10-componentes-codigo.md).

Os documentos de requisitos são material de planejamento adotado pelo usuário;
comparações externas neles não comprovam funcionalidades da Astra. Se o agente
estiver em outra máquina, transferir os três arquivos de Downloads junto deste
repasse. Não afirmar ter relido anexos que não estão disponíveis.

As seções históricas contêm estados anteriores, inclusive “sem build/execução”.
O relatório de 10/09 prevalece para a rodada já executada. O gerador
`tools/build-component-reference-catalog.py` ainda contém frases antigas:
atualizar a fonte do catálogo e seus derivados, sem converter uma validação
parcial em aprovação de toda a família.

## 1.1 Estado por entrega (atualizado em 11/09/2026)

Branch `codex/gameplay-runtime`. As colunas são independentes e **não se
colapsam**: "implementado" é código que existe; "integrado" é código ligado ao
consumidor real; "host" é suíte automatizada aprovada na máquina; "Android" é
comportamento exercitado NO APARELHO. Um APK que compila não é validação no
Android.

| Entrega | Contrato | Implementado | Integrado | Testado no host | Validado no Android |
|---|---|---|---|---|---|
| A — mundo de execução e schema | [ADR](adr/ADR-RUNTIME-GAMEPLAY.md) | sim | sim (Play roda no `GameWorld`) | 11 testes | **parcial**: Play/pausa/passo/Stop, arquivo v12, identidade de sessão por Play e reabertura no mesmo processo |
| B — recursos e importação | [recursos](runtime-gameplay.md#14-recursos-e-importação) | **parcial**: registro com GUID, identidade de malha com migração, leitor de GLB com **árvore de nós preservada** e importação pelo seletor do Android; renomear/mover/apagar no painel com confronto de dependentes; **sem** cenas reutilizáveis e texturas | sim (o pacote gráfico absorve a geometria importada e o projeto reabre relendo as fontes) | 21 testes | **sim, para a parte implementada**: assembly de cinco nós importado num projeto vazio, hierarquia e pivôs preservados, a porta editável sozinha, e o projeto reaberto relendo a fonte |
| C — materiais e luzes | [luzes](runtime-gameplay.md#13-luzes) | **parcial**: Luz anexável (direcional/pontual/spot) e override de material por instância; MaterialAsset compartilhado, slots por submesh e pré-visualização isolada **não** iniciados | sim (shader PBR consome as pontuais/spot; a direcional vira o sol) | 6 testes de luz + 1 de material em execução | **sim, para a parte implementada**: Luz criada pelo inspetor, três modalidades vistas na tela, salvar/reabrir preserva a aparência |
| D — física acessível | [ADR](adr/ADR-RUNTIME-GAMEPLAY.md) | sim, exceto sensor por colisor e `CharacterVirtual` na broadphase | sim (queries, camadas no solver, contatos no C#) | 9 testes | **parcial**: contato sólido com normal chegou ao comportamento; consultas e camadas só no host |
| E — entrada e comportamentos | [ADR](adr/ADR-RUNTIME-GAMEPLAY.md) | sim | sim (toque → ações → personagem/câmera; modelos no editor) | 7 testes | **parcial**: modelo criado, compilado e anexado no aparelho; ações de entrada só no host |
| F — IDE, console e inspetor | [catálogo de código](runtime-gameplay.md#11-catálogo-de-código-instância-schema-e-execução) · [grade](runtime-gameplay.md#15-grade-editorial) · [texto](runtime-gameplay.md#16-escrita-de-texto-no-android) | o seletor de modelo de script, a **separação instância/schema/execução** com publicação atômica, a **grade como desenho no mundo** e a **escrita sem diálogo** (campo embutido e código digitado no próprio editor) | idem | 11 testes | **parcial**: seletor de modelos, grade com profundidade, campo embutido e digitação no editor verificados no aparelho; **console continua fora**, e o afinamento de toque/composição do editor de código está endereçado ao M06.2 (reconstrução do IDE) |
| G — rodada integrada | — | **parcial** | **parcial** | — | **parcial**: duas composições montadas a partir de projetos VAZIOS pela interface (`PatioG1`: chão, plataforma, personagem com câmera e olhar, script de modelo compilado e anexado DUAS vezes com valores diferentes, Play recusado com motivo, corrigido, Play/Pausa/Passo/Stop e Stop preservando autoria; `InteriorG2`: chão, porta, luz anexável, material por instância). **Não** provou: importação nestas composições, o segundo script (defeito registrado), referências com duplicar/reparentear/desfazer/reabrir, consultas físicas em execução, e foco de input |

A rodada de 11/09 também corrigiu um defeito que bloqueava qualquer trabalho de
aparência: durante o Play o renderer publicava apenas poses, então material,
visibilidade e sombra por instância ficavam congelados no valor de autoria e uma
cor escrita por script nunca chegava à tela. Está corrigido e guardado por teste.

A rodada de aparelho de 11/09 exercitou UMA cena montada pela interface para
percorrer os caminhos; a prova de aceitação do pacote — **duas** composições
diferentes a partir de projetos vazios — continua pendente. Ela encontrou dois
defeitos que o host não pegava: a versão do arquivo de cena divergindo entre o
escritor nativo e o shell Java, e a ausência de teste no caminho Play→script.
Ambos corrigidos e agora cobertos por teste.

Detalhes, números e problemas corrigidos:
[relatório de validação](validacao/2026-09-10-runtime-gameplay.md).
Contrato efetivo da API: [runtime de gameplay](runtime-gameplay.md).

## 2. Base existente e lacunas confirmadas

| Área | Existe na base | Trabalho ainda necessário |
|---|---|---|
| Componentes | TypeId, versões, IDs de instância, descritores, preservação de tipo ausente; limite atual de 64 instâncias por objeto | Schema comum completo, ComponentReference, migração de campos e acesso geral no runtime |
| Catálogo anexável | Sete entradas nativas: Corpo físico, Personagem, Olhar, Colisor 3D, Junta, Câmera, Malha; tipos C# vindos do schema aplicado | Expandir somente com consumidores reais; as 270 referências não são 270 implementações |
| Persistência | Arquivo AETHER_EDITOR v10; múltiplos colliders/scripts/juntas, duplicação e remapeamento | Assets com GUID, identidade de sub-recursos e migração dos IDs locais de pacote |
| Execução | Play isolado, Pause, Step, Stop e física compartilhada | `EditorPlayScene` ainda contém uma cópia de `EditorDocument`; separar o mundo de execução |
| Código | Criar/editar/salvar C#, Roslyn, Aplicar, instâncias de Behavior, propriedades, forças, velocidade e eventos de sensor | API de objetos/componentes, consultas físicas, input por ações, referências e diagnóstico navegável |
| Física | Primitivas, compound com owner explícito, quatro modalidades de junta, CharacterMotor e ponte Jolt | Camadas de gameplay, contatos sólidos, sensores com personagem, consultas expostas ao projeto |
| Consultas nativas | `AetherPhysics_RayCastClosest`, `RayCastAll`, `ShapeCastClosest`, `OverlapShape` | Ligação ao mundo de gameplay, resultados com identidade de objeto/componente e filtros completos |
| Assets | Navegação de arquivos, criação de pasta/texto, geometria qualificada por pacote; ferramentas host de glTF | Importação integrada no Android, registro de recursos, renomear/mover/reimportar sem perder referência |
| Aparência | Câmera anexável com FOV/planos/prioridade; Malha com parâmetros de material por instância | MaterialAsset compartilhado, submeshes, luzes anexáveis e pré-visualização isolada |
| Interface | Add com ícone/nome, categorias/busca, cabeçalhos recolhidos, seletores de objeto | Campos C# mais ricos, referências de recursos, console e operações de arquivos |

Prova anterior: 767 testes nativos, 513 C# e 16 Java aprovados; Debug/Release
produzidos; autoria por toque, Aplicar/Play, sensores, pausa/passo/Stop e reabertura
exercitados no aparelho. Essas contagens pertencem à base, não às alterações futuras.

### Cuidados técnicos que precisam ser preservados

- O runtime Android .NET linux-bionic 8.0.27 usa Mono/SGen sob ABI de hospedagem
  CoreCLR. Preservar regiões GC-safe e associação da nova thread ao reabrir uma
  NativeActivity no mesmo processo; não tratar isso como CoreCLR desktop puro.
- OpenSSL privado, hashes, licença e carregamento anterior ao runtime são
  necessários ao Aplicar. Não substituir pelo BoringSSL do Android.
- `ObjectReference` C# contém hoje somente ObjectId. Não alegar que já tem geração,
  mundo ou proteção completa contra reutilização de identidade em outro Play.
- O filtro nativo de queries distingue hoje Static e Dynamic/kinematic. Isso não
  equivale a um sistema de camadas de gameplay nomeadas.
- O sensor atual pertence ao corpo inteiro. Não anunciar sensor por collider
  apenas adicionando um checkbox ao inspetor.
- `CharacterVirtual` não está automaticamente na broadphase como um rigid body.
  Sensores/raycast contra personagem exigem integração explícita e sem duplicação.
- ShapeCast legado possui seu próprio descritor de formas. Confirmar as formas
  aceitas na implementação; não inferir que suporta todas as formas de um corpo.
- Transform por script hoje recusa algumas alterações sobre corpo/personagem ou
  composição física. Retirar uma recusa exige implementar sincronização correta.
- Ferramenta Python de importação no host não comprova importação dentro do Android.

## 3. Ordem do pacote

Trabalhar em sete entregas conectadas. Os nomes de classes novos abaixo são
propostas de contrato; ajustar à base antes de codificar. Nenhuma assinatura
citada como objetivo deve ser apresentada como API existente.

### Entrega A — mundo de execução e schema comum

Arquivos de entrada:
`native/editor/editor_play_scene.h`, `editor_scene_physics.*`,
`editor_script_bridge.*`, `editor_document.*`, `editor_map_scene.*`,
`native/scene/components.h`, `component_properties.h`, `script_runtime.h`,
`managed/Astra.Scripting/Behavior.cs` e `Runtime/BehaviorWorld.cs`.

1. Introduzir um mundo de execução fora de `native/editor`, consumindo dados de
   cena e recursos. Ele deve possuir entidades, componentes de execução, relógio,
   mundo físico, scripts e filas de comandos. Evitar apenas mover um arquivo que
   continue importando `EditorDocument`, widgets, histórico ou estado de seleção.
2. Definir identidade persistente versus handle de execução. Handles devem
   identificar mundo/geração e recusar referências de objetos destruídos ou de
   outra sessão Play. Componentes repetíveis precisam ser endereçados por instância.
3. O editor extrai uma descrição de cena e inicia o runtime. Play não escreve no
   documento autoral. Stop encerra callbacks, scripts, física e recursos de execução
   em ordem definida; o documento original permanece intacto.
4. Centralizar metadados de propriedades, cardinalidade, dependências,
   incompatibilidades, mutabilidade em Play e tipos de referências. O inspetor e
   a API C# devem consumir esse contrato. Bindings gerados são preferíveis a duas
   listas independentes mantidas manualmente; toda geração deve ter consumidor real.
5. Evoluir a API para criar/destruir objetos e componentes, consultar pai/filhos,
   localizar componentes por tipo/instância/capacidade, habilitar/desabilitar,
   ler/escrever propriedades e trabalhar com transform local/global.
6. Criação, remoção e reparent durante callbacks entram em fila aplicada num ponto
   seguro. Definir como o resultado é obtido, quando a referência passa a existir,
   ordem de conflitos e comportamento de comandos destinados a um objeto removido.
7. Documentar autoridade de transform: dinâmica publica pose da física; cinemática
   usa movimento físico; personagem usa CharacterMotor; objetos livres usam TRS.
   Reparent/escala que exigem shear continuam tratados explicitamente.
8. Preservar Start/Update/FixedUpdate/Stop já utilizados. Se adicionar fases de
   criação, enable/disable, late update ou destruição, especificar compatibilidade,
   hidratação antes de callback, erro por instância e ordem estável. A ordem visual
   dos componentes não pode virar regra oculta de simulação.
9. Versionar a ABI e conferir tamanho/capacidade. Nenhuma exceção C++/C# atravessa
   livremente a fronteira. Threads de background não mutam o mundo diretamente.
10. Evoluir referências e aliases de propriedades preservando dados antigos e
    MissingComponent. Não substituir silenciosamente dados incompatíveis por defaults.

Conclusão: o mesmo runtime pode ser ligado a um consumidor sem editor; um script
cria um objeto, modifica componentes e o remove durante execução, sem corromper
iteradores nem alterar a cena autoral. Isso é a fundação do futuro player, ainda
não uma entrega de exportação de APK de jogo.

### Entrega B — recursos persistentes e importação real

Arquivos de entrada:
`native/editor/editor_filesystem.*`, `editor_archive.*`, `editor_map_scene.*`,
`native/scene/mesh_renderer.h`, `material_parameters.h`,
`tools/export-authoring-assets.py`, `tools/cook-gltf-map.py`,
`native/platform/atomic_asset_file.*` e integração Android em
`android/app/src/editor/java/dev/aether/editor/AetherActivity.java`.

1. Criar registro de assets com GUID estável, tipo, fonte, dependências,
   parâmetros/versão do importador, hash e derivados. Separar identidade de caminho
   e de hash de conteúdo: renomear ou editar não cria automaticamente outra identidade.
2. Definir recursos Mesh, Material, Texture, Shape, Script, InputActionMap e cena
   reutilizável. Registrar apenas tipos que recebam implementação no pacote;
   dependências e versões precisam ser explícitas.
3. Migrar referências de geometria por pacote para referências resolvíveis de
   recursos sem perder cenas v10. Definir IDs estáveis de submeshes/materiais e
   reconciliação na reimportação; índice de array isolado não é identidade suficiente.
4. Importar GLB e glTF estático pelo seletor Android, com hierarquia, instâncias,
   meshes, materiais/texturas, UV, normais/tangentes e conversões documentadas.
   Identificar parser/licença/versão e seus limites antes de integrá-lo.
5. Tratar URI via ContentResolver/descritor de arquivo e copiar fontes/dependências
   para armazenamento gerenciado. Para glTF com arquivos externos, resolver acesso
   à pasta/dependências explicitamente. Não converter `content://` em caminho POSIX.
6. Importação deve permitir progresso e cancelamento, limitar memória/dimensões/
   quantidade de elementos e recusar arquivo inválido sem substituir recurso válido.
   Extensão não suportada produz diagnóstico concreto. Skins/animações não podem
   ser anunciadas como prontas nesta importação estática.
7. Publicar registro, dependências e derivados de maneira consistente. Após importar,
   o projeto precisa abrir offline, sem depender de caminho temporário do seletor.
8. Implementar renomear, mover, criar pasta, busca/filtros, referências dependentes
   e exclusão com confirmação adequada. Explicar referência ausente; Undo de cena
   não promete restaurar arquivo excluído sem mecanismo próprio.
9. Reimportar mantendo a última versão válida até publicação da nova. Conflitos
   entre mudança externa, buffers abertos e operações do usuário precisam aparecer.
10. Criar cenas reutilizáveis do próprio usuário com remapeamento de IDs e
    referências internas, overrides definidos e instâncias independentes. É uma
    capacidade de autoria, não uma demo interna semeada no projeto.

Conclusão: importar um GLB externo no app, instanciá-lo, atribuir materiais,
renomear/mover fontes e reabrir preservando referências. Primitivas internas e
recursos importados percorrem os mesmos componentes e consumidores.

### Entrega C — materiais, luzes e apresentação da cena

Arquivos de entrada:
`native/scene/mesh_renderer.h`, `camera.h`, `material_parameters.h`,
`native/renderer/material_override.h`, `environment_lighting.h`,
`native/platform/android/instanced_renderer.*`, `dirt_road_resources.*`,
`native/rhi/shaders/` e `native/editor/editor_component_catalog.h`.

1. Introduzir MaterialAsset compartilhado e slots por submesh. Diferenciar editar
   o recurso compartilhado de criar override ou tornar o material independente.
   Essas escolhas precisam ser claras na interface e persistentes no arquivo.
2. Ligar parâmetros PBR e texturas/samplers implementados ao renderer; documentar
   linear/sRGB, transparência básica e tratamento de dados ausentes/inválidos.
3. Implementar Luz anexável, com modalidades direcional, pontual e spot como alvo
   do pacote. Auditar o renderer: a iluminação solar existente não comprova as
   outras modalidades. Cada opção só fica disponível com consumidor gráfico real.
4. Para cada modalidade, ligar pose, cor, intensidade, alcance/ângulos aplicáveis,
   habilitação, serialização, extração e alteração em Play. Definir orçamento de
   luzes e política de excedentes; nenhuma luz pode desaparecer silenciosamente.
5. Sombras por modalidade precisam de suporte próprio. Publicar a matriz real
   de capacidades; não oferecer uma opção de sombra cujo shader/pass não existe.
6. Pré-visualização de material deve renderizar o recurso real em contexto isolado.
   Luz de inspeção e modo clay não modificam a aparência autorada do jogo.
7. Expor câmera/material/luz pela API comum. FollowTarget deve poder mover também
   uma luz ou marcador; a lógica de seguir alvo não pertence exclusivamente à câmera.

Conclusão: editar uma luz e um material pela interface e por C#, com alteração
visível em objetos arbitrários; salvar/reabrir preserva a aparência. Botões,
descritores e shader compilado isoladamente não comprovam esse fluxo.

### Entrega D — física acessível ao projeto

Arquivos de entrada:
`native/physics/jolt_bridge.*`, `character_motor.*`,
`native/editor/editor_scene_physics.*`, `editor_collider_fit.h`,
`native/scene/collider.h`, `physics_body.h`, `joint.h`,
`script_runtime.h`, `managed/Astra.Scripting/Behavior.cs` e bindings nativos.

1. Aproveitar as queries existentes. Expor ao C# raycast mais próximo/múltiplo,
   shape cast e overlap, com mundo explícito, alcance, máscara, objeto ignorado,
   inclusão de sensores e política de truncamento/ordenação dos resultados.
2. Mapear resultados para objeto e instância de collider quando disponíveis.
   Definir ponto, normal, distância/fração e espaço de coordenadas. Se um dado
   precisa de consulta nativa adicional, implementá-la; não devolver zero como
   se fosse uma normal de contato válida. Tratar raio zero e buffers insuficientes.
3. Criar camadas de gameplay nomeadas e matriz de interação no projeto. Separar
   esse conceito das camadas amplas NonMoving/Moving do Jolt; documentar limite,
   defaults de migração, reciprocidade dos filtros e comportamento das queries.
4. Completar eventos de contato sólido e integração de sensores com CharacterMotor.
   Contatos entram em fila durante callbacks da física e são entregues no ponto
   seguro, por passo físico. Definir Enter/Stay/Exit, destinatários, agregação de
   subformas, normal relativa e tratamento de remoção/desativação dos participantes.
5. Resolver explicitamente CharacterVirtual versus broadphase. Conferir o backend
   antes de escolher listener/proxy/inner body; evitar dois contatos ou dois corpos
   simulando o personagem. A cápsula visual não é evidência de integração de sensores.
6. Preservar owner explícito de collider e composição em filhos. Permitir mudança
   de propriedades declaradas mutáveis no ponto seguro e reconstrução transacional
   quando necessária, mantendo pose, velocidade e referências válidas.
7. ShapeAsset inicial cobre primitivas reais, pose de instância e reutilização.
   Convexos, mesh côncava e decomposição exigem cooking e política para corpos
   dinâmicos; permanecem identificados no backlog se não incluídos de fato.
8. Ajuste automático usa limites/geometria como sugestão editável. Não escolher
   “a forma correta para gameplay” pelo nome do objeto. Trocar visual não troca a
   física silenciosamente. Caixa, esfera e cápsula são opções gerais legítimas.
9. Aumentar a API de forças com força/impulso em ponto e atualização dos motores
   existentes em Play, com unidades/limites claros e erro para alvo incompatível.
10. Sensor por collider, filtros por subforma e novas juntas avançadas exigem
    contrato/backend próprios. Não simular sua entrega com propriedades sem efeito.

Conclusão: personagem aciona sensor, contato sólido chega ao comportamento,
raycast seleciona por capacidade e máscara, câmera evita obstáculo por shape cast,
e Stop limpa pares/eventos. Reutilizar os mesmos serviços em objetos distintos.

### Entrega E — ações de entrada e comportamentos do projeto

Arquivos de entrada:
`native/platform/android/android_runtime_controls.*`, `android_main.cpp`,
controle de toque existente, `native/scene/character.h`, `camera_look.h`,
`managed/Astra.Scripting/Behavior.cs` e mundo de execução da entrega A.

1. InputActionMap como recurso: ações Button, Axis1D e Axis2D, bindings, deadzone,
   escala/sensibilidade, inversão, estado pressionado/solto e contexto habilitado.
   Ações Move/Look/Jump/Interact são dados editáveis, não nomes obrigatórios do núcleo.
2. Traduzir toque Android para ações. Teclado/gamepad usam o mesmo serviço quando
   conectados e suportados; documentar dispositivos efetivamente exercitados.
3. Definir consumo e foco: abrir IDE, teclado, modal ou arrastar gizmo não pode
   movimentar o personagem ou disparar interação. Pausa/retomada limpa estados
   transitórios para não deixar botão preso. Tratar multitouch e cancelamento.
4. Personagem e câmera consomem ações configuráveis. Expor movimento/salto do
   CharacterMotor por API; o projeto não deve chamar classes da UI Android.
5. Criar comportamentos C# de projeto: FollowTarget, câmera de órbita com obstáculos,
   interação por contrato/capacidade, porta configurável, plataforma cinemática,
   item coletável e mudança de material por evento. Arquivos editáveis e reutilizáveis.
6. Configurar alvos, eixos, ângulos, velocidades, ações e materiais no inspetor.
   Demonstrar dois scripts no mesmo objeto e o mesmo tipo em vários objetos.
   Classes auxiliares/interfaces não precisam virar componentes anexáveis.
7. Evitar buscas globais por nome a cada frame. Resolver referências e capacidades
   com validade controlada; objeto removido produz resultado definido.
8. Plataforma móvel deve compartilhar mundo com o personagem e ter política de
   transporte/velocidade do chão. Câmera deve seguir pose apropriada sem disputar
   a autoridade do corpo. Registrar interpolação e momento da atualização.

Conclusão: trocar modelo, nome e alvo preserva o comportamento. Porta, plataforma
e câmera do usuário não exigem novos tipos especiais no código da engine.

### Entrega F — IDE, console e inspetor

Arquivos de entrada:
`native/editor/editor_code_workspace.*`, `editor_screen.*`, `editor_session.*`,
`editor_reference_picker.h`, `editor_component_catalog.h`,
`managed/Astra.Scripting/Compilation/ProjectCompiler.cs`, `NativeCompiler.cs`,
`Runtime/BehaviorWorld.cs`, `native/platform/android/android_editor_text_input.*`
e `android/app/src/main/java/dev/aether/editor/EditorTextInput.java`.

1. Console com histórico limitado, filtros, limpar/copiar, severidade, origem,
   objeto/componente e arquivo/linha/coluna quando conhecidos. Tocar em diagnóstico
   abre o buffer e posiciona a seleção; mensagem de compilação não é só toast.
2. Diferenciar edição não salva, código salvo, compilando, falha e geração aplicada.
   Relatório antigo não substitui estado de revisão nova. Cancelamento/falha
   mantém a última geração válida e todos os valores de componentes preservados.
3. Preservar Apply atômico e compatibilidade de schema. Tratar conflito de arquivos
   durante Salvar todos: não anunciar transação completa se apenas parte foi salva.
4. Melhorar seleção/cursor/scroll, indentação, busca/substituição e navegação entre
   arquivos. Avaliar serviços Roslyn com cancelamento e revisão; não confundir
   compilador com widget de edição ou prometer autocomplete completo sem integração.
5. Gerar campos de enum/flags por nomes, grupos de vetores/cores, referências de
   objetos/componentes/assets e indicadores de referência ausente. Compatibilidade
   do seletor deriva do schema. Referenciar collider repetível exige instanceId.
6. Tratar alias de campo/tipo e conversão versionada; manter dados não resolvidos
   até correção. Alterar nome visível não muda PropertyId nem perde valor salvo.
7. Manter Add com ícone e nome curto, componentes inicialmente recolhidos,
   cabeçalhos compactos, categorias/busca e menu contextual por instância.
   Não restaurar barras grandes permanentes de Copiar/Colar/Restaurar.
8. Identidade visual própria da Astra, usando as referências enviadas para
   hierarquia/densidade/organização. Novos ícones raster precisam representar a
   função e ter origem documentada. Não acrescentar ícone de recurso inexistente.
9. Ocultar `.astra` e derivados na navegação normal; permitir diagnóstico por
   acesso apropriado. Diferenciar console do projeto e mensagens da engine.
10. Planejar layouts em paisagem/retrato, teclado aberto, muitos componentes,
    referências ausentes e seleção longa. Evitar sobrepor painel e alvos de toque.

Conclusão: escrever erro C#, tocar no diagnóstico, corrigir, aplicar, configurar
campos tipados e executar tudo dentro da Astra. Logs visíveis não são debugger.

### Entrega G — integração, prova e documentação

Após consolidar A–F, executar a rodada integrada autorizada. Durante implementação,
compilar ou testar de forma direcionada quando necessário para resolver um erro
real; evitar repetir suítes sem mudança relevante ou dúvida aberta.

Critérios de aceitação do pacote:

| Fluxo | Prova necessária |
|---|---|
| Referências | Duplicar/reparentear/remover/desfazer/salvar/reabrir preserva IDs e remapeia relações internas corretamente |
| Runtime | Criar/destruir objetos/componentes em callbacks não corrompe iteração; handles vencidos são recusados; Stop preserva autoria |
| Assets | GLB importado pela UI, dependências presentes, rename/move/reimport e reabertura offline funcionam |
| Materiais/luzes | Recurso compartilhado e override têm efeitos distintos; luzes implementadas chegam ao renderer e persistem |
| Física | Layers, queries, contatos, sensor-personagem e plataforma têm efeitos reais; ausência de alvo é tratada |
| Código | Dois scripts por objeto, mesmo script com valores diferentes, erro recuperável e geração anterior preservada |
| Input/UI | Foco do editor não vaza para gameplay; teclado/multitouch/cancelamento/retomada não deixam ações presas |
| Android | Play/Stop repetidos, pausa/passo, sair/reabrir no mesmo processo, recriação de superfície e compilação após retomada |

Construir duas composições diferentes partindo de projetos vazios pelas mesmas
ferramentas: por exemplo, um interior com porta/interação e um pátio com plataforma,
personagem e câmera orbital. Assets de entrada e scripts editáveis podem ser
fornecidos; a montagem autoral de aceitação precisa ocorrer pela interface.
Uma fixture gerada por ferramenta host continua útil para regressão técnica,
mas não substitui essa prova de liberdade de criação.

Comandos usados na base, a conferir no ambiente do agente:

```powershell
cmake --build build/editor-host --target aether_tests aether_ui_preview aether_physics_shared aether_transform aether_resources_shared --parallel 6
./build/editor-host/aether_tests.exe
dotnet run --project tests/Aether.Tests/Aether.Tests.csproj -c Release -p:AetherNativeBuildDir=C:/Users/donod/Downloads/atchengine/build/editor-host
./tools/generate-embedded-shaders.ps1 -All
```

No diretório `android`:

```powershell
./gradlew.bat :app:assembleDebug :app:assembleRelease :app:testDebugUnitTest --console=plain
```

O build host existente usa headers Vulkan neutros em `build/editor-vulkan-headers`;
conferir a configuração CMake antes de reproduzir em ambiente novo. Em worktree,
ajustar diretórios de build e `AetherNativeBuildDir`; não apontar por acidente para
DLLs antigas de outro checkout. Não é necessário rebuild Android após cada campo.

Para ADB: descobrir transporte/dispositivo atual, não copiar cegamente `-t 1` da
rodada anterior. Usar projeto de validação separado e instalação preservando dados.
Relacionar APK/hash, versão de assembly/schema, cenas, logs e capturas da mesma
rodada. Não atribuir falhas de PIDs antigos à versão final nem usar transições
de tela como evidência de renderização. Testes host não comprovam orçamento de FPS,
estabilidade prolongada ou todos os dispositivos.

Atualizar durante o pacote:

- Este documento com estado por entrega: contrato, implementado, integrado,
  testado no host, validado no Android — sem colapsar essas categorias.
- `docs/REFUNDACAO-ASTRA.md` e ADRs dos contratos alterados.
- `docs/componentes/README.md`, fonte do gerador e catálogo derivado, com suporte
  específico e pendência por família. Não inflar contagem com aliases/classes-base.
- `docs/componentes/fisica-codigo.md` e documentação da API efetiva.
- Um novo relatório de validação com comandos, resultados, APK, evidências,
  problemas corrigidos e lacunas remanescentes.
- Licenças/revisões/hashes de dependências e origem dos novos ícones.

## 4. Fronteiras do pacote e continuidade

Debugger completo (breakpoints/step/stack/watch) permanece S5, anterior ao
fechamento pleno de scripting/gameplay. Este pacote melhora diagnóstico e API;
não declara M8/M9/S5/S6 integralmente encerrados sem cumprir seus gates.

Exportação de jogo independente é M10/S7; hot reload avançado, tool scripts,
drawers/gizmos de projeto e ponte NoCode são S8, posteriores às bases requeridas.
Skinning/animação completa, áudio, UI de jogo, partículas, navegação, terreno,
2D, veículos, XR/rede e os demais itens do catálogo continuam visíveis no roadmap.
Não reduzir essas famílias a stubs para poder dizer que “tudo foi implementado”.

Para trabalho com mais de um agente, só dividir após fixar os contratos A/B:
runtime/schema/ABI precisam de dono de integração; assets/importação/renderer,
física/input e IDE/layout podem ter frentes delimitadas. Evitar edição concorrente
de `editor_session`, `editor_screen`, `android_main`, CMake e build Gradle.
Cada frente entrega sua ligação ao consumidor; uma interface isolada não fecha
o trabalho. Esta especificação não cria agentes nem tarefas automaticamente.

## 5. Texto curto para iniciar a tarefa no outro agente

Implemente o pacote descrito em docs/PROXIMO-PACOTE-GAMEPLAY.md, partindo do commit
a34d0325d6c4302942c14442a9d900a199d4cdd0 ou de uma base posterior conferida. Releia os
planos V2, complemento de componentes/código, histórico em docs e validação de
10/09/2026. A prioridade é execução independente do editor, schema/API comum,
assets com GUID e importação Android, materiais/luzes reais, física consultável,
input por ações, comportamentos do projeto e IDE/inspetor completos nos fluxos
descritos. Reutilize capacidades nativas confirmadas e complete seus consumidores;
não presuma suporte Astra por existir na Unity/ItsMagic. Implemente blocos amplos,
documente continuamente e consolide a validação ao fim conforme as autorizações
da tarefa. Preserve dados/projetos, migrações, identidade visual, Add e componentes
recolhidos. Não semeie demos no produto. Informe entrega, evidência e pendências
separadamente; não declare a refundação ou os 270 itens do catálogo concluídos.

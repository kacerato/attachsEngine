# Componentes de referência e execução do plano Astra

Atualização: 10/09/2026. Integra `PROMPT_REFUNDACAO_ASTRA_V2.md`, `ASTRA_COMPONENTES_CODIGO_EDITOR.md` e o histórico de `docs/REFUNDACAO-ASTRA.md`. Mantida a cadência pedida: implementar blocos amplos; testar somente após autorização explícita. O usuário autorizou a rodada atual; [resultados host/Android e falhas corrigidas](../validacao/2026-09-10-componentes-codigo.md).

Estado mais recente: [câmera/malha/inspetor](../adr/ADR-REFUNDACAO-CAMERA-MESH-INSPECTOR.md) e [composição física/juntas/referências](../adr/ADR-REFUNDACAO-COMPOSICAO-FISICA.md), compilados e exercitados na rodada autorizada. O catálogo externo continua sendo roteiro de implementação; não representa 270 componentes entregues.

**Atualização de 10/09/2026 — mundo de execução.** O Play deixou de rodar sobre uma cópia de `EditorDocument`: existe um [mundo de execução próprio](../runtime-gameplay.md), com identidade de handle por mundo+geração, fila de comandos em ponto seguro e autoridade de pose. O **schema de componentes** (`native/scene/component_schema.h`) passou a ser a única lista de nome, categoria, exigências, incompatibilidades e mutabilidade em Play; o catálogo do inspetor deriva dele, e a API em C# herda o mesmo contrato — um teste nativo recusa que voltem a ser duas listas. O arquivo de cena foi para **v12** (v11 acrescentou camadas de gameplay, v12 as ações de entrada); arquivos anteriores abrem com os padrões, que são o comportamento anterior a esses recursos. Decisões e alternativas descartadas em [ADR](../adr/ADR-RUNTIME-GAMEPLAY.md); resultados e lacunas em [validação](../validacao/2026-09-10-runtime-gameplay.md). As famílias de recursos/importação (B), materiais e luzes (C) e IDE/inspetor (F) **não** foram tocadas nessa rodada.

## Quadro por tipo

- [Unity 6: 117 entradas com função, caso, dependências e trabalho](unity.md).
- [ItsMagic: 153 componentes únicos com os mesmos campos](itsmagic.md).
- [Catálogo estruturado: 270 entradas](catalogo.json). Inclui fontes, revisão, estado Astra e assinaturas/atributos extraídos da documentação ItsMagic.
- [Pacotes Unity: inventário complementar](pacotes-unity.md): 253 declarações de tipos em sete pacotes; a análise funcional individual ainda está pendente. Esses registros não entram na contagem de 270 perfis completos.
- [Fontes e integridade dos arquivos de pacotes](pacotes-unity-inventario.json).
- [Perfis editáveis usados para gerar o quadro](perfis.tsv). `tools/build-component-reference-catalog.py` gera documentação; não cadastra componentes nem executa a engine.

Cada entrada do quadro contém função da referência, caso concreto de uso, alvo proposto na Astra, dependências específicas, etapa e trabalho de implementação. “Parcial” aponta código Astra concreto e também o que falta. “Sem implementação universal vinculada” significa que o caminho de autoria até runtime não foi estabelecido; pode existir matemática ou backend reaproveitável, mas isso não autoriza declarar o componente pronto.

## Cobertura e limites da pesquisa

Unity: fonte oficial `UnityCsReference`, branch 6000.0, revisão `a2a4a31aee6dfb63c2ef36eea79d817a6e31349b`, cruzada com o índice público da API 6000.0. O cruzamento contém 117 tipos, incluindo classes-base e auxiliares. Foram excluídos cinco candidatos sem correspondência no índice: `AudioBehaviour`, `InputSystemEventSystem`, `InputWrapper`, `MiniJSONTest` e `Painter2DExample`. A extração por declaração/herança é um inventário de fonte; ainda é preciso auditar regras condicionais de compilação e componentes internos antes de alegar cobertura absoluta.

ItsMagic: documentação oficial 2.0, repositório `ITsMagic-Software/Documentation`, revisão `d977bf8765c72567e221acb845934d30b070ac43`. Foram encontrados 203 registros `Component: yes`, consolidados em 153 nomes únicos de `JAVARuntime`. Há componentes repetidos em UI, pós-processamento e TODO; os caminhos alternativos ficam em `aliases`.

Várias páginas ItsMagic descrevem os métodos de forma automática e pouco informativa. `EnemyAI` não comprova percepção/estados só por ter `walkSpeed`; `BakeChildSuppressor`, `ConstraintPivot` e `ParticleGroup` quase só expõem construtores/instância. Esses casos permanecem no quadro com lacuna explícita, em vez de receber comportamento inventado. As propostas Astra são interpretações arquiteturais, não afirmações de igualdade interna entre engines.

Pacotes obtidos do registro oficial Unity: AI Navigation 2.0.5 (4 tipos), Animation Rigging 1.3.0 (19), Cinemachine 3.1.3 (77), Input System 1.11.2 (8), Netcode GameObjects 2.2.0 (18), Splines 2.7.2 (5) e XR Interaction Toolkit 3.0.9 (122). Há tipos-base, legados e auxiliares nessas contagens. Declarações encontradas em comentários, exemplos, Tests, Editor e Documentation foram excluídas. As versões mínimas declaradas e hashes constam no JSON; isso não equivale a compatibilidade executada com Unity 6 ou Astra.

uGUI/TextMeshPro, URP e HDRP continuam pendentes de inventário: as versões 2.0.0/17.0.3 consultadas não estavam disponíveis no endpoint público utilizado. Não substituir automaticamente por versões antigas nem declarar esses módulos cobertos. Componentes de outros pacotes, Asset Store e scripts de usuário tornam o ecossistema aberto; a meta não será representada por um número artificial como “1.000 componentes concluídos”.

O repositório [reposplugins](https://github.com/kacerato/reposplugins) continua como índice de referências. Código das engines de referência não foi copiado para o runtime Astra. Os ZIPs/TGZs consultados estão em `build/component-reference-research`; os documentos finais preservam origem e revisões independentemente desse cache.

## Ordem de implementação e dependências

| Etapa | Resultado concreto | Dependências / plano |
|---|---|---|
| A | Registro, identidades de instância, schema, valores, IDE, compilação e lifecycle | M2; S0–S5. Schema comum e mundo de execução entregues em 10/09/2026; console e campos tipados do inspetor continuam pendentes |
| B | Transform, MeshRenderer, Camera e Light anexáveis; IDs de assets; importação e extração de cena | A; M2–M5 |
| C | Formas, corpos, colliders compostos, sensores, juntas, forças e queries | A+B; M6. Queries com contato real, camadas de gameplay no solver e contatos sólidos entregues em 10/09/2026; ShapeAsset, convexos, malha côncava e sensor por colisor continuam pendentes |
| D | InputActionMap, animação, áudio, partículas, rigs e comportamentos reutilizáveis | A+B+C; M7; S5–S6. InputActionMap e sete comportamentos reutilizáveis entregues em 10/09/2026; animação, áudio, partículas e rigs não iniciados |
| E | Ambiente, volumes, sombras, efeitos, render targets e streaming | B; M5/M8 |
| F | Renderer e física 2D, sprites, tiles e juntas 2D | A+B; extensão com backend próprio |
| G | Documento UI de jogo, layout, controles, bindings e efeitos UI | A+B+D; M7–M9 |
| H | Terrain, curvas, navegação, geração de estradas e edição por regiões | B+C+D; M8–M9 |
| J | Veículos, tecido, articulações, voxels, XR/rede e módulos de plataforma | Dependências específicas; M9–M11; S7–S8 |
| R | Esclarecer semântica insuficientemente documentada | Obrigatório antes de implementar os tipos marcados; não bloqueia os demais |

Cada linha dos quadros detalha a parte específica desse trabalho: âncoras/motores para juntas, clips/bindings para animação, motion vectors para motion blur, chunks e cooking para voxels, foco/IME para input de texto etc. O caminho geral de entrega é dados persistentes → serviço/runtime → extração/efeito real → comandos autorais → UI. Testes e refinamento de execução entram quando o usuário autorizar; o estado continua “sem validação” até lá.

## Universalidade e forma de colisão

“Universal” significa composição independente do nome do objeto, da malha e da cena. Não significa que uma esfera e uma malha côncava tenham o mesmo custo ou possam ocupar os mesmos papéis físicos. Unity mantém tipos de collider explícitos; a documentação ItsMagic oferece seleção de shape e dados de vértices/convexidade. A malha visual não determina automaticamente qual aproximação é adequada ao gameplay. Fontes: [Unity — Box Collider](https://docs.unity3d.com/6000.0/Documentation/Manual/class-BoxCollider.html), [ItsMagic — Collider](https://itsmagic.com.br/documentation/docs/api-reference/Components/Physics/Collider/).

Na Astra, o novo comando **Ajustar à malha** calcula bounds e centro a partir dos triângulos disponíveis no recurso, prepara caixa/esfera/cápsula envolventes e escolhe o menor volume permitido pela escala global. Esfera/cápsula exigem escala uniforme; cápsula usa eixo Y. Todos os vértices ficam dentro da forma sugerida, mas a aproximação não representa furos/reentrâncias nem reconhece semanticamente sofá, pedra ou personagem. O mesmo ajuste é tentado na primeira anexação do colisor quando há triângulos disponíveis; sem geometria compatível, permanece a forma padrão editável. O comando de recalcular é explícito e ambos os fluxos passam por Undo. Não é recooking contínuo: mudanças posteriores na geometria exigem novo ajuste. Forma, centro e dimensões podem ser alterados pelo criador.

O payload de collider passa a v2 e lê v1 com centro zero. A API física recebe centro local sem alterar o layout dos descritores V1/V2 existentes. O corpo mantém sua origem autoral, enquanto a forma fica deslocada. O bloco de composição física abaixo acrescenta rotação local, múltiplos colliders, owner explícito e sensor no corpo em fonte. ShapeAsset, convexos, malha triangular e filtros continuam no roadmap.

## Contratos do bloco de código escrito

| Parte | Dono / vida útil | Fluxo / falha |
|---|---|---|
| `Components` | Cada objeto autoral; cópia profunda no histórico | `instanceId` monotônico por objeto; arquivo v9 grava IDs e próximo ID. Vários `ScriptBehavior` podem coexistir |
| `ScriptBehavior` | Dados do projeto | TypeId do script, arquivo, enabled e overrides por PropertyId/tipo. Dados sobrevivem à ausência de compilação |
| `EditorCodeWorkspace` | Sessão do editor | Até 16 buffers de 512 KiB, Undo/Redo próprios, busca e gravação atômica. Alteração externa interrompe gravação e mantém buffer |
| `ProjectCompiler` | Worker de compilação no processo Android | Roslyn C# 12; snapshot de até 1.024 arquivos/32 MiB. Emite DLL/PDB/schema; não instancia classes durante compilação |
| `NativeCompiler` | Serviço gerenciado do processo | Resultado candidato só publica `.astra/code/current` após aceitação da geração atual. Falha preserva geração anterior |
| `ScriptRuntimeApi` | Serviço injetado pela plataforma | ABI v2 sem headers do editor. Root/anexos UTF-8; comandos e eventos físicos com vida de uma sessão Play |
| `BehaviorWorld` | Uma sessão Play | AssemblyLoadContext coletável e instâncias por objeto/instanceId. Start/Update/FixedUpdate/Stop; falha por callback desabilita a instância e registra diagnóstico |
| `EditorScriptBridge` | Adaptador transitório do editor | Só acessa a cópia de execução. Transform local, velocidade global e movimento cinemático usam consumidores reais |

Os callbacks de acesso à cena aceitam apenas a thread proprietária. FixedUpdate acontece antes de cada passo Jolt de 1/60 s; poses são publicadas após cada passo para que o próximo callback veja a posição atual. Stop chama os comportamentos iniciados antes de destruir física/documento. Objetos inativos na origem não são instanciados. Alterar a árvore/atividade durante Play ainda não possui API pública.

`[ComponentId("project.id")]` identifica uma classe pública concreta derivada de `Astra.Behavior`; `[PropertyId("campo")]` identifica campo/propriedade pública mutável. Tipos atuais: bool, int32, float, string, enum com base int32, Vector3, ObjectReference e AssetReference. Campos sem override mantêm o valor do código. Novo schema não apaga overrides desconhecidos; mudança de tipo exige correção/migração explícita. Referências internas a objetos são remapeadas ao duplicar uma subárvore; referências externas permanecem apontando ao original. A fronteira `EditorAction` também recebe anexação de script, remoção por instanceId, edição tipada, habilitação e ajuste de collider, com versão de cena e histórico transacional.

## Pendências do bloco atual

- Tudo que foi escrito após a nova orientação está **não compilado e não executado**. APKs, capturas e resultados antigos não comprovam este bloco.
- IDE ainda usa edição de texto nativa Android. Syntax highlighting, autocomplete semântico, rename symbol, debugger, breakpoints, restauração de buffers após morte do processo, abas com rolagem e navegação de diagnósticos ainda faltam.
- Código compila no processo. Não existe contenção de loop infinito ou isolamento de scripts em outro processo. AssemblyLoadContext não é sandbox de segurança. Dependências NuGet arbitrárias do projeto não estão implementadas.
- Aplicar publica artefatos; hot reload de uma sessão Play ativa e migração de estado executando ainda não existem. ObjectReference agora usa seletor visual de objetos. AssetReference genérico ainda tem entrada textual; enums C# ainda usam valor numérico. Coleções/recursos aninhados e migração por aliases estão pendentes.
- Salvar todos os buffers pode salvar um prefixo antes de encontrar conflito no próximo; a compilação é interrompida nesse caso. Não há transação única de todos os arquivos nem watcher de mudanças externas durante o worker.
- Transform via script é recusado para um objeto com corpo/personagem ou ancestral de corpo/personagem, pois propagar a alteração para Jolt ainda falta. Corpos cinemáticos usam `MoveKinematic`; velocidade usa `SetBodyVelocity`. Forças/impulsos/torques e eventos de sensores foram ligados em fonte no bloco abaixo. Criação/remoção em Play, input abstrato, contatos sólidos e acesso genérico a todos os componentes ainda faltam.
- Runtime ainda depende do adaptador sobre `EditorDocument` para a cópia Play. A separação completa do player independente e exportação Android permanecem abertas em M9–M11/S7–S8.
- Os novos anexos usam o ícone de código já existente. Ícones próprios para famílias ainda não implementadas serão produzidos quando a função e o fluxo estiverem concretos; não há placeholders funcionais no menu.

Este documento acompanha a implementação em escala; não declara a refundação completa nem converte a lista de referências em funcionalidades prontas.


## Continuação: câmera, malha e inspetor — 10/09/2026

[Contrato detalhado e rastreio do layout](../adr/ADR-REFUNDACAO-CAMERA-MESH-INSPECTOR.md). O catálogo nativo passa a seis tipos: Corpo físico, Personagem, Olhar, Colisor 3D, Câmera e Malha, além dos scripts do schema aplicado e dos dados opcionais anteriores. Essa contagem não é cobertura de Unity/ItsMagic.

Câmera e Malha agora são componentes anexáveis independentes do `kind` histórico. Arquivo v10 migra dados das versões 1–9 e deixa de gravar malha/material fora dos componentes. A escolha da câmera usa prioridade, e seus planos/FOV percorrem os fontes dos consumidores gráficos. Malha possui referência de pacote, habilitação e overrides de material. O inspetor usa cabeçalhos compactos, Add com busca/categorias, campos numéricos do descritor e seletor da geometria existente.

A afirmação anterior de que câmera/malha ainda dependiam somente de campos especiais foi superada em fonte por este bloco. As limitações continuam: referências qualificadas por pacote, sem AssetGuid/MaterialAsset geral, sem projeção ortográfica ou roll, sem miniatura 3D renderizada; Transform permanece TRS obrigatório da entidade e a execução usa EditorDocument. A seção anterior de scripts ainda se aplica.

**Sem compilação ou execução.** GLSL novo também aguarda geração de SPIR-V. Testes e ADB continuam dependentes da autorização do usuário.


## Continuação: composição física e referências — 10/09/2026

[Contrato, fontes, versões, limites e futura aceitação](../adr/ADR-REFUNDACAO-COMPOSICAO-FISICA.md). [Exemplos C#](fisica-codigo.md). O catálogo passa a sete tipos nativos, com Junta anexável em quatro modalidades; não são quatro componentes artificiais nem cobertura total das engines de referência.

Código escrito: Colisor v3 repetível, pose local e owner explícito neste objeto/ancestral; Corpo v3 com compound, sensor, damping, gravidade/repouso e giro inicial; Junta ponto/dobradiça/deslizante/distância ligada ao Jolt; referências tipadas com seletor compartilhado por C#; remapeamento/Undo/reparent; menu de instância e overlay de forma/âncoras; comandos de força/impulso/torque e eventos de sensor no runtime C# v2. PNG próprio de Junta integrado ao atlas.

As pendências anteriores de múltiplos colliders, rotação local, owner, junta e seleção de ObjectReference foram tratadas em fonte neste bloco. Não afirmar que foram comprovadas. Sensors são por corpo e eventos por par; ShapeAsset, filtros por subforma, CharacterMotor nesses eventos, contatos sólidos, joints avançados/ruptura, mutação de shapes em Play, código/recursos/player completos continuam pendentes.

**Não compilado nem executado.** Apenas documentos/assets foram gerados. Shaders derivados, testes, APK e ADB permanecem aguardando autorização. O quadro continua contendo 117 entradas Unity e 153 ItsMagic; status parcial significa consumidor escrito com limites, não equivalência integral nem função validada.

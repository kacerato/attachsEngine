# Astra — universalidade, componentes, objetos e construção visual

**Relatório e plano de expansão · 15 de setembro de 2026**  
**Base auditada:** `codex/gameplay-runtime`, `f3801ba3`.  
**Escopo desta entrega:** leitura de código, documentos, evidências já existentes e fontes oficiais; produção de documentação e catálogo. Nenhuma implementação da engine, compilação, teste ou sessão ADB foi executada nesta análise.

**Continuação autorizada em 15/09:** a primeira implementação de P01–P04 está descrita em [Base de componentes e câmera](P01-P04-BASE-COMPONENTES-CAMERA-2026-09-15.md). As conclusões abaixo preservam o snapshot da análise; a continuação acrescenta grupos/condições de propriedades, composição de dependências, proteção de referências e representação/câmera editorial. P01–P04 continuam parciais, sem validação no aplicativo. Consultar a matriz de entrega e os limites do registro antes de delegar ou marcar itens como concluídos.

## 1. Próximo passo recomendado

O próximo pacote deve unir **propriedades e dependências universais + representação dos componentes na cena + câmera editorial completa + inspetor com construção visual comparável ao IDE**. A câmera é a primeira composição de entrega desse pacote. Sua representação não deve ser uma exceção desenhada diretamente no viewport: a mesma infraestrutura precisa desenhar e permitir editar luzes, emissores de áudio, colisores, limites de partículas e pontos de conexão.

A Astra avançou materialmente desde o relatório de 14/09. O gargalo atual já não é apenas aceitar um GLB ou mostrar suas texturas. É conseguir percorrer **objeto → componente → recurso → propriedade → dependências → resultado**, com controles consistentes e sem perder a capacidade de personalização em cada transição. As melhorias de R1–R4 devem ser aproveitadas. Não recomendo recomeçar o importer, o viewport ou o IDE.

O trabalho deve acontecer em blocos completos. Uma propriedade só está entregue quando possui representação persistente, edição, comando/histórico quando aplicável, API, consumidor real e comportamento definido na reabertura e no runtime. Os roteiros de aceitação deste plano são trabalho futuro, dependente da autorização do usuário para execução.

### Arquivos para usar este relatório

- [Atlas pesquisável offline: componentes, propriedades e quatro direções de layout](../componentes/pesquisa-2026-09-15/ATLAS-UNITY.html).
- [Catálogo integral em Markdown](../componentes/pesquisa-2026-09-15/CATALOGO-UNITY.md).
- [Dados estruturados do catálogo](../componentes/pesquisa-2026-09-15/catalogo-unity.json).
- [Cobertura, versões, dependências de pacote e limitações](../componentes/pesquisa-2026-09-15/resumo-cobertura.json).
- [Objetos, receitas de composição e caminhos de uso](../componentes/pesquisa-2026-09-15/OBJETOS-E-CAMINHOS.md).
- [Registros literais de menus GameObject encontrados nas fontes](../componentes/pesquisa-2026-09-15/menus-gameobject.json).
- [Plano mestre existente](PLANO-MESTRE-ASTRA-EDITOR-RUNTIME-ASSETS.md) e [relatório anterior de importação, texturas e malhas](RELATORIO-IMPORTACAO-TEXTURAS-MALHAS-2026-09-14.md).

## 2. O que mudou de verdade no repositório

Esta tabela substitui, para o snapshot atual, conclusões antigas que ficaram desatualizadas. “Implementado” significa encontrado no código/documentação atual; não significa revalidado neste atendimento.

| Frente | Avanço encontrado | Limite que permanece relevante |
|---|---|---|
| Abertura do projeto | Preparação de fontes em worker, progresso por recurso, publicação agregada e restauração da cena depois da biblioteca | Primeiro frame não equivale a projeto pronto; preparação e cache ainda são caros |
| Cache | Chave com conteúdo/perfil/revisão, escrita atômica e cache de dados decodificados | Cache inicialmente produzido na reabertura; ainda há leitura/hash integral das fontes e grande volume de dados |
| Publicação | Reuso de texturas pela identidade compartilhada e de buffers geométricos por igualdade dos dados | Não é residência completa por revisão/epoch; há reconstruções de instâncias, filas e descritores |
| Importação na interface | Dock real em Propriedades, com Resumo/Estrutura/Texturas/Perfil | Falta preview 3D, seleção de subasset no preview e edição rica por nó/malha/material |
| Perfis | Escala da raiz, limite de textura, perfil por fonte, padrão do projeto e reapreparação do rascunho | Escala oferecida em passos fixos; faltam eixos, políticas de geometria/animação e decisões por item |
| Materiais | Asset versionado, vínculo compartilhado, overrides por instância, texturas e fatores | Operações sobre recurso compartilhado ainda precisam integrar melhor histórico e impacto nos usuários |
| Aparência | Alpha, cutoff, dupla face, transformações UV, canais de dados e AO próprio de projeto | AO ainda compartilha UV/sampling com metallic/roughness; alguns efeitos dependem de recursos do dispositivo |
| Texturas | Extração PNG/JPEG, GUID, miniaturas, canais, mips, zoom e perfis por textura | Viewer de fonte não é inspeção exata da imagem residente na GPU; KTX2 não tem o mesmo fluxo de edição/extração |
| Gerenciamento | Grade, busca, filtros e lista de usuários por objeto/material | Relocação, substituição/variantes e histórico completo continuam no trabalho de R6 |
| Samplers | UV0/UV1, wrap/filter, anisotropia agora habilitada quando suportada | Configuração independente de sampler ainda pode duplicar mip chains; fonte embutida tem restrições de edição |

Fontes locais: [R1/R2](R1-R2-ABERTURA-CACHE-RESIDENCIA.md), [R3](R3-DOCK-PERFIS-IMPORTACAO.md), [R4](R4-TEXTURAS-MATERIAIS.md), `native/resources/material_asset.h`, `native/scene/mesh_renderer.h`, `native/editor/editor_session.cpp` e `native/editor/editor_screen.cpp`.

### 2.1 O problema da tela preta deve ser descrito com o estado atual

O registro R1/R2 compara um caso antigo de aproximadamente 64,3 s de tela preta com primeiro frame em aproximadamente 1,155 s no caminho quente. O mesmo registro informa cerca de 17,8 s até concluir a abertura quente e 99,4 s no caso frio descrito. São **medições históricas documentadas**, com corpus e condições próprios; não foram repetidas agora. O relato anterior do usuário de 15 s não foi medido novamente.

Portanto, há duas metas: manter apresentação contínua desde a abertura e reduzir o trabalho até a cena editável. Cobrir um intervalo com UI resolve a continuidade, mas não elimina o custo. O próximo trabalho de desempenho deve separar leitura, hash, cache, decode, criação de derivados, publicação e residência. Não se deve anunciar “abertura resolvida” apenas porque o fundo deixou de ficar preto. [R1/R2](R1-R2-ABERTURA-CACHE-RESIDENCIA.md)

### 2.2 Restrições concretas que enfraquecem a universalidade

1. `component_schema.h` concentra nove schemas. É uma base correta, mas ainda pequena; não equivale ao catálogo de capacidades desejado.
2. `components.h` oferece contratos para número, booleano, enum e referência de objeto. Ainda não há um modelo comum suficientemente rico para estruturas aninhadas, listas, cores, curvas, gradientes, recursos, máscaras e eventos em todos os componentes.
3. Requisitos de componente são predominantemente relações de tipo no mesmo objeto. Necessidades de recurso, ancestral, serviço, fase e backend exigem uma representação mais expressiva.
4. O componente Camera possui `enabled`, FOV vertical, near, far e prioridade. Em `editor_scene_camera.h`, a seleção da câmera usa pose mundial, mas a orientação de saída é reduzida a yaw/pitch. Isso descarta roll nesse caminho; qualquer promessa de câmera física completa precisa enfrentar essa limitação.
5. A extensão de material por desenho usa tabela com limite de 1.024 entradas no caminho documentado. O excedente não pode perder transformações de textura silenciosamente enquanto o usuário acredita que o material está aplicado.
6. Diferentes samplers podem provocar duplicação do conteúdo de imagem. O modelo deve separar imagem, visão da imagem, sampler e binding de material.
7. AO independente precisa também ter referência, canal, UV, transform e sampling independentes. Uma textura própria com parte das propriedades herdadas de outra entrada ainda deixa acoplamento semântico.
8. A classificação “sem alpha” do gerenciador se apoia em thumbnails e um subconjunto processado. Estado não examinado deve aparecer como desconhecido; a miniatura não deve ser apresentada como prova exata de ausência de alpha da fonte inteira.
9. Perfil inexistente e perfil inválido não são o mesmo estado. Cair silenciosamente no padrão após erro pode alterar a intenção de importação do projeto.
10. O schema e os painéis precisam expor valores contínuos quando o domínio é contínuo. Presets de escala, UV e dimensões são atalhos; não devem substituir a entrada válida de valores.

Esses itens são alvos derivados da inspeção, não reprodução de defeitos no aparelho. Algumas restrições podem ser decisões temporárias de custo; precisam de estado e consequência visíveis, em vez de parecerem capacidade geral concluída.

### 2.3 Higiene da documentação existente

R4 conserva seções antigas de limitações depois de registrar implementações novas. Algumas afirmações sobre usuários de textura, AO e integração com sombras ficaram superadas por seções posteriores. Manter histórico é útil; falta uma seção inicial de **estado consolidado por capacidade e commit**. Essa correção documental evita que outro agente reimplemente trabalho já feito ou marque uma capacidade como inexistente por ler o último subtítulo.

## 3. O que significa “todos os componentes Unity” nesta entrega

O catálogo anterior tinha 117 tipos Unity do núcleo e um inventário adicional parcial de pacotes. A pesquisa atual amplia o recorte para **Unity 6000.0 e 21 pacotes/fontes adicionais fixados**, consultando 117 páginas da API do núcleo e fontes de runtime dos pacotes. O resultado contém:

| Classe de informação | Quantidade | Interpretação |
|---|---:|---|
| Tipos derivados de Component selecionados | 575 | Inclui bases, auxiliares, condicionais e obsoletos; não são 575 opções anexáveis |
| Elementos UI Toolkit | 150 | Controles de outra árvore de UI; não são componentes de GameObject |
| Tipos de perfil Volume | 65 | Parâmetros em recursos/perfis de renderização |
| Recursos e contratos selecionados | 61 | Malhas, materiais, texturas, input, timeline e outros contratos |
| Tipos de apoio | 1.625 | Bases, enums, módulos, estruturas e tipos referenciados |
| Total de fichas | 2.476 | Tipos organizados, não capacidades prontas da Astra |
| Propriedades/campos declarados extraídos | 14.030 | Inclui API e candidatos serializáveis; propriedade não é automaticamente um controle de Inspector |
| Propriedades/campos próprios dos tipos Component | 6.742 | Herança é consultável separadamente |

### 3.1 Cobertura por família de pacotes

| Fonte | Versão fixada | Component |
|---|---|---:|
| UnityEngine | 6000.0, revisão fixada | 117 |
| uGUI / TextMeshPro | branch 6000.0, revisão fixada | 54 |
| Cinemachine | 3.1.3 | 77 |
| AI Navigation | 2.0.5 | 4 |
| Animation Rigging | 1.3.0 | 17 |
| Input System | 1.11.2 | 11 |
| Splines | 2.7.2 | 5 |
| Timeline | 1.8.13 | 1 |
| Netcode for GameObjects | 2.2.0 | 18 |
| Localization | 1.5.13 | 10 |
| Addressables | 2.3.16 | 1 |
| 2D Animation | 10.0.3 | 8 |
| 2D SpriteShape | 10.0.7 | 2 |
| 2D Tilemap Extras | 4.0.2 | 1 |
| XR Core Utils | 2.4.0 | 2 |
| XR Interaction Toolkit | 3.0.9 | 125 |
| AR Foundation | 6.0.8 | 45 |
| Render Pipelines Core | 17.0.4 | 44 |
| URP | 17.0.4 | 10 |
| HDRP | 17.0.4 | 20 |
| Visual Effect Graph | 17.0.4 | 3 |
| Shader Graph | 17.0.4 | 0 neste recorte; seus recursos não são Component |

Cada manifesto registra origem, versão, revisão ou hash e dependências de pacote. As contagens são resultado da seleção das fontes, não contagem de menus Unity. Algumas bases do catálogo antigo estavam misturadas com exemplos; não interpretar diferenças apenas como funções removidas ou acrescentadas.

### 3.2 O que as fichas entregam

Nome completo e pacote evitam confundir classes com o mesmo nome. A ficha inclui base e ancestrais; propriedades/campos com tipo e acesso; atributos encontrados; enums; métodos públicos declarados; requisitos `RequireComponent` próprios e herdados; quem exige o tipo; quem o referencia por propriedade; derivações; origem e linha; roteiro da família e pacote do roadmap.

A relação reversa é tão importante quanto a direta. Ao abrir uma textura, interessa saber quem a utiliza; ao abrir um componente-base, interessa saber quais tipos o especializam; ao remover uma dependência obrigatória, interessa saber quais componentes ficam inválidos. O atlas mantém essas relações separadas.

### 3.3 Limites explícitos — trabalho ainda necessário

Não existe lista finita de todos os componentes que podem existir na Unity: scripts de projeto e pacotes externos expandem o conjunto. Esta entrega **não afirma cobertura integral de Asset Store, DOTS/Entities, todos os serviços ou todos os pacotes Unity existentes**. Os limites e versões fazem parte do relatório, não de uma nota escondida.

A extração de C# é estrutural, não execução do compilador da Unity. Diretivas condicionais podem reunir APIs de configurações diferentes. **134 fichas** do conjunto final apontam arquivos com recuperação sintática/preprocessador; elas são sinalizadas para revisão. Resolução de tipos genéricos, aliases e condições exige confirmação antes de gerar contratos definitivos. Os CustomEditors e PropertyDrawers não foram inventariados integralmente. Defaults, intervalos, unidades, condições de visibilidade e semântica de cada campo ainda exigem curadoria individual. Um campo `TData` liga à estrutura concreta do constraint, mas o atlas não substitui toda a especialização do compilador.

Assim, este é um inventário extenso de contratos e relações com fontes, acompanhado de um plano funcional. **Ainda não é uma certificação de paridade de cada propriedade de cada Inspector Unity.** Não seria correto apresentar os 14 mil nomes como 14 mil controles revisados ou já implementados na Astra. A tarefa de fechamento por campo está especificada em P01 e P18; nenhum item fica implicitamente considerado entregue.

## 4. Universalidade precisa atravessar o produto inteiro

### 4.1 Contrato de propriedade proposto

Expandir os descritores existentes, preservando IDs e migrações. Os nomes abaixo definem responsabilidades propostas, não classes que necessariamente já existem.

| Grupo | Metadados necessários | Consumidores |
|---|---|---|
| Identidade | TypeId, PropertyId, versão, aliases de migração | Arquivo, API, histórico, prefab, importação |
| Valor | Inteiro, real, booleano, enum, flags, string, vetor, quaternion, cor linear/sRGB, matriz, curva, gradiente, lista e estrutura | Inspector, código, animação, NoCode |
| Referência | Tipo de asset/objeto/componente aceito, subasset, escopo, null permitido, resolução e revisão | Picker, drag/drop, grafo de dependências, reimportação |
| Domínio | Default explícito, mínimo/máximo quando existirem, unidade, precisão, passos de incremento opcionais | Editor numérico, gizmos, validação de comando |
| Contexto | Visível quando, editável quando, herdado/local, somente leitura, runtime-only, dependência de capacidade | UI, console, execução |
| Mudança | Preview, confirmação transacional, cancelamento, invalidação necessária, consumidor responsável | Undo/Redo, worker, renderer, física |
| Apresentação | Nome curto, ajuda contextual, grupo, ordem padrão, editor especializado, favorito | Layout customizável e acessibilidade |
| Integração | Animável, observável, bindable por evento, override em instância, autorização por fase | Animator, scripts, eventos, rede futura |

Separar editor especializado de semântica. Um seletor de cor e quatro campos numéricos podem editar a mesma propriedade; ambos usam o mesmo comando. Uma ferramenta visual de FOV e o campo FOV também. Mudar o layout não cria outra API de valor.

### 4.2 Dependências: cinco relações diferentes

| Relação | Exemplo | Comportamento esperado |
|---|---|---|
| Obrigatória de composição | Atributo RequireComponent | Mostrar e resolver ao adicionar; impedir remoção que invalida o conjunto ou oferecer remoção conjunta transacional |
| Referência funcional | Câmera de acompanhamento aponta para alvo | Permitir não configurado quando legítimo; indicar ausência e não executar comportamento fictício |
| Recurso | MeshRenderer usa malha/material | Picker tipado, disponibilidade, revisão, preview e usuários reversos |
| Serviço/fase | Corpo necessita mundo físico; áudio necessita backend | Registrar requisito do projeto/runtime e estado de inicialização |
| Capacidade | Sombra pontual, multivista, compute, XR | Mostrar suporte real da implementação e do dispositivo; não reduzir a um bool salvo |

`RequireComponent` na Unity adiciona dependências quando `AddComponent` é chamado; não repara retroativamente todos os objetos após mudar o atributo. A Astra pode oferecer uma auditoria de composição adicional, mas isso será uma função própria explícita. [Referência RequireComponent](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/RequireComponent.html)

**Regras que não devem ser inventadas:** uma câmera não precisa de AudioListener para gerar imagem; uma malha renderizável não precisa de Rigidbody; um collider estático não exige corpo dinâmico; um botão visual não exige o mesmo conjunto de serviços de um botão interativo; um widget UI Toolkit não recebe RectTransform só porque um Button uGUI o usa. Requisitos por algoritmo ou modo devem ser predicados condicionais.

### 4.3 Grafo de impacto e ações universais

O AssetRegistry já guarda dependências e consulta dependentes. Ampliar essa base para alcançar componentes, propriedades e referências de subassets, em vez de criar um segundo registro de recursos concorrente. Uma aresta deve informar origem, propriedade, destino, força da dependência e revisão.

O inspetor oferece “Usado por” e “Depende de”, com navegação reversível. Trocar GUID, remover fonte ou substituir malha prepara uma lista de impactos. O usuário pode reparar referências, extrair variante, manter origem ou cancelar. A resolução conserva IDs sempre que a identidade permanece; hash representa revisão de conteúdo, não a identidade autoral.

Adicionar vários componentes com requisitos deve formar uma transação. Desfazer remove exatamente o que foi criado nessa operação; componentes preexistentes continuam. Dependência cíclica, incompatibilidade, limite de cardinalidade e tipo ausente precisam gerar diagnóstico localizado antes da publicação.

### 4.4 Customização em três escalas

**Do componente:** grupos recolhíveis, ordem visual, campos favoritos, modos básico/avançado, edição múltipla, presets de valores e menus de contexto. “Avançado” expande controles existentes, não esconde suporte inexistente.

**Do workspace:** abrir, fixar, dividir, mover, recolher e redimensionar painéis; tabs de documento e de recurso; salvar disposição por orientação; restaurar layout. Persistir identidade do painel, seleção, histórico de navegação e scroll. Não persistir ponteiros de UI.

**Do projeto:** perfis de importação, qualidade, física, input, naming opcional, coleções e presets de componentes/recursos. Preferências pessoais do editor ficam separadas dos dados do projeto compartilháveis.

## 5. Câmera: percurso completo exigido

O usuário tem razão em esperar uma câmera reconhecível ao criar esse objeto. Na Unity, ícones de câmera/luz são representação editorial: podem ter escala pela distância e oclusão ou tamanho fixo, conforme configuração de gizmos. O desenho não precisa ser uma malha do jogo. [Gizmos menu](https://docs.unity3d.com/6000.0/Documentation/Manual/GizmosMenu.html)

### 5.1 Representação na cena

| Item | Entrega | Dependência |
|---|---|---|
| Símbolo de câmera | PNG transparente da família Astra ou proxy 3D editorial com silhueta clara | Registro de representação por TypeId e atlas editorial |
| Seleção | Toque no símbolo seleciona o ObjectId/ComponentInstanceId correto | Picking editorial separado de geometria do jogo |
| Orientação | Corpo/lente apontam para o mesmo eixo da vista real | Convenção de pose compartilhada; orientação completa |
| Frustum | Near/far, bordas e plano de imagem derivam da projeção efetiva | Matriz de projeção e viewport/aspect |
| Alças | FOV, near/far e tamanho ortográfico editáveis na cena | PropertyId, unidade e transação de arraste |
| Estados | Selecionada, ativa, inativa, oculta editorialmente, inválida | Estado de runtime separado da preferência de gizmo |
| Legibilidade | Tamanho mínimo/máximo, fade, ícones agrupados e filtro por tipo | Política de visualização e orçamento de overlays |
| Segurança autoral | Proxy sem colisão, sombra, exportação ou componente MeshRenderer extra oculto | Camada editorial efêmera derivada do documento |

Não anexar um cubo/câmera decorativa ao projeto para simular esse recurso. Se o usuário quiser um modelo físico de câmera como objeto do jogo, isso será uma composição autorada normal e independente.

### 5.2 Prévia e pilotagem

A Unity oferece lista pesquisável de câmeras, prévia picture-in-picture e controle em primeira pessoa; escolher uma câmera para prévia pode ser independente da seleção na hierarquia. Essa separação é útil para manter uma prévia fixada enquanto se edita outro objeto. [Cameras overlay](https://docs.unity3d.com/6000.0/Documentation/Manual/cameras-overlay.html)

Na Astra, implementar: selecionar câmera de prévia; fixar/desafixar; enquadramento com aspecto da saída; resolução e frequência ajustáveis; suspender quando oculta; abrir propriedades; alinhar câmera à vista editorial; olhar através dela; pilotar e confirmar/cancelar a pose; indicar qual câmera alimenta cada saída. A navegação editorial comum continua sem tornar a cena dirty. A pilotagem que altera a câmera autorada gera histórico.

O preview exige uma RenderView independente, com alvos e estado por vista. Não modificar a câmera global do viewport, não iniciar outro mundo de simulação e não reconstruir a cena para cada miniatura. O orçamento deve considerar largura, altura, frequência, efeitos e número de previews ativos.

### 5.3 Propriedades a desenvolver, por dependência

| Grupo | Controles / contratos | Situação Astra e próximo consumidor |
|---|---|---|
| Base | enabled, priority, near, far, vertical FOV | Já existe núcleo; unificar com gizmos e preview |
| Pose | posição/orientação mundial completa, roll e hierarquia | Corrigir redução para yaw/pitch na resolução da câmera |
| Projeção | perspectiva, ortográfica, tamanho, aspecto, eixo de FOV | Nova projeção, culling, picking e persistência |
| Saída | viewport rect, RenderTexture/RenderTarget, resolução e orientação | Gerenciamento de alvos e múltiplas vistas |
| Visibilidade | máscaras/camadas, distâncias por camada quando suportadas | Extração/culling por vista, sem alterar visibilidade autoral global |
| Fundo | cor, ambiente/céu e política de limpeza | Render graph e composição de saída |
| Qualidade | HDR, MSAA, escala de render, efeitos permitidos | Negociação de capacidades e orçamento por vista |
| Física da lente | sensor, distância focal, lens shift, gate fit | Equivalência matemática com FOV/aspect; unidade e validação |
| Exposição e foco | ISO, obturador, abertura, foco/DOF | Renderer de exposição/DOF; não basta serializar números |
| Composição de câmeras | prioridade, transição, pilha/composição quando oferecida | View scheduling, mistura e regras de profundidade |
| API espacial | world↔screen, ray, viewport↔world, frustum | Usar a projeção efetiva, inclusive ortográfica e target rect |
| Comportamentos | seguir alvo, mirar, amortecer, evitar obstáculos, caminhos | Transform/input/física/curvas; comportamentos reutilizáveis |

A lista exata da API Camera, com herança, está no atlas. Campos específicos de URP/HDRP pertencem às extensões daqueles pipelines; a Astra deve mapear a intenção para sua arquitetura. Godot Camera3D também liga a câmera a um Viewport e oferece projeção, máscaras e ambiente; isso reforça a necessidade de separar componente e vista, sem copiar o modelo de nós como se já existisse na Astra. [Unity Camera](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Camera.html), [Godot Camera3D](https://docs.godotengine.org/en/stable/classes/class_camera3d.html)

### 5.4 Critério de pacote completo

Criar câmera, vê-la na cena, selecionar pelo símbolo, editar frustum e propriedades, pilotar, fixar preview, desfazer, salvar/reabrir e usar a mesma câmera em Play. Adicionar duas câmeras e um objeto alvo deve demonstrar que preview, seleção e câmera ativa são estados diferentes. Este roteiro deverá ser executado apenas na fase autorizada; não está aprovado por esta análise.

## 6. Layout: quatro alternativas reais de composição

Foi inspecionada a evidência local `docs/validacao/evidencias/r4-gerenciador-texturas-20260915/03-textura-em-propriedades-perfil-usuarios.png`. Há preview útil e controles reais. O problema visual está na concentração de linhas de botões retangulares com peso semelhante, pouco contraste entre níveis de informação e textos longos na área estreita. Mudar somente o raio dos cantos não resolve essa estrutura.

Preservar a identidade aprovada em [FIDELIDADE do IDE](../design/m06-ide-v2/FIDELIDADE.md). A Astra já possui caminho nativo capaz de desenhar composição mais elaborada. Não há justificativa demonstrada para trocar toda a tecnologia de UI só para redesenhar tabs. Primeiro separar layout, comandos, tokens e componentes visuais; avaliar troca tecnológica apenas se uma limitação concreta impedir o resultado.

### 6.1 Estúdio — padrão de cena

Hierarquia e arquivos compactos à esquerda; cena ampla; Propriedades à direita; console inferior recolhível. A aba ativa se conecta ao corpo do painel, com recorte superior e linha de separação interrompida. Tabs inativas têm superfície recuada; estado alterado usa ponto discreto. Cabeçalho apresenta ícone, nome editável e menu; metadados ficam na linha secundária.

Dentro do inspetor, cada componente nasce recolhido, com título reconhecível e resumo contextual. Ao abrir, grupos internos têm espaço e alinhamento, não moldura em torno de cada número. Recursos aparecem como uma linha com miniatura, nome, origem e ação de abrir. Uma câmera selecionada oferece preview fixável; uma textura selecionada oferece viewer, sem abrir modal obrigatório.

### 6.2 Bancada — trabalhar profundamente um recurso

Ao abrir malha/material/textura, a região central vira um documento de recurso com preview grande, abas próprias e ferramentas específicas. Propriedades continuam como dock. Breadcrumb mostra Projeto → Fonte → Malha → Superfície; uma faixa de subassets permite alternar partes sem retornar à lista inicial.

Uma malha apresenta submesh/UV/normais/LOD/colisão como ferramentas do preview. Uma textura apresenta canais, mip e comparação fonte/derivado. O custo é ocupar temporariamente o centro; compensar com aba da cena preservada e retorno imediato. Esta opção atende o desejo de edição elaborada melhor do que comprimir todas as ferramentas na lateral.

### 6.3 Comparação — reimportação e impacto

Modo temporário com versão anterior e candidata em vistas sincronizadas, árvore de mudanças e decisões por item. Material, hierarquia, pivô e referências aparecem como mudanças específicas. Conflitos oferecem manter local, usar origem ou reparar vínculo; a decisão informa quantos objetos serão afetados.

O resumo continua acessível, mas não é o conteúdo principal. Antes/depois visual, identificação de subasset e propriedades alteradas dão sentido à operação. A publicação final é única; cancelar descarta a candidata. O modo pode ocupar a Bancada ou expandir o dock conforme espaço disponível.

### 6.4 Foco — retrato e telefone com teclado

Uma superfície principal; arquivos e hierarquia em drawer; tabs horizontais com rolagem; faixa contextual curta; console em painel inferior com altura ajustável. Propriedades, código e preview alternam preservando seleção e rascunhos. O teclado usa a orientação real do Android, com insets e campo visível.

Não obrigar quatro painéis lado a lado em largura insuficiente. A mesma função continua disponível por navegação e estado persistente. No tablet/dobrável, permitir código e preview divididos; no telefone, não ativar multivista custosa automaticamente.

### 6.5 Construção visual comum e personalização

| Elemento | Diretriz proposta |
|---|---|
| Superfícies | Fundo, painel, card e campo têm níveis de contraste controlados; verde sólido reservado à ação/seleção |
| Recortes | Raios coerentes de 10–16 dp para superfícies principais; encaixes de tabs desenhados como conjunto |
| Espaçamento | Ritmo 4/8/12/16 dp; separar grupos sem desperdiçar altura entre cada campo |
| Toque | Área utilizável de aproximadamente 44–48 dp quando necessário; desenho visual pode ser menor sem sobrepor hitboxes |
| Ícones | Raster transparente com identidade da logo; verificar alpha e tratamento sRGB; sem retângulo preto incorporado e sem SVG |
| Botões | Ícone com nome acessível; texto curto apenas quando desambigua. `Add` abre catálogo por contexto |
| Status | Origem/local/compilando/indisponível fora do rótulo da ação; detalhes sob demanda |
| Campos | Editor apropriado: número contínuo, unidade, cor, referência, máscara, curva e lista; presets opcionais |
| Densidade | Compacta/confortável; tamanho de fonte separado do tamanho do ícone; limites mínimos de legibilidade |
| Painéis | Reordenar/fixar/dividir, salvar workspace, restaurar padrão; adaptar por orientação |
| Estado | Seleção, filtros e navegação preservados; dirty é conteúdo alterado, não troca de aba |

Evitar uma tela inicial com todas as alternativas simultâneas. Elas são disposições de trabalho sobre os mesmos contratos. A primeira versão pode combinar Estúdio como padrão, Bancada ao abrir recurso, Comparação em reimportação e Foco em retrato.

## 7. Malhas, materiais e texturas: continuidade do relatório anterior

### 7.1 Malha é recurso; renderer é consumidor

Separar a edição da instância da edição do asset. No objeto: referência de malha, slots de materiais, overrides, visibilidade e participação em passes. No recurso: topologia, atributos, bounds, submeshes, derivados e usuários. Alterar vértices compartilhados deve mostrar o impacto; criar variante oferece independência deliberada.

O MeshRenderer da Unity separa materiais e opções de renderização. A Unreal oferece um editor de Static Mesh com preview e ferramentas específicas, incluindo colisão e sockets. Essas referências sustentam uma Bancada própria, sem provar suporte equivalente na Astra. [Unity MeshRenderer](https://docs.unity3d.com/6000.0/Documentation/Manual/class-MeshRenderer.html), [Unreal Static Mesh Editor](https://dev.epicgames.com/documentation/unreal-engine/static-mesh-editor-ui-in-unreal-engine)

| Área | Funções que o plano deve entregar | Dependências |
|---|---|---|
| Identidade | GUID/subasset, origem, revisão, usuários, variante e relink | Registry e histórico de recursos |
| Inspeção | Vértices/índices/triângulos, bounds, canais, slots, layout e bytes | Metadados de geometria e preview |
| Visualização | Sólido, wireframe, normais/tangentes, vértices, UV, cores e seleção de superfície | Passes/editor overlay e picking por primitiva |
| Geometria | Normais, tangentes, winding, otimização e compressão explicitamente configuradas | Derivação versionada, preservação de seams, importer |
| UV | Visualização por canal, densidade, identificação de ausência, lightmap UV quando backend existir | UVs, unwrap/bake, renderer |
| Colisão | Forma simples, convexa, decomposição e triângulos conforme corpo/backend | Cooking físico, cache e ferramentas de edição |
| LOD | Níveis, referência de cada malha, thresholds, preview de nível e transição | Simplificador, seleção/culling por vista |
| Vinculação | Pontos/sockets com transform, nomes estáveis e usuários | Subasset de conexão, pose e referências |
| Deformação | Esqueleto, bind poses, pesos, blendshapes e bounds animados | Skinning, animation graph, shader/CPU path |
| Renderização | Sombras, GI/probes, motion vectors e layers conforme suporte | Passes reais, materiais e capacidades |

### 7.2 Texturas: terminar o contrato universal

Manter o que R4 entregou e completar: edição de sampler mesmo para origem embutida; imagem compartilhada com sampler separado; UV/transform independentes em todos os bindings; representação clara de fonte versus derivado/residente; pan e zoom no viewer; classificação confiável de alpha; KTX2/formatos comprimidos com preview e políticas próprias; dimensões e mips efetivamente usados; compressão por plataforma; substituir/relocar mantendo identidade; lista de usuários completa; transação e reversão de edição de recurso compartilhado.

O editor de textura deve abrir informação detalhada sob demanda: dimensões, formato, canais, espaço de cor, bytes de fonte/CPU/GPU e causa de redução. Esses dados não devem ser colados no nome dos botões. Os perfis precisam separar semântica da imagem (cor, normal, dados) da aplicação do binding e do limite global de qualidade.

### 7.3 Importação: configurações por item e preview coerente

O dock já existe; o próximo avanço é editar seu conteúdo. Acrescentar árvore de nós e subassets com seleção real, cena de origem, inclusão por item, unidades/eixos, políticas de normais/tangentes, materiais externos, colisão/LOD gerados quando disponíveis, clipes e animação com consumidor efetivo, câmera/luz importadas, relatório contextual e histórico de decisões.

Godot demonstra configurações por nó/malha/material, extração de recursos e geração opcional de derivados. Também documenta limitações de preview de materiais externos; a Astra deve explicitar quando mostra a fonte ou a candidata efetiva para não repetir ambiguidade. Essas referências orientam o fluxo, não autorização para anunciar todos os geradores. [Godot Advanced Import Settings](https://docs.godotengine.org/en/stable/tutorials/assets_pipeline/importing_3d_scenes/advanced_import_settings.html)

### 7.4 Detecção automática de collider

“Auto” pode sugerir uma forma e dimensões a partir da geometria, mas não conhece sozinho a intenção física. Uma carroceria, uma roda, um gatilho e uma parede podem usar aproximações diferentes. Oferecer ajuste automático de bounds, recomendação explicada e edição posterior. Quando a origem possui um collider autorado, preservar sua intenção; quando não possui, não converter automaticamente tudo em corpo dinâmico.

O cilindro primitivo da Unity, por exemplo, usa collider de cápsula no padrão descrito na documentação; isso demonstra que forma visual e forma física não são idênticas por definição. A Astra pode melhorar a escolha, mas precisa mostrar o resultado e o custo. [Primitivas Unity](https://docs.unity3d.com/6000.0/Documentation/Manual/PrimitiveObjects.html)

## 8. Expansão por famílias — função, caminho e dependentes

As propriedades individuais estão nas fichas do atlas. Esta seção define a cadeia operacional de cada família e seus consumidores. A lista não deve virar centenas de switches no painel.

| Família | Caminho de uso | Quem depende do resultado |
|---|---|---|
| Transform/objeto | Criar entidade, definir pai/local, editar pose e referências | Todos os componentes espaciais, animação, física, seleção |
| Renderer estático | Importar/criar Mesh, vincular renderer e slots de material | Câmera, sombras, picking, LOD, probes, instancing |
| Renderer deformável | Importar esqueleto/pesos/targets, vincular skin e animação | Pose final, bounds dinâmicos, sombras e seleção |
| Física 3D | Forma/cooking, corpo quando necessário, material, filtros e eventos | Joints, personagem, triggers, raycasts, câmera com colisão |
| Joints/articulação | Corpos participantes, anchors, eixos, limites/motores | Mecanismos, ragdoll, suspensão e robótica |
| Física 2D | Mundo/shape 2D, body, filtros, joints/effector | Sprites, tilemaps e gameplay 2D; não presumir solver3D equivalente |
| Animação | Clip/binding, graph/state, blend, relógio e autoridade | Skinning, constraints, root motion, eventos e UI animada |
| Rig/constraints | Alvos/pesos, ordem de solução e ossos | Pose procedural; precisa detectar ciclos e escritores concorrentes |
| Câmeras | Projeção/saída, gizmo, preview e comportamentos | Imagem final, input espacial, composição e efeitos |
| Luz/ambiente | Tipo/unidade, região, sombra/GI quando disponíveis | Materiais, renderer, baking, probes e orçamento |
| Áudio | Clip/import, Source, Listener, mixer/bus, atenuação e filtros | Gameplay, timeline, UI e zona de áudio |
| Vídeo | Fonte/decoder, relógio, target de imagem e saída sonora | Material, UI, timeline e streaming |
| UI uGUI equivalente | Canvas, layout, visual, input, foco e eventos | HUD, menus, widgets e acessibilidade |
| UI Toolkit equivalente | Documento, árvore de elementos, estilo, binding e painel | UI de jogo/ferramentas; arquitetura própria, não componentes por widget |
| Texto | Fonte/atlas, fallback, shaping, layout e material | UI, placas 3D, localização e acessibilidade |
| Navegação | Coleta/bake, dados, agente, obstáculo, link e consulta | Movimento autônomo, multidão e debug de caminhos |
| Splines | Nós/tangentes, recurso de curva, consumidores | Estradas, trajetórias, extrusão, animação e instanciadores |
| Partículas/VFX | Emissão, simulação, recursos, renderer e bounds | Efeitos, eventos, iluminação e orçamento GPU/CPU |
| Terrain/vegetação | Height/layers, tiles, collider, detalhes e LOD | Ambiente, física, navegação, sombras e streaming |
| Volumes/pós | Região/peso, perfil, blending e passes suportados | Câmeras, exposição, cor, profundidade e buffers auxiliares |
| Input | Dispositivo → binding → ação → consumidor | Personagem, câmera, UI, ferramentas e multiplayer |
| Timeline | Asset → tracks/clips → bindings → relógio | Animação, áudio, câmera, sinais e replay |
| Localização | Locale/tabelas → identificador → texto/recurso | UI, fontes, áudio e carregamento |
| Recursos endereçáveis | Catálogo → handles → dependências → liberação | Cena, streaming, UI e conteúdo remoto autorizado |
| Rede | Transporte → sessão → identidade → autoridade → sincronização | Gameplay distribuído e diagnóstico; não implica backend pronto |
| XR/AR | Provedor → origem/sessão → tracking → interação/trackables | Câmera, input, rendering e capacidades específicas do aparelho |

## 9. Roadmap em pacotes completos

Os IDs P01–P18 são pacotes desta revisão, não substitutos dos marcos M00–M15 do plano mestre nem dos R0–R10 anteriores. A coluna de vínculo evita duplicar o roadmap. A ordem define dependências; nenhum prazo ou aprovação de teste é presumido.

| Pacote | Escopo fechado | Depende de | Vínculo existente |
|---|---|---|---|
| P01 · Contrato de propriedades | Tipos ricos, referências, unidades, grupos, condições, migração, API e matriz por campo | Schema/registry atuais | M02/M05/M12 |
| P02 · Composição e impacto | Requisitos condicionais, conflitos, Add transacional, grafo direto/reverso e presets | P01, AssetRegistry | M08/M10/M12, R6 |
| P03 · Câmera e representação editorial | Ícone/proxy, picking, frustum/alças, projeção completa, preview e pilotagem | P01/P02, RenderView | M04/M10/M12 |
| P04 · Casca visual e painéis | Estúdio/Foco, tabs, campos especializados, layout persistente e identidade IDE | P01, comandos existentes | M05/M10/M11, R3 |
| P05 · Bancada de recursos | Documento de mesh/material/texture, preview, breadcrumbs, usuários e navegação | P04, registro atual | M08/M10, R3/R5 |
| P06 · Reimportação visual | Candidata com preview, diff por item, conflito e escolhas persistentes | P02/P05, reconcile atual | M08/M09, R6 |
| P07 · Recursos e residência | Imagem/sampler separados, revision/epoch, derivados incrementais, cache import e budget completo | P01/P02 | M03/M14, R2/R4 |
| P08 · Texturas e materiais completos | Bindings simétricos, limites explícitos, KTX2 viewer, variantes/relink, undo compartilhado | P05/P07 | M09/M13, R4/R6 |
| P09 · Malha autorável | Inspeção de atributos, slots, variante, LOD, sockets, derivados e cooking | P05/P07, backend de geometria/física | M09/M12, R5/R7 |
| P10 · Física e input por capacidade | Extender shapes/joints/eventos, editor de forma, ações e autoridade | P01/P02/P03, solver atual | M12, R7 |
| P11 · Animação e rig | Importação/edição de clips, esqueleto, blendshape, mixer, constraints e Timeline | P01/P07/P09 | M09/M12, R8 |
| P12 · Câmera avançada, luz, áudio/vídeo | Óptica/targets, gizmos de alcance, consumidores e recursos por família | P03/P07/P10; P11 para sequências | M12/M13, R9 |
| P13 · UI e texto | Canvas/layout/widgets/input, eventos, tipografia, fontes e localização | P01/P02/P10, recursos | M12, atlas de UI |
| P14 · Navegação e splines | Bake/consulta/debug, agentes/links; curvas/consumidores reutilizáveis | P09/P10 | M12/M14 |
| P15 · 2D completo | Sprites, tilemap, rig2D, física2D e ferramentas específicas | P01/P07/P10/P13 | Atlas 2D, M12 |
| P16 · Ambiente e autoria gráfica | Terrain, partículas, volumes, shader/compute e qualidade por capacidade | P07/P09/P11/P12 | M13/M14, R9 |
| P17 · Extensões | Rede, XR/AR, conteúdo endereçável e integrações com provedor | Contratos respectivos, capacidade comprovada | Expansões do atlas |
| P18 · Paridade e distribuição | Fechar matriz por propriedade, migração/exportação e evidências autorizadas | Pacotes do escopo de release | M15, R10 |

### 9.1 Primeiro pacote delegável: P01 + P02 + P03 + base de P04

**Objetivo concreto:** criar uma câmera reconhecível na cena e editá-la de ponta a ponta pelo novo contrato universal, preservando objetos, recursos, scripts e IDE existentes.

**Entradas de código para conferir antes de alterar:** `native/scene/components.h`, `native/scene/component_schema.h`, `native/scene/camera.h`, `native/editor/editor_scene_camera.h`, `native/editor/editor_gizmo.cpp`, `native/editor/editor_screen.cpp`, `native/editor/editor_session.cpp`, `native/resources/asset_registry.h` e API managed. Mapear também o dono atual de RenderView e os recursos gráficos antes de criar uma classe paralela.

**Sequência de implementação:**

1. Estender descritores e comandos com referência de asset/componente, vetor/cor/estrutura, grupos, unidades e condições. Migrar campos existentes usados na câmera e no MeshRenderer sem quebrar formatos.
2. Implementar plano de composição que calcula dependências, conflitos e mudanças antes de anexar/remover. Conservar regras existentes de corpo/personagem/joint/câmera, revisando sua semântica com o backend.
3. Criar registro de representações editoriais por tipo, com draw, picking e handles consumindo PropertyId. Entregar câmera e usar luz/colisor como consumidores adicionais da mesma API.
4. Preservar orientação completa e adicionar projeção ortográfica ao contrato da câmera e consumidores relacionados.
5. Implementar preview por vista, seleção/fixação e pilotagem. Separar seleção do objeto, câmera da cena, câmera de preview e câmera ativa de Play.
6. Aplicar tabs e grupos do Estúdio/Foco aos painéis envolvidos; manter identidade visual do IDE. Campos contínuos têm entrada direta; presets ficam opcionais.
7. Integrar persistência, API, histórico e estados inválidos. Atualizar matriz de capacidade e documentação consolidada.

**Não deixar como conclusão parcial:** ícone apenas no menu; frustum desconectado da câmera; preview com cena fictícia; campos salvos sem consumidor; UI exclusiva para Camera que exige copiar todo painel para Light; novo layout que perde seleção/orientação. A entrega pode ser dividida em commits técnicos, mas o pacote funcional só fecha com o percurso inteiro.

### 9.2 Trabalho que pode ser delegado sem sobrepor o núcleo

**Continuação de 15/09/2026:** [P01–P04: implementação e evidências](P01-P04-BASE-COMPONENTES-CAMERA-2026-09-15.md) registra os contratos de apresentação, composição transitiva, proteção de dependências, presença editorial e câmera com Ver/Pilotar/Alinhar. A autorização ADB posterior permitiu validar percursos de adição, desfazer, pilotagem e persistência em um projeto separado. P01–P04 permanecem parciais: projeção ortográfica integrada, preview independente, alças ópticas, tipos ricos e casca Estúdio/Foco continuam pendentes. O atlas Unity não foi marcado como implementado por esse avanço.

Um agente pode cuidar da matriz semântica de propriedades/CustomEditors por família e das fontes; outro, do acabamento da Bancada sobre contratos definidos; outro, da separação imagem/sampler e residência. Fixar interfaces e propriedade de arquivos antes de implementar em paralelo. P01/P02 não devem receber duas versões concorrentes de schema.

### 9.3 Formato obrigatório da matriz de implementação

Cada linha futura deverá conter: referência/versionamento; TypeId/PropertyId Astra; função; default/unidade/domínio; origem do requisito; dependência obrigatória/funcional/capacidade; editor; API; formato/migração; consumidor; impacto de mudança; casos de erro; prova autorizada; estado. Estados permitidos: inventariado, especificado, implementado não validado, validado no escopo indicado, parcial, bloqueado por dependência e não aplicável com justificativa.

Não usar uma única coluna “feito” para câmera, material ou animação. O status precisa alcançar a propriedade e o consumidor. Um exemplo: `Camera.orthographic` não fecha com bool no arquivo; precisa alterar projeção, culling, picking, preview e exportação.

## 10. Evidência necessária quando a validação for autorizada

O pacote precisa ser avaliado por percursos de autoria, com captura antes/depois e dados de projeto. As verificações propostas incluem câmera com roll e pai rotacionado; perspectiva/ortográfica; dois previews e redução de frequência; Add com requisito já presente; desfazer remoção conjunta; recurso compartilhado usado por vários objetos; reimportação com origem renomeada; textura com alpha fino; mais desenhos que a capacidade de extensão; orientação/IME e retomada de superfície.

Desempenho deve distinguir primeiro frame, cena navegável, cena editável, processamento concluído e bytes residentes. A evolução da abertura não deve depender de reduzir qualidade silenciosamente. Testar o backend utilizado, o modo de desenho e o dispositivo suportado; um caminho direto aprovado não certifica automaticamente batching, sombras, picking e preview.

Nenhum desses roteiros foi executado neste atendimento. A documentação atual de R1–R4 continua sendo a evidência histórica disponível, com suas condições e limites.

## 11. Referências e rastreabilidade

As fichas ligam às fontes fixadas e às 117 páginas API consultadas. As referências de projeto abaixo complementam o inventário estrutural; nenhuma demonstra que uma função já existe na Astra.

| Referência | Aplicação neste plano |
|---|---|
| [UnityCsReference, revisão fixada](https://github.com/Unity-Technologies/UnityCsReference/tree/a2a4a31aee6dfb63c2ef36eea79d817a6e31349b) | Hierarquia de tipos, membros e atributos do núcleo |
| [uGUI, revisão fixada](https://github.com/Unity-Technologies/uGUI/tree/4349121947c0f924de5ec82110ebf6cd53f77fcb) | Componentes de UI/TMP e relações |
| [Graphics, revisão fixada](https://github.com/Unity-Technologies/Graphics/tree/feb4de2d9a93a4ae10d287d3a6d7003d08ea3e53) | Core/URP/HDRP/VFX/Shader Graph |
| [Unity Camera](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Camera.html) | Organização de controles e projeção |
| [Unity Cameras overlay](https://docs.unity3d.com/6000.0/Documentation/Manual/cameras-overlay.html) | Preview, seleção e pilotagem |
| [Unity Gizmos menu](https://docs.unity3d.com/6000.0/Documentation/Manual/GizmosMenu.html) | Representação editorial de componentes |
| [Unity RequireComponent](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/RequireComponent.html) | Dependência de composição |
| [Unity primitives](https://docs.unity3d.com/6000.0/Documentation/Manual/PrimitiveObjects.html) | Criação de objetos e diferença visual/física |
| [Unity MeshRenderer](https://docs.unity3d.com/6000.0/Documentation/Manual/class-MeshRenderer.html) | Separação de recurso/material/renderer |
| [Unity Texture Import Settings](https://docs.unity3d.com/6000.0/Documentation/Manual/class-TextureImporter.html) | Encaminhamento para tipos/perfis de importação de textura |
| [Godot Advanced Import Settings](https://docs.godotengine.org/en/stable/tutorials/assets_pipeline/importing_3d_scenes/advanced_import_settings.html) | Configuração por item e preview de importação |
| [Godot Camera3D](https://docs.godotengine.org/en/stable/classes/class_camera3d.html) | Câmera associada a viewport e propriedades |
| [Unreal Static Mesh Editor](https://dev.epicgames.com/documentation/unreal-engine/static-mesh-editor-ui-in-unreal-engine) | Bancada especializada de malha |
| [Unreal sockets](https://dev.epicgames.com/documentation/unreal-engine/using-sockets-with-static-meshes-in-unreal-engine) | Pontos de conexão e edição contextual |

Para continuar a implementação, usar este relatório como atualização do estado e expansão dos contratos, preservando a missão, IDs, transações, importação estrutural, código mobile e autoria reutilizável do plano mestre. As limitações de cobertura do atlas devem permanecer visíveis até serem fechadas por pesquisa e implementação efetivas.


## Adendo — aba Jogo e recarga durante execução (17/09/2026)

Solicitação do usuário registrada em [Aba Jogo do viewport e recarga](ABA-JOGO-RECARGA-2026-09-17.md). A mini prévia atual é sob demanda; não equivale a execução contínua ou hot reload. O adendo define a futura aba Cena/Jogo, mundo Play único, RenderView independente, input/foco, atualização segura de propriedades/assets/código, versões, rollback e limites de preservação de estado. É arquitetura futura, não implementação nem ampliação automática do percentual; a sequência P01/P02 permanece ativa.

# P01–P04 — primeira implementação de propriedades, composição e câmera

Data: 15/09/2026. Base: `codex/gameplay-runtime`, após `f3801ba3`.

Este registro acompanha o [plano de universalidade](PLANO-UNIVERSALIDADE-COMPONENTES-LAYOUT-2026-09-15.md), especialmente §9.1, e preserva os marcos do [plano mestre](PLANO-MESTRE-ASTRA-EDITOR-RUNTIME-ASSETS.md). O pacote iniciou P01/P02/P03 e a apresentação de campos de P04. **Os quatro pacotes completos continuam abertos.** A existência de uma propriedade na Unity ou no atlas não foi convertida em capacidade presumida da Astra.

## Estado da entrega

**Atualização após autorização `continue adb on`:** estado consolidado com a continuação da seção 6. Há agora build Android, instalação, testes focados e evidências de autoria no aparelho. Os limites de cada percurso continuam explícitos; isso não encerra P01–P04.

| Frente | Implementação neste pacote | Estado |
|---|---|---|
| Propriedades | Metadados comuns de grupo, unidade, ajuda, visibilidade e editabilidade nos quatro tipos reflexivos existentes | Implementado em código; ajuda é contrato reservado, sem painel de ajuda nesta entrega |
| Inspetor | Abas derivadas do descritor; grupos condicionais; valores com unidade separada; campos bloqueados não recebem região de edição | Câmera avaliada em paisagem; demais famílias sem prova visual neste percurso |
| Composição | Resolução transitiva de requisitos; reuso dos existentes; conflitos nas duas direções; limite e ciclo; candidato atômico | Implementado nos caminhos do editor e da API de runtime |
| Remoção | Proteção da última instância necessária; referências tipadas entre objetos; nova conferência no ponto seguro do runtime | Requisitos e referência cruzada testados em host; fila do runtime ainda sem prova específica |
| Representação | Registro de provedores para câmera, luz e colisor; marcadores PNG e linhas derivadas das propriedades | Câmera capturada em ADB; luz/colisor ainda sem prova visual neste percurso |
| Câmera | Ver, Pilotar, retornar à órbita preservada, alinhar à vista com histórico, enquadrar objetos sem malha | Autoria e persistência avaliadas em ADB; cancelamento/roll cobertos em host |
| Orientação | Roll derivado da transformação mundial e propagado ao renderer de mapas, raios, projeção editorial e culling CPU | Implementado; caminhos temporais/compute têm fallback explícito |
| Shaders | Céu e grade recebem roll; headers SPIR-V correspondentes regenerados | Geração dos artefatos concluída; não certifica a cena ou o aplicativo |

A primeira entrega tinha somente geração dos SPIR-V de `dirt_road_sky` e `editor_grid`. A seção 6 registra a autorização posterior, as compilações, os testes e os percursos reais no aparelho; não estender essas evidências aos itens ainda pendentes.

## 1. Contrato comum de apresentação — P01

`native/scene/components.h` acrescenta `PropertyPresentation` ao final dos descritores `ComponentNumber`, `ComponentBoolean`, `ComponentEnum` e `ComponentObjectReference`. Os agregados existentes continuam válidos com valores padrão.

Os metadados pertencem ao descritor de vida estática. Não são campos adicionais de cena nem uma tabela paralela da UI. A identidade continua sendo `TypeId + instanceId + PropertyId`. Um campo oculto continua com valor persistido e pode ser configurado por código. Um campo não editável é recusado pelo setter comum, além de não receber interação na UI. `help` fica reservado no contrato; nenhum tooltip funcional foi anunciado.

O inspetor reúne os grupos visíveis na ordem dos descritores, conserva os controles sem grupo e apresenta somente os campos do grupo selecionado. A ordem das propriedades na serialização não foi alterada. A paginação existente permanece; não foi substituída por uma lista virtualizada neste pacote.

### Matriz de adoção dos campos existentes

| TypeId | PropertyIds | Apresentação/condição | Consumidor existente |
|---|---|---|---|
| `astra.camera` | `vertical_fov`, `near_plane`, `far_plane` | Lente; graus/m; valores continuam contínuos dentro do domínio declarado | Projeção do renderer, vista pela câmera e frustum editorial |
| `astra.camera` | `priority`, `enabled` | Saída; prioridade inteira; `enabled` identificado como uso no Play | Resolução da câmera ativa; maior prioridade, menor ID no empate |
| `astra.camera` v2 | `projection`, `orthographic_half_height` | Lente; FOV/meia altura condicionais; leitura v1 preservada | Vista/Play, zoom, volume paralelo, GPU, sombras e LOD; detalhes e limites no registro P03 |
| `astra.render.light` | `color.r`, `color.g`, `color.b`, `intensity` | Emissão; RGB linear e escala de intensidade da Astra, sem rotular como lúmens | Extração de luzes e iluminação já existentes |
| `astra.render.light` | `range` | Volume/m; apenas pontual e spot | Atenuação e representação de alcance |
| `astra.render.light` | `inner_angle`, `outer_angle` | Volume/graus; apenas spot; rótulo explicita meio-cone | Atenuação angular e cone editorial |
| `astra.render.light` | `kind`, `enabled` | Controles comuns em todos os grupos | Seleção da modalidade e ativação |
| `astra.physics.collider` | `half_x`, `half_y`, `half_z` | Forma; apenas caixa | Geometria de colisão e contorno |
| `astra.physics.collider` | `radius` | Forma; esfera/cápsula | Geometria de colisão e contorno |
| `astra.physics.collider` | `half_height` | Forma; apenas cápsula; trecho cilíndrico sem hemisférios | Geometria de colisão e contorno |
| `astra.physics.collider` | `center_x/y/z`, `rotation_x/y/z` | Pose; rotação em graus, posição em unidades de cena | Transformação local do volume e consumidor físico existente |
| `astra.physics.collider` | `owner` | Vínculo; corpo no próprio objeto ou ancestral compatível | Resolução de proprietário; referência explícita protege a remoção do corpo |
| `astra.physics.collider` | `shape`, `enabled` | Forma e controle comum, respectivamente | Seleção da forma/ativação; nenhum novo solver |
| `astra.physics.joint` | `anchor_a_x/y/z`, `anchor_b_x/y/z`, `connected_body` | Âncoras | Vínculo entre corpos e configuração física existente |
| `astra.physics.joint` | `axis_a_x/y/z`, `axis_b_x/y/z` | Movimento; apenas dobradiça/deslizante | Eixos da restrição existente |
| `astra.physics.joint` | `limit_min`, `limit_max` | Movimento; ocultos para ponto | Limites; continuam com unidade dependente da modalidade |
| `astra.physics.joint` | `motor_velocity`, `motor_position`, `motor_force`, `spring_frequency`, `spring_damping` | Motor; somente modalidade compatível com motor ligado | Motor existente; não adiciona novos modos físicos |
| `astra.physics.joint` | `motor`, `kind`, `enabled` | Motor condicionado à modalidade; tipo/ativação comuns | Seleção de modalidade preserva validações cruzadas existentes |

Os grupos/condições de colisor e junta saíram das verificações por índice no desenho do inspetor e passaram ao descritor. O comando específico Ajustar à malha continua ligado ao serviço de geometria existente; a UI o apresenta no grupo Forma. Isso não é reconhecimento semântico automático de qualquer malha.

Versões preservadas: câmera 1, luz 1, colisor 3, junta 1. As leituras antigas do colisor continuam usando a mesma ordem de campos. Nenhuma nova projeção, forma de colisão, unidade física ou propriedade de Unity foi acrescentada apenas como controle visual.

## 2. Composição e impacto — P02

`planComponentAddition` tem duas fases: resolver os tipos necessários e, quando solicitado, materializar a coleção candidata. O menu consulta a mesma resolução sem clonar payloads de scripts/recursos a cada frame.

Fluxo de Add:

1. Resolver o tipo registrado, cardinalidade e política de alteração em Play.
2. Percorrer requisitos transitivos, reutilizando instâncias existentes.
3. Detectar ciclo, capacidade excedida e conflitos declarados por qualquer lado.
4. Materializar uma cópia candidata, com dependências antes do componente solicitado.
5. Publicar pelo caminho de histórico do editor ou pelo caminho existente de componentes do runtime.

Exemplo concreto: Add → Olhar num objeto vazio prepara Câmera + Olhar. O menu informa `Inclui Câmera`. Se já houver câmera, seus valores e sua identidade são conservados. Add → Junta prepara Corpo físico + Junta; a referência ao outro corpo continua sendo autoria explícita e pode permanecer vazia como rascunho. O pacote não inventa o segundo objeto físico.

No editor, uma única alteração de valores entra no histórico. Se a resolução falha, nenhuma dependência intermediária é anexada ao documento. A nova coleção preserva IDs das instâncias anteriores. As políticas que proíbem alterações estruturais de física em Play continuam vigentes.

Remoção considera:

- Requisitos declarados no mesmo objeto: Câmera necessária para Olhar, Corpo necessário para Junta.
- Última instância do tipo: uma instância redundante pode ser removida quando outra satisfaz a exigência.
- Referências explícitas com tipo exigido: por exemplo, `Joint.connected_body` e `Collider.owner` apontando a outro objeto.
- Nova resolução na aplicação da fila do runtime, pois um callback pode anexar um dependente entre a solicitação e o ponto seguro.

Referências nulas preservam a semântica de rascunho. Um colisor isolado não passa a exigir corpo físico por causa da regra de remoção. Referências gerais ao objeto não são confundidas com dependências de um tipo de componente.

Limites: ainda não há grafo persistente/indexado de dependências; a consulta reversa percorre o grafo quando necessária. Não há remoção conjunta de dependentes, reparação automática de cenas antigas inválidas, impacto completo de excluir/reparentear objetos, nem relações condicionais com recursos, serviços ou backend. A remoção enfileirada que deixou de ser válida é preservada/descartada no ponto seguro; ainda falta expor esse resultado posterior como diagnóstico individual para o chamador.

## 3. Presença editorial e câmera — P03

`native/editor/editor_component_visuals.h` declara um registro de provedores. Cada provedor recebe componente real, transformação mundial, proporção da vista e estado de seleção. Produz somente dados editoriais transitórios.

| Provedor | Marcador | Detalhe selecionado |
|---|---|---|
| Câmera | PNG de câmera do atlas existente, selecionável | Planos near/far, arestas do frustum e orientação |
| Luz | PNG de luz do atlas existente, selecionável | Esfera de alcance, cones interno/externo ou direção, conforme modalidade |
| Colisor | Sem marcador permanente | Volume de caixa/esfera/cápsula, centro, rotação local e transformação da hierarquia |

Câmera e luz no mesmo objeto recebem marcadores separados. A seleção é do objeto, conservando a composição universal; o desenho não depende de um nome especial de entidade. Marcadores/linhas não entram em mesh assets, sombras, colisão, salvamento ou exportação. São overlays, portanto **não têm oclusão por profundidade** neste pacote. Contornos de junta permanecem no provedor especializado anterior. Estado desativado do componente usa cor atenuada; visibilidade da hierarquia é respeitada.

O botão editorial no canto do viewport controla a exibição global dessas representações. Colisores do objeto selecionado não dependem de manter seu card aberto. Enquadrar seleção considera também origens de objetos sem malha; não usa o plano far da câmera como tamanho do objeto.

### Câmera: percurso aplicado

1. Anexar Câmera a um objeto e editar a Transformação existente.
2. Selecionar pelo marcador ou pela hierarquia; abrir o componente.
3. Usar Lente para FOV/near/far e Saída para prioridade/uso em Play.
4. `Ver` usa a câmera no viewport principal com sua projeção e pose. Pode inspecionar uma câmera desativada para Play sem ativá-la.
5. Durante essa inspeção, toque seleciona malhas normalmente; arrastes não alteram silenciosamente a câmera autoral nem a órbita preservada.
6. Sair pelo controle da vista, ou enquadrar seleção, restaura a navegação editorial. Abrir outra cena limpa a câmera de inspeção.
7. `Alinhar` aplica à câmera a pose mundial da vista usando o histórico. A conversão considera o pai; transformações que exigiriam shear não representável são recusadas.

O roll é derivado da base óptica ortonormal da transformação mundial. Escala não altera FOV; transformações degeneradas não fornecem uma pose válida. O mesmo roll chega à projeção e aos raios do editor, ao UBO de transformação mundo/vista do renderer de mapas, ao céu, à grade e ao culling/HZB CPU.

O kernel de culling GPU e a reprojeção temporal ainda possuem ABI baseada em yaw/pitch. Para câmeras com roll, a implementação usa o caminho conservador de desenho/visibilidade e resolve espacial, sem jitter/histórico temporal incompatível. Uma pirâmide GPU de pose anterior com roll também não é reutilizada. Isso protege correção, mas pode mudar custo e qualidade de antialiasing. Não declarar roll com TAA/GPU HZB completo. Caminhos antigos de demonstração instanciada fora do renderer de mapas não receberam extensão de roll.

No push constant da grade, roll ocupa `baseColorFactor.z`, preservando `xy` para jitter, `w` para altura do plano e `materialFactors.xyz` para a cor da grade. No céu, ocupa `materialFactors.x`. Os headers SPIR-V foram regenerados para acompanhar essas fontes.

## 4. Construção visual iniciada — P04

As abas usam superfície externa, recorte arredondado, aba ativa elevada e indicador inferior com a cor de identidade do tema. Os campos numéricos usam cavidade visual e unidade separada. Ações de câmera usam PNG do atlas e rótulos curtos. A adição mantém os componentes fechados e mostra dependências no subtítulo, sem criar uma sequência de popups.

Este pacote não redesenhou a casca inteira nem introduziu os modos Estúdio/Foco. Não gerou um novo conjunto de ícones. Usa os PNG existentes, sem SVG novo. A primeira implementação não tinha captura no aparelho; a continuação da seção 6 acrescenta capturas em paisagem. Retrato permanece sem avaliação deste pacote.

## 5. Próxima continuação concreta

Atualização de 17/09: [integração ortográfica](P03-PROJECAO-ORTOGRAFICA-2026-09-16.md) inclui Camera v2 com leitura v1, inspetor, lente condicional, zoom com histórico, volume paralelo, ponte Android/Play, projeção GPU, sombras e LOD. O mesmo registro documenta alças e preview independente básico com imagem real conferida por ADB. TAA temporal, comprovação de dispatch HZB e fidelidade/lifecycle completo do preview permanecem pendentes. A primeira linha abaixo não equivale à conclusão de todos os recursos de câmera.

| Ordem | Trabalho | Critério para não encerrar superficialmente |
|---|---|---|
| 1 | Completar `RenderView` perspectiva/ortográfica e câmera editorial | Projeção, clipping, rays, seleção, culling, UBO, céu/grade e formato devem consumir a modalidade; não só um enum |
| 2 | Alças de lente/clip; ampliar cobertura da pilotagem já implementada | Alças implementadas no registro P03; ainda conferir multitouch, perspectiva e hierarquias complexas no aparelho |
| 3 | Completar preview independente de câmera | PiP com target real, constantes/culling próprios e atualização limitada implementado e conferido por ADB; faltam fidelidade, configuração do orçamento, lifecycle físico e custo medido |
| 4 | Inspector com tipos ricos | Cor/HDR, vetores, recurso, lista/estrutura, máscara e curva, com serialização e consumidores reais |
| 5 | Grafo e painel de impacto | Dependências diretas/reversas, recursos/ancestral/serviço, navegação para usuários, exclusão e reparo transacional |
| 6 | Casca P04 e bancada P05 | Layout persistente, navegação objeto–componente–recurso, áreas redimensionáveis e design coerente com o IDE |
| 7 | Fechar rastreabilidade por propriedade | Atualizar matriz Astra × referência com evidência de consumidor; catálogo Unity continua inventário, não checklist automaticamente concluído |

Quando a execução for autorizada, priorizar percursos completos: Add Olhar + desfazer/refazer; reutilizar câmera configurada; bloquear corpo referenciado por junta em outro objeto; inspecionar câmera com roll sob pai rotacionado; comparar objeto/grade/seleção; sair da vista sem perder órbita; alternar grupos do colisor/luz/junta e reabrir a cena. A validação de tela e a de performance permanecem separadas da geração de shaders.

## 6. Continuação autorizada — pilotagem, histórico e aparelho

### Implementação adicional

- Câmera oferece `Ver`, `Pilotar` e `Alinhar`. Ver permanece somente inspeção; Pilotar edita a transformação do objeto no modo Edit. Não altera o componente Olhar nem acrescenta controlador ao runtime.
- Pilotar captura entidade e ID da instância de câmera. Cada gesto completo produz uma única transação `Pilotar câmera`. O histórico funde os movimentos preservando a pose anterior ao primeiro movimento.
- Um dedo usa a ferramenta de navegação selecionada: girar, deslocar no plano da vista ou avançar/recuar. Dois dedos combinam deslocamento do ponto médio e dolly pela razão das distâncias. Velocidade usa a distância de navegação editorial e a projeção, ainda sem controle próprio de velocidade de pilotagem.
- A orientação usa yaw/pitch com roll conservado; pitch é limitado a aproximadamente ±89 graus durante a pilotagem. A conversão para transformação local considera o pai e recusa pose que exigiria shear. A escala local do objeto é preservada.
- Cancelar o gesto ou perder a captura restaura a pose. `EditorHistory::cancel` aplica o inverso em uma cópia candidata e publica somente se todos os comandos puderem ser revertidos; não invalida a pilha de refazer. Falha de cancelamento conserva a operação como desfazível e informa o estado.
- Trocar para outra ação conclui a transação ativa; sair da câmera restaura a órbita editorial. Abrir outra cena limpa também o modo de pilotagem. O rótulo da vista foi reposicionado após evidência ADB de sobreposição com os controles de enquadramento; usa uma área própria, recorte e superfície arredondada.
- Correção de enquadramento: a origem de uma pasta vazia não amplia mais os limites de seus filhos. Objetos lógicos relevantes entram pelos seus pontos de origem; uma seleção inteiramente vazia recebe enquadramento de fallback.
- Corrigidas duas concatenações inválidas de `char[64]` nas mensagens de referência/dependência e uma ocorrência de indentação rejeitada pelo compilador de host.

### Evidência de execução, 15/09/2026

Dispositivo Android `25053PC47G`, `onyx`, 2772×1280; pacote `dev.aether.editor`. Projeto separado `P01Camera0915i`, criado pela interface. Nenhum projeto de trabalho anterior foi usado para editar a cena de prova.

| Verificação | Resultado e alcance |
|---|---|
| Android `:app:assembleDebug` | Build concluído e APK instalado por `adb install -r`. Aviso existente sobre versão XML do SDK não impediu a compilação |
| Host `aether_tests session_` | 51/51; inclui navegação, criação, transformação, hierarquia, componentes e enquadramento. A regressão de origem do grupo foi corrigida antes da aprovação |
| Host `aether_tests p01_` | 2/2; reuso de câmera preserva ID/FOV; composição inteira desfazível; corpo em outro objeto protegido por referência tipada |
| Host `aether_tests p03_` | 1/1; inspeção, pilotagem, roll, desfazer, cancelamento e preservação da órbita e da pilha de refazer |
| Add Olhar no aparelho | Mostra `Inclui Câmera`; adiciona os dois componentes fechados; um desfazer remove ambos e refazer os restaura |
| Persistência dos componentes | Cena salva e reaberta após reinstalação do APK, com Câmera e Olhar preservados |
| Inspeção no aparelho | Arquivos `camera-before.aescene` e `camera-inspected.aescene` idênticos após arraste e salvamento em Ver |
| Pilotagem no aparelho | Rotação X/Y mudou de `25.7831001 / 34.3774681` para `32.1440392 / 4.16324902`, mantendo posição e payloads dos componentes |
| Desfazer pilotagem no aparelho | `camera-undo.aescene` idêntico ao arquivo anterior à pilotagem; SHA-256 `78AD51504502680884D0B9B209857BBB067BFDD3F9BB816E20CA98E7D37B461F` |
| Deslocar e dolly no aparelho | Navegação com um dedo altera posição; rotação e linha serializada do Cubo permanecem iguais entre `camera-pan.aescene` e `camera-dolly.aescene`. Cubo e grade continuam visíveis após recuar |
| Layout final da câmera | Nova instalação e reabertura; rótulo `Pilotando · Objeto vazio` legível fora dos controles de enquadramento, registrado em `camera-final.png` e `camera-dolly.png` |

Captura final da interface: [câmera pilotável com geometria real](../validacao/evidencias/p01-p04-20260915/camera-final.png); [recuo com um dedo](../validacao/evidencias/p01-p04-20260915/camera-dolly.png).

### Erro Vulkan encontrado pela validação

A consulta do processo com a cena contendo Cubo encontrou `VUID-vkCmdDrawIndexed-renderPass-02684`: os passes de limpeza e preservação do atlas de sombras usavam o mesmo pipeline, mas tinham dependências de entrada diferentes (`TOP_OF_PIPE/0` versus `FRAGMENT_SHADER/SHADER_READ`). O log original está em `runtime-errors.txt`.

Corrigido em `InstancedRenderer::createShadowResources`: ambos declaram a dependência conservadora da leitura anterior do atlas, mantendo as diferenças legítimas de loadOp/layout. A [regra de compatibilidade da especificação Vulkan](https://docs.vulkan.org/spec/latest/chapters/renderpass.html#renderpass-compatibility) permite essas diferenças de attachment, mas não a diferença das dependências encontrada. Não foi desativada a camada de validação nem ocultado o diagnóstico.

APK recompilado e reinstalado depois da correção. Nova abertura do mesmo projeto com Cubo, entrada em Pilotar e arraste: processo `26930`, 411 linhas disponíveis no log, zero correspondências para `Fatal signal`, `FATAL EXCEPTION`, `VK_ERROR` e `VUID-`. [Log posterior](../validacao/evidencias/p01-p04-20260915/runtime-final.txt), [captura posterior](../validacao/evidencias/p01-p04-20260915/camera-release.png) e [SHA-256 do APK](../validacao/evidencias/p01-p04-20260915/apk-sha256.txt). Essa janela é validação funcional curta, não benchmark ou soak.

Capturas e cenas: [diretório de evidências](../validacao/evidencias/p01-p04-20260915/). Pontos principais: [Add](../validacao/evidencias/p01-p04-20260915/add-menu.png), [dois componentes](../validacao/evidencias/p01-p04-20260915/look-added.png), [desfazer](../validacao/evidencias/p01-p04-20260915/look-undo.png), [controles](../validacao/evidencias/p01-p04-20260915/camera-controls.png). A captura `camera-view.png` registra a sobreposição encontrada durante a validação, antes da correção do rótulo; não é referência final de layout.

**Não certificados neste percurso:** performance/soak, multitouch físico, retrato, roll sob pai rotacionado no aparelho, todas as modalidades de luz/colisor/junta, remoção no ponto seguro do runtime, projeção ortográfica, preview independente e paridade Unity. Roll/cancelamento e referências tipadas têm evidência de host delimitada acima. Cenas de câmera são dados reais da autoria, não prova de toda a renderização.


## Continuação P01 — propriedades compostas, 17/09

O descritor comum agora declara `ComponentTriple`: identidade persistente do conjunto, nome, três IDs de canais e semântica Vector/LinearColor. Não há armazenamento duplicado. A serialização antiga permanece escalar, na mesma ordem e versão; leitores existentes continuam compatíveis.

Adoção real: luz `color` (RGB linear); colisor `half_extents`, `center`, `rotation`; junta `anchor_a`, `anchor_b`, `axis_a`, `axis_b`. O inspetor monta uma linha com três células identificadas R/G/B ou X/Y/Z, a partir do mesmo descritor, e só agrupa quando os três canais estão visíveis no grupo atual. Cada célula encaminha para a edição numérica existente, preservando ID da instância, validação e histórico por canal. Componentes sem descritor composto mantêm os campos anteriores.

A API nativa `setComponentTriple` resolve canais por ID, clona uma candidata, escreve todos os valores e valida o componente completo antes de substituir a instância. Recusa componentes ausentes, identidades ambíguas, canais desconhecidos, limites inválidos, valores não finitos e invariantes cruzadas. Isto permite alterar um vetor de direção em uma operação sem publicar estados intermediários inválidos. Não há ainda ligação dessa operação composta à API C# ou um editor modal de três valores; a UI desta entrega continua editando um canal por transação.

Consumidores são os existentes: iluminação, pose/forma do colisor e configuração física das juntas. Não foram presumidos recursos da Unity. Luz mantém RGB linear 0–1 e intensidade separada; não foi anunciado suporte novo a HDR, conversão sRGB ou seletor visual. Recursos, máscaras, curvas, listas, ajuda e unidades dos grupos compostos ainda exigem expansão.

Validação: alvo host `aether_tests` compilado; filtro P01 **3/3**. Novo caso cobre direção válida, rejeição atômica de eixo nulo, ambiguidade entre instâncias, rejeição parcial de cor e leitura da serialização existente. APK Android Debug compilado e instalado. O layout novo e a edição agrupada ainda não foram conferidos visualmente no aparelho; instalação não equivale a essa prova.

Percentual atualizado: **P01 ≈45% (antes 40%); P03 ≈70%; plano geral ≈30% (faixa 25–35%)**. O avanço de cinco pontos em um de 18 pacotes não altera o arredondamento global. Próximas entregas: edição composta com histórico único, unidades/apresentação adaptativa e cor visual, depois referências de recursos e painel de impacto P02.


## Continuação P01 — edição conjunta e amostra de cor, 17/09

Tocar no nome do grupo RGB/XYZ abre a edição dos três canais. Células individuais preservam o caminho escalar. O editor conjunto aceita três números separados por espaços ou ponto e vírgula, inclusive vírgula decimal; teclado interno oferece Separar canais. A ponte Android solicita teclado textual para permitir os separadores. Os dados capturam entidade, instância, propriedade e versão da cena; confirmar aplica `setComponentTriple` em uma candidata e uma única operação de histórico. Cancelar, valores inválidos ou revisão alterada não publicam uma mudança parcial.

A cor da luz apresenta uma amostra calculada por conversão linear→sRGB. Os valores autorados e usados na iluminação permanecem lineares. Trata-se de amostra visual com edição numérica, não um seletor HSV, conta-gotas, HDR ou biblioteca de cores.

Validação: alvo host compilado; P01 **4/4**, incluindo abertura do grupo pela UI, entrada localizada, rejeição atômica, um undo para três canais e cancelamento. Android Debug compilado e instalado. Teclado/layout e a amostra ainda não receberam conferência visual no aparelho nesta rodada.

**Percentual atualizado: P01 ≈45%; P03 ≈70%; plano geral ≈30% (25–35%).** Mantidos os valores arredondados: esta entrega completa a edição dos grupos introduzidos na rodada anterior, mas tipos ricos restantes, seletor de cor completo, recursos e painel de impacto continuam abertos. A operação composta ainda não foi exposta à API C#.


## Continuação — contrato futuro de execução e ajustes P01, 17/09

Adendo [Aba Jogo e recarga](ABA-JOGO-RECARGA-2026-09-17.md) ligado ao plano mestre e ao plano de universalidade: distingue mini prévia sob demanda, renderização contínua e recarga de propriedades/assets/código. Define ownership de mundo/vista/input, gerações, publicação segura, rollback, preservação de estado por capacidade e aceitação. **Planejado, não implementado**, sem mudar a prioridade P01/P02.

Implementação P01 desta rodada: título Android específico para três canais, limite de texto coerente com buffer nativo, conferência do tipo da edição antes de aplicar resposta e unidade exibida no título de grupos compostos quando comum aos três canais. Sem alterar valores ou unidades persistidas. Host compilado, P01 4/4; APK Debug compilado. Este APK não foi reinstalado nem conferido visualmente nesta rodada.

**Percentual atualizado: geral ≈30% (25–35%), P01 ≈45%, P03 ≈70%.** Mantidos: arquitetura futura e ajustes de apresentação não encerram novos pacotes. Permanecem pendentes seletor de cor completo, demais tipos ricos, referências de recursos e painel de impacto.


## Continuação P01 — seletor visual de cor, 17/09

A amostra de cor abre um painel nativo com 24 escolhas de matiz e grade de 11×11 de saturação/brilho, amostra do rascunho e Aplicar/Cancelar. O painel usa o descritor LinearColor, sem comparação por nome de componente. O nome do grupo mantém a edição numérica conjunta e as células RGB mantêm edição escalar.

O rascunho visual usa HSV sobre RGB sRGB; Aplicar converte para RGB linear, valida pelo setter composto e publica uma única operação de histórico. Escolhas intermediárias não modificam documento nem luz. Revisão alterada recusa aplicação; troca de cena limpa o painel. Limites atuais: seleção discreta por células, sem arraste contínuo, HEX, HDR, conta-gotas ou biblioteca de cores. A precisão arbitrária permanece nos campos numéricos. A iluminação não é atualizada ao vivo enquanto se escolhe o rascunho.

Host compilado, P01 **5/5**. Novo percurso percorre widgets reais: abrir amostra, selecionar verde, confirmar sem alterar antes de Aplicar, um undo e cancelamento. O primeiro teste encontrou o painel num ramo incorreto do layout; corrigido antes da aprovação. Conferência visual física ainda pendente.

**Percentual atualizado: geral ≈30% (25–35%); P01 ≈45%; P03 ≈70%.** Mantido o arredondamento: há seletor básico real, mas faltam acabamento/interação contínua, demais tipos ricos e painel de impacto. Arquitetura da aba Jogo/hot reload segue futura, sem mudança nesta entrega.


## Continuação P01 — percurso físico de cor, 17/09

ADB no projeto P01Camera0915i: anexada Luz temporariamente ao objeto da câmera, aberto o grupo Emissão e a amostra de cor; escolhidos matiz verde e saturação/brilho máximos. Aplicar publicou RGB linear 0/1/0 no inspetor; um desfazer restaurou 1/1/1. Reaberto o seletor, alterado o rascunho e cancelado; o próximo desfazer removeu a adição temporária da luz, confirmando que Cancelar não acrescentou alteração de cor ao histórico. Cena salva ao final com Câmera/Olhar originais e sem a luz temporária.

Evidências em `docs/validacao/evidencias/p01-p04-20260915/`: `color-applied.png`, `color-undo.png`, `color-scene-restored.png`. O percurso comprova autoria pelo seletor e histórico. Não certifica a contribuição fotométrica da luz nos objetos: neste enquadramento não foi estabelecida uma comparação controlada de iluminação.

Melhoria decorrente: contorno claro/escuro identifica matiz e célula de saturação/brilho selecionados. APK final compilado e instalado; a melhoria dos contornos ainda não recebeu captura física após reinstalação. Testes host anteriores P01 5/5 não foram repetidos para essa mudança apenas visual. Permanecem seleção discreta, ausência de HDR/conta-gotas/HEX e falta de conferência em retrato.

**Percentual atualizado: geral ≈30% (25–35%), P01 ≈45%, P03 ≈70%.** Mantida a cobertura de implementação; esta rodada ampliou evidência física e legibilidade. Próximo bloco: referências de recursos e painel de impacto P02, sem confundir a aba Jogo futura com a mini prévia atual.


## Continuação P02 — painel de relações, 17/09

Menu do componente → Dependências abre painel no inspetor, com paginação. Enumera requisitos no mesmo objeto, referências de saída (incluindo vazias/inválidas) e componentes que exigem ou referenciam o tipo selecionado. Tocar numa relação válida seleciona o objeto e expande a instância relacionada quando conhecida. Os dados vêm dos schemas/referências existentes e são recalculados do documento; não há cópia persistida do grafo.

Uma relação de tipo não significa que remover aquela instância esteja bloqueado quando existem outras instâncias equivalentes. O bloqueio de remoção permanece no resolvedor anterior; o painel é de consulta/navegação, não executa exclusão em cascata. Referências implícitas, assets, serviços, reparo transacional e grafo indexado/transitivo ainda estão fora deste bloco. Não presumir completude por exibir uma lista.

Host compilado; P02 1/1 (abrir menu/painel e navegar do requisito da câmera para Olhar), regressões P01 5/5. Android Debug compilado. Instalação não concluída: o transporte ADB desconectou antes da instalação. Conferência visual do novo painel no aparelho ainda pendente; referências cruzadas extensas e paginação não foram exercitadas fisicamente nesta rodada.

**Percentual atualizado: P02 ≈50% (antes 45%), P01 ≈45%, P03 ≈70%; geral ≈30% (25–35%).** A média orientativa dos 18 pacotes passa a 26,4%; arredondamento global mantido. Avanço atribuído ao percurso real de consulta/navegação, sem marcar como concluído o grafo universal.


## Continuação P02 — navegação no aparelho e referências tipadas, 17/09

ADB reconectado; instalado APK do painel e conferido no projeto P01Camera0915i o percurso menu da Câmera → Dependências → Olhar. A dependência aparece e tocar abre a instância Olhar. Capturas `impact-camera.png` e `impact-navigate.png` em `docs/validacao/evidencias/p01-p04-20260915/`. Percurso somente de consulta, sem alteração autoral.

Implementação adicional: requisitos usam nomes do catálogo; referências de saída incluem o tipo requerido; referência válida com uma única instância compatível navega diretamente ao componente de destino. Mais de uma instância não escolhe arbitrariamente: navega apenas ao objeto e informa multiplicidade. Referências recebidas distinguem validade de escopo; IDs acima do domínio de ObjectId não são truncados para outro objeto. Painel ganhou identificação do componente consultado e cartões com fundo/recorte arredondado.

Host compilado, P02 **2/2**, incluindo junta→corpo em outro objeto e relação reversa. Android Debug recompilado e instalado. As capturas físicas pertencem ao painel anterior aos ajustes de cartões/rótulos; essas mudanças finais e o percurso cruzado têm evidência de build/host, ainda sem conferência visual própria. Não declarar grafo universal fechado: assets, serviços, requisitos condicionais, índice persistente e reparo continuam pendentes.

**Percentual atualizado: geral ≈30% (25–35%), P01 ≈45%, P02 ≈50%, P03 ≈70%.** Mantidos os valores arredondados; houve ampliação da precisão e da evidência de P02, sem encerramento de outro pacote.


## Continuação P02 — impacto sobre remoção, 17/09

O painel distingue relações informativas, referências inválidas e relações que bloqueiam remover a instância consultada. Requisitos/referências tipadas recebidas só bloqueiam quando a instância é a última daquele tipo; referências inválidas não são apresentadas como bloqueios válidos. Referências opcionais vazias mantêm a semântica existente (por exemplo, proprietário implícito do colisor).

Resumo de bloqueio no cabeçalho usa diretamente `componentInstanceRemovalBlockedBy` e `componentRemovalReferenceUse`, os mesmos resolvedores do menu de remoção. A frase Sem bloqueio de dependências não promete ausência de outras restrições operacionais, como Play ou transação aberta. As linhas bloqueantes/ inválidas recebem destaque; não foi implementada exclusão em cascata nem reparo automático.

Host compilado; P02 **3/3**, cobrindo câmera requerida por Olhar, referência junta→corpo e colisor com proprietário vazio válido. Android Debug compilado e instalado. Indicadores novos ainda sem conferência visual física; evidência ADB da navegação anterior não é reapresentada como prova dos novos rótulos.

**Percentual atualizado: geral ≈30% (25–35%); P01 ≈45%; P02 ≈50%; P03 ≈70%.** Mantidos: melhora a precisão da consulta existente, sem fechar recursos, requisitos condicionais, grafo indexado ou reparo. Próximo incremento relevante é ampliar a consulta para recursos e seus consumidores reais, preservando a distinção entre dependência de tipo, referência de objeto e uso de recurso.

## Continuação P02 — recursos por slot e usos compartilhados, 17/09

O painel Dependências agora enumera GUIDs persistentes de malhas, materiais e texturas explicitamente atribuídos aos slots de MeshRenderer, incluindo oclusão. A consulta encontra outros componentes da cena com o mesmo recurso e tipo de vínculo; tocar no consumidor seleciona o objeto e abre a instância correspondente. As linhas recebem recorte para impedir que identificadores longos invadam outros controles. Índices numéricos locais de pacote não são interpretados como referências persistentes.

Limites: o provedor cobre MeshRenderer, não todos os componentes. Texturas herdadas de materiais e o marcador de ausência de textura não viram usos explícitos. Não há consulta de existência ao AssetRegistry, resolução transitiva, índice de consumidores, medição de desempenho ou bloqueio de exclusão de assets. As relações representam atribuições autorais, não comprovam os bindings efetivos da GPU. Linhas diretas de GUID são informativas; a navegação implementada é para componentes consumidores.

Host e Android Debug compilados; P02 **4/4**, incluindo slots adicionais, exclusão de marcadores e atualização dos consumidores após trocar um material. APK instalado com sucesso por ADB. Não houve conferência visual física do novo percurso de recursos nesta rodada; instalação não é validação visual.

**Percentual atualizado: geral ≈30% (25–35%); P01 ≈45%; P02 ≈50%; P03 ≈70%.** Mantidos os arredondamentos: recursos explícitos ampliam P02, mas resolução pelo registro, herança, demais provedores e reparo ainda impedem fechar o pacote. Próximo bloco: conectar a consulta ao registro de assets e resolver materiais/texturas herdados com diagnóstico de recursos ausentes.

## Continuação P02 — registro e herança de materiais, 17/09

Consulta de recursos integrada ao AssetRegistry da sessão e à biblioteca EditorMapScene, tanto no desenho do painel quanto na navegação por toque. Mostra caminho cadastrado em vez de apenas GUID, diagnostica tipo incompatível e identidade não registrada; malha presente no pacote sem registro é identificada como recurso do pacote, sem falso erro de ausência. Material registrado mas indisponível na biblioteca aparece como não carregado. Registro existente não comprova existência do arquivo nem residência GPU.

Texturas locais e herdadas usam os mesmos resolvedores slotTexture/slotOcclusionTexture da extração de materiais. Overrides locais vencem e MaterialTextureNone interrompe a herança. Os bindings receberam nomes Cor base, Normal, Metal / rugosidade, Emissão e Oclusão. A busca de consumidores usa essa mesma resolução; uma textura local de um objeto pode ser relacionada à textura herdada de outro, e a relação desaparece quando removida por override.

Limites: provedor ainda restrito a MeshRenderer; texturas da fonte do pacote sem GUID explícito/compartilhado, dependências transitivas do registro, existência física dos arquivos, reparo, navegação ao documento do asset e índice de consumidores continuam pendentes. A consulta não modifica arquivos ou cenas e não implementa exclusão em cascata. Identificadores/caminhos longos continuam sujeitos ao recorte do cartão, sem detalhe expandido próprio.

Validação: host compilado, P02 **5/5**; caso novo cobre herança, consumidor reverso, tipo errado, identidade não registrada, bloqueio de herança por none e material não carregado. Android Debug compilado e instalado por ADB. Não foi realizada conferência visual física desse novo fluxo.

**Percentual atualizado: geral ≈30% (25–35%); P01 ≈45%; P02 ≈50%; P03 ≈70%.** Mantidos os arredondamentos: o vínculo recurso/consumidor ficou mais preciso, mas P02 ainda não entrega requisitos condicionais, reparo e grafo completo. Próximo bloco: navegação e detalhes de recurso no inspetor, com retorno ao componente e diagnóstico acionável, antes de ampliar operações de reparo.

## Continuação P02 — navegação de recursos e relações do registro, 17/09

Linhas de recurso em Dependências agora abrem detalhes no próprio inspetor, inclusive quando o GUID não está registrado. O painel apresenta identidade, tipo registrado, caminho, fonte e versão do importador; materiais e malhas apresentam também seu estado na biblioteca quando consultável. Não confundir registro com arquivo existente ou residência GPU.

As relações Depende de / Usado por do AssetRegistry são navegáveis por GUID e separadas dos consumidores de componentes da cena. Uma pilha de navegação guarda recurso e página anteriores; Voltar retorna pelas relações até o componente original. O limite de 64 níveis impede crescimento ilimitado em percursos repetidos. Troca de cena e abertura de outro componente limpam o contexto. Abrir um consumidor da cena seleciona e expande sua instância, encerrando essa navegação de recursos.

Sem novo registro paralelo ou mutação autoral. Ainda faltam reparo/relink, existência física dos arquivos, preview de recurso, detalhes expansíveis para caminhos longos, provedores além de MeshRenderer e indexação do grafo. O painel é consulta navegável, não a Bancada P05 completa. A navegação de relações permite seguir cadeias manualmente; não é uma análise transitiva automática.

Validação: host e Android Debug compilados; P02 **7/7**, incluindo abrir GUID não registrado, retornar ao componente preservando vínculo e distinguir direção das arestas do registro. APK instalado por ADB. Não houve conferência visual física do novo painel nesta rodada.

**Percentual atualizado: geral ≈30% (25–35%); P01 ≈45%; P02 ≈50%; P03 ≈70%.** Mantidos os arredondamentos enquanto faltam requisitos condicionais, conflitos e reparo transacional. Próximo bloco: operações de reparo tipadas com prévia de impacto e histórico, aproveitando os seletores existentes, sem substituir referências silenciosamente.

## Continuação P02 — reparo local com prévia e histórico, 17/09

Detalhes do recurso → Reparar usos locais abre seleção tipada por registro e disponibilidade. O usuário escolhe candidato, examina a lista de slots/bindings afetados e aplica explicitamente; cancelar não altera o documento. O alcance é todos os usos explícitos daquele GUID no componente aberto, não todos os objetos do projeto. A troca mantém instância, demais slots, overrides de material, sampler, canais e transforms. Para malhas, o índice temporário é resolvido novamente pela identidade de destino.

O comando repairComponentResource recebe identidade da cena e revisão esperadas, recusa Play/transação aberta/cena alterada, revalida candidato e aplica uma única alteração pelo histórico. Undo/Redo cobre todos os slots do componente em um passo. Materiais candidatos precisam estar carregados, malhas precisam existir no pacote e texturas precisam estar carregadas no catálogo do projeto ao aplicar. Falha de publicação da textura após autoria é informada como publicação pendente; não se anuncia atomicidade GPU.

Texturas herdadas do material compartilhado não são convertidas silenciosamente em overrides; só vínculos locais entram nesse reparo. GUID usado em categorias incompatíveis é recusado pelo seletor. Não há reparo de arquivo de material compartilhado, substituição global, troca de fonte, geração de variante nem migração automática de topologia. O usuário pode trocar um recurso válido deliberadamente; não é restrito a IDs ausentes.

Validação: host e Android Debug compilados, P02 **8/8**. Percurso novo usa widgets reais para escolher malha, preparar e aplicar reparo de dois slots, verifica ausência de mutação na prévia, preservação de material/textura, índice resolvido, um Undo, Redo e rejeição de revisão obsoleta. Os caminhos material/textura foram compilados, mas não receberam exercício dedicado nesta rodada. APK instalado por ADB; conferência visual física do novo fluxo ainda pendente.

**Percentual atualizado: P02 ≈55% (antes 50%); geral ≈30% (25–35%); P01 ≈45%; P03 ≈70%.** O avanço corresponde ao primeiro percurso real de reparo com prévia/histórico, somado à navegação de recursos já entregue. A média orientativa dos 18 pacotes é 26,7%; nenhum pacote foi encerrado. Próximo bloco: ampliar o contrato de reparo para usos compartilhados com transação de recurso e impacto por consumidor, além de conferir fisicamente este fluxo antes de considerá-lo validado no aparelho.

## Continuação P02/P08 — publicação consistente de material compartilhado, 17/09

A investigação do próximo reparo revelou um pré-requisito: cinco rotas de edição compartilhada gravavam o material antes de atualizar o AssetRegistry e publicavam dependências vazias. Centralizadas em commitSharedMaterial as edições de texturas, superfície, canais, amostragem e números. Material e registro agora usam o journal existente de EditorImportTransaction, sem segundo registro paralelo. A biblioteca em memória só recebe o candidato depois do commit dos arquivos; falha solicita rollback e mantém backups quando recuperação falha.

O candidato exige revisão consecutiva, valida tipos das texturas, deduplica dependências, inclui oclusão, ignora herança/none e preserva dependências não relacionadas a texturas, parâmetros/versão do importador e derivados. Retirar o último binding de uma textura remove a aresta correspondente. Antes de gravar, o material do disco é comparado semanticamente com a versão carregada; uma alteração externa é recusada. O begin do journal também confere hash dos bytes lidos. Continua válido o contrato de um único escritor do projeto; não é controle geral de concorrência entre processos.

Validação: host compilado; filtro R4 **18/18**. Ampliado percurso compartilhado para confirmar dependências em memória/disco, recusar arquivo externamente alterado sem sobrescrever a biblioteca e remover a última dependência de textura. Android Debug compilado e instalado por ADB. Não houve injeção de falha de energia, conferência visual ou novo percurso físico nesta rodada.

Limites: criação de material ainda usa a rota anterior; não foi implementado Undo/Redo de arquivos de recurso nem o novo seletor de reparo compartilhado. Journal de recuperação não equivale a histórico autoral. Publicação GPU continua etapa posterior e pode informar falha depois da gravação; não há atomicidade entre disco e GPU. Próximo bloco continua sendo histórico de recurso compartilhado e prévia por consumidor, agora sobre a gravação consistente.

**Percentual atualizado: geral ≈30% (25–35%); P01 ≈45%; P02 ≈55%; P03 ≈70%.** Mantidos: este incremento corrige a fundação de recursos, mas não fecha o percurso compartilhado planejado nem amplia automaticamente P08.

## Bloco P02/P08 — histórico de recursos e reparo em três escopos, 17/09

### Contratos implementados

- Histórico único: EditorHistory aceita transações autônomas de recurso com replay atômico. Edições de material intercalam corretamente com comandos da cena; falha no replay mantém o cursor, em vez de consumir o passo. Transações de recurso não se misturam com comandos de cena dentro de uma transação aberta.
- As cinco rotas compartilhadas (textura, superfície, canais, amostragem e números) passam a registrar snapshots antes/depois. Undo/Redo grava novamente material e AssetRegistry pelo journal, incrementa revisão e republica a biblioteca. Alteração externa no material ou conteúdo divergente recusa replay sem sobrescrever nem perder histórico. É histórico da sessão, não persistido entre reinícios.
- Reparo compartilhado: recurso de material → textura do material → Reparar no material. Candidato tipado, revisão do material e epoch/revisão da cena, prévia de consumidores e aplicação explícita. Todos os bindings daquele material que usam a textura são substituídos, incluindo oclusão. Overrides dos objetos são preservados e aparecem separados dos consumidores afetados na prévia. Outros usos em cenas não abertas também recebem o material alterado, mas não são enumerados como se tivessem sido examinados.
- Reparo local da cena: seletor de alcance Componente/Cena aberta, prévia dos usos explícitos e preparação em cópia do documento/histórico. Um consumidor incompatível recusa o conjunto antes da publicação. Aplicação reúne todos os objetos/slots em um único Undo; herança de materiais não vira override automaticamente.
- Malhas do pacote sem registro próprio entram no seletor por GUID persistente carregado; nenhuma identidade é inventada. Destinos de textura passam pela decodificação antes do reparo. Um binding originalmente ausente pode ser restaurado por Undo sem fabricar uma aresta válida no registro.

### Evidência e limites

Host compilado; P02 **10/10**, R4 **18/18**, filtro history **29/29** (filtros sobrepostos, não somar como casos únicos). Testes cobrem rejeição integral de consumidores incompatíveis, reparo multiobjeto/slots, fallback de candidatos do pacote, três usos herdados e um override preservado, troca incluindo oclusão, gravação em disco, intercalamento de ação de cena e recurso, restauração de GUID ausente e conflito externo mantendo cursor. Android Debug compilado e instalado.

Permanecem: criação/exclusão de arquivos de material fora desse novo histórico, reparo multiasset numa transação única, enumeração de consumidores em cenas fechadas, busca textual no seletor, grafo indexado e requisitos condicionais. A prévia de consumidor é autoral e não certifica contribuição visual de um binding desativado por shader/canais. Falha de publicação GPU é reportada depois da autoria persistida; disco/GPU não são uma transação única. Recuperação após queda de energia ainda não recebeu injeção física nesta rodada.

### Reestimativa

**P02 ≈65% (55% → 65%); P08 ≈55% (45% → 55%).** O aumento corresponde aos percursos completos de reparo e histórico, sem declarar encerramento. P01 ≈45% e P03 ≈70% mantidos. Média orientativa dos 18 pacotes: **26,7% → 27,8%**; estimativa global arredondada permanece **≈30% (25–35%)**. O avanço concentrado em dois pacotes não equivale a dez pontos globais. Próxima fronteira: ampliar referências/provedores e requisitos condicionais, e completar apresentação/navegação dos inspetores sob P01/P04.

### Conferência ADB deste bloco

APK final no aparelho 25053PC47G, projeto P01Camera0915i. Percurso: Cubo → Malha → Dependências → recurso → Reparar usos locais → alcance Cena aberta → malha de esfera do pacote → prévia → Aplicar substituição. A geometria do Cubo mudou para esfera e a consulta passou a indicar os dois consumidores. Um Undo restaurou o cubo; cena restaurada salva. Evidências: repair-scene-preview.png, repair-scene-applied.png e repair-scene-undo.png em docs/validacao/evidencias/p01-p04-20260915. Esta conferência encontrou e motivou a inclusão de malhas carregadas sem registro próprio no seletor.

O caso físico teve um uso afetado; atomicidade com múltiplos objetos foi exercitada no host. Reparo de textura compartilhada, override por binding e conflito de arquivo possuem evidência de host/persistência, ainda sem percurso visual físico dedicado. Nenhuma equivalência de validação é presumida entre esses casos. Percentuais finais deste bloco: P02 ≈65%, P08 ≈55%, geral ≈30% (índice central 27,8%).

## Continuação — requisitos condicionais de referências

Implementado no contrato `ComponentObjectReference` um predicado `requiredForExecution`, independente da visibilidade e da validade autoral. O resolvedor comum distingue Ready, OptionalEmpty, RequiredEmpty, Incompatible e Inactive. Referências opcionais vazias continuam válidas; rascunhos incompletos continuam editáveis e serializáveis.

Primeiro consumidor real: Junta exige o corpo conectado quando `enabled=true`, conforme o comportamento existente em `ScenePhysics`. O runtime passa a consumir o resolvedor comum; o inspetor destaca campo obrigatório/incompatível/inativo, e o painel de impacto distingue requisito de execução ausente e destino inativo. A relação estrutural Junta → Corpo no mesmo objeto permanece obrigatória. Desativar uma junta não remove referências explícitas nem libera a remoção de seus destinos: essas referências autorais continuam protegidas.

Base: plano de universalidade §4.2/P02, sem presumir contratos de outra engine. Um colisor estático não recebeu requisito de corpo físico. O predicado é metadado de código, sem mudança no formato persistido ou migração de arquivos.

Validação: build host concluído; P02 11/11 e composição 6/6, incluindo simulação dos quatro tipos de junta. Cobertura nova: rascunho ativo incompleto, desativação, destino ativo/inativo e autorreferência incompatível. Não houve build Android ou nova conferência ADB nesta continuação; a instalação do bloco anterior não contém estas mudanças.

Limites: prontidão de referência não certifica prontidão física completa. Pelo menos um corpo dinâmico, geração de shape, residência e demais condições ainda têm verificadores próprios. Não foi concluído um sistema geral de requisitos condicionais entre tipos, nem a exposição de todas as condições do solver no painel.

Percentuais mantidos conservadoramente: P02 ≈65%, P08 ≈55%, geral ≈30% (índice central 27,8%; faixa 25–35%). Esta entrega fecha uma condição real dentro da estimativa atual, sem inflar o pacote.

## Continuação — condições físicas das juntas compartilhadas

`runtime/joint_requirements.h` concentra a auditoria autoral de juntas ativas: validade dos campos, corpo no objeto, destino compatível e ativo, pelo menos um corpo dinâmico e matrizes globais finitas. Os diagnósticos possuem código estável, objeto e instância. ScenePhysics consome o mesmo verificador; o painel de impacto apresenta condições adicionais sem duplicar as linhas de composição/referência. A condição de movimento permite navegar até o Corpo físico do objeto da junta. Junta desativada ou objeto inativo não executa essa auditoria.

Evidência: build host aprovado; P02 12/12; composição 6/6, incluindo simulação das quatro juntas. Caso novo cobre par estático/cinemático recusado, navegação ao corpo, destino dinâmico satisfazendo a condição e suspensão por desativação. Sem instalação Android ou conferência ADB nesta rodada.

Correção de precisão da documentação: o runtime atual exige Corpo físico proprietário para Colisor ativo, inclusive estático. O corpo pode ser Static: não existe exigência de corpo DINÂMICO para collider estático. A referência vazia de proprietário é um rascunho válido e significa o próprio objeto na execução, que precisa conter o corpo. A documentação anterior sobre não acrescentar requisito deve ser lida como ausência de NOVA restrição de composição, não como suporte já implementado a collider ativo sem corpo.

Limites: a auditoria de junta não certifica criação de shapes, ausência de shear, compatibilidade completa de hierarquia dos corpos nem aceitação pelo Jolt; esses caminhos continuam na inicialização física. Propriedades autorais não são alteradas automaticamente. Percentual mantido: P02 ≈65%, P08 ≈55%, geral ≈30% (índice 27,8%, faixa 25–35%).

## Continuação — corpos, formas e transformações físicas

Extraídas para `runtime/physics_requirements.h` as regras efetivamente usadas pelo runtime: proprietário próprio/ancestral explícito sem atravessar outro corpo, hierarquia de corpo independente, decomposição da transformação do corpo, composição da pose do colisor, rejeição de shear e escala uniforme para esfera/cápsula. ScenePhysics usa essas funções para construir os corpos; o painel de impacto consulta as mesmas funções, evitando uma segunda interpretação geométrica.

O painel de Corpo físico enumera formas ativas vinculadas, navega até suas instâncias e sinaliza ausência de formas, excesso de 256 formas, problemas de hierarquia e transformação. O painel de Colisor resolve o proprietário efetivo (inclusive proprietário vazio significando o próprio objeto), permite navegar ao corpo e aponta problemas geométricos. Componentes/objetos que não executam respeitam a desativação. Consultas não alteram autoria, IDs ou histórico.

Validação: host compilado; P02 13/13 e composição 6/6. Novo caso verifica ausência de formas, esfera sob escala não uniforme, caixa sem rotação válida, caixa rotacionada com shear, desativação e conservação de identidade. Casos existentes exercitam composição assimétrica, shapes filhos, rejeição de shear e quatro juntas no solver. Sem novo APK/ADB nesta continuação.

Limites: consulta do corpo percorre sua subárvore, sem índice incremental; diagnósticos de transformação podem repetir quando várias formas compartilham o corpo inválido. Não certifica disponibilidade de memória, aceitação pelo backend, nem todos os requisitos de Personagem. Corpo próprio continua necessário para collider ativo com proprietário vazio; rascunho sem corpo continua editável. Não foi criado requisito de corpo dinâmico para colisor estático.

Percentuais: P02 ≈65%, P08 ≈55%, geral ≈30% (índice central 27,8%, faixa 25–35%), mantidos até fechar mais percursos e conferir a apresentação no aparelho.

## Reestimativa após presets e condições de execução

P02 passa de aproximadamente 65% para **75%**. A base do avanço é o conjunto das condições reais de execução (referências, juntas, corpos e colisores) com o novo percurso persistente de presets: capturar, nomear, listar, prévia, aplicar, adicionar com requisitos, Undo da cena, renomear e excluir. Não se atribui esse mesmo avanço novamente a P01/P08.

O índice central dos 18 pacotes passa de **27,8% para 28,3%**; o arredondamento global permanece ≈30% (faixa 25–35%). Faltam no P02 grafo indexado, cobertura de todos os provedores futuros, presets compostos/seletivos e integração dos presets ao grafo de recursos. Detalhes e limites em [Presets de componentes](P02-PRESETS-COMPONENTES-2026-09-17.md).

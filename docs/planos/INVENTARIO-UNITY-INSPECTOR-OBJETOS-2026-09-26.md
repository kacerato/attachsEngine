# Inventário 1 a 1: Inspector, objetos e dependências (Unity 6000.0 → Astra)

Data: 26/09/2026. Fonte: as 21 fichas não-componente de [`docs/referencias/unity-visual-2026-09-26`](../referencias/unity-visual-2026-09-26/README.md) (grupos **Inspector** e **Objetos**), lidas **na íntegra** nas páginas oficiais 6000.0 (UsingTheEditor só existe em 2022.3). Cada linha é uma função ou propriedade que a página descreve; as subpáginas de que elas dependem entram como **D**.

Estados: **✔** existe e funciona · **◐** existe em parte (o que falta está na coluna) · **✗** não existe · **A** adaptação explícita para toque/Android (a função existe com outra forma).

Blocos de execução no fim. Complementa o [plano de expansão](EXPANSAO-OBJETOS-COMPONENTES-API-2026-09-26.md); os 575 componentes seguem o catálogo daquele plano.

## 1. Use components — `Manual/UsingComponents.html`

| # | Função | Astra | Evidência / o que falta | Bloco |
|---|---|---|---|---|
| 1 | Adicionar pelo menu Component › categoria › tipo | A | folha de Add por família substitui o menu de topo | — |
| 2 | Botão Add Component abre o navegador | ✔ | `buildComponentSheet` | — |
| 3 | Navegar por categoria | ✔ | trilho de famílias | — |
| 4 | Buscar por nome | ✔ | busca com termos de outras engines | — |
| 5 | Scripts do projeto no navegador; não compilados ficam fora | ✔ | "Scripts do projeto" na família Lógica | — |
| 6 | Qualquer número/combinação de componentes | A | adaptação explícita: teto de 64 por objeto (Unity não tem), recusa cita o número e o cabeçalho avisa perto do teto; singulares recusam duplicata com motivo · `32e4738b` | I1 |
| 7 | Ícone de ajuda (?) no cabeçalho abre a referência | ✔ | Referência no menu abre o link https do schema (`ExternalLinks` no Android) · `e9c51e8d` | I1 |
| 8 | Valores padrão ao anexar | ✔ | descritores | — |
| 9 | Editar valor: texto | ◐ | nome do objeto, campos e listas de texto em script; o tipo texto NATIVO entra com o primeiro consumidor (Text da UI/TextMesh), como decide o §4.2 do plano | C |
| 10 | Editar valor: interruptor | ✔ | booleanos | — |
| 11 | Editar valor: lista suspensa (enum) | ✔ | lista de opções com a atual marcada · `e9c51e8d` | I1 |
| 12 | Referência: arrastar do Project | ✔ | recurso arrastado da aba Recursos para o campo · `b96c358a` | I1 |
| 13 | Referência: Object Picker (⊙) | ✔ | `buildReferencePicker`, seletor de malha/material/textura | — |
| 14 | Referência a componente, objeto ou recurso | ✔ | campo de script `component:<tipo>` com a fachada gerada; valor objeto:instância · `253e3990` | I1 |
| 15 | Menu de contexto no cabeçalho (clique direito → toque longo) | ✔ | toque longo 0,45 s · `e9c51e8d` | I1 |
| 16 | Menu ⋮ no cabeçalho | ✔ | `ComponentMenuBase` | — |
| 17 | Reset | ✔ | nativos e comportamentos C# · `e9c51e8d` | I1 |
| 18 | Remove Component com aviso de dependentes | ✔ | "Revisar remoção" + `componentRemovalBlockedBy` | — |
| 19 | Move Up | ✔ | `Components::moveInstance`, com Undo · `e9c51e8d` | I1 |
| 20 | Arrastar componente para reordenar | ✔ | arraste vertical no cabeçalho, barra de inserção, vira página nas setas, Undo · `32e4738b` | I1 |
| 21 | Move Down | ✔ | idem · `e9c51e8d` | I1 |
| 22 | Copy Component | ✔ | nativos e C# · `e9c51e8d` | I1 |
| 23 | Paste Component As New | ✔ | `e9c51e8d` | I1 |
| 24 | Paste Component Values | ✔ | nativos e C# · `e9c51e8d` | I1 |
| 25 | Editar propriedades no Play | ✔ | "Inspecionar" no Play; espelho → `applyPlayEdits` → API do `GameWorld`; física remontada no ponto seguro; campos C# por `edit` da ABI; recusas nomeadas (Estático, reordenar, trocar script) · `f10bead6` | I1 |
| 26 | Voltar aos valores de antes ao sair do Play | ✔ | documento autoral não muda de revisão; parar descarta o espelho · `f10bead6` | I1 |

## 2. Manage components and their values — `InspectorManageComponents.html`

| # | Função | Astra | Evidência / o que falta | Bloco |
|---|---|---|---|---|
| 27 | Adicionar, remover, reordenar e editar valores | ✔ | reordenar pelo menu (19, 21) e pelo arraste (20) · `32e4738b` | I1 |
| 28 | D · Advanced Object Picker (filtro de tipo editável, busca) | ✔ | janela própria: `t:Tipo`, chip de tipo removível, lista/grade/tabela, painel do item; sem abas de provedor (só objetos de cena) · `e8e5dc8d` | I2 |
| 29 | D · Expressões em campo numérico (`2*3`, `+=5`, `L(a,b)`, `R(a,b)`) | ✔ | avaliador `+ - * / % ^`, funções, pi, relativos, L/R; teclado com operadores e prévia; "Expressão" no teclado do sistema · `88cced9c` | I2 |
| 30 | D · Curvas | ✔ | `Astra.AnimationCurve` em scripts com editor de curvas · `ff1d8e32` | I2 |
| 31 | D · Arrays | ✔ | `T[]`/`List<T>` em campos de script; nativos seguem por slot (materiais) e lista de clipes · `1e634f23` | I2 |
| 32 | D · Bar slider (dividir um todo em partes, ex. LOD) | ✔ | barra do LOD Group: divisores, marcador da vista, inserir/apagar nível · `89c5349e` | I2 |
| 33 | D · Cores e gradientes | ✔ | janela de cor (SV/matiz contínuos, RGB 0–255/0–1/HSV, hex, alfa, HDR, original, amostras em bibliotecas) · `59c4a626`; `Astra.Gradient` com editor e presets · `a09ec1e9`; conta-gotas lendo o pixel da tela na GPU · `07566b4b` | I2 |

## 3. Introduction to components — `Components.html`

| # | Função | Astra | Evidência / o que falta | Bloco |
|---|---|---|---|---|
| 34 | Todo objeto tem exatamente um Transform | ✔ | `SceneObject.transform` | — |
| 35 | Create Empty com Transform padrão | ✔ | receita `basic.empty` | — |
| 36 | Selecionar na Hierarchy ou na Scene view mostra os componentes | ✔ | | — |

## 4. Manage references — `InspectorReferences.html`

| # | Função | Astra | Evidência / o que falta | Bloco |
|---|---|---|---|---|
| 37 | Referências padrão ao criar (Cubo com malha e material) | ✔ | cubo da biblioteca | — |
| 38 | Objeto vazio + Malha fica sem referência até atribuir | ✔ | | — |
| 39 | Arrastar da Hierarchy para o campo | ✔ | validação do seletor; recusa não reparenteia · `b96c358a` | I1 |
| 40 | Arrastar do Project para o campo | ✔ | recurso da aba Recursos no campo de recurso · `b96c358a` | I1 |
| 41 | Object Picker filtra pelo tipo do campo | ✔ | `requiredType`, tipo de recurso | — |
| 42 | Picker clássico × avançado (alternar) | ✔ | preferência do projeto · `e8e5dc8d` | I2 |
| 43 | Atribuir objeto a campo de componente usa o primeiro componente do tipo | ✔ | seletor e arraste guardam o 1º do tipo; seletor avisa quando há vários · `253e3990` | I1 |
| 44 | Recusar objeto sem o componente exigido | ✔ | `referenceAccepts` | — |

## 5. Use arrays — `InspectorArray.html`

| # | Função | Astra | Evidência / o que falta | Bloco |
|---|---|---|---|---|
| 45 | Lista de valores ou referências do mesmo tipo | ✔ | "array:<tipo>" para todos os tipos de campo · `1e634f23` | I2 |
| 46 | Botão + | ✔ | `1e634f23` | I2 |
| 47 | Botão − | ✔ | remove o escolhido ou o último · `1e634f23` | I2 |
| 48 | Campo Size (vários de uma vez) | ✔ | crescer repete o último, diminuir corta · `1e634f23` | I2 |
| 49 | Novo elemento copia o anterior | ✔ | `1e634f23` | I2 |
| 50 | Reordenar arrastando o cabeçalho do elemento | ✔ | alça do elemento, vira página pela seta; clipes seguem por botão · `1e634f23` | I2 |

## 6. Use curves — `InspectorCurves.html` (+ D Edit Animation curves)

| # | Função | Astra | Evidência / o que falta | Bloco |
|---|---|---|---|---|
| 51 | Campo de curva e Curve Editor (chaves, tangentes, wrap) | ✔ | toque duplo, arraste, alças, 5 modos + lados quebrados, Clamp/Loop/PingPong, enquadrar/zoom · `ff1d8e32` | I2 |
| 52 | Salvar preset de curva | ✔ | `ff1d8e32` | I2 |
| 53 | Apagar preset | ✔ | toque longo no preset · `ff1d8e32` | I2 |
| 54 | Substituir preset | ✔ | `ff1d8e32` | I2 |
| 55 | Bibliotecas de presets (criar, alternar) | ✔ | .astra/libraries (cores, gradientes, curvas) · `ff1d8e32` | I2 |
| 56 | Add Factory Presets To Current Library | ✔ | 6 presets de fábrica · `ff1d8e32` | I2 |

## 7. Manage the Inspector window — `InspectorOptions.html`

| # | Função | Astra | Evidência / o que falta | Bloco |
|---|---|---|---|---|
| 57 | Abrir Inspector como janela ou aba | ✔ | A: painel fixo + janela focada com abas sobre o viewport (uma tela no telefone) · `cba8cc82` | I3 |
| 58 | Vários Inspectors | ✔ | até 8 abas focadas, cada uma presa ao seu alvo · `cba8cc82` | I3 |
| 59 | Mostra o que foi escolhido na Hierarchy, Scene ou Project | ✔ | objeto, textura, modelo, material `9ce0c9fe`, HDRI `e4608651` e perfil de ambiente `586671c8` em Propriedades; script abre no código, cena abre (A) | I3 |
| 60 | Modo Debug (campos privados e estado de execução) | ✔ | valores crus do esquema e campos de script ocultos · `902b804f` | I3 |
| 61 | Voltar ao modo Normal | ✔ | ⋮ › Modo Normal · `902b804f` | I3 |

## 8. Focused Inspectors — `InspectorFocused.html`

| # | Função | Astra | Evidência / o que falta | Bloco |
|---|---|---|---|---|
| 62 | Travar o Inspector na seleção atual | ✔ | cadeado no cabeçalho; edição vai ao travado · `902b804f` | I3 |
| 63 | Inspector focado de objeto ou recurso | ✔ | objeto `cba8cc82`; material, HDRI e perfil de ambiente (botão de janela ou toque longo no arquivo) `3383e558` | I3 |
| 64 | Inspector focado de componente ou referência (Properties) | ✔ | aba presa à instância do componente, só aquele cartão · `cba8cc82` | I3 |
| 65 | Abrir por ⋮ › Properties | ✔ | ⋮ do cartão, ⋮ do Inspector e menu da Hierarquia · `cba8cc82` | I3 |
| 66 | Restaurar os focados ao reabrir o projeto | ✔ | `.astra/editor-preferences.astra`; volta só se o id ainda é o mesmo objeto · `cba8cc82` | I3 |
| 67 | Ping (localizar na Hierarchy) | ✔ | abre os pais, rola e pisca a linha · `902b804f` | I3 |
| 68 | Caminho completo do item | ✔ | linha "Cena / pai / item" na janela focada · `cba8cc82` | I3 |
| 69 | Abrir focado do item sob o ponteiro | ✔ | A: toque longo na linha da Hierarchy, sem trocar a seleção · `cba8cc82` | I3 |

## 9. Assign icons — `InspectorAssignIcons.html`

| # | Função | Astra | Evidência / o que falta | Bloco |
|---|---|---|---|---|
| 70 | Select icon no Inspector | ✗ | | O4 |
| 71 | Ícone rótulo (cápsula colorida com o nome) na Scene view | ✗ | | O4 |
| 72 | Ícone só imagem (círculo colorido) | ✗ | | O4 |
| 73 | Ícone customizado a partir de textura | ✗ | | O4 |
| 74 | Ícone de script em todo objeto com o script | ✗ | | O4 |
| 75 | Ícone de prefab nas instâncias | ✗ | depende de prefab | P |
| 76 | Remover ícone (None) | ✗ | | O4 |
| 77 | Controle pelo menu Gizmos | ✗ | | O4 |

## 10. Inspect items — `InspectorItems.html`

| # | Função | Astra | Evidência / o que falta | Bloco |
|---|---|---|---|---|
| 78 | Objeto único: componentes e materiais | ✔ | | — |
| 79 | Vários objetos: componentes em comum | A | modo "Selecionar vários" na Hierarquia e na vista (toque soma/tira, no lugar de Ctrl/Shift+clique), Tudo/Filhos/Inverter/Nada; edição replicada num passo de Desfazer; gizmo move/gira/escala todos pelo próprio pivô · `441092d2` | I4 |
| 80 | Aviso de componentes ocultos na multisseleção | ✔ | "N componentes ocultos: nem todos têm" · `441092d2` | I4 |
| 81 | Valor igual mostra; diferente mostra "—" | ✔ | transformação, nome, ativo, camada, números, booleanos, enums, referências e canais · `441092d2` | I4 |
| 82 | Set to Value of [objeto] | A | toque longo no campo com "—" abre a lista de objetos com o valor de cada um · `441092d2` | I4 |
| 83 | Aviso de componente sem edição múltipla | ✔ | vínculo com a fonte, LOD, malha com skin e script não resolvido: "Edição múltipla não disponível" · `441092d2` | I4 |
| 84 | Instância de prefab: opções e overrides em negrito | ✗ | | P |
| 85 | Vários prefabs (sem Select/Revert/Apply) | ✗ | | P |
| 86 | Recurso único: importação e propriedades | ✔ | textura, modelo, material `9ce0c9fe`, HDRI com receita reimportável `e4608651` (degraus `267fd86c`), perfil de ambiente pelo esquema `586671c8`; áudio: não aplicável até existir o subsistema (bloco próprio) | I3 |
| 87 | Vários recursos: comuns e "—" | ✔ | seleção por tipos; texturas com perfil comum e Aplicar atômico (`df0c1538`); materiais com campos mistos e histórico (`bd67ebce`); perfis de ambiente e receitas HDRI em conjunto, cópia de valor, persistência e Desfazer/Refazer atômicos (`9644b320`; [entrega I4 ambientes/UV](I4-AMBIENTES-UV-2026-09-28.md)). Tipos sem editor conjunto próprio mantêm a visão de tipos/contagem, sem propriedades fictícias | I4 |
| 88 | Script: campos públicos/[SerializeField]; HideInInspector | ✔ | `[SerializeField]` expõe privado, `[HideInInspector]` guarda sem mostrar · `1e634f23` | I2 |
| 89 | Ping pelo ⋮ | ✔ | ⋮ do Inspector e ⋮ da janela focada · `902b804f` | I3 |

## 11. Unity's interface — `2022.3/UsingTheEditor.html`

| # | Função | Astra | Evidência / o que falta | Bloco |
|---|---|---|---|---|
| 90 | Toolbar: Play, histórico de Undo, busca, visibilidade de camadas, layouts | ✔ | histórico (toque longo em Desfazer) `7698fcb4`; camadas nas opções do viewport (Unity 6 View Options) `6ce8459b`; busca global `40a9fb9f`; layouts no menu Cena (A: sem janelas soltas) `2a10ca75` | I3 |
| 91 | Hierarchy | ✔ | | — |
| 92 | Game view | ✔ | workspace Play | — |
| 93 | Scene view 3D/2D | ◐ | falta modo 2D | S |
| 94 | Overlays | ◐ | barras do viewport fixas | T |
| 95 | Inspector | ✔ | | — |
| 96 | Project window | ✔ | Arquivos/Recursos | — |
| 97 | Barra de status | ✔ | A: sob o viewport; mensagem do console, contagens, atividade e janela de trabalhos com Cancelar; modo de otimização de código não aplicável (compilação gerenciada única) · `6dc5fe35` | I3 |

## 12. Hierarchy window — `hierarchy-reference.html` (+ D Manage GameObjects in the Hierarchy)

| # | Função | Astra | Evidência / o que falta | Bloco |
|---|---|---|---|---|
| 98 | Ícone de objeto comum | ✔ | `iconForEntity` | — |
| 99 | Ícone de prefab | ✗ | | P |
| 100 | Ícone de variante | ✗ | | P |
| 101 | Ícone de prefab de modelo | ◐ | importação tem vínculo, sem ícone próprio | P |
| 102 | Visível | ✔ | olho | — |
| 103 | Oculto | ✔ | | — |
| 104 | Pai visível com filhos ocultos (marcador) | ✔ | olho por linha, estado do editor fora da cena e do Desfazer; ponto no pai quando os filhos diferem; guardado no projeto · `22fe9bd9` | O3 |
| 105 | Pai oculto com filhos visíveis (marcador) | A | toque longo no olho age só no objeto (Alt+clique da Unity) · `22fe9bd9` | O3 |
| 106 | Selecionável na cena | ✔ | mão por linha · `22fe9bd9` | O3 |
| 107 | Não selecionável | ✔ | mão riscada: toque na vista não pega, a Hierarquia seleciona · `22fe9bd9` | O3 |
| 108 | Pai selecionável com filhos não | ✔ | marcador no pai · `22fe9bd9` | O3 |
| 109 | Pai não selecionável com filhos sim | A | toque longo na mão age só no objeto · `22fe9bd9` | O3 |
| 110 | Indicador de override + menu Overrides | ✗ | | P |
| 111 | Selo + em objeto adicionado à instância | ✗ | | P |

## 13. GameObject — `class-GameObject.html`

| # | Função | Astra | Evidência / o que falta | Bloco |
|---|---|---|---|---|
| 112 | Ativo (caixa ao lado do nome) / `SetActive` | ✔ | `InspectorActive`, `GameObject.SetActive` | — |
| 113 | `activeSelf` | ✔ | `GameObject.ActiveSelf`, ABI v14, persistência e lifecycle validados no aparelho — [entrega O1a](O1A-ATIVACAO-2026-09-28.md) | O1 |
| 114 | `activeInHierarchy` | ✔ | | — |
| 115 | Static (GI, Occlusion, Batching, Navigation, Reflection Probe) | ✗ | flag removida por falta de consumidor | O1 |
| 116 | Tag | ✔ | Catálogo, Inspector, persistência e runtime validados no Android — [tags O1a](O1A-TAGS-2026-09-28.md) | O1 |
| 117 | Layer | ✔ | camada de gameplay no Inspector | — |
| 118 | `CompareTag` | ✔ | Catálogo, Inspector, persistência e runtime validados no Android — [tags O1a](O1A-TAGS-2026-09-28.md) | O1 |
| 119 | `AddComponent<T>` em execução | ✔ | fachada gerada | — |
| 120 | `Destroy(componente)` | ✔ | `Component.Remove` | — |
| 121 | Habilitar/desabilitar componentes por script | ◐ | propriedade `enabled` onde existe; falta uniforme | O1 |
| 122 | `GetComponent<T>` | ✔ | fachada gerada | — |
| 123 | Escrever propriedades por script | ✔ | | — |
| 124 | Chamar métodos (ex. `AddForce`) | ✔ | `Physics` | — |
| 125 | `GetComponent` de script pelo tipo da classe | ◐ | `FindBehavior<T>` protegido; falta em `GameObject` | O1 |
| 126 | `GetComponent` devolve nulo quando ausente | ✔ | | — |
| 127 | Campo público de objeto no Inspector + arrastar | ✔ | campo `object` de script aceita objeto arrastado · `b96c358a` | I1 |
| 128 | Campo de tipo componente (arrastar objeto que o tem) | ✔ | objeto sem o tipo é recusado com o motivo; instância removida aparece "ausente" · `253e3990` | I1 |
| 129 | Array de referências | ✔ | objeto e componente por elemento, seletor por elemento · `1e634f23` | I2 |
| 130 | Filhos pelo Transform (`childCount`, enumerar) | ✔ | `Children()`, `ChildAt` | — |
| 131 | `Transform.Find` | ✔ | `GameObject.Find(name)` | — |
| 132 | `BroadcastMessage` | ✗ | | O1 |
| 133 | `SendMessage` | ✗ | | O1 |
| 134 | `SendMessageUpwards` | ✗ | | O1 |
| 135 | `GameObject.Find` global | ✗ | só a partir de um objeto | O1 |
| 136 | `FindWithTag` | ✔ | Catálogo, Inspector, persistência e runtime validados no Android — [tags O1a](O1A-TAGS-2026-09-28.md) | O1 |
| 137 | `FindGameObjectsWithTag` | ✔ | Catálogo, Inspector, persistência e runtime validados no Android — [tags O1a](O1A-TAGS-2026-09-28.md) | O1 |
| 138 | `Instantiate` | ✗ | | O1 |
| 139 | `Destroy` com atraso | ✗ | | O1 |
| 140 | `Destroy(this)` remove só o script | ◐ | remover componente de script existe; falta atalho | O1 |
| 141 | `CreatePrimitive` (6 tipos) | ✗ | | O2 |

## 14. Primitive objects — `PrimitiveObjects.html`

| # | Função | Astra | Evidência / o que falta | Bloco |
|---|---|---|---|---|
| 142 | Cubo 1×1, textura repetida por face | ✔ | biblioteca | — |
| 143 | Esfera diâmetro 1, UV esférica | ✗ | | O2 |
| 144 | Cilindro 2×1 | ✗ | | O2 |
| 145 | Cápsula 1×2 | ✗ | | O2 |
| 146 | Quad 1×1 no plano XY | ✗ | | O2 |
| 147 | Plano 10×10, 200 triângulos, XZ, uma face | ◐ | "Chão" é cubo achatado | O2 |
| 148 | Colisor padrão em cada primitiva | ✗ | | O2 |

## 15. Prefabs — `Prefabs.html` (+ D subpáginas)

| # | Função | Astra | Evidência / o que falta | Bloco |
|---|---|---|---|---|
| 149 | Criar prefab a partir de objeto | ✗ | "receita de objeto" de preset não é prefab | P |
| 150 | Instanciar na cena | ✗ | | P |
| 151 | Editar o recurso (modo prefab) | ✗ | | P |
| 152 | Prefabs aninhados | ✗ | | P |
| 153 | Variantes | ✗ | | P |
| 154 | Overrides de instância (componente, dado, objeto) | ✗ | | P |
| 155 | Unpack | ✗ | | P |
| 156 | Instanciar em execução | ✗ | | P |
| 157 | Inspector da instância (Open, Select, Overrides, Apply, Revert) | ✗ | | P |

## 16. Edit prefab assets — `EditingInPrefabMode.html`

| # | Função | Astra | Evidência / o que falta | Bloco |
|---|---|---|---|---|
| 158 | Abrir em isolamento | ✗ | | P |
| 159 | Abrir em contexto | ✗ | | P |
| 160 | Trilha (breadcrumb) no topo da Scene view | ✗ | | P |
| 161 | Barra na Hierarchy com voltar | ✗ | | P |
| 162 | Preferência de modo padrão | ✗ | | P |
| 163 | Cena de fundo para edição isolada | ✗ | | P |
| 164 | Contexto Normal/Cinza/Oculto | ✗ | | P |
| 165 | Resto da cena não selecionável; encaixe continua | ✗ | | P |
| 166 | Transform raiz travado em contexto | ✗ | | P |
| 167 | Show Overrides | ✗ | | P |
| 168 | Auto Save | ✗ | | P |
| 169 | Allow Auto Save nas configurações | ✗ | | P |
| 170 | Perguntar ao sair sem Auto Save | ✗ | | P |

## 17. Terrain — `terrain-UsingTerrains.html`

| # | Função | Astra | Evidência / o que falta | Bloco |
|---|---|---|---|---|
| 171 | Criar Terrain + recurso de terreno | ✗ | | T |
| 172 | Terrenos vizinhos | ✗ | | T |
| 173 | Esculpir (subir/baixar, altura, suavizar, carimbo) | ✗ | | T |
| 174 | Pintar camadas de textura | ✗ | | T |
| 175 | Árvores | ✗ | | T |
| 176 | Detalhes (grama, flores, pedras) | ✗ | | T |
| 177 | Configurações do terreno | ✗ | | T |
| 178 | Pincéis embutidos e por textura | ✗ | | T |
| 179 | Tamanho e opacidade do pincel | ✗ | | T |
| 180 | Overlays: ferramentas, ajustes, máscaras, atributos (força alvo) | ✗ | | T |
| 181 | Atalhos de pincel/objeto/tamanho/opacidade | A | botões na paleta | T |
| 182 | Enquadrar a área sob o dedo | ✗ | | T |

## 18. Sprite Renderer — `sprite/renderer/renderer-landing.html`

| # | Função | Astra | Evidência / o que falta | Bloco |
|---|---|---|---|---|
| 183 | Sprite (arrastar / seletor) | ✗ | | S |
| 184 | Open Sprite Editor | ✗ | | S |
| 185 | Color | ✗ | | S |
| 186 | Flip X/Y | ✗ | | S |
| 187 | Draw Mode Simple/Sliced/Tiled | ✗ | | S |
| 188 | Mask Interaction | ✗ | | S |
| 189 | Sprite Sort Point | ✗ | | S |
| 190 | Material | ✗ | | S |
| 191 | Size | ✗ | | S |
| 192 | Tile Mode Continuous/Adaptive | ✗ | | S |
| 193 | Stretch Value | ✗ | | S |
| 194 | Sorting Layer (+ Add Layer) | ✗ | | S |
| 195 | Order in Layer | ✗ | | S |
| 196 | Rendering Layer Mask | ✗ | | S |

## 19. Tilemap — `tilemaps/work-with-tilemaps/create-tilemap.html`

| # | Função | Astra | Evidência / o que falta | Bloco |
|---|---|---|---|---|
| 197 | Criar Grid + Tilemap (retangular, hex ×2, isométrico ×2) | ✗ | | M |
| 198 | Tile Palette: escolher paleta | ✗ | | M |
| 199 | Alvo ativo | ✗ | | M |
| 200 | Pintar com o pincel ativo; vários tiles | ✗ | | M |
| 201 | Pegar tile da cena | ✗ | | M |
| 202 | Preencher, linha, aleatório | ✗ | | M |
| 203 | Selecionar tiles na cena | ✗ | | M |
| 204 | Mover tiles | ✗ | | M |
| 205 | Escala pela seleção de grade | ✗ | | M |
| 206 | Apagar (seleção, pincel, borracha) | ✗ | | M |
| 207 | Outro tilemap no mesmo grid + aviso de layout | ✗ | | M |

## 20. Canvas — `UICanvas.html`

| # | Função | Astra | Evidência / o que falta | Bloco |
|---|---|---|---|---|
| 208 | Canvas e elementos filhos | ✗ | | C |
| 209 | Criar elemento de UI cria o Canvas | ✗ | | C |
| 210 | Retângulo do Canvas na Scene view | ✗ | | C |
| 211 | EventSystem | ✗ | | C |
| 212 | Ordem de desenho pela Hierarchy | ✗ | | C |
| 213 | `SetAsFirstSibling`/`SetAsLastSibling`/`SetSiblingIndex` | ◐ | reordenar existe no editor e `SetParent(index)` | C |
| 214 | Screen Space – Overlay | ✗ | | C |
| 215 | Screen Space – Camera | ✗ | | C |
| 216 | World Space | ✗ | | C |
| 217 | Additional Shader Channels | ✗ | | C |
| 218 | Use Reflection Probes | ✗ | | C |
| 219 | Vertex Color Always in Gamma | ✗ | | C |

## Contagem

| Estado | Itens |
|---|---:|
| ✔ existe | 107 |
| ◐ parcial | 9 |
| ✗ falta | 96 |
| A adaptação | 7 |
| **Total** | **219** |

## Blocos de execução (ordem por dependência)

| Bloco | Itens | Por que nesta ordem |
|---|---|---|
| **I1** Inspector de componentes | 6, 7, 11, 12, 14, 15, 17, 19–27, 39, 40, 43, 127, 128 | base de toda edição; Play-edit é pré-requisito para testar os demais |
| **I2** Tipos de campo | 9, 28–33, 42, 45–56, 88, 129 | curvas, gradientes, listas e texto são exigidos por partículas, linha, UI, áudio |
| **I3** Janela Inspector | 57–69, 86, 89, 90, 97 | |
| **I4** Multisseleção | 79–83, 87 | ✔ 79–83 `441092d2`; 87: texturas `df0c1538`, materiais `bd67ebce`, [ambientes e UV](I4-AMBIENTES-UV-2026-09-28.md) `9644b320` |
| **O1** GameObject e API | 113, 115, 116, 118, 121, 125, 132–140 | 113 entregue; [O1a parcial](O1A-ATIVACAO-2026-09-28.md); [tags entregues e validadas no Android](O1A-TAGS-2026-09-28.md); faltam habilitação uniforme, static com consumidor, mensagens, Instantiate |
| **O2** Primitivas | 141, 143–148 | |
| **O3** Hierarquia | 104–109 | ✔ `22fe9bd9` |
| **O4** Ícones de objeto | 70–74, 76, 77 | |
| **P** Prefabs | 75, 84, 85, 99–101, 110, 111, 149–170 | exige identidade de recurso e overrides |
| **S** Sprite e 2D | 93, 183–196 | backend 2D (Box2D, renderer de sprite) |
| **M** Tilemap | 197–207 | depende de S |
| **T** Terreno | 94, 171–182 | |
| **C** Canvas e UI | 208–219 | exige texto de runtime |

Cada item é marcado nesta tabela com o commit que o entrega.

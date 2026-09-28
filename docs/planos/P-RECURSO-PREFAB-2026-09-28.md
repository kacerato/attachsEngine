# Prefab — recurso, instância e execução

Etapa 6 e Unpack da etapa 7 do bloco Objetos/Scripts/Prefabs, validados no
aparelho em 28/09. Isto não declara prontas as etapas de overrides, edição da
fonte, aninhamento e variantes, nem o bloco inteiro.

## Referências e decisões

- [Godot 4.5 — PackedScene](https://docs.godotengine.org/en/4.5/classes/class_packedscene.html):
  o recurso representa uma hierarquia serializada, separada dos objetos vivos.
  A Astra também mantém fonte e instância separadas; captura a subárvore
  escolhida, sem copiar estado global da cena ou dependências do editor.
- [Godot 4.5 — implementação de SceneState](https://github.com/godotengine/godot/blob/4.5/scene/resources/packed_scene.cpp):
  identidades de nós, resolução de referências e tratamento de instanciação
  são responsabilidades explícitas. A Astra reutiliza seu grafo e cria todos
  os destinos antes de remapear referências, inclusive referências adiante.
- [Unity 6000.0 — overrides](https://docs.unity3d.com/6000.0/Documentation/Manual/PrefabInstanceOverrides.html):
  mudanças locais precisam ser distintas da fonte. O vínculo Astra guarda a
  identidade do nó e uma base persistente para a próxima etapa de comparação;
  guardar essa base ainda não significa executar Apply/Revert.
- [Unity 6000.0 — Unpack](https://docs.unity3d.com/6000.0/Documentation/Manual/UnpackingPrefabInstances.html):
  desfazer o vínculo conserva o resultado efetivo. A Astra retira os metadados
  da instância inteira, mesmo quando selecionada por um filho, sem depender da
  disponibilidade da fonte e sem trocar identidades, valores ou referências.
- [Unity 6000.0 — Prefab Mode](https://docs.unity3d.com/6000.0/Documentation/Manual/EditingInPrefabMode.html):
  contexto de autoria identificado e retorno por breadcrumb. O usuário escolheu
  uma rota contextual, preservando seleção e enquadramento ao retornar à cena.
  Essa rota pertence à etapa 8 e ainda não foi implementada neste pacote.
- Vídeo oficial pesquisado: [Unity — Improved Prefab Workflows](https://www.youtube.com/watch?v=ibmdm_PoyMA).
  A ferramenta não conseguiu abrir o vídeo; não há alegação de observação dos
  seus quadros. As decisões de interação acima foram verificadas no manual.

## Contrato implementado

`ASTRA_PREFAB 1` guarda GUID do recurso, raiz e objetos com identidade local
estável, pai, tipo, nome, transform, atividade, flags, camada, tag e componentes
com suas identidades de instância. Os codecs de componentes existentes são
reutilizados. Não há dependência do runtime sobre `EditorArchive`.

O limite é 32 MiB por recurso e 256 KiB por registro de objeto. Arquivo inválido,
versão desconhecida, componente indisponível, hierarquia inválida ou referência
a objeto fora da subárvore são recusados com diagnóstico. Falhas de leitura ou
captura não substituem o recurso anterior. Referências internas de objetos,
componentes e listas são remapeadas; recursos do projeto usam GUID.

`astra.prefab.link` guarda fonte, nó original, raiz da instância e base de
comparação. Não é um componente adicionável pelo catálogo. O vínculo persiste
na cena e participa da inspeção de uso de recursos. Duplicar a instância inteira
troca seu dono; duplicar somente um filho solta o vínculo desse trecho. Modelos
importados contidos na hierarquia também recebem identidades de instância
próprias, usando o mesmo remapeamento no editor e no runtime.

Criar o recurso prepara cena/histórico antes de publicar arquivo e registro no
journal existente. Desfazer a conversão retira o vínculo da seleção; o arquivo
criado permanece como recurso reutilizável. Instanciar/desfazer/refazer uma
hierarquia inteira ocupa um passo. Índices transitórios de malha são resolvidos
novamente a partir do GUID. Dependências de malhas e clipes apontam para o
recurso de origem quando são sub-recursos importados. A exclusão de tags confere
também prefabs salvos.

Na UI: ações do objeto → Criar prefab da seleção; Arquivos → selecionar
`.prefab` → Instanciar prefab. Raízes vinculadas têm ícone próprio do atlas;
o Inspector mostra o caminho da fonte sem consumir espaço com um painel novo.
Desvincular instância de prefab fica nas ações do objeto e ocupa um passo de
Desfazer/Refazer. Outras instâncias permanecem vinculadas. A listagem Todos do
catálogo também passou a incluir os scripts antes disponíveis só em Lógica/busca.

Na API: `parent.InstantiatePrefab(AssetGuid)` cria uma subárvore no mundo atual.
A ABI 19 separa criação e consulta dos scripts anexados: consultar tamanho não
cria outra instância. Tipos e propriedades autoradas são preparados antes de
`Awake`; tipo ausente ou falha de binding desfaz a criação inteira. Ativação,
física e destruição seguem os sistemas existentes. Valores em Play não escrevem
a fonte nem a cena autoral.

## Validação de host

- 6 testes no filtro `prefab_`: roundtrip, remapeamento, persistência do vínculo,
  duplicação, rollback, publicação do recurso/registro, fonte ausente, ownership
  de importação, proteção de tags e ABI.
- Regressões relacionadas: `import_` 51, `play_` 33, `preset` 9, `duplic` 10.
  Os filtros se sobrepõem; os números não são uma soma de testes únicos.
- 22 testes de comportamentos C# passaram; o teste adicional de compilação do
  cenário `tests/fixtures/prefab/PrefabProbe.cs` também passou.

## Limites ainda abertos

Apply/Revert, propagação de atualizações, rota de edição da fonte,
prefabs aninhados e variantes continuam na sequência do plano. Capturar uma
hierarquia que já contém vínculo de prefab é recusado explicitamente enquanto
a composição não estiver implementada. Não há achatamento silencioso.

O GUID da API pode ser obtido do registro do projeto; ainda não existe um
campo C# de recurso prefab com seletor dedicado no Inspector. Medições em
escala e validação integrada pertencem à etapa 12.

## Aceite Android

APK debug SHA-256: `58496FB45036327EDABC5CDC7314F6262BA72DBB74F72AF7517AA282C3AD6197`.
Projeto isolado `PrefabBlock0928`; projetos anteriores preservados.

1. Adicionar PrefabReceiver à esfera, escrever Value=41 no Inspector e criar
   `Prefabs/Esfera.prefab` pelas ações do objeto. Arquivo inspecionado: malha por
   GUID, corpo, colisor e valor autoral presentes.
2. Instanciar pela lista de arquivos, desfazer e refazer; salvar e reabrir.
   A segunda esfera conserva o caminho da fonte, o script e os quatro componentes.
3. Desvincular a segunda esfera, desfazer/refazer, salvar e reabrir. O vínculo da
   primeira permanece; a segunda mantém os quatro componentes e a identidade 4.
4. Instanciar duas hierarquias pela API C#, conferir Value=41 antes de Awake
   (o padrão do código é 7), alterar só a primeira, raycast nas duas, destruir a
   primeira e conferir a segunda. `PREFAB PASS` às 17:29:03 no APK acima.
   Stop devolve a cena autoral, sem persistir os objetos criados em Play.

Capturas, fontes salvas e logs: `docs/validacao/evidencias/p-prefab-20260928/`.
A inspeção visual confirmou identidade do vínculo e fonte legível em uma linha,
além de ações com área de toque adequada no landscape 2772×1280. A futura rota
de edição e o fluxo completo em portrait ainda exigem seu próprio aceite.
O ícone foi integrado como SVG/PNG e ao atlas real; não foi usada imagem
conceitual como prova de implementação.

Contagem deste pacote no inventário: quatro itens entregues (149, 150, 155,
156). Total acumulado: 127 existentes, 5 parciais, 80 ausentes e 7 adaptações.

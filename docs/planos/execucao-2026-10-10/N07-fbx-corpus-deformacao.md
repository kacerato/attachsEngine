# N07 — pesquisa executável do corpus FBX deformável

Estado em 10/10/2026: expansão vertical conectada ao CMake/dispatcher/editor.
Baseline 7612/41541: material/biblioteca **20/20**, 520 FBX estáticos decodificados;
corpus 151 deformável, 150 aceitos e uma recusa dual quaternion. Desses aceitos,
99 têm skin, 75 morph, 72 takes e 23 não têm mesh; categorias se sobrepõem.
O cenário cold Play deformável passou após corrigir a expectativa de 500 ms
em um frame limitado a 250 ms para dois avanços de 250 ms. A pesquisa
preliminar abaixo permanece separada dos resultados de importação efetiva.

`tools/validation/inspect_fbx_sources.cpp` foi compilado comC++20/O1/g0/Werror e
executado sobre as151 fontes FBX de `analise-plugins`. O relatório detalhado fica
em `local-assets/fbx-deformation-research.tsv`, ignorado e privado. Não publicar
arquivos, imagens, nomes internos ou payloads licenciados como exemplos públicos.

| Observação do decoder nativo | Fontes |
|---|---:|
| Arquivos lidos pelo ufbx0.20.0 |151/151|
| Com deformers de skin |100|
| Com stacks/takes |91|
| Sem malha |23|
| Com blend deformers |75|
| Com constraints declaradas |0|
| Atributos linear/rigid preparados pelo candidato |99|
| Fontes com takes TRS preparados pelo candidato |67|

As categorias se sobrepõem. A pesquisa autorizou top4 e aproximação curta de steps
para revelar dependências, sem gravar receitas nem recursos no projeto. Uma skin
exige dual quaternion. Nos takes,18 fontes têm stacks sem canais TRS efetivos,
duas ultrapassavam o teto inicial de262144 chaves originais e quatro exigiam diagnóstico
mais específico do intervalo/tempos/bake. Esses casos não contam como importados.
Os520 FBX do POLYGON são outro corpus, já reservado ao cenário estático; estes
números não são uma soma de formatos aceitos ou uma contagem de capacidades.

## Consequência para a implementação

Não basta acrescentar JOINTS/WEIGHTS ao importador estático. O caminho necessário é:

1. Preparar skin, morph e takes no mesmo espaço local que a geometria; expandir
   os atributos pelos índices de controle antes de costuras/normais/LOD.
2. Emitir skins e samplers no intercâmbio glTF e recusar qualquer canal ou skin
   descartado pelo consumidor existente. Nó animado usaTRS; matriz com shear exige
   diagnóstico, sem decomposição silenciosa.
3. Persistir escolhas de pesos/taxa/steps na receita; incluir todos os vetores e
   strings de deformação no custo retido real do histórico, antes da publicação.
4. Reconciliar juntas, clipes e caches pelo consumidor existente. Falha de GPU ou
   metadado deve preservar também as instâncias e o cursor do histórico.
5. Tratar fontes sem malha como biblioteca de animação/rig: sub-recursos navegáveis
   e vínculo compatível com o rig da cena. Não inventar um triângulo para contornar
   a exigência de geometria de `stageSource`/`importGlb`.

`fbx_scene_import` agora produz skin/morph/takes no consumidor glTF existente.
`fbx_morph` prepara offsets por vértice de controle, defaults e in-betweens separados;
o conversor amostra os pesos efetivos por take. `fbx_deformation` limita orçamento
de chaves originais e retidas separadamente, respeita intervalo nomeado e amostra
fronteiras em quaternion/TRS. Receita12 persiste deformação, top4, steps e30/60Hz;
leitores1–11 mantêm o contrato estático. Mudanças entram na chave do derivado.

Fontes sem mesh têm hierarquia e takes nativos, sem triângulo artificial. O cache
e a publicação aceitam essa representação. `editor_model_takes` oferece extração
AECLIP independente com mapeamento explícito de nós; `editor_fbx_rig_binding`
valida hierarquia, transforms de referência e nomes/ordem dos morphs antes de
gravar. Não é retargeting humanoide. Dual quaternion, constraints, NURBS e
propriedades animadas sem consumidor continuam recusados com diagnóstico.

O histórico contabiliza capacidades reais dos vetores/strings de skin, morph,
samplers, cues e regiões; recusa composição autoral/propriedades nesse cache.
Reimportação religa mudanças de juntas herdadas, preserva troca local de osso vivo
e recusa juntas/alvos excluídos pela receita. A publicação mantém fonte, registro,
mapa, receita, índice, instâncias, GPU e cursor do histórico no mesmo contrato.
As correções de tangentes UV degeneradas e merge independente XYZ passaram na
baseline 60941; isso não resolve a recusa de skin dual quaternion.

Foi acrescentado um único cenário vertical autoral skin+morph+take+meshless,
UI655×300, extração editável, Undo/Redo, falha gráfica e coldPlay. O corpus151
passou pelo dispatcher real sob política explícita, com os resultados separados
no cabeçalho. O cold Play completo e a captura do cenário corrigido continuam
gates de aceite, distintos da decodificação das fontes.

## Workflow mobile proposto

NÃO IREI SER SIMPLISTA NO DESIGN.

O arquivo deve continuar sendo o contexto. A superfície focal de importação
alterna entre estrutura, malha, rig, takes e perdas, mantendo uma prévia dominante
e controles48px. Selecionar um take revela intervalo/taxa e reprodução; selecionar
uma skin revela número de juntas e perdas de pesos. Opções incompatíveis ficam
indisponíveis com o motivo real. A biblioteca oferece os clipes como sub-recursos
do arquivo, sem exigir scripts externos para extrair ou vincular animação.

A alternativa estrutural para arquivos sem malha é abrir diretamente os takes e
oferecer um rig compatível da cena como alvo de prévia. Essa rota precisa validar
hierarquia, correspondência e transforms; nomes iguais sozinhos não provam
compatibilidade. A fonte permanece isolada e preservada quando o vínculo falha.

[Unity6000.0 Rig](https://docs.unity3d.com/6000.0/Documentation/Manual/FBXImporter-Rig.html)
distingue importar rig de retargeting. [Unity6000.0 Euler resampling](https://docs.unity3d.com/6000.0/Documentation/Manual/AnimationEulerCurveImport.html)
explica o bake quaternion e suas diferenças de fidelidade.
[Godot4.5 Advanced Import](https://docs.godotengine.org/en/4.5/tutorials/assets_pipeline/importing_3d_scenes/advanced_import_settings.html)
separa os sub-recursos por contexto e permite biblioteca de animação. Esses
princípios orientam o workflow; não autorizam afirmar paridade. A implementação
precisa de captura executável nova e de um cenário personagem→deformação→clipe
editável→reimportação→Undo/Redo→coldPlay antes do aceite.

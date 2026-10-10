# Cobertura dos plugins fornecidos — N01–N16

Inventário de origem em 10/10/2026. Presença de arquivo ou referência não declara
capacidade implementada. `plugins-itens.csv` cobre cada arquivo dos quatro pacotes
e das integrações extraídas, com caminho, SHA-256, tamanho, blocos responsáveis e
estado de origem. Os `.meta` permanecem junto às fontes e são lidos pelos
conversores de GUID; não são contados novamente como capacidades. Miniaturas
originais não demonstram preview/render Astra.

`plugins-declaracoes.csv` relaciona cada declaração encontrada nos catálogos
de código fornecidos a caminho/linha/blocos. Declarações incluem campos privados,
tipos, membros herdados e exemplos: não são contagem de APIs utilizáveis Astra.
Não inclui bytecode das DLLs nem inventa corpo de método onde há apenas metadados.

Fonte local: `C:/Users/donod/Downloads/analise-plugins/analise-plugins`.
Licença fornecida: `../licenca_de_uso_de_materiais.txt`, autoriza uso e adaptação
dentro dos direitos do emissor, pede comunicação prévia para comercialização e
preserva direitos de terceiros. Este registro não publica o payload privado.

| Pacote extraído | Arquivos inventariados | Cobertura |
|---|---:|---|
| UMotionPro-1.29p04 | 280 | Todos os arquivos presentes, sem `.meta` |
| FinalIK-2.2 | 503 | Todos os arquivos presentes, sem `.meta` |
| BoZo-2.2 | 879 | Todos os arquivos presentes, sem `.meta` |
| FinalIK-integracoes | 75 | Todos os arquivos presentes, sem `.meta` |
| BoZo-integracoes | 2 | Todos os arquivos presentes, sem `.meta` |
| POLYGONIcons-v1.01 | 1077 | Todos os arquivos presentes, sem `.meta` |

POLYGON Icons v1.01: localizado e inventariado.
Origem adicional: `C:/Users/donod/Downloads/POLYGON Icons Pack - Low Poly 3D Art by Synty v1.01/poly.unitypackage`; licença/aviso `Readme.txt`
ao lado do pacote. A extração local está em `C:/Users/donod/Downloads/atchengine/local-assets/synty-polygon-icons-v1.01`.
O catálogo original mantém GUID/meta e SHA-256; 520 prefabs e 520 FBX são fontes,
não tipos de componente. `baked-catalog.json` local identifica sprites/GLB derivados
por Blender e suas limitações; não comprova conversão do shader/lighting Unity.
As saídas privadas permanecem fora da documentação pública e do atlas da engine.



## Responsabilidade por capacidade

| Origem concreta | Capacidades e destino | Estado Astra nesta auditoria |
|---|---|---|
| UMotion `Assets/UMotionEditor` e 75 HTML em `Manual` | Pose, seleção, curves, layers, intervals, cues N01; property animation N02; rig/mirror N03; IK/Child-Of/pinning N04; root motion N05; bake/FBX N06; import N07; API N08; Timeline N09 | Manual/DLL são referência; não executados no Android nem tratados como implementação Astra |
| UMotion `Assets/UMotionExamples` | Kyle, quatro `.anim`, quatro controllers, projetos, materiais/texturas, cena N01/N03/N05/N07/N16 | Receitas Kyle existentes devem ser reconciliadas; inventário não certifica importação de todos itens |
| FinalIK `Assets/Plugins/RootMotion/FinalIK` | Aim/LookAt/CCD/FABRIK/FABRIKRoot/trig/limb/arm/leg/biped/fullbody/finger/VR, limites, posers N03/N04 | Código Unity exige backend nativo e editor; VRIK exige tracking externo |
| FinalIK Grounder/Interaction/Offset/HitReaction/Recoil/Utilities | Contatos, interação, twist, inércia, reação, ragdoll, locomotion N04/N05/N12/N15 | Não basta anexar nomes de componentes |
| FinalIK Baker e Editor | Bake/export N06; inspeção/gizmos/SDK N08; recursos de demos N07/N16 | Conversão independente de Unity necessária |
| `FinalIK-integracoes` (11 pacotes) | VR SDK, Photon/PUN, Animation Rigging e outras pontes N04/N05/N08/N13/N14 | Dependências externas preservadas; não equivalem a rede/XR Astra |
| BoZo `Assets/BoZo_StylizedModularCharacters` | Body/outfit/attachments, shapes, proporções, LOD/merge, cores/materiais, save/icon capture, creator UI N05/N07/N10/N11/N13/N15 | Mapear modelo/runtime nativos; bugs de origem descritos em ANALISE-BOZO não devem ser transplantados |
| BoZo materiais/shadergraphs e integrações Built-In/URP | Materiais, texturas, bake N07/N14; editor/extensions N08 | Shaders customizados precisam conversão específica e diagnóstico |
| POLYGON Icons v1.01 `Assets/PolygonIcons` | 520 sprites/modelos, 17 materiais e 16 paletas N07/N10/N11/N14 | Fonte/GUID preservados; sprites/GLB offline existentes não equivalem a shader ou cena Unity nativos |
| Todos os FBX, imagens, Unity YAML/prefabs/controllers | Biblioteca e formatos N07; cenas exemplos desmontáveis N16 | CSV separa origem de suporte; arquivo convertido só é aceito após consumer/editor/persistência |

## Ligações com todo o plano

N01–N06 usam a autoria/rig/movimento/exportação pesquisadas; N07 possui inventário
e conversores; N08 dá comandos tipados aos mesmos backends; N09 compõe recursos;
N10 implementa UI de jogo e editor; N11 geometria/UV/material; N12 física e
regeneração; N13 save/replay/rede; N14 render/atmosfera/2D e opcionais; N15
orçamentos mensurados; N16 docs, distribuição e aceite. Esses vínculos são
responsabilidade de trabalho, não promoção automática de todo arquivo a feature.

## Conversão de materiais

O bloco N07 adiciona leitura tipada do YAML Material da Unity e conversão do
workflow Standard metálico compatível. `.mat` original e `.meta` são preservados.
Shaders customizados, specular workflow, metallic/smoothness packed maps e
propriedades sem consumidor equivalente são recusados com diagnóstico. Texturas
suportadas mantêm referências por GUID e interpretação de cor/dado. O relatório
da conversão não promete equivalência entre BRDFs nem bake do shader Unity.
O estado de compilação, host, pacote e Android deve ser registrado separadamente
na entrega N07; ADB permanece bloqueado pelo proprietário até o fim do plano.

Referências: Unity 6 (6000.0), [Standard Shader parameters](https://docs.unity3d.com/6000.0/Documentation/Manual/StandardShaderMaterialParameters.html),
[URP Lit](https://docs.unity.com/en-us/engine/6000.0/manual/materials-and-shaders/built-in/shaders-in-universalrp/reference/lit-shader),
[Unity text serialization](https://docs.unity3d.com/6000.0/Documentation/Manual/FormatDescription.html).

Regeneração: `python tools/validation/inventory_plugin_sources.py --source <pasta-extraida>`.

## Conversores e evidência por família — revisão posterior

O saldo vigente está em `N07-saldo-real.md`, confrontado com as páginas15–16 do
plano. O inventário de origem permanece distinto do aceite da engine.

| Família de fontes | Caminho nativo e estado |
|---|---|
| FBX POLYGON | Dispatcher ufbx, 520 originais decodificados no gate 7612/41541; galeria offline não é usada como prova desse decoder |
| FBX UMotion/FinalIK/BoZo | 151 originais pelo dispatcher, 150 aceitos e um recusado por dual quaternion; 99 skin, 75 morph, 72 takes, 23 sem mesh, categorias sobrepostas; cenário deformável com cold Play aprovado |
| TGA/TIFF | 7 TGA e4 TIFF originais aceitos sob políticas explícitas; variantes fora do contrato continuam recusadas |
| Material Unity Standard | Conversão e consumidores reais nos cenários aceitos; shadergraphs/custom shaders não equivalentes continuam diagnóstico |
| Unity YAML anim/prefab/controller | Conversores GUID/fileID integrados ao editor; cenário 20 da Biblioteca aprovado no gate 20/20, com toque, merge/conflito, Undo/Redo, reabertura e Play; variantes/Humanoid/contratos não equivalentes recusados |
| PSD, cenas Unity, variantes/patched prefabs e demais contratos não equivalentes | Lacunas explícitas; preservação de fonte não vira selo de suporte |

Nenhuma dessas linhas certifica todos os 2.816 arquivos como utilizáveis. O CSV
não recebeu promoção automática de status, e nenhuma licença de terceiros foi
substituída pelo simples acesso local aos pacotes.

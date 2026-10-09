# Animation Studio: fontes de personagens

O adaptador usa Blender 4.5 para gerar GLB, consumido pelo importador nativo
existente. Não depende de Unity instalada nem executa scripts dos pacotes.
As receitas referenciam arquivos fornecidos pelo proprietário; esses arquivos
não estão neste diretório e não são incluídos automaticamente no APK público.

## Conversão reproduzível

Execute uma receita por fonte. A raiz é a pasta de **um pacote**, por exemplo
`UMotionPro-1.29p04`, e não a pasta que contém todos os pacotes.

```powershell
& $blender -b --python-exit-code 1 --python tools/animation-studio/import_character.py -- $packageRoot tools/animation-studio/robot-kyle.recipe.json build/animation-character-library/RobotKyle.glb
```

| Receita | Pacote | Resultado |
|---|---|---|
| robot-kyle.recipe.json | UMotion Pro 1.29p04 | RobotKyle.glb, 49 juntas, sem clipes de origem |
| viking.recipe.json | Final IK 2.2 | Viking.glb, 22 juntas, Idle |
| viking-walk.recipe.json | Final IK 2.2 | VikingWalk.glb, 22 juntas, Walk |
| viking-run.recipe.json | Final IK 2.2 | VikingRun.glb, 22 juntas, Run |

O destino deve terminar em `.glb` e ficar fora do pacote original. Cada receita
declara materiais/texturas e altura uniforme em metros. O conversor mantém
armature, parents, skin, bind e ações de seus donos, inclusive o nó de movimento
da raiz. Não aplica a rotina de malha estática que desparenta os meshes. Ações
são amostradas na exportação; equivalência contínua com o sampler FBX não é
declarada. O relatório `.import.json` registra fontes/hashes, juntas, materiais,
ações e hash do GLB. Uma fonte alterada durante a conversão recusa a publicação.

Clipes adicionais só compartilham um rig quando nomes, parents e repousos
coincidem. Os FBX Viking Idle, Walk e Run têm repousos diferentes; conservá-los
como fontes completas separadas evita afirmar retargeting por igualdade de
nomes. O limite atual do consumidor é 256 juntas e quatro influências de skin.
Behaviours, controllers, prefabs e shaders Unity não são convertidos por esta
ferramenta. TGA é resolvido na exportação para uma imagem suportada no GLB.

## Projeto editável privado de aceite

Depois de gerar os quatro GLBs:

```powershell
& build/editor-host/aether_gui_preview.exe write-animation-library build/animation-character-library build/AnimationStudioPacotes-novo
```

O destino deve ser novo. O comando usa EditorSession, o registro de assets,
o journal de importação e o serializer real: importa quatro fontes, extrai três
clipes Viking, cria/grava um canal de braço Kyle e monta um palco iluminado.
Em outra sessão, recarrega registro/fontes/cena, compara os clipes e avalia o
runtime. O projeto mantém GLBs e auditorias; não copia os pacotes Unity inteiros.
Ele serve para conferir biblioteca, bindings e autoria na engine, sem declarar
portabilidade dos behaviours Unity, retargeting ou IK.

O aceite nativo dos GLBs é opt-in, sem tornar assets privados requisito do CI:

```powershell
$env:AE_ANIMATION_CHARACTER_LIBRARY = (Resolve-Path build/animation-character-library).Path
& build/editor-host/aether_animation_clip_editor_tests.exe animation_character_package
```

Esse cenário verifica importação, amostragem, paletas e deformação CPU. Ele não
substitui abrir o projeto, inspecionar materiais/skin no renderer e testar os
gestos no Android. Em Windows, disponibilize as DLLs do compilador MinGW no PATH
antes de executar os consumidores host.

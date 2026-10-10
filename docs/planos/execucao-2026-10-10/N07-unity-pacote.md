# N07 — conversão Unity com referências exatas

Estado em 10/10/2026: conversores, publicador, entradas CMake e comandos do
editor integrados. Sintaxe estrita final dos conversores/editor/cenário aprovada
(46764, 97256 e 86231, saídas 0). O build conjunto7612/41541 terminou com
sucesso e [library-integrated.log](../../validacao/n01-n07-2026-10-10/host/library-integrated.log)
registrou 20/20 cenários, incluindo o fluxo Unity descrito abaixo. A
[captura executável 655×300](../../validacao/n01-n07-2026-10-10/host/capturas/unity-library-655.png)
foi inspecionada: origem/recurso são distinguíveis, a superfície focal cabe e
Instanciar/Reimportar permanecem acessíveis. É rasterização CPU da UI host,
sem evidência de imagem Vulkan ou aparelho. Não encerra N07 inteiro.

## Problema e contrato

Preservar `.meta` e listar um prefab não permite utilizá-lo. A conversão deve
resolver GUID **e** fileID dentro da revisão do pacote, produzir recursos
nativos e conservar fonte, baseline, receita e alterações locais em uma única
publicação. fileIDs são inteiros de 64 bits, nunca números de ponto flutuante.

O leitor novo aceita o dialeto textual delimitado por objetos Unity, mapas,
sequências com indentação Unity e coleções inline. Recusa aliases, merges,
blocos de texto e construções não interpretadas. Limites: fonte de 64 MiB,
524.288 nós YAML, profundidade 64 e 65.536 objetos. A resolução de subassets
usa `fileIDToRecycleName` ou `internalIDToNameTable` do `.meta`; nomes sozinhos
não substituem fileIDs ausentes.

## Cadeias integradas

- `.anim`: TRS genérico e blend shape por nome real do renderer selecionado;
  curvas nativas com tangentes, pesos e descontinuidade, reflexão LH/RH e
  IDs autorais estáveis. A reimportação compara baseline/local/fonte por
  canal e preserva camadas, propriedades, drivers, cues e regiões locais.
- `.prefab`: GameObject, Transform, MeshFilter e MeshRenderer completos;
  hierarquia real, mesh/submeshes publicados e materiais compartilhados.
  O resultado usa `runtime::Prefab` e o fluxo nativo de instanciação.
- `.controller`: parâmetros tipados, camadas Override independentes,
  estados com clipes, BlendTree 1D, condições, duração/offset e interrupção.
  O resultado usa o Animator nativo; estados e transições conservam identidade
  por fileID, parâmetros/camadas por nome exato e fronteira monotônica.

Controllers e prefabs usam conflito explícito do recurso inteiro quando
ambos os lados mudam. Clipes oferecem merge por canal. `Manter local` e
`Receber origem` são decisões persistidas, não fallback silencioso. Instâncias
de prefab na cena continuam usando o mecanismo existente de overrides.

O publicador usa o journal do projeto para arquivo nativo,
registro e índice; baseline imutável é endereçado pelo hash. Undo/Redo passa
pelo mesmo publicador e rejeita alteração externa de arquivo, receita ou
índice. A fonte privada não é sobrescrita para fabricar um resultado nativo.

Undo autoral conserva revisão e fronteira de IDs monotônicas. A verificação do
histórico Unity normaliza somente esses dois campos para comparar conteúdo;
todos os demais dados, receita, dependências e índice permanecem exatos. O hash
do arquivo atual precisa coincidir com o registro atual, e o journal verifica
esse hash antes de escrever. Assim, editar e desfazer um clipe não produz um
conflito falso na reimportação anterior nem permite sobrescrever edição externa.

## Limitações explícitas

Humanoid/Avatar, curvas Euler ZXY sem bake declarado, eventos executando
callbacks Unity, animação comprimida e PPtr não possuem conversão equivalente
neste pacote. Blend shapes exigem renderer compatível selecionado.

PrefabVariant/PrefabInstance com patches, SkinnedMeshRenderer em prefab,
colliders, scripts, tags/camadas sem mapa e componentes desconhecidos são
recusados como um todo. FBX com skin/morph/takes tem seu caminho independente;
isso não prova conversão de um prefab Unity que os referencia.

Controllers com máscaras, IK, sincronização, máquinas aninhadas,
StateMachineBehaviour, blends sem semântica equivalente ou Write Defaults
desativado são recusados. Não se recompila MonoBehaviour nem shader Unity.

Recortes de takes definidos no `.meta` que não correspondem a um take nativo
exigem materialização explícita; não se escolhe uma animação por aproximação.
TIFF/TGA/FBX possuem contratos próprios já documentados. PSD e cenas `.unity`
continuam lacunas, além dos previews/miniaturas gerais exigidos por N07.

## Aceite host aprovado

Um cenário da biblioteca importa FBX e Standard reais, converte prefab com
fileID acima de 2^53, instancia o mesmo recurso duas vezes, converte/edit/reimporta
clipe, conserva override, desfaz/refaz receita, converte controller e reabre
recursos. As ações Instanciar e Vincular passam pelo roteador de toque em
655×300; o controller é vinculado ao Animator criado pelo comando de
componentes. Após reabrir, o Play deve mover somente a instância vinculada,
consumindo a curva convertida. Recusas de dependências e conflito devem conservar registro,
arquivo nativo, índice e cursor de histórico. O cenário foi executado com sucesso
na rodada integrada, junto dos demais19 cenários.
Ele também intercala edição/Undo autoral entre a reimportação e seu Undo/Redo,
para proteger o contrato de revisões monotônicas.

A Biblioteca oferece detalhes por item, Reimportar e as escolhas explícitas
Manter local/Receber origem. O rodapé usa uma única faixa de 48 dp e os
diagnósticos são paginados. O ícone `assets/source-conflict` está no atlas
nativo (339 entradas). `N07-unity-conflito-conceito.png` é hipótese gerada;
nenhuma imagem conceitual conta como captura da engine.

## Referências e decisões

[Unity 6000.0 — formato textual](https://docs.unity3d.com/6000.0/Documentation/Manual/FormatDescription.html):
objetos e relações por IDs exatos; adaptação: leitor limitado, com recusa
determinística antes da publicação.

[Unity 6000.0 — transições](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Transition.html):
condições, tempo, duração e interrupção possuem semântica própria; adaptação:
mapear somente contratos equivalentes do Animator existente.

[Godot 4.5 — importação avançada](https://docs.godotengine.org/en/4.5/tutorials/assets_pipeline/importing_3d_scenes/advanced_import_settings.html):
inspeção de sub-recursos e configuração contextual antes do uso; adaptação:
uma superfície focal da Biblioteca com diagnóstico e ações de origem/override,
preservando alvos de toque de 48 dp em paisagem.

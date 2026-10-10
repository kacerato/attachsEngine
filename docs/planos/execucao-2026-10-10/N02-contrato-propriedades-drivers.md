# N02 — contrato de propriedades e drivers

Estado: código em integração; o build e o cenário executável desta revisão ainda
não constituem aceite. N02 permanece parcial; UI e SDK tipados estão integrados
ao código, aguardando a rodada conjunta de execução e inspeção visual.

## Identidade e dados

AECLIP 7 preserva leitores 1–6. Trilhas gerais ficam separadas de TRS/morph para
não colidir com a chave otimizada `(objeto << 2) | AnimationPath`. O binding
relativo resolve o objeto; a trilha identifica instância local do componente,
tipo, versão e propriedade. Material também guarda slot e GUID da primitiva;
UI guarda GUID do documento e ID do nó. A substituição de um alvo produz recusa,
sem reutilizar um índice como se fosse sua identidade anterior.

O recurso distingue número, vetor 2/3, RGB linear, quaternion, booleano, enum,
objeto e GUID. Objetos e recursos nunca passam por float. Quaternion reutiliza
o sampler real; referências e opções mantêm chaves discretas. Reverter uma
trilha discreta também reverte a continuidade na fronteira das chaves.

Persistir um tipo não o torna elegível para qualquer componente. O catálogo
exige setter, capacidade, mutabilidade e ausência de reconstrução pesada por
quadro. Bool/enums exigem opt-in. Referências e quaternions de componente ainda
não têm contrato registrado de publicação e não aparecem como opções falsas.

## Consumidores e publicação

Intensidade/cor de luz e FOV usam os descritores existentes. Material publica
fatores reais por slot, exigindo override local ativado previamente. Opacidade
usa o lease vigente de Canvas e `GuiRuntime::setOpacity`. O compositor prepara
todos os candidatos, valida cada componente e só então publica. Falha em um
alvo não permite gravar o restante do lote. Play informa as invalidações dos
mesmos schemas usados pelos setters de scripts.

Preview possui cópia de SceneGraph e instâncias privadas de GuiRuntime, sem
scripts, física ou áudio. Cancelar destrói as cópias. Desenho de UI reutiliza
layout/draw e projeção de Canvas no mundo. O documento autoral e a autoridade
do material não precisam ser reconstruídos após sair da prévia.

## Drivers

O grafo acíclico compilado oferece source, constante, soma, multiplicação,
escala/bias, clamp, remap e curva. Limites: 512 nodes, 256 entradas, 128 saídas
e 8.192 chaves de curvas; os IDs pertencem ao mesmo alocador do clipe.
Entradas aceitam tempo do clipe, propriedade numérica elegível ou parâmetro
Float do Animator por ID e identidade do controlador. Saídas são numéricas;
não há conversão implícita de bool, enum ou referências.

A ordem é scripts → composição do clipe → drivers → física → extração.
Cada grafo lê a mesma composição do clipe antes da publicação. Saídas não são
entradas de outros drivers no mesmo quadro; feedback por propriedade é recusado.
Ciclos internos, IDs ausentes e overflow são diagnosticados antes de publicar.
Amostragem matemática usa scratch limitado; preparação de bindings e mistura
ainda alocam no consumidor, o que precisa de medição no N15.

## Autoria e aceite

NÃO IREI SER SIMPLISTA NO DESIGN.

Propriedades usam o destino de objeto existente e uma superfície contextual,
com catálogo, canais do clipe, valor no cursor, gravação e navegação de chaves.
Numéricos editam expressões; bool/enums apresentam seu significado. Valores,
tempos e remoções passam pelo mesmo journal de arquivos e Undo/Redo do clipe.
O cenário focado cobre luz, material, câmera e UI, toque em 655 × 300, preview
sem alterar fonte, reabertura, clone, setter inválido, lote quebrado e driver
Animator → escala/bias → intensidade. Passou no executável host na rodada final de 55/55 cenários de autoria.

Busca contextual, remapeamento com identidade preservada, edição individual de
curvas e autoria das oito operações do DAG agora usam os controles executáveis.
O grafo permanece em rascunho até Aplicar, que gera uma operação de Undo; Cancelar
descarta o rascunho. Entradas, saídas, valores e recusa de ciclos são inspecionáveis.
Esses caminhos passaram no cenário vertical integrado, incluindo recusa de ciclo e Play.

O SDK ABI 12 acrescenta catálogo, parâmetros Float do Animator, binding de objeto,
criação, gravação, amostragem e remapeamento tipados e publicação do DAG. GUIDs e
IDs atravessam a ABI sem float. O rascunho captura proprietário e revisão de cena;
alterar esse contexto exige reabrir o rascunho. A compilação gerenciada passou
com zero avisos e zero erros; a execução contra a DLL nativa ABI 12 passou: 1 cenário, zero falhas e zero pulos.

Capturas reais de propriedades e drivers em 655×300 foram inspecionadas: valores,
ações contextuais e resultado do DAG são legíveis. São controles CPU, sem prova
do renderer Vulkan. Permanecem pendentes combinações adicionais de
camadas, medição N15 e aceite Android/ADB. As referências de objeto/recurso e
quaternion permanecem tipos persistíveis sem setters de componente elegíveis;
não representam suporte de autoria desses componentes no catálogo.

## Referências concretas

- [Godot 4.5 — introdução à animação](https://docs.godotengine.org/en/4.5/tutorials/animation/introduction.html):
  descoberta de trilhas a partir da propriedade; adaptada à seleção contextual.
- [Godot 4.5 — Animation](https://docs.godotengine.org/en/4.5/classes/class_animation.html):
  distinção entre atualização contínua/discreta; Astra mantém identidade sem
  coerção e documenta sua própria regra de continuidade reversa.
- [Godot 4.5 — tipos de trilhas](https://docs.godotengine.org/en/4.5/tutorials/animation/animation_track_types.html):
  separação entre propriedades, TRS, morph e curvas; não se presume equivalência
  de eventos, áudio ou métodos só porque o recurso possui trilhas gerais.
- [Unity 6000.0 — EditorCurveBinding](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/EditorCurveBinding.html):
  caminho/tipo/propriedade como referência; Astra acrescenta identidade local e
  versão esperada para recusar componente substituído.
- [Godot — AnimationMixer, código oficial](https://github.com/godotengine/godot/blob/4.5/scene/animation/animation_mixer.cpp):
  referência de composição e cache de alvo; a implementação Astra preserva seu
  mixer existente e publica propriedades gerais em lote separado e validado.

As referências não são prova do comportamento da Astra. A imagem conceitual
permanece hipótese; a evidência deve vir da UI executável e dos consumidores.

A dependência de um canal material retém o GUID da primitiva no binding e registra
o arquivo importado proprietário como dependência de projeto, usando a mesma
regra dos prefabs. Primitivas ausentes recusam publicação; não são registradas
como arquivos fictícios. O cenário usa uma fonte GLB real, não um GUID inventado.

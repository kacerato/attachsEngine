# N03 — pacote de integração preparado

Estado em10/10/2026, durante o build7612: os novos arquivos abaixo estão
isolados e não incluídos no CMake. A verificação de sintaxe C++20 com Werror dos
três TUs novos (candidatos, overlay e readback) passou; não houve ligação,
execução nem exposição no app. Esta preparação não altera a baseline aceita.

## Donos e limites

`RigAsset` continua dono da descrição reutilizável; SceneGraph possui os objetos
reais; skin/bind pertencem ao modelo deformador. O mapa de juntas resolvidas é
derivado; a identidade do recurso, sua revisão, o pai e o placement da instância
precisam persistir no componente real da raiz. O editor conserva draft/seleção e publica pelo
journal/History existentes. Nenhum novo gerenciador de recursos ou journal.

A integração anexa AssetType Rig/RigPose/RetargetProfile após NavMesh15, mantendo
os valores antigos. Pose precisa de revisão própria: AERPOSE2 escreve a revisão,
leitor1 assume1, rigRevision permanece o contrato de compatibilidade. Edição e
Undo conservam revisão monotônica; identidade removida do rig não é reciclada.

Preparadores não escrevem: capturar rig da seleção, editar recurso com revisão
esperada, capturar pose com máscara, preparar perfil com duas revisões e mapa.
A validação produz candidato tipado, GUID, bytes, dependências, revisão/hash
esperados. O publicador recebe candidatos já preparados, para N08 incluir
Clip+Rig+Pose+Retarget em uma transação sem executar commits individuais em laço.

`editor/editor_rig_authoring_candidate.{h,cpp}` prepara os comandos tipados de
captura, criação/edição/remoção de controle, par/plano de espelho, cadeia,
referência mascarada, pose e perfil. Cada saída é adotada somente depois da
validação e compilação; o original não é modificado. A normalização do plano
divide normal e distância pelo mesmo comprimento, preservando seu lugar.
Criação aloca no frontier; editar controle conserva sua junta; remover controle
retira também os pares de espelho dependentes. Esses arquivos estão isolados,
sem ligação nem consumidor de UI nesta baseline. O envelope comum de
publicação permanece responsabilidade de N08, sem duplicação neste módulo.

Sufixos coordenados para integração: `.aerig`, `.aerpose`, `.aertgt`. N08 prepara
um variant fechado Clip/Rig/Pose/Retarget, CAS por GUID/revisão/hash e até32
recursos/64MiB retidos. Criação, edição e remoção devem coexistir em uma única
publicação pelo mesmo journal; não são32 commits em sequência.

## Placement e revalidação

`runtime/rig_placement.h` prepara a compensação no espaço do pai real:
`placement = raizInstância * inverse(referênciaRaiz)`. Leitura da raiz remove a
compensação; escrita recompõe. Filhos conservam TRS local. Máscaras são aplicadas
no espaço do recurso antes de recompor a raiz; isso preserva canais não editados
em uma instância rotacionada/escalada. Parent ID da raiz entra no binding para
recusar reparentamento inesperado. A mesma borda deve ser usada por preview,
pose e RetargetInstance, sem três implementações divergentes.

O mapa de transferência verifica parentesco nas duas direções: cadeias de
origem não podem virar ramos independentes no destino, e ramos independentes
não podem ser sobrepostos como ancestrais. Vínculos desaparecidos são reportados
por junta; aplicação que afeta uma junta inválida recusa antes de qualquer escrita.

## Autoria mobile

NÃO IREI SER SIMPLISTA NO DESIGN.

Entrada a partir da seleção articulada e do recurso, com viewport dominante.
Uma superfície alterna Controles/Espelho/Poses/Transferência. Seleção sobre a
hierarquia ou modelo é a mesma identidade de junta/controle. Campos são
paginados em48dp; ações são contextuais. Diagnósticos têm uma rota própria e não
competem permanentemente com o modelo. Poses e perfis são recursos reutilizáveis,
não estado privado de uma janela.

`editor/editor_rig_screen.{h,cpp}` prepara a superfície sem incluir o grande
header Session/Screen: snapshot de apresentação, ações por enum tipado e uma
folha lateral. Em655×300, a área do modelo mantém toda a altura; a folha usa
cabeçalho48, três linhas48, pager48 e ação48. A troca entre os cinco destinos
substitui a folha, em vez de empilhar abas e painéis. IDs de ação não truncam
identidades64 de juntas. Esta construção ainda está isolada, sem entrada ou
handler no app e sem captura executável; não conta como interface entregue.

`editor_rig_overlay` acumula matrizes parent-before-child e desenha somente
bindings vivos com a câmera real do editor. O picking do viewport usa quadrado
48px, distância/profundidade e desempate por ID; não ocupa milhares de regiões
UiInputRouter. O controle escolhido mostra limites hinge/cone-twist em torno
da orientação de referência e do pai avaliado. Limite da raiz exige placement
explícito. Isso é overlay de autoria, não geometria deformadora criada escondida.

`scene/rig_instance.h` e `runtime/rig_placement_binding.{h,cpp}` preparam essa
persistência sem registrar componente no menu nesta baseline. O modelo grava
GUID, revisão do rig, pai e TRS com precisão de round-trip; uma leitura inválida
preserva o valor anterior. O preparador resolve a hierarquia real antes de
produzir o componente e conserva sua identidade quando recebe uma cópia de
componente existente. A revalidação recusa revisão ou pai diferente. A definição
do descriptor, binding de recurso, schema, clonagem/remap, consumer e interface
entram juntos na janela N03. Não recomputar placement ao abrir a ferramenta:
isso incorporaria o movimento animado da raiz à base e alteraria poses após
salvar/reabrir. Rebind explícito é uma operação autoral, com History.

A identidade da junta raiz também é persistida. Revalidar uma revisão mais
recente conserva placement e movimento atual: não refaz `raiz*inverse(ref)`.
O preparador de revalidação recusa troca de identidade da raiz/pai e revisões
regressivas; juntas não raiz ausentes continuam issues locais no binding. Isso
permite editar um controle ou limite e renovar a revisão sem deslocar a
instância nem absorver animação na base. Estabelecer outra base é uma operação
distinta, explícita.

Sintaxe C++20 estrita do TU `rig_placement_binding.cpp` aprovada (processo57103,
saída0); o descriptor permanece intencionalmente sem definição/registro até a
janela de integração. Esta verificação não liga nem executa o componente.

## Thumbnail produzida pelo renderer

Referência: [Unity6000.0 AsyncGPUReadback](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Rendering.AsyncGPUReadback.html)
separa pedido de entrega, aceitando latência para evitar espera síncrona.
Adaptação: uma cópia bounded ligada à fence/lease existente; não reproduzir uma
API Unity nem introduzir uma espera da GPU no caminho de desenho.

A imagem conceitual não pode virar thumbnail da pose. O preview Vulkan atual
tem lease/target final, mas a leitura completa ainda não está integrada.
`platform/android/preview_thumbnail_readback.{h,cpp}` é preparação isolada para
copiar até512×512 RGBA8 do target após o passe de pós-processamento, restaurar
o layout e consumir somente após a fence da mesma submissão. Cancelar invalida
a entrega, conservando o buffer até a fence. A lease conserva epoch/revisões;
o editor deve recusá-la se o recurso/cena mudou antes da publicação.

Integração ainda necessária: target com TRANSFER_SRC, chamada após o passe,
registro da fence após submit, consumo/cancelamento no lifecycle do
renderer e callback de pixels à sessão. CookTexture/KTX2 existente produz o
recurso de imagem; Pose+Texture devem ser publicados juntos. Não existe raster
CPU substituto nem readback síncrono por frame neste contrato. A hipótese está
com sintaxe aprovada, mas sem ligação/validação Vulkan; não conta como capacidade
disponível.

A swapchain real espera e reseta a fence dentro de `acquireNextImage`
(`rhi/device.cpp`). O módulo oferece `takeAfterSubmissionWait` para consumir
depois da espera comprovada da mesma fence e antes de nova submissão: não
consulta uma fence cujo sinal já foi resetado nem adiciona espera da GPU.
`take` permanece a alternativa de polling antes do reset. Sintaxe estrita do
módulo após este ajuste passou; ainda falta a integração executável acima.

## Um aceite integrado

Para o consumer, [Godot4.5 SkeletonModifier3D](https://docs.godotengine.org/en/4.5/classes/class_skeletonmodifier3d.html)
e [Unity Animation Rigging1.2 — Rigging Workflow](https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.2/manual/RiggingWorkflow.html)
posicionam modificações depois da avaliação da animação. Na Astra, qualquer
integração automática deve respeitar a autoridade de Transform já consultada
pelo SceneAnimator; retarget não pode escrever um corpo possuído pela física.
Essa regra ainda precisa ser conectada ao runtime/SDK, além da avaliação C++.

Criar mecanismo de três juntas pela engine, capturar rig, configurar controle e
limite, criar espelho manual assimétrico, salvar pose e thumbnail, aplicar com
peso em outra instância deslocada/rotacionada, salvar e reabrir. Remover uma junta
e verificar diagnóstico localizado e recusa atômica. Criar mapa entre dois rigs
de repousos distintos, comparar poses no mesmo tempo, avaliar e fazer bake pelo
mesmo avaliador, usar clipe independente em Play. Undo/Redo deve restaurar
recursos/dependências/cena sem mudar revisões para trás. API usa os mesmos
candidatos e publicador. Captura host prova layout; imagem Vulkan/aparelho fica
distinta, sem ADB antes da liberação final do plano.

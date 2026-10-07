# Autoria física durável — aceite após o merge

Fechamento do bloco U01/U02/U04/U05/U09 em 2026-10-06, dentro dos contratos e orçamentos explícitos abaixo. Repositório confirmado `https://github.com/kacerato/attachsEngine.git`, branch `codex/gameplay-runtime`, base `6349ee60d799ea191d42b3d5c97934e454001d38`. Main e branch remotas coincidiam com essa base no início e no fim da revisão. Esta entrega está no checkout e no APK instalado; não registra novo commit/push. As alterações do usuário em renderer/RHI/shaders/plataforma Android foram preservadas por comparação com a base.

## Capacidade entregue

Autorar no objeto existente preserva visual, materiais, filhos, IDs e referências. Fontes tipadas por objeto/slot e suas matrizes relativas alimentam a colisão; autoridade própria em descendentes impede absorção silenciosa. A lista e o toque na superfície escolhem fontes sem trocar o Body selecionado. Primitivas, convexo e composto continuam distintos.

Agora Base/Atual/Clipe padrão selecionam geometria efetiva. Skin/blend shapes usam o deformer CPU e palette já usados pela renderização; clipes são amostrados pelo SceneAnimator numa cópia do documento. Partes, voxels, vértices, erro de volume, prazo e instante são editáveis pelo IME numérico real. CollisionRecipe v2 persiste os parâmetros; v1 migra como pose base, incluindo os Colliders de baseline. Os defaults, referências e fluxo estão no [plano técnico](../../../planos/ui-universal/BLOCO-AUTORIA-FISICA-2026-10-06.md).

Prévia e cancelamento não publicam recursos nem mutam a cena. Apply verifica novamente geometria/origens, correspondências, conflitos e cooking, publica recursos imutáveis e faz uma transação. A base nova e os overrides locais permanecem separados; remoções, campos alterados e malha física própria são preservados na regeneração opt-in após reimportação. O primeiro Apply de prefab leva as dependências Body/Motor ausentes junto da receita; componentes existentes conservam endereços independentes. Cache LRU de oito resultados usa projeto, geometria, origens e parâmetros completos; não guarda a entrada grande nem aceita um arquivo externo como resultado confiável.

Rebuild do mesmo Body conserva momento linear e angular a partir da massa e inércia reais do Jolt. Alterações autorais explícitas de velocidade têm precedência; as restrições e limites do solver continuam aplicados. A identidade do componente impede transferir movimento para um Body substituído.

## Gates separados

| Camada | Evidência | Resultado |
|---|---|---|
| Host/build | logs de build; contratos gerados atuais | native editor/solver e executável de aceite compilados |
| Regressão | `gui-regression.log` | 109/109 cenários |
| Aceite final direcionado | `focused-tests-final.log` | 9/9: quatro ampliações e cinco aceites de regeneração |
| Contratos | `component-contracts.txt` | 49 schemas, 576 bindings numéricos/bool, 567 opções de enums; round-trips; isto não é contagem de paridade |
| Android/build | `android-build.log` | Release, BUILD SUCCESSFUL |
| Instalação | `install.log`, hash do APK local e retirado da instalação | iguais: `df56401d0173757585628f5097c8eb1ca388c66654f1a23bf10d714a0e084c28` |
| Aparelho | POCO F7 / 25053PC47G, Android 16, ADB ativo | autoria, IME, prévia, Apply, Undo/Redo, Save e reabertura fria |
| Consumidor de arquivos do aparelho | `device-project-reopen.log`, `device-budget-reopen.log` | GLBs/hash/registro/arquivo reabertos e consultados no Jolt **do host** |

A correção de deadline durante preparação foi feita depois da regressão 109/109; o aceite final 9/9, o orçamento grande e o APK incluem essa correção. A mudança posterior do executável de aceite somente permite conferir a pose Atual, além de Clipe; não alterou a engine/APK.

O cenário de skin fechada deformou realmente um osso, gerou a forma e confirmou um hit no Jolt fora dos limites da base. Os demais aceites protegem blend shapes, instante de clipe, perda de ossos, isolamento do documento, invalidadores do cache, prefab vinculado, reimportação real, conflitos, recusa de publicação e migração de um arquivo v1 não vazio retirado do aparelho em revisão anterior. Mudança de massa/escala conserva os dois momentos; edições explícitas de velocidades prevalecem.

## Arquivos e histórico no aparelho

`Physical Pose 20261006`: autoria com 6 partes máximas, 10.000 voxels, 32 vértices, erro 0,75%, prazo 30s, Clipe padrão em 0,6s. A prévia deixou o arquivo intacto; um Undo recuperou exatamente o arquivo anterior; um Redo recuperou exatamente o aplicado. Depois da reabertura fria, esses valores aparecem na UI e no arquivo. Restaurar somente a receita e seus Colliders antigos no arquivo novo reproduz byte a byte o arquivo anterior: visual, skin, animação, Body/Motor, hierarquia e identidades não mudaram.

`Physical Budget 20261006`: malha fechada de 99.372 triângulos, sem Body/Collider inicial. Prévia com 400.000 voxels gerou um casco; a tentativa tardia de cancelar chegou após Ready e **não conta como aceite de cancelamento**. Outra tentativa com 350.000 voxels teve o mesmo resultado. Com 300.000 voxels, o pedido logo após Start produziu “Geração cancelada: a cena permanece intacta”, sem resultado/Apply. Arquivo e registro de assets são byte-idênticos aos anteriores; não havia arquivo Collision publicado. Uma nova geração com esses parâmetros passou, Apply criou as dependências e um casco, Undo/Redo são byte-exatos e reabertura fria preserva 8/300.000/64/1%/120s/Atual. Os dois projetos editáveis retirados do aparelho estão nesta pasta; caches de importação/código são derivados e não necessários ao verificador nativo.

| Arquivo | SHA256 |
|---|---|
| Pose anterior/prévia/Undo | `4a101bf636fb5a85a002a57696a4325863a715679a659951da75d4ef5f9e85aa` |
| Pose Apply/Redo/reaberto | `e04952bf12f7c5641497380b464da7d25f31e5ee3d324e1189bb737f2438d8b3` |
| Budget anterior/cancelado/Undo | `fc7645ce12cdcfcfdb80a730320480c1f3ff7edf32901b339514f963d2e0e3ef` |
| Budget Apply/Redo/reaberto | `2f70fd41a3356d20f9bdc696001c221103e1ccf40bca6e02390975cb1ef93f33` |

## Revisão visual real

NÃO IREI SER SIMPLISTA NO DESIGN. Cada PNG retido foi aberto e examinado individualmente. Não foi gravado vídeo nesta revisão, portanto não há alegação de análise frame a frame de um vídeo. A lista/hashes de todas as capturas está em `acceptance.json`.

As quatro capturas `advanced-*` são UI real executável em raster software: três páginas compactas de duas propriedades e uma superfície ampla de seis. Comprovam legibilidade, alcance dos campos/IME, navegação, instante desabilitado e ausência de alças durante configuração; não comprovam renderização Vulkan. As capturas `device-*` e as dos diretórios de sampling são do APK com viewport Vulkan no aparelho.

Sequência Pose 01–13: abertura, seleção, ações, setup, ajustes, IME, valores editados, rascunho, prévia, correspondência, Apply, reabertura e Play. O desenho em cunha no Edit vem dos pesos de morph autorais da fixture; o clipe retorna à forma cúbica no Play. Não foi feita uma comparação quantitativa de pixels entre GPU e deformer CPU. O Play observado é um smoke de execução, não aceite completo de locomoção/animação U06–U08.

Sequência Budget 14–24: abertura, setup, parâmetros, saída dos ajustes, geração, Ready, novo ajuste, Apply, reabertura e valores persistidos. `device-17-budget-start.png` ainda mostra a superfície de ajustes; o nome registra uma tentativa de Start, não prova geração. `device-20-budget-cancel.png` e `device-budget-cancel-run/after.png` mostram Ready após cancelamento tardio; só `device-budget-cancel-early/*` mostra cancelamento efetivo. `device-budget-ready-run/running.png` e `after.png` mostram geração e prévia do trabalho novo após cancelamento. Casco verde é candidato; casco branco no Inspector é a forma aplicada. Labels, estado, erro e ações permanecem alcançáveis sem alças competindo com o toque na seleção de fontes.

## Custo medido e limites preservados

Host Windows, probe nativo com build de desenvolvimento, geometria fechada subdividida de 99.372 triângulos, 400.000 voxels: Ready 18.089,5ms, um casco, saída 1.156 bytes, pico de working set **do processo inteiro** 151.293.952 bytes (~144,3 MiB). Cancelamento na preparação 16,2ms; durante voxelização 856,2ms; ambos sem resultado. Prazo configurado 1s foi recusado após **6.423,7ms**: a biblioteca termina uma fase interna antes do callback. Esse excesso está preservado no JSON, não escondido por uma média.

Android com 300.000 voxels: máximo **amostrado** de PSS do editor inteiro 401.841 KiB (~392,4 MiB). Não é pico absoluto do allocator nem memória exclusiva do bake. As amostras e screenshots estão em `device-budget-ready-run`. A duração do script inclui ADB, dumpsys e screenshots; não mede exatamente a latência nativa do bake. O processo mantém caches/allocators após Ready, sem promessa de devolução imediata de todo o working set.

Entrada até 100.000 triângulos / 128 fontes; resultado até 32 partes / 64 vértices; até 400.000 voxels; captura de pose até 300.000 vértices. A geometria precisa ser fechada e orientada. Escala negativa autoral permanece recusada pelo Transform. Erro de volume não garante distância/cavidades arbitrárias; o autor revisa a forma. O cenário grande é convexo, embora subdividido: os aceites côncavos com cavidade pertencem aos cenários de regeneração, não a esta medição grande. Não há campanha thermal, paridade universal de concavidades nem prova em aparelhos de entrada nesta revisão.

Pose de colisão é captura estática. Animação não recooka colisão por frame. Deadline/cancelamento são cooperativos. A ampliação de formatos, orçamentos e políticas além desses contratos exige novos consumidores/aceites; não é suporte implícito.

## Fechamento e continuidade

Os critérios do bloco de autoria física durável estão aceitos. O roadmap UI original permanece byte-idêntico, com universo e pendências próprios. Continuam os blocos U03/U06/U07/U08 (locomoção/representação), U10/R0–R12 (UI), editor visual completo de malha/ProBuilder e U12/U13/U14 (SDK/reprodução/rede/custo amplo). U11 tem seu fechamento histórico próprio. Não há declaração de engine completa.

Verificação reproduzível: `python tools/validation/verify-physical-authoring.py`. O registro vincula source, arquivos do aparelho, logs, capturas e limites; hashes verificam integridade, não substituem a inspeção visual descrita acima. A tentativa de remover caches duplicados das cópias de evidência foi rejeitada pela revisão automática com “blocked by policy”, sem motivo detalhado; arquivos foram mantidos.

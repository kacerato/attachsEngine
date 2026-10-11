# N03 — rig autorável, poses e transferência de movimento

Estado em 10/10/2026: recurso/matemática/consumer nativos integrados ao CMake; sintaxe estrita aprovada. Autoria no editor, publicação dos três recursos, comparação visual e aceite executável ainda em integração. Nenhuma evidência física Android; ADB continua bloqueado até o fim do plano.

## Referências e decisões

- [Godot 4.5 — Retargeting 3D Skeletons](https://docs.godotengine.org/en/4.5/tutorials/assets_pipeline/retargeting_3d_skeletons.html): nomes iguais não compensam repousos diferentes; mapas manuais, validação de parentesco e escala de movimento são dados reais do sistema. Astra mantém nomes livres e correspondências por IDs.
- [Godot 4.5-stable — RetargetModifier3D](https://github.com/godotengine/godot/blob/4.5-stable/scene/3d/retarget_modifier_3d.cpp): repousos e bases anteriores/posteriores pertencem a um cache invalidado pela topologia. Astra compila a orientação global de referência e exige as duas revisões do perfil; não reinterpreta arquivos no quadro.
- [Unity 6000.0 — Retargeting](https://docs.unity3d.com/6000.0/Documentation/Manual/Retargeting.html): a transferência humanoide depende de um Avatar configurado. Isso não demonstra transferência genérica entre quaisquer mecanismos; Astra não usa esse nome como promessa de suporte universal.
- [Unity Animation Rigging 1.2.1 — Rigging Workflow](https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.2/manual/RiggingWorkflow.html): controles e visualização de autoria têm responsabilidade diferente do esqueleto deformador. A ordem de avaliação é explícita. O [vídeo indicado pelo próprio manual](https://youtu.be/DzW_jQGO1dc) é referência de workflow; sua indicação foi localizada, mas o vídeo não foi assistido nesta etapa.
- UMotion local, `manual-texto/RigHierarchy.txt` e `MirrorMapping.txt`: espelho é bidirecional; sugestões podem falhar e precisam de edição manual. Juntas customizadas não atualizam correspondências por magia.
- FinalIK 2.2 local, `Shared Scripts/BipedReferences.cs`: referências anatômicas são verificadas por presença e relações; uma lista de nomes não substitui vínculos válidos. Fontes privadas foram apenas estudadas; nenhum payload foi publicado nem transplantado.

## Cadeia real

```text
SceneGraph / Transform / ImportLink / caminhos portáteis
  -> captura de RigAsset
  -> validação / compilação parent-before-child
  -> controle FK, limite, espelho ou pose mascarada
  -> candidato de alterações de cena
  -> History / journal de recurso no editor
  -> serialização / reabertura

Rig origem + Rig destino + RetargetProfile + clipe TRS
  -> CompiledRetarget + CompiledRigAnimation
  -> evaluateRetarget
       -> RetargetInstance / SceneGraph
       -> bakeRetarget / clipe independente / N01
       -> preview isolado / diagnóstico / comparação
```

O rig referencia transformações normais. Ele não muda a ordem das juntas da skin, as matrizes inversas de bind, os pesos ou a geometria importada. Não cria personagem, câmera, cilindro ou malha invisível.

## Dados e invariantes

`RigAsset` guarda GUID, revisão, fronteira monotônica de IDs de 64 bits, juntas, controles, cadeias contíguas e pares manuais de espelho. Junta guarda pai, caminho relativo portátil, identidade opcional de nó importado, bind capturado e referência de autoria separada. Referência começa igual ao bind; editá-la não altera a skin nem a cena automaticamente.

O formato inicial aceita escala uniforme positiva. Escala singular, reflexão e escala não uniforme são recusadas explicitamente; não há decomposição que esconda shear. Validação cobre raiz única, ciclos, referências ausentes, IDs/caminhos duplicados, parentesco, segmentos degenerados e orçamento de leitura. Novos IDs não podem reutilizar identidades removidas.

Controle define canais efetivos, espaço de referência local ou espaço do rig e limite livre/dobradiça/cone-twist. Limites são medidos em torno da orientação de referência. Cadeias apenas descrevem juntas contíguas neste pacote; CCD/FABRIK e constraints ordenadas continuam dependências de N04, sem menu antecipado.

`RigPose` guarda GUID próprio, rig/revisão, nome, thumbnail opcional por GUID de recurso e entradas mascaradas por ID de junta. Aplicação ponderada usa arco quaternion, preserva canais fora da máscara e aplica limites. Revisão divergente exige revalidação explícita. Ainda falta produzir a thumbnail real pela captura do viewport e publicar o recurso no editor.

`RetargetProfile` referencia duas revisões, mapa um a um, orientação frontal, escala de movimento, correção quaternion por correspondência e políticas tipadas. Preservar referência local de translação é o padrão de membros. Outras opções são offset preservado, offset escalado e reconstrução de posição global; reconstruir posição não promete solução IK nem preservação de comprimento. Escala pode preservar referência ou aplicar a razão uniforme da origem. Juntas do destino sem mapa mantêm referência; canais animados da origem sem mapa são recusados, salvo descarte explicitamente autorizado.

## Matemática compartilhada

Para rotações globais de origem `S(t)`, referência `S0`, referência destino `T0` e correção de frente/cadeia `C`, a orientação desejada é:

```text
D = S(t) * inverse(S0)
T(t) = C * D * inverse(C) * T0
localDestino(t) = inverse(paiDestino(t)) * T(t)
```

Juntas são avaliadas com pais antes dos filhos. A postura de referência é preservada quando `D` é identidade; comprimento do destino não é substituído pela translação absoluta da origem. O avaliador não aloca e prepara seus buffers antes da execução.

Espelho em modo referência reflete o delta de posição/orientação em espaço do rig e o aplica à referência do lado correspondente, preservando assimetria. Modo absoluto usa o plano `dot(normal,p)=offset`. A orientação refletida usa a conjugação da rotação pela reflexão; não é uma negação arbitrária dos ângulos Euler. Canais selecionados e canais habilitados nos dois controles limitam a operação.

O mesmo avaliador gera o bake 30/60/120 Hz, com orçamento agregado e cancelamento entre amostras. Fonte permanece intacta. Morphs, propriedades tipadas e drivers sem correspondência própria são recusados; não desaparecem silenciosamente. A saída conserva cues/regiões e vira um clipe TRS independente.

O workspace retém a cadeia ideal e a cadeia limitada, erros posicionais/angulares por junta, máximos e quantidade de juntas limitadas. Esses dados são reais; os overlays de diagnóstico ainda precisam ser ligados à UI.

## Placement e lifecycle em integração

A raiz de outra instância pode ter posição/orientação diferentes. A ligação precisa guardar a compensação `placement = raizInstância * inverse(referênciaRaiz)` e aplicá-la na borda da cena; o recurso permanece no próprio referencial. Essa correção foi identificada na revisão e será integrada após o build conjunto corrente. Não considerar aplicação em outra instância aceita antes dela.

IDs de objetos não são reciclados. A ligação verifica existência e parentesco antes de escrever qualquer junta; remoção invalida os vínculos afetados. Um frame inválido conserva a cena anterior. Preview terá cópia isolada e descartável, sem lista frágil de restauração da cena autoral. O editor publica alterações por History; o consumer runtime não depende de janela aberta.

## Design mobile

NÃO IREI SER SIMPLISTA NO DESIGN.

Perfil `inside-design-system` da skill `avoid-ai-design`: manter tokens/fontes e os ícones angulares Astra; não introduzir tema novo, cards ou chrome decorativo. Paleta funcional: canvas `#191C21`, superfície `#242830`, linha `#414954`, texto `#EEF1F5`, seleção `#70ACF5` e âmbar de limite/diagnóstico. Eixos usam as cores semânticas já existentes.

O ganho estrutural é transformar Rig em modo contextual: viewport dominante com controles selecionáveis, uma superfície que alterna Controles/Espelho/Poses/Transferência. Seleção sobre o modelo ou hierarquia envia apenas os campos pertinentes. Altura útil de 300 dp exige alvos de 48 dp e paginação de propriedades, sem esconder a imagem para caber uma lista completa.

Hipótese gerada pelo tool nativo de imagem: `N03-rig-conceito.png`. Prompt: editor Astra em paisagem 655×300 lógicos, braço mecânico de três juntas, viewport 68%, superfície contextual 32%, seleção de acoplamento, espaço de referência e limite de dobradiça, paleta existente, sem cards/neon ou recursos IK inventados. Revisão visual: a imagem preserva o modelo e distingue seleção/limite, mas expõe controles demais para garantir 48 dp na altura mínima. A implementação precisa paginar campos e ações, não copiar essa densidade. O braço ilustrado é arte conceitual, não asset já importado nem captura da engine.

Comparação origem/destino é sob demanda sobre a mesma cena avaliada, no mesmo tempo. Deslocamento lateral ou sobreposição é estado de visualização; não grava transforms na fonte. Nada de segundo renderer fake. A imagem gerada é hipótese; a captura posterior precisa provar legibilidade, criação, edição, erros e disponibilidade das ações.

## Arquivos e evidência atual

- `native/resources/rig_asset.{h,cpp}`: formatos AERIG1/AERPOSE1/AERTGT1, validação e edição transacional em memória.
- `native/resources/rig_math.cpp`: composição TRS uniforme, reflexão/rotação compartilhada e limites.
- `native/resources/rig_evaluation.{h,cpp}`: compilação, FK, espelho, pose, samplers, retarget e bake.
- `native/runtime/rig_instance.{h,cpp}`: captura/binding, candidato de cena e instância runtime.
- `tests/native/test_rig_authoring.cpp`: dois cenários integrados; preparados, **não executados**.
- CMake: fontes nos módulos reais e alvo `aether_rig_authoring_tests`.
- Sintaxe estrita dos cinco TUs: processo 44591, saída 0; não equivale a build/link ou aceite.

Preparação posterior ao congelamento N07/7612, ainda **sem inclusão CMake nem
consumidor do editor**: `runtime/rig_placement.h` contém conversões validadas da
raiz entre recurso e instância; `editor/editor_rig_overlay.{h,cpp}` acumula
matrizes da hierarquia real uma vez e projeta controles/limites de dobradiça e
cone-twist com a mesma câmera do viewport. O picking oferece alvo de48px e
prioridade por distância/profundidade sem registrar milhares de regiões do
roteador. Sua integração ainda deve acontecer junto de bindings, preview,
autoria, History e API. Arquivos preparados não são recursos disponíveis no app.

Na próxima janela também será renumerada a faixa `LibraryItemDetailsBase`, que
se sobrepõe aos controles FBX. Os caminhos atuais são exclusivos: o builder da
Biblioteca mostra importador **ou** lista; o handler de importação exige
`importPanel` e consome seus IDs antes do handler da lista. A rodada atual
exercita as duas rotas. Não foi identificada falha funcional causada por essa
sobreposição; a renumeração preservará a unicidade para evolução do editor.

Aceite pendente: mecânico de três juntas editável pela UI, humanoide sem nomes convencionais, pose em outra instância/reabertura, mapa manual, remoção localizada, Walk entre duas fontes reais de repousos diferentes, proporcionalidade, loop/twists/erros quantificados, publicação/Undo/Redo/API e capturas host. Dados de rigs importados disponíveis em N07 não fecham esses requisitos.

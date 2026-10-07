# Implementação em grandes blocos — attachsEngine

Pedido: fechar as pendências U01–U10 e ampliar a autoria de malha visual. O universo continua sendo o roadmap original preservado e a extensão de objetos/colisão/controle; esta ordem não elimina requisitos. Repositório confirmado: https://github.com/kacerato/attachsEngine.git, branch codex/gameplay-runtime. U11 anterior tem evidência própria; modelagem visual completa é ampliação nova, ainda não aceita.

## Ordem por dependência

1. **Autoria física durável — U01/U02/U04/U05/U09: fechado dentro dos contratos publicados.** Receita persistente por instância, fontes tipadas/toque no viewport, parâmetros numéricos efetivos, skin/blend shapes e captura de clipe, correspondência de partes, merge base/local/nova, recursos imutáveis, reimportação opt-in, prefab, histórico, cancelamento e prévia real. Skin/pose têm cadeia própria implementada e aceita; ampliar os limites além de 100 mil triângulos/128 fontes/32 partes/64 vértices/400 mil voxels não fica implicitamente suportado. Pose física é snapshot estático; prazo é cooperativo.
2. **Locomoção e representação — U03/U06/U07/U08.** Apoio por forma/contato, degraus/agachar, plataformas, fontes de intenção e arbitragem com foco/lifecycle, orientação visual e animação/root motion. Água, voo e veículos dependem de consumidores físicos adequados e terão critérios distintos, dentro deste bloco.
3. **UI componível — U10 / R0–R12.** Contratos e migração, instâncias, skins/recursos, entrada/foco, layout, texto/IME/idiomas, bindings, coleções, formulários/inventário, composição espacial, animação, extensões/exportação, acessibilidade e escala. Cada requisito do arquivo original permanece obrigatório; o inventário de progresso não substitui o texto.
4. **Autoria de malha visual.** Modelo de topologia com arestas/faces/vértices, seleção e edição no viewport, operações de modelagem, UVs, slots/material, normais/tangentes, recursos e instâncias, prefab/reimportação, colisão e custo mobile. Referência de capacidade: Unity ProBuilder 6.0.9. Sem declarar paridade por reutilizar a edição física de U11.
5. **SDK, reprodução/rede e orçamento — U12/U13/U14.** Contratos e operações seguras, snapshots/posse/correção, limites de determinismo e medições representativas; execução acompanha a estabilização dos blocos anteriores.

## Bloco em execução: receita e regeneração

Fonte + slots + geometria/pose → worker V-HACD → revisão candidata → correspondência de partes → merge base/local/nova → preflight Jolt → registro/publicação imutável → uma transação de cena → arquivo/reabertura → prefab/instância.

A receita pertence ao objeto, com referências enumeráveis que seguem duplicação/prefab. A física executa os Colliders publicados, não a receita nem o worker. A nova base é guardada junto do resultado para uma segunda regeneração não reinterpretar o override como valor herdado. Remoções locais são alterações, não pedidos para recriar componentes silenciosamente. Correspondência espacial é sugestão; ambiguidade precisa de escolha explícita. Recursos ausentes e resultados obsoletos impedem Apply antes da publicação.

NÃO IREI SER SIMPLISTA NO DESIGN. Reutilizar a superfície contextual de colisão, com o viewport dominante. Identidade da parte, vínculo com a revisão anterior e retenção de edição aparecem junto da prévia paginada; nenhuma janela permanente nova. Estados: geração, cancelado, desatualizado, correspondência exata/sugerida/manual, conflito, edição preservada, parte removida e erro de publicação.

Referências estudadas: [Godot 4.5 Import process](https://docs.godotengine.org/en/4.5/tutorials/assets_pipeline/import_process.html), [importador de cenas 4.5-stable](https://github.com/godotengine/godot/blob/4.5-stable/editor/import/3d/resource_importer_scene.cpp), [Unity 6000.0 prefab overrides](https://docs.unity3d.com/6000.0/Documentation/Manual/PrefabInstanceOverrides.html). Princípios: parâmetros persistentes por recurso, fonte separada do derivado, base distinta da edição local e reimportação explícita. A adaptação usa AssetRegistry, import journal, ComponentType, Prefab e EditorHistory reais já existentes.

Aceite: decompor malhas filhas → editar pose/estado/recurso de uma parte e remover outra → mudar uma fonte → regenerar sem mutar a cena → revisar correspondência → Apply preserva UIDs/edições/remoções e Body/visual → um Undo/Redo exato → salvar/reabrir → instanciar prefab e regenerar só a instância → consultas Jolt distinguem parede/cavidade. Conferir erro/staleness/publicação recusada e UI compacta no executável; build/instalação/aceite Android são gates separados.

Estado do primeiro fechamento: **aplicado e aceito em 2026-10-06**, no recorte de receita/regeneração. Cinco cenários direcionados e regressão GUI 105/105; Apply, paginação de todas as partes, um Undo/Redo byte-exatos e reabertura no Android físico; arquivo/GLBs retirados do aparelho reabertos e consultados no Jolt do host. O Apply de prefab publica a revisão física completa e preserva os endereços independentes. Não marcar U05/U09 completos por este recorte. [Relatório e limites](../../validacao/ui-universal-2026-10-06/collision-regeneration/REPORT.md); [registro verificável](../../validacao/ui-universal-2026-10-06/collision-regeneration/acceptance.json).

## Fechamento do bloco de autoria física após o merge

Base preservada `6349ee60d799ea191d42b3d5c97934e454001d38`. O parágrafo anterior é o recorte histórico; esta revisão acrescenta as cadeias que faltavam ao bloco: captura real de skin/morph/clipe e migração v1→v2, cache bounded com invalidadores, seleção de fontes no viewport, parâmetros/IME/paginação compacta, primeira autoria em prefab vinculado com dependências atômicas e conservação de momentum na reconstrução do mesmo Body. Não elimina os requisitos dos demais blocos.

**Bloco 1 aceito:** 109/109 na regressão, 9/9 aceites finais, Android Release compilado/instalado com hash conferido. Autoria/Save/Undo/Redo/reabertura fria em duas cenas no POCO F7; cancelamento real sem mudança de cena/registro; entrada de 99.372 triângulos medida no host e aparelho. Arquivos retirados do Android reabertos/consultados no Jolt do host, sem atribuir essa query ao aparelho. UI compacta executável e capturas físicas examinadas individualmente; nenhum vídeo gravado nesta revisão. [Plano e contratos](BLOCO-AUTORIA-FISICA-2026-10-06.md), [relatório integral](../../validacao/ui-universal-2026-10-06/physical-authoring/REPORT.md), [registro verificável](../../validacao/ui-universal-2026-10-06/physical-authoring/acceptance.json).

Deadline de 1s excedeu uma fase interna e foi recusado após 6,4s: é cooperativo, não tempo real. Erro de volume não é garantia universal de cavidades. Os limites e a natureza estática da captura são contrato, não stubs silenciosos. U14 ainda exige campanha de custo/thermal mais ampla. Os próximos blocos permanecem 2, 3, 4 e 5 acima. O roadmap UI original permanece intacto. Mudanças locais e APK instalado nesta revisão; sem novo commit/push.

## U07 fechado dentro do bloco de locomoção — 07/10/2026

As cinco fontes possuem arbitragem por motor, exclusividade, prioridades, cancelamento
e consumidor físico real. Política passa por Inspector, arquivo, prefab/overrides e
histórico; C# envia/libera Script/IA e observa o passo consumido. A revisão preserva
a câmera virtual/Cérebro do merge 4081cad9. [Plano U07](U07-POSSE-CONTROLE-2026-10-07.md)
e [aceite](../../validacao/u07-2026-10-07/REPORT.md): 8/8 direcionados, 117/117
regressões, ProjectCompiler, Android instalado/hash igual, UI/Script/IA/prioridades,
pausa/retomada, salvar/desfazer/refazer e reabertura fria no POCO F7. Gamepad e
desconexão no host, sem gamepad físico no Android. Os 200 quadros da nova gravação
foram examinados; órbita cruza 90° sem inversão e Body/colisão/rig giram juntos na
direção da câmera. O laboratório editável possui dois rigs, clipes, piso PBR e
atmosfera, sem substituir o objeto por um cilindro. Isso fecha U07, não todo o bloco
2: U03/U06/U08 continuam separados, assim como UI componível, modelagem visual
completa e SDK/rede/custo amplo. O roadmap UI original permanece intacto.

## Correção integrada: movimento, animação e câmera — 07/10/2026

A revisão posterior substitui a amostra de um clipe/CesiumMan por oito clipes
Godot TPS e movimento medido, controle aéreo real, câmera virtual/Cérebro com
varredura contra piso e catálogo Current antes de Play. APK embute só o laboratório.
12/12 direcionados, 121/121 regressões no reteste, ProjectCompiler 1/1, ProjectStore
7/7; 527 quadros examinados, principal atualizado e projetos do usuário preservados.
Os parágrafos acima ficam históricos. [Contrato e limites](U07-MOVIMENTO-ANIMACAO-CAMERA-2026-10-07.md)
e [evidência atual](../../validacao/u07-2026-10-07/REPORT.md). U08 amplo e os outros
blocos não são declarados completos por esta correção.

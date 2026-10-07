# Bloco de autoria física durável — revisão após o merge

Alvo confirmado: `https://github.com/kacerato/attachsEngine.git`, branch `codex/gameplay-runtime`, base `6349ee60d799ea191d42b3d5c97934e454001d38`, igual à main remota no início da revisão. O bloco conserva U01/U02/U04/U05/U09 do plano de blocos integrados. Não redefine o roadmap UI original, não incorpora U03/U06–U08, não anuncia modelagem visual ProBuilder nem conclusão da engine.

## Decisão e referências

NÃO VOU IMPLEMENTAR DEPENDÊNCIAS DE FORMA CENOGRÁFICA. A fonte visual continua independente da autoridade física. Skin e blend shapes usam os mesmos dados, qualidade de influências, palette e avaliador CPU da renderização. A amostragem de clipes usa o SceneAnimator real sobre uma cópia da cena; scripts, física e o documento autoral não avançam.

- Unity **6000.0**, [SkinnedMeshRenderer.BakeMesh](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/SkinnedMeshRenderer.BakeMesh.html) e [bindings oficiais](https://github.com/Unity-Technologies/UnityCsReference/blob/6000.0/Runtime/Export/Graphics/GraphicsRenderers.bindings.cs): capturar uma pose em CPU como geometria independente. A adaptação reaproveita o deformer existente e aplica as matrizes afins reais até o Body; não adiciona uma skin física fictícia.
- Godot **4.5**, [Collision shapes (3D)](https://docs.godotengine.org/en/4.5/tutorials/physics/collision_shapes_3d.html), [MeshConvexDecompositionSettings](https://docs.godotengine.org/en/4.5/classes/class_meshconvexdecompositionsettings.html) e [editor 4.5-stable](https://github.com/godotengine/godot/blob/4.5-stable/editor/scene/3d/mesh_instance_3d_editor_plugin.cpp): geração contextual, parâmetros efetivos e separação entre malha e formas. Os parâmetros da Godot não são aliases dos do V-HACD 4.
- Unity **6000.0**, [prefab overrides](https://docs.unity3d.com/6000.0/Documentation/Manual/PrefabInstanceOverrides.html): edição da instância separada da fonte. A primeira revisão de colisão publica junto os Body/Motor que faltavam, usando os conflitos, remapeamento e preflight já existentes; não sobrescreve componentes homônimos da fonte.
- Jolt **5.6.0**, versão confirmada no source incluído, [MotionProperties](https://jrouwe.github.io/JoltPhysicsDocs/5.6.0/_motion_properties_8h_source.html): massa e tensor de inércia reais. Na reconstrução, `p = m·v` e `L = I_world·ω` são capturados e restaurados pela nova massa/inércia para a mesma identidade de Body. Edição explícita das velocidades tem precedência. Travas de graus de liberdade e limites do solver continuam aplicados.

## Cadeia

Objeto/slot/GUID + hierarquia + pose → geometria CPU → hash e origens → worker V-HACD → prévia → correspondência das partes → merge base/local/nova → preflight Jolt → recursos imutáveis e registro → transação única → arquivo/prefab → consumidor físico.

O cache LRU guarda até oito resultados validados, limitado pelas 32 partes/64 vértices por casco. Chave: projeto, parâmetros completos, geometria exata e origens completas. Identidade, matriz, slot, pose, tempo ou geometria diferentes impedem reutilização. Reimportação não aplica colisão automaticamente. Apply recolhe a geometria novamente e mantém os gates de recurso ausente, staleness, autoridade, conflito e publicação.

CollisionRecipe v2 persiste política de pose e instante. Arquivos v1 continuam interpretados como geometria base, incluindo os baselines de Collider com suas versões próprias. A geração não muta malha visual, materiais, ossos, filhos ou referências. A colisão publicada é uma captura estática; seguir uma animação continuamente exige outra política de simulação, não recooking escondido por frame.

## Workflow

NÃO IREI SER SIMPLISTA NO DESIGN. Ações do objeto → Configurar locomoção → Decompor → Fontes/Ajustes → Gerar → Revisar → Aplicar. Toque na malha alterna seu slot sem perder a seleção do Body. A lista permite escolhas precisas e mostra impedimentos. Os ajustes ocupam a superfície contextual existente; o viewport permanece disponível. Não há painel permanente nem nova coleção de cards.

Base/Atual/Clipe padrão têm comportamento distinto. O instante só é editável no modo Clipe padrão. Partes, voxels, vértices, erro de volume e prazo têm limites e entrada numérica real. Em 800×400 são duas propriedades por página; em superfícies maiores, seis. IME inválido não altera o rascunho. O documento muda somente no Apply. Alças de edição da forma ficam ocultas enquanto o autor escolhe fontes ou ajusta o bake.

## Limites e aceite

Entrada até 100 mil triângulos, 128 fontes, 32 partes, 64 vértices por casco, 400 mil voxels; captura deformada até 300 mil vértices. Geometria aberta/não orientada, ossos ausentes, transformações singulares e canais sem destino recebem erro. O erro percentual de volume não garante tolerância de distância nem toda cavidade; a prévia permanece necessária. Escala negativa autoral continua recusada pelo contrato de Transform existente.

O prazo é cooperativo, conferido na preparação e nos callbacks. Uma etapa interna da biblioteca pode ultrapassá-lo antes de devolver controle. Cena e recursos não recebem resultado cancelado ou expirado. O relatório deve conservar essa duração medida, sem transformar o prazo em garantia de tempo real.

Aceites: malha fechada com skin deformada alcança o Jolt fora dos limites da base; blend shape e instante do clipe alteram o hash/forma sem modificar o documento; configuração compacta por ponteiro/IME persiste; arquivo v1 migra; primeiro bake em prefab conserva vínculos e publica dependências; reimportação e overrides continuam protegidos; mudança de massa/escala conserva momentum. Completar build, provas host, capturas executáveis, instalação e autoria/reabertura no Android antes de registrar fechamento. Evidências desta revisão: `docs/validacao/ui-universal-2026-10-06/physical-authoring/`.

## Aceite concluído

Bloco de autoria física durável fechado nos contratos acima: regressão 109/109; aceites finais 9/9; Release Android compilado e instalado; duas cenas autoradas/salvas/reabertas no POCO F7, histórico byte-exato e cancelamento real sem publicação. Skin fechada deformada e arquivos do Android chegam ao Jolt do host. O cenário grande de 99.372 triângulos passou; o prazo de 1s foi recusado após 6.423,7ms, e o máximo amostrado de PSS do editor Android foi 401.841 KiB. Esses números não são garantia thermal nem pico exclusivo do worker. [Relatório](../../validacao/ui-universal-2026-10-06/physical-authoring/REPORT.md) e [manifesto](../../validacao/ui-universal-2026-10-06/physical-authoring/acceptance.json) preservam evidências e limitações; o roadmap UI e os outros blocos permanecem pendentes.

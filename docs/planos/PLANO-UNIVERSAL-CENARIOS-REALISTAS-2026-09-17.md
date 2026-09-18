# Astra — plano universal de cenários realistas e autoria gráfica

**Data:** 17/09/2026. **Natureza:** arquitetura e sequência de implementação; este documento não declara recursos novos implementados ou validados. **Referência externa principal:** Unity 6.0 (6000.0), distinguindo os controles gerais dos específicos da URP. **Integração:** [P02 — presets de componentes](P02-PRESETS-COMPONENTES-2026-09-17.md), [plano de universalidade](PLANO-UNIVERSALIDADE-COMPONENTES-LAYOUT-2026-09-15.md), [plano mestre M08–M14](PLANO-MESTRE-ASTRA-EDITOR-RUNTIME-ASSETS.md), [plano de renderização](../PLANO-RECONSTRUCAO-RENDERIZACAO.md) e [plano mobile](../PLANO-ENGINE-MOBILE.md).

## 1. Resultado pretendido e limite honesto

O objetivo é permitir criar **diversos** mundos 3D convincentes no próprio editor: importar fontes de qualidade, preservar seus objetos, tratar materiais e UV, compor iluminação e ambiente, atribuir comportamento, escolher qualidade por aparelho e medir o resultado. Uma demo bonita é evidência de uso da plataforma, não o lugar onde as regras gráficas vivem. Não há um comando universal que converta uma fonte sem detalhe, mapas PBR ou UV adequadas em um mundo fotorealista. O sistema deve diagnosticar essa falta e deixar o autor substituir ou enriquecer a fonte.

**Definição de pronto por função:** dados autoráveis e versionados; apresentação no editor; API de edição e de leitura; consumidor efetivo no runtime/renderer; dependências e capacidade; save/reopen/reimport/Undo; diagnóstico; desempenho; evidência no aparelho após autorização. Sem consumidor, o controle permanece *especificado*, não “implementado”. A matriz por propriedade segue o formato obrigatório da seção 9.3 do [plano de universalidade](PLANO-UNIVERSALIDADE-COMPONENTES-LAYOUT-2026-09-15.md).

**Estado de partida confirmado no código/documentação nesta data:**

| Base existente | O que já permite | Limite que orienta este plano |
|---|---|---|
| `component_schema.h` e `component_properties.h` | Schema de 9 tipos, requisitos no mesmo objeto, composição transacional, propriedades numéricas/booleanas/enums/referência de objeto e triplas | Recursos, serviço/fase e capacidade ainda não formam um contrato de dependências completo; valores ricos ainda são parciais |
| P02 | Presets persistentes para 8 tipos nativos, prévia, aplicar/adicionar, identidade de malha, Undo de cena | Não há presets compostos autorais, aplicação por campo, referências C# portáteis, remapeamento entre projetos nem registro universal de dependências da biblioteca |
| `ImportProfile` | Escala uniforme e dimensão máxima de textura, separados dos limites do dispositivo | Não cobre normal/tangente/UV/LOD/colisão/material/rig por fonte ou por nó |
| `MeshRenderer`/`MaterialAsset` e R4 | Malha por GUID, slots, material compartilhado e override, bindings, canais e transformação UV por binding | O caminho indireto, sombras, sampler sem bindless e histórico de recurso têm limites documentados em R4; não declarar equivalência integral entre todos os caminhos |
| `Light`/luzes pontuais | Direcional, point e spot com cor, intensidade, alcance/cone; cascatas do sol | Sombras locais ainda não existem no componente; limite atual de 8 luzes pontuais em `punctual_lights.h` |
| `ResolvedRenderingPolicy` | Eixos globais independentes de sombra, ambiente, pós, textura, LOD, resolução, AA e budgets, com degradação reportável | Um eixo configurável não prova que toda técnica correspondente ou todo fluxo de autoria já existe |

## 2. Caminho de uma engine grande, traduzido para a Astra

Na Unity, o autor escolhe o modelo no Project para ajustar importação; edita o material e suas texturas; ajusta Mesh Renderer e LOD Group na instância; configura luzes, probes e iluminação; ativa o pós na câmera e define Volume/Profile; regula a qualidade no asset do pipeline. Esse percurso é documentado, respectivamente, em [Model Import Settings](https://docs.unity3d.com/6000.0/Documentation/Manual/FBXImporter-Model.html), [URP Lit](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/lit-shader.html), [Mesh Renderer](https://docs.unity3d.com/6000.0/Documentation/Manual/class-MeshRenderer.html), [LOD Group](https://docs.unity3d.com/6000.0/Documentation/Manual/class-LODGroup.html), [Light](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Light.html), [pós com Volume](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/add-post-processing.html) e [URP Asset](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/universalrp-asset.html). Os nomes e opções próprios da URP servem como referência de intenção; a Astra mantém um pipeline Vulkan escalável, sem copiar URP/HDRP como subsistemas.

Percurso esperado na Astra:

1. **Selecionar fonte e avaliar qualidade.** Mostrar origem/licença, nós, triângulos, materiais, mapas, resolução, UV, densidade de texel, escalas, animações e diagnósticos. Separar “ausente na fonte”, “não importado” e “não suportado pelo renderer”.
2. **Configurar importação.** Editar perfil do projeto/fonte/nó, ver preview e diff dos derivados, confirmar publicação atômica e preservar GUIDs de subassets e overrides de instância.
3. **Trabalhar recursos.** Abrir malha, textura, material, animação e shader como recursos independentes; criar variantes; ver usuários/dependências; mudar uma imagem sem reconstruir geometria.
4. **Compor objetos.** Anexar MeshRenderer, Collider, LODGroup, Light, Camera, Volume, áudio, script etc. O Add resolve requisitos e relata referências ainda vazias.
5. **Iluminar e enquadrar.** Ambiente/sol, luzes locais, sombras, GI/probes/lightmaps quando implementados, câmera, exposição e efeitos com resultado visível no viewport e no Play.
6. **Ajustar política global.** Qualidade definida por eixos e capacidades do aparelho; efeito por cena apenas quando for intenção autoral (por exemplo um volume de neblina), não por detecção de nome de mapa.
7. **Medir e distribuir.** Perfil de CPU/GPU/memória, warnings de budget, captura comparável, save/reopen/export/Play preservando identidade e comportamento.

## 3. Contrato de dados e fronteiras de responsabilidade

```text
Fonte externa (GLB/glTF + dependências; outras famílias quando suportadas)
  └─ SourceRecord + proveniência + ImportProfile versionado
      └─ ImportIR: hierarquia, primitivas, UV/tangentes, imagens, materiais, clips
          └─ Derivados com GUID estável: Mesh/Texture/Sampler/Material/Animation/Scene
              └─ Instâncias autorais: Transform + componentes + overrides locais
                  ├─ Extração de render: ObjectId, slots, layers, LOD, material efetivo
                  ├─ Mundo físico/áudio/scripts: referências e fases próprias
                  └─ RenderView + RenderGraph + ResolvedRenderingPolicy → GPU
```

- **Fonte** preserva bytes/referência, proveniência e opções de importação. **Derivado** é regenerável e identificável por GUID; **instância** preserva edição local; **runtime** possui handles transitórios. Preset não serializa handle GPU, slot efêmero, revisão transitória nem ObjectId de outra cena.
- **Identidade:** GUID de fonte/subasset/asset de projeto, TypeId, PropertyId e InstanceId têm papéis distintos. Reimportação usa correspondência estável, diff e decisão para renomeações ambíguas; jamais troca silenciosamente o alvo de referência.
- **Um comando semântico:** UI, C#, gizmo, preset, animação e ferramenta NoCode passam pela mesma validação de propriedade e pela mesma notificação de impacto. O editor especializado só muda a apresentação.
- **Cinco arestas:** requisito de composição; referência funcional; referência a recurso; serviço/fase; capacidade de backend/dispositivo. O grafo direto/reverso deve unir AssetRegistry, componente e propriedade, com origem, destino, revisão e força.
- **Invalidar o mínimo:** fator/UV de material → material e descritor; imagem → seus usuários e mips; mudança de geometria/UV → malha, tangente, LOD, colisão e bake dependentes; luz → cluster/sombra/probes afetados; política → nova época de resolução. Falha de publicação GPU mantém a última versão válida e mostra pendência.
- **Preservar autoria:** culling, instancing e batching mantêm ObjectId, seleção, hierarquia e material por instância. Fusão destrutiva, se oferecida, é derivado de distribuição com mapa de retorno à fonte.

## 4. Catálogo funcional: recursos e componentes

**Legenda:** `E` = base existente com escopo parcial; `P` = especificado/planejado. Nenhum `P` é botão funcional. Cada linha deve ser desdobrada em fichas de PropertyId com estado e evidência conforme P01/P18. As classes e métodos sugeridos nas próximas seções são contratos propostos, não APIs disponíveis hoje.

### 4.1 Projeto, importação e recursos

| Família e referência Unity | Funções, subfunções e propriedades que o autor precisa | Astra: responsabilidade, dependências e situação |
|---|---|---|
| Modelo / Model Import Settings | escala/unidade/eixo; preservar hierarquia; incluir câmeras/luzes; normais/tangentes; UV1/UV2; compressão; read/write; materiais; rig/animação; colisão | `ModelImportProfile` por projeto/fonte/nó + relatório/preview/diff. Depende de M08/M09 e P05/P06/P07/P09. `ImportProfile` atual só tem escala/textura: **E/P** |
| Textura / Texture Importer | papel cor/dado/normal/HDR; sRGB; canais/swizzle; alpha; dimensão; mips; filtro/wrap/aniso; compressão por aparelho; residência | `TextureAsset` + `TextureImportProfile` + `SamplerAsset`/estado. R4 já cobre várias escolhas e viewer, mas separação imagem/sampler, distribuição e caminhos equivalentes ficam em P07/P08: **E/P** |
| Malha / Mesh asset | nós/primitivas, atributos, pivô, bounds, escala, tangentes por UV, lightmap UV, slots, sockets, LOD, collider derivado, variantes | Documento de malha em P05/P09; `MeshRenderer` não substitui edição do recurso. Dependências com física, GI, animação: **E/P** |
| Material / URP Lit como referência | superfície opaca/máscara/transparente; baseColor, normal, metal/rough, AO, emissão; UV/tiling/offset/rotação por binding; dupla face; variantes | `MaterialAsset` compartilhado + override por instância/slot + compilação variante. R4 parcial; recursos avançados só com consumidor e budget em M13/P08: **E/P** |
| Shader/Compute autorável | parâmetros refletidos, variantes, entradas/saídas de passe, recursos de GPU, erros por linha, versão válida anterior | M13.1/M13.2; Inspector deriva da reflexão, IDE e API de passes com capacidade declarada: **P** |
| Cena/prefab/subasset | instanciar, editar override, reverter/aplicar, navegar origem, atualizar após reimportar | M08/M10; IDs estáveis, diff, grafo de usuários e transação: **E/P** |
| Política de qualidade global / URP Asset | resolução, sombras, luzes, GI/AO, pós, LOD, textura/streaming, memória, térmica e perfil de dispositivo | `ProjectRenderingSettings` → `ResolvedRenderingPolicy`; ampliar consumidores e diagnóstico, sem ramificação por preset/cena: **E/P** |

### 4.2 Composição da cena e ambiente

| Família e referência Unity | Funções, subfunções e propriedades no Inspector | Astra: componente/recurso, dependências e situação |
|---|---|---|
| Transform + MeshRenderer | malha e slots; material/override; cast/receive shadows; render layer; GI/lightmap; probes; motion vectors; occlusion | `MeshRenderer` já tem malha/material/slots. Flags restantes exigem pass/shader/extraction/probe e P09/P12/P16: **E/P** |
| LOD Group / HLOD | níveis por área de tela ou erro, renderer de cada nível, crossfade, hysteresis, preview de câmera; derivado distante sem perder fonte | `LODGroup` proposto, assets de LOD e seleção na extração; política global limita custo. P09/M14: **P** |
| Collider / física | shape, origem, escala, layer, material físico, trigger, mesh cooking; vínculos com corpo ou personagem | Componentes físicos existentes; geração derivada e edição visual/reimport em P09/P10: **E/P** |
| Light | directional/point/spot; cor/temperatura; intensidade/unidade; range/angles; modo real-time/mixed/baked; cookie; sombras, viés, resolução, layer | `Light` tem tipos/cor/intensidade/range/cone. Sombras locais, modo baked, cookie e unidades calibradas exigem renderer/bake: **E/P** |
| Environment / sky | céu HDRI ou procedural, rotação, sol, irradiância SH, reflexão, exposição de referência, atmosfera/neblina | Ambiente hemisférico e sombra do sol existem; SH/probes/atmosfera e autoria completa P16/M13: **E/P** |
| Lighting Settings / bake | iluminação direta/indireta, lightmap UV, resolução, atlas, probes, invalidação e progresso de bake | Recurso do projeto/cena + serviço de bake, workers e cache. Sem controle falso antes do consumidor. P07/P12/P16/M13: **P** |
| Light Probe Volume | distribuição/grade, influência, interpolação, prioridade, bake e visualização | Componente/asset de irradiância; renderer deve consumir por objeto. P16/M13: **P** |
| Reflection Probe | origem/volume/parallax, prioridade/blend, captura baked/realtime, resolução/budget, fallback de céu | Componente/asset + captura/atlas + amostragem especular; P16/M13/M14: **P** |
| Camera / RenderView | projeção, clipping, viewport, layers, exposição, output, pós, preview, prioridades | Câmera e editor preview parciais; separar câmera, vista e alvo; P03/P12/M13: **E/P** |
| Volume + Post Profile | global/local, shape, priority, blend distance/weight; overrides de exposição, tonemap, bloom, AO, color grading, fog, AA | Recurso `PostProfile` e componente `Volume` propostos; camera/layer/RenderView e render graph consumidores. P16/M13: **P** |
| Decal / Terrain / Foliage / Particles | projeção, blend, normal/roughness, scattering, vegetação com LOD/vento, emissão e colisão quando cabível | Famílias separadas com recursos e capacidades próprias; entram após base de material/LOD/volume em P16/M14: **P** |
| Animator / áudio / interações | clips, rig, state machine, eventos, áudio espacial, gatilhos/scripts; prova do mundo como jogo | P11/P12/M12; realismo interativo depende desses consumidores, sem misturá-los ao renderer: **E/P** |

**Dependências reais do MeshRenderer:** malha é obrigatória para desenhar; material pode herdar da fonte; sombra depende do passe e do tipo de luz; probe depende do serviço e do recurso; lightmap depende de UV e bake; motion vectors dependem de pose anterior/skinning e pass temporal. A interface mostra “ausente”, “herdado”, “indisponível no backend” e “desativado pela política” como estados diferentes.

## 5. Propriedades e telas que precisam existir

### 5.1 Inspector do modelo e relatório de importação

Grupos: **Fonte** (arquivo, hash/revisão, créditos/licença, dependências, tamanho), **Estrutura** (árvore de nós, pivô, escala/unidade/eixo, inclusão/exclusão por nó), **Geometria** (triângulos por nó, normais/tangentes, UV disponíveis, densidade de texel, bounds, slots), **Aparência** (material e texturas resolvidos, semanticidade sRGB/linear, alpha, extensões), **Derivados** (LOD, colisão, UV de lightmap, compressão, texture profile), **Animação** (rig/clips/morphs) e **Publicação** (preview/diff, conflitos de GUID, custo estimado, Apply/Cancel). Perfil de projeto e override por fonte/nó apresentam o valor efetivo e a origem da herança.

Diagnósticos específicos de “textura esticada”: UV ausente, ilha com densidade muito desigual, transform incorreto, canal UV errado, escala de objeto incompatível, tangente calculada para outro UV, wrap inadequado, projeção do asset fonte não preservada. Corrigir com UV da fonte/reimport, binding UV adequado ou transformação controlada; oferecer triplanar como **modo de material com custo e limitação explícitos**, nunca deformar UV automaticamente sem preview. Textura em maior resolução não corrige mapeamento errado.

### 5.2 Inspector de textura, material e malha

- **Textura:** preview por canal/mip/alpha, tipo color/data/normal/HDR, espaço de cor, swizzle, normal Y, tamanho e compressão resolvidos por perfil de aparelho, mips, alpha edge, filtro/wrap/aniso, bytes residentes, usuários reversos e warnings.
- **Material:** tipo de superfície, culling, alpha cutoff, fatores PBR, slots com textura/canal/UV/sampler/transform, escala normal/AO/emissão, origem de cada valor (fonte → asset → instância), preview sob iluminação controlada, variantes/shader e custo. Mostrar efeito no caminho direto/indireto/sombra e capacidade que faltar.
- **Malha:** hierarquia/primitivas e slots, triângulos/atributos, UV0/UV1/tangentes, orientação/normais, bounds/pivô, LODs, collision mesh, lightmap UV, usuários e edição derivada. Material e mesh são documentos distintos da instância.

### 5.3 Inspector de objeto, luz, câmera e ambiente

- **MeshRenderer:** identidade do mesh, cada material slot, camada, visibilidade, cast/receive shadow, GI/probe/lightmap e LOD associado. Propriedades desabilitadas têm motivo e requisito; não persistir opção sem consumidor.
- **Light:** tipo, cor ou temperatura, unidade/intensidade, range e cone condicionais, layer, modo realtime/baked/mixed, cookie, sombras (tipo/resolução/bias/normal bias/fade/budget) apenas onde suportadas. Gizmo mostra volume e cone.
- **Environment/Lighting:** céu/HDRI, sol, luz ambiente difusa e especular, reflexão de fallback, neblina/atmosfera, bake e estado de invalidação; gizmos para probes e volumes.
- **Camera/Volume:** enquadramento/clipping/layers/output; exposição manual/automática, tonemap, AA, bloom, AO, gradação/LUT, fog e prioridades/blend de volumes. Preview mostra o mesmo contrato de RenderView usado em Play.
- **Qualidade do projeto:** pedido do autor, resultado resolvido por dispositivo, motivo de downgrade e custo por eixo. Preset global é ponto inicial editável, não versão alternativa do renderer.

### 5.4 Matriz inicial de campos, dependências e consumidores

Esta é a primeira lista operacional de campos. **Campo semântico** não equivale a PropertyId já publicado: IDs novos só podem ser fixados com schema, migração e consumidor; os existentes preservam seus IDs. `Atual` significa que há base no código, não fechamento de todos os caminhos. A ficha individual de P01/P18 acrescentará default, unidade, domínio, API, formato, erros e prova.

| Local / campo semântico | Estado | Dependência e consumidor que deve reagir | Ação visível no Inspector / impacto |
|---|---|---|---|
| Modelo: `source.scale`, unidade e eixo | escala atual; restante proposto | importador → transform, bounds, física e LOD | perfil efetivo, preview de dimensões e reimportação |
| Modelo: preservação de nós/subassets | parcial | reconciliador M08 → hierarquia e GUID | árvore com conflito por nó, sem perder seleção |
| Modelo: `normalMode`, `tangentUvSet` | proposto | UV presente + importador → normal map e shader | diagnósticos de tangente ausente/incoerente; regenerar derivado |
| Modelo: `uvSet`, `lightmapUv` | UV da fonte atual; geração proposta | mesh + material/bake → amostragem e GI | visualizar canal/ilhas; reimportar ou gerar UV de bake |
| Modelo: `lodSource`/`lodError` | proposto | malha derivada + LODGroup + política | níveis em preview e estimativa de erro/custo |
| Modelo: `collisionSource` | proposto | mesh + cooker físico | distinguir shape autoral de derivado; custo e invalidação |
| Textura: papel e espaço de cor | parcial R4 | texture profile → decode, mips e shader PBR | indicar cor sRGB/dado linear e valor efetivo |
| Textura: canais/normal Y/alpha | parcial R4 | binding/material → BRDF, cobertura e sombra | preview por canal, warning de mapa incompatível |
| Textura: mips, dimensão, filtro, wrap, aniso | parcial R4 | perfil + sampler + capability → memória e aliasing | original/pedido/resolvido/bytes e motivo de redução |
| Material: superfície e dupla face | parcial R4 | pipeline de cor/profundidade/sombra | opções condicionais; preview e custos de overdraw |
| Material: base/normal/metal/rough/AO/emissão | parcial R4 | textura semanticamente válida + shader PBR | slot por binding, herança, variação e canal isolado |
| Material: UV set e transform por binding | parcial R4 | UV da malha + caminho direto/indireto/sombra | selecionar canal; escala/deslocamento/rotação; checar estiramento |
| Material: parâmetro avançado (`clearcoat`, transmissão etc.) | proposto | shader, iluminação, transparência e budget | só exibir editável quando houver variante consumidora |
| MeshRenderer: malha, material e slots | atual/parcial | GUID válido + extraction/render + AssetRegistry | origem, override, diff por slot e usuários reversos |
| MeshRenderer: cast/receive shadow | proposto | pass de sombra do tipo de luz + material efetivo | opção por objeto com estado de capacidade e preview |
| MeshRenderer: probe/lightmap e layer | proposto | probe/bake + render layer + shader | picker tipado, estado de bake e fallback explícito |
| LODGroup: níveis, hysteresis e fade | proposto | meshes derivadas + extração + política global | barras por erro/tela e preview sem substituir origem |
| Light: tipo, cor, intensidade, range e cone | atual | component schema + luz direta/extraction | controles condicionais e gizmo de alcance |
| Light: temperatura, cookie, unidades físicas | proposto | conversão de unidade + textura cookie + shader | modo/unidade explícitos, sem reinterpretar cena legada |
| Light: sombras locais, bias e resolução | proposto | shadow atlas de point/spot + política/budget | controles surgem com consumidor e perda por capability visível |
| Environment: céu, SH e reflexo | hemisfério atual; restante proposto | sky asset + diffuse/specular IBL | preview do céu/sonda e estado de atualização |
| Reflection/Light Probe: volume e bake | proposto | geometria, lightmap/SH, bake/capture, shader | gizmos, resolução, prioridade, blend, invalidação |
| Camera: projeção, clipping, vista | parcial | RenderView + culling/picking/output | preview equivalente ao Play e propriedades por câmera |
| Post Volume: shape, prioridade, overrides | proposto | câmera/layer + render graph + perfil | lista de overrides ativos e blend espacial |
| Qualidade: sombra/LOD/textura/pós/resolução | parcial | ResolvedRenderingPolicy + device capabilities | pedido/resolvido/causa e custo por eixo global |

### 5.5 Mecânicas reutilizáveis para o mundo em primeira pessoa

Realismo interativo exige sistemas que funcionem em qualquer mapa: `InputAction` e remapeamento; `CharacterMotor`/colisão e degraus; `CameraLook` e captura de ponteiro/toque; `Interactable` com alvo, alcance e ação; `Door/Hinge` ou junta com animação/estado; `Pickup/Inventory`; `Trigger`; `AudioSource` espacial; `SaveState` por identidade persistente; eventos de script com fase e autoridade definidas. `Character` e `CameraLook` já são bases nativas, mas a cadeia inteira não está fechada. P10/P11/P12/M12 devem entregar cada mecânica como componente ou contrato de serviço, com propriedades, gizmo, API C#, persistência, fase Play, eventos, erros e exemplo reutilizável. A demo pode compor essas peças; o componente não deve carregar nome do mundo.

### 5.6 Aquisição e qualificação de fontes 3D

Para mundos realistas, a seleção de GLB/glTF deve exigir preview de resolução e densidade, materiais PBR/texturas efetivas, UV/tangentes, escala, hierarquia editável, dimensões de memória e licença/proveniência registrada. Classificar cada candidato como **fonte pronta**, **reparável** (por material/UV/LOD/bake) ou **insuficiente para a meta visual**. Se uma licença permite teste mas restringe redistribuição, o recurso fica marcado no projeto e no relatório de exportação. Assets de cena podem ser compostos de múltiplas fontes coerentes; um único GLB com poucos materiais e sem texturas não é critério de qualidade alta, mesmo que tenha muitos nós ou triângulos.

## 6. APIs e comandos propostos

Os nomes abaixo fixam **responsabilidades**, não assinaturas já existentes. Implementar adaptadores C++ do editor e ponte C# somente quando tipo, tempo de vida, erro e consumidor estiverem definidos. API de edição de asset pode ser restrita ao Editor; runtime pode ler e ajustar somente valores marcados mutáveis durante Play.

| Serviço proposto | Operações mínimas | Validação/efeito obrigatório |
|---|---|---|
| `AssetImportService` | `Analyze`, `Stage(profile)`, `PreviewDiff`, `Commit`, `Cancel`, `Reimport` | Confere dependências, orçamento, GUID/subasset, overrides, migração, atomicidade; relatório de rejeição por item |
| `AssetRegistry` ampliado | `Find`, `UsersOf`, `DependenciesOf`, `ResolveSubasset`, `Move`, `DeleteImpact` | Grafo direto/reverso por propriedade; referência quebrada não vira outro asset silenciosamente |
| `MeshAssetService` | `Inspect`, `GenerateTangents`, `GenerateLightmapUv`, `BuildLod`, `CookCollision` | Derivados versionados por fonte/parâmetros; seleção mantém objeto e slots |
| `TextureAssetService` | `Inspect`, `SetImportProfile`, `RebuildMips`, `GetResidency` | Cor/dado, sampler, compressão/capability, invalidação seletiva |
| `MaterialService` | `CreateVariant`, `SetBinding`, `SetFactor`, `ResolveEffective`, `InspectPipelineCoverage` | Herança fonte/asset/instância, todos os caminhos de render e shadow, erro por binding |
| `SceneComponentService` | `PlanAdd`, `Add`, `SetProperty`, `ApplyPreset`, `RemoveImpact` | Um TypeId/PropertyId e transação de dependências; Undo/Redo; fase Play; capacidades |
| `LightingService` | `RequestBake`, `CancelBake`, `Invalidate`, `CaptureProbe`, `InspectStatus` | IDs e versões do resultado, UI de progresso, fallback explícito quando bake/probe faltar |
| `RenderViewService` | `CreateView`, `SetCamera`, `SetOutput`, `Capture`, `InspectEffectivePolicy` | Mesma cena e material efetivo em preview/Play; handles internos não persistem |
| `RenderExtensionService` | `RegisterShader`, `RegisterPass`, `DeclareResources`, `Compile`, `Publish` | M13: valida ciclo/estágio/acesso/capacidade/budget e mantém versão anterior se falhar |
| `ProfilerService` | `CaptureFrame`, `CapturePassCosts`, `CaptureResidency`, `ExportReport` | Separa CPU/GPU/editor/Play, capability, resolução e estado térmico |

**Contrato de propriedade sugerido:** `{typeId, propertyId, schemaVersion, valueType, default, unit, domain, visibleWhen, editableWhen, referenceKind, requiredCapability, playMutability, invalidation, serializer, migrator, consumer}`. Aplicar uma alteração retorna `{applied, rejected, pendingGpu, degraded}` mais causa e alvos afetados. Mesmo contrato alimenta Inspector, script, preset, animação, Undo e documentação. Não expor setter de sombra pontual até haver passe e extração da sombra pontual.

## 7. Presets e a integração específica com P02

P02 permanece dono de **captura/aplicação de valores de componente e composição transacional**. Este plano consome e amplia o contrato, sem transformar todo recurso gráfico em um `ComponentValue` gigante.

| Categoria | Conteúdo persistente | Relação com P02 |
|---|---|---|
| Preset de componente | TypeId/versão, PropertyIds/valores, GUIDs tipados e requisitos | P02 existente; ampliar seleção por campo, diff por slot, recursos reversos, migração e C# quando houver contrato |
| Preset composto/receita | Conjunto ordenado de componentes, parâmetros, referências internas e requisitos externos declarados | Extensão P02 pendente; Add em uma transação, sem copiar ObjectIds de cena |
| Perfil de importação | Escolhas de projeto/fonte/nó para gerar derivados | P05/P06/P07/P09; pode aparecer em biblioteca de perfis, não no payload de um componente |
| Material/Texture/Post Profile | Assets versionados e reutilizáveis com GUID e usuários | P05/P07/P08/P16; preset de componente guarda **referência**, não cópia opaca do recurso |
| Política global de qualidade | Eixos independentes e budgets do projeto/dispositivo | `ResolvedRenderingPolicy`; P02 não codifica um preset de qualidade no GLB ou na cena |

**Ao aplicar um preset gráfico:** preparar candidato e diff; validar TypeId/versão/capacidades; resolver GUID e remap ao importar de outro projeto; calcular dependências/componentes adicionais; mostrar resultado efetivo; registrar uma transação de história; publicar CPU e depois GPU. Se GPU falhar, estado autoral persiste com diagnóstico de publicação pendente; não fingir equivalência visual. Um preset “ambiente realista” só pode ser receita composta de componentes e referências a assets que existem, sem habilitar GI/probe/sombra indisponíveis.

## 8. Sequência de implementação em blocos completos

Os blocos abaixo são **subpacotes de P02/P05–P09/P11–P12/P16 e M08/M09/M12–M14**, não uma numeração concorrente do roadmap. Fechar cada bloco em editor, persistência, API, consumer e evidência antes de marcar uma linha como pronta. Trabalho posterior pode avançar arquitetura sem rodar testes até receber autorização para validar.

| Bloco | Entrega completa e visível | Pré-requisito / planos | Gate de fechamento quando validação for autorizada |
|---|---|---|---|
| G0 · matriz e corpus | Fichas por propriedade/consumidor, 3 fontes representativas com proveniência e referência visual; critérios de densidade/material/UV/triângulos | P01/P18, M08, atlas Unity | Cada controle tem estado, evidência e dono; relatório distingue arte ruim de falha técnica |
| G1 · composição/presets | Referências tipadas, grafo de impacto, receitas multi componente, diff seletivo, remap entre projetos, migração e Undo de biblioteca | P02 + P01/AssetRegistry | Add/apply/save/reopen/Undo/reimport preservam GUID, valores e IDs de cena |
| G2 · importação autorável | Perfis completos por fonte/nó, preview/diff, UV/tangentes, semântica de mapas, escala e subassets; reimport transacional | P05/P06/P07/P08/P09, M08/M09 | Importação de fontes distintas sem textura esticada introduzida pela engine; diagnósticos nos casos ruins da fonte |
| G3 · materiais/meshes | Documento de malha, LOD e collision derivado; materiais PBR e overrides simétricos no direto/indireto/sombra | G2, P08/P09, M09/M13 | Mesma aparência e identidade nos caminhos relevantes; edit/save/reopen por slot |
| G4 · luz e GI de base | Escala fotométrica definida, mais luzes por orçamento, sombras spot/point, IBL difuso/especular, probes/lightmaps com autoria e fallback | G3, P12/P16, M13 | Interior com materiais corretos, contato/sombras locais e reflexos coerentes; custo medido |
| G5 · câmera e pós | RenderView independente, HDR intermediário, exposição/tonemap, Volume/Profile, AO/temporal/bloom/grading conforme backend | G3/G4, P03/P12/P16, M13 | Cena/Play/preview usam resultado equivalente; sem UI controlando efeito ausente |
| G6 · mundo interativo escalável | LOD/HLOD/streaming, objetos preservados, animação/áudio/colisão/script, profiling e políticas térmicas | G1–G5, P09/P11/P12/P16, M12/M14 | Três mundos jogáveis, conteúdo editável e budgets sustentados em aparelhos alvo |

**Ordem crítica:** qualidade da fonte e fidelidade PBR/UV → composição e edição reproduzível → luz/reflexos/sombras/GI → HDR/pós → LOD/streaming e escala. Pós não repara ausência de geometria, material ou iluminação. O G4 não deve prometer 30 luzes com sombra antes de existir orçamento, atlas e perfil de dispositivo para elas. Técnicas avançadas (DDGI/SDF/SSR/virtual shadows/MicroMesh) pertencem a experimentos condicionais de M14 após base comparável.

## 9. Gates de qualidade de um mundo realista

Escolher um **interior** (muitos materiais e luzes locais), um **exterior** (vegetação, distância e sol) e um **misto interativo** (portas/objetos manipuláveis, colisão, animação/áudio). Usar fontes com PBR e geometria suficiente, manter metadados de proveniência e licença, e guardar referência visual com câmera, horário, exposição e resolução fixos. A matriz de avaliação deve cobrir:

1. **Fonte:** meshes/nós/slots/UV/tangentes/texturas e densidade esperadas, sem substituir fonte fraca por filtro visual; relatório de faltas e extensões.
2. **Autoria:** selecionar um objeto filho, trocar slot e UV, ajustar luz/volume, salvar, fechar, reabrir, reimportar e verificar overrides/Undo; não aceitar mundo colapsado em 8 meshes como prova de editabilidade.
3. **Fidelidade:** comparação por enquadramento controlado; texturas sem estiramento indevido, normal/alpha corretos, sombras locais, GI/reflexos coerentes; listar capacidades ativas e degradadas.
4. **Execução:** percorrer em primeira pessoa, colidir, acionar pelo menos duas interações de naturezas diferentes, mudar de área e retornar; Play/preview/export contam como caminhos distintos.
5. **Performance:** tempo CPU/GPU por passe, p95, memória residente e uploads, abertura fria/quente, editor vs Play, temperatura e resolução interna; perfis C/B/A/S ajustam eixos sem retirar conteúdo autoral silenciosamente.
6. **Regressão:** três assets de procedências diferentes, formatos/materials distintos e casos adversos de UV, normal, alpha, reimport e referência ausente; o mesmo componente/API funciona em todos.

Nenhuma dessas validações foi executada para este plano. Os números e evidências de documentos anteriores continuam válidos apenas em seus próprios escopos. A implementação futura deve registrar por linha: **inventariado → especificado → implementado não validado → validado no escopo indicado**, ou **parcial/bloqueado/não aplicável** com motivo.

## 10. Referências e manutenção da matriz

- Catálogo já pesquisado: [atlas offline Unity](../componentes/pesquisa-2026-09-15/ATLAS-UNITY.html) e [cobertura/limites](../componentes/pesquisa-2026-09-15/resumo-cobertura.json). O inventário estrutural não significa paridade de cada Inspector ou CustomEditor.
- Unity 6.0: [Model Import Settings](https://docs.unity3d.com/6000.0/Documentation/Manual/FBXImporter-Model.html), [Mesh Renderer](https://docs.unity3d.com/6000.0/Documentation/Manual/class-MeshRenderer.html), [LOD Group](https://docs.unity3d.com/6000.0/Documentation/Manual/class-LODGroup.html), [Light](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Light.html), [URP Lit](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/lit-shader.html), [pós com Volume](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/add-post-processing.html) e [URP Asset](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/universalrp-asset.html). O pacote URP é apenas fonte de padrões de autoria, não dependência da Astra.
- Ao executar cada bloco, abrir fichas com: referência e versão; TypeId/PropertyId; função/caso de uso; default/unidade/domínio; origem e tipo de dependência; editor; API; serialização/migração; consumidor; invalidação; erros; prova autorizada; estado. Uma linha sem consumidor não pode avançar para “implementada”.

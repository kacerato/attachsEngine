# G7 — API universal do plano gráfico (C# e Flow), 23/10/2026

Plano: `docs/planos/DESEMPENHO-GRAFICOS-VIVOS-2026-10-17.md`, seção “G7 — API universal, editor e publicação”.
Branch `claude/awesome-gauss-9bd32c` sobre `codex/ui-play-performance-apk-export` (17c96512).

O que entrou no plano gráfico (G2, G3a, G4, PK-1..PK-9, V-1, V-2) foi conferido contra a fachada C# gerada
(`managed/Astra.Scripting/Generated/Components.g.cs`, saída de `scene::componentCSharpApi()`) e contra o “Flow”
que roda no Play. Referências: Unity 6000.0 ScriptReference e SRP Core/URP/HDRP 17 (links por linha abaixo).

## O que é o “Flow” aqui

O Flow que executa no Play do aparelho é a **Conexão de evento** (`astra.logic.event_connection`, o UnityEvent da
Astra): “quando ESTE objeto emitir o evento X, faça Y”, sem código. O `Aether.Flow` gerenciado (AST + interpretador +
gerador C#) continua sendo a prova de conceito registrada em `docs/ESTADO.md`: não está ligado ao Play nem ao editor,
então “nó Flow” neste bloco significa evento ou método no catálogo da Conexão de evento (`scene/event_connection.h`).

## Cobertura antes × depois

Legenda: ✅ coberto com nome/assinatura da Unity · 🟡 coberto com nome da Astra (o mesmo dado, outro nome) ·
❌ ausente · ⚠️ existia mas não funcionava como na Unity.

| Recurso do plano | Unity (6000.0 / SRP 17) | Antes | Depois |
|---|---|---|---|
| Volume: forma, peso, prioridade, mistura | `Volume.isGlobal/weight/priority/blendDistance` | 🟡 `EnvironmentVolume.VolumeShape/Weight/Priority/BlendDistance` | ✅ `Astra.Rendering.Volume.IsGlobal/Weight/Priority/BlendDistance` (e o 🟡 continua) |
| Perfil compartilhado | `Volume.sharedProfile` | 🟡 `GetProfile/SetProfile(AssetGuid)` | ✅ `Volume.SharedProfile`; Flow: método **Volume: trocar perfil** (recurso) |
| Perfil da instância | `Volume.profile`, `HasInstantiatedProfile()` | ⚠️ escrever um campo de um Volume com perfil não tinha efeito (a coleta lia o recurso) | ✅ `Volume.Profile` copia o perfil para o Volume (método nativo `instantiate_profile`); `HasInstantiatedProfile()` |
| Overrides do perfil | `VolumeProfile.TryGet<T>/Add<T>/Remove<T>/Has<T>` | 🟡 booleanos `Override*` (Add Override) | ✅ `VolumeProfile` com os mesmos quatro métodos |
| Caixa por parâmetro | `VolumeParameter.overrideState`, `.value`, `Override(x)` | ❌ sem acesso (só no Inspector) | ✅ `VolumeParameter<T>.OverrideState/Value/Override`; nativo `parameter_overridden`/`set_parameter_overridden` + tabela gerada `VolumeParameterTable` |
| Cabeçalho do override | `VolumeComponent.active`, `SetAllOverridesTo` | ❌ | ✅ `VolumeComponent.Active`, `SetAllOverridesTo`; nativo `component_active`/`set_component_active` |
| Exposure (HDRP) | `Exposure.mode/meteringMode/fixedExposure/compensation/limitMin/Max…` | 🟡 `Exposure*` com enums em português | ✅ classe `Exposure` |
| Tonemapping | `Tonemapping.mode` | 🟡 `ToneMapper` (Nenhum/Neutro/ACES/AgX) | ✅ `Tonemapping.Mode` (`TonemappingMode.None/Neutral/ACES/AgX`) |
| Color Adjustments | `postExposure, contrast, colorFilter, hueShift, saturation` | 🟡 campos soltos | ✅ classe `ColorAdjustments` |
| Bloom | `threshold, intensity, scatter, clamp, tint, highQualityFiltering, dirtTexture, dirtIntensity` | 🟡 campos soltos + `GetBloomDirt` | ✅ classe `Bloom` |
| Depth of Field | `mode, gaussianStart/End/MaxRadius, highQualitySampling, focusDistance, focalLength, aperture, bladeCount/Curvature/Rotation` | 🟡 `Dof*` | ✅ classe `DepthOfField` |
| Motion Blur | `quality, intensity, clamp` | 🟡 | ✅ `MotionBlur` |
| Chromatic Aberration | `intensity` | 🟡 | ✅ `ChromaticAberration` |
| Lens Distortion | `intensity, xMultiplier, yMultiplier, center, scale` | 🟡 | ✅ `LensDistortion` |
| Vignette | `color, center, intensity, smoothness, rounded` | 🟡 | ✅ `Vignette` |
| Color Curves | `master/red/green/blue/hueVsHue/…` (TextureCurve) | ❌ curvas sem API | 🟡 `ColorCurves` liga/desliga e marca caixas; **chaves das curvas continuam só no Inspector** |
| White Balance, Lift Gamma Gain, Shadows Midtones Highlights | três overrides na URP | 🟡 um override “Cor” | ✅ três classes sobre o mesmo override (adicionar uma adiciona as três — diferença declarada) |
| Film Grain, Ambient Occlusion | `FilmGrain.intensity`; AO do HDRP | 🟡 | ✅ `FilmGrain`, `AmbientOcclusion` |
| Céu, sol e lua, atmosfera física, luz indireta, neblina | Visual Environment/Skybox, PBR Sky, Indirect Lighting Controller, Fog | 🟡 | ✅ `Sky`, `Celestial`, `PhysicallyBasedSky`, `IndirectLightingController`, `Fog` |
| Nuvens | `CloudLayer` (HDRP) | 🟡 `Cloud*` | ✅ classe `CloudLayer` |
| Hora do dia | (sem tipo na Unity; plano: `TimeOfDay.Current.Hour/Speed`, `HourChanged`) | ⚠️ `Hour` existia, mas no Play a hora era “autoral + tempo”: escrever 18,5 deslocava em vez de pôr 18,5 | ✅ relógio de Play no próprio componente (`SceneTimeOfDayClock`): `Hour = 18.5f` vale no quadro seguinte; `TimeOfDay.Current`, `Speed` (60 = 1 min real por hora), `HourChanged/Sunrise/Sunset/DayChanged` |
| Eventos de hora no Flow | — | ❌ | ✅ eventos **Hora do dia: hora mudou / nascer do sol / pôr do sol / dia mudou** |
| Luz da Lua | Moon do ciclo HDRP | 🟡 `MoonLight` (referência) | 🟡 igual; fase/tamanho/brilho por `Celestial` |
| RenderSettings | `RenderSettings.ambientMode, ambientSkyColor/EquatorColor/GroundColor, ambientLight, ambientIntensity, reflectionIntensity, fog, fogMode, fogColor, fogDensity, fogStart/EndDistance, skybox, sun` | ❌ estático inexistente (campos espalhados no Volume e nas Configurações de iluminação) | ✅ `Astra.Rendering.RenderSettings` sobre o Volume global da janela Iluminação; cada escrita marca a caixa e põe o override no perfil |
| Light | `type, shadows, cullingMask, renderingLayerMask, shadowStrength/Bias/NormalBias/NearPlane, shadowResolution, intensity, range, spotAngle, innerSpotAngle, color, colorTemperature` | 🟡 `Kind`, `ShadowMode`, `RenderingLayers`; resto com nome da Unity | ✅ `Type`, `Shadows`, `RenderingLayerMask`, `CullingMaskBits`, `ColorRgb` |
| Light › Mode | `lightmapBakeType` | ❌ gravado sem bake (PK-3) | ❌ **continua** — vira propriedade com o bake do G3c |
| MeshRenderer | `shadowCastingMode, receiveShadows, lightProbeUsage, reflectionProbeUsage, renderingLayerMask, sharedMaterial, material, lightmapScaleOffset` | 🟡 `CastShadows`, `LightProbes`, `ReflectionProbes`, `GetMaterial/SetMaterial`, `Component.Material(slot)` | ✅ aliases + `SharedMaterial`, `Material`, `MaterialAt`, `LightmapScaleOffset`; Flow: **Malha: trocar material** (recurso) |
| Camera | `clearFlags, cullingMask, rect, usePhysicalProperties, focalLength, sensorSize, gateFit, allowHDR, fieldOfView, near/farClipPlane, orthographic(Size), depth`; URP `renderPostProcessing`, `volumeLayerMask` | 🟡 `BackgroundType`, `ViewportX..`, `PhysicalCamera`, `Hdr`, `VerticalFov`, `PostProcessing`, `EnvironmentMask` | ✅ aliases com os nomes da Unity |
| ReflectionProbe | `RenderProbe()`, `mode, refreshMode, timeSlicingMode, size, center, bakedTexture` | 🟡 `RenderProbe()` + `Type/RefreshMode/TimeSlicing/BoxSize/BoxOffset` | ✅ `Mode`, `RefreshModeUnity`, `TimeSlicingMode`, `Size`, `Center`, `BakedTexture`; Flow: **Sonda de reflexo: capturar agora** |
| ReflectionProbe › bake | `Lightmapping.BakeReflectionProbe` (só editor na Unity) | 🟡 bake na janela Iluminação | 🟡 igual; no Play, `RenderProbe()` numa sonda Tempo real “Via script”. `IsFinishedRendering` **não existe** |
| LightProbeGroup | `probePositions` | 🟡 X/Y/Z por slot | ✅ `GetProbePosition/SetProbePosition(i)`, `ProbeCount()` |
| Material | `renderer.material.SetFloat/SetColor/SetTexture` com `_BaseColor, _Smoothness, _Metallic, _BumpScale, _Cutoff, _OcclusionStrength, _EmissionColor, _BaseMap, _BumpMap, _MetallicGlossMap, _EmissionMap, _OcclusionMap` (+ `_Color/_MainTex/_Glossiness`) | 🟡 `MaterialSlot.BaseColor/Smoothness/...` | ✅ `MaterialSlot.SetFloat/GetFloat/SetColor/GetColor/SetTexture/GetTexture/SetTextureOffset/SetTextureScale/HasProperty`; nome desconhecido lança |
| Material › shader | `material.shader = Shader.Find("…/Unlit")` | ❌ por renderer | 🟡 trocar para um Material Lit/Simple Lit/Unlit (`SharedMaterial`, Flow “Malha: trocar material`). **Trocar o shader de um override por objeto continua fora** |
| Entrada como evento (Flow) | Input System 1.11 `PlayerInput` (Behavior = Invoke Unity Events): `performed`/`canceled` por ação | ❌ ações só por script (`Input.Pressed`) | ✅ componente **Entrada do jogador** (`astra.input.player_input`): ações observadas (nomes do mapa da cena), eventos **Entrada: ação executada/cancelada** com filtro **Ação** na Conexão de evento; C# `PlayerInput.OnPerformed/Performed`; ícone `component/player-input`; catálogo “Entrada do jogador” |
| Busca no mundo | `FindFirstObjectByType<T>`, `FindObjectsByType<T>` | ❌ | ✅ `SceneQuery.FindFirstObjectByType<T>/FindObjectsByType<T>/Root` |

Contagem (nesta tabela, 31 linhas): antes ✅ 0 · 🟡 22 · ❌ 7 · ⚠️ 2. Depois ✅ 24 · 🟡 5 · ❌ 2
(`Light.lightmapBakeType`, curvas por chave e `IsFinishedRendering`/contagem de sondas como lacunas nomeadas).

## O que mudou no código

- Nativo: `SceneTimeOfDayClock` (`runtime/scene_time_of_day.*`, chamado por `EditorPlayScene::advanceTimeOfDay`), eventos
  `hour_changed/sunrise/sunset/day_changed` no descritor da Hora do dia; 8 métodos do Volume e `set_shared_material` do
  Mesh Renderer (`runtime/component_operations.cpp`); `GameWorld::replaceComponent`; serviços de recurso nas operações
  (`ComponentOperationServices::assets/environmentProfiles/resolveResource`, os mesmos do script).
- Conexão de evento **v3**: recurso do método (perfil ou material), eventos 40–45 e métodos 41–44 (36–40 e o evento 39 são das Partículas, G5). Cenas v2 abrem igual.
- Gerador da fachada: structs `partial`, métodos com recurso recebem `AssetGuid`, tabela `VolumeParameterTable`.
- Gerenciado: `managed/Astra.Scripting/Rendering/` (`Volume.cs`, `RenderSettings.cs`, `UnityNames.cs`, `SceneQuery.cs`) e
  `MaterialSlot.SetFloat/SetColor/SetTexture` em `Materials.cs`.

## Exemplos (C# e Flow)

Scripts em `examples/g7-api/Scripts/`; projetos gerados por `tools/g7_api_scene_author.cpp` (12 projetos, “G7 n … (C#)”
e “G7 n … (Flow)”). Capturas do POCO em `capturas/`.

| # | Exemplo | C# | Flow (Conexão de evento) |
|---|---|---|---|
| 1 | Ciclo de dia acelerado | `DiaAcelerado`: `Hour = 16`, `Speed = 720`, `HourChanged`/`Sunset` | Duração do dia 2 min; **Hora mudou → Sonda: capturar agora** |
| 2 | Perfil de Volume ao entrar numa área | `PerfilPorArea`: `TriggerEnter` → `Volume.SharedProfile = Área coberta` | **Sensor 3D: entrou → Volume: trocar perfil (Área coberta)**; saiu → Exterior |
| 3 | Desfoque ao mirar | `MiraComDesfoque`: botão “Mirar” → `Volume.Weight` 0→1, `DepthOfField.FocusDistance.Override(raio)`, `Camera.FieldOfView` | **Entrada do jogador: ação executada (Mirar) → Volume: definir peso 1**; cancelada → 0 |
| 4 | Postes ao anoitecer | `PostesAoAnoitecer`: `Sunset/Sunrise` → `Light.Enabled` + `_EmissionColor` | **Pôr do sol → Ativar Postes**; nascer → Desativar |
| 5 | Assar sonda por script | `SondaPorScript`: troca a cor do painel e `RenderProbe()` | **Timer 2 s → Sonda de reflexo: capturar agora** (painel em tween) |
| 6 | Trocar material | `TrocaDeMaterial`: `SharedMaterial` Ouro (Lit) ↔ Neon (Unlit), `SetFloat("_Smoothness")` | **Timer → Malha: trocar material (Neon)**, outro Timer volta ao Ouro |

## Validação

Host (26/10, base 145ba63b mesclada): suíte nativa 1723/1723; gerenciada 529 ok / 0 falhas / 71 pulados (`dotnet run`),
com `G7GraphicsApiTests` (7, inclusive o script real do exemplo 3 com a ação Mirar segurada), `test_g7_graphics_api` (9)
e os testes de toque da sessão `g7_touch_button_drives_player_input_connection_to_volume_weight` e
`g7_generated_aim_scene_touch` (cena gerada do exemplo 3, por `G7_AIM_SCENE`).

POCO, 26/10 09:48, Astra Dev `0.3.0-dev.261026999-g7-api-0938` (prancha `capturas/g7-prancha-20261026.jpg`, os 12
projetos na mesma rodada, com a guarda de ligação e primeiro plano):

| # | C# | Flow |
|---|---|---|
| 1 Dia acelerado | ✅ `HourChanged` 17h→23h, pôr do sol, noite com estrelas | ✅ hora mudou → sonda recapturada |
| 2 Perfil por área | ✅ “Perfil da área ativo”, sem queda (a correção do Play da base resolveu o mutex destruído) | ✅ troca de perfil, sem queda |
| 3 Desfoque ao mirar | ✅ “Mirando”/“Mira solta”, FOV 60→30 e desfoque | ✅ segurar Mirar liga o desfoque; soltar desliga |
| 4 Postes ao anoitecer | ✅ `Sunset` acende luz e emissão | ✅ pôr do sol → Ativar Postes |
| 5 Sonda por script | ✅ cor do painel + `RenderProbe()` | ✅ Timer → capturar agora |
| 6 Troca de material | ✅ Ouro (Lit) ↔ Neon (Unlit) | ✅ Timer → Malha: trocar material |

Causa do exemplo 3 em C#: o objeto “Câmera” gerado pela ferramenta não tinha o componente Camera, então
`GetComponent<Camera>()` vinha nulo e o Update caía no primeiro quadro (o toque chegava: botão secundário = bit 2 no
Play). O gerador agora cria a câmera com o componente, como os modelos de cena. O log de diagnóstico do toque e das
falhas de script usado para achar isso saiu do código depois da rodada.

Revisão dos exemplos 2 e 3 (26/10 13:25 e 13:47, APK `0.3.0-dev.261026999-g7-api-1235`, prancha
`capturas/g7-prancha-ex2-ex3-20261026.jpg`): na primeira prancha os dois não mostravam o efeito.
- 2: o visitante caía de 24 m e entrava na área em ~2 s, antes da primeira captura; o sensor também encostava no chão.
  Agora a área fica acima do piso e o visitante cai de 80 m (~4 s): 1 s fora (céu claro) e 7 s dentro (perfil coberto),
  em C# e em Flow.
- 3: sem nada perto da câmera, o foco caía em objetos à mesma distância e o fundo quase não borrava. Agora há um alvo a
  4 m na linha de visão: segurando Mirar, o alvo fica nítido e o fundo desfoca, nas duas variantes e nas duas miradas
  (o teste `g7_generated_aim_scene_touch` também confere a segunda mirada).

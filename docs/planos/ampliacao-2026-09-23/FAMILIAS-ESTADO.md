# Saldo de capacidades Astra

**Universo atual: 91 linhas; 87 obrigatórias e 4 condicionais P19.**
**Encerradas neste registro: 10; ainda sem encerramento comprovado: 77.**

A contagem de 44 famílias de componentes + 18 recursos do resumo original não cobre as 91 linhas atuais do catálogo. Este saldo usa as linhas efetivas, sem converter tipos, receitas ou lotes de 50 em famílias encerradas.

`auditar` significa cobertura ainda não reconciliada, não ausência de implementação. Uma família parcial pode ter muitos consumidores funcionando. Fechar exige conferir cada requisito do catálogo, registrar o aceite e retirar todas as lacunas de implementação. A pedido do usuário, esta rodada usa evidências host/APK; a qualificação física integrada P20 permanece separada. Build e captura host não equivalem a prova física Android.

**Estados obrigatórios:** auditar: 72; concluida: 10; parcial: 5.

| ID | Capacidade | Estado | Evidências / lacunas |
|---|---|---|---|
| F001 | [B Objeto / Unity GameObject, Godot Node](COMPONENTES.md:17) | concluida | `docs/planos/FAMILIAS-BASE-2026-10-01.md`, `docs/validacao/evidencias/families-base-20261001/package-manifest.json`, `tests/native/test_family_base.cpp`, `tests/Aether.Tests/FamilyBaseContractsTests.cs` Limite: Revisão ABI34 não instalada/executada no aparelho; captura é do executável host, não Vulkan Android. |
| F002 | [B Transform 3D / Transform, Node3D](COMPONENTES.md:18) | concluida | `docs/planos/FAMILIAS-BASE-2026-10-01.md`, `docs/validacao/evidencias/families-base-20261001/package-manifest.json`, `tests/native/test_family_base.cpp`, `tests/Aether.Tests/FamilyBaseContractsTests.cs` Limite: Revisão ABI34 não instalada/executada no aparelho; captura é do executável host, não Vulkan Android. |
| F003 | [B Transform 2D / RectTransform/Transform, Node2D](COMPONENTES.md:19) | auditar | Conferir requisitos, consumidores e aceite. |
| F004 | [R/S Cena / Scene, PackedScene/SceneTree](COMPONENTES.md:20) | auditar | Conferir requisitos, consumidores e aceite. |
| F005 | [R Prefab/subcena / Prefab, PackedScene](COMPONENTES.md:21) | parcial | `native/runtime/prefab.cpp`, `native/editor/editor_prefab_overrides.cpp`, `tests/native/test_prefab_structure.cpp`, `docs/planos/PREFAB-COMPONENTES-2026-10-01.md`, `docs/validacao/evidencias/families-prefab-20261001/package-manifest.json`, `docs/planos/PREFAB-COLECOES-2026-10-01.md`, `docs/validacao/evidencias/families-prefab-collections-20261001/package-manifest.json` — Reconciliar adição/remoção/reparent de objetos da árvore.; Nested prefabs e variantes com dependências acíclicas. Limite: Sem execução física Android desta revisão. |
| F006 | [R Receita/preset](COMPONENTES.md:22) | concluida | `docs/planos/RECEITAS-REFERENCIAS-2026-10-01.md`, `tests/native/test_recipe_bindings.cpp`, `docs/validacao/evidencias/families-recipes-20261001/native-pagination.log`, `docs/validacao/evidencias/families-recipes-20261001/package-manifest.json` Limite: Sem instalacao/execucao fisica Android desta revisao. UI e runtime verificados no host; arquivo de receita v3, ABI34 no APK. |
| F007 | [C Comportamento / MonoBehaviour, script em Node](COMPONENTES.md:23) | concluida | `docs/planos/FAMILIA-COMPORTAMENTOS-2026-10-01.md`, `docs/validacao/evidencias/o1bc-scripts-mensagens-20260928/README.md`, `docs/validacao/evidencias/inspecao-scripts-play-20260928/README.md`, `docs/validacao/evidencias/families-prefab-20261001/package-manifest.json` Limite: Cenarios e capturas historicos reconciliados; nao reexecutados nesta revisao. Sem instalacao/execucao fisica nova da ABI34. |
| F008 | [S Eventos/conexões / UnityEvent, signals](COMPONENTES.md:24) | auditar | Conferir requisitos, consumidores e aceite. |
| F009 | [C/S Timer / timers de gameplay, Timer](COMPONENTES.md:25) | concluida | `docs/planos/FAMILIAS-TIMER-GRUPOS-2026-10-01.md`, `docs/validacao/evidencias/timer-controls-abi28-20261001/README.md`, `docs/validacao/evidencias/families-prefab-20261001/package-manifest.json` Limite: Reconciliacao atual usa cenarios host e capturas existentes; nao reexecutados nesta revisao. Pacote ABI34 conferido; sem execucao fisica Android nova. |
| F010 | [R/S Tween / animação de propriedades, Tween](COMPONENTES.md:26) | parcial | `docs/planos/CONSTRAINTS-TWEEN-2026-09-30.md`, `docs/validacao/evidencias/number-tweens-abi30-20261001/README.md` — Sequencia/paralelo compostos ainda sem contrato persistente e scheduler; TransformTween e NumberTween isolados funcionam. |
| F011 | [S Grupos/tags/consultas](COMPONENTES.md:27) | concluida | `docs/planos/FAMILIAS-TIMER-GRUPOS-2026-10-01.md`, `docs/validacao/evidencias/groups-abi25-20261001/README.md`, `docs/validacao/evidencias/families-prefab-20261001/package-manifest.json` Limite: Reconciliacao atual usa cenarios host e capturas existentes; nao reexecutados nesta revisao. Pacote ABI34 conferido; sem execucao fisica Android nova. |
| F012 | [R Mesh / Mesh, ArrayMesh](COMPONENTES.md:35) | auditar | Conferir requisitos, consumidores e aceite. |
| F013 | [C MeshRenderer / MeshFilter+MeshRenderer, MeshInstance3D](COMPONENTES.md:36) | auditar | Conferir requisitos, consumidores e aceite. |
| F014 | [C SkinnedMesh / SkinnedMeshRenderer, MeshInstance3D+Skin+Skeleton3D](COMPONENTES.md:37) | auditar | Conferir requisitos, consumidores e aceite. |
| F015 | [C InstancedRenderer / instancing Unity, MultiMeshInstance3D](COMPONENTES.md:38) | auditar | Conferir requisitos, consumidores e aceite. |
| F016 | [C LODGroup / LODGroup, visibility ranges](COMPONENTES.md:39) | auditar | Conferir requisitos, consumidores e aceite. |
| F017 | [R Shader](COMPONENTES.md:40) | auditar | Conferir requisitos, consumidores e aceite. |
| F018 | [R Material / Material, ShaderMaterial](COMPONENTES.md:41) | auditar | Conferir requisitos, consumidores e aceite. |
| F019 | [R Texture / Texture, Texture2D](COMPONENTES.md:42) | auditar | Conferir requisitos, consumidores e aceite. |
| F020 | [R Sampler/binding](COMPONENTES.md:43) | auditar | Conferir requisitos, consumidores e aceite. |
| F021 | [R RenderTarget / RenderTexture, ViewportTexture](COMPONENTES.md:44) | auditar | Conferir requisitos, consumidores e aceite. |
| F022 | [C Camera / Camera, Camera3D](COMPONENTES.md:45) | auditar | Conferir requisitos, consumidores e aceite. |
| F023 | [C CameraRig / Cinemachine, composição Camera3D+SpringArm3D](COMPONENTES.md:46) | auditar | Conferir requisitos, consumidores e aceite. |
| F024 | [C Decal / URP DecalProjector, Decal](COMPONENTES.md:47) | auditar | Conferir requisitos, consumidores e aceite. |
| F025 | [C Line/Trail / LineRenderer/TrailRenderer, Line2D](COMPONENTES.md:48) | auditar | Conferir requisitos, consumidores e aceite. |
| F026 | [C Light / Light; DirectionalLight3D, OmniLight3D, SpotLight3D](COMPONENTES.md:56) | auditar | Conferir requisitos, consumidores e aceite. |
| F027 | [Configuração de sombra](COMPONENTES.md:57) | auditar | Conferir requisitos, consumidores e aceite. |
| F028 | [C/R Ambiente / Volume, WorldEnvironment](COMPONENTES.md:58) | auditar | Conferir requisitos, consumidores e aceite. |
| F029 | [R Céu/atmosfera](COMPONENTES.md:59) | auditar | Conferir requisitos, consumidores e aceite. |
| F030 | [C ReflectionProbe](COMPONENTES.md:60) | auditar | Conferir requisitos, consumidores e aceite. |
| F031 | [C Irradiance/LightProbe volume](COMPONENTES.md:61) | auditar | Conferir requisitos, consumidores e aceite. |
| F032 | [R Lightmap/bake settings](COMPONENTES.md:62) | auditar | Conferir requisitos, consumidores e aceite. |
| F033 | [C Fog volume](COMPONENTES.md:63) | auditar | Conferir requisitos, consumidores e aceite. |
| F034 | [C/R WaterBody/WaterWorld](COMPONENTES.md:64) | auditar | Conferir requisitos, consumidores e aceite. |
| F035 | [C Corpo 3D / Rigidbody, RigidBody3D/StaticBody3D/AnimatableBody3D](COMPONENTES.md:72) | auditar | Conferir requisitos, consumidores e aceite. |
| F036 | [C Collider box/sphere/capsule](COMPONENTES.md:73) | auditar | Conferir requisitos, consumidores e aceite. |
| F037 | [C Collider convex/mesh/heightfield](COMPONENTES.md:74) | auditar | Conferir requisitos, consumidores e aceite. |
| F038 | [C Composição de colisores](COMPONENTES.md:75) | auditar | Conferir requisitos, consumidores e aceite. |
| F039 | [R PhysicsMaterial](COMPONENTES.md:76) | auditar | Conferir requisitos, consumidores e aceite. |
| F040 | [C Sensor/Area](COMPONENTES.md:77) | auditar | Conferir requisitos, consumidores e aceite. |
| F041 | [C Joint fixo/hinge/slider/spring/6DOF](COMPONENTES.md:78) | auditar | Conferir requisitos, consumidores e aceite. |
| F042 | [C CharacterMotor / CharacterController, CharacterBody3D](COMPONENTES.md:79) | auditar | Conferir requisitos, consumidores e aceite. |
| F043 | [S Physics queries](COMPONENTES.md:80) | auditar | Conferir requisitos, consumidores e aceite. |
| F044 | [C/R Ragdoll](COMPONENTES.md:81) | auditar | Conferir requisitos, consumidores e aceite. |
| F045 | [C/S Physics2D](COMPONENTES.md:82) | auditar | Conferir requisitos, consumidores e aceite. |
| F046 | [R/S ActionMap/InputContext](COMPONENTES.md:90) | auditar | Conferir requisitos, consumidores e aceite. |
| F047 | [C/R Canvas/root UI](COMPONENTES.md:91) | auditar | Conferir requisitos, consumidores e aceite. |
| F048 | [C Rect/layout](COMPONENTES.md:92) | auditar | Conferir requisitos, consumidores e aceite. |
| F049 | [C Containers row/column/grid/flow](COMPONENTES.md:93) | auditar | Conferir requisitos, consumidores e aceite. |
| F050 | [C Image/NineSlice](COMPONENTES.md:94) | auditar | Conferir requisitos, consumidores e aceite. |
| F051 | [C Text/RichText](COMPONENTES.md:95) | auditar | Conferir requisitos, consumidores e aceite. |
| F052 | [C Button/Toggle/Radio](COMPONENTES.md:96) | auditar | Conferir requisitos, consumidores e aceite. |
| F053 | [C Slider/Range/Progress](COMPONENTES.md:97) | auditar | Conferir requisitos, consumidores e aceite. |
| F054 | [C Scroll/List/Tree/Dropdown](COMPONENTES.md:98) | auditar | Conferir requisitos, consumidores e aceite. |
| F055 | [C TextInput](COMPONENTES.md:99) | auditar | Conferir requisitos, consumidores e aceite. |
| F056 | [R Theme/Style](COMPONENTES.md:100) | auditar | Conferir requisitos, consumidores e aceite. |
| F057 | [S Accessibility/Localization](COMPONENTES.md:101) | auditar | Conferir requisitos, consumidores e aceite. |
| F058 | [R AudioClip/Stream](COMPONENTES.md:102) | parcial | `docs/planos/AUDIO-2026-09-30.md`, `tests/native/test_runtime_audio.cpp` — Streaming de clipe e pontos de loop autorados nao implementados; WAV decodificado/cached e loop inteiro funcionam. |
| F059 | [C AudioSource 2D/3D](COMPONENTES.md:103) | parcial | `docs/planos/AUDIO-2026-09-30.md`, `docs/planos/AUDIO-API-ABI-2026-09-30.md` — Prioridade/preempcao de fontes e mistura espacial continua ainda nao implementadas; dimensao plana/espacial e demais propriedades possuem consumidor. |
| F060 | [C AudioListener](COMPONENTES.md:104) | concluida | `docs/planos/FAMILIA-LISTENER-2026-10-01.md`, `docs/validacao/evidencias/families-audio-listener-20261001/package-manifest.json`, `docs/validacao/evidencias/families-audio-listener-20261001/native-final.log` Limite: Captura e cenarios executados no host. APK conferido; revisao nao instalada/executada fisicamente no Android. |
| F061 | [R/S Mixer/buses/effects](COMPONENTES.md:105) | parcial | `docs/planos/AUDIO-2026-09-30.md` — Sends, efeitos DSP/parametros e snapshots ainda nao implementados; buses atuais executam apenas encadeamento de ganho, mute/solo. |
| F062 | [R AnimationClip](COMPONENTES.md:113) | auditar | Conferir requisitos, consumidores e aceite. |
| F063 | [C AnimationPlayer](COMPONENTES.md:114) | auditar | Conferir requisitos, consumidores e aceite. |
| F064 | [R Animator graph](COMPONENTES.md:115) | auditar | Conferir requisitos, consumidores e aceite. |
| F065 | [C/R Rig/IK/constraints](COMPONENTES.md:116) | auditar | Conferir requisitos, consumidores e aceite. |
| F066 | [R/C Timeline](COMPONENTES.md:117) | auditar | Conferir requisitos, consumidores e aceite. |
| F067 | [C NavigationRegion/Surface](COMPONENTES.md:118) | auditar | Conferir requisitos, consumidores e aceite. |
| F068 | [C NavigationAgent](COMPONENTES.md:119) | auditar | Conferir requisitos, consumidores e aceite. |
| F069 | [C NavigationObstacle/Link](COMPONENTES.md:120) | auditar | Conferir requisitos, consumidores e aceite. |
| F070 | [C Perception/state logic](COMPONENTES.md:121) | auditar | Conferir requisitos, consumidores e aceite. |
| F071 | [C Sprite/AnimatedSprite](COMPONENTES.md:122) | auditar | Conferir requisitos, consumidores e aceite. |
| F072 | [R TileSet / C TileMapLayer](COMPONENTES.md:123) | auditar | Conferir requisitos, consumidores e aceite. |
| F073 | [C Camera2D/Parallax](COMPONENTES.md:124) | auditar | Conferir requisitos, consumidores e aceite. |
| F074 | [C Light2D/Occluder2D](COMPONENTES.md:125) | auditar | Conferir requisitos, consumidores e aceite. |
| F075 | [C/R ParticleEmitter](COMPONENTES.md:126) | auditar | Conferir requisitos, consumidores e aceite. |
| F076 | [R Curve/Gradient](COMPONENTES.md:127) | concluida | `docs/planos/CURVAS-GRADIENTES-2026-10-01.md`, `docs/validacao/evidencias/families-curves-gradients-20261001/package-manifest.json`, `docs/validacao/evidencias/families-curves-gradients-20261001/native-final.log`, `docs/validacao/evidencias/families-curves-gradients-20261001/managed-sampling-final.log`, `docs/validacao/evidencias/families-curves-gradients-20261001/managed-behavior-final.log` Limite: Captura e cenarios executados no host. APK conferido; revisao nao instalada/executada fisicamente no Android. |
| F077 | [R Spline / C Path](COMPONENTES.md:128) | concluida | `docs/planos/SPLINE-FRAME-2026-10-01.md`, `docs/validacao/evidencias/families-spline-frame-20261001/package-manifest.json`, `docs/validacao/evidencias/families-spline-frame-20261001/native-final.log`, `docs/validacao/evidencias/families-spline-frame-20261001/managed-transport-final.log` Limite: Host e pacote ABI35; revisao nao instalada/executada fisicamente no Android. |
| F078 | [C PathFollower](COMPONENTES.md:129) | concluida | `docs/planos/FAMILIA-SEGUIDOR-CAMINHO-2026-10-01.md`, `tests/native/test_scene_paths.cpp`, `tests/native/test_script_paths.cpp`, `docs/validacao/evidencias/p15a-android-20260930/README.md`, `docs/validacao/evidencias/families-recipes-20261001/package-manifest.json`, `docs/planos/SPLINE-FRAME-2026-10-01.md`, `docs/validacao/evidencias/families-spline-frame-20261001/package-manifest.json`, `docs/validacao/evidencias/families-spline-frame-20261001/native-final.log` Limite: Aceite P15a anterior reconciliado; nao reexecutado. Pacote ABI34 conferido, sem execucao fisica desta revisao. |
| F079 | [C Terrain](COMPONENTES.md:130) | auditar | Conferir requisitos, consumidores e aceite. |
| F080 | [C Foliage/scatter](COMPONENTES.md:131) | auditar | Conferir requisitos, consumidores e aceite. |
| F081 | [S World streaming](COMPONENTES.md:132) | auditar | Conferir requisitos, consumidores e aceite. |
| F082 | [S SaveGame](COMPONENTES.md:138) | auditar | Conferir requisitos, consumidores e aceite. |
| F083 | [E Import profiles](COMPONENTES.md:139) | auditar | Conferir requisitos, consumidores e aceite. |
| F084 | [E Inspector extensions](COMPONENTES.md:140) | auditar | Conferir requisitos, consumidores e aceite. |
| F085 | [E Code/graph workspace](COMPONENTES.md:141) | auditar | Conferir requisitos, consumidores e aceite. |
| F086 | [E Profiler/debug overlays](COMPONENTES.md:142) | auditar | Conferir requisitos, consumidores e aceite. |
| F087 | [S Build/export profiles](COMPONENTES.md:143) | auditar | Conferir requisitos, consumidores e aceite. |
| F088 | [C/S Network identity/replication](COMPONENTES.md:144) | condicional | Conferir requisitos, consumidores e aceite. |
| F089 | [C XR origin/controller/AR data](COMPONENTES.md:145) | condicional | Conferir requisitos, consumidores e aceite. |
| F090 | [C VideoPlayer](COMPONENTES.md:146) | condicional | Conferir requisitos, consumidores e aceite. |
| F091 | [C Vehicle/Cloth/SoftBody](COMPONENTES.md:147) | condicional | Conferir requisitos, consumidores e aceite. |

Requisitos completos, dependências e evidências revisadas são mantidos em `FAMILIAS-ESTADO.json`; este Markdown é gerado por `tools/report-family-progress.py`. O atlas é referência, não confirmação automática de suporte.

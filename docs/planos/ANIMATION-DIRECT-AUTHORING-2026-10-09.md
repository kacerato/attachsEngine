# Autoria direta e atmosfera — 09/10/2026

## Objetivo e recorte

Fechar o caminho selecionar uma articulação real → criar/reutilizar canal →
preparar pose FK → gravar → interpolar → salvar/reabrir → executar o recurso.
O editor permanece geral: objetos de mecanismo, câmeras e rigs usam TRS; morphs
continuam no caminho numérico. Não há um novo controlador exclusivo do Kyle.

## Origem do exemplo anterior

`tests/native/animation_package_project_fixture.h` importou Robot Kyle sem
nenhum clipe. O gerador usou `createAnimationClip` (2 s, Euler),
`addAnimationClipTrack` no objeto `Right_Upper_Arm_Joint_01`, `seekAnimationClip`
(1 s), `previewAnimationClipPose` (Z local +35°) e `recordAnimationClipPose`.
As poses de repouso em 0/2 s já existiam no canal. Depois atribuiu o recurso a
`scene::Animation`. Portanto houve autoria pelo backend real, programaticamente;
não foi playback importado e não foi uma operação manual no touch. Os três
Vikings tinham clipes importados e rigs distintos. Isso não prova retargeting.

## Referências concretas

- Godot **4.5**, [introdução à animação](https://docs.godotengine.org/en/4.5/tutorials/animation/introduction.html):
  seleção, tempo, keyframe e curva como um fluxo, sem confundir recurso/Player.
- Godot **4.5**, [editor de Skeleton3D](https://github.com/godotengine/godot/blob/4.5/editor/scene/3d/skeleton_3d_editor_plugin.cpp):
  seleção de ossos e transformação editável precisam permanecer distintas do
  rest e da avaliação do runtime. Astra usa os IDs reais de `SkinnedMesh::bones`.
- Unity Learn **2019.4**, [Create keyframed animation clips](https://learn.unity.com/tutorial/create-keyframed-animation-clips?version=2019.4):
  criar → escolher tempo → manipular → gravar → conferir no Play. A adaptação
  mobile conserva viewport e timeline, revelando XYZ somente quando solicitado.
- Godot **4.5**, [ProceduralSkyMaterial](https://docs.godotengine.org/en/4.5/classes/class_proceduralskymaterial.html):
  cores superior/horizonte/inferior autoráveis são diferentes de espalhamento
  físico. Não fingimos que as cores procedurais recolorem o modelo físico.

## Cadeia e ownership

SceneGraph/SkinnedMesh (IDs de juntas) → cache de topologia por revisão da cena →
SceneGraph avaliado do AnimationClipPreview → projeção e escolha no viewport →
canal AECLIP existente ou criação explícita → rascunho de pose → journal de
recursos → serialização → sampler/compositor utilizado pelo Play.

Selecionar, focalizar e navegar não criam recursos nem chaves. Canais novos só
são publicados pela ação TRS explícita; canais existentes conservam seu modo de
rotação. Não se converte Euler para Quaternion silenciosamente. A camada isolada
edita valores crus; resultado composto continua disponível. Rest e documento
fonte permanecem intactos. Topologia é reconstruída quando muda a revisão da
cena; posições de juntas são obtidas da pose avaliada, sem duplicar o esqueleto.

## UX e limites

NÃO IREI SER SIMPLISTA NO DESIGN.

Modo Pose oferece juntas em raio X, TRS, XYZ, foco e navegação. Toque curto
seleciona; arraste mantém navegação; gizmos mantêm captura própria. Dois dedos
continuam pan/pinch. A árvore é a alternativa exata para dedos/ossos sobrepostos.
O overlay não promete oclusão anatômica nem seleção de polígonos/pesos do skin.
Ampliação temporária da prévia empresta o espaço das chaves ao viewport,
mantendo transporte/tempo. A timeline retorna por outro toque, sem trocar clipe.
Picking de malhas é limitado à raiz antes da construção/deformação dos
candidatos, reutiliza a pose deformada e respeita a visibilidade do editor.
FK é a hierarquia real de transforms; não há solver IK novo neste recorte.
Eventos/drivers, espelho de rig, retargeting e bake FK/IK/root motion permanecem
separados: exigem dados de rig, constraints, solver, referências, lifecycle e API.

## Atmosfera

Novos ambientes procedurais e fallback da Scene View recebem cores diurnas
lineares: zênite (0,14; 0,38; 0,72), horizonte (0,46; 0,66; 0,82), inferior
(0,18; 0,32; 0,50). Ambientes salvos conservam suas cores. O shader une as duas
metades exatamente na cor de horizonte, com smoothstep e derivada zero na
fronteira. O halo usa função suave em y². Não acrescenta textura, LUT, passada,
loop ou integração por pixel. SPIR-V deve ser regenerado/validado junto à fonte.
O laboratório de pacotes usa explicitamente esse céu procedural, preservando
luz direcional, materiais e sombras. PhysicalAtmosphere permanece disponível
com seu solo planetário e scattering existentes; não sofreu recoloração fake.

## Validação desta revisão

ADB indisponível por solicitação do usuário. Registrar build, cenários host,
capturas reais do UI raster e persistência separadamente. Captura de UI no host
não é prova do renderer Vulkan/skin no Android. A proposta gerada a partir da
captura anterior é hipótese visual; somente a UI executável conta como entrega.

### Resultado em 10/10/2026

- Build final: `aether_animation_clip_editor_tests`, `aether_gui_preview` e
  `aether_environment_authoring_tests` compilados e ligados, sem erros.
- Animação: **39/39** na revisão final, após quatro rodadas completas de
  investigação também aprovadas. Cenário novo cobre juntas reais, seleção sem
  publicação, canal explícito/reuso, FK, pose gravada, câmera, preview expandido,
  reabertura fria e consumo pelo Play. Ambiente: **16/16**.
- Uma recusa numérica perdia o motivo ao reinicializar o preview: corrigida e
  protegida por alteração real da cena enquanto o campo recusado permanece
  aberto. Uma falha inicial de recomposição após Undo não reapareceu nas quatro
  rodadas instrumentadas nem na revisão final; não se atribui uma causa não
  demonstrada. O cenário agora inclui estado, diagnóstico, pose e profundidade
  de histórico na falha. A interface preserva o motivo real de uma recusa de
  replay, em vez de escondê-lo atrás de uma mensagem genérica.
- Céu: fonte e SPIR-V regenerados/validados; não houve inspeção visual nova do
  renderer Vulkan. C# do tutorial compilado contra o SDK atual: zero erros/avisos.
- Laboratório regenerado: **4 fontes / 4 clipes autorais**, origem preservada,
  reabertura e consumidor real aprovados. Projeto local em
  `build/AnimationStudioPacotes-direct-20261010`; assets privados fora do Git.
- Capturas executáveis de UI/juntas em 853×394 e 655×300, com preview expandido,
  inspecionadas. O raster não desenha a malha 3D; sua imagem não é prova de skin
  Vulkan, atmosfera renderizada, touch físico ou desempenho sustentado.
- Evidência selecionada em `docs/validacao/animation-direct-2026-10-10/`;
  documentação pública e Atualizações descrevem disponibilidade em fonte,
  tutorial reproduzível, API e limitações. O APK público 0.3.0 não foi trocado.

Nenhum solver IK, retargeting, evento/driver, biblioteca integral dos plugins ou
bake FK/IK/root motion é anunciado como entregue por este bloco. Não houve ADB,
compilação/instalação Android ou novo vídeo nesta revisão.

#pragma once
#include "core/base.h"
#include <cstddef>

namespace ae::scene {
// A ABI é independente de Editor e de headers do CLR. Todos os callbacks e
// chamadas de runtime executam na thread dona do mundo, e os buffers só valem
// durante a chamada.
//
// **Compatibilidade:** os campos de v2 permanecem nas mesmas posições. Um
// consumidor que declare `version==2` continua encontrando o que esperava; o
// runtime gerenciado exige a versão corrente para usar os campos novos e recusa `size`
// divergente, porque uma struct maior do que a acordada seria lida além do fim.
// Pacotes de consulta requerem a versão corrente: ABI44 acrescenta o objeto do
// Collider ao resultado (64 bytes). Consumidores ABI43 devem ser recompilados;
// não chamar consultas sem negociar versão, mesmo se os slots não mudaram.
//
// Convenções dos campos novos:
//   • `u64` de objeto é o id persistente no mundo; zero é "nenhum".
//   • `generation` devolve zero para objeto destruído — é assim que um handle
//     guardado por um script vence ainda dentro do mesmo callback.
//   • funções `int` devolvem 1/0 para sucesso/recusa e -1 quando o argumento
//     nem chega a ser avaliado; `lastStatus` traz o motivo real da última recusa.
//   • propriedade trafega como (kind, bits): 0=float (padrão IEEE em 32 bits),
//     1=bool, 2=enum u32, 3=referência de objeto.

// Consultas físicas na fronteira: POD puro, buffers do chamador, identidade de
// OBJETO (nunca handle de corpo nativo). `flags` do filtro: bit0 estático,
// bit1 dinâmico, bit2 inclui sensores. `flags` do acerto: bit0 tem normal,
// bit1 o corpo acertado é sensor.
struct ScriptQueryFilter {
  u32 size=sizeof(ScriptQueryFilter);
  u32 gameplayLayerMask=0xffffffffu;
  u32 flags=3;
  u32 reserved=0;
  u64 ignore=0;
};
struct ScriptQueryHit {
  u64 object=0;
  u64 collider=0;
  float point[3]{};
  float normal[3]{};
  float distance=0;
  float fraction=0;
  u32 flags=0;
  u32 reserved=0;
  u64 colliderObject=0;
};
static_assert(sizeof(ScriptQueryHit)==64 && offsetof(ScriptQueryHit,colliderObject)==56);
struct ScriptShapeQuery {
  u32 kind=1; // 0 caixa, 1 esfera, 2 cápsula
  float halfExtent[3]{.5f,.5f,.5f};
  float radius=.5f;
  float halfHeight=.5f;
  float rotation[4]{0,0,0,1};
};

struct ScriptTweenState {u32 size=24,status=0;double elapsed=0;u32 flags=0,reserved=0;};
static_assert(sizeof(ScriptTweenState)==24);

struct ScriptAssetGuid { u64 high=0,low=0; };

// ABI v9: animação (Animation legado da Unity). `op`: 0 Play, 1 CrossFade,
// 2 Blend, 3 Stop, 4 Rewind. Clipe zero é o clipe padrão em Play/CrossFade e
// todos os clipes em Stop/Rewind. `playMode`: 0 para a mesma camada, 1 para
// todas. `seconds` é a duração do fade; `targetWeight` o peso do Blend.
struct ScriptAnimationCommand {
  u32 size=sizeof(ScriptAnimationCommand),op=0;
  ScriptAssetGuid clip{};
  float seconds=0,targetWeight=1;
  u32 playMode=0,reserved=0;
};
// AnimationState: `wrapMode` na ordem de resources::AnimationWrapMode.
struct ScriptAnimationState {
  u32 size=sizeof(ScriptAnimationState),enabled=0;
  ScriptAssetGuid clip{};
  float time=0,speed=1,weight=0,length=0;
  u32 layer=0,wrapMode=1;
};

// ABI v7. Enums cross as u32; booleans also use u32 so C++/CLR layout is stable.
// v7 appends temporal upscaler quality, availability and the executed algorithm.
// The settings block mirrors ProjectRenderingSettings in one place at the ABI.
struct ScriptRenderingSettings {
  u32 size=sizeof(ScriptRenderingSettings),schemaVersion=0;
  u32 preset=0,shadows=0,ambient=0,post=0,textures=0,waterMesh=0;
  float resolutionScale=0; u32 maximumRenderHz=0;
  u32 shadowCascadeCount=0,shadowCascadeResolution=0,shadowFilterTaps=0,shadowFarFilterTaps=0;
  float shadowMaximumDistance=0,shadowDepthBiasConstant=0,shadowDepthBiasSlope=0,shadowNormalOffsetTexels=0;
  u32 staticShadowCache=0; float shadowCacheGuardBandRatio=0,shadowCascadeBlendRatio=0,shadowDistanceFadeRatio=0;
  float lodPixelErrorBudget=0,coverageLodPixelErrorBudget=0,lodHysteresisBandRatio=0;
  u32 lodSelection=0,materialShaderVariants=0,environmentSplitSumBrdf=0;
  float normalMapMaximumDistance=0,specularProbeMaximumDistance=0,metallicRoughnessMaximumDistance=0;
  float emissiveMaximumDistance=0,materialDetailFadeBandRatio=0; u32 thermalDistanceScaling=0;
  u32 antiAliasing=0,upscalingFilter=0,postFxaa=0,postVignette=0;
  float bloomThreshold=0,bloomIntensity=0,postContrast=0,postSaturation=0,postSharpen=0,temporalHistoryWeight=0;
  u32 dynamicResolution=0; float dynamicResolutionMinimumScale=0,dynamicResolutionDecreaseStep=0;
  float dynamicResolutionIncreaseStep=0,dynamicResolutionRecoveryHeadroomRatio=0;
  u32 dynamicResolutionOverloadFrames=0,dynamicResolutionRecoveryFrames=0;
  u32 temporalUpscalerQuality=0;
  // S2: Mipmap Streaming. Anexado ao fim; o tamanho exato separa as versões.
  u32 textureStreaming=0,textureStreamingBudgetMegabytes=0,textureStreamingMaxLevelReduction=0;
  u32 textureStreamingUploadKilobytesPerFrame=0;
};

struct ScriptResolvedRenderingPolicy {
  u32 size=sizeof(ScriptResolvedRenderingPolicy);
  u32 renderHz=0,simulationHz=0; float frameIntervalMs=0,cpuLaneBudgetMs=0,gpuLaneBudgetMs=0,compositorReserveMs=0;
  u32 hzbMinimumCandidateDraws=0,hzbHysteresisFrames=0; float hzbNormalizedDepthBias=0,lodPixelErrorBudget=0;
  float coverageLodPixelErrorBudget=0,lodHysteresisBandRatio=0;
  u32 shadowsEnabled=0,shadowCascadeCount=0,shadowCascadeResolution=0,shadowFilterTaps=0,shadowFarFilterTaps=0;
  float shadowMaximumDistance=0,shadowDepthBiasConstant=0,shadowDepthBiasSlope=0,shadowNormalOffsetTexels=0;
  u32 shadowStabilizeTexelSnap=0,staticShadowCache=0; float shadowCacheGuardBandRatio=0,shadowCascadeBlendRatio=0,shadowDistanceFadeRatio=0;
  u32 ambientHemispheric=0,ambientSpecularProbe=0,ambientSplitSumBrdf=0;
  u32 postDedicatedPass=0,postBloom=0,antiAliasing=0,upscalingFilter=0,postVignette=0;
  float bloomThreshold=0,bloomIntensity=0,postContrast=0,postSaturation=0,postSharpen=0,vignetteIntensity=0,temporalHistoryWeight=0;
  u32 lodSelection=0,materialShaderVariants=0,waterMesh=0,textureResidencyMipBias=0;
  float samplerAnisotropy=0,normalMapMaximumDistance=0,specularProbeMaximumDistance=0;
  float metallicRoughnessMaximumDistance=0,emissiveMaximumDistance=0,materialDetailFadeBandRatio=0;
  u32 dynamicResolutionEnabled=0; float dynamicResolutionMinimumScale=0,dynamicResolutionMaximumScale=0;
  float dynamicResolutionDecreaseStep=0,dynamicResolutionIncreaseStep=0,dynamicResolutionRecoveryHeadroomRatio=0;
  u32 dynamicResolutionOverloadFrames=0,dynamicResolutionRecoveryFrames=0;
  float resolutionScale=0; u32 effectiveProfile=0,clampCount=0;
  u32 temporalUpscalerQuality=0;
  u32 textureStreaming=0,textureStreamingBudgetMegabytes=0,textureStreamingMaxLevelReduction=0;
  u32 textureStreamingUploadKilobytesPerFrame=0;
};

// S5: estatísticas do último quadro da cena (zero na GPU = não medida).
struct ScriptSceneStatistics {
  float frameIntervalMs=0,gpuFrameMs=0;
  u32 renderWidth=0,renderHeight=0,drawCalls=0,lodDraws=0,lodReducedDraws=0,reserved=0;
  u64 triangles=0,lodBaseTriangles=0,lodSelectedTriangles=0;
};

// S2: Texture.*TextureMemory e contagens do streaming no último quadro.
struct ScriptTextureStreamingStats {
  u64 budgetBytes=0,totalBytes=0,desiredBytes=0,targetBytes=0,currentBytes=0,nonStreamingBytes=0;
  u64 uploadedBytesLastFrame=0;
  u32 active=0,overBudget=0,streamingTextures=0,pendingLoads=0,budgetReducedTextures=0;
  u32 uploadsLastFrame=0,failedUploads=0,reserved=0;
};

struct ScriptRenderingCapabilities {
  u32 size=sizeof(ScriptRenderingCapabilities),profile=0,recommendedProfile=0,recommendationSource=0;
  u32 maximumImage2DSize=0,maximumImageArrayLayers=0,supportsDepthSampling=0;
  float maximumSamplerAnisotropy=0,displayHz=0;
  u32 armAsr=0,fsr2=0; // renderer::TemporalUpscalerAvailability
};
struct ScriptRenderingState {
  u32 size=sizeof(ScriptRenderingState),world=0,pending=0,lastRequestSucceeded=0,effectiveAvailable=0;
  u64 pendingRequestId=0;
  ScriptRenderingSettings requested{};
  ScriptResolvedRenderingPolicy effective{};
  ScriptRenderingCapabilities capabilities{};
  // What the renderer actually ran in the last presented frame, and why a
  // requested temporal upscaler did not run (TemporalUpscalerAvailability).
  u32 executedUpscaler=0,executedStatus=0;
  ScriptTextureStreamingStats textureStreaming{};
  ScriptSceneStatistics frame{};
};

struct ScriptAudioSnapshot {
  u32 size=sizeof(ScriptAudioSnapshot),state=0,outputRunning=0,reserved=0;
  double cursor=0;
};
static_assert(sizeof(ScriptAudioSnapshot)==24);

struct ScriptTimeState {
  u32 size=sizeof(ScriptTimeState),reserved=0;
  u64 frameCount=0;
  double simulationTime=0,unscaledTime=0;
  float delta=0,unscaledDelta=0,timeScale=1,frameScale=1;
};
static_assert(sizeof(ScriptTimeState)==48);
struct ScriptInputBinding {
  u32 source=0,code=0,negativeCode=0,axis=0;
  float scale=1;u32 invert=0;
};
static_assert(sizeof(ScriptInputBinding)==24);
struct ScriptInputActionState {
  u32 size=sizeof(ScriptInputActionState),flags=0,interaction=0,deviceGroups=0,phase=0;
  float duration=0,progress=0,elapsed=0;
};
static_assert(sizeof(ScriptInputActionState)==32);

struct ScriptTimerState {u32 size=sizeof(ScriptTimerState),flags=0;double remaining=0;};
static_assert(sizeof(ScriptTimerState)==16);

struct ScriptNumberTweenParameters {u32 size=24,easing=0;float destination=0,duration=1;u32 flags=0,reserved=0;};
struct ScriptNumberTweenState {u32 size=32,status=0,failure=0,flags=0;double elapsed=0;float value=0,duration=0;};
static_assert(sizeof(ScriptNumberTweenParameters)==24&&sizeof(ScriptNumberTweenState)==32);
struct ScriptCharacterState {u32 size=80,groundState=3,flags=0,reserved=0;float position[3]{},velocity[3]{},motorVelocity[3]{},groundVelocity[3]{},groundNormal[3]{};u32 tailReserved=0;};
static_assert(sizeof(ScriptCharacterState)==80);
struct ScriptFieldState {u32 size=64,flags=0;float weight=0,acceleration[3]{},windVelocity[3]{};float windDrag=0,linearDrag=0,angularDrag=0,overrideWeight=0;u32 affectedBodies=0;float affectedMass=0;u32 reserved=0;};
static_assert(sizeof(ScriptFieldState)==64);
struct ScriptBodyState {u32 size=48,flags=0;float linear[3]{},angular[3]{},centerOfMass[3]{};u32 reserved=0;};
static_assert(sizeof(ScriptBodyState)==48);
struct ScriptGuiState {
  u32 world=0,node=0,kind=0,visible=0,enabled=0;
  float value=0,minimum=0,maximum=1;
  u32 event=0; // 0 none, 1 click, 2 value changed
};
static_assert(sizeof(ScriptGuiState)==36);
struct ScriptGuiProperties {
  float anchors[4]{},offsets[4]{};
  u32 background=0,foreground=0,accent=0,clipChildren=0;
  float fontSize=18,radius=0;
};
static_assert(sizeof(ScriptGuiProperties)==56);

struct ScriptGuiSizing {
  float minimum[2]{},preferred[2]{},flexible[2]{},padding[4]{},spacing[2]{};
  u32 alignment=0,columns=2,ignore=0,imageFit=0,imageTint=0xFFFFFFFF;
};
static_assert(sizeof(ScriptGuiSizing)==68);
struct ScriptGuiCanvas {
  u32 mode=0;float resolution[2]{},position[3]{},rotation[3]{},unitsPerPixel=0;u32 occlusion=0;
};
static_assert(sizeof(ScriptGuiCanvas)==44);
struct ScriptGuiBehavior {
  u32 clickable=0,action=0,target=0;float value=0;
  u32 enabled=0,autoPlay=0,loop=0,pingPong=0,easing=1;
  float duration=.3f,delay=0,from[4]{0,0,1,1},to[4]{0,0,1,1};
};
static_assert(sizeof(ScriptGuiBehavior)==76);
struct ScriptGuiAction {u32 event=1,action=0,target=0;float value=0;};
static_assert(sizeof(ScriptGuiAction)==16);
struct ScriptGuiTransitions {
  u32 enabled=0,easing=1;float duration=.12f;
  float poses[12]{0,0,1,1,0,0,.94f,1,0,0,1,.45f};
  u32 tints[3]{0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF};
};
static_assert(sizeof(ScriptGuiTransitions)==72);
struct ScriptGuiControl {
  u32 mode=0,axis=0,gate=0,showBase=1,showKnob=1,pressed=0,pointer=0,device=0;
  float inputRadius=80,baseRadius=80,knobRadius=28,deadzone=.12f,outerDeadzone=0,exponent=1,sensitivity=1,returnSeconds=.12f,x=0,y=0;
};
static_assert(sizeof(ScriptGuiControl)==72);

struct ScriptSceneAccess {
  u32 version=45,size=sizeof(ScriptSceneAccess);
  void *context=nullptr;
  int (*exists)(void *,u64)=nullptr;
  int (*getTransform)(void *,u64,float *)=nullptr; // position3 quaternion4 scale3, local space
  int (*setTransform)(void *,u64,const float *)=nullptr;
  int (*setVelocity)(void *,u64,const float *)=nullptr; // world space
  int (*moveKinematic)(void *,u64,const float *)=nullptr; // world position3 quaternion4
  void (*log)(void *,u64,const u8 *,int)=nullptr;
  int (*bodyForce)(void *,u64,const float *,u32)=nullptr; // force/impulse/torque/angular impulse
  int (*getVelocity)(void *,u64,float *)=nullptr;
  // --- v3: identidade -----------------------------------------------------
  u32 (*worldId)(void *)=nullptr;
  u32 (*generation)(void *,u64)=nullptr;
  u32 (*lastStatus)(void *)=nullptr;
  // --- v3: hierarquia -----------------------------------------------------
  u64 (*parentOf)(void *,u64)=nullptr;
  int (*childCount)(void *,u64)=nullptr;
  u64 (*childAt)(void *,u64,u32)=nullptr;
  u64 (*findChild)(void *,u64,const u8 *,int,int)=nullptr;
  int (*getName)(void *,u64,u8 *,int)=nullptr;
  int (*setName)(void *,u64,const u8 *,int)=nullptr;
  int (*getActive)(void *,u64)=nullptr;
  int (*setActive)(void *,u64,int)=nullptr;
  // --- v3: ciclo de vida --------------------------------------------------
  u64 (*createObject)(void *,u64,const u8 *,int)=nullptr;
  int (*destroyObject)(void *,u64)=nullptr;
  int (*setParent)(void *,u64,u64,u32)=nullptr;
  // --- v3: componentes ----------------------------------------------------
  int (*componentCount)(void *,u64)=nullptr;
  u64 (*componentAt)(void *,u64,u32,u8 *,int)=nullptr;
  u64 (*findComponent)(void *,u64,const u8 *,int,u32)=nullptr;
  u64 (*addComponent)(void *,u64,const u8 *,int)=nullptr;
  int (*removeComponent)(void *,u64,u64)=nullptr;
  int (*getProperty)(void *,u64,u64,const u8 *,int,u32 *,u64 *)=nullptr;
  int (*setProperty)(void *,u64,u64,const u8 *,int,u32,u64)=nullptr;
  // --- v3: transform de mundo --------------------------------------------
  int (*getWorldTransform)(void *,u64,float *)=nullptr;
  int (*setWorldTransform)(void *,u64,const float *)=nullptr;
  // --- v4: consultas físicas ---------------------------------------------
  // Todas devolvem a contagem REAL de acertos, que pode exceder `capacity`:
  // quem chama decide se repete com um buffer maior, em vez de receber um
  // resultado truncado sem saber. -1 significa argumento inválido.
  int (*rayCast)(void *,const float *,const float *,const ScriptQueryFilter *,ScriptQueryHit *,int)=nullptr;
  int (*shapeCast)(void *,const ScriptShapeQuery *,const float *,const float *,const ScriptQueryFilter *,ScriptQueryHit *)=nullptr;
  int (*overlap)(void *,const ScriptShapeQuery *,const float *,const ScriptQueryFilter *,ScriptQueryHit *,int)=nullptr;
  // Camadas de gameplay do projeto, para que um script possa montar a máscara
  // por nome em vez de por índice mágico.
  int (*layerByName)(void *,const u8 *,int)=nullptr;
  int (*layerName)(void *,u32,u8 *,int)=nullptr;
  // --- v5: entrada por ações ---------------------------------------------
  // O núcleo não conhece "Mover" nem "Saltar": o projeto nomeia suas ações e
  // diz quais papéis elas cumprem. `inputRole` traduz papel (0 mover, 1 olhar,
  // 2 saltar) no nome escolhido, para um script que queira o padrão do projeto.
  int (*inputAxis)(void *,const u8 *,int,float *)=nullptr;      // escreve x,y
  int (*inputButton)(void *,const u8 *,int,u32)=nullptr;        // 0 down, 1 pressed, 2 released
  int (*inputContext)(void *,const u8 *,int,int)=nullptr;       // -1 apenas consulta
  int (*inputRole)(void *,u32,u8 *,int)=nullptr;
  // --- v6: política gráfica de execução e recursos tipados ---------------
  int (*getRenderingState)(void *,u32,ScriptRenderingState *)=nullptr;
  int (*setRenderingSettings)(void *,u32,const ScriptRenderingSettings *,u64 *)=nullptr;
  int (*copyRenderingDiagnostics)(void *,u32,u8 *,int)=nullptr;
  int (*getComponentResource)(void *,u64,u64,const u8 *,int,u32,ScriptAssetGuid *)=nullptr;
  int (*setComponentResource)(void *,u64,u64,const u8 *,int,u32,ScriptAssetGuid)=nullptr;
  // ABI v7: valores definidos pelo schema por slot (float=0, enum=2).
  int (*getComponentSlotProperty)(void *,u64,u64,const u8 *,int,u32,u32 *,u64 *)=nullptr;
  int (*setComponentSlotProperty)(void *,u64,u64,const u8 *,int,u32,u32,u64)=nullptr;
  // ABI v8: comandos de gameplay. Movimento recebe right/forward em [-1,1]
  // e yaw em radianos; olhar recebe delta normalizado da viewport (x,y).
  int (*characterMove)(void *,u64,const float *)=nullptr;
  int (*characterJump)(void *,u64)=nullptr;
  int (*cameraLook)(void *,u64,const float *)=nullptr;
  // ABI v9: animação por componente (objeto, instância do componente).
  int (*animationCommand)(void *,u64,u64,const ScriptAnimationCommand *)=nullptr;
  int (*getAnimationState)(void *,u64,u64,ScriptAssetGuid,ScriptAnimationState *)=nullptr;
  int (*setAnimationState)(void *,u64,u64,const ScriptAnimationState *)=nullptr;
  // Devolve quantos clipes o componente lista (ou -1); com `index` válido
  // escreve a identidade e o nome (UTF-8, truncado em `capacity`).
  int (*animationClipAt)(void *,u64,u64,u32,ScriptAssetGuid *,u8 *,int)=nullptr;
  // ABI v10: recursos de coleção endereçados pela identidade do elemento.
  int (*resourceElementId)(void *,u64,u64,const u8 *,int,u32,u64 *)=nullptr;
  int (*getResourceByElementId)(void *,u64,u64,const u8 *,int,u64,ScriptAssetGuid *)=nullptr;
  int (*setResourceByElementId)(void *,u64,u64,const u8 *,int,u64,ScriptAssetGuid)=nullptr;
  // ABI v11: edições da lista de clipes no mundo Play; o ID novo só é escrito
  // quando o recurso foi resolvido e a alteração entrou no componente.
  int (*appendAnimationClip)(void *,u64,u64,ScriptAssetGuid,u64 *)=nullptr;
  int (*removeAnimationClip)(void *,u64,u64,u64)=nullptr;
  int (*moveAnimationClip)(void *,u64,u64,u64,u32)=nullptr;
  // ABI v12: 0 mantém transform local, 1 preserva a pose mundial.
  int (*setParentWithPolicy)(void *,u64,u64,u32,u32)=nullptr;
  // ABI v13: kind 0 destrói, 1 troca pai, 2 remove componente. O retorno
  // confirma apenas entrada na fila; queryOperation devolve o resultado final.
  int (*queueStructuralOperation)(void *,u32,u64,u64,u32,u32,u64 *)=nullptr;
  int (*queryOperation)(void *,u32,u64,u32 *,u32 *)=nullptr;
  // ABI v14: estado local; getActive continua incluindo os ancestrais.
  int (*getActiveSelf)(void *,u64)=nullptr;
  // ABI v15: tags do catálogo do projeto; busca exclui raiz sintética e inativos.
  int (*getTag)(void *,u64,u8 *,int)=nullptr;
  int (*setTag)(void *,u64,const u8 *,int)=nullptr;
  int (*compareTag)(void *,u64,const u8 *,int)=nullptr;
  int (*findTagged)(void *,const u8 *,int,u64 *,int,int)=nullptr;
  // ABI v16: criação de script validado pelo catálogo C# e destruição no relógio do mundo.
  u64 (*addBehavior)(void *,u64,const u8 *,int,const u8 *,int)=nullptr;
  int (*destroyAfter)(void *,u64,double)=nullptr;
  // ABI v17: pré-ordem de pares origem/destino; capacity em pares, consulta sem criar.
  int (*instantiate)(void *,u64,u64,u64 *,int)=nullptr;
  int (*finishInstantiation)(void *,u64,int)=nullptr;
  // ABI v18: primitive kind follows scene::PrimitiveType; zero on failure.
  u64 (*createPrimitive)(void *,u64,u32)=nullptr;
  // v19: creation happens once; querying attachments never creates objects.
  u64 (*instantiatePrefab)(void *,u64,ScriptAssetGuid)=nullptr;
  int (*instantiationAttachments)(void *,u64,u8 *,int)=nullptr;
  // v20: tripla refletida por id; os três canais são aplicados atomicamente.
  int (*setTriple)(void *,u64,u64,const u8 *,int,const float *)=nullptr;
  // v21: independent XY solver. command 0 get velocity, 1 set velocity,
  // 2 force, 3 impulse, 4 torque, 5 angular impulse, 6 move kinematic.
  // vectors have 3 floats: XY + angular degrees (torque uses scalar[0]).
  int (*body2DCommand)(void *,u64,u32,u32,u32,const float *,float *)=nullptr;
  // query 0 ray (translation XY), 1 circle overlap. count may exceed capacity;
  // negative is failure. Hits use XY, Z=0; overlap has no normal.
  int (*query2D)(void *,u32,u32,const float *,const float *,float,const ScriptQueryFilter *,ScriptQueryHit *,int)=nullptr;
  // v22: point 0 count, 1 read by index, 2 read by identity, 3 insert before
  // index (count appends), 4 edit by identity, 5 remove, 6 move to index.
  // Packed values contain position XYZ and relative in/out handles XYZ.
  // v35: 7/8 read full point by index/ID, 9/10 insert/edit full point;
  // ten floats append roll degrees. Legacy edit 4 preserves existing roll.
  int (*pathPointCommand)(void *,u64,u32,u32,u64,u32,u64,u32,const float *,float *,u64 *)=nullptr;
  // Runtime 0 sample world curve (output XYZ+tangentXYZ, scalar=length),
  // 1 restart follower, 2 stop follower, 3 progress distance, 4 playing (0/1).
  // v35: 5 samples frame XYZ+tangentXYZ+upXYZ+rollDegrees, scalar=length.
  int (*pathRuntimeCommand)(void *,u64,u32,u32,u64,u32,double,u32,float *,double *)=nullptr;
  // v23: readonly observed voice, not the requested playback property.
  int (*audioSnapshot)(void *,u64,u32,u32,u64,ScriptAudioSnapshot *)=nullptr;
  // v24: session clock. Read/set validate world identity; frameScale remains
  // frozen for the current frame even when requested timeScale changes.
  int (*timeSnapshot)(void *,u32,ScriptTimeState *)=nullptr;
  int (*setTimeScale)(void *,u32,float)=nullptr;
  // v25: named membership, world-scoped queries and bounded enumeration.
  // membership operation: -1 query, 0 remove, 1 add; query returns 0/1, -1 on
  // failure. groupAt UINT_MAX with no buffer returns the membership count.
  int (*groupMembership)(void *,u64,const u8 *,int,int)=nullptr;
  int (*findGroup)(void *,const u8 *,int,u64 *,int,int)=nullptr;
  int (*groupAt)(void *,u64,u32,u8 *,int)=nullptr;
  // Binding command: effective=0, override=1, restore=2, restore all=3, authored=4.
  int (*inputBindingCommand)(void *,const u8 *,int,u32,u32,ScriptInputBinding *)=nullptr;
  // Profile: export=0 (size probe supported), import=1. Failure=-1.
  int (*inputProfile)(void *,u32,const u8 *,int,u8 *,int)=nullptr;
  int (*inputCaptureCommand)(void *,u32,const u8 *,int,u32,u32,u32,u32)=nullptr;
  // v28: timer instance query/start/stop/pause/resume (0..4). Positive start
  // seconds updates only the runtime interval; countdown never serializes.
  int (*timerCommand)(void *,u64,u64,u32,float,ScriptTimerState *)=nullptr;
  // v29: per-instance transform tween query/restart/cancel/pause/resume (0..4).
  int (*tweenCommand)(void *,u64,u64,u32,ScriptTweenState *)=nullptr;
  // v30: runtime numeric tracks over explicitly eligible scalar PropertyIds.
  int (*numberTweenCreate)(void *,u64,u64,const u8 *,int,const ScriptNumberTweenParameters *,u64 *)=nullptr;
  int (*numberTweenCommand)(void *,u64,u32,ScriptNumberTweenState *)=nullptr;
  int (*characterSnapshot)(void *,u64,ScriptCharacterState *)=nullptr;
  int (*bodyCommand)(void *,u64,u32,u32,u64,u32,const float *,const float *,ScriptBodyState *)=nullptr;
  int (*fieldQuery)(void *,u64,u32,u32,u64,u32,const float *,u32,ScriptFieldState *)=nullptr;
  // -1 reads, 0..31 writes. Return current layer, -1 on explicit status failure.
  int (*objectLayer)(void *,u64,u32,u32,int)=nullptr;
  // v36: action state / runtime enabled override / restore / global group read/write.
  int (*inputActionCommand)(void *,const u8 *,int,u32,ScriptInputActionState *)=nullptr;
  // v37: voice transport: Play=0, Pause=1, Stop=2, Seek=3, Resume=4.
  int (*audioCommand)(void *,u64,u32,u32,u64,u32,double)=nullptr;
  // v38: Find=0, Read=1, Text=2, Value=3, Visible=4, Enabled=5, Poll=6.
  // Handles are (world,node); a previous Play session is always rejected.
  int (*guiCommand)(void *,u32,u32,u32,const u8 *,int,float,ScriptGuiState *)=nullptr;
  int (*guiProperties)(void *,u32,u32,u32,ScriptGuiProperties *)=nullptr; // 0 read, 1 write
  int (*guiText)(void *,u32,u32,u32,u8 *,int)=nullptr; // 0 text, 1 name. Returns UTF8 length, -1 on error.
  int (*guiSizing)(void *,u32,u32,u32,ScriptGuiSizing *)=nullptr;
  int (*guiCanvas)(void *,u32,u32,ScriptGuiCanvas *)=nullptr;
  int (*guiBehavior)(void *,u32,u32,u32,ScriptGuiBehavior *)=nullptr;
  // op 0 count, 1 read, 2 append, 3 replace, 4 remove, 5 move to value.target.
  int (*guiAction)(void *,u32,u32,u32,u32,ScriptGuiAction *)=nullptr;
  int (*guiTransitions)(void *,u32,u32,u32,ScriptGuiTransitions *)=nullptr;
  // v42: lease identity is distinct from owner/component/node identity.
  u64 (*guiInstance)(void *,u32,u64,u64)=nullptr;
  // Tagged existing typed payloads, with an exact byte-size contract.
  int (*guiInstanceRequest)(void *,u32,u64,u32,u32,u32,u32,void *,u32,u8 *,int,float)=nullptr;
  // v45: ÚLTIMO campo do núcleo. Funções novas entram como famílias nomeadas
  // (scene/script_extensions.h): devolve a tabela da família e escreve versão e
  // tamanho dela, ou nulo quando este host não a oferece.
  const void *(*extension)(void *,const u8 *name,int nameLength,u32 *version,u32 *size)=nullptr;
  bool available() const {
    return version==45&&size>=sizeof(ScriptSceneAccess)&&exists&&getTransform&&setTransform&&setVelocity&&moveKinematic&&log&&bodyForce&&getVelocity&&
           worldId&&generation&&lastStatus&&parentOf&&childCount&&childAt&&findChild&&getName&&setName&&
           getActive&&setActive&&createObject&&destroyObject&&setParent&&componentCount&&componentAt&&
           findComponent&&addComponent&&removeComponent&&getProperty&&setProperty&&
           getWorldTransform&&setWorldTransform&&rayCast&&shapeCast&&overlap&&layerByName&&layerName&&
           inputAxis&&inputButton&&inputContext&&inputRole&&getRenderingState&&setRenderingSettings&&
           copyRenderingDiagnostics&&getComponentResource&&setComponentResource&&
           getComponentSlotProperty&&setComponentSlotProperty&&characterMove&&characterJump&&cameraLook&&
           animationCommand&&getAnimationState&&setAnimationState&&animationClipAt&&
           resourceElementId&&getResourceByElementId&&setResourceByElementId&&
            appendAnimationClip&&removeAnimationClip&&moveAnimationClip&&setParentWithPolicy&&
            queueStructuralOperation&&queryOperation&&getActiveSelf&&getTag&&setTag&&compareTag&&findTagged&&addBehavior&&destroyAfter&&instantiate&&finishInstantiation&&createPrimitive&&instantiatePrefab&&instantiationAttachments&&setTriple&&body2DCommand&&query2D&&pathPointCommand&&pathRuntimeCommand&&audioSnapshot&&timeSnapshot&&setTimeScale&&groupMembership&&findGroup&&groupAt&&inputBindingCommand&&inputProfile&&inputCaptureCommand&&timerCommand&&tweenCommand&&numberTweenCreate&&numberTweenCommand&&characterSnapshot&&bodyCommand&&fieldQuery&&objectLayer&&inputActionCommand&&audioCommand&&guiCommand&&guiProperties&&guiText&&guiSizing&&guiCanvas&&guiBehavior&&guiAction&&guiTransitions&&guiInstance&&guiInstanceRequest&&extension;
  }
};
static_assert(offsetof(ScriptSceneAccess,characterJump)==offsetof(ScriptSceneAccess,characterMove)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,cameraLook)==offsetof(ScriptSceneAccess,characterJump)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,animationCommand)==offsetof(ScriptSceneAccess,cameraLook)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,appendAnimationClip)==offsetof(ScriptSceneAccess,setResourceByElementId)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,getActiveSelf)==offsetof(ScriptSceneAccess,queryOperation)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,getTag)==offsetof(ScriptSceneAccess,getActiveSelf)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,addBehavior)==offsetof(ScriptSceneAccess,findTagged)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,setTriple)==offsetof(ScriptSceneAccess,instantiationAttachments)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,body2DCommand)==offsetof(ScriptSceneAccess,setTriple)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,pathPointCommand)==offsetof(ScriptSceneAccess,query2D)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,audioSnapshot)==offsetof(ScriptSceneAccess,pathRuntimeCommand)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,timeSnapshot)==offsetof(ScriptSceneAccess,audioSnapshot)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,setTimeScale)==offsetof(ScriptSceneAccess,timeSnapshot)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,groupMembership)==offsetof(ScriptSceneAccess,setTimeScale)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,groupAt)==offsetof(ScriptSceneAccess,findGroup)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,inputBindingCommand)==offsetof(ScriptSceneAccess,groupAt)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,inputProfile)==offsetof(ScriptSceneAccess,inputBindingCommand)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,inputCaptureCommand)==offsetof(ScriptSceneAccess,inputProfile)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,timerCommand)==offsetof(ScriptSceneAccess,inputCaptureCommand)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,tweenCommand)==offsetof(ScriptSceneAccess,timerCommand)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,numberTweenCreate)==offsetof(ScriptSceneAccess,tweenCommand)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,numberTweenCommand)==offsetof(ScriptSceneAccess,numberTweenCreate)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,characterSnapshot)==offsetof(ScriptSceneAccess,numberTweenCommand)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,bodyCommand)==offsetof(ScriptSceneAccess,characterSnapshot)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,fieldQuery)==offsetof(ScriptSceneAccess,bodyCommand)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,objectLayer)==offsetof(ScriptSceneAccess,fieldQuery)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,inputActionCommand)==offsetof(ScriptSceneAccess,objectLayer)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,audioCommand)==offsetof(ScriptSceneAccess,inputActionCommand)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,guiCommand)==offsetof(ScriptSceneAccess,audioCommand)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,guiProperties)==offsetof(ScriptSceneAccess,guiCommand)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,guiText)==offsetof(ScriptSceneAccess,guiProperties)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,guiSizing)==offsetof(ScriptSceneAccess,guiText)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,guiCanvas)==offsetof(ScriptSceneAccess,guiSizing)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,guiBehavior)==offsetof(ScriptSceneAccess,guiCanvas)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,guiAction)==offsetof(ScriptSceneAccess,guiBehavior)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,guiTransitions)==offsetof(ScriptSceneAccess,guiAction)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,guiInstance)==offsetof(ScriptSceneAccess,guiTransitions)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,guiInstanceRequest)==offsetof(ScriptSceneAccess,guiInstance)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,extension)==offsetof(ScriptSceneAccess,guiInstanceRequest)+sizeof(void*));
static_assert(sizeof(ScriptSceneAccess)==offsetof(ScriptSceneAccess,extension)+sizeof(void*));
static_assert(sizeof(ScriptAnimationCommand)==40 && sizeof(ScriptAnimationState)==48);
// Espelhados em managed/Astra.Scripting/Graphics.cs; a ponte exige o tamanho
// exato. Mudar aqui exige mudar lá e o teste gerenciado que confere os dois.
static_assert(sizeof(ScriptRenderingSettings)==224 && sizeof(ScriptResolvedRenderingPolicy)==272 &&
              sizeof(ScriptRenderingCapabilities)==44 && sizeof(ScriptTextureStreamingStats)==88 &&
              sizeof(ScriptSceneStatistics)==56 && sizeof(ScriptRenderingState)==728);
struct ScriptRuntimeApi {
  int (*start)(const u8 *,int,const u8 *,int,const ScriptSceneAccess *)=nullptr;
  int (*update)(float)=nullptr;
  int (*fixedUpdate)(float)=nullptr;
  void (*stop)()=nullptr;
  int (*copyDiagnostics)(u8 *,int)=nullptr;
  int (*trigger)(u64,u64,u32)=nullptr;
  // Contato SÓLIDO, entregue aos dois objetos do par. `normal` é nulo quando o
  // backend não sabe informá-la (o fim de um contato não traz geometria).
  int (*contact)(u64,u64,u32,const float *)=nullptr;
  // Um evento agregado por timer e quadro; count preserva disparos perdidos
  // quando um quadro demora mais que o intervalo configurado.
  int (*timer)(u64,u64,u32)=nullptr;
  // Depois de animação e física, antes do acompanhamento de câmera.
  int (*lateUpdate)(float)=nullptr;
  // Evento do aplicativo (ScriptLifecycleEvent) com valor 0/1.
  int (*lifecycle)(u32,u32)=nullptr;
  // Inspector com o Play rodando: objeto, instância e JSON UTF-8
  // {"Enabled":bool,"Properties":{id:valor}} no mesmo formato dos anexos do
  // Start. Opcional: um runtime sem ela recusa a edição ao vivo pelo nome, e o
  // resto do Play segue igual.
  int (*edit)(u64,u64,const u8 *,int)=nullptr;
  // Inspection only. Null destination captures once; second call copies that
  // snapshot without invoking user getters again. Negative means unavailable.
  int (*inspectFields)(u64,u8 *,int)=nullptr;
  int (*characterSnapshot)(void *,u64,ScriptCharacterState *)=nullptr;
  bool available() const {
    return start&&update&&fixedUpdate&&stop&&copyDiagnostics&&trigger&&contact&&timer&&lateUpdate&&lifecycle;
  }
};
// Eventos do aplicativo entregues aos comportamentos (Behavior.ApplicationPause
// e ApplicationFocus). Os valores atravessam a fronteira: não renumerar.
enum class ScriptLifecycleEvent : u32 { ApplicationPause=0, ApplicationFocus=1 };
}

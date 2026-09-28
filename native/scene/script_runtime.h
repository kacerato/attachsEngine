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
};
struct ScriptShapeQuery {
  u32 kind=1; // 0 caixa, 1 esfera, 2 cápsula
  float halfExtent[3]{.5f,.5f,.5f};
  float radius=.5f;
  float halfHeight=.5f;
  float rotation[4]{0,0,0,1};
};

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

struct ScriptSceneAccess {
  u32 version=14,size=sizeof(ScriptSceneAccess);
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
  bool available() const {
    return exists&&getTransform&&setTransform&&setVelocity&&moveKinematic&&log&&bodyForce&&getVelocity&&
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
            queueStructuralOperation&&queryOperation&&getActiveSelf;
  }
};
static_assert(offsetof(ScriptSceneAccess,characterJump)==offsetof(ScriptSceneAccess,characterMove)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,cameraLook)==offsetof(ScriptSceneAccess,characterJump)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,animationCommand)==offsetof(ScriptSceneAccess,cameraLook)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,appendAnimationClip)==offsetof(ScriptSceneAccess,setResourceByElementId)+sizeof(void*));
static_assert(offsetof(ScriptSceneAccess,getActiveSelf)==offsetof(ScriptSceneAccess,queryOperation)+sizeof(void*));
static_assert(sizeof(ScriptSceneAccess)==offsetof(ScriptSceneAccess,getActiveSelf)+sizeof(void*));
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
  bool available() const {
    return start&&update&&fixedUpdate&&stop&&copyDiagnostics&&trigger&&contact&&timer&&lateUpdate&&lifecycle;
  }
};
// Eventos do aplicativo entregues aos comportamentos (Behavior.ApplicationPause
// e ApplicationFocus). Os valores atravessam a fronteira: não renumerar.
enum class ScriptLifecycleEvent : u32 { ApplicationPause=0, ApplicationFocus=1 };
}

#include "harness.h"
#include "runtime/runtime_rendering_state.h"

using namespace ae;

AE_TEST(runtime_graphics_waits_for_real_effective_ack_and_restores_authoring) {
  renderer::ProjectRenderingSettings authored{};
  authored.resolutionScale=.85f;
  renderer::RenderingCapabilities capabilities{};
  const auto initial=renderer::resolveRenderingPolicy(authored,capabilities,renderer::ThermalPressure::None);
  struct Request {u64 id=0;renderer::ProjectRenderingSettings settings{};} last;
  runtime::RuntimeRenderingState state;
  state.configure(authored,capabilities,renderer::ThermalPressure::None,
    [&](u64 id,const renderer::ProjectRenderingSettings &settings,bool){last={id,settings};return true;});
  AE_EXPECT_TRUE(state.begin(17,initial),"sessão começa");
  auto changed=authored;changed.resolutionScale=.75f;
  changed.upscalingFilter=renderer::UpscalingFilter::Fsr1;u64 request=0;
  AE_EXPECT_TRUE(state.request(17,changed,request)&&request==last.id,"pedido chega ao consumidor");
  AE_EXPECT_TRUE(state.pending()&&state.requested().resolutionScale==.75f,"intenção fica visível");
  AE_EXPECT_EQ(state.effective().resolutionScale,initial.resolutionScale,"efetivo não antecipa GPU");
  const auto applied=renderer::resolveRenderingPolicy(changed,capabilities,renderer::ThermalPressure::None);
  AE_EXPECT_TRUE(state.complete(request,true,applied),"frame boundary confirma");
  AE_EXPECT_EQ(state.effective().resolutionScale,.75f,"efetivo confirmado");
  AE_EXPECT_EQ(state.effective().post.upscalingFilter,renderer::UpscalingFilter::Fsr1,"FSR confirmado pelo consumidor");
  state.end();
  AE_EXPECT_EQ(last.settings.resolutionScale,.85f,"Stop pede restauração autoral");
}

AE_TEST(runtime_graphics_rejects_stale_world_invalid_values_and_parallel_request) {
  renderer::ProjectRenderingSettings authored{};renderer::RenderingCapabilities capabilities{};
  runtime::RuntimeRenderingState state;u32 calls=0;
  state.configure(authored,capabilities,renderer::ThermalPressure::None,
    [&](u64,const renderer::ProjectRenderingSettings &,bool){++calls;return true;});
  AE_EXPECT_TRUE(state.begin(31,renderer::resolveRenderingPolicy(authored,capabilities,renderer::ThermalPressure::None)),"sessão");
  u64 id=0;auto invalid=authored;invalid.resolutionScale=NAN;
  AE_EXPECT_TRUE(!state.request(31,invalid,id)&&calls==0,"NaN recusado antes do consumidor");
  invalid=authored;invalid.preset=static_cast<renderer::QualityPreset>(999);
  AE_EXPECT_TRUE(!state.request(31,invalid,id)&&calls==0,"enum inválido recusado antes do consumidor");
  auto valid=authored;valid.resolutionScale=.8f;
  AE_EXPECT_TRUE(!state.request(30,valid,id),"sessão velha recusada");
  AE_EXPECT_TRUE(state.request(31,valid,id),"pedido aceito");
  u64 second=0;AE_EXPECT_TRUE(!state.request(31,authored,second),"um rebuild por vez");
  AE_EXPECT_TRUE(state.complete(id,false,state.effective()),"falha reconhecida");
  AE_EXPECT_EQ(state.requested().resolutionScale,.8f,"pedido falho preservado para diagnóstico");
}

AE_TEST(runtime_graphics_destructive_failure_does_not_report_the_destroyed_policy) {
  renderer::ProjectRenderingSettings authored{};renderer::RenderingCapabilities capabilities{};
  runtime::RuntimeRenderingState state;u32 restores=0;
  state.configure(authored,capabilities,renderer::ThermalPressure::None,
    [&](u64,const renderer::ProjectRenderingSettings &,bool restoring){if(restoring)++restores;return true;});
  AE_EXPECT_TRUE(state.begin(37,renderer::resolveRenderingPolicy(
      authored,capabilities,renderer::ThermalPressure::None)),"sessão");
  auto requested=authored;requested.resolutionScale=.7f;u64 id=0;
  AE_EXPECT_TRUE(state.request(37,requested,id),"pedido aceito");
  AE_EXPECT_TRUE(state.complete(id,false,renderer::ResolvedRenderingPolicy{},false),"falha destrutiva reconhecida");
  AE_EXPECT_TRUE(!state.effectiveAvailable()&&!state.lastRequestSucceeded(),
                 "renderer destruído não permanece disponível como efetivo");
  state.end();
  AE_EXPECT_EQ(restores,1u,"Stop tenta reconstruir autoria mesmo após primeira aplicação falhar");
}

AE_TEST(runtime_graphics_stop_replaces_a_request_not_yet_consumed) {
  renderer::ProjectRenderingSettings authored{};authored.resolutionScale=.9f;
  renderer::RenderingCapabilities capabilities{};
  struct Queued {u64 id=0;renderer::ProjectRenderingSettings settings{};bool pending=false;} queue;
  runtime::RuntimeRenderingState state;
  state.configure(authored,capabilities,renderer::ThermalPressure::None,
    [&](u64 id,const renderer::ProjectRenderingSettings &settings,bool restoring) {
      if(queue.pending&&!restoring) return false;
      queue={id,settings,true};return true;
    });
  AE_EXPECT_TRUE(state.begin(41,renderer::resolveRenderingPolicy(
      authored,capabilities,renderer::ThermalPressure::None)),"sessão");
  auto gameplay=authored;gameplay.resolutionScale=.6f;u64 request=0;
  AE_EXPECT_TRUE(state.request(41,gameplay,request)&&queue.settings.resolutionScale==.6f,
                 "pedido de gameplay fica na fila");
  state.end();
  AE_EXPECT_TRUE(queue.pending&&queue.id!=request&&queue.settings.resolutionScale==.9f,
                 "Stop substitui a fila pela restauração autoral");
}

AE_TEST(runtime_graphics_refuses_unavailable_temporal_upscaler_with_reason) {
  renderer::ProjectRenderingSettings authored{};
  renderer::RenderingCapabilities capabilities{};
  capabilities.armAsr=renderer::TemporalUpscalerAvailability::MissingFloat16;
  capabilities.fsr2=renderer::TemporalUpscalerAvailability::Available;
  runtime::RuntimeRenderingState state;u32 calls=0;
  state.configure(authored,capabilities,renderer::ThermalPressure::None,
    [&](u64,const renderer::ProjectRenderingSettings &,bool){++calls;return true;});
  AE_EXPECT_TRUE(state.begin(5,renderer::resolveRenderingPolicy(authored,capabilities,renderer::ThermalPressure::None)),"sessão");
  auto asr=authored;asr.upscalingFilter=renderer::UpscalingFilter::ArmAsr;u64 id=0;
  AE_EXPECT_TRUE(!state.request(5,asr,id)&&calls==0,"ASR sem float16 recusado antes do renderer");
  auto fsr2=authored;fsr2.upscalingFilter=renderer::UpscalingFilter::Fsr2;
  fsr2.temporalUpscalerQuality=renderer::TemporalUpscalerQuality::Performance;
  AE_EXPECT_TRUE(state.request(5,fsr2,id)&&calls==1,"FSR 2 disponível chega ao consumidor");
  state.setExecution(renderer::UpscalingFilter::Bilinear,renderer::TemporalUpscalerAvailability::ContextCreationFailed);
  AE_EXPECT_EQ(state.executedStatus(),renderer::TemporalUpscalerAvailability::ContextCreationFailed,
               "falha real do contexto fica visível, separada da política");
}

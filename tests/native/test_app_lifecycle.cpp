#include "harness.h"
#include "platform/app_lifecycle.h"

using namespace ae::platform;
using namespace ae::test;

AE_TEST(lifecycle_so_ativa_com_janela_foco_e_resume) {
  AppLifecycle lifecycle;
  lifecycle.apply(AppEvent::Resume);
  lifecycle.apply(AppEvent::GainFocus);
  AE_EXPECT_TRUE(!lifecycle.isActive(), "sem janela o aplicativo nao pode ficar ativo");

  const LifecycleAction action = lifecycle.apply(AppEvent::WindowCreated);
  AE_EXPECT_TRUE(hasAction(action, LifecycleAction::CreateSurface), "a primeira janela deve criar a surface");
  AE_EXPECT_TRUE(hasAction(action, LifecycleAction::BecameActive), "janela + foco + resume devem ativar o aplicativo");
  AE_EXPECT_TRUE(lifecycle.isActive(), "o estado final deveria estar ativo");
}

AE_TEST(lifecycle_perda_de_janela_para_e_destroi_surface_uma_vez) {
  AppLifecycle lifecycle;
  lifecycle.apply(AppEvent::Resume);
  lifecycle.apply(AppEvent::GainFocus);
  lifecycle.apply(AppEvent::WindowCreated);

  const LifecycleAction first = lifecycle.apply(AppEvent::WindowDestroyed);
  AE_EXPECT_TRUE(hasAction(first, LifecycleAction::DestroySurface), "perder a janela deve destruir a surface");
  AE_EXPECT_TRUE(hasAction(first, LifecycleAction::BecameInactive), "perder a janela deve suspender o aplicativo");
  AE_EXPECT_TRUE(!lifecycle.hasWindow(), "a janela perdida nao pode continuar marcada como valida");

  const LifecycleAction duplicate = lifecycle.apply(AppEvent::WindowDestroyed);
  AE_EXPECT_TRUE(!hasAction(duplicate, LifecycleAction::DestroySurface), "evento duplicado nao pode destruir a surface duas vezes");
}

AE_TEST(lifecycle_pause_e_retomada_preservam_a_surface) {
  AppLifecycle lifecycle;
  lifecycle.apply(AppEvent::WindowCreated);
  lifecycle.apply(AppEvent::GainFocus);
  lifecycle.apply(AppEvent::Resume);

  const LifecycleAction pause = lifecycle.apply(AppEvent::Pause);
  AE_EXPECT_TRUE(hasAction(pause, LifecycleAction::BecameInactive), "pause deve suspender o aplicativo");
  AE_EXPECT_TRUE(!hasAction(pause, LifecycleAction::DestroySurface), "pause sozinho nao perde a janela");
  AE_EXPECT_TRUE(lifecycle.hasWindow(), "a janela deve sobreviver ao pause enquanto o Android a preservar");

  const LifecycleAction resume = lifecycle.apply(AppEvent::Resume);
  AE_EXPECT_TRUE(hasAction(resume, LifecycleAction::BecameActive), "resume deve reativar quando foco e janela continuam validos");
}

AE_TEST(lifecycle_destroy_limpa_recursos_e_ignora_eventos_tardios) {
  AppLifecycle lifecycle;
  lifecycle.apply(AppEvent::WindowCreated);
  lifecycle.apply(AppEvent::Resume);
  lifecycle.apply(AppEvent::GainFocus);

  const LifecycleAction destroy = lifecycle.apply(AppEvent::Destroy);
  AE_EXPECT_TRUE(hasAction(destroy, LifecycleAction::DestroySurface), "destroy deve liberar a surface existente");
  AE_EXPECT_TRUE(hasAction(destroy, LifecycleAction::BecameInactive), "destroy deve encerrar o estado ativo");
  AE_EXPECT_TRUE(hasAction(destroy, LifecycleAction::Quit), "destroy deve encerrar o loop nativo");
  AE_EXPECT_TRUE(lifecycle.isDestroyed(), "o estado final deve ser destruido");

  const LifecycleAction late = lifecycle.apply(AppEvent::WindowCreated);
  AE_EXPECT_TRUE(!hasAction(late, LifecycleAction::CreateSurface), "eventos tardios nao podem recriar recursos");
  AE_EXPECT_TRUE(!lifecycle.hasWindow(), "um aplicativo destruido nao pode readquirir janela");
}

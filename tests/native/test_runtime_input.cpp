#include "harness.h"
#include "editor/editor_archive.h"
#include "editor/editor_document.h"
#include "runtime/input_actions.h"
#include "platform/android/android_game_input.h"

using namespace ae;
using namespace ae::editor;
using ae::runtime::ActionKind;
using ae::runtime::InputAction;
using ae::runtime::InputActionMap;
using ae::runtime::InputBinding;
using ae::runtime::InputDeviceState;
using ae::runtime::InputService;
using ae::runtime::InputSource;

AE_TEST(android_game_input_feeds_actions_and_releases_devices) {
  ae::platform::android::AndroidGameInputState raw;
  InputActionMap map;
  InputAction key;key.id="Tecla";key.kind=ActionKind::Button;
  key.bindings={{InputSource::Key,69,0,0,1,false}};
  InputAction button;button.id="Controle";button.kind=ActionKind::Button;
  button.bindings={{InputSource::GamepadButton,96,0,0,1,false}};
  InputAction axis;axis.id="Direção";axis.kind=ActionKind::Axis2D;
  axis.bindings={{InputSource::GamepadAxis,0,0,0,1,false},
                 {InputSource::GamepadAxis,1,0,1,1,false}};
  AE_EXPECT_TRUE(map.add(key)&&map.add(button)&&map.add(axis),"ações de entrada válidas");
  InputService service;service.setMap(map);
  raw.key(7,false,69,true);raw.key(8,true,96,true);
  raw.axes(8,{.5f,.75f,0,0,0,0,0,0});
  service.submit(raw.snapshot());
  float move[2]{};service.axis2("Direção",move);
  AE_EXPECT_TRUE(service.justPressed("Tecla")&&service.justPressed("Controle")&&
                 move[0]>.4f&&move[1]>.7f,"teclado e controle chegam ao mesmo mapa");
  raw.key(7,false,69,true);service.submit(raw.snapshot());
  AE_EXPECT_TRUE(!service.justPressed("Tecla"),"repetição não gera novo pulso");
  raw.disconnect(8);service.submit(raw.snapshot());
  service.axis2("Direção",move);
  AE_EXPECT_TRUE(!service.pressed("Controle")&&move[0]==0&&move[1]==0,
                 "desconexão limpa botão e eixos do controle");
  raw.clear();service.reset();service.submit(raw.snapshot());
  AE_EXPECT_TRUE(!service.pressed("Tecla")&&!service.justReleased("Tecla"),
                 "perda de foco não deixa tecla presa nem soltura fantasma");
}

AE_TEST(input_default_map_reproduces_the_existing_touch_controls) {
  InputActionMap map;
  AE_EXPECT_TRUE(map.valid(), "mapa padrão válido");
  AE_EXPECT_TRUE(map.find("Mover") && map.find("Olhar") && map.find("Saltar"), "ações padrão");
  AE_EXPECT_EQ(map.moveAction(), std::string("Mover"), "papel de movimento");
  AE_EXPECT_EQ(map.lookAction(), std::string("Olhar"), "papel de olhar");
  AE_EXPECT_EQ(map.jumpAction(), std::string("Saltar"), "papel de salto");

  InputService service;
  service.setMap(map);
  InputDeviceState device;
  device.moveX = .8f;
  device.moveY = -.5f;
  device.lookX = .2f;
  device.lookY = .1f;
  device.touchButtons = 1;
  service.submit(device);

  float move[2]{}, look[2]{};
  service.axis2("Mover", move);
  service.axis2("Olhar", look);
  AE_EXPECT_TRUE(move[0] > .7f && move[1] < -.4f, "manche vira o eixo de movimento");
  AE_EXPECT_TRUE(look[0] > .19f && look[1] > .09f, "arraste vira o eixo de olhar sem zona morta");
  AE_EXPECT_TRUE(service.pressed("Saltar") && service.justPressed("Saltar"), "botão desceu neste quadro");

  service.submit(device);
  AE_EXPECT_TRUE(service.pressed("Saltar") && !service.justPressed("Saltar"), "segurar não repete o pulso");
  device.touchButtons = 0;
  service.submit(device);
  AE_EXPECT_TRUE(!service.pressed("Saltar") && service.justReleased("Saltar"), "soltura detectada uma vez");
}

AE_TEST(input_rejects_bindings_that_runtime_cannot_evaluate) {
  InputActionMap map;
  InputAction invalid;invalid.id="Fora da máscara";invalid.kind=ActionKind::Button;
  invalid.bindings={{InputSource::TouchButton,32,0,0,1,false}};
  AE_EXPECT_TRUE(!map.add(invalid),"bit de toque 32 não pode deslocar uma máscara de 32 bits");
  invalid.bindings={{InputSource::Key,42,0,1,1,false}};
  AE_EXPECT_TRUE(!map.add(invalid),"botão não consome eixo 1");
}

AE_TEST(input_actions_are_project_data_not_core_names) {
  InputActionMap map;
  // O projeto renomeia a própria ação: o papel acompanha, senão renomear
  // desconectaria o personagem do controle sem nenhum aviso.
  AE_EXPECT_TRUE(map.rename("Mover", "Caminhar"), "renomear ação");
  AE_EXPECT_EQ(map.moveAction(), std::string("Caminhar"), "papel segue o nome novo");
  AE_EXPECT_TRUE(!map.find("Mover"), "nome antigo não existe mais");
  AE_EXPECT_TRUE(!map.rename("Caminhar", "Olhar"), "nome duplicado é recusado");

  InputAction interact;
  interact.id = "Interagir";
  interact.kind = ActionKind::Button;
  interact.bindings = {{InputSource::TouchButton, 1, 0, 0, 1, false}};
  AE_EXPECT_TRUE(map.add(interact), "ação nova do projeto");
  AE_EXPECT_TRUE(!map.add(interact), "id repetido é recusado");
  AE_EXPECT_TRUE(map.setJumpAction("Interagir"), "papel aponta para outra ação");
  AE_EXPECT_TRUE(!map.setJumpAction("Inexistente"), "papel não aceita ação que não existe");

  // Remover a ação que cumpre o papel limpa o papel em vez de deixá-lo
  // apontando para o vazio.
  AE_EXPECT_TRUE(map.remove("Interagir"), "remover ação");
  AE_EXPECT_TRUE(map.jumpAction().empty(), "papel limpo junto");
  AE_EXPECT_TRUE(map.valid(), "mapa continua consistente");

  InputService service;
  service.setMap(map);
  InputDeviceState device;
  device.moveX = 1;
  service.submit(device);
  AE_EXPECT_TRUE(service.axis("Caminhar") > .9f, "a ação renomeada responde");
  AE_EXPECT_EQ(service.axis("Mover"), 0.f, "o nome antigo não responde a nada");
}

AE_TEST(input_deadzone_sensitivity_and_inversion_are_configurable) {
  InputActionMap map;
  InputAction axis;
  axis.id = "Passo";
  axis.kind = ActionKind::Axis1D;
  axis.deadzone = .5f;
  axis.sensitivity = 2;
  axis.bindings = {{InputSource::TouchMove, 0, 0, 0, 1, true}};
  AE_EXPECT_TRUE(map.add(axis), "ação configurada");

  InputService service;
  service.setMap(map);
  InputDeviceState device;
  device.moveX = .4f;
  service.submit(device);
  AE_EXPECT_EQ(service.axis("Passo"), 0.f, "abaixo da zona morta lê zero");

  device.moveX = 1;
  service.submit(device);
  // Invertido e com sensibilidade 2: 1 -> -1 -> zona morta reescalona para -1 -> x2.
  AE_EXPECT_TRUE(service.axis("Passo") < -1.9f, "inversão e sensibilidade aplicadas");
}

AE_TEST(input_focus_and_context_never_leave_a_button_stuck) {
  InputActionMap map;
  InputAction menu;
  menu.id = "Menu";
  menu.kind = ActionKind::Button;
  menu.context = "jogo";
  menu.bindings = {{InputSource::TouchButton, 0, 0, 0, 1, false}};
  AE_EXPECT_TRUE(map.add(menu), "ação com contexto");

  InputService service;
  service.setMap(map);
  InputDeviceState device;
  device.moveX = 1;
  device.touchButtons = 1;
  service.submit(device);
  AE_EXPECT_TRUE(service.pressed("Menu") && service.axis("Mover") > .9f, "com foco tudo responde");

  // A interface tomou o toque: o gameplay lê zero e o botão é SOLTO na hora.
  service.setGameplayFocus(false);
  AE_EXPECT_TRUE(!service.pressed("Menu"), "botão solto ao perder o foco");
  AE_EXPECT_TRUE(service.justReleased("Menu"), "a soltura é observável, não silenciosa");
  service.submit(device);
  AE_EXPECT_TRUE(!service.pressed("Menu") && service.axis("Mover") == 0.f,
                 "sem foco o estado do dispositivo não vaza para o gameplay");

  service.setGameplayFocus(true);
  service.submit(device);
  AE_EXPECT_TRUE(service.pressed("Menu"), "retomar o foco devolve o controle");

  // Contexto desligado: a ação some sem que o usuário perca a configuração.
  service.setContextEnabled("jogo", false);
  service.submit(device);
  AE_EXPECT_TRUE(!service.pressed("Menu"), "contexto desligado zera a ação");
  AE_EXPECT_TRUE(service.axis("Mover") > .9f, "as outras ações continuam");
  service.setContextEnabled("jogo", true);
  service.submit(device);
  AE_EXPECT_TRUE(service.pressed("Menu"), "contexto religado");
  AE_EXPECT_TRUE(map.find("Menu")->context == "jogo", "a configuração nunca foi apagada");
}

AE_TEST(input_map_roundtrips_in_the_scene_archive) {
  EditorDocument doc;
  auto map = doc.inputActions();
  InputAction interact;
  interact.id = "Interagir";
  interact.kind = ActionKind::Button;
  interact.context = "exploração";
  interact.bindings = {{InputSource::TouchButton, 1, 0, 0, 1, false},
                       {InputSource::Key, 69, 0, 0, 1, false}};
  AE_EXPECT_TRUE(map.add(interact), "ação do projeto");
  AE_EXPECT_TRUE(map.rename("Saltar", "Pular") && map.setJumpAction("Pular"), "renomeada");
  AE_EXPECT_TRUE(doc.setInputActions(map), "cena aceita o mapa");

  const auto text = serializeEditorDocument(doc, 0);
  EditorDocument loaded;
  AE_EXPECT_TRUE(deserializeEditorDocument(text, 0, loaded), "ida e volta");
  AE_EXPECT_TRUE(loaded.inputActions() == map, "ações, bindings e papéis preservados");
  AE_EXPECT_EQ(serializeEditorDocument(loaded, 0), text, "representação estável");

  // Uma cena sem a seção (arquivo anterior a v12) abre com o mapa padrão em vez
  // de ficar sem controle nenhum.
  EditorDocument fresh;
  AE_EXPECT_TRUE(fresh.inputActions().isDefault(), "mapa padrão para cena nova");
}

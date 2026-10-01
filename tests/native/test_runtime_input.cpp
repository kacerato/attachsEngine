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
using ae::runtime::InputCaptureStatus;
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

AE_TEST(input_mouse_producer_accumulates_transients_latches_clicks_and_releases_owners) {
  ae::platform::android::AndroidGameInputState raw;InputActionMap map;
  InputAction click;click.id="Clique";click.kind=ActionKind::Button;click.deadzone=0;click.bindings={{InputSource::MouseButton,0}};
  InputAction motion;motion.id="Cursor";motion.kind=ActionKind::Axis2D;motion.deadzone=0;
  motion.bindings={{InputSource::MouseAxis,0,0,0},{InputSource::MouseAxis,1,0,1}};
  InputAction wheel;wheel.id="Roda";wheel.kind=ActionKind::Axis1D;wheel.deadzone=0;wheel.bindings={{InputSource::MouseAxis,3}};
  AE_EXPECT_TRUE(map.add(click)&&map.add(motion)&&map.add(wheel),"real mouse actions");InputService input;input.setMap(map);
  raw.mouse(3,10,10,100,100,0);raw.mouse(3,20,5,100,100,1);raw.mouse(3,30,0,100,100,0,0,1);
  input.submit(raw.snapshot());float axes[2]{};input.axis2("Cursor",axes);
  AE_EXPECT_TRUE(std::abs(axes[0]-.2f)<.0001f && std::abs(axes[1]+.1f)<.0001f,"motion accumulates in viewport fractions with screen Y");
  AE_EXPECT_TRUE(input.justPressed("Clique") && input.pressed("Clique") && input.axis("Roda")==1,"quick click retained until snapshot; wheel independent");
  raw.finishFrame();input.submit(raw.snapshot());input.axis2("Cursor",axes);
  AE_EXPECT_TRUE(input.justReleased("Clique") && !input.pressed("Clique") && axes[0]==0 && axes[1]==0 && input.axis("Roda")==0,"transients reset each frame; release follows pulse");
  raw.mouse(3,30,0,100,100,1);raw.finishFrame();input.submit(raw.snapshot());
  AE_EXPECT_TRUE(input.pressed("Clique"),"held button survives frame boundary");
  raw.key(7,false,69,true);raw.disconnect(3);input.submit(raw.snapshot());
  AE_EXPECT_TRUE(!input.pressed("Clique") && raw.snapshot().keys.size()==1,"mouse disconnect preserves other device");
  raw.mouse(4,900,800,100,100,0);input.submit(raw.snapshot());input.axis2("Cursor",axes);
  AE_EXPECT_TRUE(axes[0]==0 && axes[1]==0,"new owner establishes position without jump");
  input.setGameplayFocus(false);raw.mouse(4,950,850,100,100,1);input.submit(raw.snapshot());
  AE_EXPECT_TRUE(!input.pressed("Clique") && input.axis("Cursor")==0,"focus consumes mouse actions");
  AE_EXPECT_TRUE(ae::platform::android::AndroidGameInputState::mousePointerId(0)!=0,"mouse and touch pointer IDs cannot collide");
}

AE_TEST(input_mouse_scene16_persists_channels_and_legacy_read_is_transactional) {
  EditorDocument doc;auto map=doc.inputActions();InputAction action;action.id="Mouse";action.kind=ActionKind::Axis1D;
  action.bindings={{InputSource::MouseAxis,3,0,0,.5f,true}};
  AE_EXPECT_TRUE(map.add(action)&&doc.setInputActions(map),"authored scroll action");
  EditorDocument loaded;const auto saved=serializeEditorDocument(doc,0);
  AE_EXPECT_TRUE(saved.starts_with("AETHER_EDITOR 17 ") && deserializeEditorDocument(saved,0,loaded),"current archive preserves scene16 mouse sources");
  AE_EXPECT_TRUE(loaded.inputActions()==map,"source/code/output/scale/inversion preserved");
  std::ostringstream storage;map.write(storage);InputActionMap previous;const auto baseline=previous;
  std::istringstream legacy(storage.str());
  AE_EXPECT_TRUE(!previous.read(legacy,static_cast<u32>(InputSource::GamepadButton)) && previous==baseline,"legacy contract refuses new sources without replacing prior state");
  auto invalid=action;invalid.bindings[0].code=4;
  AE_EXPECT_TRUE(!invalid.valid(),"four mouse axis channels bounded");
  invalid.bindings={{InputSource::MouseButton,5}};AE_EXPECT_TRUE(!invalid.valid(),"five mouse buttons bounded");
}

AE_TEST(input_profiles_are_transactional_runtime_overrides_and_reject_changed_authoring) {
  InputActionMap authored;InputService input;input.setMap(authored);InputBinding previous;
  AE_EXPECT_TRUE(input.binding("Saltar",0,previous,true),"authored binding available");
  const InputBinding replacement{InputSource::MouseButton,1};
  AE_EXPECT_TRUE(input.overrideBinding("Saltar",0,replacement),"apply player override");
  InputDeviceState device;device.mouseButtons=2;input.submit(device);
  AE_EXPECT_TRUE(input.justPressed("Saltar")&&input.authoredMap()==authored,"override consumed without authoring mutation");
  const auto profile=input.exportProfile();input.removeAllOverrides();input.submit(device);
  AE_EXPECT_TRUE(!input.pressed("Saltar")&&input.map()==authored,"restore releases held override");
  AE_EXPECT_TRUE(input.importProfile(profile),"reload compatible saved profile");input.submit(device);
  AE_EXPECT_TRUE(input.justPressed("Saltar"),"restored override reaches real consumer");
  const auto effective=input.map();
  AE_EXPECT_TRUE(!input.importProfile(profile+"garbage")&&input.map()==effective,"trailing corruption rejected atomically");
  auto changed=authored;AE_EXPECT_TRUE(changed.rename("Saltar","Pular"),"project evolves");input.setMap(changed);
  AE_EXPECT_TRUE(!input.importProfile(profile)&&input.map()==changed,"profile never rebinds wrong action after authoring change");
  input.setMap(authored);input.setContextEnabled("menu",false);input.setGameplayFocus(false);
  AE_EXPECT_TRUE(input.importProfile(profile)&&!input.gameplayFocus()&&!input.contextEnabled("menu"),"load preserves focus and context policy");
  InputBinding invalid=replacement;invalid.code=99;
  AE_EXPECT_TRUE(!input.overrideBinding("Saltar",0,invalid),"invalid button rejected");
  AE_EXPECT_TRUE(input.removeOverride("Saltar",0)&&input.map()==authored,"restore one binding");
}

AE_TEST(input_runtime_capture_waits_for_new_press_blocks_actions_and_restores_profile) {
  InputService input;InputDeviceState device;InputBinding binding;InputActionMap authored;
  InputAction cancelAction;cancelAction.id="Voltar";cancelAction.kind=ActionKind::Button;cancelAction.bindings={{InputSource::Key,111}};
  AE_EXPECT_TRUE(authored.add(cancelAction),"cancel key also has gameplay meaning");input.setMap(authored);
  AE_EXPECT_TRUE(input.beginBindingCapture("Saltar",0,InputSource::Key),"runtime capture begins");
  device.keys={62};input.submit(device);AE_EXPECT_TRUE(input.captureStatus()==InputCaptureStatus::Waiting&&!input.pressed("Saltar"),"initial held key not captured");
  device.keys.clear();input.submit(device);device.keys={62};input.submit(device);
  AE_EXPECT_TRUE(input.captureStatus()==InputCaptureStatus::Completed&&input.binding("Saltar",0,binding)&&binding.code==62,"new key captured into effective binding");
  input.submit(device);AE_EXPECT_TRUE(!input.pressed("Saltar"),"accepted held key cannot leak into gameplay");
  device.keys.clear();input.submit(device);device.keys={62};input.submit(device);
  AE_EXPECT_TRUE(input.justPressed("Saltar"),"fresh gameplay press reaches rebound action");
  const auto profile=input.exportProfile();input.removeAllOverrides();AE_EXPECT_TRUE(input.importProfile(profile),"captured binding persists as profile");
  AE_EXPECT_TRUE(input.beginBindingCapture("Saltar",0,InputSource::MouseButton),"second source capture");device={};input.submit(device);device.keys={111};input.submit(device);
  AE_EXPECT_TRUE(input.captureStatus()==InputCaptureStatus::Cancelled&&input.binding("Saltar",0,binding)&&binding.source==InputSource::Key,"Escape cancels without altering binding");
  input.submit(device);AE_EXPECT_TRUE(!input.pressed("Voltar"),"held cancel key cannot leak to gameplay");
  device.keys.clear();input.submit(device);device.keys={111};input.submit(device);AE_EXPECT_TRUE(input.justPressed("Voltar"),"a later fresh cancel key resumes normal gameplay meaning");
  AE_EXPECT_TRUE(input.beginBindingCapture("Saltar",0,InputSource::MouseButton),"mouse capture");device={};input.submit(device);device.mouseButtons=4;input.submit(device);
  AE_EXPECT_TRUE(input.captureStatus()==InputCaptureStatus::Completed&&input.binding("Saltar",0,binding)&&binding.code==2,"named middle button captured");
  AE_EXPECT_TRUE(input.beginBindingCapture("Saltar",0,InputSource::Key),"focus test");input.setGameplayFocus(false);
  AE_EXPECT_TRUE(input.captureStatus()==InputCaptureStatus::Cancelled,"focus loss cancels pending capture");
}
AE_TEST(input_runtime_capture_axis_neutral_direction_and_negative_keys_have_real_constraints) {
  InputService input;InputDeviceState device;InputBinding binding;
  AE_EXPECT_TRUE(!input.beginBindingCapture("Saltar",0,InputSource::Key,true),"button cannot have negative key capture");
  AE_EXPECT_TRUE(input.beginBindingCapture("Olhar",0,InputSource::GamepadAxis),"axis capture");device.gamepadAxes[2]=-.9f;input.submit(device);input.submit(device);
  AE_EXPECT_TRUE(input.captureStatus()==InputCaptureStatus::Waiting,"preheld axis rejected");device.gamepadAxes[2]=0;input.submit(device);device.gamepadAxes[2]=-.9f;input.submit(device);
  AE_EXPECT_TRUE(input.captureStatus()==InputCaptureStatus::Completed&&input.binding("Olhar",0,binding)&&binding.source==InputSource::GamepadAxis&&binding.code==2&&binding.invert,"neutral gate and captured direction");
  input.cancelBindingCapture();AE_EXPECT_TRUE(input.overrideBinding("Mover",0,{InputSource::Key,32,0,0}),"positive key authored as player override");
  AE_EXPECT_TRUE(input.beginBindingCapture("Mover",0,InputSource::Key,true),"capture negative member");device={};input.submit(device);device.keys={32};input.submit(device);
  AE_EXPECT_TRUE(input.captureStatus()==InputCaptureStatus::Waiting,"negative cannot equal positive");device.keys={29};input.submit(device);
  AE_EXPECT_TRUE(input.captureStatus()==InputCaptureStatus::Completed&&input.binding("Mover",0,binding)&&binding.code==32&&binding.negativeCode==29,"distinct negative key committed");
  device.keys.clear();input.submit(device);device.keys={29};input.submit(device);AE_EXPECT_TRUE(input.axis("Mover")<-.9f,"captured negative drives real movement action");
}

AE_TEST(input_runtime_capture_observes_android_quick_keys_and_gamepad_clicks_between_frames) {
  ae::platform::android::AndroidGameInputState raw;InputService input;
  AE_EXPECT_TRUE(input.beginBindingCapture("Saltar",0,InputSource::Key),"begin runtime capture");input.submit(raw.snapshot());
  raw.key(1,false,62,true);raw.key(1,false,62,false);input.submit(raw.snapshot());
  AE_EXPECT_TRUE(input.captureStatus()==InputCaptureStatus::Completed,"quick press retained for capture");raw.finishFrame();input.submit(raw.snapshot());
  raw.key(1,false,62,true);raw.key(1,false,62,false);input.submit(raw.snapshot());
  AE_EXPECT_TRUE(input.justPressed("Saltar"),"quick gameplay press retained for one frame");raw.finishFrame();input.submit(raw.snapshot());
  AE_EXPECT_TRUE(input.justReleased("Saltar")&&!input.pressed("Saltar"),"quick release follows without stuck key");
  AE_EXPECT_TRUE(input.beginBindingCapture("Saltar",0,InputSource::GamepadButton),"gamepad capture");input.submit(raw.snapshot());
  raw.key(2,true,96,true);raw.key(2,true,96,false);input.submit(raw.snapshot());InputBinding binding;
  AE_EXPECT_TRUE(input.captureStatus()==InputCaptureStatus::Completed&&input.binding("Saltar",0,binding)&&binding.code==96,"gamepad quick click reaches rebind");
  raw.disconnect(2);AE_EXPECT_TRUE(raw.snapshot().gamepadButtons.empty(),"disconnect also clears latched clicks");
}

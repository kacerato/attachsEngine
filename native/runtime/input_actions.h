// As ações de entrada do projeto.
//
// O núcleo não conhece "Mover", "Olhar" nem "Saltar": ele conhece AÇÕES, e quem
// decide quais existem, o que as aciona e quais papéis elas cumprem é o projeto.
// Um jogo que chame sua ação de andar de "Caminhar" funciona igual, e um que
// não tenha salto simplesmente não declara a ação.
//
// Duas metades:
//   • `InputActionMap` é o RECURSO — dado autorado, serializado com a cena.
//   • `InputService` é o SERVIÇO — recebe o estado bruto do dispositivo a cada
//     quadro e publica o valor de cada ação.
//
// Foco é parte do contrato, não um detalhe: quando a interface consome o toque
// (abrir a IDE, digitar, arrastar um gizmo), o gameplay recebe zero e os botões
// são SOLTOS. Sem isso, abrir um painel com o dedo sobre o manche deixaria o
// personagem andando sozinho — que é exatamente o defeito que esta regra evita.
#pragma once
#include "core/base.h"

#include <array>
#include <iomanip>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace ae::runtime {

enum class ActionKind : u32 { Button = 0, Axis1D = 1, Axis2D = 2 };
enum class InputInteraction : u32 { Press, Hold, Tap };
enum class InputPhase : u32 { Waiting, Started, Performed, Canceled, Disabled };
enum InputDeviceGroup : u32 { InputTouch=1, InputKeyboardMouse=2, InputGamepad=4, InputAllDevices=7 };

// De onde o valor bruto vem. O projeto escolhe pela fonte, não por um código de
// dispositivo: `TouchMove` é "o manche virtual", em qualquer aparelho.
enum class InputSource : u32 {
  None = 0,
  TouchMove = 1,     // manche virtual; eixo 0 = direita, 1 = frente
  TouchLook = 2,     // arraste de olhar; eixo 0 = horizontal, 1 = vertical
  TouchButton = 3,   // botão da interface de jogo (salto, interagir)
  Key = 4,           // tecla única; `code`/`negativeCode` formam um eixo
  GamepadAxis = 5,
  GamepadButton = 6,
  MouseButton = 7,   // 0 primary, 1 secondary, 2 middle, 3 back, 4 forward
  MouseAxis = 8,     // 0 dx/viewport, 1 dy down/viewport, 2 horizontal scroll, 3 vertical scroll
};

struct InputBinding {
  InputSource source = InputSource::None;
  // Código do dispositivo quando a fonte precisa de um: tecla/botão positivo.
  u32 code = 0;
  // Tecla/botão que produz -1 num eixo montado com duas teclas.
  u32 negativeCode = 0;
  // Eixo da fonte quando ela tem mais de um (0 = X, 1 = Y).
  u32 axis = 0;
  float scale = 1;
  bool invert = false;

  bool valid() const;
};

struct InputAction {
  std::string id;
  ActionKind kind = ActionKind::Button;
  // Abaixo da zona morta o eixo lê zero. Deriva do manche analógico, mas vale
  // para qualquer fonte contínua.
  float deadzone = .12f;
  float sensitivity = 1;
  // Vazio significa "sempre ativa". Um contexto desabilitado zera a ação sem
  // apagar o que o usuário configurou.
  std::string context;
  std::vector<InputBinding> bindings;
  bool enabled = true;
  InputInteraction interaction = InputInteraction::Press;
  float duration = .5f; // Unscaled sampled input time; Hold minimum / Tap maximum.
  u32 deviceGroups = InputAllDevices;

  bool valid() const;
};

// Declarados ANTES do mapa: o `operator==` dele compara vetores destes tipos no
// próprio corpo da classe, e o libc++ não encontra uma sobrecarga declarada
// depois (o libstdc++ encontrava — a divergência só apareceu no build Android).
bool operator==(const InputBinding &a, const InputBinding &b);
bool operator==(const InputAction &a, const InputAction &b);

class InputActionMap final {
public:
  static constexpr u32 kMaximumActions = 64;
  static constexpr u32 kMaximumBindings = 16;

  // O mapa padrão reproduz o controle de toque que o editor já oferecia, com
  // nomes em português que o usuário pode trocar. Os papéis abaixo apontam para
  // ele; trocar o nome da ação e o papel junto continua funcionando.
  InputActionMap();

  const std::vector<InputAction> &actions() const noexcept { return actions_; }
  const InputAction *find(std::string_view id) const;
  bool add(const InputAction &action);
  bool remove(std::string_view id);
  bool rename(std::string_view id, std::string_view renamed);
  // Changes bindings/kind/settings in place without moving the action or
  // changing its identity and the roles that reference it.
  bool replace(std::string_view id,const InputAction &candidate);

  // Papéis: qual AÇÃO move o personagem, gira a câmera e salta. São dados
  // editáveis — o núcleo não exige que se chamem Move/Look/Jump.
  const std::string &moveAction() const noexcept { return move_; }
  const std::string &lookAction() const noexcept { return look_; }
  const std::string &jumpAction() const noexcept { return jump_; }
  bool setMoveAction(std::string_view id);
  bool setLookAction(std::string_view id);
  bool setJumpAction(std::string_view id);

  bool valid() const;
  bool isDefault() const;
  void write(std::ostream &out) const;
  bool read(std::istream &in,u32 maximumSource=static_cast<u32>(InputSource::MouseAxis),bool allowVersioned=true);
  friend bool operator==(const InputActionMap &a, const InputActionMap &b) {
    return a.actions_ == b.actions_ && a.move_ == b.move_ && a.look_ == b.look_ && a.jump_ == b.jump_;
  }

private:
  bool assignRole(std::string &role, std::string_view id);
  std::vector<InputAction> actions_;
  std::string move_, look_, jump_;
};


// Estado bruto de um quadro, publicado pela plataforma.
struct VirtualActionState final {
  u64 instance=0;u32 node=0;std::string action;ActionKind kind=ActionKind::Axis2D;
  float x=0,y=0;u32 pressCount=0;
};
struct InputDeviceState {
  // Complete snapshot of identified authored sources. Missing sources release
  // their contribution without releasing another source of the same action.
  std::vector<VirtualActionState> virtualActions;
  // Manche virtual, já normalizado em [-1,1].
  float moveX = 0, moveY = 0;
  // Arraste de olhar do quadro, em frações de tela.
  float lookX = 0, lookY = 0;
  // Botões da interface de jogo, por índice (0 = salto, 1 = interagir, ...).
  u32 touchButtons = 0;
  // Teclas Android (key code), botões de gamepad (key code) e eixos lógicos.
  // No Android os eixos 0..7 são esquerda X/frente, direita X/cima,
  // gatilhos esquerdo/direito e D-pad X/cima, respectivamente.
  std::vector<u32> keys;
  std::vector<u32> gamepadButtons;
  std::array<float, 8> gamepadAxes{};
  u32 mouseButtons=0;
  std::array<float,4> mouseAxes{};
  // Device removal / pointer cancellation aborts interactions rather than
  // interpreting a synthetic release as a successful tap.
  u32 canceledDeviceGroups=0;
  // Source-specific ownership loss; keyboard and mouse share a filtering group
  // but losing one device must not abort an unrelated binding of the other.
  u32 canceledSources=0; // Bit indexed by InputSource.
};

enum class InputCaptureStatus : u32 {Idle,Waiting,Completed,Cancelled};

class InputService final {
public:
  void setMap(const InputActionMap &map);
  const InputActionMap &map() const noexcept { return map_; }
  const InputActionMap &authoredMap() const noexcept {return authoredMap_;}
  bool binding(std::string_view action,u32 index,InputBinding &out,bool authored=false) const;
  bool overrideBinding(std::string_view action,u32 index,const InputBinding &binding);
  bool removeOverride(std::string_view action,u32 index);
  void removeAllOverrides();
  std::string exportProfile() const;
  bool importProfile(std::string_view profile);
  bool beginBindingCapture(std::string_view action,u32 index,InputSource source,bool negative=false,u32 cancelKey=111);
  void cancelBindingCapture() noexcept;
  InputCaptureStatus captureStatus() const noexcept {return captureStatus_;}

  // Contexto habilitado/desabilitado. Um contexto desconhecido conta como
  // habilitado: nunca silencia uma ação por engano de digitação.
  void setContextEnabled(std::string_view context, bool enabled);
  bool contextEnabled(std::string_view context) const;

  // Quando falso, todas as ações leem zero e nenhum botão fica preso. É o que a
  // sessão liga ao rotear o toque para a interface.
  void setGameplayFocus(bool focused);
  bool gameplayFocus() const noexcept { return focus_; }

  void submit(const InputDeviceState &state,double unscaledElapsed=0);
  void copyPolicyFrom(const InputService &other) {
    if(!(map_==other.map_))setMap(other.map_);
    enabledOverrides_=other.enabledOverrides_;disabledContexts_=other.disabledContexts_;
    deviceGroups_=other.deviceGroups_;setGameplayFocus(other.gameplayFocus()&&other.captureStatus()!=InputCaptureStatus::Waiting);
  }
  bool setActionEnabled(std::string_view action,bool enabled);
  bool restoreActionEnabled(std::string_view action);
  bool actionEnabled(std::string_view action) const;
  bool setDeviceGroups(u32 groups);
  u32 deviceGroups() const noexcept {return deviceGroups_;}
  InputPhase phase(std::string_view action) const;
  float progress(std::string_view action) const;
  float elapsed(std::string_view action) const;
  // Pausa, retomada e cancelamento de toque: solta tudo sem gerar "just
  // released" fantasma no quadro seguinte.
  void reset();

  float axis(std::string_view action) const;
  void axis2(std::string_view action, float out[2]) const;
  bool pressed(std::string_view action) const;
  bool justPressed(std::string_view action) const;
  bool justReleased(std::string_view action) const;

private:
  struct Value {
    float x = 0, y = 0;
    bool down = false, wasDown = false;
    bool physicalDown=false,performedPulse=false;
    double elapsed=0;
    InputPhase phase=InputPhase::Waiting;
  };
  const Value *value(std::string_view action) const;
  float evaluate(const InputAction &action, const InputDeviceState &state, u32 axis) const;
  float evaluateHardware(const InputAction &action,const InputDeviceState &state,u32 axis) const;

  InputActionMap map_,authoredMap_;
  std::vector<Value> values_;
  std::vector<int> enabledOverrides_;
  u32 deviceGroups_=InputAllDevices;
  std::vector<std::string> disabledContexts_;
  bool focus_ = true;
  InputCaptureStatus captureStatus_=InputCaptureStatus::Idle;
  std::string captureAction_;
  u32 captureIndex_=0,captureCancelKey_=111;
  InputSource captureSource_=InputSource::None;
  bool captureNegative_=false,captureSeed_=false,captureReleaseGate_=false;
  InputBinding captureAccepted_;
  std::array<bool,8> captureAxisNeutral_{};
  InputDeviceState capturePrevious_;
  void capture(const InputDeviceState &state);
};

} // namespace ae::runtime

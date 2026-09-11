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

  bool valid() const;
};

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
  bool read(std::istream &in);
  friend bool operator==(const InputActionMap &a, const InputActionMap &b) {
    return a.actions_ == b.actions_ && a.move_ == b.move_ && a.look_ == b.look_ && a.jump_ == b.jump_;
  }

private:
  bool assignRole(std::string &role, std::string_view id);
  std::vector<InputAction> actions_;
  std::string move_, look_, jump_;
};

bool operator==(const InputBinding &a, const InputBinding &b);
bool operator==(const InputAction &a, const InputAction &b);

// Estado bruto de um quadro, publicado pela plataforma.
struct InputDeviceState {
  // Manche virtual, já normalizado em [-1,1].
  float moveX = 0, moveY = 0;
  // Arraste de olhar do quadro, em frações de tela.
  float lookX = 0, lookY = 0;
  // Botões da interface de jogo, por índice (0 = salto, 1 = interagir, ...).
  u32 touchButtons = 0;
  // Teclas e botões de gamepad pressionados neste quadro. O serviço avalia
  // essas fontes; o shell Android ainda não alimenta nenhuma delas, e por isso
  // elas não são apresentadas como exercitadas no aparelho.
  std::vector<u32> keys;
  std::vector<u32> gamepadButtons;
  std::array<float, 8> gamepadAxes{};
};

class InputService final {
public:
  void setMap(const InputActionMap &map);
  const InputActionMap &map() const noexcept { return map_; }

  // Contexto habilitado/desabilitado. Um contexto desconhecido conta como
  // habilitado: nunca silencia uma ação por engano de digitação.
  void setContextEnabled(std::string_view context, bool enabled);
  bool contextEnabled(std::string_view context) const;

  // Quando falso, todas as ações leem zero e nenhum botão fica preso. É o que a
  // sessão liga ao rotear o toque para a interface.
  void setGameplayFocus(bool focused);
  bool gameplayFocus() const noexcept { return focus_; }

  void submit(const InputDeviceState &state);
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
  };
  const Value *value(std::string_view action) const;
  float evaluate(const InputAction &action, const InputDeviceState &state, u32 axis) const;

  InputActionMap map_;
  std::vector<Value> values_;
  std::vector<std::string> disabledContexts_;
  bool focus_ = true;
};

} // namespace ae::runtime

// G7: Entrada do jogador — o PlayerInput do Input System com Behavior =
// Invoke Unity Events. Mora num objeto e transforma as ações do mapa de entrada
// da cena (Configurações › Entrada) em eventos do componente: "ação executada"
// quando a ação dispara e "ação cancelada" quando é solta. A Conexão de evento
// escolhe a ação pelo nome e liga o evento a um objeto ou método, sem script;
// um script assina os mesmos eventos (OnPerformed/OnCanceled).
//
// Referência: Unity Input System 1.11, PlayerInput (Behavior "Invoke Unity
// Events") — https://docs.unity3d.com/Packages/com.unity.inputsystem@1.11/manual/PlayerInput.html
//
// O valor do evento é o índice da ação no mapa, a partir de 1. As opções abaixo
// são endereços fixos; o Inspector mostra o nome da ação do projeto no lugar do
// rótulo, como as camadas de objeto (editor_screen.cpp).
#pragma once
#include "scene/components.h"

#include <array>
#include <cmath>

namespace ae::scene {

inline constexpr u32 kPlayerInputActionCount=32;
// Máscara das ações observadas: bit N = ação N+1 do mapa.
inline constexpr std::array<ComponentEnumOption,kPlayerInputActionCount> inputActionOptions{{{1u<<0,"Ação 1"},{1u<<1,"Ação 2"},{1u<<2,"Ação 3"},{1u<<3,"Ação 4"},{1u<<4,"Ação 5"},{1u<<5,"Ação 6"},{1u<<6,"Ação 7"},{1u<<7,"Ação 8"},{1u<<8,"Ação 9"},{1u<<9,"Ação 10"},{1u<<10,"Ação 11"},{1u<<11,"Ação 12"},{1u<<12,"Ação 13"},{1u<<13,"Ação 14"},{1u<<14,"Ação 15"},{1u<<15,"Ação 16"},{1u<<16,"Ação 17"},{1u<<17,"Ação 18"},{1u<<18,"Ação 19"},{1u<<19,"Ação 20"},{1u<<20,"Ação 21"},{1u<<21,"Ação 22"},{1u<<22,"Ação 23"},{1u<<23,"Ação 24"},{1u<<24,"Ação 25"},{1u<<25,"Ação 26"},{1u<<26,"Ação 27"},{1u<<27,"Ação 28"},{1u<<28,"Ação 29"},{1u<<29,"Ação 30"},{1u<<30,"Ação 31"},{1u<<31,"Ação 32"}}};
// Filtro da Conexão de evento: 0 aceita qualquer ação.
inline constexpr std::array<ComponentEnumOption,kPlayerInputActionCount+1> inputActionFilterOptions{{{0,"Qualquer ação"},{1,"Ação 1"},{2,"Ação 2"},{3,"Ação 3"},{4,"Ação 4"},{5,"Ação 5"},{6,"Ação 6"},{7,"Ação 7"},{8,"Ação 8"},{9,"Ação 9"},{10,"Ação 10"},{11,"Ação 11"},{12,"Ação 12"},{13,"Ação 13"},{14,"Ação 14"},{15,"Ação 15"},{16,"Ação 16"},{17,"Ação 17"},{18,"Ação 18"},{19,"Ação 19"},{20,"Ação 20"},{21,"Ação 21"},{22,"Ação 22"},{23,"Ação 23"},{24,"Ação 24"},{25,"Ação 25"},{26,"Ação 26"},{27,"Ação 27"},{28,"Ação 28"},{29,"Ação 29"},{30,"Ação 30"},{31,"Ação 31"},{32,"Ação 32"}}};

class PlayerInput final : public ComponentValue {
public:
  bool enabled=true;
  u32 actions=~0u;
  static const ComponentType descriptor;
  const ComponentType &type() const override {return descriptor;}
  std::unique_ptr<ComponentValue> clone() const override {return std::make_unique<PlayerInput>(*this);}
  bool valid() const override {return true;}
  void write(std::ostream &out) const override {out<<enabled<<' '<<actions;}
  bool read(std::istream &in,u32 version) override {return version==1&&static_cast<bool>(in>>enabled>>actions);}
};
inline const PlayerInput &playerInput(const ComponentValue &v) {return static_cast<const PlayerInput &>(v);}
inline PlayerInput &playerInput(ComponentValue &v) {return static_cast<PlayerInput &>(v);}

inline constexpr std::array<ComponentBoolean,1> playerInputBooleans{{
  {"enabled","Ativa",[](const ComponentValue &v){return playerInput(v).enabled;},[](ComponentValue &v,bool b){playerInput(v).enabled=b;},
   {"Entrada","","Desligada não emite eventos; as ações continuam valendo para scripts e personagem"}},
}};
inline constexpr std::array<ComponentEnum,1> playerInputEnums{{
  {"actions","Ações",inputActionOptions,[](const ComponentValue &v){return playerInput(v).actions;},
   [](ComponentValue &v,u32 x){playerInput(v).actions=x;},
   {"Entrada","","Ações do mapa de entrada da cena que geram eventos neste objeto"},false,true},
}};
inline constexpr std::array<ComponentParameter,1> playerInputPayload{{{"action","Ação",ComponentValueKind::Integer}}};
inline constexpr std::array<ComponentEvent,2> playerInputEvents{{
  {"performed","Ação executada","A ação disparou neste quadro (performed do Input System); o valor é o índice da ação, a partir de 1",playerInputPayload},
  {"canceled","Ação cancelada","A ação foi solta neste quadro (canceled do Input System)",playerInputPayload},
}};
inline const ComponentType PlayerInput::descriptor{
  "astra.input.player_input",1,[]()->std::unique_ptr<ComponentValue>{return std::make_unique<PlayerInput>();},
  {},playerInputBooleans,playerInputEnums,nullptr,false,{},{},{},{},{},{},{},playerInputEvents
};

} // namespace ae::scene

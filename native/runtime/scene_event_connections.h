// Consumidor das Conexões de evento (scene/event_connection.h) no Play.
//
// Lê a fila de eventos de componente pelo cursor `Connections`, procura no
// objeto emissor as conexões ligadas àquele evento e executa a ação pelos
// mesmos caminhos dos scripts: ativação por GameWorld e método declarado por
// invokeComponentMethod. Nenhum ponteiro de objeto é retido entre eventos: o
// receptor é resolvido pelo handle atual a cada entrega.
#pragma once
#include "runtime/component_operations.h"
#include "runtime/object_activation_connection.h"
#include "scene/event_connection.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ae::runtime {

// Vazio quando todo evento declarado tem identidade persistente na conexão,
// todo método acionável também, e toda linha das tabelas aponta para um
// descritor existente com a assinatura que a conexão sabe montar.
std::vector<std::string> auditEventConnectionCatalog();

class SceneEventConnections final {
public:
  void reset() {diagnostic_.clear();deliveries_=0;fired_.clear();}
  const std::string &diagnostic() const noexcept {return diagnostic_;}
  u64 deliveries() const noexcept {return deliveries_;}
  // Processa os eventos pendentes; devolve falso só se a fila não estiver ligada.
  bool process(const ComponentOperationServices &services,ComponentEventQueue &queue);
private:
  void deliver(const ComponentOperationServices &services,const ComponentEventRecord &record);
  WorldStatus execute(const ComponentOperationServices &services,ObjectId source,const scene::EventConnection &connection);
  std::set<std::pair<ObjectId,u64>> fired_;
  std::string diagnostic_;
  u64 deliveries_=0;
};

} // namespace ae::runtime

#include "runtime/scene_event_connections.h"

#include "scene/component_schema.h"

namespace ae::runtime {

std::vector<std::string> auditEventConnectionCatalog() {
  std::vector<std::string> issues;
  for(usize i=0;i<scene::eventConnectionEventKeys.size();++i) {
    const auto &key=scene::eventConnectionEventKeys[i];
    const auto *schema=scene::findComponentSchema(key.type);
    const auto *event=schema?scene::findComponentEvent(*schema->type,key.event):nullptr;
    if(!event) issues.push_back("evento "+std::to_string(key.value)+" sem descritor");
    else {
      // O outro objeto, quando existe, é o primeiro valor do payload.
      const bool object=!event->payload.empty()&&event->payload[0].kind==scene::ComponentValueKind::Object;
      if(object!=scene::eventConnectionCarriesObject(key.value)) issues.push_back("evento "+std::to_string(key.value)+": filtro por objeto incoerente");
    }
    for(usize j=0;j<i;++j) if(scene::eventConnectionEventKeys[j].value==key.value) issues.push_back("evento "+std::to_string(key.value)+" repetido");
    bool labeled=false;for(const auto &option:scene::eventConnectionEvents) labeled=labeled||option.value==key.value;
    if(!labeled) issues.push_back("evento "+std::to_string(key.value)+" sem rótulo");
  }
  for(usize i=0;i<scene::eventConnectionMethodKeys.size();++i) {
    const auto &key=scene::eventConnectionMethodKeys[i];
    const auto *schema=scene::findComponentSchema(key.type);
    const auto *method=schema?scene::findComponentMethod(*schema->type,key.method):nullptr;
    if(!method) {issues.push_back("método "+std::to_string(key.value)+" sem descritor");continue;}
    const bool number=method->parameters.size()==1&&method->parameters[0].kind==scene::ComponentValueKind::Number;
    if(!(method->parameters.empty()||number)||number!=scene::eventConnectionTakesNumber(key.value)||method->result!=scene::ComponentValueKind::None)
      issues.push_back("método "+std::to_string(key.value)+": assinatura que a conexão não monta");
    for(usize j=0;j<i;++j) if(scene::eventConnectionMethodKeys[j].value==key.value) issues.push_back("método "+std::to_string(key.value)+" repetido");
    bool labeled=false;for(const auto &option:scene::eventConnectionMethods) labeled=labeled||option.value==key.value;
    if(!labeled) issues.push_back("método "+std::to_string(key.value)+" sem rótulo");
  }
  // Completude no sentido inverso: um evento ou comando novo num tipo precisa
  // ganhar identidade aqui, ou a conexão ficaria atrás do catálogo.
  for(const auto &schema:scene::componentSchemas) {
    for(const auto &event:schema.type->events)
      if(!scene::findEventConnectionEvent(schema.type->id,event.id))
        issues.push_back(std::string(schema.type->id)+"/"+std::string(event.id)+": evento sem identidade de conexão");
    for(const auto &method:schema.type->methods) {
      const bool command=method.result==scene::ComponentValueKind::None&&
        (method.parameters.empty()||(method.parameters.size()==1&&method.parameters[0].kind==scene::ComponentValueKind::Number));
      if(!command) continue;
      bool found=false;
      for(const auto &key:scene::eventConnectionMethodKeys) found=found||(key.type==schema.type->id&&key.method==method.id);
      if(!found) issues.push_back(std::string(schema.type->id)+"/"+std::string(method.id)+": método sem identidade de conexão");
    }
  }
  return issues;
}

bool SceneEventConnections::process(const ComponentOperationServices &services,ComponentEventQueue &queue) {
  if(!services.world || !queue.attached(ComponentEventQueue::Consumer::Connections)) return false;
  const auto lost=queue.consume(ComponentEventQueue::Consumer::Connections,[&](const ComponentEventRecord &record,u64) {
    deliver(services,record);
  });
  if(lost) diagnostic_="Conexões de evento: "+std::to_string(lost)+" eventos descartados por estouro da fila";
  return true;
}

void SceneEventConnections::deliver(const ComponentOperationServices &services,const ComponentEventRecord &record) {
  auto &world=*services.world;
  if(!record.type || record.event>=record.type->events.size()) return;
  const auto *key=scene::findEventConnectionEvent(record.type->id,record.type->events[record.event].id);
  if(!key) return;
  const auto source=record.object;
  if(world.validate(source)!=WorldStatus::Ok || !world.activeInHierarchy(source)) return;
  const u64 other=record.count&&record.values[0].valueKind()==scene::ComponentValueKind::Object?record.values[0].object:0;
  const auto count=world.componentCount(source);
  for(u32 i=0;i<count;++i) {
    // A ação anterior pode ter desativado o próprio emissor.
    if(!world.activeInHierarchy(source)) break;
    const auto *value=world.readComponent(world.componentAt(source,i));
    if(!value || &value->type()!=&scene::EventConnection::descriptor) continue;
    // Cópia: a ação pode alterar o objeto e invalidar o valor lido.
    const auto connection=scene::eventConnection(*value);
    if(!connection.enabled || connection.action==0 || connection.event!=key->value) continue;
    if(connection.clipTag>=0&&(connection.event==30||connection.event==32)&&
       (!record.count||record.values[0].valueKind()!=scene::ComponentValueKind::Integer||record.values[0].integer!=static_cast<i64>(connection.clipTag)))continue;
    if(connection.otherFilter && connection.otherFilter!=other) continue;
    if(connection.once && fired_.contains({source.id,connection.instanceId()})) continue;
    const auto status=execute(services,source.id,connection);
    if(status==WorldStatus::Ok) {
      ++deliveries_;diagnostic_.clear();
      if(connection.once) fired_.insert({source.id,connection.instanceId()});
    } else {
      diagnostic_="Conexão de evento "+std::to_string(connection.instanceId())+" no objeto "+std::to_string(source.id)+": "+
                  worldStatusMessage(status);
    }
  }
}

WorldStatus SceneEventConnections::execute(const ComponentOperationServices &services,ObjectId source,const scene::EventConnection &connection) {
  auto &world=*services.world;
  const auto receiver=connection.receiver?static_cast<ObjectId>(connection.receiver):source;
  if(connection.action<scene::kEventConnectionCallMethod)
    return applyObjectActivationConnection(world,receiver,connection.action);
  const auto *method=scene::findEventConnectionMethod(connection.method);
  if(!method) return WorldStatus::UnknownOperation;
  const auto target=world.handle(receiver);
  const auto valid=world.validate(target);
  if(valid!=WorldStatus::Ok) return valid;
  const auto count=world.componentCount(target);
  for(u32 i=0;i<count;++i) {
    const auto component=world.componentAt(target,i);
    const auto *value=world.readComponent(component);
    if(!value || value->type().id!=method->type) continue;
    scene::ComponentOperationValue result;
    if(scene::eventConnectionTakesNumber(connection.method)) {
      const auto argument=scene::ComponentOperationValue::makeNumber(connection.argument);
      return invokeComponentMethod(services,component,method->method,std::span(&argument,1),result);
    }
    return invokeComponentMethod(services,component,method->method,{},result);
  }
  return WorldStatus::ComponentMissing;
}

} // namespace ae::runtime

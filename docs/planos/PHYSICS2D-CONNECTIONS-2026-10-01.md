# Conexões autoráveis de física 2D — P06 / P12

Referências: [Godot 4.5 Area2D](https://docs.godotengine.org/en/4.5/classes/class_area2d.html), [código oficial Area2D 4.5](https://github.com/godotengine/godot/blob/4.5/scene/2d/physics/area_2d.cpp) e [workflow de signals 4.5](https://docs.godotengine.org/en/4.5/getting_started/step_by_step/signals.html). Princípio extraído: volume sensor autorado, evento definido, receptor explícito e condição opcional. Não implementa todas as propriedades de Area2D, como gravity override/audio bus.

## Dependências e execução

Body2D → Collider2D → Box2D3.1.1 → Physics2DEvent → PhysicsEventConnection2D → GameWorld.setActive → consumidores. Mundo XY existente, sem corpo Jolt travado em dois eixos. A ligação nativa executa antes do callback C# TriggerEnter/ContactEnter. Ativar/desativar/alternar usam o mesmo consumidor real de ativação do Timer e das conexões3D.

Novo tipo astra.physics2d.event_connection, versão1, repetível: enabled, evento entrada/permanência/saída de sensor ou contato, ação desconectada/ativar/desativar/alternar, receiver e other_filter. Eventos do Box2D têm identidade de forma; múltiplas formas podem produzir múltiplos disparos. Não se promete agregação por par de corpos; alternar em permanência ou múltiplas formas deve considerar essa frequência. O callback de sensor segue a direção first(sensor) → second(visitor) do backend; contato é distribuído aos dois corpos.

Body2D exige Collider2D pela composição existente. O solver recusa conexão ativa sem corpo explícito e sem uma forma do emissor compatível com sensor/contato. Um corpo com formas mistas pode receber os dois tipos de evento. Editar a configuração da conexão em Play invalida a física pelo contrato existente; o ponto seguro revalida antes de prosseguir. Isto também fecha a revalidação de conexões3D editadas em runtime. O custo do rebuild é em edição, não por evento disparado.

Não retém delegates/pointers. Fonte inativa não dispara; filtro resolve o outro objeto por identidade; receptor destruído/pendente produz diagnóstico sem acesso inválido. A saída de visitante destruído pode acionar a conexão nativa de um sensor sobrevivente; o callback C# mantém o contrato anterior de receber somente outro objeto vivo. Stop limpa estado e contador. Cena16/prefab3 usam o serializer de componentes e remapeamento de referências existentes; ABI27 permanece. Fachada C# gerada tem propriedades realmente consumidas.

## Autoria e aceite

NÃO IREI SER SIMPLISTA NO DESIGN.

Sensor 2D conectado cria corpo estático, colisor sensor e ação ativar, com o selecionado como receptor e a conexão aberta na Inspeção. Evento → ação → receptor fica no grupo Conexão; condição de outro objeto em Filtro. Picker/IME, Undo/Redo, arquivo, linha contextual no viewport e ícone vetorial próprio usam o pipeline real. A linha aparece só quando a conexão está em edição. Sem referência válida, o editor mantém um rascunho diagnosticável.

Aceite: criar pela receita → alterar evento e receptor → buscar com IME → Undo/Redo → salvar/reabrir/clonar receptor interno → Play → corpo entrar no sensor → receptor de luz ativar antes de callback C# → Stop restaurar autoria. Configuração incompatível inicial e editada em Play deve ser recusada. Fixture Physics2DConnection-20261001 publica PHYSICS2D CONNECTION READY/PASS.

[Resultados e capturas](../validacao/evidencias/physics2d-connections-20261001/README.md) serão registrados após execução. Pacote adiciona1 tipo/1 fachada/1 receita/1 ícone: totais34/33/56/228. P06, UI geral de gameplay e conexões arbitrárias permanecem parciais; nenhuma contagem representa paridade com todas as APIs de Unity/Godot.

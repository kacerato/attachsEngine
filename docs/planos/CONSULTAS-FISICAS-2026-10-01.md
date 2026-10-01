# F043 — consultas físicas contra o mundo real

## Escopo e cadeia existente

RayCast/RayCastAll, ShapeCast e Overlap usam ScenePhysics e o NarrowPhaseQuery do Jolt carregado pelo GameWorld. O hit resolve o corpo de backend para objeto e instância real do colisor; o SDK captura mundo e geração. As consultas não são componentes persistidos: consomem os corpos, colisores, transforms e layers autorados e carregados pela cena. O caminho é Behavior.Physics → NativeBehaviorRuntime → ScriptBridge → ScenePhysics → Jolt → IDs e contato → SDK.

Os filtros existentes cobrem máscara de gameplay, corpos estáticos/dinâmicos, sensores e objeto ignorado. Sensores ficam excluídos por padrão. O vetor de translação define direção e alcance em unidades de mundo. RayCastAll ordena por fração crescente. Buffers limitados devolvem o total real e o SDK expõe truncamento; o callback permite consulta de contagem com capacidade zero e buffer nulo. Formas suportadas: caixa, esfera, cápsula e cilindro; esta família não promete casts de malha ou movimento com rotação ao longo da varredura.

## Correções em execução

Antes desta revisão, pacotes com valores inválidos podiam parecer ausência de hits; callbacks após Stop podiam alcançar consumidores já desligados; quaternions inválidos chegavam ao Jolt. A correção valida dados antes de acessar o backend, recusa pacotes inválidos com -1 e mantém zero reservado para uma consulta válida sem acerto. O SDK recusa valores inválidos e capacidade fora de 1..4096, e propaga erro de backend por exceção explícita. Nenhuma rejeição publica saída parcial.

O contrato de sobreposição inicial será medido em cenário real antes do encerramento. Raio dentro de sólido convexo deve indicar fração/distância zero sem inventar normal de superfície. Overlap parado identifica o contato inicial e não oferece normal direcionada. A política de ShapeCast será explicitada junto ao consumidor e às evidências após validação, sem assumir que os defaults de outra engine descrevem o Jolt.

## Referências concretas

- [Godot 4.5 — PhysicsDirectSpaceState3D](https://docs.godotengine.org/en/4.5/classes/class_physicsdirectspacestate3d.html): consultas pertencem ao espaço físico; distingue varredura de sobreposição e registra que cast_motion ignora penetração inicial. A Astra precisa de seu próprio contrato medido, sem presumir igualdade de solver.
- [Godot 4.5 — PhysicsShapeQueryParameters3D](https://docs.godotengine.org/en/4.5/classes/class_physicsshapequeryparameters3d.html): forma, transform, máscara, exclusões e participação de áreas/corpos pertencem aos parâmetros reais da consulta. Astra adapta áreas para sensores e classes de movimento para seu filtro existente.
- [Fonte oficial Godot, tag 4.5-stable — physics_server_3d.cpp](https://github.com/godotengine/godot/blob/4.5-stable/servers/physics_server_3d.cpp): separação entre parâmetros autorais de consulta e estado do servidor físico.
- Fonte do Jolt integrado: native/third_party/JoltPhysics/Jolt/Physics/Collision/RayCast.h, ShapeCast.h e NarrowPhaseQuery.h. RayCastSettings trata convexos como sólidos; varredura é linear e sua forma tem ownership explícito durante a chamada. A documentação externa serve de referência de capacidade; o código integrado e a execução definem o comportamento da Astra.

## Aceite

Salvar/reabrir cena com corpos reais → iniciar mundo → raycast/shape cast/overlap → conferir objeto/colisor e alcance → filtros/sensor → buffer truncado → desabilitar/reabilitar colisor → consultas refletirem lifecycle → parar Play → callback antigo recusar acesso. Entradas inválidas e sobreposição inicial têm cenários dirigidos. Não há redesign de UI nem novo nome no menu para uma consulta de script; editor de colisores/layers e comportamento compilado continuam os caminhos de autoria.

Implementação e validação em andamento. Evidência de host, transporte SDK, APK e dispositivo será separada; esta rodada não usa instalação/execução física por instrução do usuário.

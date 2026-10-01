# Conexões autoráveis de física 3D — P06 / P07

Referências versionadas: [Unity 6.0 (6000.0), Collider.OnTriggerEnter](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Collider.OnTriggerEnter.html); [Godot 4.5, Area3D](https://docs.godotengine.org/en/4.5/classes/class_area3d.html); [implementação oficial Area3D 4.5](https://github.com/godotengine/godot/blob/4.5/scene/3d/physics/area_3d.cpp); [workflow de signals 4.5](https://docs.godotengine.org/en/4.5/getting_started/step_by_step/signals.html). A separação entre emissor, evento e receptor vem dessas referências. A Astra mantém identidade de componentes e mundo próprios, com ações tipadas em lugar de nomes de métodos arbitrários.

## Cadeia funcional

Corpo físico + colisor próprio → Jolt → eventos de sensor/contato do ScenePhysics → PhysicsEventConnection3D → ativação de objeto no GameWorld → consumidores de luz/render/física/scripts. A ação nativa precede o callback C# existente; não é um segundo solver nem depende de um script para executar.

Novo tipo `astra.physics.event_connection`, versão1, repetível. Propriedades: ativa; entrada/permanência/saída de sensor ou contato; desconectado/ativar/desativar/alternar; receptor por ObjectReference; filtro opcional do outro objeto. Campo vazio do filtro aceita todos. Permanência pode produzir uma ação em cada passo físico: alternar nesse evento deve ser uma escolha deliberada. Contatos são distribuídos para ambos os corpos; sensores seguem a direção sensor → outro do backend real.

O corpo e o colisor devem estar no emissor e o colisor deve pertencer a ele. O Play recusa evento de sensor em corpo sólido, contato em sensor e ownership incompatível. A conexão desabilitada ou desconectada permanece como autoria sem executar. O receptor ausente ou com destruição pendente é excluído pelo handle e produz diagnóstico; nenhum ponteiro de objeto ou closure permanece retido. Stop limpa estado e estatísticas. Ativar altera activeSelf; ancestrais inativos continuam bloqueando activeInHierarchy.

Cena16, prefab3 e clone usam o contrato existente de componentes/referências: remapeamento interno e preservação de alvos externos, sem novo formato de arquivo. IDs de componente são locais ao objeto; a identidade completa inclui o objeto. API C# é gerada do mesmo schema e atravessa as operações reais de propriedade do GameWorld. ABI27 permanece.

## Autoria

NÃO IREI SER SIMPLISTA NO DESIGN.

A receita Sensor conectado cria corpo estático sensor, colisor e conexão em um comando de histórico. O objeto selecionado vira receptor; sem seleção válida, a referência permanece para configurar. A criação abre a conexão diretamente na Inspeção. O grupo Conexão reúne evento, ação e receptor; Filtro separa a condição opcional. Busca de receptor, IME, Undo/Redo e persistência usam primitives reais existentes. Uma linha até a posição mundial do receptor aparece no viewport somente durante a edição dessa conexão. O ícone vetorial event/physics-connection está integrado ao atlas e à hierarquia.

As capturas do executável host foram inspecionadas: portrait permite editar os essenciais na mesma superfície; landscape usa paginação de propriedades. A janela focada existente fica estreita em portrait e não é apresentada como um redesign concluído. Imagens host não são prova no dispositivo. Pesquisa de vídeos de Area3D/signals foi feita; nenhum vídeo foi assistido e não há afirmação de observação de interações em vídeo.

## Aceite

Criar sensor conectado → escolher evento e receptor → editar filtro → Undo/Redo → salvar/reabrir → corpo dinâmico entrar → receptor de luz ativar antes de TriggerEnter C# → Stop restaurar autoria. Fixture independente PhysicsConnection-20261001 publica PHYSICS CONNECTION READY/PASS. [Evidências](../validacao/evidencias/physics-connections-20261001/README.md).

Um tipo e uma fachada novos: 33 schemas, 32 fachadas C#, 55 receitas e 227 ícones. Implementado: conexão tipada de física3D para ativação de objeto. Parcial: P06/eventos gerais. Pesquisado, sem suporte declarado: métodos arbitrários, argumentos/payload autoráveis, prioridade e sinais personalizados. Não inclui conexões de física2D ou disparo de áudio. Qualificação visual/física Android deve ser registrada separadamente.

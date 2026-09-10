# Refundação — composição física, juntas e referências

10/09/2026. Estado atualizado: **bloco compilado e exercitado no host e Android, com limites explícitos**. Ver [rodada autorizada e evidências](../validacao/2026-09-10-componentes-codigo.md). O histórico abaixo descreve a implementação antes da autorização.

## Pedido e referências

Continuação de “prepare uma mega adição pq depois vou autorizar adb”. Preserva a orientação de implementar blocos amplos e testar somente com autorização. Foram feitos leitura de fontes/planos, edição, geração de um ícone e empacotamento do atlas visual. Não houve build da engine, testes, regeneração de shaders, APK, instalação, captura, benchmark ou ADB. Geração documental e de assets não comprova execução da engine.

V2 §§18.1–18.4 e complemento componentes/código: cardinalidade, associação explícita collider→body, referências persistentes, reparent, distinção entre componente/recurso/serviço. As dez imagens ItsMagic orientam cabeçalhos compactos, campos internos e seleção contextual; não comprovam capacidades da Astra.

Fontes primárias consultadas:

- [Unity — compound colliders](https://docs.unity3d.com/6000.0/Documentation/Manual/compound-colliders-introduction.html): coleção de formas no objeto ou filhos tratada por um corpo. A associação explícita da Astra segue seu plano, não uma descoberta implícita de Rigidbody.
- [Jolt StaticCompoundShape](https://github.com/jrouwe/JoltPhysics/blob/78d483dc3d375581203cf070ea2790e8045e0879/Jolt/Physics/Collision/Shape/StaticCompoundShape.h) e [CompoundShape](https://github.com/jrouwe/JoltPhysics/blob/78d483dc3d375581203cf070ea2790e8045e0879/Jolt/Physics/Collision/Shape/CompoundShape.h): revisão efetivamente vendorizada, registrada em native/third_party/JoltPhysics/VENDORED_COMMIT.txt.
- [BodyCreationSettings](https://github.com/jrouwe/JoltPhysics/blob/78d483dc3d375581203cf070ea2790e8045e0879/Jolt/Physics/Body/BodyCreationSettings.h) e [Body](https://github.com/jrouwe/JoltPhysics/blob/78d483dc3d375581203cf070ea2790e8045e0879/Jolt/Physics/Body/Body.h): propriedades e comandos consumidos pela integração.
- native/physics/jolt_bridge.h/.cpp: Point/Hinge/Slider/Distance e motores já tinham consumidores na ABI V2. Este bloco os conecta ao componente autoral. Não equivale a toda a API Unity/ItsMagic.

StaticCompoundShape significa estrutura de subformas fixa; o corpo pode ser estático, cinemático ou dinâmico.

## Implementação conectada

| Parte | Autoria | Consumidor escrito |
|---|---|---|
| Colisor 3D v3 | Repetível; caixa/esfera/cápsula; enabled; centro/rotação; owner | Agrupamento por owner e transformação de cada subforma em um corpo Jolt composto |
| Corpo físico v3 | Massa, movimento, atrito, restituição, velocidades, damping, gravidade, repouso e sensor | BodyCreationSettings, massa total e velocidade inicial |
| Junta v1 | Repetível; quatro tipos; corpo conectado, âncoras/eixos, limites/motor | Criação após todos os corpos no mesmo mundo, inclusive referências posteriores no arquivo |
| Referência tipada | PropertyId, tipo requerido, escopo, ID | Seletor filtrado, setter, remapeamento e revalidação de hierarquia |
| Inspetor | Cabeçalho e campos por instância | Editar, copiar/colar/restaurar/remover e ajustar colisor separadamente |
| Viewport | Forma do colisor aberto e âncoras da junta aberta | Linhas projetadas/recortadas, sem picking de shape |
| C# | Força, impulso, torque, velocidade e callbacks de sensor | ABI de acesso à cena v2, comandos no mundo Play e eventos após Step |
| Ícone | Junta raster com identidade Astra | PNG, catálogo, atlas, enum e Add/inspetor |

Fontes principais: native/scene/{components,component_properties,collider,physics_body,joint,script_runtime}.h; native/editor/editor_scene_physics.*, editor_component_references.h, editor_reference_picker.h, editor_collider_geometry.h, editor_screen.*, editor_session.*, editor_history.cpp, editor_script_bridge.*, editor_play_scene.h; native/physics/jolt_bridge.*; managed/Astra.Scripting/Behavior.cs e Runtime/{BehaviorWorld,NativeBehaviorRuntime}.cs; binding Android em android_main.cpp.

## Posse e construção

Collider.owner=0 significa **Neste objeto**, explicitamente. Um ID deve apontar para corpo no mesmo objeto ou ancestral, sem atravessar outro corpo independente. Colisor em filho não exige malha nem corpo próprio. O seletor filtra alvos; Neste objeto permite manter rascunho sem corpo, com diagnóstico ao executar.

Construção reúne colisores habilitados de objetos efetivamente ativos, considerando ancestrais. Corpo precisa de pelo menos uma forma ativa vinculada. Limites atuais: 64 componentes por objeto, 256 formas por corpo, mundo de 1.024 corpos e capacidades existentes de pares/contatos de 4.096. Não são resultados de desempenho.

Transform do colisor = transform mundial do objeto × pose local. A pose é convertida para o referencial rígido do corpo; escala entra nas dimensões. A origem autoral não é substituída pelo centro de massa. Caixa aceita escala não uniforme sem shear; esfera/cápsula exigem escala global uniforme. Rotação sob escala não uniforme pode produzir shear e é recusada com diagnóstico. Ajustar à malha recalcula dimensões/centro e zera a rotação local da sugestão; preserva owner/enabled.

Corpos independentes sob ancestrais móveis ainda são recusados: mecanismos usam objetos irmãos com Junta. Filhos com apenas colisores vinculados ao ancestral compõem seu corpo. Personagem mantém cápsula/motor próprios e não pode herdar pose de corpo móvel. Não existe mundo por componente.

AetherPhysics_CreateCompoundBodyV1 e AetherBodyDynamicsV1 são entradas adicionais, preservando layouts de corpo V1/V2. Settings mantêm referências às shapes durante criação; o corpo Jolt mantém as shapes depois. Falha interrompe Play e destrói o mundo candidato. Stop libera personagens antes do mundo e limpa mapas/eventos.

## Juntas

Junta conecta o corpo do objeto a outro corpo ativo distinto; exige pelo menos um dinâmico. Várias juntas podem coexistir. Isso não garante estabilidade de uma cadeia arbitrariamente sobre-restrita.

| Tipo | Campos consumidos |
|---|---|
| Ponto | Duas âncoras locais, uma por corpo; sem motor/limites |
| Dobradiça | Âncoras/eixos locais; limites em graus, mínimo −180..0 e máximo 0..180; alvo em graus e velocidade graus/s convertidos para radianos |
| Deslizante | Âncoras/eixos; limites/alvo em unidades de cena, velocidade em unidades/s |
| Distância | Limites não negativos em unidades de cena; não equivale a SpringJoint |

Motores de dobradiça/deslizante: desligado, velocidade, posição ou posição+velocidade; esforço máximo, frequência/amortecimento de posição. Campos sem consumidor são ocultos, preservando dados. Trocar tipo adequa intervalos e desliga motor quando o destino não o oferece, no mesmo histórico. Frames completos, ruptura, edição de motor em Play e mola de distância continuam pendentes.

## Identidade e referências

Widgets usam índice da coleção somente no frame desenhado. Abertura, menu, edição, clipboard, remoção e referência usam instanceId persistente. Tipo é identidade de capacidade, não de cópia. Cabeçalhos repetíveis mostram sufixo de instância. Add usa catálogo separado, respeita allowMultiple e mantém anexos recolhidos.

Seletor de objetos: busca nome/ID, paginação, pai, ID secundário e limpar. Compartilhado por owner do colisor, corpo da junta e ObjectReference C#. AssetReference genérico ainda não possui catálogo GUID; geometria de pacote mantém seu seletor próprio.

Duplicação resolve todos os novos IDs antes de remapear referências nativas e C#, incluindo irmãos; referências externas permanecem. Reparent prepara árvore candidata e recusa quebrar associação ancestral previamente válida. Referências já ausentes não se tornam válidas automaticamente. Excluir alvo preserva o ID pendente; UI mostra ausência e Play checa composição física.

Colar mantém identidade do destino e recusa referências nativas incompatíveis. Restaurar mantém identidade e pode deixar junta sem alvo, como rascunho. Autoria passa pelo histórico; Play usa cópia de execução.

## Arquivo e ABI

Arquivo permanece **AETHER_EDITOR 10**, pois IDs de instância já existem. Colisor e Corpo evoluem para versão 3; Junta inicia em 1. Colisor v1 migra dimensões; v2 preserva centro; rotação=0, owner=Neste objeto e enabled=true são defaults antigos. Corpo v2 preserva seis números e recebe novos defaults; v1 mantém a separação da caixa implícita. Leitor de tipos desconhecidos preserva dados. Migrações não foram executadas nesta rodada.

ScriptSceneAccess agora usa **versão 2**, conferindo size/version e novos pointers. Android resolve também Trigger. Nativo e managed precisam ser empacotados juntos; DLL/APK anteriores não comprovam compatibilidade. Defaults de ISceneAccess sinalizam métodos não suportados em adaptadores antigos; o adaptador Astra implementa os cinco novos métodos.

## Forças e eventos C#

ISceneAccess: AddForce, AddImpulse, AddTorque, AddAngularImpulse, GetBodyVelocity, além de SetBodyVelocity/MoveKinematic. Vetores são mundiais. Força/torque acumulam até Step; impulso é imediato. Forças requerem corpo dinâmico e retornam false para alvo incompatível. Handles são conferidos sob lock; ativação ocorre após liberar lock de escrita. Força em ponto arbitrário permanece pendente.

Sequência: Update → zero ou mais [FixedUpdate → personagem → Step Jolt → publicar poses → callbacks de sensor]. Passo 1/60 s, recuperação limitada a 0,25 s. Pausa não executa o ciclo; passo manual segue o mesmo caminho.

Sensor pertence ao **corpo inteiro**, sem resposta de contato. TriggerEnter/Stay/Exit(ObjectReference other) são entregues aos comportamentos habilitados do objeto proprietário do sensor. Eventos agregam par de corpos mesmo quando várias subformas se sobrepõem. Leitura ocorre por Step, não só ao final do frame. Direções seguem o listener nativo; o outro corpo não recebe automaticamente callback se não for sensor.

Limites: estático–estático não gera o par móvel requerido pelo filtro; sensor cinemático pode observar estáticos pelo caminho existente. CharacterMotor não está mapeado no encaminhamento de sensor. IDs de subcolliders, pontos/normais, contatos sólidos e filtros personalizados estão pendentes. Transform por script é recusado se mover corpo/personagem ou colisor ativo composto; reconstrução de compound em Play não existe.

[Exemplo C#](../componentes/fisica-codigo.md).

## Layout e arte

Cabeçalhos/campos seguem estrutura das imagens 2/4/9; catálogo contextual segue a intenção das imagens 3/5, com busca. Mantida identidade Astra. Ícone Junta: assets/astra-visual/icons/named/component/joint.png; [prompt e origem](../../assets/astra-visual/icons/named/component/joint-generation.md). Produzido por image_gen e integrado pelo empacotador existente.

Wireframe mostra apenas o componente aberto. Vermelho indica owner incompatível; desabilitado usa cor atenuada. Overlay recorta câmera/viewport, sem profundidade. Não é captura no aparelho, gizmo de arraste de shape ou exibição de todos os componentes. Densidade/toques/teclado/atlas aguardam execução autorizada.

## Pendências e futura aceitação

Ainda faltam: ShapeAsset/GUID; convexos/malha côncava; filtros por forma; mutação de shapes/owners em Play; corpos independentes sob ancestrais móveis; contatos sólidos; sensores com personagens; juntas fixa/6DOF/cone-twist/mola de distância; ruptura; força em ponto; API comum de todos os componentes; bindings gerados por schema único; runtime sem EditorDocument; player/exportação; demais famílias do quadro. Este bloco não implementa 270 componentes nem encerra a refundação.

Após autorização, executar uma rodada integrada:

1. Compilar nativo/managed, adaptar expectativas de widgets e mocks de ABI v2, regenerar shaders pendentes do bloco anterior e produzir APK/managed/atlas coerentes.
2. Exercitar arquivos legados/v10, componentes v1/v2/v3, instâncias, copiar/colar/reset/remover, duplicar referências, undo/redo e reparent.
3. Composição assimétrica, formas em filhos, origem/centro de massa, repouso/massa/escala/shear e os quatro tipos de junta/motores.
4. Força/impulso/torque, sensor com estático/dinâmico, vários substeps, pausa/passo/Stop, exceptions de callback e alvo ausente.
5. ADB somente autorizado: edição por toque, seletores, teclado, salvar/reabrir, código Aplicar/Play/Stop, captura em movimento e logs. Separar falhas e o que não foi coberto.

Na entrega inicial nenhuma etapa acima havia sido executada. A autorização posterior abriu a [rodada integrada](../validacao/2026-09-10-componentes-codigo.md): 767 testes nativos, 513 managed e 16 Java aprovados; Debug/Release produzidos; edição, Aplicar, Play, sensores, pausa/passo/Stop e reabertura exercitados no aparelho. Nem todas as combinações das etapas acima foram cobertas no Android; o relatório delimita a prova.

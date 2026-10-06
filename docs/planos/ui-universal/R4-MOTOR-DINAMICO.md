# R4.6 — motor de corpo dinâmico

Atualização: esta entrega inicial foi ampliada para autoria no objeto existente,
colisão medida/convexa/composta e apoio pela geometria real (formato v2). A receita
cilíndrica é somente um exemplo. Consulte
[o roadmap universal de objetos, colisão e controle](ROADMAP-OBJETOS-COLISAO-CONTROLE-UNIVERSAL-2026-10-04.md)
para o estado atual, a migração e os limites; as evidências abaixo documentam a revisão inicial.

Repositório verificado: `https://github.com/kacerato/attachsEngine.git`.
Checkout: `C:/Users/donod/Downloads/atchengine`. O roadmap integral continua
preservado; esta etapa não conclui R4 nem os pacotes restantes.

## Autoridade e cadeia real

`ComponentSchema/contratos gerados -> recipe -> EditorHistory/arquivo ->
GameWorld -> Canvas/InputService ou comando de script -> passo fixo ->
força/impulso Jolt -> pose PhysicsBody -> visual/canvas subordinado`.

DynamicBodyMotor requer PhysicsBody e Collider no mesmo objeto. Ativo, exige
corpo dinâmico sólido, colisor próprio habilitado e eixos de locomoção livres.
Character é incompatível. Corpo inexistente, sensor, estático, cinemático ou
setup incompleto produz diagnóstico; não se fabrica uma cápsula para substituir
um motor indisponível.

**Jogador dinâmico**, em Criar/Física, compõe Body dinâmico de 70 kg,
Collider Cylinder de raio 0,45/meia altura 1, motor e filho Visual cilíndrico
com recursos reais da biblioteca. A raiz começa com rotação travada nos três
eixos e sem amortecimento linear; essas configurações continuam editáveis no
Body. O filho tem somente malha/material. A composição tem um Undo e exige a
geometria carregada para aparecer disponível. Catálogo: 79 receitas. Física 3D:
11 tipos registrados, um tipo novo neste pacote; isso não é paridade com Godot.

## Locomoção, apoio e propriedades

Velocidade define o alvo; aceleração limita a força para alcançá-lo. Frenagem
limita a correção quando não há input; controle no ar multiplica esse limite.
A força usa a massa real autorada e o delta fixo de 1/60 s, sem sobrescrever
velocidade nem escrever Transform. Impulsos e contatos do solver permanecem
observáveis, e o motor os corrige gradualmente conforme os limites configurados.

O apoio usa um raio central e até quatro periféricos, sem alocação de coleção
por motor/subpasso. Ignora o próprio corpo, sensores e camadas sem interação.
Centro até os pés, alcance abaixo dos pés, raio de sondagem e rampa máxima são
editáveis no grupo Chão. A normal elegível projeta o alvo no plano da rampa.
Subida acima do apoio não concede um novo salto. O comando de salto é uma
tentativa única no passo seguinte; precisa de apoio e velocidade de salto >0.

Acompanhar plataforma acrescenta a velocidade no ponto de apoio, incluindo
velocidade angular. Desligar a opção mantém contato/fricção físicos, mas retira
o alvo explícito de acompanhar. Gravidade continua pertencendo ao Body/campos.
O ponto é consultado no solver, considerando o centro de massa físico de shapes
compostos; não se calcula rotação em torno da origem autorada.
O motor não adiciona uma segunda força de gravidade nem promete subir degraus
como CharacterVirtual.

Todos os nove números e dois booleanos são consumidos no passo físico. Alterar
valores usa o caminho de edição seguro existente; reconstruções preservam
velocidade do Body e input quando a instância do motor continua sendo a mesma.
Disable/removal/inatividade cancela tentativa de salto; cancelamento de UI
retira seu vetor. Remover Body/Collider é protegido por dependência estrutural;
remover motor referenciado pelo Canvas é protegido pelo contrato de referência.

## Autoria e API

NÃO IREI SER SIMPLISTA NO DESIGN.

A receita apresenta componentes e filho visual antes de criar. A seleção abre
o Inspector do motor nos grupos Locomoção/Chão; não cria outro painel fixo.
O novo ícone corpo/força é gerado em `tools/generate-dynamic-motor-icon.py` e
empacotado pelo atlas real. O Canvas usa o mesmo picker e contrato para aceitar
Character ou DynamicBodyMotor/Body compatíveis. Nulo continua significando
ações globais; atribuir um corpo sozinho não inventa um motor.

`Astra.Components.DynamicBodyMotor` oferece propriedades tipadas geradas da
mesma declaração. `GameObject.DynamicMotor()` oferece acesso ligado a mundo,
geração e instância:

- `Move(Vector2, yawRadians)` aplica no quadro de scripts atual; FixedUpdate
  pode substituir Update. Na ausência de novo comando, o próximo quadro volta
  ao input do Canvas. Não há input persistente por chamada esquecida.
- `ReleaseMove()` devolve o controle ao Canvas.
- `TryJump()` informa que uma tentativa foi enfileirada, não que o salto foi
  aceito no ar. Só uma tentativa fica pendente por motor.
- `ReadBodyState()` lê velocidades/centro de massa/sono reais do Body.

Os comandos usam as operações tipadas 100–103 do despacho físico existente,
verificando a instância DynamicBodyMotor. Operações 0–9 de PhysicsBody não
mudam. Layout da tabela e struct de estado permanecem ABI 43/48 bytes; não se
reempacota um estado de motor dentro de um estado de corpo. O SDK e native
atuais precisam ser distribuídos juntos para o tipo e os comandos novos.

A validação do receptor é compartilhada também pela leitura de ações do bridge
C#. A conferência Android encontrou esse caminho ainda restrito a Character;
ele foi corrigido e o cenário existente de API scoped agora percorre as duas
autoridades, preservando isolamento entre Canvas e recusa de leases retirados.

No exemplo de aceite, **IMPULSO** emite uma ação independente e o script usa
`PhysicsBody().AddImpulseAtPosition` no centro de massa. A posição é publicada
em LateUpdate após a física. A leitura `DynamicMotor().ReadBodyState()` atravessa
o mesmo bridge usado por scripts de usuário.

## Referências e decisões

- [Godot 4.5 RigidBody3D](https://docs.godotengine.org/en/4.5/classes/class_rigidbody3d.html) e [fonte 4.5-stable](https://github.com/godotengine/godot/blob/4.5-stable/scene/3d/physics/rigid_body_3d.cpp): forças pertencem ao corpo e a pose deve acompanhar a simulação. Aqui, reutilizamos o Body/Jolt e seu ownership.
- [Godot 4.5 Using RigidBody](https://docs.godotengine.org/en/4.5/tutorials/physics/rigid_body.html): controlar a integração física evita dois escritores de pose; o Inspector separa massa, damping e locks. O motor acrescenta somente locomoção e sondagem à autoria existente.
- [Unity 6000.0 Rigidbody.AddForce](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Rigidbody.AddForce.html): força depende de massa e passo; impulso altera movimento uma vez. Adaptamos os dois caminhos existentes no bridge Jolt, sem escrever velocidade para simular força.

Vídeos foram procurados para workflow; não foram observados como evidência de
interação. A implementação se apoia no código/manual oficiais e no aceite da
UI executável, registrado separadamente em `VALIDACAO-2026-10-04.md`.

## Limites explícitos

Eixo vertical é Y mundial. A sonda exige configurar dimensões coerentes com o
colisor/escala; não promete ajustar automaticamente malhas, rotação livre ou
gravidade arbitrária. Degraus, floor snap, coyote time, jump buffering com janela
temporal, respawn, pareamento de hardware e Inspector remoto continuam pendentes.
O motor não cancela contatos/fricção quando a opção de plataforma está desligada.
Grounding por sondagem não prova todos os formatos de colisão, bordas ou rampas.
Multitouch sintético host não substitui aceite de dedos simultâneos no Android.

# Navegação: malha, agentes, obstáculos e links (bloco J — F067–F070)

Família nova **Navegação** com cinco componentes, recurso de projeto `.navmesh` e o
avaliador de Play `runtime/scene_navigation`. Dependência nova aprovada pelo usuário
em 09/10/2026: **Recast/Detour 1.6.0** (zlib), vendorizado sem alterações em
`native/third_party/recastnavigation/` (Recast, Detour, DetourCrowd, DetourTileCache;
origem e commit em `VERSION.txt`).

Referências estudadas:

- Unity AI Navigation 2.0 — [NavMeshSurface](https://docs.unity3d.com/Packages/com.unity.ai.navigation@2.0/manual/NavMeshSurface.html),
  [NavMeshLink](https://docs.unity3d.com/Packages/com.unity.ai.navigation@2.0/manual/NavMeshLink.html),
  [NavMeshModifier](https://docs.unity3d.com/Packages/com.unity.ai.navigation@2.0/manual/NavMeshModifier.html);
  Unity 6000.0 [NavMeshAgent](https://docs.unity3d.com/6000.0/Documentation/Manual/class-NavMeshAgent.html),
  [NavMeshObstacle](https://docs.unity3d.com/6000.0/Documentation/Manual/class-NavMeshObstacle.html).
- Godot 4.5 [navegação 3D](https://docs.godotengine.org/en/4.5/tutorials/navigation/navigation_introduction_3d.html),
  [NavigationAgent3D](https://docs.godotengine.org/en/4.5/classes/class_navigationagent3d.html).
- [Recast/Detour 1.6.0](https://github.com/recastnavigation/recastnavigation/tree/v1.6.0) (exemplo `Sample_TempObstacles`).

## Componentes (5 implementados)

| Componente | Id | Consumidor | Persistência |
|---|---|---|---|
| Superfície de navegação | `astra.navigation.surface` | bake no editor; malha carregada no Play | v1 + GUID do `.navmesh` |
| Agente de navegação | `astra.navigation.agent` | multidão do Detour → Personagem, Motor dinâmico, corpo ou pose | v1 |
| Obstáculo de navegação | `astra.navigation.obstacle` | recorte no TileCache | v1 |
| Link de navegação | `astra.navigation.link` (vários por objeto) | conexão fora da malha nos tiles | v1 |
| Modificador de navegação | `astra.navigation.modifier` | área por triângulo no bake ou exclusão | v1 |

### Superfície
- **Coletar**: toda a cena, este objeto e filhos, ou um volume (centro/tamanho no espaço do objeto); **camada** física (todas ou uma).
- Agente do bake: raio, altura, degrau, inclinação máxima. Precisão: célula, altura da célula, área mínima de região, tile (16–128 células). Avançado: aresta máxima, erro de borda, amostra e erro de detalhe.
- Coleta os **colisores estáticos** pelo mesmo caminho do Play (inclusive colisor Malha), com a forma real do Jolt; esferas e cápsulas saem tesseladas pelo Jolt. Objetos com Agente ou Obstáculo não viram chão assado.
- Bake em **tiles** de camadas do TileCache numa thread de trabalho, com progresso por tile e **Cancelar** (a malha anterior fica). O resultado é `Navegação/<nome>.navmesh`, registrado como `navmesh` no registro de recursos; assar de novo reescreve o mesmo recurso e mantém o GUID. A atribuição do recurso à Superfície é um passo de Desfazer.
- Inspector: cartão com polígonos, área, tiles e tempo do bake; estado **Assada e atual / Desatualizada / Recurso ausente / Sem malha**. "Desatualizada" compara o hash da geometria coletada hoje (mais as configurações) com o do bake. Botões Assar/Assar de novo/Cancelar, Malha visível/oculta e Desvincular.
- Viewport: polígonos translúcidos por área (caminhável, salto, difícil) e o contorno.
- Métodos: `is_ready`, `polygon_count`.

### Agente
- Velocidade, aceleração, giro, distância de parada, frear ao chegar; raio, altura, deslocamento da base; desvio de agentes (nenhum, baixo, médio, bom, alto — presets do Detour); áreas (usar salto, atravessar difícil, custos).
- **Superfície** explícita ou a mais próxima; **Seguir objeto** persegue um alvo sem script, refazendo o caminho quando o alvo se move além de "Refazer caminho a".
- Quem move o objeto: **Personagem** e **Motor dinâmico** recebem a velocidade pela posse de controle, fonte **IA** (a de menor prioridade: o jogador ou um script vencem); **corpo móvel** recebe a velocidade linear; sem física, o agente move a **própria pose**. O corredor do agente segue a posição real do objeto a cada quadro. Giro para o movimento: Personagem e pose.
- Links: o Personagem e a pose seguem a travessia da multidão; corpo dinâmico segue por velocidade.
- Métodos: `set_destination(Vector3)`, `stop`, `resume`, `warp(Vector3)`, `remaining_distance`, `path_status` (0 nenhum, 1 completo, 2 parcial, 3 inválido), `has_path`, `is_on_link`, `velocity`.
- Eventos: `destination_reached`, `path_failed`, `link_entered` (chaves 27–29 das conexões de evento); conexões podem chamar `stop` e `resume` (chaves 30–31).

### Obstáculo
- Caixa (gira com o objeto em Y) ou cilindro; centro, tamanho, raio, altura.
- Sempre **recorta** a malha (TileCache): só os tiles tocados são remontados. "Recortar só parado" tira o recorte enquanto o objeto se move e o recoloca depois de "Tempo até parado"; o limiar de movimento decide o que é mover.
- Método: `is_carving`.

### Link
- Início e fim no espaço do objeto, raio de conexão, sentido duplo ou único, área (caminhável, salto, difícil). Mudanças em Play reconstroem só os tiles das pontas.

### Modificador
- Alterar área (caminhável, não caminhável, salto, difícil) ou Ignorar no bake; aplica aos filhos que não têm modificador próprio.

## Editor

- Criar → **Navegação** (categoria nova): Superfície, Agente que persegue (Personagem + Agente, alvo = seleção), Obstáculo (com caixa visual) e Link.
- Ícones novos `navigation/surface`, `agent`, `obstacle`, `link`, `modifier` no atlas.
- Gizmos: anel e altura do agente, forma do obstáculo, arco do link com as pontas, volume da superfície.

## Diferenças da referência

| Aspecto | Astra | Classificação |
|---|---|---|
| Tipos de agente (Agent Type ID) | Superfície escolhida ou a mais próxima | Adaptação explícita |
| NavMeshObstacle sem Carve (só desvio) | Obstáculo sempre recorta | Pendente |
| Áreas do projeto com nomes e custos | Três áreas embutidas; custo por agente | Pendente |
| Superfície que se move com o objeto | Malha fica onde foi assada | Pendente |
| NavMeshModifierVolume | Não implementado | Pendente |
| Prioridade de desvio (avoidance priority) | Não implementado (Detour não tem) | Pendente |
| Animação de travessia de link (autoTraverseOffMeshLink=false) | Travessia linear da multidão | Pendente |
| Consultas estáticas (NavMesh.SamplePosition/CalculatePath/Raycast) pela API C# | Existem no NavWorld; não expostas | Pendente |
| Compressão das camadas no arquivo | Camadas guardadas sem compressão | Adaptação explícita |

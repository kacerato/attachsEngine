# Mais 50 — campos físicos 3D

## Contagem do pacote

**4 componentes + 40 propriedades semânticas + 6 entradas de API = 50 adições.**
Este pacote é adicional ao pacote de molas/comandos físicos de 2026-10-01.
Vector3 conta uma propriedade, sem recontar XYZ como três features.
As mesmas oito propriedades comuns são implementadas para cada um dos quatro
tipos; não são 40 conceitos de propriedades diferentes. Fachadas geradas e
contratos auxiliares não aumentam essa contagem.

| Nº | Categoria | Adição |
|---:|---|---|
| 1 | Componente | `GravityField` |
| 2 | Componente | `WindField` |
| 3 | Componente | `DragField` |
| 4 | Componente | `RadialField` |
| 5 | GravityField | `Enabled` |
| 6 | GravityField | `WakeBodies` |
| 7 | GravityField | `Shape` |
| 8 | GravityField | `HalfExtents (Vector3)` |
| 9 | GravityField | `Radius` |
| 10 | GravityField | `Offset (Vector3)` |
| 11 | GravityField | `Falloff` |
| 12 | GravityField | `AffectedLayer` |
| 13 | GravityField | `Vector (Vector3)` |
| 14 | GravityField | `ReplaceWorldGravity` |
| 15 | WindField | `Enabled` |
| 16 | WindField | `WakeBodies` |
| 17 | WindField | `Shape` |
| 18 | WindField | `HalfExtents (Vector3)` |
| 19 | WindField | `Radius` |
| 20 | WindField | `Offset (Vector3)` |
| 21 | WindField | `Falloff` |
| 22 | WindField | `AffectedLayer` |
| 23 | WindField | `Vector (Vector3)` |
| 24 | WindField | `Coefficient` |
| 25 | DragField | `Enabled` |
| 26 | DragField | `WakeBodies` |
| 27 | DragField | `Shape` |
| 28 | DragField | `HalfExtents (Vector3)` |
| 29 | DragField | `Radius` |
| 30 | DragField | `Offset (Vector3)` |
| 31 | DragField | `Falloff` |
| 32 | DragField | `AffectedLayer` |
| 33 | DragField | `LinearDrag` |
| 34 | DragField | `AngularDrag` |
| 35 | RadialField | `Enabled` |
| 36 | RadialField | `WakeBodies` |
| 37 | RadialField | `Shape` |
| 38 | RadialField | `HalfExtents (Vector3)` |
| 39 | RadialField | `Radius` |
| 40 | RadialField | `Offset (Vector3)` |
| 41 | RadialField | `Falloff` |
| 42 | RadialField | `AffectedLayer` |
| 43 | RadialField | `Acceleration` |
| 44 | RadialField | `TangentialAcceleration` |
| 45 | API runtime | `GameObject.PhysicsField(string typeId)` |
| 46 | API runtime | `PhysicsFieldRuntime.Sample(Vector3, uint)` |
| 47 | API runtime | `TrySample(Vector3, uint, out PhysicsFieldSample)` |
| 48 | API runtime | `Contains(Vector3)` |
| 49 | API runtime | `AffectedBodyCount` |
| 50 | API runtime | `AffectedMass` |

Registro após a expansão: **41 schemas, 40 tipos anexáveis, 40 fachadas**.
Receitas: **69 → 73**. Ícones: **243 → 247**.
Os quatro tipos novos têm consumidor executável. Não há novo tipo parcial ou
placeholder neste pacote, nem declaração de paridade com o atlas de pesquisa.

## Referências versionadas e limites da adaptação

Godot **4.5**, [Area3D](https://docs.godotengine.org/en/4.5/classes/class_area3d.html):
gravidade de área, gravidade pontual, amortecimento linear/angular e propriedades
de vento motivam a família. A documentação limita o vento de Area3D a SoftBody3D.
Astra adapta o conceito para corpos rígidos dinâmicos, com acoplamento em kg/s;
não promete SoftBody, árvores ou cloth.

[Código oficial do solver Godot 4.5](https://github.com/godotengine/godot/blob/4.5/modules/godot_physics_3d/godot_area_3d.cpp):
`compute_gravity` distingue vetor e centro pontual e trata o centro sem direção.
Extraído esse cuidado numérico. Astra usa aceleração radial constante modulada
por falloff, com componente tangencial própria; não copia gravidade inversa
ao quadrado nem prioridades de Area3D.

[Código oficial do node/Inspector Godot 4.5](https://github.com/godotengine/godot/blob/4.5/scene/3d/physics/area_3d.cpp)
e [workflow de áreas, Godot 4.5](https://docs.godotengine.org/en/4.5/tutorials/physics/using_area_2d.html):
separar volume, efeito e política de combinação evita campos sem significado.
Astra mantém esses grupos no Inspector contextual, usando seus próprios
controles vetoriais e criação. A referência 2D informa o workflow; o consumidor
implementado aqui é 3D. Foi localizado o
[walkthrough móvel de Area3D no Xogot](https://github.com/xogot-projects/Xogot-Area3D);
o vídeo não foi reproduzido e não é usado como prova de comportamento.

Backend real: Jolt vendorizado, commit
`78d483dc3d375581203cf070ea2790e8045e0879`.
Força, velocidade, sono, massa e gravidade são lidos/escritos no solver existente.

## Cadeia e contratos

`Create/Add → ComponentType → reflection/Inspector → archive → GameWorld →
ScenePhysics::applyPhysicsFields → Jolt → snapshot/SDK/gizmo`.

Os campos usam COM atual do solver para testar pertencimento, inclusive para
estatísticas. Não detectam interseção de superfícies de collider, nem enviam
eventos enter/exit. Apenas corpos dinâmicos ativos participam; estáticos,
cinemáticos, Character e Physics2D não recebem esses efeitos.

Cada campo tem autoria versão 1; todos os controles expostos são persistidos e
consumidos. Alteração de propriedade é vista no próximo passo fixo, sem
reconstruir o corpo. Alterações estruturais seguem o safe point de GameWorld.
Remoção invalida consultas depois do flush; Stop rejeita callbacks antigos.
Desativar componente ou ancestral remove o efeito. Estado temporal do solver
não é salvo como configuração do campo.

Volume usa matriz completa da hierarquia, incluindo escala não uniforme:
esfera escalada é elipsoide; centro local acompanha pais. Vetores de efeito
usam uma base ortonormal extraída da orientação, sem amplificação pela escala.
Caixa mede distância normalizada por máximo eixo; esfera por distância radial
local. Uniforme vale 1 dentro da borda; Linear vale 1-d; Suave usa smoothstep.
No centro radial e nos polos do eixo de vórtice, direções singulares contribuem
zero. Transformação não invertível produz erro explícito.

Gravidades locais somam acelerações. A maior influência dos campos de
substituição cancela a gravidade mundial uma única vez, respeitando o
GravityFactor real do corpo nessa compensação. A aceleração local independe
desse fator. Vento acumula coeficiente e velocidade ponderada; a massa real
determina resposta. Vento e arrasto linear têm integração exponencial conjunta,
que evita inversão por taxas altas. Arrasto angular também é exponencial.
Forças/acelerações são aplicadas antes da atualização Jolt; limites, locks,
colisões e amortecimento próprio do corpo continuam sob controle do solver.

WakeBodies falso preserva corpos dormindo. Verdadeiro permite acordar quando
há mudança efetiva; campo com efeito numérico zero não mantém corpo acordado.
Camada é todas ou uma das 32 camadas, sem modificar a matriz de colisão.
Estatísticas contam corpos elegíveis agora, não garantem força diferente de
zero: um volume habilitado de gravidade zero pode contar um corpo.

ABI **33** acrescenta FieldQuery ao final de ScriptSceneAccess. O callback
BodyCommand e seu payload de 48 bytes conservam posições/layout. Novo payload
de campo: 64 bytes com size/reserved, flags, peso, aceleração, vento, taxas e
estatísticas. SDK/native exigem versão/callback completos. Consultas validam
mundo, geração, identidade de componente, camada e ponto finito. Contains é
geométrico mesmo com campo desativado; Sample retorna influência zero.
Queries exigem sessão física rodando. TrySample retorna falhas de estado;
argumento inválido continua sendo erro de chamada.

## Defaults e domínios

| Propriedade | Default | Domínio/unidade |
|---|---|---|
| Enabled / WakeBodies | true / true | bool |
| Shape / Falloff / AffectedLayer | caixa / uniforme / todas | caixa ou esfera; uniforme/linear/suave; 32 camadas ou todas |
| HalfExtents / Radius | (3,3,3) / 3 | 0,001–10000 m |
| Offset | (0,0,0) | ±10000 m |
| Gravity.Vector / ReplaceWorldGravity | (0; -9,81; 0) / true | ±10000 m/s² / bool |
| Wind.Vector / Coefficient | (5,0,0) / 1 | ±10000 m/s / 0–10000 kg/s |
| LinearDrag / AngularDrag | 1 / 1 | 0–1000 s⁻¹ |
| Acceleration / TangentialAcceleration | -9,81 / 0 | ±10000 m/s² |

Valores vetoriais são escritos atomicamente. Dimensão zero e número não finito
são rejeitados. HalfExtents aparece só para caixa; Radius só para esfera.
Ambos ficam persistidos para troca de forma sem perder configuração.

## Editor

NÃO IREI SER SIMPLISTA NO DESIGN.

Seleção → Volume / Efeito / Alcance na superfície contextual existente.
Proposta estrutural aplicada: o volume e suas direções aparecem no próprio
viewport; os controles trocam por contexto, sem novo painel permanente.
O Inspector reaproveita edição XYZ, busca, grupos, reset e menu de componente.
Create e Add possuem os quatro tipos na família Física 3D/Campos. Receitas
criam o campo no alvo de vista; não criam collider físico decorativo.

Gizmo usa o mesmo volume transformado, centro e direção do modelo. Detalhes
aparecem na seleção. Cada tipo tem ícone próprio integrado em SVG/PNG, enum,
catálogo e atlas. A imagem gerada é somente hipótese visual; as capturas
`field-volume.png`, `field-wind.png`, `field-scope.png` e `field-sphere.png`
são rasterizações executáveis da UI real, com atlas/font reais.
Elas não renderizam a cena Vulkan nem substituem teste de toque no aparelho.
A revisão corrigiu enquadramento da fixture e encurtou rótulos de vetores para
manter valores e unidades legíveis. Orientação de cena permanece em paisagem;
este pacote não altera a orientação já corrigida no pacote anterior.

## Validação e custo

Resultados e hashes finais: `docs/validacao/evidencias/fields50-20261001/`.

Executável dedicado com quatro cenários:
1. Quatro receitas reais, serialização/reabertura, escrita vetorial atômica,
   undo/redo e volume visual.
2. Hierarquia, orientação, escala não uniforme, caixa/esfera, falloff
   linear/suave, camada e desativação.
3. Jolt real: gravidade sobreposta, vento dependente da massa, arrasto forte
   linear/angular, atração/vórtice e sono/ativação.
4. ScriptBridge nativo real, ABI33, pertencimento/massa/camada, edição viva,
   mundo/geração/layout inválidos, remoção segura e Stop.
O runtime gerenciado nesse cenário é capturado por harness, portanto a prova
do consumidor físico é nativa; não se apresenta isso como execução C# completa.

Também são repetidos apenas os quatro cenários do pacote anterior, incluindo
CCD e comandos de corpo, e dois testes gerenciados de layout (anterior/novo).
A suíte geral não é executada.

Resultado final: **4/4 cenários novos, 4/4 regressões anteriores, 2/2 testes
gerenciados de ABI passaram**. SDK e script: zero avisos/erros. Build Android:
**assembleDebug passou**. SDK, Rendering e atlas dentro do APK têm bytes iguais
aos assets gerados. Cinco capturas executáveis tiveram zero instâncias
descartadas, fontes ausentes ou comandos recortados.

Candidatos de campo são indexados por revisão estrutural e matrizes são
preparadas uma vez por passo fixo. O loop custa O(corpos × campos); não há índice
espacial nesta versão. Benchmark dedicado mede 96 corpos/32 campos sobrepostos
em 60 passos, incluindo campos, solver e sincronização, com aquecimento.
É host Debug, não FPS Android; números ficam em `benchmark.log`.
Medição desta execução: média **2,709 ms**, p95 **2,968 ms**, máximo **3,127 ms**
por passo completo. Não é medição isolada do campo nem limite universal.

Projeto editável: `build/acceptance/Fields50-20261001-final`, quatro campos,
quatro esferas físicas, câmera, sol e `Scripts/Fields50Probe.cs`. Export verifica
reabertura do arquivo. O script compila contra a SDK e usa as seis APIs mais
fachada gerada; seu READY/PASS físico no Android ainda precisa ser obtido.
APK Android compilado e empacotamento conferido não significam validação física.
Nesta rodada não houve instalação/interação com o aparelho, respeitando o
encerramento da rodada anterior pedido pelo usuário. Não há captura Android
ou alegação de toque/Play físico novo para este pacote.

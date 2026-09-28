# O2 — Primitivas com geometria e colisão

28/09/2026. Etapa 5 do bloco ampliado. Implementada e validada no Android. Seis primitivas disponíveis (cinco novas e
cubo preservado); sete itens do inventário cobertos nesta entrega.

## Contrato e referências

A referência de capacidade é a [Unity 6000.0, Primitive objects](https://docs.unity3d.com/6000.0/Documentation/Manual/PrimitiveObjects.html)
e sua [API CreatePrimitive](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/GameObject.CreatePrimitive.html).
O princípio aplicado é criar uma forma utilizável, com renderização e colisão,
cuja transformação e material já podem ser editados. Dimensões locais:

| Tipo | Geometria | Colisão inicial |
| --- | --- | --- |
| Cubo | 1 × 1 × 1; malha anterior preservada | Caixa |
| Esfera | Raio 0,5; 32 segmentos × 16 intervalos | Esfera analítica |
| Cápsula | Altura 2, raio 0,5, eixo Y | Cápsula analítica |
| Cilindro | Altura 2, raio 0,5, eixo Y, 32 lados | Casco convexo da malha |
| Plano | 10 × 10 em XZ, 200 triângulos, frente +Y | Malha triangular |
| Quad | 1 × 1 em XY, dois triângulos, frente +Z | Malha triangular |

O cilindro usa o casco de 32 lados, não uma superfície física circular exata.
Esfera/cápsula usam colisores analíticos e malhas visuais tesselladas. Plano e
quad começam como corpos estáticos: o backend recusa malha triangular dinâmica;
não converte silenciosamente em caixa. As novas malhas têm frente definida;
o controle existente de faces do material pode habilitar as duas faces.

O [código de PrimitiveMesh da Godot 4.5](https://github.com/godotengine/godot/blob/4.5/scene/resources/3d/primitive_meshes.cpp)
foi usado para estudar separação de tampas, costuras UV e bases de normais e
tangentes. A implementação da Astra gera diretamente seu formato de vértice.
O [menu de criação da Unity 6000.0](https://github.com/Unity-Technologies/UnityCsReference/blob/6000.0/Editor/Mono/Commands/GOCreationCommands.cs)
mostra a criação contextual de objetos: na Astra, as receitas entram na folha
existente, com seleção de destino e descrição da forma antes de criar.

## Caminho dos dados

`PrimitiveType → biblioteca de geometria → GUID/slot → MeshRenderer + PhysicsBody
+ Collider → renderer/Jolt`. Editor e runtime usam `configurePrimitive`.
O cubo conserva geometria, slot inicial e identidade legada. As cinco formas
novas têm GUID derivado de tipo/versão, independente da ordem da biblioteca.
Material é o PBR existente; não há propriedades exclusivas sem consumidor.

`parent.CreatePrimitive(PrimitiveType.Sphere)` cria um filho no transform local
identidade. O pai é explícito, como em `CreateChild`. A ponte nativa/gerenciada
passa à ABI 18. Enum inválido, pai vencido e recurso ausente são recusados;
o host injeta a biblioteca real antes de iniciar os scripts. Criação publica
invalidação física; destruição segue o ponto seguro e remove os corpos.

No editor, uma criação é um comando de histórico. Colisores, material,
transformação e vínculo de malha usam os formatos de cena existentes. Plano e
quad recebem novos SVGs no gerador de ícones, rasterizados e integrados ao atlas
e ao enum consumidos pelo aplicativo. Não há imagem conceitual substituindo UI.

## Validação concluída

- Geometria: dimensões, bounds, índices, triângulos não degenerados, orientação,
  normais unitárias, tangentes ortogonais e UVs.
- Editor: seis formas, undo/redo, salvar/reabrir, reordenar a biblioteca por GUID
  e cozinhar todos os colisores no Jolt real.
- ABI: criar as seis, consultar colisão, destruir e confirmar ausência física.
  A diagonal de um quad pode produzir dois acertos triangulares; o teste respeita
  o total retornado pela consulta, mesmo com buffer de um resultado.
- Script de aceite `tests/fixtures/primitives/PrimitiveProbe.cs`: compilação pelo
  compilador de projeto, materiais PBR diferentes e raios contra seis objetos.

Não altera o estado das etapas de prefab, ícones de objeto ou Static.

No aparelho 25053PC47G, projeto separado `PrimitiveBlock0928`, o fluxo real foi:
criar esfera pela folha → enquadrar → editar cor R de 0,55 para 0,9 no Inspector
→ desfazer/refazer → salvar → encerrar e reabrir → Play. A esfera autoral
conservou a cor. O script criou as seis formas, mudou os materiais e confirmou
os seis colisores às 16:20:01 e 16:21:31. O segundo cenário girou o quad 180°
para mostrar sua frente; a captura anterior mostrava seu verso corretamente
recortado. A captura final mostra as seis formas da API e a esfera autoral.

Build C++ e Android concluídos. 4 execuções de teste `primitives_` (incluindo
uma regressão de glTF), 11 `creation`, 1 `independent_authoring`, 4 `editor_map_`
e 32 `play_` passaram; 20 testes C# de comportamentos e o cenário de primitivas
compilado pelo compilador do projeto passaram. Esses filtros podem sobrepor-se;
não representam uma contagem de testes únicos. Não foi feito benchmark nem
validação em portrait nesta entrega.

APK instalado: `FCC4610EFB63A59381CDDFDE3BA107AC41F7B9ADB03F65FB4392682E6390D584`.
[Evidências](../validacao/evidencias/o2-primitivas-20260928/): menu real, edição,
undo, cena salva, reabertura, captura das seis formas e log de aceite.

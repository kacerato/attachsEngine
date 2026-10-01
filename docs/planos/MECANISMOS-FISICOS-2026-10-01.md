# Bloco de mecanismos físicos — 2026-10-01

## Entrega e fronteira

Expansão vertical dos componentes existentes Joint e Collider: cinco novos modos
de junta (Fixed, Cone, SwingTwist, SixDOF, Spring), cilindro nativo, nove receitas,
referenciais completos, 58 propriedades numéricas adicionais e 12 enums de canais.
Não são cinco novos schemas: Joint passa de quatro para nove modos e Collider de
quatro para cinco formas. O catálogo passa de 57 para 66 receitas. Os 34 schemas/33 fachadas não devem ser infladas por variantes.

Implementar o bloco inteiro antes de executar build/testes foi a restrição desta
entrega. O fechamento usa uma cena integrada e verificações de fronteira/solver,
com capturas do Inspector executável. Resultados ficam na seção de validação.

## Referências concretas

- Unity 6000.0 [FixedJoint](https://docs.unity3d.com/6000.0/Documentation/Manual/class-FixedJoint.html)
  e [SpringJoint](https://docs.unity3d.com/6000.0/Documentation/Manual/class-SpringJoint.html):
  conexão entre corpos, âncoras e resposta de mola. Astra usa frequência em Hz e
  razão de amortecimento do Jolt, não os coeficientes em N/m da Unity.
- Godot 4.5 [Generic6DOFJoint3D](https://docs.godotengine.org/en/4.5/classes/class_generic6dofjoint3d.html)
  e [ConeTwistJoint3D](https://docs.godotengine.org/en/4.5/classes/class_conetwistjoint3d.html):
  separar translação/rotação e restringir os eixos independentemente. Astra não
  expõe bias/softness sem equivalente real no backend.
- Godot 4.5 [CylinderShape3D](https://docs.godotengine.org/en/4.5/classes/class_cylindershape3d.html):
  forma física com raio e altura, separada da malha visual.
- Código Jolt **5.6** vendorizado em `native/third_party/JoltPhysics`,
  [arquitetura oficial](https://github.com/jrouwe/JoltPhysics/blob/v5.6.0/Docs/Architecture.md)
  e headers `FixedConstraint`, `ConeConstraint`, `SwingTwistConstraint`,
  `SixDOFConstraint`, `DistanceConstraint`: ownership da constraint pelo mundo,
  frames ortonormais, limites angulares e canais suportados.

## Cadeia funcional

Receita / Inspector / C# → schema tipado → Joint v2 ou Collider v7 → ScenePhysics
→ descritor V3 validado → constraint/shape Jolt → pose e consultas → editor.

V1 e V2 da ponte de juntas mantêm layout e recorte anteriores. V3 tem tamanho e
versão próprios, frames A/B, cone/torção e seis canais. Referenciais degenerados,
limites invertidos e motor num eixo travado são recusados antes do solver.
Joint v1 migra preservando os 19 números anteriores; Collider v1–v6 continua
legível. Identidades de componentes e referências aos corpos usam o mecanismo
existente de arquivo, clone, prefab e histórico.

6DOF usa uma base X/Y autorada em cada corpo. Cada canal tem Travado/Limitado/Livre,
limites, atrito, modo de motor, velocidade/alvo, força ou torque e mola do motor.
Rotações são graus no editor/API e radianos na ponte; limites Y/Z usam pirâmide
para permitir assimetria. A mola independente usa DistanceConstraint com limites
elásticos. Não há promessa de limites rotacionais elásticos, motores de
SwingTwist, quebra de juntas, ragdoll ou veículo neste bloco.

Cilindros têm eixo Y e tampas planas; escalas X/Z precisam coincidir. Altura Y
pode variar. A primitiva visual Cylinder passa a usar essa forma exata (raio .5,
altura 2), sem cozinhar casco da malha. Raycast, sweep e overlap preservam a
identidade do colisor. ShapeQuery.Cylinder expõe a mesma geometria ao C#.

## Autoria e lifecycle

Criar → Física → mecanismo cria corpo, colisor e junta em um comando de histórico.
Um corpo selecionado é conectado; sem ele a inspeção indica a referência ausente.
As âncoras locais são editáveis. A receita não afirma montar automaticamente um
ragdoll ou uma máquina completa.

Inspector reutiliza navegação por grupos e só mostra controles consumidos pelo
modo. Os seis canais têm destinos Mov X/Y/Z e Rot X/Y/Z; motor e limites surgem
conforme o estado. Ícones próprios entram no gerador vetorial e atlas real.
Mudar propriedades invalida a física no ponto seguro; reconstrução preserva
velocidades conforme a infraestrutura existente. Joint passa a permitir remoção
estrutural em Play, seguida de reconstrução, liberando o corpo no solver.

## Validação do bloco

A implementação completa precedeu a rodada de testes. Build nativo e SDK C#
passaram; SDK sem avisos nem erros. As 64 verificações nativas passaram:
5 cenários integrados `mechanisms_block`, 45 de contratos `component_`, 9 de
juntas, 4 de primitivas e 1 de todas as receitas compostas.

A cena integrada cobre cinco mecanismos no mesmo mundo, cilindros reais,
roundtrip, motor 6DOF, deformação da mola, consulta de tampa plana, edição do alvo
e remoção. O complemento angular distingue liberdade/limite/fixação e valida a
fronteira V3. Criação, conexão, Undo/Redo e retorno por toque são exercitados.
O teste revelou e corrigiu a ordem dos corpos no 6DOF: o alvo autorado movimenta
o proprietário relativamente ao corpo conectado, como o contrato promete.

Capturas reais do executável em 360×800 e 480×900 estão em
`docs/validacao/evidencias/mechanisms-20261001/`. Em retrato, o Inspector de Joint
ou cilindro ocupa a largura inteira, com retorno pela navegação existente;
paisagem conserva a distribuição anterior. A rotação da atividade Android do
editor foi liberada; o launcher mantém sua orientação. `concept-inspector.png`
é somente proposta visual; `inspector-360.png`, `inspector-480.png` e
`spring-360.png` são capturas da implementação executável.

Cena reabrível: `docs/validacao/evidencias/mechanisms-20261001/mechanisms.aescene`.
Logs dos testes acompanham as capturas. `:app:assembleDebug` passou, incluindo a rotação por sensor. APK:
`android/app/build/outputs/apk/debug/app-debug.apk`, SHA-256
`34FC20F1C4C36013EC28C957C3A96D27D3CABFD0BE38AF25CED369BD7D052F7B`.
O build Android mantém avisos de `extractNativeLibs` e API Java depreciada.
Ainda não há evidência visual física para este bloco: o aparelho estava bloqueado,
e a conexão ADB caiu durante a instalação. A instalação não foi confirmada; as
duas portas anunciadas por mDNS recusaram reconexão. Capturas host não substituem
evidência no aparelho.

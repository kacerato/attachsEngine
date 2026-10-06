# Character com visual e conversão — 04/10/2026

Checkout: `C:/Users/donod/Downloads/atchengine`; remoto verificado:
`https://github.com/kacerato/attachsEngine.git`. Continuação de R4.5, sem declarar
o restante do roadmap completo.

## Decisão e cadeia

Character é a autoridade de locomoção com cápsula própria. Body/Collider são
outra autoridade. O visual não deve obrigar o usuário a escolher uma física
incorreta, nem dois sistemas devem escrever sua pose.

`biblioteca de geometria -> receita ou conversão -> grafo/ComponentSchema ->
EditorHistory -> arquivo de cena -> Jolt CharacterMotor -> pose do visual ->
Canvas/ações existentes`.

A receita **Personagem cilíndrico**, em Criar/Física, produz raiz Character e
filho visual com MeshRenderer/material da biblioteca real. A disponibilidade
depende da geometria cilíndrica carregada. Os dois objetos entram no mesmo Undo.
Não acrescenta tipos de componentes: reutiliza Character, MeshRenderer e os
consumidores já registrados.

## Converter uma malha existente

Selecione a malha e abra **Ações do objeto > Criar raiz Character…**; a mesma
entrada está no menu contextual da Hierarquia. A prévia ocupa o Inspector,
mantendo o viewport visível. Ela informa a autoridade nova, a retirada de
Body/Collider, a cápsula sugerida e a possibilidade de Undo. Não abre um painel
permanente separado.

O comando mede os slots da geometria carregada, incluindo rotação e reflexão,
e cria uma raiz com escala
mundial unitária, aos pés do visual. Reparenta o objeto selecionado sem trocar
seu ID; conserva malha, materiais, componentes sobreviventes, filhos e suas
poses mundiais. A raiz substitui o visual na ordem dos irmãos. Body/Collider e
o flag físico legado são removidos do visual. Referências compatíveis continuam
apontando para o mesmo objeto; o alvo de entrada do Canvas deve ser escolhido
explicitamente na raiz Character criada.

Undo/Redo restaura os mesmos IDs, ordem, transforms e configurações físicas
anteriores. O arquivo de cena persiste a composição convertida pelo formato
existente. O histórico de Undo é de sessão, não um backup persistente para
desconverter depois de fechar o projeto.

## Limites e recusa explícita

O comando faz preflight sem mutação, seguido de revalidação no momento de aplicar.
Recusa geometria ausente, matrizes singulares/cisalhadas, dimensões fora do
contrato Character, hierarquia com
corpo móvel/Character ancestral e subárvore com outra autoridade física.
Instâncias de prefab devem ser desvinculadas primeiro.

Dependências de componentes sobreviventes e vínculos tipados são verificados.
Campos de script que referenciam os Body/Collider retirados bloqueiam a conversão,
inclusive arrays. Uma chamada a Body escrita no código de um script não é
reescrita automaticamente: a prévia explicita que esse código precisa de
adaptação. Nenhuma referência incompatível é apagada para forçar o comando.

A cápsula sugerida aproxima as dimensões da geometria; não preserva a forma
original de colisão. O usuário pode ajustar a cápsula e a locomoção no Inspector
Character após converter. A receita DynamicCylinder/DynamicBodyMotor foi
acrescentada no pacote seguinte [R4.6](R4-MOTOR-DINAMICO.md), por uma cadeia física
própria; esta conversão continua destinada ao Character. Modelos deformados/animados
exigem aceite próprio; medir a geometria carregada não comprova todos os estados
de animação.

## Referências registradas

- [Unity 6000.0: Character Controller](https://docs.unity3d.com/6000.0/Documentation/Manual/class-CharacterController.html): separar controle de personagem da dinâmica Rigidbody; forma e locomoção editáveis têm efeitos distintos. Adaptamos ao CharacterMotor/Jolt já existente, sem copiar o visual do editor.
- [Godot 4.5: CharacterBody3D](https://docs.godotengine.org/en/4.5/classes/class_characterbody3d.html) e [fonte 4.5-stable](https://github.com/godotengine/godot/blob/4.5-stable/scene/3d/physics/character_body_3d.cpp): autoridade de movimento do corpo controlado e detecção de chão/rampa são capacidades do motor, não da malha.
- [Godot 4.5: Player scene and input actions](https://docs.godotengine.org/en/4.5/getting_started/first_3d_game/02.player_input.html): workflow de corpo na raiz e representação visual subordinada. Aqui, a representação original conserva sua identidade ao receber uma raiz nova.

As buscas de vídeos encontraram tutoriais de CharacterController; não foram
usadas como prova de um workflow observado. As decisões acima se apoiam no
manual, tutorial e código oficiais.

## Aceite realizado

O host passou 68/68 cenários, incluindo criação com motor Jolt real, round-trip
do arquivo, preservação de material/IDs/referências/pose/filhos, Undo/Redo e
recusa sem mutação. O exemplo C# compila com o SDK real sem avisos ou erros.
O APK Android foi compilado e instalado; o SHA-256 local confere com `base.apk`.

No aparelho, foram exercitados menu contextual, prévia, conversão, Undo/Redo,
atribuição explícita do receptor, Save e reabertura. Em Play após reabrir,
o joystick deslocou a raiz a X=3,05; ao soltar, o vetor zerou. O botão elevou
a raiz a Y=0,82 e registrou um pulso de salto. O visual e a câmera acompanharam.
Criar/Física também gerou Personagem cilíndrico e Visual cilíndrico, e Undo
retirou os dois juntos. Os gestos são eventos ADB individuais no aparelho real;
não são prova de três dedos físicos simultâneos.

A prévia mantém o viewport visível e explica a troca de autoridade e as recusas.
A receita apresenta a composição antes de criar; os campos de cápsula aparecem
no Inspector real. As capturas usam os ícones Character e Cylinder do atlas
existente, sem imagem conceitual como prova. Evidência, pacote e capturas estão
em [VALIDACAO-2026-10-04.md](VALIDACAO-2026-10-04.md), seção R4.5.

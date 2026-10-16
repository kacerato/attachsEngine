# Estúdio de animação v2 e Biblioteca como gerenciador — proposta e execução

Data: 15/10/2026. Origem: revisão do proprietário após o code 81 — "validar com
teste de host é fácil; no editor é outro. Montar uma animação hoje é difícil
para quem não sabe nada e sem praticidade; o problema não é o número de
funções, é como são aplicadas. A aba Biblioteca não faz sentido: botão de
importar solto, abre tudo na tela. Não gosto de texto informativo."

NÃO IREI SER SIMPLISTA NO DESIGN.

## 1. O que a captura real mostra (Astra Dev code 77–79, Kyle Tutorial)

Estúdio (`v77_c`, `v78_a`): abre com a aba Chave e um cartão "Canal · Posição
X" de uma trilha que o usuário não escolheu; o modo pose está desligado; a
coluna da direita é uma pilha de cartões com texto de estado ("Toque uma chave
na timeline…", "Arraste o alvo na ponta selecionada", "Desligado · toque para
ligar"). Para posar: aba Pose → "Modo pose" → tocar um osso → ferramenta →
arrastar → barra "Gravar chave / Cancelar". Criar clipe: "Novo" → escolher
"Quaternion / Euler / Progressivo" antes de existir qualquer pose. Quatro abas
(Chave, Rig, Pose, IK) escondem o essencial e expõem o avançado no mesmo nível.

Biblioteca (`v79_b`): lista plana de 23 itens com rótulos "Converter /
Diagnóstico / Abrir" em texto; "Importar pasta" como botão de texto no
cabeçalho; filtros por ciclo de toque; o diagnóstico vira uma página de texto.
"Recursos importados" é outro espaço com outra lista. Importar modelo mora no
"+" da Hierarquia; importar pasta mora na Biblioteca; Unity entra pela
Biblioteca; textura/HDRI entram pelo Ambiente. Quatro portas para a mesma ação.

## 2. Princípios (extraídos, não copiados)

UMotion 1.29 (Pose Editor / Clip Editor): posar é o estado normal; auto-key
"Generate"; chaves-mestras por quadro; lista de propriedades animadas com ícone
do tipo e seleção sincronizada com o osso; canais do osso selecionado como
campos editáveis com cor de "modificado"; transporte com início/fim/velocidade.
Unity 6000.0 Animation window: gravação (record) liga o auto-key e marca
em vermelho o que muda; a timeline só mostra as propriedades do objeto.
Godot 4.3 AnimationPlayer: a chave fica ao lado da propriedade no Inspector —
quem nunca animou entende "este valor, neste tempo".

Regras para a Astra em toque:
1. **Zero texto de estado.** Estado vira cor, ícone, badge ou controle
   desabilitado. Diagnóstico vira um chip com ícone no lugar onde o problema
   está (linha do osso, chave, cartão).
2. **Posar é o padrão.** Abrir o estúdio = tocar um osso e arrastar. Auto-key
   sempre ligado (modo "gerar"); soltar o gizmo grava e pulsa a chave na régua.
   Nada de "Gravar/Cancelar" por gesto: Desfazer é o cancelar.
3. **Um lugar para cada coisa.** Esquerda: ossos (árvore) com ícone de
   "animado/modificado". Centro: vista + timeline (régua, chaves-mestras,
   linhas do osso selecionado). Direita: *o osso selecionado* — canais
   Posição/Rotação/Escala com valor e botão de chave por canal (Godot), e abaixo
   as ferramentas de pose (espelhar, copiar/colar, repouso, fantasma, IK).
   Rig e export saem do fluxo principal (menu ⋮ do estúdio).
4. **Criar é uma ação, não um formulário.** "Animar" no menu de contexto do
   objeto e "Novo clipe" no estúdio pedem só o nome; duração 2 s e rotação
   quaternion são padrão; Euler/progressivo ficam no menu ⋮ do canal.
5. **Importar é universal.** Um botão na barra superior (ícone importar) abre o
   menu de contexto com: Modelo (GLB/FBX), Pasta de pacote, Textura ou HDRI,
   Unity (.anim/.prefab/.controller). A Biblioteca é o gerenciador do que já
   entrou: trilho de tipos à esquerda, cartões ao centro (ícone do tipo, nome,
   pacote, badge de estado), detalhes à direita com ações em ícone. "Recursos
   importados" e "Biblioteca" viram um só espaço com dois filtros de origem.

## 3. Estúdio v2 — layout e controles

```
┌ barra do estúdio: ← | nome do clipe ▾ | ⟲ ⟳ | ⏮ ◀ ▶ ▶ ⏭ | 1.52 s | ⋮ ┐
│ ossos        │ vista 3D (gizmo do osso, coluna de ferramentas)  │ osso    │
│ ▸ Root ●     │                                                   │ Posição │
│   Ribs ●     │                                                   │ x y z ◆ │
│   Neck       │                                                   │ Rotação │
│   Head ●     ├───────────────────────────────────────────────────┤ x y z ◆ │
│   L_Shoulder │ régua 0 ─ 0.5 ─ 1.0 ─ 1.5 ─ 2.0 ─ 2.5 ─ 3 s        │ ───────  │
│   L_UpperArm │ ◆ ◆   ◆     ◆   ◆  chaves-mestras                  │ ⟷ ⎘ ⎗ ↺ │
│   …          │ Head·Rotação  ◆      ◆                             │ ghost IK│
└──────────────┴────────────────────────────────────────────────────┴─────────┘
```

- **Árvore de ossos:** ponto verde = tem chaves; ponto âmbar = modificado sem
  chave (impossível com auto-key, mas vale para camadas mudas); toque seleciona
  e enquadra sob demanda (toque duplo). Busca por nome no cabeçalho.
- **Vista:** coluna de ferramentas Mover/Girar/Escalar/Enquadrar/Órbita/IK,
  como hoje; o nome do osso vira um chip sobre o gizmo, não uma faixa.
- **Timeline:** régua com arrasto; linha de chaves-mestras (toque seleciona
  todas do quadro; arrasto move); linhas do osso selecionado (Posição, Rotação,
  Escala) com chaves; toque longo numa chave abre o menu de contexto (tempo,
  valor, tangente, copiar, excluir). Zoom por pinça.
- **Painel do osso (direita):** canais com valor e **botão de chave** por
  canal/propriedade (estado: vazio = sem chave; cheio = chave neste tempo;
  contorno = chave em outro tempo). Abaixo, ferramentas de pose como grade de
  ícones: espelhar edição, copiar pose, colar pose, pose de repouso, fantasma,
  ossos visíveis, IK na ponta, fixar no intervalo. Biblioteca de poses como
  chips. Sem cartão de texto.
- **Menu ⋮ do estúdio:** Camadas, Rig, Root motion, Bake, Exportar GLB,
  Consolidar, Configurações do clipe (duração, taxa, loop, modo de rotação).
- **Abrir:** menu de contexto do objeto → "Animar" (cria "Clipe 1" se não há
  clipe; senão abre o último). Inspector › Animação › Clipes › "+" idem.

## 4. Biblioteca v2 — gerenciador

```
┌ Biblioteca                      🔍 buscar…                  [⤓ Importar ▾] ┐
│ Tipos        │ cartões                                   │ detalhe        │
│ ● Todos  23  │ ▣ PickUp.anim        UMotion · Clipe  ✓   │ ícone + nome   │
│ ◻ Modelos 3  │ ▣ Idle.anim          UMotion · Clipe  ↻   │ estado (badge) │
│ ◻ Clipes  4  │ ▣ Robot Kyle.fbx     UMotion · Modelo ✓   │ ações: ícones  │
│ ◻ Materiais  │ ▣ CubeMaterial.mat   UMotion · Material ⚠ │ dependências   │
│ ◻ Texturas   │ …                                          │ diagnóstico ⚠  │
│ ◻ Prefabs    │                                            │ como chips     │
│ ◻ Pacotes    │                                            │                │
└──────────────┴────────────────────────────────────────────┴────────────────┘
```

- Badges: ✓ pronto (nativo), ↻ converter, ⚠ diagnóstico, ⏳ preparando.
- Ações no detalhe como botões de ícone com rótulo curto: Abrir, Converter,
  Instanciar, Vincular, Reimportar, Manter local / Receber origem.
- O botão **Importar** é o mesmo da barra superior (universal).
- "Recursos importados" vira o filtro de origem "Projeto" dentro da Biblioteca;
  a aba antiga sai do menu Cena.

## 5. Execução por blocos (cada um com preview no host e captura)

E1. Estúdio: posar por padrão + auto-key + chaves-mestras + painel do osso
    com botões de chave + árvore com estado; retirar cartões de texto; "Animar"
    no menu de contexto; "Novo" pedindo só o nome. Rig/IK avançado/Bake/Export
    vão para o menu ⋮.
E2. Timeline: toque longo na chave abre menu de contexto; pinça para zoom;
    linhas do osso selecionado.
E3. Importar universal na barra superior (menu de contexto) e Biblioteca com
    trilho de tipos, cartões com badge e detalhe com ações em ícone; união
    com Recursos importados.
E4. Revisão dos plugins (UMotion, Final IK, BoZo, POLYGON) com o novo filtro:
    só entra o que tem fluxo completo no editor.

Hipótese visual: gerada por imagem a partir das capturas (ver pasta
`docs/planos/ui-universal/estudio-v2/`). A imagem não é evidência; cada
controle desenhado foi confrontado com modelo, serialização e ação antes de
codificar.

## 6. Estado (15/10/2026)

- E1 entregue (commit 902d28dc): posar por padrão na largura de estúdio,
  painel do osso com chave por canal, grade de ferramentas, menu ⋮, "Animar",
  pontos na árvore, textos de estado removidos. Diferenças da hipótese: a
  linha de chaves-mestras e o toque longo na chave ficam para E2; o compacto
  (< 900 px) mantém Pose/Novo no cabeçalho por não ter painel lateral.
- E3 entregue (commit 41216ee1): Importar universal na barra, Biblioteca com
  trilho/cartões/detalhe. União com "Recursos importados" feita em 15/10: as
  malhas do projeto viram cartões no trilho **Projeto** (contagem no trilho,
  "Na cena" no canto do cartão e no detalhe, pelo mesmo widget do arrasto para
  a cena), o item do menu Cena passa a "Malhas do projeto" e abre a Biblioteca
  nesse trilho; a lista de texto "Adicionar malha N" foi removida. Selo
  **origem sem registro**: um cartão de aviso por raiz importada cuja origem
  não está em `.astra/assets.astra` (projeto copiado sem a pasta `.astra`), com
  o diagnóstico em chips e a ação "Importar modelo".
- Evidência: capturas do preview do host em `estudio-v2/` (studio-kyle*,
  library-*); suítes 71/71, 1569/1569, 126/126, 24/24. Sem aparelho (ADB off).
- E2 entregue: linha de chaves-mestras na metade de baixo da régua (um losango
  por quadro com chave em qualquer trilha da camada; tocar seleciona o quadro
  inteiro e leva o cursor; arrastar move todas as chaves do quadro com um
  único passo de histórico; sem chave perto vale como scrub), toque longo numa
  chave abre o menu de contexto da chave (`KeyLevel`: Tempo, Valor, Tangente,
  Curva/Dopesheet, Copiar, Recortar, Excluir) e pinça na timeline faz zoom da
  régua em torno do ponto médio dos dedos, cancelando o arrasto em curso; o
  dedo que sobra não faz scrub nem arrasta chave. A faixa própria de 24 px foi
  descartada porque roubava linhas a 394 px e esvaziava o canvas a 300 px.
- O modo `scene-kyle-menu` do preview mostra "Criar" em vez das ações do
  objeto por um artefato de seleção do preview, não do editor.

## 7. E4 — plugins pelo filtro "só entra o que tem fluxo completo no editor"

Levantamento sobre o código (15/10/2026), não sobre a lista de 11/10:

| Origem | O que existe | Fluxo no editor | Decisão |
|---|---|---|---|
| UMotion 1.29 (N01/N02) | dopesheet, curvas, camadas, cues, pose, auto-key, chaves-mestras, menu da chave, pinça, biblioteca de poses, espelho, IK no estúdio, Fixar, Seguir, root motion, bake, GLB | completo pelo estúdio v2 | nada a acrescentar agora; "exibição de ossos" e "editar em Play" são cosméticos ou avançados e ficam fora |
| Final IK 2.2 | 30 componentes (`astra.ik.*`: two_bone, chain, fabrik_root, look_at, aim, rotation_limit, grounder, foot_grounding, full_body, body_by_targets, finger_curl, hand_poser, offset, twist_relaxer, hit_reaction, recoil, inertia, amplifier, body_tilt, penetration_avoidance, socket, interaction_*) | só pelo Inspector › + (ossos manuais, salvo Interação, Corpo inteiro, Grounder e Apoio do pé); o assistente que monta tudo fica escondido no ⋮ do estúdio; os modelos de cena prontos só servem o manequim | **lacuna real**: não havia caminho de criação sobre o personagem do usuário |
| BoZo 2.2 | nada nativo (só inventário) | — | fora: exige modelo/runtime modular (N05/N07/N10/N11), não é lacuna de fluxo |
| POLYGON Icons 1.01 | 520 FBX decodificados; galeria offline | Importar › Pasta de pacote → Biblioteca › Modelos → Na cena | fluxo existe pela Biblioteca v2; galeria própria não entra |
| VRIK | — | exige rastreamento XR | fora |

Entregue: categoria **Personagem** no menu Criar (e no "+" da Hierarquia) com
receitas sobre o personagem da seleção, resolvido pelo ancestral que carrega
animação/Animator/sistema de corpo ou pelo objeto abaixo da raiz com esqueleto
(sem seleção, o único esqueleto da cena): **Olhar para alvo** (LookAt na cabeça,
alvo 2 m à frente), **Mirar com as mãos** (Aim em cada mão, alvo 1,5 m no +Z da
mão), **Mãos ao alvo** (IK de dois ossos com alvo onde a mão está), **Pés no
chão** (Apoio do pé por pé + Grounder com a pelve), **Corpo inteiro** (pelve,
peito e até quatro pontas com alvos) e **Carregar caixa** (mesa, caixa com
pegadas e interação, o mesmo do modelo de cena, numa pasta ao lado do
personagem). Cada receita é um passo de Desfazer; a categoria fica desligada
sem personagem; o assistente completo continua no estúdio.

Referências: Unity Animation Rigging 1.3 (TwoBoneIKConstraint, MultiAimConstraint,
Rig Builder) https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.3/manual/ConstraintComponents.html;
Final IK 2.2 (LookAtIK, AimIK, LimbIK, Grounder, FullBodyBipedIK, InteractionSystem),
scripts locais em `analise-plugins`; Godot 4.3 SkeletonIK3D
https://docs.godotengine.org/en/4.3/classes/class_skeletonik3d.html e LookAtModifier3D
https://docs.godotengine.org/en/4.3/classes/class_lookatmodifier3d.html.
- Aparelho (code 82–85, projeto "Kyle – Carregar caixa" gerado no host): o
  fluxo Animar → osso → chave por canal → campo → arrasto com auto-key foi
  conferido; enquadramento de abertura passou a usar os ossos (ef01c9a0) e os
  campos deixaram de mostrar ruído (5acdf468). Capturas `build/device-rig/v82_*`,
  `v84_*`, `v85_*`.
- Fechado (code 86): a malha do Kyle não aparecia nem no estúdio nem na cena
  desse projeto. Causa: a cópia para o aparelho foi feita com `*`, que não
  inclui `.astra/` (registro `assets.astra` + `imports/<guid>.nodes`); sem o
  registro o guid do GLB referenciado por `astra.import.link` não resolve e o
  app recriou o registro só com o clipe. Enviado o `.astra` e o registro
  fundido (malha + clipe): cena e estúdio desenham o robô (`v86_scene2`,
  `v86_studio2`). Não é defeito do editor; fica a lacuna de produto: um
  projeto copiado sem `.astra` perde as malhas em silêncio — a Biblioteca
  deveria acusar "origem sem registro" no cartão do modelo (próxima rodada).

Referências: UMotion Pro 1.29p04 manual local (`PoseEditor`, `Animation`,
`Channels`, `Dopesheet`, `AnimatedPropertiesList`, `Playback`); Unity 6000.0
https://docs.unity3d.com/6000.0/Documentation/Manual/animeditor-AnimatingAGameObject.html;
Godot 4.3 https://docs.godotengine.org/en/4.3/tutorials/animation/introduction.html.
